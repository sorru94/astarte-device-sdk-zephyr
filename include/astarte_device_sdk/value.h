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

/** @private Astarte binary blob type. Internal use, do not inspect its content directly. */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_value_binaryblob_t, void);

/** @private Astarte binary blob array type. Internal use, do not inspect its content directly. */
typedef struct
{
    /** @brief Array of binary blobs */
    const void **blobs;
    /** @brief Array of sizes of each binary blob */
    size_t *sizes;
    /** @brief Number of elements in both the array of binary blobs and array of sizes */
    size_t count;
} astarte_value_binaryblobarray_t;

/** @private Astarte bool array type. Internal use, do not inspect its content directly. */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_value_booleanarray_t, bool);
/** @private Astarte double array type. Internal use, do not inspect its content directly. */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_value_doublearray_t, double);
/** @private Astarte integer array type. Internal use, do not inspect its content directly. */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_value_integerarray_t, int32_t);
/** @private Astarte long integer array type. Internal use, do not inspect its content directly. */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_value_longintegerarray_t, int64_t);
/** @private Astarte string array type. Internal use, do not inspect its content directly. */
ASTARTE_UTIL_DEFINE_ARRAY(astarte_value_stringarray_t, const char *);

/** @private Union grouping all the individual Astarte types.
 * Internal use, do not inspect its content directly. */
typedef union
{
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_BINARYBLOB */
    astarte_value_binaryblob_t binaryblob;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_BOOLEAN */
    bool boolean;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_DATETIME */
    int64_t datetime;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_DOUBLE */
    double dbl;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_INTEGER */
    int32_t integer;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_LONGINTEGER */
    int64_t longinteger;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_STRING */
    const char *string;

    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY */
    astarte_value_binaryblobarray_t binaryblob_array;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_BOOLEANARRAY */
    astarte_value_booleanarray_t boolean_array;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_DATETIMEARRAY */
    astarte_value_longintegerarray_t datetime_array;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_DOUBLEARRAY */
    astarte_value_doublearray_t double_array;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_INTEGERARRAY */
    astarte_value_integerarray_t integer_array;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY */
    astarte_value_longintegerarray_t longinteger_array;
    /** @brief Union variant associated with tag #ASTARTE_MAPPING_TYPE_STRINGARRAY */
    astarte_value_stringarray_t string_array;
} astarte_value_param_t;

/**
 * @brief Value type
 *
 * @warning Do not inspect its content directly. Use the provided functions to modify it.
 * @details This struct is a tagged enum and represents every possible type that can be sent to
 * astarte. This struct should be initialized only by using one of the defined
 * `astarte_value_from_*` methods.
 *
 * See the following initialization methods:
 *  - #astarte_value_from_binaryblob
 *  - #astarte_value_from_boolean
 *  - #astarte_value_from_double
 *  - #astarte_value_from_datetime
 *  - #astarte_value_from_integer
 *  - #astarte_value_from_longinteger
 *  - #astarte_value_from_string
 *  - #astarte_value_from_binaryblob_array
 *  - #astarte_value_from_boolean_array
 *  - #astarte_value_from_datetime_array
 *  - #astarte_value_from_double_array
 *  - #astarte_value_from_integer_array
 *  - #astarte_value_from_longinteger_array
 *  - #astarte_value_from_string_array
 *
 * See the following getter methods:
 *  - #astarte_value_to_binaryblob
 *  - #astarte_value_to_boolean
 *  - #astarte_value_to_double
 *  - #astarte_value_to_datetime
 *  - #astarte_value_to_integer
 *  - #astarte_value_to_longinteger
 *  - #astarte_value_to_string
 *  - #astarte_value_to_binaryblob_array
 *  - #astarte_value_to_boolean_array
 *  - #astarte_value_to_datetime_array
 *  - #astarte_value_to_double_array
 *  - #astarte_value_to_integer_array
 *  - #astarte_value_to_longinteger_array
 *  - #astarte_value_to_string_array
 */
typedef struct
{
    /** @brief Data portion of the tagged enum */
    astarte_value_param_t data;
    /** @brief Tag of the tagged enum */
    astarte_mapping_type_t tag;
} astarte_value_t;

/**
 * @brief Key-value pair, used to store an endpoint and Astarte value.
 *
 * @warning Do not inspect its content directly. Use the provided functions to modify it.
 * @details This struct is intended to be used when streaming aggregates.
 *
 * See the following initialization/getter methods:
 *  - #astarte_value_pair_new
 *  - #astarte_value_pair_to_endpoint_and_value
 */
typedef struct
{
    /** @brief Endpoint for the pair */
    const char *endpoint;
    /** @brief Value for the pair */
    astarte_value_t value;
} astarte_value_pair_t;

/**
 * @brief Array of Astarte value pairs.
 *
 * @warning Do not inspect its content directly. Use the provided functions to modify it.
 * @details This struct is intended to be used when streaming aggregates.
 *
 * See the following initialization/getter methods:
 *  - #astarte_value_pair_array_new
 *  - #astarte_value_pair_array_to_value_pairs
 */
typedef struct
{
    /** @brief Array of Astarte value pairs. */
    astarte_value_pair_t *buf;
    /** @brief Size of the array of Astarte value pairs. */
    size_t len;
} astarte_value_pair_array_t;

#ifdef __cplusplus
extern "C" {
#endif

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
 * @brief Initialize an astarte value from the passed double.
 *
 * @param[in] dbl value that will be wrapped in the astarte value.
 * @return The astarte value that wraps a double with type tag ASTARTE_MAPPING_TYPE_DOUBLE.
 */
astarte_value_t astarte_value_from_double(double dbl);
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
 * @brief Initialize an astarte value from the passed string.
 *
 * @param[in] string value that will be wrapped in the astarte value.
 * @return The astarte value that wraps a string with type tag ASTARTE_MAPPING_TYPE_STRING.
 */
astarte_value_t astarte_value_from_string(const char *string);

/**
 * @brief Initialize an astarte value from the passed string_array.
 *
 * @param[in] blobs Stores a list of buffers, the i-th buffer is of size sizes[i].
 * @param[in] sizes Sizes of each of the buffer stored in buf.
 * @param[in] count Number of elements in both buf and sizes.
 * @return The astarte value that wraps a binary_array with type tag
 * ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY.
 */
astarte_value_t astarte_value_from_binaryblob_array(
    const void **blobs, size_t *sizes, size_t count);
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
 * @brief Initialize an astarte value from the passed string_array.
 *
 * @param[in] string_array value that will be wrapped in the astarte value.
 * @param[in] len The length of the passed array.
 * @return The astarte value that wraps a string_array with type tag
 * ASTARTE_MAPPING_TYPE_STRINGARRAY.
 */
astarte_value_t astarte_value_from_string_array(const char **string_array, size_t len);

/**
 * @brief Get the type of an Astarte value.
 *
 * @param[in] value Astarte value for which to check the type.
 * @return The astarte value type.
 */
astarte_mapping_type_t astarte_value_get_type(astarte_value_t value);

/**
 * @brief Convert an Astarte value (of the binary blob type) to an array of binary blobs.
 *
 * @note This function will not perform allocation and will return references the buffer contained
 * in the value parameter.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] binaryblob Array extracted.
 * @param[out] len Size of extracted array.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_binaryblob(astarte_value_t value, void **binaryblob, size_t *len);
/**
 * @brief Convert an Astarte value (of the boolean type) to a bool.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] boolean Extracted value.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_boolean(astarte_value_t value, bool *boolean);
/**
 * @brief Convert an Astarte value (of the datetime type) to an int64_t.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] datetime Extracted value.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_datetime(astarte_value_t value, int64_t *datetime);
/**
 * @brief Convert an Astarte value (of double type) to a double.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] dbl Double where to store the extracted value.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_double(astarte_value_t value, double *dbl);
/**
 * @brief Convert an Astarte value (of the integer type) to an int32_t.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] integer Extracted value.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_integer(astarte_value_t value, int32_t *integer);
/**
 * @brief Convert an Astarte value (of the long integer type) to an int64_t.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] longinteger Extracted value.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_longinteger(astarte_value_t value, int64_t *longinteger);
/**
 * @brief Convert an Astarte value (of the string type) to a const char *.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] string Extracted value.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_string(astarte_value_t value, const char **string);

/**
 * @brief Convert an Astarte value (of binary blob array type) to an array of binary blobs arrays.
 *
 * @note This function will not perform allocation and will return references the buffer contained
 * in the value parameter.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] blobs Array of binary blob arrays.
 * @param[out] sizes Array of sizes for the binaryblob arrays.
 * @param[out] count Number of elements for the buf and sizes arrays.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_binaryblob_array(
    astarte_value_t value, const void ***blobs, size_t **sizes, size_t *count);
/**
 * @brief Convert an Astarte value (of booleanarray type) to a bool array.
 *
 * @note This function will not perform allocation and will return references the buffer contained
 * in the value parameter.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] boolean_array Array extracted.
 * @param[out] len Size of extracted array.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_boolean_array(
    astarte_value_t value, bool **boolean_array, size_t *len);
/**
 * @brief Convert an Astarte value (of datetimearray type) to an int64_t array.
 *
 * @note This function will not perform allocation and will return references the buffer contained
 * in the value parameter.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] datetime_array Array extracted.
 * @param[out] len Size of extracted array.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_datetime_array(
    astarte_value_t value, int64_t **datetime_array, size_t *len);
/**
 * @brief Convert an Astarte value (of doublearray type) to an double array.
 *
 * @note This function will not perform allocation and will return references the buffer contained
 * in the value parameter.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] double_array Array extracted.
 * @param[out] len Size of extracted array.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_double_array(
    astarte_value_t value, double **double_array, size_t *len);
/**
 * @brief Convert an Astarte value (of integerarray type) to an int32_t array.
 *
 * @note This function will not perform allocation and will return references the buffer contained
 * in the value parameter.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] integer_array Array extracted.
 * @param[out] len Size of extracted array.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_integer_array(
    astarte_value_t value, int32_t **integer_array, size_t *len);
/**
 * @brief Convert an Astarte value (of longintegerarray type) to an int64_t array.
 *
 * @note This function will not perform allocation and will return references the buffer contained
 * in the value parameter.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] longinteger_array Array extracted.
 * @param[out] len Size of extracted array.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_longinteger_array(
    astarte_value_t value, int64_t **longinteger_array, size_t *len);
/**
 * @brief Convert an Astarte value (of string type) to an const char* array.
 *
 * @note This function will not perform allocation and will return references the buffer contained
 * in the value parameter.
 *
 * @param[in] value Astarte value to use for the extraction.
 * @param[out] string_array Array extracted.
 * @param[out] len Size of extracted array.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_to_string_array(
    astarte_value_t value, const char ***string_array, size_t *len);

/**
 * @brief Initialize an Astarte value pair.
 *
 * @param[in] endpoint Endpoint to store in the pair.
 * @param[in] value Value to store in the pair.
 * @return The Astarte value pair created from the inputs.
 */
astarte_value_pair_t astarte_value_pair_new(const char *endpoint, astarte_value_t value);

/**
 * @brief Initialize an Astarte value pair array.
 *
 * @param[in] value_pairs Array of value pairs.
 * @param[in] len Size of the array of value pairs.
 * @return The Astarte value pair array created from the inputs.
 */
astarte_value_pair_array_t astarte_value_pair_array_new(
    astarte_value_pair_t *value_pairs, size_t len);

/**
 * @brief Convert an Astarte value pair to an Astarte value and endpoint.
 *
 * @param[in] value_pair Input Astarte value pair.
 * @param[out] endpoint Endpoint extracted from the pair.
 * @param[out] value Value extracted from the pair.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_pair_to_endpoint_and_value(
    astarte_value_pair_t value_pair, const char **endpoint, astarte_value_t *value);

/**
 * @brief Convert an Astarte value pair array to an array of Astarte value pairs.
 *
 * @param[in] value_pair_array Input Astarte value pair array.
 * @param[out] value_pairs Extracted array of value pairs.
 * @param[out] len Size of the extracted array.
 * @return ASTARTE_RESULT_OK if the conversion was successful, an error otherwise.
 */
astarte_result_t astarte_value_pair_array_to_value_pairs(
    astarte_value_pair_array_t value_pair_array, astarte_value_pair_t **value_pairs, size_t *len);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ASTARTE_DEVICE_SDK_VALUE_H */
