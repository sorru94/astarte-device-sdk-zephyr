/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DEVICE_SESSION_MANAGER_H
#define DEVICE_SESSION_MANAGER_H

/**
 * @file device/session_manager.h
 * @brief Device session management header.
 */

#include "device/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Polls the internal device instance for updates.
 *
 * @param[in] device Handle to the device instance.
 * @return ASTARTE_RESULT_OK if successful, otherwise an error code.
 */
astarte_result_t astarte_device_internal_poll(astarte_device_handle_t device);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_SESSION_MANAGER_H
