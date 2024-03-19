/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_INTERFACE_H
#define ASTARTE_DEVICE_SDK_INTERFACE_H

/**
 * @file interface.h
 * @brief [Astarte
 * interface](https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html)
 * representation.
 */

/**
 * @defgroup interface Interface
 * @ingroup astarte_device_sdk
 * @{
 */

#include "astarte_device_sdk/result.h"

/**
 * @brief interface ownership
 *
 * This enum represents the possible ownerhips of an Astarte interface
 * https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#astarte-interface-schema-ownership
 */
typedef enum
{
    ASTARTE_INTERFACE_OWNERSHIP_DEVICE = 1, /**< Device owned interface */
    ASTARTE_INTERFACE_OWNERSHIP_SERVER, /**< Server owned interface*/
} astarte_interface_ownership_t;

/**
 * @brief interface type
 *
 * This enum represents the possible types of an Astarte interface
 * https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#astarte-interface-schema-type
 */
typedef enum
{
    ASTARTE_INTERFACE_TYPE_DATASTREAM = 1, /**< Datastream interface */
    ASTARTE_INTERFACE_TYPE_PROPERTIES, /**< Properties interface */
} astarte_interface_type_t;

/**
 * @brief interface aggregation
 *
 * This enum represents the possible Astarte interface aggregation
 * https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#astarte-interface-schema-aggregation
 */
typedef enum
{
    ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL = 1, /**< Aggregation individual */
    ASTARTE_INTERFACE_AGGREGATION_OBJECT, /**< Aggregation object */
} astarte_interface_aggregation_t;

/**
 * @brief Maximum allowed length of an interface name
 *
 * Includes the terminating null char.
 * https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#astarte-interface-schema-interface_name
 */
#define ASTARTE_INTERFACE_NAME_MAX_SIZE 128

/**
 * @brief Astarte interface definition
 *
 * This struct represents a subset of the information contained in an Astarte interface, and can be
 * used to specify some details about a specific interface.
 */
typedef struct
{
    /** @brief Interface name */
    const char *name;
    /** @brief Major version */
    uint32_t major_version;
    /** @brief Minor version */
    uint32_t minor_version;
    /** @brief Ownership, see #astarte_interface_ownership_t */
    astarte_interface_ownership_t ownership;
    /** @brief Type, see #astarte_interface_type_t */
    astarte_interface_type_t type;
    /** @brief Aggregation, see #astarte_interface_aggregation_t */
    astarte_interface_aggregation_t aggregation;
} astarte_interface_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validate an Astarte interface.
 *
 * @param[in] interface Interface to validate.
 * @return ASTARTE_RESULT_OK on success, otherwise an error is returned.
 */
astarte_result_t astarte_interface_validate(const astarte_interface_t *interface);

#ifdef __cplusplus
}
#endif

/** @} */

#endif // ASTARTE_DEVICE_SDK_INTERFACE_H
