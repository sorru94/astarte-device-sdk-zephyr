/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/mqtt.h"
#include "test_storage_common.h"

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_mqtt_flow)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    const uint16_t packet_id = 12345;

    // Create a mock MQTT message
    astarte_storage_mqtt_message_t msg_in = { .type = 3, // e.g., PUBLISH
        .qos = 2,
        .topic = "test/astarte/topic",
        .data_size = 14,
        .data = "mock_payload_1" };

    astarte_storage_mqtt_message_t msg_out = { 0 };

    // 1. Verify fetching a non-existent message returns NOT_FOUND
    ares = astarte_storage_mqtt_find_alloc(
        &fixture->caching_handle, STORAGE_MQTT_MSG_INCOMING, packet_id, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Expected NOT_FOUND for empty storage");

    // 2. Insert the message
    ares = astarte_storage_mqtt_insert(
        &fixture->caching_handle, STORAGE_MQTT_MSG_INCOMING, packet_id, &msg_in);
    zassert_equal(ares, ASTARTE_RESULT_OK, "MQTT Insert failed: %s", astarte_result_to_name(ares));

    // 3. Find and allocate the stored message
    ares = astarte_storage_mqtt_find_alloc(
        &fixture->caching_handle, STORAGE_MQTT_MSG_INCOMING, packet_id, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_OK, "MQTT Find failed: %s", astarte_result_to_name(ares));

    // 4. Verify data integrity
    zassert_equal(msg_out.type, msg_in.type, "Type mismatch");
    zassert_equal(msg_out.qos, msg_in.qos, "QoS mismatch");
    zassert_equal(msg_out.data_size, msg_in.data_size, "Data size mismatch");
    zassert_mem_equal(
        msg_out.topic, msg_in.topic, strlen(msg_in.topic) + 1, "Topic string mismatch");
    zassert_mem_equal(msg_out.data, msg_in.data, msg_in.data_size, "Payload data mismatch");

    // 5. Free the dynamically allocated output message
    astarte_storage_mqtt_find_free(&msg_out);
    zassert_is_null(msg_out.topic, "Topic pointer should be NULL after free");
    zassert_is_null(msg_out.data, "Data pointer should be NULL after free");

    // 6. Delete the message
    ares = astarte_storage_mqtt_delete(
        &fixture->caching_handle, STORAGE_MQTT_MSG_INCOMING, packet_id);
    zassert_equal(ares, ASTARTE_RESULT_OK, "MQTT Delete failed: %s", astarte_result_to_name(ares));

    // 7. Verify deletion
    ares = astarte_storage_mqtt_find_alloc(
        &fixture->caching_handle, STORAGE_MQTT_MSG_INCOMING, packet_id, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Message should have been deleted");
}
