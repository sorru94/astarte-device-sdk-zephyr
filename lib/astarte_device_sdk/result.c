/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/result.h"

const char *astarte_result_to_name(astarte_result_t code)
{
    switch (code) {
#define X_SWITCH(name, value)                                                                      \
    case name:                                                                                     \
        return #name;
        ASTARTE_RESULT_MAP(X_SWITCH)
#undef X_SWITCH
        default:
            return "UNKNOWN RESULT CODE";
    }
}
