/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/hash_map.h>
#include <zephyr/sys/hash_map_api.h>
#include <zephyr/sys/spsc_lockfree.h>

#include <data_deserialize.h>
#include <object_private.h>

#include "device_handler.h"
#include "utilities.h"

/************************************************
 *   Constants, static variables and defines    *
 ***********************************************/

LOG_MODULE_REGISTER(e2e_data, CONFIG_DATA_LOG_LEVEL);

enum message_type
{
    INDIVIDUAL,
    OBJECT,
    SET_PROPERTY,
    UNSET_PROPERTY,
};

typedef struct
{
    enum message_type type;
    const char *path;
    astarte_data_t data;
    byte_array_t raw_bson_data;
    astarte_object_entry_t *entries;
    size_t entries_length;
} message_t;

SPSC_DECLARE(messages, message_t);

typedef struct
{
    const astarte_interface_t *interface;
    struct spsc_messages messages;
    message_t messages_buf[2];
} map_value_t;

SYS_HASHMAP_DEFINE_STATIC(interface_map);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

void free_map_entry_callback(uint64_t key, uint64_t value, void *cookie);

/************************************************
 *         Global functions definition          *
 ***********************************************/

void data_init(const astarte_interface_t *interfaces[], size_t interfaces_len)
{
    CHECK_HALT(
        !sys_hashmap_is_empty(&interface_map), "Attempting to initialize non empty device data");

    for (size_t i = 0; i < interfaces_len; ++i) {

        uint64_t key
            = perfect_hash_device_interface(interfaces[i]->name, strlen(interfaces[i]->name));

        map_value_t *value = calloc(1, sizeof(map_value_t));
        CHECK_HALT(!value, "Could not allocate value required memory");
        value->interface = interfaces[i];
        struct spsc_messages tmp_spsc
            = SPSC_INITIALIZER(ARRAY_SIZE(value->messages_buf), value->messages_buf);
        memcpy(&value->messages, &tmp_spsc, sizeof(struct spsc_messages));

        sys_hashmap_insert(&interface_map, key, POINTER_TO_UINT(value), NULL);
    }
}

void data_free()
{
    sys_hashmap_clear(&interface_map, free_map_entry_callback, NULL);
}

size_t data_size(const astarte_interface_t *interfaces[], size_t interfaces_len)
{
    size_t not_received_count = 0;
    LOG_INF("Checking remaining expected messagges");

    for (size_t i = 0; i < interfaces_len; ++i) {
        const astarte_interface_t *interface = interfaces[i];

        uint64_t key = perfect_hash_device_interface(interface->name, strlen(interface->name));
        uint64_t map_value_p = { 0 };

        CHECK_HALT(!sys_hashmap_get(&interface_map, key, &map_value_p),
            "Extracting data from an unknown interface");

        map_value_t *map_value = UINT_TO_POINTER(map_value_p);

        not_received_count += spsc_consumable(&map_value->messages);
    }

    LOG_INF("Count of expected messages: %zu", not_received_count);
    return not_received_count;
}

const astarte_interface_t *data_get_interface(const char *interface_name, size_t len)
{
    uint64_t key = perfect_hash_device_interface(interface_name, len);
    uint64_t value = { 0 };

    if (sys_hashmap_get(&interface_map, key, &value)) {
        map_value_t *map_value = UINT_TO_POINTER(value);
        return map_value->interface;
    }

    return NULL;
}

int data_add_individual(const astarte_interface_t *interface, const char *path,
    const astarte_data_t *data, const optional_int64_t *timestamp)
{
    uint64_t key = perfect_hash_device_interface(interface->name, strlen(interface->name));
    uint64_t map_value_p = 0;

    if (!sys_hashmap_get(&interface_map, key, &map_value_p)) {
        LOG_ERR("Adding data from an unknown interface");
        return 1;
    }

    map_value_t *map_value = UINT_TO_POINTER(map_value_p);

    message_t *message = spsc_acquire(&map_value->messages);
    CHECK_RET_1(message == NULL, "Space for expected messages is exhausted");
    message->type = INDIVIDUAL;
    message->path = path;
    message->data = *data;
    spsc_produce(&map_value->messages);

    LOG_INF("Added individual to the expected list.");

    return 0;
}

int data_add_object(const astarte_interface_t *interface, const char *path,
    const byte_array_t *value, astarte_object_entry_t *entries, size_t entries_length,
    const optional_int64_t *timestamp)
{
    uint64_t key = perfect_hash_device_interface(interface->name, strlen(interface->name));
    uint64_t map_value_p = 0;

    if (!sys_hashmap_get(&interface_map, key, &map_value_p)) {
        LOG_ERR("Adding data from an unknown interface");
        return 1;
    }

    map_value_t *map_value = UINT_TO_POINTER(map_value_p);

    message_t *message = spsc_acquire(&map_value->messages);
    CHECK_RET_1(message == NULL, "Space for expected messages is exhausted");
    message->type = OBJECT;
    message->path = path;
    // The object data has been deserialized into object entries.
    // However, for the objects the deserialization is partially performed shallowly.
    // As a consequence the original raw bson should be preserved.
    // This is not an issue for individuals and properties, where the deserialization is complete
    // and the original bson can be discarded.
    message->raw_bson_data = *value;
    message->entries = entries;
    message->entries_length = entries_length;
    spsc_produce(&map_value->messages);

    LOG_INF("Added object to the expected list.");

    return 0;
}

int data_add_set_property(
    const astarte_interface_t *interface, const char *path, const astarte_data_t *data)
{
    uint64_t key = perfect_hash_device_interface(interface->name, strlen(interface->name));
    uint64_t map_value_p = { 0 };

    if (!sys_hashmap_get(&interface_map, key, &map_value_p)) {
        LOG_ERR("Adding data from an unknown interface");
        return 1;
    }

    map_value_t *map_value = UINT_TO_POINTER(map_value_p);

    message_t *message = spsc_acquire(&map_value->messages);
    CHECK_RET_1(message == NULL, "Space for expected messages is exhausted");
    message->type = SET_PROPERTY;
    message->path = path;
    message->data = *data;
    spsc_produce(&map_value->messages);

    LOG_INF("Added set property to the expected list.");

    return 0;
}

int data_add_unset_property(const astarte_interface_t *interface, const char *path)
{
    uint64_t key = perfect_hash_device_interface(interface->name, strlen(interface->name));
    uint64_t map_value_p = { 0 };

    if (!sys_hashmap_get(&interface_map, key, &map_value_p)) {
        LOG_ERR("Adding data from an unknown interface");
        return 1;
    }

    map_value_t *map_value = UINT_TO_POINTER(map_value_p);

    message_t *message = spsc_acquire(&map_value->messages);
    CHECK_RET_1(message == NULL, "Space for expected messages is exhausted");
    message->type = UNSET_PROPERTY;
    message->path = path;
    spsc_produce(&map_value->messages);

    LOG_INF("Added unset property to the expected list.");

    return 0;
}

int data_expected_individual(
    const char *interface_name, const char *path, const astarte_data_t *data)
{
    uint64_t key = perfect_hash_device_interface(interface_name, strlen(interface_name));
    uint64_t map_value_p = { 0 };

    if (!sys_hashmap_get(&interface_map, key, &map_value_p)) {
        LOG_ERR("Extracting data from an unknown interface");
        return 1;
    }

    map_value_t *map_value = UINT_TO_POINTER(map_value_p);

    message_t *expected = spsc_peek(&map_value->messages);
    CHECK_RET_1(expected == NULL, "No more expected messages");

    CHECK_RET_1(
        expected->type != INDIVIDUAL, "Expected an individual but got a different message type");
    CHECK_RET_1(strcmp(expected->path, path) != 0, "Received path does not match expected one");

    // Compare the expected message payload with the received one
    CHECK_RET_1(!astarte_data_are_equal(&expected->data, data), "Unexpected element received");

    free((char *) expected->path);
    data_destroy_deserialized(expected->data);

    (void *) spsc_consume(&map_value->messages);
    spsc_release(&map_value->messages);

    LOG_INF("Individual received matched expected one");
    return 0;
}

int data_expected_object(const char *interface_name, const char *path,
    const astarte_object_entry_t *entries, size_t entries_length)
{
    uint64_t key = perfect_hash_device_interface(interface_name, strlen(interface_name));
    uint64_t map_value_p = { 0 };

    if (!sys_hashmap_get(&interface_map, key, &map_value_p)) {
        LOG_ERR("Extracting data from an unknown interface");
        return 1;
    }

    map_value_t *map_value = UINT_TO_POINTER(map_value_p);

    message_t *expected = spsc_peek(&map_value->messages);
    CHECK_RET_1(expected == NULL, "No more expected messages");

    CHECK_RET_1(expected->type != OBJECT, "Expected an object but got a different message type");
    CHECK_RET_1(strcmp(expected->path, path) != 0, "Received path does not match expected one");

    // Compare the expected message payload with the received one
    CHECK_RET_1(!astarte_objects_are_equal(
                    expected->entries, expected->entries_length, entries, entries_length),
        "Unexpected element received");

    free((char *) expected->path);
    free((void *) expected->raw_bson_data.buf);
    astarte_object_entries_destroy_deserialized(expected->entries, expected->entries_length);

    (void *) spsc_consume(&map_value->messages);
    spsc_release(&map_value->messages);

    LOG_INF("Object received matched expected one");
    return 0;
}

int data_expected_set_property(
    const char *interface_name, const char *path, const astarte_data_t *data)
{
    uint64_t key = perfect_hash_device_interface(interface_name, strlen(interface_name));
    uint64_t map_value_p = { 0 };

    if (!sys_hashmap_get(&interface_map, key, &map_value_p)) {
        LOG_ERR("Extracting data from an unknown interface");
        return 1;
    }

    map_value_t *map_value = UINT_TO_POINTER(map_value_p);

    message_t *expected = spsc_peek(&map_value->messages);
    CHECK_RET_1(expected == NULL, "No more expected messages");

    CHECK_RET_1(expected->type != SET_PROPERTY,
        "Expected an set property but got a different message type");
    CHECK_RET_1(strcmp(expected->path, path) != 0, "Received path does not match expected one");

    // Compare the expected message payload with the received one
    CHECK_RET_1(!astarte_data_are_equal(&expected->data, data), "Unexpected element received");

    free((char *) expected->path);
    data_destroy_deserialized(expected->data);

    (void *) spsc_consume(&map_value->messages);
    spsc_release(&map_value->messages);

    LOG_INF("Set property received matched expected one");
    return 0;
}

int data_expected_unset_property(const char *interface_name, const char *path)
{
    uint64_t key = perfect_hash_device_interface(interface_name, strlen(interface_name));
    uint64_t map_value_p = { 0 };

    if (!sys_hashmap_get(&interface_map, key, &map_value_p)) {
        LOG_ERR("Extracting data from an unknown interface");
        return 1;
    }

    map_value_t *map_value = UINT_TO_POINTER(map_value_p);

    message_t *expected = spsc_peek(&map_value->messages);
    CHECK_RET_1(expected == NULL, "No more expected messages");

    CHECK_RET_1(expected->type != UNSET_PROPERTY,
        "Expected an unset property but got a different message type");
    CHECK_RET_1(strcmp(expected->path, path) != 0, "Received path does not match expected one");

    free((char *) expected->path);

    (void *) spsc_consume(&map_value->messages);
    spsc_release(&map_value->messages);

    LOG_INF("Set property received matched expected one");
    return 0;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

void free_map_entry_callback(uint64_t key, uint64_t value, void *cookie)
{
    map_value_t *map_value = UINT_TO_POINTER(value);

    for (message_t *message; (message = spsc_consume(&map_value->messages)) != NULL;) {
        free((char *) message->path);
        if ((message->type == INDIVIDUAL) || (message->type == SET_PROPERTY)) {
            data_destroy_deserialized(message->data);
        } else if (message->type == OBJECT) {
            free((void *) message->raw_bson_data.buf);
            astarte_object_entries_destroy_deserialized(message->entries, message->entries_length);
        }
        spsc_release(&map_value->messages);
    }

    free(map_value);
}
