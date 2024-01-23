#ifndef AD_INTERFACE_H
#define AD_INTERFACE_H

#include <inttypes.h>

/**
 * @brief interface ownership
 *
 * This enum represents the possible ownerhips of an Astarte interface
 * https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#astarte-interface-schema-ownership
 */
typedef enum
{
    OWNERSHIP_DEVICE = 1, /**< Device owned interface */
    OWNERSHIP_SERVER, /**< Server owned interface*/
} ad_interface_ownership_t;

/**
 * @brief interface type
 *
 * This enum represents the possible types of an Astarte interface
 * https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#astarte-interface-schema-type
 */
typedef enum
{
    TYPE_DATASTREAM = 1, /**< Datastream interface */
    TYPE_PROPERTIES, /**< Properties interface */
} ad_interface_type_t;

/**
 * @brief interface aggregation
 *
 * This enum represents the possible Astarte interface aggregation
 * https://docs.astarte-platform.org/astarte/latest/040-interface_schema.html#astarte-interface-schema-aggregation
 */
typedef enum
{
    AGGREGATION_INDIVIDUAL = 1, /**< Aggregation individual */
    AGGREGATION_OBJECT, /**< Aggregation object */
} ad_interface_aggregation_t;

/**
 * @brief Astarte interface definition
 *
 * This struct represents a subset of the information contained in an Astarte interface, and can be
 * used to specify some details about a specific interface.
 * 
 */
typedef struct
{
    const char *name; /**< Interface name */
    uint32_t major_version; /**< Major version */
    uint32_t minor_version; /**< Minor version */
    ad_interface_ownership_t ownership; /**< Ownership, see #interface_ownership_t */
    ad_interface_type_t type; /**< Type, see #interface_type_t */
} ad_interface_t;

#endif // AD_INTERFACE_H
