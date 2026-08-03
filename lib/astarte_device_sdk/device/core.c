/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "astarte_device_sdk/device.h"

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
#include "storage/core.h"
#include "storage/prop.h"
#include "storage/sync.h"
#endif

#include "alloc.h"
#include "cleanup.h"
#include "device/core.h"
#include "device/dispatcher.h"
#include "device/session_manager.h"
#include "mqtt/pubsub.h"
#include "pairing/core.h"

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

#define POLLING_ERROR_RETRY_DELAY_MS 50
#define TRANSMISSION_PACING_MAX_TOKENS 10
#define TRANSMISSION_PACING_TOKEN_VALUE_MS 20
#define TRANSMISSION_EMPTY_QUEUE_WAITING_MS 100
#define TRANSMISSION_EVENT_WAITING_MS 50
#define TRANSMISSION_ERROR_RETRY_DELAY_MS 50

/************************************************
 *         Static variables declaration         *
 ***********************************************/

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static struct astarte_device device_instance = { 0 };
static bool device_initialized = false;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static astarte_result_t initialize_introspection(
    astarte_device_handle_t device, const astarte_interface_t **interfaces, size_t interfaces_size);
static astarte_result_t initialize_mqtt_topics(astarte_device_handle_t device);
static void astarte_device_worker_thread_entry(void *par1, void * /*par2*/, void * /*par3*/);

static void refill_transmission_tokens(uint32_t *tokens, int64_t *last_refill);
static void process_transmission_queue(
    struct astarte_device *device, uint32_t *tokens, int64_t last_refill);

// Helper to safely destroy a partially initialized device
static void cleanup_device_creation(astarte_device_handle_t *handle_ptr)
{
    if (!handle_ptr) {
        return;
    }

    astarte_device_handle_t handle = *handle_ptr;
    if (!handle) {
        return;
    }

    astarte_transmission_queue_clear(&handle->transmission_queue);
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    astarte_storage_destroy(&handle->caching);
#endif
    introspection_free(handle->introspection);

    device_initialized = false;
}

ASTARTE_SCOPE_DEFER_DEFINE(cleanup_device_creation, astarte_device_handle_t *);

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

static astarte_result_t refresh_client_cert_handler(astarte_mqtt_t *astarte_mqtt)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);
    astarte_tls_credentials_client_crt_t *client_crt = &device->client_crt;

    ASTARTE_LOG_DBG("Refreshing the MQTT client certificate");

    if (strlen(client_crt->crt_pem) != 0) {
        ares = astarte_pairing_verify_client_certificate(
            device->http_timeout_ms, device->device_id, device->cred_secr, client_crt->crt_pem);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_CLIENT_CERT_INVALID)) {
            ASTARTE_LOG_ERR("Verify client certificate failed: %s", astarte_result_to_name(ares));
            return ares;
        }
        if (ares == ASTARTE_RESULT_OK) {
            ASTARTE_LOG_DBG("Previous certificate is still valid, no refresh required");
            return ares;
        }
        ares = astarte_tls_credential_delete();
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Can't delete the client TLS cert: %s", astarte_result_to_name(ares));
            return ares;
        }
    }

    ares = astarte_pairing_get_client_certificate(
        device->http_timeout_ms, device->device_id, device->cred_secr, client_crt);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed getting the client TLS cert: %s", astarte_result_to_name(ares));
        return ares;
    }

    ares = astarte_tls_credential_add(client_crt);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed adding the client TLS cert: %s", astarte_result_to_name(ares));
        return ares;
    }

    ASTARTE_LOG_DBG("MQTT client certificate updated");

    return ares;
}

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_new(astarte_device_config_t *cfg, astarte_device_handle_t *device)
{
    ASTARTE_LOG_DBG("Creating a new device instance");
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (!cfg || !device) {
        ASTARTE_LOG_ERR("Received NULL reference for configuration or device handle");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    if (device_initialized) {
        ASTARTE_LOG_ERR("Device is already initialized. Only a single instance is allowed.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    memset(&device_instance, 0, sizeof(struct astarte_device));

    astarte_device_handle_t handle = &device_instance;
    scope_defer(cleanup_device_creation)(&handle);

    handle->http_timeout_ms = cfg->http_timeout_ms;
    memcpy(handle->device_id, cfg->device_id, ASTARTE_DEVICE_ID_LEN + 1);
    memcpy(handle->cred_secr, cfg->cred_secr, ASTARTE_PAIRING_CRED_SECR_LEN + 1);
    handle->connection_cbk = cfg->connection_cbk;
    handle->disconnection_cbk = cfg->disconnection_cbk;
    handle->datastream_individual_cbk = cfg->datastream_individual_cbk;
    handle->datastream_object_cbk = cfg->datastream_object_cbk;
    handle->property_set_cbk = cfg->property_set_cbk;
    handle->property_unset_cbk = cfg->property_unset_cbk;
    handle->cbk_user_data = cfg->cbk_user_data;
    handle->connection_state = DEVICE_DISCONNECTED;
    handle->synchronization_completed = false;

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    ares = astarte_storage_init(&handle->caching);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Storage initialization failure %s", astarte_result_to_name(ares));
        return ares;
    }

    ASTARTE_LOG_DBG("Getting stored synchronization");
    ares
        = astarte_storage_synchronization_get(&handle->caching, &handle->synchronization_completed);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Synchronization state getter failure %s", astarte_result_to_name(ares));
        return ares;
    }
    ASTARTE_LOG_DBG("Done fetching device synchronization '%d'", handle->synchronization_completed);
#endif

    ASTARTE_LOG_DBG("Initializing introspection");
    ares = initialize_introspection(handle, cfg->interfaces, cfg->interfaces_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Introspection initialization failure %s", astarte_result_to_name(ares));
        return ares;
    }

    ares = initialize_mqtt_topics(handle);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed initialization for MQTT topics %s", astarte_result_to_name(ares));
        return ares;
    }

    ASTARTE_LOG_DBG("Initializing Astarte MQTT client");
    astarte_mqtt_config_t astarte_mqtt_config = { 0 };
    astarte_mqtt_config.clean_session = false;
    astarte_mqtt_config.connection_timeout_ms = cfg->mqtt_connection_timeout_ms;
    astarte_mqtt_config.poll_timeout_ms = cfg->mqtt_poll_timeout_ms;
    astarte_mqtt_config.refresh_client_cert_cbk = refresh_client_cert_handler;
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    astarte_mqtt_config.storage = &handle->caching;
#endif

    // Wire up dispatcher
    astarte_mqtt_config.on_subscribed_cbk = astarte_device_dispatcher_on_subscribed;
    astarte_mqtt_config.on_connected_cbk = astarte_device_dispatcher_on_connected;
    astarte_mqtt_config.on_disconnected_cbk = astarte_device_dispatcher_on_disconnected;
    astarte_mqtt_config.on_incoming_cbk = astarte_device_dispatcher_on_incoming;

    ASTARTE_LOG_DBG("Getting MQTT broker hostname and port");
    ares = astarte_pairing_get_mqtt_broker_hostname_and_port(handle->http_timeout_ms,
        handle->device_id, handle->cred_secr, astarte_mqtt_config.broker_hostname,
        astarte_mqtt_config.broker_port);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed in parsing the MQTT broker URL %s", astarte_result_to_name(ares));
        return ares;
    }

    ASTARTE_LOG_DBG("Getting MQTT broker client ID");
    int snprintf_rc = snprintf(astarte_mqtt_config.client_id, sizeof(astarte_mqtt_config.client_id),
        CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/%s", handle->device_id);
    if (snprintf_rc != ASTARTE_MQTT_CLIENT_ID_LEN) {
        ASTARTE_LOG_ERR("Error encoding MQTT client ID");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    ares = astarte_mqtt_init(&astarte_mqtt_config, &handle->astarte_mqtt);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR(
            "Failure intializing the Astarte MQTT client %s", astarte_result_to_name(ares));
        return ares;
    }

    // Initialize the handle data to be used during the handshake with Astarte
    handle->mqtt_session_present_flag = 0;
    handle->reconnection_timepoint = sys_timepoint_calc(K_NO_WAIT);
    backoff_init(&handle->backoff_ctx, CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_BACKOFF_MULT_COEFF_MS,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_BACKOFF_CUTOFF_COEFF_MS);

    // Initialize the transmission queue
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    ares = astarte_transmission_queue_init(&handle->transmission_queue, &handle->caching);
#else
    ares = astarte_transmission_queue_init(&handle->transmission_queue);
#endif
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR(
            "Failure intializing the transmission queue %s", astarte_result_to_name(ares));
        return ares;
    }

    // Initialize the error event queue
    k_msgq_init(&handle->error_queue, handle->error_queue_buffer,
        sizeof(astarte_device_error_event_t), ASTARTE_DEVICE_ERROR_QUEUE_SIZE);

    ASTARTE_LOG_DBG("Initializing Astarte worker thread");

    k_event_init(&handle->events);
    k_thread_create(&handle->worker_thread, handle->worker_thread_stack,
        K_THREAD_STACK_SIZEOF(handle->worker_thread_stack),
        (k_thread_entry_t) astarte_device_worker_thread_entry, handle, NULL, NULL,
        K_PRIO_PREEMPT(CONFIG_ASTARTE_DEVICE_SDK_WORKER_THREAD_PRIORITY), 0, K_NO_WAIT);

    // Transfer ownership and disarm
    *device = handle;
    handle = NULL;

    // Device is now initialized
    device_initialized = true;

    ASTARTE_LOG_DBG("Device instance creation completed");

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_device_destroy(astarte_device_handle_t device)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    ASTARTE_LOG_DBG("Destroying an Astarte device instance");

    if (!device) {
        return ASTARTE_RESULT_OK;
    }

    if (!device_initialized) {
        ASTARTE_LOG_ERR("Device is not initialized. Cannot destroy it.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    if (device->connection_state != DEVICE_DISCONNECTED) {
        ares = astarte_device_force_disconnect(device);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Failed disconnecting the device: %s", astarte_result_to_name(ares));
            return ares;
        }
    }

    astarte_mqtt_clear_all_pending(&device->astarte_mqtt);

    ares = astarte_tls_credential_delete();
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed deleting the client TLS cert: %s", astarte_result_to_name(ares));
        return ares;
    }

    // Signal the transmission thread to exit and wait for it to cleanly terminate
    ASTARTE_LOG_DBG("Stopping the Astarte worker thread");
    k_event_post(&device->events, ASTARTE_DEVICE_DESTROY_EVENT_BIT);
    k_thread_join(&device->worker_thread, K_FOREVER);

    astarte_transmission_queue_clear(&device->transmission_queue);
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    astarte_storage_destroy(&device->caching);
#endif
    introspection_free(device->introspection);

    device_initialized = false;

    ASTARTE_LOG_DBG("Astarte device instance destroyed");

    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_device_add_interface(
    astarte_device_handle_t device, const astarte_interface_t *interface)
{
    if (!device || !interface) {
        ASTARTE_LOG_ERR("Received NULL reference for device handle or interface");
        return ASTARTE_RESULT_INVALID_PARAM;
    }
    ASTARTE_LOG_DBG("Adding interface %s to the Astarte device", interface->name);
    return introspection_update(&device->introspection, interface);
}

astarte_result_t astarte_device_get_error_event(
    astarte_device_handle_t device, astarte_device_error_event_t *event, k_timeout_t timeout)
{
    if (!device || !event) {
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    if (k_msgq_get(&device->error_queue, event, timeout) == 0) {
        return ASTARTE_RESULT_OK;
    }

    return ASTARTE_RESULT_TIMEOUT;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t initialize_introspection(
    astarte_device_handle_t device, const astarte_interface_t **interfaces, size_t interfaces_size)
{
    astarte_result_t ares = introspection_init(&device->introspection);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Introspection initialization failure %s", astarte_result_to_name(ares));
        return ares;
    }
    if (interfaces) {
        for (size_t i = 0; i < interfaces_size; i++) {
            ares = introspection_add(&device->introspection, interfaces[i]);
            if (ares != ASTARTE_RESULT_OK) {
                ASTARTE_LOG_ERR("Introspection add failure %s", astarte_result_to_name(ares));
                return ares;
            }
        }
    }
    return ASTARTE_RESULT_OK;
}

static astarte_result_t initialize_mqtt_topics(astarte_device_handle_t device)
{
    int snprintf_rc = snprintf(
        device->base_topic, MQTT_BASE_TOPIC_LEN + 1, MQTT_TOPIC_PREFIX "%s", device->device_id);
    if (snprintf_rc != MQTT_BASE_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding base topic");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    snprintf_rc = snprintf(device->control_topic, MQTT_CONTROL_TOPIC_LEN + 1,
        MQTT_TOPIC_PREFIX "%s" MQTT_CONTROL_TOPIC_SUFFIX, device->device_id);
    if (snprintf_rc != MQTT_CONTROL_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding base control topic");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    snprintf_rc
        = snprintf(device->control_empty_cache_topic, MQTT_CONTROL_EMPTY_CACHE_TOPIC_LEN + 1,
            MQTT_TOPIC_PREFIX "%s" MQTT_CONTROL_EMPTY_CACHE_TOPIC_SUFFIX, device->device_id);
    if (snprintf_rc != MQTT_CONTROL_EMPTY_CACHE_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding empty cache publish topic");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    snprintf_rc
        = snprintf(device->control_consumer_prop_topic, MQTT_CONTROL_CONSUMER_PROP_TOPIC_LEN + 1,
            MQTT_TOPIC_PREFIX "%s" MQTT_CONTROL_CONSUMER_PROP_TOPIC_SUFFIX, device->device_id);
    if (snprintf_rc != MQTT_CONTROL_CONSUMER_PROP_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding Astarte purte properties topic");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    snprintf_rc
        = snprintf(device->control_producer_prop_topic, MQTT_CONTROL_PRODUCER_PROP_TOPIC_LEN + 1,
            MQTT_TOPIC_PREFIX "%s" MQTT_CONTROL_PRODUCER_PROP_TOPIC_SUFFIX, device->device_id);
    if (snprintf_rc != MQTT_CONTROL_PRODUCER_PROP_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding device purge properties topic");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    return ASTARTE_RESULT_OK;
}

static void astarte_device_worker_thread_entry(void *par1, void * /*par2*/, void * /*par3*/)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct astarte_device *device = (struct astarte_device *) par1;

    // Initialize token bucket state
    uint32_t transmission_tokens = TRANSMISSION_PACING_MAX_TOKENS;
    int64_t last_token_refill = k_uptime_get();

    while (true) {
        // Check if a destroy was requested before doing anything else
        uint32_t events = k_event_test(&device->events, ASTARTE_DEVICE_DESTROY_EVENT_BIT);
        if (events & ASTARTE_DEVICE_DESTROY_EVENT_BIT) {
            break;
        }

        // Poll the state machine and MQTT client
        ares = astarte_device_internal_poll(device);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Error polling the device: %s", astarte_result_to_name(ares));

            astarte_device_error_event_t err_ev
                = { .result = ares, .context = "astarte_device_internal_poll" };
            k_msgq_put(&device->error_queue, &err_ev, K_NO_WAIT);

            k_msleep(POLLING_ERROR_RETRY_DELAY_MS);
            continue;
        }

        // Wait for connection
        events = k_event_wait(&device->events,
            ASTARTE_DEVICE_CONNECTION_EVENT_BIT | ASTARTE_DEVICE_DESTROY_EVENT_BIT, false,
            K_MSEC(TRANSMISSION_EVENT_WAITING_MS));

        // If the device is being destroyed, break the loop
        if (events & ASTARTE_DEVICE_DESTROY_EVENT_BIT) {
            break;
        }

        // If we timed out waiting for a connection, loop back and poll again
        if (!(events & ASTARTE_DEVICE_CONNECTION_EVENT_BIT)) {
            continue;
        }

        // Refill tokens and process the transmission queue
        refill_transmission_tokens(&transmission_tokens, &last_token_refill);
        process_transmission_queue(device, &transmission_tokens, last_token_refill);
    }
}

static void refill_transmission_tokens(uint32_t *tokens, int64_t *last_refill)
{
    int64_t now = k_uptime_get();
    int64_t elapsed = now - *last_refill;

    // Generate tokens based on elapsed time
    if (elapsed >= TRANSMISSION_PACING_TOKEN_VALUE_MS) {
        int64_t generated_tokens = elapsed / TRANSMISSION_PACING_TOKEN_VALUE_MS;
        if (*tokens + generated_tokens > TRANSMISSION_PACING_MAX_TOKENS) {
            *tokens = TRANSMISSION_PACING_MAX_TOKENS;
        } else {
            *tokens += (uint32_t) generated_tokens;
        }

        // No more widening warning: generated_tokens is already int64_t
        *last_refill += generated_tokens * TRANSMISSION_PACING_TOKEN_VALUE_MS;
    }
}

static void process_transmission_queue(
    struct astarte_device *device, uint32_t *tokens, int64_t last_refill)
{
    struct astarte_device_transmission_queue_msg msg = { 0 };
    astarte_result_t ares = astarte_transmission_queue_peek(&device->transmission_queue, &msg);
    if (ares != ASTARTE_RESULT_OK) {
        // Prevent CPU starvation when the queue is empty
        k_msleep(TRANSMISSION_EMPTY_QUEUE_WAITING_MS);
        goto exit;
    }

    // Check if we have tokens to transmit
    if (*tokens == 0) {
        // Out of tokens: calculate exact time until the next token is ready and sleep
        int64_t wait_ms = TRANSMISSION_PACING_TOKEN_VALUE_MS - (k_uptime_get() - last_refill);
        if (wait_ms > 0) {
            if (wait_ms > INT32_MAX) {
                ASTARTE_LOG_ERR("Wait time exceeds maximum value for k_msleep");
                wait_ms = INT32_MAX;
            }
            k_msleep((int32_t) wait_ms);
        }
        goto exit;
    }

    ASTARTE_LOG_DBG("Transmitting message for %s%s", msg.interface_name, msg.path);
    ASTARTE_LOG_HEXDUMP_DBG(msg.payload, msg.payload_len, "Payload: ");

    ares = astarte_device_dispatcher_publish_data(
        device, msg.interface_name, msg.path, msg.payload, msg.payload_len, msg.qos);
    if (ares == ASTARTE_RESULT_OK) {
        ares = astarte_transmission_queue_discard_by_retention(
            &device->transmission_queue, msg.retention);
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR(
                "Failed to remove message from queue: %s", astarte_result_to_name(ares));
        }
        // Consume a token for the successful transmission
        (*tokens)--;

    } else {
        ASTARTE_LOG_ERR("Failed to transmit message: %s", astarte_result_to_name(ares));

        astarte_device_error_event_t err_ev
            = { .result = ares, .context = "process_transmission_queue" };
        k_msgq_put(&device->error_queue, &err_ev, K_NO_WAIT);

        // Message safely remains at the head of the queue
        // Sleep for a short duration to back off before the loop retries.
        k_msleep(TRANSMISSION_ERROR_RETRY_DELAY_MS);
    }

exit:
    astarte_transmission_queue_msg_cleanup(&msg);
}
