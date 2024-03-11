/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_VALUE_H
#define ASTARTE_DEVICE_SDK_VALUE_H

/**
 * @file value.h
 * @brief Definitions for Astarte values
 */

/**
 * @defgroup values Values
 * @ingroup astarte_device_sdk
 * @{
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/mapping.h"
#include "astarte_device_sdk/result.h"
#include "astarte_device_sdk/util.h"

/** @brief Container for a binary blob type */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_value_binaryblob_t, void);
/** @brief Container for a integer array type */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_value_integerarray_t, int32_t);
/** @brief Container for a long integer array type */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_value_longintegerarray_t, int64_t);
/** @brief Container for a double array type */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_value_doublearray_t, double);
/** @brief Container for a string array type */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_value_stringarray_t, const char *const);
/** @brief Container for a bool array type */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_value_booleanarray_t, bool);

// TODO use the ASTARTE_UTIL_DEFINE_ARRAY macro to also generate astarte_value_binaryblobarray_t
// and refactor astarte_bson_serializer_append_binary_array to accept the array as input
/** @brief Container for a binary blob array type */
typedef struct
{
    /** @brief Array of binary blobs */
    const void *const *blobs;
    /** @brief Array of sizes of each binary blob */
    const size_t *sizes;
    /** @brief Number of elements in both the array of binary blobs and array of sizes */
    size_t count;
} astarte_value_binaryblobarray_t;

/** @brief Union grouping all the containers for Astarte types */
typedef union
{
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_INTEGER */
    int32_t integer;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_LONGINTEGER */
    int64_t longinteger;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_DOUBLE */
    double dbl;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_STRING */
    const char *string;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_BINARYBLOB */
    astarte_value_binaryblob_t binaryblob;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_BOOLEAN */
    bool boolean;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_DATETIME */
    int64_t datetime;

    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_INTEGERARRAY */
    astarte_value_integerarray_t integer_array;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY */
    astarte_value_longintegerarray_t longinteger_array;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_DOUBLEARRAY */
    astarte_value_doublearray_t double_array;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_STRINGARRAY */
    astarte_value_stringarray_t string_array;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY */
    astarte_value_binaryblobarray_t binaryblob_array;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_BOOLEANARRAY */
    astarte_value_booleanarray_t boolean_array;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_DATETIMEARRAY */
    astarte_value_longintegerarray_t datetime_array;
} astarte_value_param_t;

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
    astarte_value_param_t value;
    /** @brief Tag of the tagged enum */
    astarte_mapping_type_t tag;
} astarte_value_t;

/**
 * @brief Generic key-value pair using a string as key and an astarte_value_t as value.
 *
 * @details This struct is intended to be used when streaming aggregates.
 */
typedef struct
{
    /** @brief Endpoint for the pair */
    const char *endpoint;
    /** @brief Value for the pair */
    astarte_value_t value;
} astarte_value_pair_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize an astarte value from the passed integer.
 *
 * @param[in] integer value that will be wrapped in the astarte value.
 * @return The astarte value that wraps an integer with type tag ASTARTE_MAPPING_TYPE_INTEGER.
 */
astarte_value_t astarte_value_from_integer(int32_t integer);
/**
 * @brief Initialize an astarte value from the passed longinteger.
 *
 * @param[in] longinteger value that will be wrapped in the astarte value.
 * @return The astarte value that wraps a longinteger with type tag
 * ASTARTE_MAPPING_TYPE_LONGINTEGER.
 */
astarte_value_t astarte_value_from_longinteger(int64_t longinteger);
/**
 * @brief Initialize an astarte value from the passed double.
 *
 * @param[in] dbl value that will be wrapped in the astarte value.
 * @return The astarte value that wraps a double with type tag ASTARTE_MAPPING_TYPE_DOUBLE.
 */
astarte_value_t astarte_value_from_double(double dbl);
/**
 * @brief Initialize an astarte value from the passed string.
 *
 * @param[in] string value that will be wrapped in the astarte value.
 * @return The astarte value that wraps a string with type tag ASTARTE_MAPPING_TYPE_STRING.
 */
astarte_value_t astarte_value_from_string(const char *string);
/**
 * @brief Initialize an astarte value from the passed binaryblob.
 *
 * @param[in] binaryblob value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps a binaryblob with type tag ASTARTE_MAPPING_TYPE_BINARYBLOB.
 */
astarte_value_t astarte_value_from_binaryblob(void *binaryblob, size_t len);
/**
 * @brief Initialize an astarte value from the passed boolean.
 *
 * @param[in] boolean value that will be wrapped in the astarte value.
 * @return The astarte value that wraps a boolean with type tag ASTARTE_MAPPING_TYPE_BOOLEAN.
 */
astarte_value_t astarte_value_from_boolean(bool boolean);
/**
 * @brief Initialize an astarte value from the passed datetime.
 *
 * @param[in] datetime value that will be wrapped in the astarte value.
 * @return The astarte value that wraps a datetime with type tag ASTARTE_MAPPING_TYPE_DATETIME.
 */
astarte_value_t astarte_value_from_datetime(int64_t datetime);

/**
 * @brief Initialize an astarte value from the passed integer_array.
 *
 * @param[in] integer_array value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps an integer_array with type tag
 * ASTARTE_MAPPING_TYPE_INTEGERARRAY.
 */
astarte_value_t astarte_value_from_integer_array(int32_t *integer_array, size_t len);
/**
 * @brief Initialize an astarte value from the passed longinteger_array.
 *
 * @param[in] longinteger_array value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps a longinteger_array with type tag
 * ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY.
 */
astarte_value_t astarte_value_from_longinteger_array(int64_t *longinteger_array, size_t len);
/**
 * @brief Initialize an astarte value from the passed double_array.
 *
 * @param[in] double_array value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps a double_array with type tag
 * ASTARTE_MAPPING_TYPE_DOUBLEARRAY.
 */
astarte_value_t astarte_value_from_double_array(double *double_array, size_t len);
/**
 * @brief Initialize an astarte value from the passed string_array.
 *
 * @param[in] string_array value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps a string_array with type tag
 * ASTARTE_MAPPING_TYPE_STRINGARRAY.
 */
astarte_value_t astarte_value_from_string_array(const char *const *string_array, size_t len);
/**
 * @brief Initialize an astarte value from the passed string_array.
 *
 * @param[in] buf Stores a list of buffers, the i-th buffer is of size size[i].
 * @param[in] sizes Sizes of each of the buffer stored in buf.
 * @param[in] count Number of elements in both buf and sizes.
 * @return The astarte value that wraps a binary_array with type tag
 * ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY.
 */
astarte_value_t astarte_value_from_binaryblob_array(
    const void *const *buf, const size_t *sizes, size_t count);
/**
 * @brief Initialize an astarte value from the passed boolean_array.
 *
 * @param[in] boolean_array value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps a boolean_array with type tag
 * ASTARTE_MAPPING_TYPE_BOOLEANARRAY.
 */
astarte_value_t astarte_value_from_boolean_array(bool *boolean_array, size_t len);
/**
 * @brief Initialize an astarte value from the passed datetime_array.
 *
 * @param[in] datetime_array value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps a datetime_array with type tag
 * ASTARTE_MAPPING_TYPE_DATETIMEARRAY.
 */
astarte_value_t astarte_value_from_datetime_array(int64_t *datetime_array, size_t len);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ASTARTE_DEVICE_SDK_VALUE_H */
