/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "key_value/mutex.h"

#include <zephyr/sys/mutex.h>

#include "log.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_key_value, CONFIG_ASTARTE_DEVICE_SDK_KEY_VALUE_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

// This mutex will be shared by all instances of this driver.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static SYS_MUTEX_DEFINE(astarte_key_value_mutex);
#define MUTEX_LOCK_TIMEOUT_MS 5000

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_key_value_mutex_lock(void)
{
    int mutex_rc = sys_mutex_lock(&astarte_key_value_mutex, K_MSEC(MUTEX_LOCK_TIMEOUT_MS));
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex lock failed with %d", mutex_rc);
    return (mutex_rc != 0) ? ASTARTE_RESULT_MUTEX_LOCK_ERROR : ASTARTE_RESULT_OK;
}

void astarte_key_value_mutex_unlock(void)
{
    int mutex_rc = sys_mutex_unlock(&astarte_key_value_mutex);
    ASTARTE_LOG_COND_ERR(mutex_rc != 0, "System mutex unlock failed with %d", mutex_rc);
}
