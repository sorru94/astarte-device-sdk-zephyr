/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "key_value/entry_hash.h"

#include <stdint.h>
#include <string.h>

#include <zephyr/sys/crc.h>

#include "key_value/entry.h"
#include "key_value/entry_list.h"

/************************************************
 *         Global functions definitions         *
 ***********************************************/

uint16_t astarte_key_value_entry_hash_generate(const char *namespace, const char *key)
{
    uint16_t crc = UINT16_MAX; // Zephyr's crc16_ccitt seed
    crc = crc16_ccitt(crc, (const uint8_t *) namespace, strlen(namespace));
    crc = crc16_ccitt(crc, (const uint8_t *) key, strlen(key));

    // Fallbacks to avoid invalid Zephyr NVS IDs or our reserved locations
    if (crc > ASTARTE_KEY_VALUE_ENTRY_MAX_USABLE_ID) {
        crc = 0;
    }
    return crc;
}
