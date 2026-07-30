// (C) Copyright 2026, SECO Mind Srl
//
// SPDX-License-Identifier: Apache-2.0

#include "alloc.h"

#include <zephyr/kernel.h>

#ifndef CONFIG_ASTARTE_DEVICE_SDK_ENABLE_HEAP
#include <stdlib.h>
#endif

#ifdef CONFIG_ASTARTE_DEVICE_SDK_ENABLE_HEAP
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
// NOLINTBEGIN(readability-math-missing-parentheses)
K_HEAP_DEFINE(astarte_sdk_heap, CONFIG_ASTARTE_DEVICE_SDK_HEAP_SIZE);
// NOLINTEND(readability-math-missing-parentheses)
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
#endif

void *astarte_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
#ifndef CONFIG_ASTARTE_DEVICE_SDK_ENABLE_HEAP
    return k_malloc(size);
#else
    return k_heap_alloc(&astarte_sdk_heap, size, K_NO_WAIT);
#endif
}

void *astarte_calloc(size_t nmemb, size_t size)
{
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
#ifndef CONFIG_ASTARTE_DEVICE_SDK_ENABLE_HEAP
    return k_calloc(nmemb, size);
#else
    size_t total_size = nmemb * size;

    void *ptr = k_heap_alloc(&astarte_sdk_heap, total_size, K_NO_WAIT);

    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }

    return ptr;
#endif
}

void *astarte_realloc(void *ptr, size_t size)
{
    // standard realloc behavior for NULL pointers
    if (ptr == NULL) {
        return astarte_malloc(size);
    }

    // standard realloc behavior for 0 size
    if (size == 0) {
        astarte_free(ptr);
        return NULL;
    }

#ifndef CONFIG_ASTARTE_DEVICE_SDK_ENABLE_HEAP
    return k_realloc(ptr, size);
#else
    return k_heap_realloc(&astarte_sdk_heap, ptr, size, K_NO_WAIT);
#endif
}

void astarte_free(void *ptr)
{
#ifndef CONFIG_ASTARTE_DEVICE_SDK_ENABLE_HEAP
    k_free(ptr);
#else
    if (ptr != NULL) {
        k_heap_free(&astarte_sdk_heap, ptr);
    }
#endif
}
