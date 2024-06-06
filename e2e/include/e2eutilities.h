/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef E2EUTILITIES_H
#define E2EUTILITIES_H

#include "astarte_device_sdk/interface.h"
#include "e2erunner.h"

#include <stdlib.h>

/**
 * @file e2eutilities.h
 * @brief Utilities needed in e2e tests
 */

#define CHECK_HALT(expr, ...)                                                                      \
    if (expr) {                                                                                    \
        LOG_ERR(__VA_ARGS__); /* NOLINT */                                                         \
        k_fatal_halt(-1);                                                                          \
    }

#define CHECK_ASTARTE_OK_HALT(expr, ...) CHECK_HALT(expr != ASTARTE_RESULT_OK, __VA_ARGS__)

// clang-format off
#define MAKE_ASTARTE_INDIVIDUAL_SCALAR(ENUM, PARAM, VALUE)                                         \
    (astarte_individual_t)                                                                         \
    {                                                                                              \
        .data = {                                                                                  \
            .PARAM = (VALUE),                                                                      \
        },                                                                                         \
        .tag = (ENUM), \
    }

#define MAKE_ASTARTE_INDIVIDUAL_ARRAY(ENUM, PARAM, ARRAY, LEN)                                     \
    (astarte_individual_t)                                                                         \
    {                                                                                              \
        .data = {                                                                                  \
            .PARAM = {                                                                             \
                .buf = (ARRAY),                                                                    \
                .len = (LEN),                                                                      \
            },                                                                                     \
        },                                                                                         \
        .tag = (ENUM), \
    }

#define MAKE_ASTARTE_INDIVIDUAL_BINARYBLOB_ARRAY(BLOBS, SIZES, COUNT)                              \
    (astarte_individual_t)                                                                         \
    {                                                                                              \
        .data = {                                                                                  \
            .binaryblob_array = {                                                                  \
                .blobs = (BLOBS),                                                                  \
                .sizes = (SIZES),                                                                  \
                .count = (COUNT),                                                                  \
            },                                                                                     \
        },                                                                                         \
        .tag = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY,                                               \
    }

// clang-format on

e2e_interface_data_t *get_e2e_interface_data(
    e2e_interface_data_array_t *interfaces_array, const char *interface_name);

const e2e_individual_data_t *get_e2e_individual_data(
    const e2e_individual_data_array_t *mapping_array, const char *endpoint);

const astarte_object_entry_t *get_astarte_object_entry(
    const e2e_object_entry_array_t *value_pair_array, const char *endpoint);

bool astarte_value_equal(astarte_individual_t *a, astarte_individual_t *b);

#endif /* E2EUTILITIES_H */
