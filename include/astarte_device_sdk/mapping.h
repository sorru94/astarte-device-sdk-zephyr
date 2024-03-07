/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_MAPPING_H
#define ASTARTE_DEVICE_SDK_MAPPING_H

/**
 * @file mapping.h
 * @brief [Astarte
 * interface
 * mapping](https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#mapping)
 * representation.
 */

/**
 * @defgroup mapping Mapping
 * @ingroup astarte_device_sdk
 * @{
 */

/**
 * @brief Allowed types in a mapping on an Astarte interface
 *
 * See more about [Astarte
 * interface mapping
 * types](https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#astarte-mapping-schema-type).
 */
typedef enum
{
    /** @brief Astarte integer type */
    ASTARTE_MAPPING_TYPE_INTEGER = 1,
    /** @brief Astarte longinteger type */
    ASTARTE_MAPPING_TYPE_LONGINTEGER = 2,
    /** @brief Astarte double type */
    ASTARTE_MAPPING_TYPE_DOUBLE = 3,
    /** @brief Astarte string type */
    ASTARTE_MAPPING_TYPE_STRING = 4,
    /** @brief Astarte binaryblob type */
    ASTARTE_MAPPING_TYPE_BINARYBLOB = 5,
    /** @brief Astarte boolean type */
    ASTARTE_MAPPING_TYPE_BOOLEAN = 6,
    /** @brief Astarte datetime type */
    ASTARTE_MAPPING_TYPE_DATETIME = 7,

    /** @brief Astarte integerarray type */
    ASTARTE_MAPPING_TYPE_INTEGERARRAY = 8,
    /** @brief Astarte longintegerarray type */
    ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY = 9,
    /** @brief Astarte doublearray type */
    ASTARTE_MAPPING_TYPE_DOUBLEARRAY = 10,
    /** @brief Astarte stringarray type */
    ASTARTE_MAPPING_TYPE_STRINGARRAY = 11,
    /** @brief Astarte binaryblobarray type */
    ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY = 12,
    /** @brief Astarte booleanarray type */
    ASTARTE_MAPPING_TYPE_BOOLEANARRAY = 13,
    /** @brief Astarte datetimearray type */
    ASTARTE_MAPPING_TYPE_DATETIMEARRAY = 14,
} astarte_mapping_type_t;

/** @} */

#endif // ASTARTE_DEVICE_SDK_MAPPING_H
