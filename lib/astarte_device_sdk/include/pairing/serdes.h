/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PAIRING_PRIVATE_H
#define PAIRING_PRIVATE_H

/**
 * @file pairing/serdes.h
 * @brief Serializing and deserializing utilities for the Astarte pairing API.
 */

#include "astarte_device_sdk/pairing.h"

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encode the payload for the device registration HTTP request.
 *
 * @param[in] device_id Device ID to be encoded in the payload.
 * @param[out] out_buff Buffer where to store the encoded payload.
 * @param[in] out_buff_size Size of the output buffer.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_pairing_serialize_register_device_payload(
    const char *device_id, char *out_buff, size_t out_buff_size);

/**
 * @brief Parse the response from the device registration HTTP request.
 *
 * @param[in] resp_buf Response to parse.
 * @param[out] out_cred_secr Returned credential secret.
 * @param[in] out_cred_secr_size Output credential secret buffer size
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_pairing_deserialize_register_device_response(
    char *resp_buf, char *out_cred_secr, size_t out_cred_secr_size);
/**
 * @brief Parse the response from the get broker url HTTP request.
 *
 * @param[in] resp_buf Response to parse.
 * @param[out] out_url Returned MQTT broker URL.
 * @param[in] out_url_size Output MQTT broker URL buffer size
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_pairing_deserialize_get_broker_url_response(
    char *resp_buf, char *out_url, size_t out_url_size);
/**
 * @brief Encode the payload for the get client certificate HTTP request.
 *
 * @param[in] csr Certificate signing request to be encoded in the payload.
 * @param[out] out_buff Buffer where to store the encoded payload.
 * @param[in] out_buff_size Size of the output buffer.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_pairing_serialize_get_client_certificate_payload(
    const char *csr, char *out_buff, size_t out_buff_size);
/**
 * @brief Parse the response from the get client certificate HTTP request.
 *
 * @param[in] resp_buf Response to parse.
 * @param[out] out_deb Returned deb certificate.
 * @param[in] out_deb_size Size of the output buffer.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_pairing_deserialize_get_client_certificate_response(
    char *resp_buf, char *out_deb, size_t out_deb_size);
/**
 * @brief Encode the payload for the verify client certificate HTTP request.
 *
 * @param[in] crt_pem Certificate to be encoded in the payload.
 * @param[out] out_buff Buffer where to store the encoded payload.
 * @param[in] out_buff_size Size of the output buffer.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_pairing_serialize_verify_client_certificate_payload(
    const char *crt_pem, char *out_buff, size_t out_buff_size);
/**
 * @brief Parse the response from the verify client certificate HTTP request.
 *
 * @param[in] resp_buf Response to parse.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_pairing_deserialize_verify_client_certificate_response(char *resp_buf);

#ifdef __cplusplus
}
#endif

#endif /* PAIRING_PRIVATE_H */
