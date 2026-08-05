/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/clock.h>
#include <zephyr/ztest.h>

#include "device/transmission_queue.h"

#include "alloc.h"

#include <time.h>

// Define a fixture for the transmission queue tests
struct astarte_transmission_queue_fixture
{
    struct astarte_device_transmission_queue queue;
    astarte_storage_data_t storage;
    struct k_mutex test_mutex;
};

static void *astarte_transmission_queue_test_setup(void)
{
    struct astarte_transmission_queue_fixture *fixture
        = astarte_calloc(1, sizeof(struct astarte_transmission_queue_fixture));
    zassert_not_null(fixture, "Failed allocating test fixture");

    k_mutex_init(&fixture->test_mutex);

    return fixture;
}

static void astarte_transmission_queue_test_before(void *f)
{
    struct astarte_transmission_queue_fixture *fixture
        = (struct astarte_transmission_queue_fixture *) f;

    astarte_result_t ares = ASTARTE_RESULT_OK;

    /* Set a fake system time (Jan 1 2026 + 1 day) */
    struct timespec fake_time = { .tv_sec = 1767225600 + 86400, .tv_nsec = 0 };
    sys_clock_settime(SYS_CLOCK_REALTIME, &fake_time);

    k_mutex_lock(&fixture->test_mutex, K_FOREVER);
    ares = astarte_storage_init(&fixture->storage);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Storage init failed: %s", astarte_result_to_name(ares));

    ares = astarte_transmission_queue_init(&fixture->queue, &fixture->storage);

    zassert_equal(ares, ASTARTE_RESULT_OK, "Queue init failed: %s", astarte_result_to_name(ares));
}

static void astarte_transmission_queue_test_after(void *f)
{
    struct astarte_transmission_queue_fixture *fixture
        = (struct astarte_transmission_queue_fixture *) f;

    astarte_transmission_queue_clear(&fixture->queue);

    astarte_storage_destroy(&fixture->storage);
    k_mutex_unlock(&fixture->test_mutex);
}

static void astarte_transmission_queue_test_teardown(void *f)
{
    struct astarte_transmission_queue_fixture *fixture
        = (struct astarte_transmission_queue_fixture *) f;
    astarte_free(fixture);
}

ZTEST_SUITE(astarte_transmission_queue, NULL, astarte_transmission_queue_test_setup,
    astarte_transmission_queue_test_before, astarte_transmission_queue_test_after,
    astarte_transmission_queue_test_teardown);

ZTEST_F(astarte_transmission_queue, test_transmission_queue_invalid_params)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    struct astarte_device_transmission_queue_msg msg = { .interface_name = "org.test",
        .path = "/1",
        .payload = "data",
        .payload_len = 4,
        .qos = 1,
        .retention = ASTARTE_MAPPING_RETENTION_VOLATILE };

    // Test passing NULL to init
    ares = astarte_transmission_queue_init(NULL, &fixture->storage);
    zassert_equal(ares, ASTARTE_RESULT_INVALID_PARAM, "Init with NULL should return INVALID_PARAM");

    // Test passing NULL parameters to insert
    ares = astarte_transmission_queue_insert(NULL, &msg);
    zassert_equal(
        ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for NULL queue handle");

    ares = astarte_transmission_queue_insert(&fixture->queue, NULL);
    zassert_equal(ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for NULL message");

    // Test invalid payload configuration (length > 0 but null pointer)
    struct astarte_device_transmission_queue_msg inv_msg = msg;
    inv_msg.payload = NULL;
    ares = astarte_transmission_queue_insert(&fixture->queue, &inv_msg);
    zassert_equal(
        ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for malformed payload");

    // Test missing interface or path
    struct astarte_device_transmission_queue_msg no_iface = msg;
    no_iface.interface_name = NULL;
    ares = astarte_transmission_queue_insert(&fixture->queue, &no_iface);
    zassert_equal(ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for NULL interface");

    // Peek/Discard with NULL parameters
    ares = astarte_transmission_queue_peek(NULL, &msg);
    zassert_equal(
        ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for NULL queue on peek");

    ares
        = astarte_transmission_queue_discard_by_retention(NULL, ASTARTE_MAPPING_RETENTION_VOLATILE);
    zassert_equal(
        ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for NULL queue on discard");
}

ZTEST_F(astarte_transmission_queue, test_transmission_queue_insert_and_peek_volatile)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    struct astarte_device_transmission_queue_msg msg_in
        = { .interface_name = "org.astarteplatform.test.Volatile",
              .path = "/test/path",
              .payload = "mock_payload",
              .payload_len = 12,
              .qos = 1,
              .retention = ASTARTE_MAPPING_RETENTION_VOLATILE };

    // Peek on empty queue should return NOT_FOUND
    struct astarte_device_transmission_queue_msg msg_out = { 0 };
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Expected NOT_FOUND for empty queue");

    // Insert message
    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_in);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Queue insert failed: %s", astarte_result_to_name(ares));

    // Peek message
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Queue peek failed: %s", astarte_result_to_name(ares));

    // Verify data integrity
    zassert_equal(msg_out.qos, msg_in.qos, "QoS mismatch");
    zassert_equal(msg_out.payload_len, msg_in.payload_len, "Payload size mismatch");
    zassert_mem_equal(msg_out.interface_name, msg_in.interface_name,
        strlen(msg_in.interface_name) + 1, "Interface mismatch");
    zassert_mem_equal(msg_out.path, msg_in.path, strlen(msg_in.path) + 1, "Path mismatch");
    zassert_mem_equal(msg_out.payload, msg_in.payload, msg_in.payload_len, "Payload mismatch");

    // Clean up peeked message
    astarte_transmission_queue_msg_cleanup(&msg_out);

    // Discard message
    ares = astarte_transmission_queue_discard_by_retention(
        &fixture->queue, ASTARTE_MAPPING_RETENTION_VOLATILE);
    zassert_equal(
        ares, ASTARTE_RESULT_OK, "Queue discard failed: %s", astarte_result_to_name(ares));

    // Verify deletion
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Message should have been discarded");
}

ZTEST_F(astarte_transmission_queue, test_transmission_queue_insert_and_peek_storage)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    struct astarte_device_transmission_queue_msg msg_in
        = { .interface_name = "org.astarteplatform.test.Volatile",
              .path = "/test/path",
              .payload = "mock_payload",
              .payload_len = 12,
              .qos = 1,
              .retention = ASTARTE_MAPPING_RETENTION_STORED };

    // Peek on empty queue should return NOT_FOUND
    struct astarte_device_transmission_queue_msg msg_out = { 0 };
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Expected NOT_FOUND for empty queue");

    // Insert message
    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_in);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Queue insert failed: %s", astarte_result_to_name(ares));

    // Peek message
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Queue peek failed: %s", astarte_result_to_name(ares));

    // Verify data integrity
    zassert_equal(msg_out.qos, msg_in.qos, "QoS mismatch");
    zassert_equal(msg_out.payload_len, msg_in.payload_len, "Payload size mismatch");
    zassert_mem_equal(msg_out.interface_name, msg_in.interface_name,
        strlen(msg_in.interface_name) + 1, "Interface mismatch");
    zassert_mem_equal(msg_out.path, msg_in.path, strlen(msg_in.path) + 1, "Path mismatch");
    zassert_mem_equal(msg_out.payload, msg_in.payload, msg_in.payload_len, "Payload mismatch");

    // Clean up peeked message
    astarte_transmission_queue_msg_cleanup(&msg_out);

    // Discard message
    ares = astarte_transmission_queue_discard_by_retention(
        &fixture->queue, ASTARTE_MAPPING_RETENTION_STORED);
    zassert_equal(
        ares, ASTARTE_RESULT_OK, "Queue discard failed: %s", astarte_result_to_name(ares));

    // Verify deletion
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Message should have been discarded");
}

ZTEST_F(astarte_transmission_queue, test_transmission_queue_mixed_retention_ordering)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    struct astarte_device_transmission_queue_msg msg_volatile
        = { .interface_name = "org.astarteplatform.test.Volatile",
              .path = "/mixed/vol",
              .payload = "vol_data",
              .payload_len = 8,
              .qos = 1,
              .retention = ASTARTE_MAPPING_RETENTION_VOLATILE };

    struct astarte_device_transmission_queue_msg msg_discard
        = { .interface_name = "org.astarteplatform.test.Discard",
              .path = "/mixed/disc",
              .payload = "disc_data",
              .payload_len = 9,
              .qos = 0,
              .retention = ASTARTE_MAPPING_RETENTION_DISCARD };

    struct astarte_device_transmission_queue_msg msg_stored
        = { .interface_name = "org.astarteplatform.test.Stored",
              .path = "/mixed/stor",
              .payload = "stor_data",
              .payload_len = 9,
              .qos = 2,
              .retention = ASTARTE_MAPPING_RETENTION_STORED };

    // Insert messages into distinct mediums with delays to enforce unique, ascending timestamps.
    // Insertion Order: Discard -> Volatile -> Stored
    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_discard);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Push to discard queue failed");

    k_msleep(5);

    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_volatile);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Push to volatile queue failed");

    k_msleep(5);

    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_stored);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Push to stored queue failed");

    struct astarte_device_transmission_queue_msg msg_out = { 0 };

    // Peek: Should return Discard (oldest timestamp)
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Peek 1 failed");
    zassert_equal(
        msg_out.retention, ASTARTE_MAPPING_RETENTION_DISCARD, "Expected oldest to be Discard");
    astarte_transmission_queue_msg_cleanup(&msg_out);

    ares = astarte_transmission_queue_discard_by_retention(
        &fixture->queue, ASTARTE_MAPPING_RETENTION_DISCARD);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Discarding Discard failed");

    // Peek: Should return Volatile (next oldest)
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Peek 2 failed");
    zassert_equal(
        msg_out.retention, ASTARTE_MAPPING_RETENTION_VOLATILE, "Expected next to be Volatile");
    astarte_transmission_queue_msg_cleanup(&msg_out);

    ares = astarte_transmission_queue_discard_by_retention(
        &fixture->queue, ASTARTE_MAPPING_RETENTION_VOLATILE);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Discarding Volatile failed");

    // Peek: Should return Stored (newest)
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Peek 3 failed");
    zassert_equal(
        msg_out.retention, ASTARTE_MAPPING_RETENTION_STORED, "Expected last to be Stored");
    astarte_transmission_queue_msg_cleanup(&msg_out);

    ares = astarte_transmission_queue_discard_by_retention(
        &fixture->queue, ASTARTE_MAPPING_RETENTION_STORED);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Discarding Stored failed");

    // Verify Empty State
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Queue should be completely empty");
}

ZTEST_F(astarte_transmission_queue, test_transmission_queue_mixed_retention_ordering_no_time)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    // Tear down the default fixture queue
    astarte_transmission_queue_clear(&fixture->queue);

    // Roll the system clock back to the UNIX epoch (Time is NOT valid)
    struct timespec invalid_time = { .tv_sec = 0, .tv_nsec = 0 };
    sys_clock_settime(SYS_CLOCK_REALTIME, &invalid_time);

    // Re-initialize the queue so it registers system_time_valid = false
    ares = astarte_transmission_queue_init(&fixture->queue, &fixture->storage);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Queue init failed with invalid time");
    zassert_false(
        fixture->queue.system_time_valid, "Queue should have detected invalid system time");

    // Create messages for all three storage mediums
    struct astarte_device_transmission_queue_msg msg_stored
        = { .interface_name = "org.astarteplatform.test.Stored",
              .path = "/mixed/stor",
              .payload = "stor_data",
              .payload_len = 9,
              .qos = 2,
              .retention = ASTARTE_MAPPING_RETENTION_STORED };

    struct astarte_device_transmission_queue_msg msg_volatile
        = { .interface_name = "org.astarteplatform.test.Volatile",
              .path = "/mixed/vol",
              .payload = "vol_data",
              .payload_len = 8,
              .qos = 1,
              .retention = ASTARTE_MAPPING_RETENTION_VOLATILE };

    struct astarte_device_transmission_queue_msg msg_discard
        = { .interface_name = "org.astarteplatform.test.Discard",
              .path = "/mixed/disc",
              .payload = "disc_data",
              .payload_len = 9,
              .qos = 0,
              .retention = ASTARTE_MAPPING_RETENTION_DISCARD };

    // Insert them in a specific, non-intuitive order: Stored -> Volatile -> Discard
    // We omit the k_msleep() used in the valid-time tests to prove timestamps don't matter.
    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_stored);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Push to stored queue failed");

    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_volatile);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Push to volatile queue failed");

    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_discard);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Push to discard queue failed");

    struct astarte_device_transmission_queue_msg msg_out = { 0 };

    // Verify Exact Dequeue Ordering (Should match insertion order exactly)

    // Peek 1: Should return Stored
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Peek 1 failed");
    zassert_equal(
        msg_out.retention, ASTARTE_MAPPING_RETENTION_STORED, "Expected oldest to be Stored");
    astarte_transmission_queue_msg_cleanup(&msg_out);
    ares = astarte_transmission_queue_discard_by_retention(
        &fixture->queue, ASTARTE_MAPPING_RETENTION_STORED);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Discarding Stored failed");

    // Peek 2: Should return Volatile
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Peek 2 failed");
    zassert_equal(
        msg_out.retention, ASTARTE_MAPPING_RETENTION_VOLATILE, "Expected next to be Volatile");
    astarte_transmission_queue_msg_cleanup(&msg_out);
    ares = astarte_transmission_queue_discard_by_retention(
        &fixture->queue, ASTARTE_MAPPING_RETENTION_VOLATILE);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Discarding Volatile failed");

    // Peek 3: Should return Discard
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Peek 3 failed");
    zassert_equal(
        msg_out.retention, ASTARTE_MAPPING_RETENTION_DISCARD, "Expected last to be Discard");
    astarte_transmission_queue_msg_cleanup(&msg_out);
    ares = astarte_transmission_queue_discard_by_retention(
        &fixture->queue, ASTARTE_MAPPING_RETENTION_DISCARD);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Discarding Discard failed");

    // Verify Empty State
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Queue should be completely empty");
}

ZTEST_F(astarte_transmission_queue, test_transmission_queue_purge_discard)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    // Create a discard message
    struct astarte_device_transmission_queue_msg msg_discard
        = { .interface_name = "org.astarteplatform.test.Discard",
              .path = "/discard",
              .payload = "data",
              .payload_len = 4,
              .qos = 0,
              .retention = ASTARTE_MAPPING_RETENTION_DISCARD };

    // Insert message into discard queue
    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_discard);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Push to discard queue failed");

    // Verify it exists
    struct astarte_device_transmission_queue_msg msg_peek = { 0 };
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_peek);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Peek failed after discard insert");
    astarte_transmission_queue_msg_cleanup(&msg_peek);

    // Purge the discard queue
    astarte_transmission_queue_purge_discard(&fixture->queue);

    // Verify it's gone
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_peek);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Queue should be empty after purge");
}

ZTEST_F(astarte_transmission_queue, test_transmission_queue_zero_payload)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    struct astarte_device_transmission_queue_msg msg_empty
        = { .interface_name = "org.astarteplatform.test.Empty",
              .path = "/empty",
              .payload = NULL,
              .payload_len = 0,
              .qos = 2,
              .retention = ASTARTE_MAPPING_RETENTION_VOLATILE };

    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_empty);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Push zero-payload failed");

    struct astarte_device_transmission_queue_msg msg_peek = { 0 };
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_peek);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Peek zero-payload failed");

    zassert_equal(msg_peek.payload_len, 0, "Payload length should be exactly 0");
    zassert_is_null(msg_peek.payload, "Payload pointer should be strictly NULL");

    astarte_transmission_queue_msg_cleanup(&msg_peek);
}

ZTEST_F(astarte_transmission_queue, test_transmission_queue_invalid_interface_params)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    // Test missing interface struct for an interface operation
    struct astarte_device_transmission_queue_msg msg
        = { .operation = ASTARTE_TRANSMISSION_OP_ADD_INTERFACE, .interface = NULL };

    ares = astarte_transmission_queue_insert(&fixture->queue, &msg);
    zassert_equal(
        ares, ASTARTE_RESULT_INVALID_PARAM, "Expected INVALID_PARAM for NULL interface struct");
}

ZTEST_F(astarte_transmission_queue, test_transmission_queue_insert_and_peek_add_interface)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_interface_t mock_interface = { 0 };

    struct astarte_device_transmission_queue_msg msg_in
        = { .operation = ASTARTE_TRANSMISSION_OP_ADD_INTERFACE, .interface = &mock_interface };

    // Insert message
    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_in);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Queue insert failed: %s", astarte_result_to_name(ares));

    // Peek message
    struct astarte_device_transmission_queue_msg msg_out = { 0 };
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Queue peek failed: %s", astarte_result_to_name(ares));

    // Verify data integrity
    zassert_equal(msg_out.operation, ASTARTE_TRANSMISSION_OP_ADD_INTERFACE, "Operation mismatch");
    zassert_equal(msg_out.interface, msg_in.interface, "Interface pointer mismatch");
}

ZTEST_F(astarte_transmission_queue, test_transmission_queue_insert_and_peek_remove_interface)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_interface_t mock_interface = { 0 };

    struct astarte_device_transmission_queue_msg msg_in
        = { .operation = ASTARTE_TRANSMISSION_OP_REMOVE_INTERFACE, .interface = &mock_interface };

    // Insert message
    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_in);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Queue insert failed: %s", astarte_result_to_name(ares));

    // Peek message
    struct astarte_device_transmission_queue_msg msg_out = { 0 };
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Queue peek failed: %s", astarte_result_to_name(ares));

    // Verify data integrity
    zassert_equal(
        msg_out.operation, ASTARTE_TRANSMISSION_OP_REMOVE_INTERFACE, "Operation mismatch");
    zassert_equal(msg_out.interface, msg_in.interface, "Interface pointer mismatch");
}

ZTEST_F(astarte_transmission_queue, test_transmission_queue_interface_ordering)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;
    astarte_interface_t mock_interface = { 0 };

    struct astarte_device_transmission_queue_msg msg_interface
        = { .operation = ASTARTE_TRANSMISSION_OP_ADD_INTERFACE, .interface = &mock_interface };

    struct astarte_device_transmission_queue_msg msg_volatile
        = { .operation = ASTARTE_TRANSMISSION_OP_DATA,
              .interface_name = "org.astarteplatform.test.Volatile",
              .path = "/data",
              .payload = "data",
              .payload_len = 4,
              .qos = 1,
              .retention = ASTARTE_MAPPING_RETENTION_VOLATILE };

    // Insert interface message first
    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_interface);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Push to interface queue failed");

    k_msleep(5);

    // Insert volatile message second
    ares = astarte_transmission_queue_insert(&fixture->queue, &msg_volatile);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Push to volatile queue failed");

    struct astarte_device_transmission_queue_msg msg_out = { 0 };

    // Peek should return the interface message (oldest)
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Peek 1 failed");
    zassert_equal(msg_out.operation, ASTARTE_TRANSMISSION_OP_ADD_INTERFACE,
        "Expected oldest to be ADD_INTERFACE");

    // Discard the interface message
    ares = astarte_transmission_queue_discard_interface(&fixture->queue);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Discarding interface message failed");

    // Peek should now return the volatile message
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Peek 2 failed");
    zassert_equal(
        msg_out.retention, ASTARTE_MAPPING_RETENTION_VOLATILE, "Expected next to be VOLATILE");
    astarte_transmission_queue_msg_cleanup(&msg_out);

    // Discard the volatile message
    ares = astarte_transmission_queue_discard_by_retention(
        &fixture->queue, ASTARTE_MAPPING_RETENTION_VOLATILE);
    zassert_equal(ares, ASTARTE_RESULT_OK, "Discarding volatile message failed");

    // Queue should now be empty
    ares = astarte_transmission_queue_peek(&fixture->queue, &msg_out);
    zassert_equal(ares, ASTARTE_RESULT_NOT_FOUND, "Queue should be completely empty");
}
