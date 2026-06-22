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
#include "device/core.h"
#include "device/dispatcher.h"
#include "mqtt/pubsub.h"
#include "pairing/core.h"

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static astarte_result_t initialize_introspection(
    astarte_device_handle_t device, const astarte_interface_t **interfaces, size_t interfaces_size);
static astarte_result_t initialize_mqtt_topics(astarte_device_handle_t device);

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
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_device_handle_t handle = NULL;

    ASTARTE_LOG_DBG("Creating a new device instance");

    if (!cfg || !device) {
        ASTARTE_LOG_ERR("Received NULL reference for configuration or device handle");
        ares = ASTARTE_RESULT_INVALID_PARAM;
        goto failure;
    }

    handle = astarte_calloc(1, sizeof(struct astarte_device));
    if (!handle) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        ares = ASTARTE_RESULT_OUT_OF_MEMORY;
        goto failure;
    }

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

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    ares = astarte_storage_init(&handle->caching);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Storage initialization failure %s", astarte_result_to_name(ares));
        goto failure;
    }
#endif

    // Initializing the connection hashmap and status flags
    handle->synchronization_completed = false;
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    ASTARTE_LOG_DBG("Getting stored synchronization");
    ares
        = astarte_storage_synchronization_get(&handle->caching, &handle->synchronization_completed);
    if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_NOT_FOUND)) {
        ASTARTE_LOG_ERR("Synchronization state getter failure %s", astarte_result_to_name(ares));
        goto failure;
    }
    ASTARTE_LOG_DBG("Done fetching device synchronization '%d'", handle->synchronization_completed);
#endif
    handle->connection_state = DEVICE_DISCONNECTED;

    ASTARTE_LOG_DBG("Initializing introspection");
    ares = initialize_introspection(handle, cfg->interfaces, cfg->interfaces_size);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Introspection initialization failure %s", astarte_result_to_name(ares));
        goto failure;
    }

    ares = initialize_mqtt_topics(handle);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed initialization for MQTT topics %s", astarte_result_to_name(ares));
        goto failure;
    }

    ASTARTE_LOG_DBG("Initializing Astarte MQTT client");
    astarte_mqtt_config_t astarte_mqtt_config = { 0 };
    astarte_mqtt_config.clean_session = false;
    astarte_mqtt_config.connection_timeout_ms = cfg->mqtt_connection_timeout_ms;
    astarte_mqtt_config.poll_timeout_ms = cfg->mqtt_poll_timeout_ms;
    astarte_mqtt_config.refresh_client_cert_cbk = refresh_client_cert_handler;

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
        goto failure;
    }

    ASTARTE_LOG_DBG("Getting MQTT broker client ID");
    int snprintf_rc = snprintf(astarte_mqtt_config.client_id, sizeof(astarte_mqtt_config.client_id),
        CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/%s", handle->device_id);
    if (snprintf_rc != ASTARTE_MQTT_CLIENT_ID_LEN) {
        ASTARTE_LOG_ERR("Error encoding MQTT client ID");
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto failure;
    }

    ares = astarte_mqtt_init(&astarte_mqtt_config, &handle->astarte_mqtt);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR(
            "Failure intializing the Astarte MQTT client %s", astarte_result_to_name(ares));
        goto failure;
    }

    // Initialize the handle data to be used during the handshake with Astarte
    handle->mqtt_session_present_flag = 0;
    handle->reconnection_timepoint = sys_timepoint_calc(K_NO_WAIT);
    backoff_init(&handle->backoff_ctx, CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_BACKOFF_MULT_COEFF_MS,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_BACKOFF_CUTOFF_COEFF_MS);

    *device = handle;

    ASTARTE_LOG_DBG("Device instance creation completed");

    return ares;

failure:

    if (handle) {
#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
        astarte_storage_destroy(&handle->caching);
#endif
        introspection_free(handle->introspection);
    }
    astarte_free(handle);
    return ares;
}

astarte_result_t astarte_device_destroy(astarte_device_handle_t device)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    ASTARTE_LOG_DBG("Destroying an Astarte device instance");

    if (!device) {
        return ASTARTE_RESULT_OK;
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

#ifdef CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE
    astarte_storage_destroy(&device->caching);
#endif

    introspection_free(device->introspection);
    astarte_free(device);

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
