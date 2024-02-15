/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/dlist.h>
#include <zephyr/sys/util.h>

#include "astarte_device_sdk/error.h"
#include "astarte_device_sdk/interface.h"

#include "introspection.h"

LOG_MODULE_REGISTER( // NOLINT
    astarte_introspection, CONFIG_ASTARTE_DEVICE_SDK_INTROSPECTION_LOG_LEVEL); // NOLINT

typedef struct
{
    const astarte_interface_t *interface;
    /** @cond INTERNAL_HIDDEN */
    sys_dnode_t node;
    /** @endcond */
} introspection_node_t;

/**
 * @brief Function used to find a `introspection_node_t` from an interface_name
 *
 * @details The passed interface_name is used as key to search in the inner structure
 *
 * @param[in] introspection a pointer to an introspection struct
 * @param[in] interface_name Interface name used in the comparison
 * @return The node whose interface name matches the one passed
 * @retval NULL no interface matched the passed name
 */
static introspection_node_t *find_node_by_name(
    introspection_t *introspection, char *interface_name);

/**
 * @brief Counts the number of digits of the passed paramter `num`
 *
 * @return The count of digits in num
 */
static uint8_t get_digit_count(uint32_t num);

/**
 * @brief Frees the node passed
 *
 * @details This function should be called only from a 'safe' context.
 * Nodes from the list should be freely removable
 *
 * @param[in] alloc_node a pointer to a dynamically allocated introspection_node_t
 */
static inline void node_free(introspection_node_t *alloc_node);

astarte_err_t introspection_init(introspection_t *introspection)
{
    if (!introspection) {
        LOG_ERR("Called introspection initialize function with invalid pointer"); // NOLINT
        return ASTARTE_ERR_INVALID_PARAM;
    }

    *introspection = (introspection_t){
        .list = calloc(1, sizeof(sys_dlist_t)),
    };

    if (!introspection->list) {
        return ASTARTE_ERR_OUT_OF_MEMORY;
    }

    // the list passed here has to be dynamically allocated
    // inside this function the pointer of the list gets stored as a head and tail pointer
    // and will be later used to check for list emptyness and other functionalities
    sys_dlist_init(introspection->list);

    return ASTARTE_OK;
}

astarte_err_t introspection_add(
    introspection_t *introspection, const astarte_interface_t *interface)
{
    introspection_node_t *check_alloc_node
        = find_node_by_name(introspection, (char *) interface->name);

    if (check_alloc_node) {
        return ASTARTE_ERR_INTERFACE_ALREADY_PRESENT;
    }

    introspection_node_t *alloc_node = calloc(1, sizeof(introspection_node_t));

    if (!alloc_node) {
        return ASTARTE_ERR_OUT_OF_MEMORY;
    }

    *alloc_node = (introspection_node_t){
        .interface = interface,
        .node = {},
    };

    sys_dnode_init(&alloc_node->node);

    sys_dlist_append(introspection->list, &alloc_node->node);

    return ASTARTE_OK;
}

const astarte_interface_t *introspection_get(introspection_t *introspection, char *interface_name)
{
    introspection_node_t *alloc_node = find_node_by_name(introspection, interface_name);

    if (!alloc_node) {
        return NULL;
    }

    return alloc_node->interface;
}

astarte_err_t introspection_remove(introspection_t *introspection, char *interface_name)
{
    introspection_node_t *alloc_node = find_node_by_name(introspection, interface_name);

    if (!alloc_node) {
        return ASTARTE_ERR_INTERFACE_NOT_FOUND;
    }

    node_free(alloc_node);

    return ASTARTE_OK;
}

size_t introspection_get_string_size(introspection_t *introspection)
{
    introspection_node_t *iter_node = NULL;
    size_t len = 0;

    SYS_DLIST_FOR_EACH_CONTAINER(introspection->list, iter_node, node) // NOLINT
    {
        size_t name_len = strnlen(iter_node->interface->name, INTERFACE_NAME_MAX_SIZE);
        size_t major_len = get_digit_count(iter_node->interface->major_version);
        size_t minor_len = get_digit_count(iter_node->interface->minor_version);
        // size of the separators 3 (name:1:0; 2 ':' and 1 ';')
        // the separator ';' of the last interface is not present in an introspection
        // but we use it in the count as the byte needed for the null terminator char
        const static size_t separator_len = 3;

        len += name_len + major_len + minor_len + separator_len;
    }

    // MAX to correctly handle the case of no interfaces
    return MAX(1, len);
}

void introspection_fill_string(introspection_t *introspection, char *buffer, size_t buffer_size)
{
    introspection_node_t *iter_node = NULL;
    size_t result_len = 0;

    SYS_DLIST_FOR_EACH_CONTAINER(introspection->list, iter_node, node) // NOLINT
    {
        result_len += snprintf(buffer + result_len, buffer_size - result_len, "%s:%u:%u;",
            iter_node->interface->name, iter_node->interface->major_version,
            iter_node->interface->minor_version);
    }

    // to ensure that even the case of an empty collection gets handled correctly
    buffer[buffer_size - 1] = '\0';
}

void introspection_free(introspection_t introspection)
{
    introspection_node_t *alloc_node = NULL;
    introspection_node_t *next = NULL;

    SYS_DLIST_FOR_EACH_CONTAINER_SAFE(introspection.list, alloc_node, next, node)
    {
        node_free(alloc_node);
    }

    free(introspection.list);
}

static inline void node_free(introspection_node_t *alloc_node)
{
    sys_dlist_remove(&alloc_node->node);
    free((void *) alloc_node);
}

static introspection_node_t *find_node_by_name(introspection_t *introspection, char *interface_name)
{
    introspection_node_t *iter_node = NULL;

    SYS_DLIST_FOR_EACH_CONTAINER(introspection->list, iter_node, node) // NOLINT
    {
        if (strncmp(interface_name, iter_node->interface->name, INTERFACE_NAME_MAX_SIZE) == 0) {
            return iter_node;
        }
    }

    return NULL;
}

static uint8_t get_digit_count(uint32_t num)
{
    const uint8_t max_digit = 9;
    const uint8_t max_digit_plus_1 = max_digit + 1;

    uint8_t count = 1;

    while (num > max_digit) {
        num /= max_digit_plus_1;
        count += 1;
    }

    return count;
}
