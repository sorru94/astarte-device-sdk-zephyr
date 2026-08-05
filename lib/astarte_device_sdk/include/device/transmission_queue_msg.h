/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_TRANSMISSION_QUEUE_MSG_H
#define DEVICE_TRANSMISSION_QUEUE_MSG_H

/**
 * @file device/transmission_queue_msg.h
 * @brief Device transmission queue volatile message management and utilities.
 */

#include "device/transmission_queue.h"

#include <stdint.h>

/**
 * @brief Initialize a volatile message from a generic queue message.
 *
 * @param[out] volatile_msg Pointer to the volatile message structure to initialize.
 * @param[in] queue_msg Pointer to the generic queue message containing the source data.
 * @param[in] timestamp The timestamp (in milliseconds) to assign to the volatile message.
 * @param[in] sequence_number The ordering sequence number to assign to the volatile message.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_transmission_queue_volatile_msg_init(
    struct astarte_device_transmission_queue_volatile_msg *volatile_msg,
    const struct astarte_device_transmission_queue_msg *queue_msg, uint64_t timestamp,
    uint64_t sequence_number);

/**
 * @brief Deep copy a volatile message into a generic queue message.
 *
 * @param[out] queue_msg Pointer to the generic queue message structure to populate.
 * @param[in] volatile_msg Pointer to the volatile message containing the source data.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_transmission_queue_msg_from_volatile_msg_deep_cpy(
    struct astarte_device_transmission_queue_msg *queue_msg,
    const struct astarte_device_transmission_queue_volatile_msg *volatile_msg);

/**
 * @brief Clean up memory allocated inside a volatile message.
 *
 * @param[in,out] msg Pointer to the volatile message structure to clean up.
 */
void astarte_transmission_queue_volatile_msg_cleanup(
    struct astarte_device_transmission_queue_volatile_msg *msg);

#endif // DEVICE_TRANSMISSION_QUEUE_MSG_H
