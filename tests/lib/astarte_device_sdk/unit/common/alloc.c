/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

void *astarte_malloc(size_t size)
{
    return malloc(size);
}

void *astarte_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

void *astarte_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void astarte_free(void *ptr)
{
    free(ptr);
}
