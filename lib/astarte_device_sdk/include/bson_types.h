/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BSON_TYPES_H
#define BSON_TYPES_H

/**
 * @file bson_types.h
 * @brief Astarte BSON types definitions.
 */

#include <stdint.h>

// clang-format off

/** @brief Single byte value for the BSON type double */
#define ASTARTE_BSON_TYPE_DOUBLE    ((uint8_t)0x01)
/** @brief Single byte value for the BSON type string */
#define ASTARTE_BSON_TYPE_STRING    ((uint8_t)0x02)
/** @brief Single byte value for the BSON type document */
#define ASTARTE_BSON_TYPE_DOCUMENT  ((uint8_t)0x03)
/** @brief Single byte value for the BSON type array */
#define ASTARTE_BSON_TYPE_ARRAY     ((uint8_t)0x04)
/** @brief Single byte value for the BSON type binary */
#define ASTARTE_BSON_TYPE_BINARY    ((uint8_t)0x05)
/** @brief Single byte value for the BSON type boolean */
#define ASTARTE_BSON_TYPE_BOOLEAN   ((uint8_t)0x08)
/** @brief Single byte value for the BSON type datetime */
#define ASTARTE_BSON_TYPE_DATETIME  ((uint8_t)0x09)
/** @brief Single byte value for the BSON type 32 bits integer */
#define ASTARTE_BSON_TYPE_INT32     ((uint8_t)0x10)
/** @brief Single byte value for the BSON type 64 bits integer */
#define ASTARTE_BSON_TYPE_INT64     ((uint8_t)0x12)

/** @brief Single byte value for the BSON subtype generic binary (used in the binary type) */
#define ASTARTE_BSON_SUBTYPE_DEFAULT_BINARY ((uint8_t)0x00)

// clang-format on

#endif // BSON_TYPES_H
