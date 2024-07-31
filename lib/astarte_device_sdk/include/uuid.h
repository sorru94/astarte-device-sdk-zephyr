/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UUID_H
#define UUID_H

/**
 * @file uuid.h
 * @brief Utility functions for the generation and parsing of Universal Unique Identifier.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"

/** @brief Number of bytes in the binary representation of a UUID. */
#define ASTARTE_UUID_SIZE 16

/** @brief Length of the UUID canonical string representation. */
#define ASTARTE_UUID_STR_LEN 36

/** @brief Length of the UUID base64 string representation. */
#define ASTARTE_UUID_BASE64_LEN 24

/** @brief Length of the UUID base64 URL and filename safe string representation. */
#define ASTARTE_UUID_BASE64URL_LEN 22

/** @brief Binary representation of a UUID. */
typedef uint8_t astarte_uuid_t[ASTARTE_UUID_SIZE];

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generate a UUIDv4.
 *
 * @details This function computes a random UUID using hardware RNG.
 *
 * @param[out] out The UUID where the result will be written.
 */
void astarte_uuid_generate_v4(astarte_uuid_t out);

/**
 * @brief Generate a UUIDv5.
 *
 * @details This function computes a deterministic UUID starting from a namespace UUID and binary
 * data.
 *
 * @param[in] namespace The string representation of an UUID to be used as namespace.
 * @param[in] data A pointer to the data that will be hashed to produce the UUID.
 * @param[in] data_size The size of the data buffer.
 * @param[out] out The UUID where the result will be written.
 * @return ASTARTE_RESULT_OK when successful, ASTARTE_RESULT_INTERNAL_ERROR otherwise.
 */
astarte_result_t astarte_uuid_generate_v5(
    const astarte_uuid_t namespace, const void *data, size_t data_size, astarte_uuid_t out);

/**
 * @brief Generate a UUIDv5 and return base 64 (RFC 4648 sec. 5) URL and filename safe string
 * representation.
 *
 * @details This function has the same result as calling #astarte_uuid_generate_v5 and
 * #astarte_uuid_to_base64url one after the other.
 *
 * @param[in] namespace The string representation of an UUID to be used as namespace.
 * @param[in] data A pointer to the data that will be hashed to produce the UUID.
 * @param[in] data_size The size of the data buffer.
 * @param[out] out A pointer to a previously allocated buffer where the result will be written.
 * @param[in] out_size The size of the out buffer. Should be at least 23 bytes.
 * @return ASTARTE_RESULT_OK when successful, ASTARTE_RESULT_INTERNAL_ERROR otherwise.
 */
astarte_result_t astarte_uuid_generate_v5_to_base64url(
    const astarte_uuid_t namespace, const void *data, size_t data_size, char *out, size_t out_size);

/**
 * @brief Parse a UUID from its canonical (RFC4122) string representation.
 *
 * @param input A pointer to the string to be parsed.
 * @param out The UUID where the result will be written.
 * @return ASTARTE_RESULT_INTERNAL_ERROR upon failure, ASTARTE_RESULT_OK otherwise.
 */
astarte_result_t astarte_uuid_from_string(const char *input, astarte_uuid_t out);

/**
 * @brief Convert a UUID to its canonical (RFC4122) string representation.
 *
 * @param[in] uuid The UUID to convert to string.
 * @param[out] out A pointer to a previously allocated buffer where the result will be written.
 * @param[in] out_size The size of the out buffer. Should be at least 37 bytes.
 * @return ASTARTE_RESULT_OK on success, an error code otherwise.
 */
astarte_result_t astarte_uuid_to_string(const astarte_uuid_t uuid, char *out, size_t out_size);

/**
 * @brief Convert a UUID to its base 64 (RFC 3548, RFC 4648) string representation.
 *
 * @param[in] uuid The UUID to convert to string.
 * @param[out] out A pointer to a previously allocated buffer where the result will be written.
 * @param[in] out_size The size of the out buffer. Should be at least 25 bytes.
 * @return ASTARTE_RESULT_OK on success, an error code otherwise.
 */
astarte_result_t astarte_uuid_to_base64(const astarte_uuid_t uuid, char *out, size_t out_size);

/**
 * @brief Convert a UUID to its base 64 (RFC 4648 sec. 5) URL and filename safe string
 * representation.
 *
 * @param[in] uuid The UUID to convert to string.
 * @param[out] out A pointer to a previously allocated buffer where the result will be written.
 * @param[in] out_size The size of the out buffer. Should be at least 23 bytes.
 * @return ASTARTE_RESULT_OK on success, an error code otherwise.
 */
astarte_result_t astarte_uuid_to_base64url(const astarte_uuid_t uuid, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif // UUID_H
