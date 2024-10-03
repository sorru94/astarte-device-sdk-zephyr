/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "astarte_device_sdk/device.h"

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
#include "device_caching.h"
#endif
#include "device_connection.h"
#include "device_private.h"
#include "device_rx.h"
#include "device_tx.h"
#include "pairing_private.h"
#include "tls_credentials.h"

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Initialize MQTT topics.
 *
 * @param[in] device Handle to the device instance.
 * @return ASTARTE_RESULT_OK if publish has been successful, an error code otherwise.
 */
static astarte_result_t initialize_mqtt_topics(astarte_device_handle_t device);

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

static astarte_result_t refresh_client_cert_handler(astarte_mqtt_t *astarte_mqtt)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct astarte_device *device = CONTAINER_OF(astarte_mqtt, struct astarte_device, astarte_mqtt);
    astarte_tls_credentials_client_crt_t *client_crt = &device->client_crt;

    if (strlen(client_crt->crt_pem) != 0) {
        ares = astarte_pairing_verify_client_certificate(
            device->http_timeout_ms, device->device_id, device->cred_secr, client_crt->crt_pem);
        if ((ares != ASTARTE_RESULT_OK) && (ares != ASTARTE_RESULT_CLIENT_CERT_INVALID)) {
            ASTARTE_LOG_ERR("Verify client certificate failed: %s.", astarte_result_to_name(ares));
            return ares;
        }
        if (ares == ASTARTE_RESULT_OK) {
            // Certificate is valid, exit
            return ares;
        }
        ares = astarte_tls_credential_delete();
        if (ares != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Can't delete the client TLS cert: %s.", astarte_result_to_name(ares));
            return ares;
        }
    }

    ares = astarte_pairing_get_client_certificate(
        device->http_timeout_ms, device->device_id, device->cred_secr, client_crt);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed getting the client TLS cert: %s.", astarte_result_to_name(ares));
        memset(client_crt->privkey_pem, 0, ARRAY_SIZE(client_crt->privkey_pem));
        memset(client_crt->crt_pem, 0, ARRAY_SIZE(client_crt->crt_pem));
        return ares;
    }

    ares = astarte_tls_credential_add(client_crt);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed adding the client TLS cert: %s.", astarte_result_to_name(ares));
        return ares;
    }

    return ares;
}

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_new(astarte_device_config_t *cfg, astarte_device_handle_t *device)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    astarte_device_handle_t handle = calloc(1, sizeof(struct astarte_device));
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

    // Initializing the connection hashmap and status flags
    handle->synchronization_completed = false;
    handle->connection_state = DEVICE_DISCONNECTED;

    ASTARTE_LOG_DBG("Initializing introspection");
    ares = introspection_init(&handle->introspection);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Introspection initialization failure %s.", astarte_result_to_name(ares));
        goto failure;
    }
    if (cfg->interfaces) {
        for (size_t i = 0; i < cfg->interfaces_size; i++) {
            ares = introspection_add(&handle->introspection, cfg->interfaces[i]);
            if (ares != ASTARTE_RESULT_OK) {
                ASTARTE_LOG_ERR("Introspection add failure %s.", astarte_result_to_name(ares));
                goto failure;
            }
        }
    }

    ares = initialize_mqtt_topics(handle);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed initialization for MQTT topics %s.", astarte_result_to_name(ares));
        goto failure;
    }

    ASTARTE_LOG_DBG("Initializing Astarte MQTT client.");
    astarte_mqtt_config_t astarte_mqtt_config = { 0 };
    astarte_mqtt_config.clean_session = false;
    astarte_mqtt_config.connection_timeout_ms = cfg->mqtt_connection_timeout_ms;
    astarte_mqtt_config.poll_timeout_ms = cfg->mqtt_poll_timeout_ms;
    astarte_mqtt_config.refresh_client_cert_cbk = refresh_client_cert_handler;
    astarte_mqtt_config.on_subscribed_cbk = astarte_device_connection_on_subscribed_handler;
    astarte_mqtt_config.on_connected_cbk = astarte_device_connection_on_connected_handler;
    astarte_mqtt_config.on_disconnected_cbk = astarte_device_connection_on_disconnected_handler;
    astarte_mqtt_config.on_incoming_cbk = astarte_device_rx_on_incoming_handler;

    ASTARTE_LOG_DBG("Getting MQTT broker hostname and port");
    ares = astarte_pairing_get_mqtt_broker_hostname_and_port(handle->http_timeout_ms,
        handle->device_id, handle->cred_secr, astarte_mqtt_config.broker_hostname,
        astarte_mqtt_config.broker_port);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed in parsing the MQTT broker URL");
        goto failure;
    }

    ASTARTE_LOG_DBG("Getting MQTT broker client ID");
    int snprintf_rc = snprintf(astarte_mqtt_config.client_id, sizeof(astarte_mqtt_config.client_id),
        CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/%s", handle->device_id);
    if (snprintf_rc != ASTARTE_MQTT_CLIENT_ID_LEN) {
        ASTARTE_LOG_ERR("Error encoding MQTT client ID.");
        ares = ASTARTE_RESULT_INTERNAL_ERROR;
        goto failure;
    }

    ares = astarte_mqtt_init(&astarte_mqtt_config, &handle->astarte_mqtt);
    if (ares != ASTARTE_RESULT_OK) {
        goto failure;
    }

    // Initialize the handle data to be used during the handshake with Astarte
    handle->mqtt_session_present_flag = 0;
    handle->reconnection_timepoint = sys_timepoint_calc(K_NO_WAIT);
    backoff_context_init(&handle->backoff_ctx,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_ASTARTE_BACKOFF_INITIAL_MS,
        CONFIG_ASTARTE_DEVICE_SDK_RECONNECTION_ASTARTE_BACKOFF_MAX_MS, true);

    *device = handle;

    return ares;

failure:
    if (handle) {
        introspection_free(handle->introspection);
    }
    free(handle);
    return ares;
}

astarte_result_t astarte_device_destroy(astarte_device_handle_t device)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    if (device->connection_state != DEVICE_DISCONNECTED) {
        ares = astarte_device_disconnect(device);
        if (ares != ASTARTE_RESULT_OK) {
            return ares;
        }
    }

    ares = astarte_tls_credential_delete();
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed deleting the client TLS cert: %s.", astarte_result_to_name(ares));
        return ares;
    }

    introspection_free(device->introspection);
    free(device);
    return ASTARTE_RESULT_OK;
}

astarte_result_t astarte_device_add_interface(
    astarte_device_handle_t device, const astarte_interface_t *interface)
{
    return introspection_update(&device->introspection, interface);
}

astarte_result_t astarte_device_connect(astarte_device_handle_t device)
{
    return astarte_device_connection_connect(device);
}

astarte_result_t astarte_device_disconnect(astarte_device_handle_t device)
{
    return astarte_device_connection_disconnect(device);
}

astarte_result_t astarte_device_poll(astarte_device_handle_t device)
{
    return astarte_device_connection_poll(device);
}

astarte_result_t astarte_device_stream_individual(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_individual_t individual,
    const int64_t *timestamp)
{
    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called stream individual function when the device is not connected.");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    return astarte_device_tx_stream_individual(device, interface_name, path, individual, timestamp);
}

astarte_result_t astarte_device_stream_aggregated(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_object_entry_t *entries,
    size_t entries_len, const int64_t *timestamp)
{
    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called stream aggregated function when the device is not connected.");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    return astarte_device_tx_stream_aggregated(
        device, interface_name, path, entries, entries_len, timestamp);
}

astarte_result_t astarte_device_set_property(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_individual_t individual)
{
    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called set property function when the device is not connected.");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    return astarte_device_tx_set_property(device, interface_name, path, individual);
}

astarte_result_t astarte_device_unset_property(
    astarte_device_handle_t device, const char *interface_name, const char *path)
{
    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called unset property function when the device is not connected.");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    return astarte_device_tx_unset_property(device, interface_name, path);
}

#if defined(CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
astarte_result_t astarte_device_get_property(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_device_property_loader_cbk_t loader_cbk,
    void *user_data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_individual_t individual = { 0 };
    uint32_t out_major = 0U;
    ares = astarte_device_caching_property_load(interface_name, path, &out_major, &individual);
    if (ares != ASTARTE_RESULT_OK) {
        if (ares != ASTARTE_RESULT_NOT_FOUND) {
            ASTARTE_LOG_ERR("Failed getting property: %s.", astarte_result_to_name(ares));
        }
        return ares;
    }

    astarte_device_property_loader_event_t event = { .device = device,
        .interface_name = interface_name,
        .path = path,
        .individual = individual,
        .user_data = user_data };
    loader_cbk(event);

    astarte_device_caching_property_destroy_loaded(individual);
    return ares;
}
#endif

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t initialize_mqtt_topics(astarte_device_handle_t device)
{
    int snprintf_rc = snprintf(
        device->base_topic, MQTT_BASE_TOPIC_LEN + 1, MQTT_TOPIC_PREFIX "%s", device->device_id);
    if (snprintf_rc != MQTT_BASE_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding base topic.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    snprintf_rc = snprintf(device->control_topic, MQTT_CONTROL_TOPIC_LEN + 1,
        MQTT_TOPIC_PREFIX "%s" MQTT_CONTROL_TOPIC_SUFFIX, device->device_id);
    if (snprintf_rc != MQTT_CONTROL_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding base control topic.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    snprintf_rc
        = snprintf(device->control_empty_cache_topic, MQTT_CONTROL_EMPTY_CACHE_TOPIC_LEN + 1,
            MQTT_TOPIC_PREFIX "%s" MQTT_CONTROL_EMPTY_CACHE_TOPIC_SUFFIX, device->device_id);
    if (snprintf_rc != MQTT_CONTROL_EMPTY_CACHE_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding empty cache publish topic.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    snprintf_rc
        = snprintf(device->control_consumer_prop_topic, MQTT_CONTROL_CONSUMER_PROP_TOPIC_LEN + 1,
            MQTT_TOPIC_PREFIX "%s" MQTT_CONTROL_CONSUMER_PROP_TOPIC_SUFFIX, device->device_id);
    if (snprintf_rc != MQTT_CONTROL_CONSUMER_PROP_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding Astarte purte properties topic.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    snprintf_rc
        = snprintf(device->control_producer_prop_topic, MQTT_CONTROL_PRODUCER_PROP_TOPIC_LEN + 1,
            MQTT_TOPIC_PREFIX "%s" MQTT_CONTROL_PRODUCER_PROP_TOPIC_SUFFIX, device->device_id);
    if (snprintf_rc != MQTT_CONTROL_PRODUCER_PROP_TOPIC_LEN) {
        ASTARTE_LOG_ERR("Error encoding device purge properties topic.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }
    return ASTARTE_RESULT_OK;
}
