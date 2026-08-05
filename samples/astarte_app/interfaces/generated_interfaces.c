/**
 * @file generated_interfaces.c
 * @brief Contains automatically generated interfaces.
 *
 * @warning Do not modify this file manually.
 */

// NOLINENUMBERLINT

// clang-format off

#include "generated_interfaces.h"

// Interface names should resemble as closely as possible their respective .json file names.
// NOLINTBEGIN(readability-identifier-naming)

/** @brief Automatically generated mapping definition. */
static const astarte_mapping_t org_astarteplatform_zephyr_examples_DeviceAggregate_mappings[14] = {
    {
        .endpoint = "/%{sensor_id}/double_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLE,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/integer_endpoint",
        .type = ASTARTE_MAPPING_TYPE_INTEGER,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/boolean_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BOOLEAN,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/longinteger_endpoint",
        .type = ASTARTE_MAPPING_TYPE_LONGINTEGER,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/string_endpoint",
        .type = ASTARTE_MAPPING_TYPE_STRING,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/binaryblob_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOB,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/datetime_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DATETIME,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/doublearray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLEARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/integerarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_INTEGERARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/booleanarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BOOLEANARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/longintegerarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/stringarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_STRINGARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/binaryblobarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/datetimearray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DATETIMEARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
};

/** @brief Automatically generated interface definition. */
const astarte_interface_t org_astarteplatform_zephyr_examples_DeviceAggregate = {
    .name = "org.astarteplatform.zephyr.examples.DeviceAggregate",
    .major_version = 0,
    .minor_version = 1,
    .type = ASTARTE_INTERFACE_TYPE_DATASTREAM,
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_DEVICE,
    .aggregation = ASTARTE_INTERFACE_AGGREGATION_OBJECT,
    .mappings = org_astarteplatform_zephyr_examples_DeviceAggregate_mappings,
    .mappings_length = 14U,
};

/** @brief Automatically generated mapping definition. */
static const astarte_mapping_t org_astarteplatform_zephyr_examples_DeviceDatastream_mappings[14] = {
    {
        .endpoint = "/binaryblob_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOB,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/binaryblobarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/boolean_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BOOLEAN,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/booleanarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BOOLEANARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/datetime_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DATETIME,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/datetimearray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DATETIMEARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/double_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLE,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/doublearray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLEARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/integer_endpoint",
        .type = ASTARTE_MAPPING_TYPE_INTEGER,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/integerarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_INTEGERARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/longinteger_endpoint",
        .type = ASTARTE_MAPPING_TYPE_LONGINTEGER,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/longintegerarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/string_endpoint",
        .type = ASTARTE_MAPPING_TYPE_STRING,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/stringarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_STRINGARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
};

/** @brief Automatically generated interface definition. */
const astarte_interface_t org_astarteplatform_zephyr_examples_DeviceDatastream = {
    .name = "org.astarteplatform.zephyr.examples.DeviceDatastream",
    .major_version = 0,
    .minor_version = 1,
    .type = ASTARTE_INTERFACE_TYPE_DATASTREAM,
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_DEVICE,
    .aggregation = ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL,
    .mappings = org_astarteplatform_zephyr_examples_DeviceDatastream_mappings,
    .mappings_length = 14U,
};

/** @brief Automatically generated mapping definition. */
static const astarte_mapping_t org_astarteplatform_zephyr_examples_DeviceProperty_mappings[14] = {
    {
        .endpoint = "/%{sensor_id}/double_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLE,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/integer_endpoint",
        .type = ASTARTE_MAPPING_TYPE_INTEGER,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/boolean_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BOOLEAN,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/longinteger_endpoint",
        .type = ASTARTE_MAPPING_TYPE_LONGINTEGER,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/string_endpoint",
        .type = ASTARTE_MAPPING_TYPE_STRING,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/binaryblob_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOB,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/datetime_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DATETIME,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/doublearray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLEARRAY,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/integerarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_INTEGERARRAY,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/booleanarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BOOLEANARRAY,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/longintegerarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/stringarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_STRINGARRAY,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/binaryblobarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/datetimearray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DATETIMEARRAY,
        .allow_unset = true,
    },
};

/** @brief Automatically generated interface definition. */
const astarte_interface_t org_astarteplatform_zephyr_examples_DeviceProperty = {
    .name = "org.astarteplatform.zephyr.examples.DeviceProperty",
    .major_version = 0,
    .minor_version = 1,
    .type = ASTARTE_INTERFACE_TYPE_PROPERTIES,
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_DEVICE,
    .aggregation = ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL,
    .mappings = org_astarteplatform_zephyr_examples_DeviceProperty_mappings,
    .mappings_length = 14U,
};

/** @brief Automatically generated mapping definition. */
static const astarte_mapping_t org_astarteplatform_zephyr_examples_ServerAggregate_mappings[14] = {
    {
        .endpoint = "/%{sensor_id}/double_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLE,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/integer_endpoint",
        .type = ASTARTE_MAPPING_TYPE_INTEGER,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/boolean_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BOOLEAN,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/longinteger_endpoint",
        .type = ASTARTE_MAPPING_TYPE_LONGINTEGER,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/string_endpoint",
        .type = ASTARTE_MAPPING_TYPE_STRING,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/binaryblob_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOB,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/datetime_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DATETIME,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/doublearray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLEARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/integerarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_INTEGERARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/booleanarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BOOLEANARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/longintegerarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/stringarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_STRINGARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/binaryblobarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/%{sensor_id}/datetimearray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DATETIMEARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
};

/** @brief Automatically generated interface definition. */
const astarte_interface_t org_astarteplatform_zephyr_examples_ServerAggregate = {
    .name = "org.astarteplatform.zephyr.examples.ServerAggregate",
    .major_version = 0,
    .minor_version = 1,
    .type = ASTARTE_INTERFACE_TYPE_DATASTREAM,
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_SERVER,
    .aggregation = ASTARTE_INTERFACE_AGGREGATION_OBJECT,
    .mappings = org_astarteplatform_zephyr_examples_ServerAggregate_mappings,
    .mappings_length = 14U,
};

/** @brief Automatically generated mapping definition. */
static const astarte_mapping_t org_astarteplatform_zephyr_examples_ServerDatastream_mappings[14] = {
    {
        .endpoint = "/binaryblob_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOB,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/binaryblobarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/boolean_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BOOLEAN,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/booleanarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BOOLEANARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/datetime_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DATETIME,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/datetimearray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DATETIMEARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/double_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLE,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/doublearray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLEARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = true,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/integer_endpoint",
        .type = ASTARTE_MAPPING_TYPE_INTEGER,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/integerarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_INTEGERARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/longinteger_endpoint",
        .type = ASTARTE_MAPPING_TYPE_LONGINTEGER,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/longintegerarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/string_endpoint",
        .type = ASTARTE_MAPPING_TYPE_STRING,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
    {
        .endpoint = "/stringarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_STRINGARRAY,
        .reliability = ASTARTE_MAPPING_RELIABILITY_UNRELIABLE,
        .explicit_timestamp = false,
        .required = false,
        .retention = ASTARTE_MAPPING_RETENTION_DISCARD,
    },
};

/** @brief Automatically generated interface definition. */
const astarte_interface_t org_astarteplatform_zephyr_examples_ServerDatastream = {
    .name = "org.astarteplatform.zephyr.examples.ServerDatastream",
    .major_version = 0,
    .minor_version = 1,
    .type = ASTARTE_INTERFACE_TYPE_DATASTREAM,
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_SERVER,
    .aggregation = ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL,
    .mappings = org_astarteplatform_zephyr_examples_ServerDatastream_mappings,
    .mappings_length = 14U,
};

/** @brief Automatically generated mapping definition. */
static const astarte_mapping_t org_astarteplatform_zephyr_examples_ServerProperty_mappings[14] = {
    {
        .endpoint = "/%{sensor_id}/double_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLE,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/integer_endpoint",
        .type = ASTARTE_MAPPING_TYPE_INTEGER,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/boolean_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BOOLEAN,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/longinteger_endpoint",
        .type = ASTARTE_MAPPING_TYPE_LONGINTEGER,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/string_endpoint",
        .type = ASTARTE_MAPPING_TYPE_STRING,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/binaryblob_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOB,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/datetime_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DATETIME,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/doublearray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DOUBLEARRAY,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/integerarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_INTEGERARRAY,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/booleanarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BOOLEANARRAY,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/longintegerarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_LONGINTEGERARRAY,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/stringarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_STRINGARRAY,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/binaryblobarray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_BINARYBLOBARRAY,
        .allow_unset = true,
    },
    {
        .endpoint = "/%{sensor_id}/datetimearray_endpoint",
        .type = ASTARTE_MAPPING_TYPE_DATETIMEARRAY,
        .allow_unset = true,
    },
};

/** @brief Automatically generated interface definition. */
const astarte_interface_t org_astarteplatform_zephyr_examples_ServerProperty = {
    .name = "org.astarteplatform.zephyr.examples.ServerProperty",
    .major_version = 0,
    .minor_version = 1,
    .type = ASTARTE_INTERFACE_TYPE_PROPERTIES,
    .ownership = ASTARTE_INTERFACE_OWNERSHIP_SERVER,
    .aggregation = ASTARTE_INTERFACE_AGGREGATION_INDIVIDUAL,
    .mappings = org_astarteplatform_zephyr_examples_ServerProperty_mappings,
    .mappings_length = 14U,
};

// NOLINTEND(readability-identifier-naming)
