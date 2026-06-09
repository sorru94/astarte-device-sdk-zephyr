/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ASTARTE_ALLOC_H
#define ASTARTE_ALLOC_H

/**
 * @file alloc.h
 * @brief Memory allocation abstraction layer for the Astarte Device SDK.
 *
 * @note This module provides wrappers around standard memory allocation functions
 * to allow for custom memory management implementations if needed.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocates size bytes of uninitialized memory.
 *
 * @param[in] size The size of the memory block to allocate, in bytes.
 * @return A pointer to the allocated memory. If size is 0 or the allocation fails, returns NULL.
 */
void *astarte_malloc(size_t size);

/**
 * @brief Allocates memory for an array of elements and initializes all bytes in the allocated
 * storage to zero.
 *
 * @param[in] nmemb The number of elements to allocate.
 * @param[in] size The size of each element, in bytes.
 * @return A pointer to the allocated memory. If nmemb or size is 0, or if the allocation fails,
 * returns NULL.
 */
void *astarte_calloc(size_t nmemb, size_t size);

/**
 * @brief Changes the size of the memory block pointed to by ptr to size bytes.
 *
 * @param[in] ptr Pointer to the memory block to be reallocated. If NULL, the behavior is identical
 * to astarte_malloc(size).
 * @param[in] size The new size of the memory block, in bytes.
 * @return A pointer to the newly allocated memory block. If the allocation fails, returns NULL and
 * the original memory block is left untouched.
 */
void *astarte_realloc(void *ptr, size_t size);

/**
 * @brief Frees the memory space pointed to by ptr.
 *
 * @param[in] ptr Pointer to the memory block to be freed. If NULL, no operation is performed.
 */
void astarte_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* ASTARTE_ALLOC_H */
