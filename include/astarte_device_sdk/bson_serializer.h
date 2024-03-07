/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_BSON_SERIALIZER_H
#define ASTARTE_DEVICE_SDK_BSON_SERIALIZER_H

/**
 * @file bson_serializer.h
 * @brief Astarte BSON serializer functions.
 *
 * @ingroup utils
 * @{
 */

#include "astarte_device_sdk/result.h"

/**
 * @brief Typedefined serializer handle.
 */
typedef struct astarte_bson_serializer *astarte_bson_serializer_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief create a new instance of the BSON serializer.
 *
 * @details This function has to be called to initialize and allocate memory for an instance of the
 * BSON serializer.
 * @return The handle to the initialized BSON serializer instance.
 */
astarte_bson_serializer_handle_t astarte_bson_serializer_new(void);

/**
 * @brief destroy given BSON serializer instance.
 *
 * @details This function has to be called to destroy and free the memory used by a serializer
 * instance.
 * @param[in] bson a valid handle for the serializer instance that will be destroyed.
 */
void astarte_bson_serializer_destroy(astarte_bson_serializer_handle_t bson);

/**
 * @brief getter for the BSON serializer internal buffer.
 *
 * @details This function might be used to get internal buffer without any data copy. The returned
 * buffer will be invalid after serializer destruction.
 * @param[in] bson a valid handle for the serializer instance.
 * @param[out] size the size of the internal buffer. Optional, pass NULL if not used.
 * @return Reference to the internal buffer.
 */
const void *astarte_bson_serializer_get_document(astarte_bson_serializer_handle_t bson, int *size);

/**
 * @brief copy BSON serializer internal buffer to a different buffer.
 *
 * @details This function should be used to get a copy of the BSON document data. The document
 * should be terminated by calling the append end of document function before calling this function.
 * @param[in] bson a valid handle for the serializer instance.
 * @param[out] out_buf destination buffer, previously allocated.
 * @param[in] out_buf_size destination buffer size.
 * @param[out] out_doc_size BSON document size (that is <= out_buf_size).
 * Optional, pass NULL if not used.
 * @return ASTARTE_RESULT_OK on success, otherwise an error is returned.
 */
astarte_result_t astarte_bson_serializer_serialize_document(
    astarte_bson_serializer_handle_t bson, void *out_buf, int out_buf_size, int *out_doc_size);

/**
 * @brief return the document size
 *
 * @details This function returns BSON document size in bytes.
 * @param[in] bson a valid handle for the serializer instance.
 * @return Size in bytes for the serialized document.
 */
size_t astarte_bson_serializer_document_size(astarte_bson_serializer_handle_t bson);

/**
 * @brief append end of document marker.
 *
 * @details BSON document MUST be manually terminated with an end of document marker.
 * @param[in,out] bson a valid handle for the serializer instance.
 */
void astarte_bson_serializer_append_end_of_document(astarte_bson_serializer_handle_t bson);

/**
 * @brief append a double value
 *
 * @details This function appends a double value to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] value a double floating point value.
 */
void astarte_bson_serializer_append_double(
    astarte_bson_serializer_handle_t bson, const char *name, double value);

/**
 * @brief append an int32 value
 *
 * @details This function appends an int32 value to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] value a 32 bits signed integer value.
 */
void astarte_bson_serializer_append_int32(
    astarte_bson_serializer_handle_t bson, const char *name, int32_t value);

/**
 * @brief append an int64 value
 *
 * @details This function appends an int64 value to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] value a 64 bits signed integer value.
 */
void astarte_bson_serializer_append_int64(
    astarte_bson_serializer_handle_t bson, const char *name, int64_t value);

/**
 * @brief append a binary blob value
 *
 * @details This function appends a binary blob to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] value the buffer that holds the binary blob.
 * @param[in] size blob size in bytes.
 */
void astarte_bson_serializer_append_binary(
    astarte_bson_serializer_handle_t bson, const char *name, const void *value, size_t size);

/**
 * @brief append an UTF-8 string
 *
 * @details This function appends an UTF-8 string to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] string a 0 terminated UTF-8 string.
 */
void astarte_bson_serializer_append_string(
    astarte_bson_serializer_handle_t bson, const char *name, const char *string);

/**
 * @brief append a date time value
 *
 * @details This function appends a date time value to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] epoch_millis a 64 bits unsigned integer storing date time in milliseconds since epoch.
 */
void astarte_bson_serializer_append_datetime(
    astarte_bson_serializer_handle_t bson, const char *name, uint64_t epoch_millis);

/**
 * @brief append a boolean value
 *
 * @details This function appends a boolean value to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] value 0 as false value, not 0 as true value.
 */
void astarte_bson_serializer_append_boolean(
    astarte_bson_serializer_handle_t bson, const char *name, bool value);

/**
 * @brief append a sub-BSON document.
 *
 * @details This function appends a BSON subdocument to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] document a valid BSON document (that has been already terminated).
 */
void astarte_bson_serializer_append_document(
    astarte_bson_serializer_handle_t bson, const char *name, const void *document);

/**
 * @brief append a double array
 *
 * @details This function appends a double array to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] arr an array of doubles.
 * @param[in] count the number of items stored in double_array.
 * @return ASTARTE_RESULT_INTERNAL_ERROR upon serialization failure. ASTARTE_RESULT_OK otherwise.
 */
astarte_result_t astarte_bson_serializer_append_double_array(
    astarte_bson_serializer_handle_t bson, const char *name, const double *arr, int count);

/**
 * @brief append an int32 array
 *
 * @details This function appends an int32 array to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] arr an array of signed 32 bit integers.
 * @param[in] count the number of items stored in int32_array.
 * @return ASTARTE_RESULT_INTERNAL_ERROR upon serialization failure. ASTARTE_RESULT_OK otherwise.
 */
astarte_result_t astarte_bson_serializer_append_int32_array(
    astarte_bson_serializer_handle_t bson, const char *name, const int32_t *arr, int count);

/**
 * @brief append an int64 array
 *
 * @details This function appends an int64 array to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] arr an array of signed 64 bit integers.
 * @param[in] count the number of items stored in int64_array.
 * @return ASTARTE_RESULT_INTERNAL_ERROR upon serialization failure. ASTARTE_RESULT_OK otherwise.
 */
astarte_result_t astarte_bson_serializer_append_int64_array(
    astarte_bson_serializer_handle_t bson, const char *name, const int64_t *arr, int count);

/**
 * @brief append a string array
 *
 * @details This function appends a string array to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] arr an array of 0 terminated UTF-8 strings.
 * @param[in] count the number of items stored in string_array.
 * @return ASTARTE_RESULT_INTERNAL_ERROR upon serialization failure. ASTARTE_RESULT_OK otherwise.
 */
astarte_result_t astarte_bson_serializer_append_string_array(
    astarte_bson_serializer_handle_t bson, const char *name, const char *const *arr, int count);

/**
 * @brief append a binary blob array
 *
 * @details This function appends a binary blob array to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] arr an array of binary blobs (void *).
 * @param[in] sizes an array with the sizes of each binary in binary_array parameter.
 * @param[in] count the number of items stored in binary_array.
 * @return ASTARTE_RESULT_INTERNAL_ERROR upon serialization failure. ASTARTE_RESULT_OK otherwise.
 */
astarte_result_t astarte_bson_serializer_append_binary_array(astarte_bson_serializer_handle_t bson,
    const char *name, const void *const *arr, const int *sizes, int count);

/**
 * @brief append a date time array
 *
 * @details This function appends a date time array to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] arr an array of 64 bits unsigned integer storing date time in
 * milliseconds since epoch.
 * @param[in] count the number of items stored in epoch_millis_array.
 * @return ASTARTE_RESULT_INTERNAL_ERROR upon serialization failure. ASTARTE_RESULT_OK otherwise.
 */
astarte_result_t astarte_bson_serializer_append_datetime_array(
    astarte_bson_serializer_handle_t bson, const char *name, const int64_t *arr, int count);

/**
 * @brief append a boolean array
 *
 * @details This function appends a boolean array to the document.
 * @param[in,out] bson a valid handle for the serializer instance.
 * @param[in] name BSON element name, which is a C string.
 * @param[in] arr an array of stdbool booleans.
 * @param[in] count the number of items stored in boolean_array.
 * @return ASTARTE_RESULT_INTERNAL_ERROR upon serialization failure. ASTARTE_RESULT_OK otherwise.
 */
astarte_result_t astarte_bson_serializer_append_boolean_array(
    astarte_bson_serializer_handle_t bson, const char *name, const bool *arr, int count);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif // ASTARTE_DEVICE_SDK_BSON_SERIALIZER_H
