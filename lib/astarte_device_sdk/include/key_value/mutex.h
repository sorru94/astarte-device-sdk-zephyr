/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KEY_VALUE_MUTEX_H
#define KEY_VALUE_MUTEX_H

/**
 * @file key_value/mutex.h
 * @brief Wrapper for the mutex used in the key value module.
 */

#include "astarte_device_sdk/astarte.h"
#include "astarte_device_sdk/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Lock the key-value storage mutex.
 *
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_key_value_mutex_lock(void);

/** @brief Unlock the key-value storage mutex. */
void astarte_key_value_mutex_unlock(void);

#ifdef __cplusplus
}
#endif

#endif // KEY_VALUE_MUTEX_H
