/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "device_checks.h"

#include "interface_private.h"
#include "log.h"
#include "mapping_private.h"

ASTARTE_LOG_MODULE_DECLARE(astarte_device, CONFIG_ASTARTE_DEVICE_SDK_DEVICE_LOG_LEVEL);

astarte_result_t device_checks_individual_datastream(introspection_t *introspection,
    const char *interface_name, const char *path, astarte_value_t value, const int64_t *timestamp,
    int *qos)
{
    astarte_result_t astarte_rc = ASTARTE_RESULT_OK;

    const astarte_interface_t *interface = introspection_get(introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Couldn't find interface in device introspection (%s).", interface_name);
        return ASTARTE_RESULT_INTERFACE_NOT_FOUND;
    }

    const astarte_mapping_t *mapping = NULL;
    astarte_rc = astarte_interface_get_mapping_from_path(interface, path, &mapping);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Couldn't find mapping in interface %s for path %s.", interface_name, path);
        return astarte_rc;
    }

    astarte_rc = astarte_mapping_check_value(mapping, value);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Value validation failed, interface/path (%s/%s).", interface_name, path);
        return astarte_rc;
    }

    if (mapping->explicit_timestamp && !timestamp) {
        ASTARTE_LOG_ERR(
            "Explicit timestamp required for interface %s, path %s.", interface_name, path);
        astarte_rc = ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_REQUIRED;
        return astarte_rc;
    }

    if (!mapping->explicit_timestamp && timestamp) {
        ASTARTE_LOG_ERR(
            "Explicit timestamp not supported for interface %s, path %s.", interface_name, path);
        astarte_rc = ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_NOT_SUPPORTED;
        return astarte_rc;
    }

    if (qos) {
        *qos = mapping->reliability;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t device_checks_aggregated_datastream(introspection_t *introspection,
    const char *interface_name, const char *path, astarte_value_pair_t *values,
    size_t values_length, const int64_t *timestamp, int *qos)
{
    astarte_result_t astarte_rc = ASTARTE_RESULT_OK;

    const astarte_interface_t *interface = introspection_get(introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Couldn't find interface in device introspection (%s).", interface_name);
        return ASTARTE_RESULT_INTERFACE_NOT_FOUND;
    }

    for (size_t i = 0; i < values_length; i++) {
        const astarte_mapping_t *mapping = NULL;
        astarte_value_pair_t value_pair = values[i];
        astarte_rc = astarte_interface_get_mapping_from_paths(
            interface, path, value_pair.endpoint, &mapping);
        if (astarte_rc != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Couldn't find mapping in interface %s for path %s/%s.", interface_name,
                path, value_pair.endpoint);
            return astarte_rc;
        }

        astarte_rc = astarte_mapping_check_value(mapping, value_pair.value);
        if (astarte_rc != ASTARTE_RESULT_OK) {
            ASTARTE_LOG_ERR("Value validation failed, interface/path (%s/%s/%s).", interface_name,
                path, value_pair.endpoint);
            return astarte_rc;
        }

        if (mapping->explicit_timestamp && !timestamp) {
            ASTARTE_LOG_ERR(
                "Explicit timestamp required for interface %s, path %s.", interface_name, path);
            astarte_rc = ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_REQUIRED;
            return astarte_rc;
        }

        if (!mapping->explicit_timestamp && timestamp) {
            ASTARTE_LOG_ERR("Explicit timestamp not supported for interface %s, path %s.",
                interface_name, path);
            astarte_rc = ASTARTE_RESULT_MAPPING_EXPLICIT_TIMESTAMP_NOT_SUPPORTED;
            return astarte_rc;
        }

        // All the QoS are the same in an aggregated interface, as such taking any of them is good
        *qos = mapping->reliability;
    }

    return ASTARTE_RESULT_OK;
}

astarte_result_t device_checks_set_property(introspection_t *introspection,
    const char *interface_name, const char *path, astarte_value_t value)
{
    return device_checks_individual_datastream(
        introspection, interface_name, path, value, NULL, NULL);
}

astarte_result_t device_checks_unset_property(
    introspection_t *introspection, const char *interface_name, const char *path)
{
    astarte_result_t astarte_rc = ASTARTE_RESULT_OK;

    const astarte_interface_t *interface = introspection_get(introspection, interface_name);
    if (!interface) {
        ASTARTE_LOG_ERR("Couldn't find interface in device introspection (%s).", interface_name);
        return ASTARTE_RESULT_INTERFACE_NOT_FOUND;
    }

    const astarte_mapping_t *mapping = NULL;
    astarte_rc = astarte_interface_get_mapping_from_path(interface, path, &mapping);
    if (astarte_rc != ASTARTE_RESULT_OK) {
        ASTARTE_LOG_ERR("Couldn't find mapping in interface %s for path %s.", interface_name, path);
        return astarte_rc;
    }

    if (!mapping->allow_unset) {
        ASTARTE_LOG_ERR("Unset is not allowed for interface %s, path %s.", interface_name, path);
        astarte_rc = ASTARTE_RESULT_MAPPING_UNSET_NOT_ALLOWED;
        return astarte_rc;
    }

    return ASTARTE_RESULT_OK;
}
