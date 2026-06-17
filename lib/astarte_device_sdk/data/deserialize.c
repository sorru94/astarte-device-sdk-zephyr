/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data/deserialize.h"

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "bson/deserializer.h"
#include "bson/types.h"
#include "data/deserialize_array.h"
#include "data/deserialize_scalar.h"
#include "interface_private.h"
#include "mapping_private.h"

#include "log.h"
ASTARTE_LOG_MODULE_REGISTER(data_deserialize, CONFIG_ASTARTE_DEVICE_SDK_DATA_LOG_LEVEL);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_data_deserialize(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    switch (type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
        case ASTARTE_MAPPING_TYPE_DATETIME:
        case ASTARTE_MAPPING_TYPE_DOUBLE:
        case ASTARTE_MAPPING_TYPE_INTEGER:
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
        case ASTARTE_MAPPING_TYPE_STRING:
            ares = astarte_data_deserialize_scalar(bson_elem, type, data);
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            ares = astarte_data_deserialize_array(bson_elem, type, data);
            break;
        default:
            ASTARTE_LOG_ERR("Unsupported mapping type.");
            ares = ASTARTE_RESULT_INTERNAL_ERROR;
            break;
    }

    if (ares == ASTARTE_RESULT_OK) {
        data->is_owned = true;
    }

    return ares;
}

void astarte_data_destroy_deserialized(astarte_data_t data)
{
    if (!data.is_owned) {
        return;
    }

    switch (data.tag) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
            astarte_free((void *) data.data.binaryblob.buf);
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            astarte_free((void *) data.data.string);
            break;
        case ASTARTE_MAPPING_TYPE_INTEGERARRAY:
            astarte_free((void *) data.data.integer_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY:
            astarte_free((void *) data.data.longinteger_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLEARRAY:
            astarte_free((void *) data.data.double_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_STRINGARRAY:
            for (size_t i = 0; i < data.data.string_array.len; i++) {
                astarte_free((void *) data.data.string_array.buf[i]);
            }
            astarte_free((void *) data.data.string_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY:
            for (size_t i = 0; i < data.data.binaryblob_array.count; i++) {
                astarte_free((void *) data.data.binaryblob_array.blobs[i]);
            }
            astarte_free(data.data.binaryblob_array.sizes);
            astarte_free((void *) data.data.binaryblob_array.blobs);
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEANARRAY:
            astarte_free((void *) data.data.boolean_array.buf);
            break;
        case ASTARTE_MAPPING_TYPE_DATETIMEARRAY:
            astarte_free((void *) data.data.datetime_array.buf);
            break;
        default:
            break;
    }
}
