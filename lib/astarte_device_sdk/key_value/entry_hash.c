/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "key_value/entry_hash.h"

#include <stdint.h>
#include <string.h>

#include <zephyr/sys/hash_function.h>

#include "key_value/entry.h"
#include "key_value/entry_list.h"

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/* Golden Ratio fraction constant used to distribute elements uniformly during hash combination */
#define HASH_COMBINE_MAGIC 0x9e3779b9U

/* Left and right bit-shift bounds for modifying the avalanche properties of the combined hash */
#define HASH_COMBINE_LSHIFT 6U
#define HASH_COMBINE_RSHIFT 2U

/* Bit width constraint used to extract the upper 32 bits during Lemire's range reduction method */
#define LEMIRE_SHIFT 32U

/************************************************
 * Global functions definitions         *
 ***********************************************/

uint32_t astarte_key_value_entry_hash_generate(const char *namespace, const char *key)
{
    // Generate individual hashes using Zephyr's built-in optimized hash function
    uint32_t hash_nsp = sys_hash32((const void *) namespace, strlen(namespace));
    uint32_t hash_key = sys_hash32((const void *) key, strlen(key));

    // Combine the hashes using a standard permutation
    uint32_t hash = hash_nsp
        ^ (hash_key + HASH_COMBINE_MAGIC + (hash_nsp << HASH_COMBINE_LSHIFT)
            + (hash_nsp >> HASH_COMBINE_RSHIFT));

    // Lemire's Multiply-and-Shift Method for fast, unbiased range mapping
    uint32_t range
        = ASTARTE_KEY_VALUE_ENTRY_MAX_USABLE_ID - ASTARTE_KEY_VALUE_ENTRY_MIN_USABLE_ID + 1;
    uint32_t offset = (uint32_t) (((uint64_t) hash * (uint64_t) range) >> LEMIRE_SHIFT);

    return ASTARTE_KEY_VALUE_ENTRY_MIN_USABLE_ID + offset;
}
