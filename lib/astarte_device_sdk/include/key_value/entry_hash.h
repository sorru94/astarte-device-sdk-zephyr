/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KEY_VALUE_ENTRY_HASH_H
#define KEY_VALUE_ENTRY_HASH_H

/**
 * @file key_value/entry_hash.h
 * @brief Hash function helpers for the key value library.
 */

#include <stdint.h>

/**
 * @brief Generate an hash from the namespace and key.
 *
 * @param[in] namespace Used for generation of the hash.
 * @param[in] key Used for generation of the hash.
 * @return The generated hash.
 */
uint16_t astarte_key_value_entry_hash_generate(const char *namespace, const char *key);

#endif // KEY_VALUE_ENTRY_HASH_H
