/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INTROSPECTION_H
#define INTROSPECTION_H

/**
 * @file
 * @brief Astarte introspection representation
 * https://docs.astarte-platform.org/astarte/latest/080-mqtt-v1-protocol.html#introspection
 */

#include <zephyr/sys/dlist.h>

#include "astarte_device_sdk/interface.h"
#include "astarte_device_sdk/result.h"

/** @brief Introspection struct. */
typedef struct
{
    /** @cond INTERNAL_HIDDEN */
    sys_dlist_t *list;
    /** @endcond */
} introspection_t;

/** @brief Introspection node, used internally. */
typedef struct
{
    /** @cond INTERNAL_HIDDEN */
    const astarte_interface_t *interface;
    sys_dnode_t node;
    /** @endcond */
} introspection_node_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the introspection struct
 *
 * @details The resulting struct should be deallocated by the user by calling #introspection_free
 *
 * @param[out] introspection a pointer to an uninitialized introspection struct
 * @return result code of the operation
 * @retval ASTARTE_RESULT_INVALID_PARAM invalid pointer passed as argument
 * @retval ASTARTE_RESULT_OUT_OF_MEMORY couldn't allocate the memory for the inner data
 * @retval ASTARTE_RESULT_OK initialization completed successfully
 */
astarte_result_t introspection_init(introspection_t *introspection);

/**
 * @brief Adds an interface to the introspection list
 *
 * @details No update will be performed by this function. If an interface with the
 * Same name is already present an error will be returned.
 *
 * @param[in,out] introspection a pointer to an introspection struct initialized using
 * #introspection_init
 * @param[in] interface the pointer to an interface struct
 * @return result code of the operation:
 * @retval ASTARTE_RESULT_INTERFACE_ALREADY_PRESENT the interface was already present
 * @retval ASTARTE_RESULT_OUT_OF_MEMORY couldn't allocate the memory for the new node
 * @retval ASTARTE_RESULT_OK insertion completed successfully
 */
astarte_result_t introspection_add(
    introspection_t *introspection, const astarte_interface_t *interface);

/**
 * @brief Updates or adds an interface in the introspection list
 *
 * @details If no interface with the same name as the one passed exists,
 * the function adds the interface to the introspection list. If an interface matching the name is
 * present, the function checks the new interface to ensure it is a valid interface update.
 *
 * @param[in,out] introspection a pointer to an introspection struct initialized using
 * #introspection_init
 * @param[in] interface the pointer to an interface struct
 * @return result code of the operation:
 * @retval ASTARTE_RESULT_INTERFACE_ALREADY_PRESENT the interface was already present
 * @retval ASTARTE_RESULT_OUT_OF_MEMORY couldn't allocate the memory for the new node
 * @retval ASTARTE_RESULT_OK insertion completed successfully
 */
astarte_result_t introspection_update(
    introspection_t *introspection, const astarte_interface_t *interface);

/**
 * @brief Retrieves an interface from the introspection list using the name as a key
 *
 * @details A null pointer is returned if no interface is found for the passed interface_name
 *
 * @param[in] introspection a pointer to an introspection struct initialized using
 * #introspection_init
 * @param[in] interface_name the name of one of the interfaces contained in the introspection list
 * @return pointer to the interface matching the passed interface_name
 * @retval NULL no interface was found
 */
const astarte_interface_t *introspection_get(
    introspection_t *introspection, const char *interface_name);

/**
 * @brief Removes an interface from the introspection list
 *
 * @param[in,out] introspection a pointer to an introspection struct initialized using
 * #introspection_init
 * @param[in] interface_name the name of one of the interfaces contained in the introspection list
 * @return result code of the operation:
 * @retval ASTARTE_RESULT_INTERFACE_NOT_FOUND no interface matching the name was found
 * @retval ASTARTE_RESULT_OK interface removed successfully
 */
astarte_result_t introspection_remove(introspection_t *introspection, const char *interface_name);

/**
 * @brief Computes the introspection string length
 *
 * @details The returned length includes the byte for the terminating null character '\0'.
 * A buffer of the returned size in bytes can be allocated and passed to #introspection_fill_string
 *
 * @param[in] introspection a pointer to an introspection struct initialized using
 * #introspection_init
 * @return size of the introspection string in bytes, including the NULL terminator.
 */
size_t introspection_get_string_size(introspection_t *introspection);

/**
 * @brief Returns the introspection string as described in astarte documentation
 *
 * @details An empty string is returned if no interfaces got added with #introspection_add
 * The ordering of the interface names is not guaranteed and it should't be relied on
 * https://docs.astarte-platform.org/astarte/latest/080-mqtt-v1-protocol.html#introspection
 *
 * @param[in] introspection a pointer to an introspection struct initialized using
 * #introspection_init
 * @param[out] buffer result buffer correctly allocated of the size retrieved by calling
 * #introspection_get_string_size
 * @param[in] buffer_size Size of the buffer retrieved by calling #introspection_get_string_size
 */
void introspection_fill_string(introspection_t *introspection, char *buffer, size_t buffer_size);

/**
 * @brief Returns the first node of the introspection that can be used to iterate the collection
 *
 * @param[in] introspection a pointer to an introspection struct initialized using
 * #introspection_init
 * @return pointer to the first node that can be passed to #introspection_iter_next for iteration.
 * @retval NULL If no node on the introspection is found
 */
introspection_node_t *introspection_iter(introspection_t *introspection);

/**
 * @brief Returns the next node of the introspection
 *
 * @param[in] introspection a pointer to an introspection struct initialized using
 * #introspection_init
 * @param[in] current a pointer to the current introspection_node_t
 * @return pointer to the current node of the passed introspection.
 * @retval NULL If iteration is complete and there are no more nodes in this introspection
 * or if a NULL pointer gets passed as the current pointer parameter
 */
introspection_node_t *introspection_iter_next(
    introspection_t *introspection, introspection_node_t *current);

/**
 * @brief Deallocates the introspection struct
 *
 * @details The struct must first get initialized using #introspection_init
 *
 * @param[in] introspection an owned introspection struct initialized using #introspection_init
 */
void introspection_free(introspection_t introspection);

#ifdef __cplusplus
}
#endif

#endif // INTROSPECTION_H
