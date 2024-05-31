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
    const char *path, astarte_individual_t individual, const int64_t *timestamp)
{
    astarte_result_t astarte_rc = ASTARTE_RESULT_OK;

    const astarte_mapping_t *mapping = NULL;
    astarte_rc = astarte_interface_get_mapping_from_path(interface, path, &mapping);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Can't find mapping in interface %s for path %s.", interface->name, path);
        return astarte_rc;
    }

    astarte_rc = astarte_mapping_check_individual(mapping, individual);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR(
            "Individual validation failed, interface/path (%s/%s).", interface->name, path);
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
    const char *path, astarte_object_t object, const int64_t *timestamp)
{
    astarte_result_t astarte_rc = ASTARTE_RESULT_OK;

    astarte_object_entry_t *entries = NULL;
    size_t entries_len = 0;
    astarte_rc = astarte_object_to_entries(object, &entries, &entries_len);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Invalid Astarte object.");
        return ASTARTE_RESULT_INVALID_PARAM;
    }

    for (size_t i = 0; i < entries_len; i++) {
        const astarte_mapping_t *mapping = NULL;
        astarte_object_entry_t entry = entries[i];
        astarte_rc
            = astarte_interface_get_mapping_from_paths(interface, path, entry.endpoint, &mapping);
        if (astarte_rc != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Can't find mapping in interface %s for path %s/%s.", interface->name,
                path, entry.endpoint);
            return astarte_rc;
        }

        astarte_rc = astarte_mapping_check_individual(mapping, entry.individual);
        if (astarte_rc != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Individual validation failed, interface/path (%s/%s/%s).",
                interface->name, path, entry.endpoint);
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
    const astarte_interface_t *interface, const char *path, astarte_individual_t individual)
{
    return data_validation_individual_datastream(interface, path, individual, NULL);
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
