/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device_tx.h"

#include "bson_serializer.h"
#include "data_validation.h"
#include "individual_private.h"
#include "object_private.h"

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(device_transmission, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_TX_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Publish data.
 *
 * @param[in] device Handle to the device instance.
 * @param[in] interface_name Interface where to publish data.
 * @param[in] path Path where to publish data.
 * @param[in] data Data to publish.
 * @param[in] data_size Size of data to publish.
 * @param[in] qos Quality of service for MQTT publish.
 * @return ASTARTE_RESULT_OK if publish has been successful, an error code otherwise.
 */
static astarte_result_t publish_data(astarte_device_handle_t device, const char *interface_name,
    const char *path, void *data, int data_size, int qos);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_device_tx_stream_individual(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_individual_t individual,
    const int64_t *timestamp)
{
    astarte_bson_serializer_t bson = { 0 };
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called stream individual function when the device is not connected.");
        ares = ASTARTE_RESULT_DEVICE_NOT_READY;
        goto exit;
    }

    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Couldn't find interface in device introspection (%s).", interface_name);
        ares = ASTARTE_RESULT_INTERFACE_NOT_FOUND;
        goto exit;
    }

    ares = data_validation_individual_datastream(interface, path, individual, timestamp);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Device individual data validation failed.");
        goto exit;
    }

    int qos = 0;
    ares = astarte_interface_get_qos(interface, path, &qos);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed getting QoS for individual data streaming.");
        goto exit;
    }

    ares = astarte_bson_serializer_init(&bson);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not initialize the bson serializer");
        goto exit;
    }
    ares = astarte_individual_serialize(&bson, "v", individual);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }

    if (timestamp) {
        astarte_bson_serializer_append_datetime(&bson, "t", *timestamp);
    }
    astarte_bson_serializer_append_end_of_document(&bson);

    int len = 0;
    void *data = (void *) astarte_bson_serializer_get_serialized(bson, &len);
    if (!data) {
        ASTARTE_LOG_ERR("Error during BSON serialization.");
        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }
    if (len < 0) {
        ASTARTE_LOG_ERR("BSON document is too long for MQTT publish.");
        ASTARTE_LOG_ERR("Interface: %s, path: %s", interface_name, path);
        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    ares = publish_data(device, interface_name, path, data, len, qos);

exit:
    astarte_bson_serializer_destroy(&bson);
    return ares;
}

astarte_result_t astarte_device_tx_stream_aggregated(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_object_entry_t *entries,
    size_t entries_len, const int64_t *timestamp)
{
    astarte_bson_serializer_t outer_bson = { 0 };
    astarte_bson_serializer_t inner_bson = { 0 };
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called stream aggregated function when the device is not connected.");
        ares = ASTARTE_RESULT_DEVICE_NOT_READY;
        goto exit;
    }

    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Couldn't find interface in device introspection (%s).", interface_name);
        ares = ASTARTE_RESULT_INTERFACE_NOT_FOUND;
        goto exit;
    }

    // This validation section has been moved outside the data_validation_aggregated_datastream
    // function as it is only required for transmission.
    if (interface->mappings_length != entries_len) {
        ASTARTE_LOG_ERR("Incomplete aggregated datastream (%s/%s).", interface->name, path);
        ares = ASTARTE_RESULT_INCOMPLETE_AGGREGATION_OBJECT;
        goto exit;
    }

    ares = data_validation_aggregated_datastream(interface, path, entries, entries_len, timestamp);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Device aggregated data validation failed.");
        goto exit;
    }

    int qos = 0;
    ares = astarte_interface_get_qos(interface, NULL, &qos);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Failed getting QoS for aggregated data streaming.");
        goto exit;
    }

    ares = astarte_bson_serializer_init(&outer_bson);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not initialize the bson serializer");
        goto exit;
    }
    ares = astarte_bson_serializer_init(&inner_bson);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Could not initialize the bson serializer");
        goto exit;
    }
    ares = astarte_object_entries_serialize(&inner_bson, entries, entries_len);
    if (ares != ASTARTE_RESULT_OK) {
        goto exit;
    }
    astarte_bson_serializer_append_end_of_document(&inner_bson);
    int inner_len = 0;
    const void *inner_data = astarte_bson_serializer_get_serialized(inner_bson, &inner_len);
    if (!inner_data) {
        ASTARTE_LOG_ERR("Error during BSON serialization");
        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }
    if (inner_len < 0) {
        ASTARTE_LOG_ERR("BSON document is too long for MQTT publish.");
        ASTARTE_LOG_ERR("Interface: %s, path: %s", interface_name, path);

        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    astarte_bson_serializer_append_document(&outer_bson, "v", inner_data);

    if (timestamp) {
        astarte_bson_serializer_append_datetime(&outer_bson, "t", *timestamp);
    }
    astarte_bson_serializer_append_end_of_document(&outer_bson);

    int len = 0;
    const void *data = astarte_bson_serializer_get_serialized(outer_bson, &len);
    if (!data) {
        ASTARTE_LOG_ERR("Error during BSON serialization");
        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }
    if (len < 0) {
        ASTARTE_LOG_ERR("BSON document is too long for MQTT publish.");
        ASTARTE_LOG_ERR("Interface: %s, path: %s", interface_name, path);

        ares = ASTARTE_RESULT_BSON_SERIALIZER_ERROR;
        goto exit;
    }

    ares = publish_data(device, interface_name, path, (void *) data, len, qos);

exit:
    astarte_bson_serializer_destroy(&outer_bson);
    astarte_bson_serializer_destroy(&inner_bson);

    return ares;
}

astarte_result_t astarte_device_tx_set_property(astarte_device_handle_t device,
    const char *interface_name, const char *path, astarte_individual_t individual)
{
    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called set property function when the device is not connected.");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Couldn't find interface in device introspection (%s).", interface_name);
        return ASTARTE_RESULT_INTERFACE_NOT_FOUND;
    }

    astarte_result_t ares = data_validation_set_property(interface, path, individual);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Property data validation failed.");
        return ares;
    }

    return astarte_device_stream_individual(device, interface_name, path, individual, NULL);
}

astarte_result_t astarte_device_tx_unset_property(
    astarte_device_handle_t device, const char *interface_name, const char *path)
{
    if (device->connection_state != DEVICE_CONNECTED) {
        ASTARTE_LOG_ERR("Called unset property function when the device is not connected.");
        return ASTARTE_RESULT_DEVICE_NOT_READY;
    }

    const astarte_interface_t *interface = introspection_get(
        &device->introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Couldn't find interface in device introspection (%s).", interface_name);
        return ASTARTE_RESULT_INTERFACE_NOT_FOUND;
    }

    astarte_result_t ares = data_validation_unset_property(interface, path);
    if (ares != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Device property unset failed.");
        return ares;
    }

    return publish_data(device, interface_name, path, "", 0, 2);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t publish_data(astarte_device_handle_t device, const char *interface_name,
    const char *path, void *data, int data_size, int qos)
{
    if (path[0] != '/') {
        ASTARTE_LOG_ERR("Invalid path: %s (must be start with /)", path);
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    if (qos < 0 || qos > 2) {
        ASTARTE_LOG_ERR("Invalid QoS: %d (must be 0, 1 or 2)", qos);
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    char *topic = NULL;
    int ret = asprintf(&topic, CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME "/%s/%s%s", device->device_id,
        interface_name, path);
    if (ret < 0) {
        ASTARTE_LOG_ERR("Error encoding MQTT topic.");
        ASTARTE_LOG_ERR("Might be out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    astarte_mqtt_publish(&device->astarte_mqtt, topic, data, data_size, qos, NULL);
    free(topic);

    return ASTARTE_RESULT_OK;
}
