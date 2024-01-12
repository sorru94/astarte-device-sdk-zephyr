/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/error.h"

#include <stdlib.h>

#define ERR_TBL_IT(err)                                                                            \
    {                                                                                              \
        err, #err                                                                                  \
    }

typedef struct
{
    astarte_err_t code;
    const char *msg;
} astarte_err_msg_t;

static const astarte_err_msg_t astarte_err_msg_table[] = {
    ERR_TBL_IT(ASTARTE_OK),
    ERR_TBL_IT(ASTARTE_ERR),
};

static const char astarte_unknown_msg[] = "ERROR";

const char *astarte_err_to_name(astarte_err_t code)
{
    for (size_t i = 0; i < sizeof(astarte_err_msg_table) / sizeof(astarte_err_msg_table[0]); ++i) {
        if (astarte_err_msg_table[i].code == code) {
            return astarte_err_msg_table[i].msg;
        }
    }

    return astarte_unknown_msg;
}
