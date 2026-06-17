// (C) Copyright 2026, SECO Mind Srl
//
// SPDX-License-Identifier: Apache-2.0

#include "alloc.h"

#include <zephyr/kernel.h>

#ifndef CONFIG_ASTARTE_DEVICE_SDK_ENABLE_HEAP
#include <stdlib.h>
#endif

#ifdef CONFIG_ASTARTE_DEVICE_SDK_ENABLE_HEAP
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
K_HEAP_DEFINE(astarte_sdk_heap, CONFIG_ASTARTE_DEVICE_SDK_HEAP_SIZE);
#endif

void *astarte_malloc(size_t size)
{
#ifndef CONFIG_ASTARTE_DEVICE_SDK_ENABLE_HEAP
    return k_malloc(size);
#else
    if (size == 0) {
        return NULL;
    }

    return k_heap_alloc(&astarte_sdk_heap, size, K_NO_WAIT);
#endif
}

void *astarte_calloc(size_t nmemb, size_t size)
{
#ifndef CONFIG_ASTARTE_DEVICE_SDK_ENABLE_HEAP
    return k_calloc(nmemb, size);
#else
    size_t total_size = nmemb * size;

    if (total_size == 0) {
        return NULL;
    }

    void *ptr = k_heap_alloc(&astarte_sdk_heap, total_size, K_NO_WAIT);

    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }

    return ptr;
#endif
}

void *astarte_realloc(void *ptr, size_t size)
{
#ifndef CONFIG_ASTARTE_DEVICE_SDK_ENABLE_HEAP
    return k_realloc(ptr, size);
#else
    if (ptr == NULL) {
        return astarte_malloc(size);
    }

    if (size == 0) {
        astarte_free(ptr);
        return NULL;
    }

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
