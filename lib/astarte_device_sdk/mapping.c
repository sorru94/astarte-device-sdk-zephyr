/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "astarte_device_sdk/mapping.h"
#include "mapping_private.h"

#include <regex.h>

#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_mapping, CONFIG_ASTARTE_DEVICE_SDK_MAPPING_LOG_LEVEL);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_mapping_check_path(astarte_mapping_t mapping, const char *path)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    regex_t preg;
    if (regcomp(&preg, mapping.regex_endpoint, REG_EXTENDED) != 0) {
        ASTARTE_LOG_ERR("Compilation command FAILED.");
        return ASTARTE_RESULT_INTERNAL_ERROR;
    }

    if (regexec(&preg, path, 0, NULL, 0) != 0) {
        res = ASTARTE_RESULT_MAPPING_PATH_MISMATCH;
        goto exit;
    }

exit:
    regfree(&preg);
    return res;
}
