/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_DEVICE_SDK_UUID_H
#define ASTARTE_DEVICE_SDK_UUID_H

#include "astarte_device_sdk/error.h"

#include <stddef.h>
#include <stdint.h>

#define UUID_LEN 16

typedef uint8_t uuid_t[UUID_LEN];

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief generate a UUIDv5.
 *
 * @details This function computes a deterministic UUID starting from a namespace UUID and binary
 * data.
 * @param namespace The string representation of an UUID to be used as namespace.
 * @param data A pointer to the data that will be hashed to produce the UUID.
 * @param data_size The size of the data buffer.
 * @param out The UUID where the result will be written.
 * @return ASTARTE_OK when successful, ASTARTE_ERR otherwise.
 */
astarte_err_t uuid_generate_v5(
    const uuid_t namespace, const void *data, size_t data_size, uuid_t out);

/**
 * @brief convert a UUID to its string representation.
 *
 * @details Convert a UUID to its canonical (RFC4122) string representation.
 * @param uuid The UUID.
 * @param out  A pointer to a previously allocated buffer where the result will be written.
 * @param out_size The size of the out buffer. Should be at least 37 bytes.
 * @return ASTARTE_ERR upon failure, ASTARTE_OK otherwise.
 */
astarte_err_t uuid_to_string(const uuid_t uuid, char *out, size_t out_size);

/**
 * @brief parse a UUID from its string representation.
 *
 * @details parse a UUID from its canonical (RFC4122) string representation.
 * @param input A pointer to the string to be parsed.
 * @param out The UUID where the result will be written.
 * @return 0 if the parsing was succesfull, -1 otherwise.
 */
astarte_err_t uuid_from_string(const char *input, uuid_t out);

/**
 * @brief generate a UUIDv4.
 *
 * @details This function computes a random UUID using hardware RNG.
 * @param out The UUID where the result will be written.
 */
void uuid_generate_v4(uuid_t out);

#ifdef __cplusplus
}
#endif

#endif // ASTARTE_DEVICE_SDK_UUID_H
