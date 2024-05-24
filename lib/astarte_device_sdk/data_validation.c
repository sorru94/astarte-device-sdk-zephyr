/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "data_validation.h"

#include "interface_private.h"
#include "log.h"
#include "mapping_private.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t data_validation_individual_datastream(const astarte_interface_t *interface,
    const char *path, astarte_value_t value, const int64_t *timestamp)
{
    astarte_result_t astarte_rc = ASTARTE_RESULT_OK;

    const astarte_mapping_t *mapping = NULL;
    astarte_rc = astarte_interface_get_mapping_from_path(interface, path, &mapping);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Can't find mapping in interface %s for path %s.", interface->name, path);
        return astarte_rc;
    }

    astarte_rc = astarte_mapping_check_value(mapping, value);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Value validation failed, interface/path (%s/%s).", interface->name, path);
        return astarte_rc;
    }

    if (mapping->explicit_timestamp && !timestamp) {
        ASTARTE_LOG_ERR(
            "Explicit timestamp required for interface %s, path %s.", interface->name, path);
        astarte_rc = ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_REQUIRED;
        return astarte_rc;
    }

    if (!mapping->explicit_timestamp && timestamp) {
        ASTARTE_LOG_ERR(
            "Explicit timestamp not supported for interface %s, path %s.", interface->name, path);
        astarte_rc = ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_NOT_SUPPORTED;
        return astarte_rc;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t data_validation_aggregated_datastream(const astarte_interface_t *interface,
    const char *path, astarte_value_pair_array_t value_pair_array, const int64_t *timestamp)
{
    astarte_result_t astarte_rc = ASTARTE_RESULT_OK;

    astarte_value_pair_t *value_pairs = NULL;
    size_t value_pairs_len = 0;
    astarte_rc
        = astarte_value_pair_array_to_value_pairs(value_pair_array, &value_pairs, &value_pairs_len);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Invalid Astarte value pair array.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    for (size_t i = 0; i < value_pairs_len; i++) {
        const astarte_mapping_t *mapping = NULL;
        astarte_value_pair_t value_pair = value_pairs[i];
        astarte_rc = astarte_interface_get_mapping_from_paths(
            interface, path, value_pair.endpoint, &mapping);
        if (astarte_rc != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Can't find mapping in interface %s for path %s/%s.", interface->name,
                path, value_pair.endpoint);
            return astarte_rc;
        }

        astarte_rc = astarte_mapping_check_value(mapping, value_pair.value);
        if (astarte_rc != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Value validation failed, interface/path (%s/%s/%s).", interface->name,
                path, value_pair.endpoint);
            return astarte_rc;
        }

        if (mapping->explicit_timestamp && !timestamp) {
            ASTARTE_LOG_ERR(
                "Explicit timestamp required for interface %s, path %s.", interface->name, path);
            astarte_rc = ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_REQUIRED;
            return astarte_rc;
        }

        if (!mapping->explicit_timestamp && timestamp) {
            ASTARTE_LOG_ERR("Explicit timestamp not supported for interface %s, path %s.",
                interface->name, path);
            astarte_rc = ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_NOT_SUPPORTED;
            return astarte_rc;
        }
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t data_validation_set_property(
    const astarte_interface_t *interface, const char *path, astarte_value_t value)
{
    return data_validation_individual_datastream(interface, path, value, NULL);
}

astarte_result_t data_validation_unset_property(
    const astarte_interface_t *interface, const char *path)
{
    astarte_result_t astarte_rc = ASTARTE_RESULT_OK;

    const astarte_mapping_t *mapping = NULL;
    astarte_rc = astarte_interface_get_mapping_from_path(interface, path, &mapping);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Can't find mapping in interface %s for path %s.", interface->name, path);
        return astarte_rc;
    }

    if (!mapping->allow_unset) {
        ASTARTE_LOG_ERR("Unset is not allowed for interface %s, path %s.", interface->name, path);
        astarte_rc = ASTARTE_RESULT_MAPPING_UNSET_NOT_ALLOWED;
        return astarte_rc;
    }

    return ASTARTE_RESULT_OK;
}
