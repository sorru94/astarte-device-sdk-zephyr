/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "introspection.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/dlist.h>
#include <zephyr/sys/util.h>

#include "astarte_device_sdk/error.h"
#include "astarte_device_sdk/interface.h"

LOG_MODULE_REGISTER( // NOLINT
    astarte_introspection, CONFIG_ASTARTE_DEVICE_SDK_INTROSPECTION_LOG_LEVEL); // NOLINT

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

/**
 * @brief Check whether an interface is valid and compatible with this introspection
 *
 * @details The previous node gets returned in `introspection_node` parameter and should be
 * replaced with the new interface if it passes the check.
 *
 * @param[in] introspection a pointer to an introspection struct initialized using
 * #introspection_new
 * @param[in] interface the pointer to an interface struct
 * @param[out] introspection_node Output pointer that if not NULL will be set to the
 * to the location of the old node found, or NULL if there was no previous node matching
 * the passed interface->name
 * @return result code of the operation:
 * @retval ASTARTE_ERR_INTERFACE_INVALID_VERSION_ZERO the interface has both major and
 * minor version set to 0
 * @retval ASTARTE_ERR_INTERFACE_CONFLICTING the passed interface colflicts with the
 * previously defined one with the same name
 * @retval ASTARTE_OK check completed successfully
 */
static astarte_err_t check_interface_update(introspection_t *introspection,
    const astarte_interface_t *interface, introspection_node_t **introspection_node);

/**
 * @brief Allocates and append a new node to the introspection struct passed
 *
 * @param[in,out] introspection a pointer to an introspection struct initialized using
 * #introspection_new
 * @param[in] interface the pointer to an interface struct
 * @return result code of the operation:
 * @retval ASTARTE_ERR_INTERFACE_INVALID_VERSION_ZERO the interface has both major and
 * minor version set to 0
 * @retval ASTARTE_OK check completed successfully
 */
static astarte_err_t append_introspection_node(
    introspection_t *introspection, const astarte_interface_t *interface);

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
    astarte_err_t res = astarte_interface_validate(interface);
    if (res != ASTARTE_OK) {
        return res;
    }

    introspection_node_t *check_alloc_node
        = find_node_by_name(introspection, (char *) interface->name);
    if (check_alloc_node) {
        return ASTARTE_ERR_INTERFACE_ALREADY_PRESENT;
    }

    res = append_introspection_node(introspection, interface);
    if (res != ASTARTE_OK) {
        return res;
    }

    return ASTARTE_OK;
}

astarte_err_t introspection_update(
    introspection_t *introspection, const astarte_interface_t *interface)
{

    astarte_err_t res = astarte_interface_validate(interface);
    if (res != ASTARTE_OK) {
        return res;
    }

    introspection_node_t *old_node = NULL;
    res = check_interface_update(introspection, interface, &old_node);
    if (res != ASTARTE_OK) {
        return res;
    }

    if (old_node) {
        // we are updating an old node
        old_node->interface = interface;
    } else {
        // no previous interface with the same name so we append a new node
        res = append_introspection_node(introspection, interface);
        if (res != ASTARTE_OK) {
            return res;
        }
    }

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

const introspection_node_t *introspection_iter(introspection_t *introspection)
{
    // this is just used in the successive macro call to extract the type of the container
    introspection_node_t *iter_node = NULL;

    return SYS_DLIST_PEEK_HEAD_CONTAINER(introspection->list, iter_node, node);
}

const introspection_node_t *introspection_iter_next(
    introspection_t *introspection, const introspection_node_t *current)
{
    // NOLINTNEXTLINE(incompatible-pointer-types-discards-qualifiers)
    return SYS_DLIST_PEEK_NEXT_CONTAINER(introspection->list, current, node);
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

static astarte_err_t check_interface_update(introspection_t *introspection,
    const astarte_interface_t *interface, introspection_node_t **introspection_node)
{
    introspection_node_t *old_node = find_node_by_name(introspection, (char *) interface->name);

    if (old_node) {
        const astarte_interface_t *old_interface = old_node->interface;

        LOG_WRN("Trying to add an interface already present in introspection"); // NOLINT

        // Check if ownership and type are the same
        if ((interface->ownership != old_interface->ownership)
            || (interface->type != old_interface->type)) {
            LOG_ERR("Interface ownership/type conflicts with the one in introspection"); // NOLINT
            return ASTARTE_ERR_INTERFACE_CONFLICTING;
        }

        // Check if major versions align correctly
        if (interface->major_version < old_interface->major_version) {
            LOG_ERR("Interface with smaller major version than one in introspection"); // NOLINT
            return ASTARTE_ERR_INTERFACE_CONFLICTING;
        }

        // Check if minor versions aligns correctly
        if ((interface->major_version == old_interface->major_version)
            && (interface->minor_version <= old_interface->minor_version)) {
            // NOLINTNEXTLINE
            LOG_ERR(
                "Interface with same major version and smaller or equal minor version than the one "
                "in introspection");
            return ASTARTE_ERR_INTERFACE_CONFLICTING;
        }

        LOG_WRN("Interface '%s' can be overwritten with new version '%u.%u'", // NOLINT
            interface->name, interface->major_version, interface->minor_version);
    }

    // in the case of no old node this will be set to NULL and that is expected
    if (introspection_node) {
        *introspection_node = old_node;
    }

    return ASTARTE_OK;
}

static astarte_err_t append_introspection_node(
    introspection_t *introspection, const astarte_interface_t *interface)
{
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
