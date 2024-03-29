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

astarte_result_t astarte_mapping_array_to_scalar_type(
    astarte_mapping_type_t array_type, astarte_mapping_type_t *scalar_type)
{
    astarte_result_t res = ASTARTE_RESULT_OK;
    switch (array_type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
            *scalar_type = ASTARTE_MAPPING_TYPE_BINARYBLOB;
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
            *scalar_type = ASTARTE_MAPPING_TYPE_BOOLEAN;
            break;
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
            *scalar_type = ASTARTE_MAPPING_TYPE_DATETIME;
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
            *scalar_type = ASTARTE_MAPPING_TYPE_DOUBLE;
            break;
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
            *scalar_type = ASTARTE_MAPPING_TYPE_INTEGER;
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
            *scalar_type = ASTARTE_MAPPING_TYPE_LONGINTEGER;
            break;
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            *scalar_type = ASTARTE_MAPPING_TYPE_STRING;
            break;
        default:
            ASTARTE_LOG_ERR("Attempting to conversion array->scalar on non array type.");
            res = ASTARTE_RESULT_INTERNAL_ERROR;
            break;
    }
    return res;
}

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
    }

    regfree(&preg);
    return res;
}
