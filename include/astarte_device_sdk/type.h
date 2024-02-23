/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_TYPE_H
#define ASTARTE_DEVICE_SDK_TYPE_H

/**
 * @file type.h
 * @brief Astarte supported types definitions
 */

/**
 * @defgroup types Astarte types
 * @ingroup astarte_device_sdk
 * @{
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/bson_serializer.h"
#include "astarte_device_sdk/error.h"
#include "astarte_device_sdk/util.h"

/**
 * @brief mapping type
 *
 * This enum represents the possible types of an
 * [Astarte interface
 * mapping](https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#astarte-mapping-schema-type)
 */
typedef enum
{
    /** @brief Astarte integer type */
    TYPE_INTEGER = 1,
    /** @brief Astarte longinteger type */
    TYPE_LONGINTEGER = 2,
    /** @brief Astarte double type */
    TYPE_DOUBLE = 3,
    /** @brief Astarte string type */
    TYPE_STRING = 4,
    /** @brief Astarte binaryblob type */
    TYPE_BINARYBLOB = 5,
    /** @brief Astarte boolean type */
    TYPE_BOOLEAN = 6,
    /** @brief Astarte datetime type */
    TYPE_DATETIME = 7,

    /** @brief Astarte integerarray type */
    TYPE_INTEGERARRAY = 8,
    /** @brief Astarte longintegerarray type */
    TYPE_LONGINTEGERARRAY = 9,
    /** @brief Astarte doublearray type */
    TYPE_DOUBLEARRAY = 10,
    /** @brief Astarte stringarray type */
    TYPE_STRINGARRAY = 11,
    /** @brief Astarte binaryblobarray type */
    TYPE_BINARYBLOBARRAY = 12,
    /** @brief Astarte booleanarray type */
    TYPE_BOOLEANARRAY = 13,
    /** @brief Astarte datetimearray type */
    TYPE_DATETIMEARRAY = 14,
} astarte_type_t;

/** @brief Container for a binary blob type */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_binaryblob_t, void);
/** @brief Integer array type */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_int32_array_t, int32_t);
/** @brief Long integer array type */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_int64_array_t, int64_t);
/** @brief Double array type */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_double_array_t, double);
/** @brief String array type */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_string_array_t, const char *const);
/** @brief Bool array type */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_bool_array_t, bool);

// TODO use the ASTARTE_UTIL_DEFINE_ARRAY macro to also generate astarte_binary_arrays_t
// and refactor bson_serializer_append_binary_array to accept the array as input
/** @brief Astarte binary arrays type */
typedef struct
{
    /** @brief Array of binary blobs */
    const void *const *blobs;
    /** @brief Array of sizes of each binary blobs */
    const int *sizes;
    /** @brief Number of elements in both array blobs and sizes */
    size_t count;
} astarte_binary_arrays_t;

/** @brief Union grouping all the containers for Astarte types */
typedef union
{
    /** @brief Union variant associated with tag #TYPE_INTEGER */
    int32_t integer;
    /** @brief Union variant associated with tag #TYPE_LONGINTEGER */
    int64_t longinteger;
    /** @brief Union variant associated with tag #TYPE_DOUBLE */
    double dbl;
    /** @brief Union variant associated with tag #TYPE_STRING */
    const char *string;
    /** @brief Union variant associated with tag #TYPE_BINARYBLOB */
    astarte_binaryblob_t binaryblob;
    /** @brief Union variant associated with tag #TYPE_BOOLEAN */
    bool boolean;
    /** @brief Union variant associated with tag #TYPE_DATETIME */
    int64_t datetime;

    /** @brief Union variant associated with tag #TYPE_INTEGERARRAY */
    astarte_int32_array_t integer_array;
    /** @brief Union variant associated with tag #TYPE_LONGINTEGERARRAY */
    astarte_int64_array_t longinteger_array;
    /** @brief Union variant associated with tag #TYPE_DOUBLEARRAY */
    astarte_double_array_t double_array;
    /** @brief Union variant associated with tag #TYPE_STRINGARRAY */
    astarte_string_array_t string_array;
    /** @brief Union variant associated with tag #TYPE_BINARYBLOBARRAY */
    astarte_binary_arrays_t binaryblob_array;
    /** @brief Union variant associated with tag #TYPE_BOOLEANARRAY */
    astarte_bool_array_t boolean_array;
    /** @brief Union variant associated with tag #TYPE_DATETIMEARRAY */
    astarte_int64_array_t datetime_array;
} astarte_type_param_t;

/**
 * @brief Value type
 *
 * @details This struct is a tagged enum and represents every possible type that can be sent to
 * astarte. This struct should be initialized only by using one of the defined
 * `astarte_value_from_*` methods.
 *
 * See the following initialization methods:
 *  - #astarte_value_from_integer
 *  - #astarte_value_from_longinteger
 *  - #astarte_value_from_double
 *  - #astarte_value_from_string
 *  - #astarte_value_from_binaryblob
 *  - #astarte_value_from_boolean
 *  - #astarte_value_from_datetime
 *  - #astarte_value_from_integer_array
 *  - #astarte_value_from_longinteger_array
 *  - #astarte_value_from_double_array
 *  - #astarte_value_from_string_array
 *  - #astarte_value_from_binaryblob_array
 *  - #astarte_value_from_boolean_array
 *  - #astarte_value_from_datetime_array
 */
typedef struct
{
    /** @brief Value of the tagged enum */
    astarte_type_param_t value;
    /** @brief Tag of the tagged enum */
    astarte_type_t tag;
} astarte_value_t;

/**
 * @brief Serializes the passed #astarte_value_t to the passed #bson_serializer_handle_t
 *
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] key BSON key name, which is a C string.
 * @param[in] value the #astarte_value_t to serialize to the bson.
 * @return ASTARTE_OK if successful, otherwise an error code.
 */
astarte_err_t astarte_value_serialize(
    bson_serializer_handle_t bson, char *key, astarte_value_t value);

/**
 * @brief Initialize an astarte value from the passed integer.
 *
 * @param[in] integer value that will be wrapped in the astarte value.
 * @return The astarte value that wraps an integer with type tag TYPE_INTEGER.
 */
astarte_value_t astarte_value_from_integer(int32_t integer);
/**
 * @brief Initialize an astarte value from the passed longinteger.
 *
 * @param[in] longinteger value that will be wrapped in the astarte value.
 * @return The astarte value that wraps a longinteger with type tag TYPE_LONGINTEGER.
 */
astarte_value_t astarte_value_from_longinteger(int64_t longinteger);
/**
 * @brief Initialize an astarte value from the passed double.
 *
 * @param[in] dbl value that will be wrapped in the astarte value.
 * @return The astarte value that wraps a double with type tag TYPE_DOUBLE.
 */
astarte_value_t astarte_value_from_double(double dbl);
/**
 * @brief Initialize an astarte value from the passed string.
 *
 * @param[in] string value that will be wrapped in the astarte value.
 * @return The astarte value that wraps a string with type tag TYPE_STRING.
 */
astarte_value_t astarte_value_from_string(const char *string);
/**
 * @brief Initialize an astarte value from the passed binaryblob.
 *
 * @param[in] binaryblob value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps a binaryblob with type tag TYPE_BINARYBLOB.
 */
astarte_value_t astarte_value_from_binaryblob(void *binaryblob, size_t len);
/**
 * @brief Initialize an astarte value from the passed boolean.
 *
 * @param[in] boolean value that will be wrapped in the astarte value.
 * @return The astarte value that wraps a boolean with type tag TYPE_BOOLEAN.
 */
astarte_value_t astarte_value_from_boolean(bool boolean);
/**
 * @brief Initialize an astarte value from the passed datetime.
 *
 * @param[in] datetime value that will be wrapped in the astarte value.
 * @return The astarte value that wraps a datetime with type tag TYPE_DATETIME.
 */
astarte_value_t astarte_value_from_datetime(int64_t datetime);

/**
 * @brief Initialize an astarte value from the passed integer_array.
 *
 * @param[in] integer_array value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps an integer_array with type tag TYPE_INTEGERARRAY.
 */
astarte_value_t astarte_value_from_integer_array(int32_t *integer_array, size_t len);
/**
 * @brief Initialize an astarte value from the passed longinteger_array.
 *
 * @param[in] longinteger_array value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps a longinteger_array with type tag TYPE_LONGINTEGERARRAY.
 */
astarte_value_t astarte_value_from_longinteger_array(int64_t *longinteger_array, size_t len);
/**
 * @brief Initialize an astarte value from the passed double_array.
 *
 * @param[in] double_array value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps a double_array with type tag TYPE_DOUBLEARRAY.
 */
astarte_value_t astarte_value_from_double_array(double *double_array, size_t len);
/**
 * @brief Initialize an astarte value from the passed string_array.
 *
 * @param[in] string_array value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps a string_array with type tag TYPE_STRINGARRAY.
 */
astarte_value_t astarte_value_from_string_array(const char *const *string_array, size_t len);
/**
 * @brief Initialize an astarte value from the passed string_array.
 *
 * @param[in] buf Stores a list of buffers, the i-th buffer is of size size[i].
 * @param[in] sizes Sizes of each of the buffer stored in buf.
 * @param[in] count Number of elements in both buf and sizes.
 * @return The astarte value that wraps a binary_array with type tag TYPE_BINARYBLOBARRAY.
 */
astarte_value_t astarte_value_from_binaryblob_array(
    const void *const *buf, const int *sizes, size_t count);
/**
 * @brief Initialize an astarte value from the passed boolean_array.
 *
 * @param[in] boolean_array value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps a boolean_array with type tag TYPE_BOOLEANARRAY.
 */
astarte_value_t astarte_value_from_boolean_array(bool *boolean_array, size_t len);
/**
 * @brief Initialize an astarte value from the passed datetime_array.
 *
 * @param[in] datetime_array value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps a datetime_array with type tag TYPE_DATETIMEARRAY.
 */
astarte_value_t astarte_value_from_datetime_array(int64_t *datetime_array, size_t len);

/**
 * @}
 */

#endif /* ASTARTE_DEVICE_SDK_TYPE_H */
