/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data/deserialize_scalar.h"

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "bson/types.h"
#include "mapping_private.h"

#include "log.h"
ASTARTE_LOG_MODULE_DECLARE(data_deserialize, CONFIG_ASTARTE_DEVICE_SDK_DATA_LOG_LEVEL);

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Deserialize a bson element containing a binaryblob.
 *
 * @note This function will perform dynamic allocation.
 *
 * @param[in] bson_elem BSON element to deserialize.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_binaryblob(
    astarte_bson_element_t bson_elem, astarte_data_t *data);

/**
 * @brief Deserialize a bson element containing a string.
 *
 * @note This function will perform dynamic allocation.
 *
 * @param[in] bson_elem BSON element to deserialize.
 * @param[out] data The Astarte data where to store the deserialized data.
 * @return ASTARTE_RESULT_OK upon success, an error code otherwise.
 */
static astarte_result_t deserialize_string(astarte_bson_element_t bson_elem, astarte_data_t *data);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_data_deserialize_scalar(
    astarte_bson_element_t bson_elem, astarte_mapping_type_t type, astarte_data_t *data)
{
    astarte_result_t ares = ASTARTE_RESULT_OK;

    if (!astarte_mapping_bson_is_compatible(type, bson_elem.type)) {
        ASTARTE_LOG_ERR("BSON element is not of the expected type.");
        return ASTARTE_RESULT_BSON_DESERIALIZER_TYPES_ERROR;
    }

    switch (type) {
        case ASTARTE_MAPPING_TYPE_BINARYBLOB:
            ASTARTE_LOG_DBG("Deserializing binary blob data.");
            ares = deserialize_binaryblob(bson_elem, data);
            break;
        case ASTARTE_MAPPING_TYPE_BOOLEAN:
            ASTARTE_LOG_DBG("Deserializing boolean data.");
            bool bool_tmp = astarte_bson_deserializer_element_to_bool(bson_elem);
            *data = astarte_data_from_boolean(bool_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_DATETIME:
            ASTARTE_LOG_DBG("Deserializing datetime data.");
            int64_t datetime_tmp = astarte_bson_deserializer_element_to_datetime(bson_elem);
            *data = astarte_data_from_datetime(datetime_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_DOUBLE:
            ASTARTE_LOG_DBG("Deserializing double data.");
            double double_tmp = astarte_bson_deserializer_element_to_double(bson_elem);
            *data = astarte_data_from_double(double_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_INTEGER:
            ASTARTE_LOG_DBG("Deserializing integer data.");
            int32_t int32_tmp = astarte_bson_deserializer_element_to_int32(bson_elem);
            *data = astarte_data_from_integer(int32_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_LONGINTEGER:
            ASTARTE_LOG_DBG("Deserializing long integer data.");
            int64_t int64_tmp = 0U;
            if (bson_elem.type == ASTARTE_BSON_TYPE_INT32) {
                int64_tmp = (int64_t) astarte_bson_deserializer_element_to_int32(bson_elem);
            } else {
                int64_tmp = astarte_bson_deserializer_element_to_int64(bson_elem);
            }
            *data = astarte_data_from_longinteger(int64_tmp);
            break;
        case ASTARTE_MAPPING_TYPE_STRING:
            ASTARTE_LOG_DBG("Deserializing string data.");
            ares = deserialize_string(bson_elem, data);
            break;
        default:
            ASTARTE_LOG_ERR("Unsupported mapping type.");
            ares = ASTARTE_RESULT_INTERNAL_ERROR;
            break;
    }
    return ares;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static astarte_result_t deserialize_binaryblob(
    astarte_bson_element_t bson_elem, astarte_data_t *data)
{
    uint32_t deserialized_len = 0;
    const uint8_t *deserialized
        = astarte_bson_deserializer_element_to_binary(bson_elem, &deserialized_len);

    scope_var(scoped_uint8, deserialized_buf)(deserialized_len);
    if (!deserialized_buf) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    memcpy(deserialized_buf, deserialized, deserialized_len);

    // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
    *data = astarte_data_from_binaryblob((const void *) deserialized_buf, deserialized_len);

    // Transfer ownership and disarm the auto-cleanup
    deserialized_buf = NULL;

    return ASTARTE_RESULT_OK;
}

static astarte_result_t deserialize_string(astarte_bson_element_t bson_elem, astarte_data_t *data)
{
    uint32_t deserialized_len = 0;
    const char *deserialized
        = astarte_bson_deserializer_element_to_string(bson_elem, &deserialized_len);

    scope_var(scoped_char, deserialized_buf)(deserialized_len + 1);
    if (!deserialized_buf) {
        ASTARTE_LOG_ERR("Out of memory %s: %d", __FILE__, __LINE__);
        return ASTARTE_RESULT_OUT_OF_MEMORY;
    }

    strncpy(deserialized_buf, deserialized, deserialized_len);

    // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
    *data = astarte_data_from_string(deserialized_buf);

    // Transfer ownership and disarm the auto-cleanup
    deserialized_buf = NULL;

    return ASTARTE_RESULT_OK;
}
