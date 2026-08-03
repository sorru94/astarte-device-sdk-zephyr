/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/trans.h"
#include "test_storage_common.h"

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_trans_invalid_params)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_storage_transmission_indexes_t indexes = { 0 };

    struct astarte_storage_transmission_msg msg_valid = { .interface_name = "org.test",
        .path = "/1",
        .payload = "data",
        .payload_len = 4,
        .qos = 1,
        .timestamp = 0 };

    // Test NULL handle
    ares = astarte_storage_transmission_push(NULL, &indexes, &msg_valid);
    zassert_equal(ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for NULL handle");

    // Test NULL indexes
    ares = astarte_storage_transmission_push(&fixture->caching_handle, NULL, &msg_valid);
    zassert_equal(ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for NULL indexes");

    // Test Invalid payload configuration (length > 0 but null pointer)
    struct astarte_storage_transmission_msg msg_inv_payload = msg_valid;
    msg_inv_payload.payload_len = 5;
    msg_inv_payload.payload = NULL;
    ares = astarte_storage_transmission_push(&fixture->caching_handle, &indexes, &msg_inv_payload);
    zassert_equal(
        ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for malformed payload");

    // Test missing interface or path
    struct astarte_storage_transmission_msg msg_no_iface = msg_valid;
    msg_no_iface.interface_name = NULL;
    ares = astarte_storage_transmission_push(&fixture->caching_handle, &indexes, &msg_no_iface);
    zassert_equal(ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for NULL interface");

    // Peek/Get with NULL parameters
    ares = astarte_storage_transmission_peek(&fixture->caching_handle, NULL, &msg_valid);
    zassert_equal(
        ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for NULL indexes on peek");

    ares = astarte_storage_transmission_get(&fixture->caching_handle, &indexes, NULL);
    zassert_equal(
        ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for NULL message on get");
}

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_transmission_queue)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_storage_transmission_indexes_t indexes = { 0 };

    // Verify default indexes on empty queue
    ares = astarte_storage_transmission_get_indexes(&fixture->caching_handle, &indexes);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Get indexes failed");
    zassert_equal(indexes.head, 1, "Empty queue head should default to 1");
    zassert_equal(indexes.tail, 0, "Empty queue tail should default to 0");

    // Setup a mock transmission message
    struct astarte_storage_transmission_msg msg_push
        = { .interface_name = "org.astarteplatform.test.Interface",
              .path = "/test/path/1",
              .payload = "sensor_data",
              .payload_len = 11,
              .qos = 1,
              .timestamp = 1672531199000 };

    // Push message into the queue
    ares = astarte_storage_transmission_push(&fixture->caching_handle, &indexes, &msg_push);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Transmission push failed");
    zassert_equal(indexes.tail, 1, "Tail should have incremented to 1");

    // Peek at the message (reads without deleting)
    struct astarte_storage_transmission_msg msg_peek = { 0 };
    ares = astarte_storage_transmission_peek(&fixture->caching_handle, &indexes, &msg_peek);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Transmission peek failed");
    zassert_equal(msg_peek.qos, msg_push.qos, "QoS mismatch on peek");
    zassert_equal(msg_peek.timestamp, msg_push.timestamp, "Timestamp mismatch on peek");
    zassert_mem_equal(msg_peek.interface_name, msg_push.interface_name,
        strlen(msg_push.interface_name) + 1, "Interface name mismatch");
    zassert_mem_equal(msg_peek.path, msg_push.path, strlen(msg_push.path) + 1, "Path mismatch");

    // Cleanup peeked message
    astarte_storage_transmission_msg_cleanup(&msg_peek);

    // Get the message (reads AND deletes from queue, incrementing head)
    struct astarte_storage_transmission_msg msg_get = { 0 };
    ares = astarte_storage_transmission_get(&fixture->caching_handle, &indexes, &msg_get);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Transmission get failed");
    zassert_equal(indexes.head, 2, "Head should have incremented to 2 after get");

    // Cleanup retrieved message
    astarte_storage_transmission_msg_cleanup(&msg_get);

    // Verify the queue is now conceptually empty
    ares = astarte_storage_transmission_peek(&fixture->caching_handle, &indexes, &msg_peek);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Queue should be empty after get");
}

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_transmission_discard)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_storage_transmission_indexes_t indexes = { 0 };
    ares = astarte_storage_transmission_get_indexes(&fixture->caching_handle, &indexes);

    struct astarte_storage_transmission_msg msg_push
        = { .interface_name = "org.astarteplatform.test.Discard",
              .path = "/discard/test",
              .payload = "data",
              .payload_len = 4,
              .qos = 0,
              .timestamp = 1000 };

    // Push a message
    ares = astarte_storage_transmission_push(&fixture->caching_handle, &indexes, &msg_push);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Push failed");

    uint32_t expected_head_after_discard = indexes.head + 1;

    // Discard the message directly
    ares = astarte_storage_transmission_discard(&fixture->caching_handle, &indexes);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Discard failed");
    zassert_equal(indexes.head, expected_head_after_discard, "Head should increment upon discard");

    // Verify it is gone
    struct astarte_storage_transmission_msg msg_peek = { 0 };
    ares = astarte_storage_transmission_peek(&fixture->caching_handle, &indexes, &msg_peek);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Message should not exist after discard");
}

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_trans_zero_payload)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_storage_transmission_indexes_t indexes = { 0 };
    ares = astarte_storage_transmission_get_indexes(&fixture->caching_handle, &indexes);

    struct astarte_storage_transmission_msg msg_push
        = { .interface_name = "org.astarteplatform.test.Empty",
              .path = "/empty",
              .payload = NULL,
              .payload_len = 0,
              .qos = 2,
              .timestamp = 1000 };

    // Push the zero-length message
    ares = astarte_storage_transmission_push(&fixture->caching_handle, &indexes, &msg_push);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Push zero-payload failed");

    // Retrieve it
    struct astarte_storage_transmission_msg msg_get = { 0 };
    ares = astarte_storage_transmission_get(&fixture->caching_handle, &indexes, &msg_get);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Get zero-payload failed");

    // Verify payload configuration safely navigated `extract_payload`
    zassert_equal(msg_get.payload_len, 0, "Payload length should be exactly 0");
    zassert_is_null(msg_get.payload, "Payload pointer should be strictly NULL");
    zassert_mem_equal(msg_get.interface_name, msg_push.interface_name,
        strlen(msg_push.interface_name) + 1, "Interface mismatch");

    astarte_storage_transmission_msg_cleanup(&msg_get);
}

ZTEST_F(astarte_device_sdk_storage, test_device_astarte_storage_trans_index_wrap_around)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_storage_transmission_indexes_t indexes = { 0 };

    // To test the complex arithmetic sequence math in `get_indexes`, we simulate
    // a queue that has wrapped around the 32-bit integer limit.
    // We manually insert entries at keys: (UINT32_MAX - 1), UINT32_MAX, 0, 1.

    uint8_t dummy_buf[1] = { 0xFF }; // A single byte payload to satisfy key_value_insert
    char key[12]; // MAX_UINT32_STR_LEN is 11, +1 for safety

    uint32_t expected_head = UINT32_MAX - 1;
    uint32_t expected_tail = 1;

    // Insert element 1: UINT32_MAX - 1
    snprintf(key, sizeof(key), "%010u", UINT32_MAX - 1);
    astarte_key_value_insert(
        &fixture->caching_handle.trans_storage, key, dummy_buf, sizeof(dummy_buf));

    // Insert element 2: UINT32_MAX
    snprintf(key, sizeof(key), "%010u", UINT32_MAX);
    astarte_key_value_insert(
        &fixture->caching_handle.trans_storage, key, dummy_buf, sizeof(dummy_buf));

    // Insert element 3: 0
    snprintf(key, sizeof(key), "%010u", 0);
    astarte_key_value_insert(
        &fixture->caching_handle.trans_storage, key, dummy_buf, sizeof(dummy_buf));

    // Insert element 4: 1
    snprintf(key, sizeof(key), "%010u", 1);
    astarte_key_value_insert(
        &fixture->caching_handle.trans_storage, key, dummy_buf, sizeof(dummy_buf));

    // Trigger the index calculation mechanism
    ares = astarte_storage_transmission_get_indexes(&fixture->caching_handle, &indexes);

    // Verify the math worked successfully
    zassert_equal(ares, ASTARTE_RESULT_OK, "Get indexes failed after wrap-around simulation");
    zassert_equal(indexes.head, expected_head, "Calculated head %u does not match expected %u",
        indexes.head, expected_head);
    zassert_equal(indexes.tail, expected_tail, "Calculated tail %u does not match expected %u",
        indexes.tail, expected_tail);
}
