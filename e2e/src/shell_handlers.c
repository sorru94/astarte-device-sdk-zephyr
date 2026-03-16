/*
 * (C) Copyright 2026, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "shell_handlers.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/base64.h>

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/interface.h>
#include <data_deserialize.h>
#include <interface_private.h>
#include <object_private.h>

#include "data.h"
#include "device_handler.h"
#include "utilities.h"

LOG_MODULE_REGISTER(shell_handlers, CONFIG_SHELL_HANDLERS_LOG_LEVEL);

/************************************************
 *   Constants, static variables and defines    *
 ***********************************************/

#define DISCONNECT_CMD dvcshellcmd_disconnect
#define DISCONNECT_HELP "Disconnect the device and end the executable"

#define SEND_CMD dvcshellcmd_send
#define SEND_HELP "Send device data"

#define SEND_SUBCMD_SET send_subcommand_set

#define SEND_INDIVIDUAL_ARG individual
#define SEND_INDIVIDUAL_HELP                                                                       \
    "Send an individual property from the device with the data passed as argument. This command "  \
    "expects <interface_name> <path> <bson_value> <optional_timestamp>"

#define SEND_OBJECT_ARG object
#define SEND_OBJECT_HELP                                                                           \
    "Send an object from the device with the data passed as argument. This command expects "       \
    "<interface_name> <path> <bson_value> <optional_timestamp>"

#define SEND_PROPERTY_SUBCMD property
#define SEND_PROPERTY_HELP "Handle send of property interfaces subcommand."

#define SEND_PROPERTY_SUBCMD_SET send_property_subcommand_set

#define SEND_PROPERTY_SET_ARG set
#define SEND_PROPERTY_SET_HELP                                                                     \
    "Set a property with the data passed as argument. This command expects <interface_name> "      \
    "<path> <bson_value>"

#define SEND_PROPERTY_UNSET_ARG unset
#define SEND_PROPERTY_UNSET_HELP                                                                   \
    "Unset a property with the data passed as argument. This command expects <interface_name> "    \
    "<path>"

#define EXPECT_CMD dvcshellcmd_expect
#define EXPECT_HELP "Set the data expected from the server"

#define EXPECT_SUBCMD_SET expect_subcommand_set

#define EXPECT_INDIVIDUAL_ARG individual
#define EXPECT_INDIVIDUAL_HELP                                                                     \
    "Expect an individual property from the device with the data passed as argument. This "        \
    "command expects <interface_name> <path> <bson_value> <optional_timestamp>"

#define EXPECT_OBJECT_ARG object
#define EXPECT_OBJECT_HELP                                                                         \
    "Expect an object with the data passed as argument. This command expects <interface_name> "    \
    "<path> <bson_value> <optional_timestamp>"

#define EXPECT_PROPERTY_SUBCMD property
#define EXPECT_PROPERTY_HELP "Expect a property."

#define EXPECT_PROPERTY_SUBCMD_SET expect_property_subcommand_set

#define EXPECT_PROPERTY_SET_ARG set
#define EXPECT_PROPERTY_SET_HELP                                                                   \
    "Expect a property with the data passed as argument. This command expects <interface_name> "   \
    "<path> <bson_value>"

#define EXPECT_PROPERTY_UNSET_ARG unset
#define EXPECT_PROPERTY_UNSET_HELP                                                                 \
    "Expect an unset of the property with the data passed as argument. This command expects "      \
    "<interface_name> <path>"

/************************************************
 *         Static functions declaration         *
 ***********************************************/

static int cmd_disconnect(const struct shell *sh, size_t argc, char **argv);

static int cmd_send_individual(const struct shell *shell, size_t argc, char **argv);
static int cmd_send_object(const struct shell *shell, size_t argc, char **argv);
static int cmd_send_property_set(const struct shell *shell, size_t argc, char **argv);
static int cmd_send_property_unset(const struct shell *shell, size_t argc, char **argv);

static int cmd_expect_individual(const struct shell *shell, size_t argc, char **argv);
static int cmd_expect_object(const struct shell *shell, size_t argc, char **argv);
static int cmd_expect_property_set(const struct shell *shell, size_t argc, char **argv);
static int cmd_expect_property_unset(const struct shell *shell, size_t argc, char **argv);

static void shell_helpers_skip_param(char ***args, size_t *argc);
static const astarte_interface_t *shell_helpers_next_interface_param(char ***args, size_t *argc);
static char *shell_helpers_next_alloc_string_param(char ***args, size_t *argc);
static byte_array_t shell_helpers_next_alloc_base64_param(char ***args, size_t *argc);
static optional_int64_t shell_helpers_next_timestamp_param(char ***args, size_t *argc);

static int parse_bson_to_alloc_astarte_invividual(
    const astarte_interface_t *interface, char *path, byte_array_t *buf, astarte_data_t *out_data);
static int parse_bson_to_alloc_astarte_object(const astarte_interface_t *interface, char *path,
    byte_array_t *buf, astarte_object_entry_t **out_entries, size_t *out_entries_length);

/************************************************
 *          Shell commands declaration          *
 ***********************************************/

SHELL_CMD_REGISTER(DISCONNECT_CMD, NULL, DISCONNECT_HELP, cmd_disconnect);

SHELL_STATIC_SUBCMD_SET_CREATE(SEND_PROPERTY_SUBCMD_SET,
    SHELL_CMD_ARG(SEND_PROPERTY_SET_ARG, NULL, SEND_PROPERTY_SET_HELP, cmd_send_property_set, 4, 0),
    SHELL_CMD_ARG(
        SEND_PROPERTY_UNSET_ARG, NULL, SEND_PROPERTY_UNSET_HELP, cmd_send_property_unset, 3, 0),
    SHELL_SUBCMD_SET_END);
SHELL_STATIC_SUBCMD_SET_CREATE(SEND_SUBCMD_SET,
    SHELL_CMD_ARG(SEND_INDIVIDUAL_ARG, NULL, SEND_INDIVIDUAL_HELP, cmd_send_individual, 4, 1),
    SHELL_CMD_ARG(SEND_OBJECT_ARG, NULL, SEND_OBJECT_HELP, cmd_send_object, 4, 1),
    SHELL_CMD(SEND_PROPERTY_SUBCMD, &SEND_PROPERTY_SUBCMD_SET, SEND_PROPERTY_HELP, NULL),
    SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(SEND_CMD, &SEND_SUBCMD_SET, SEND_HELP, NULL);

SHELL_STATIC_SUBCMD_SET_CREATE(EXPECT_PROPERTY_SUBCMD_SET,
    SHELL_CMD_ARG(
        EXPECT_PROPERTY_SET_ARG, NULL, EXPECT_PROPERTY_SET_HELP, cmd_expect_property_set, 4, 0),
    SHELL_CMD_ARG(EXPECT_PROPERTY_UNSET_ARG, NULL, EXPECT_PROPERTY_UNSET_HELP,
        cmd_expect_property_unset, 3, 0),
    SHELL_SUBCMD_SET_END);
SHELL_STATIC_SUBCMD_SET_CREATE(EXPECT_SUBCMD_SET,
    SHELL_CMD_ARG(EXPECT_INDIVIDUAL_ARG, NULL, EXPECT_INDIVIDUAL_HELP, cmd_expect_individual, 4, 1),
    SHELL_CMD_ARG(EXPECT_OBJECT_ARG, NULL, EXPECT_OBJECT_HELP, cmd_expect_object, 4, 1),
    SHELL_CMD(EXPECT_PROPERTY_SUBCMD, &EXPECT_PROPERTY_SUBCMD_SET, EXPECT_PROPERTY_HELP, NULL),
    SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(EXPECT_CMD, &EXPECT_SUBCMD_SET, EXPECT_HELP, NULL);

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static int cmd_disconnect(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    LOG_INF("Disconnect command handler");
    LOG_INF("Stopping and joining the astarte device polling thread.");
    disconnect_device();
    return 0;
}

static int cmd_send_individual(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);

    int return_code = 1;
    char *path = NULL;
    byte_array_t value = { 0 };
    astarte_data_t data = { 0 };

    LOG_INF("Send individual command handler");

    // First parameter is the command name (maybe?)
    shell_helpers_skip_param(&argv, &argc);

    // Second parameter is the interface name
    const astarte_interface_t *interface = shell_helpers_next_interface_param(&argv, &argc);
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");

    // Third parameter is the path
    path = shell_helpers_next_alloc_string_param(&argv, &argc);
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");

    // Fourth parameter is the payload
    value = shell_helpers_next_alloc_base64_param(&argv, &argc);
    CHECK_GOTO(value.len == 0, cleanup, "Invalid individual parameter passed");
    CHECK_GOTO(parse_bson_to_alloc_astarte_invividual(interface, path, &value, &data) != 0, cleanup,
        "Could not parse the BSON to an Astarte individual");

    // Fifth parameter is the timestamp
    optional_int64_t timestamp = shell_helpers_next_timestamp_param(&argv, &argc);

    // Tansmit the payload using the Astarte device
    CHECK_GOTO(device_send_individual(interface->name, path, data, timestamp) != 0, cleanup,
        "Failed to send individual to astarte");

    LOG_INF("Individual datastream sent");
    return_code = 0;

cleanup:
    data_destroy_deserialized(data);
    free((void *) value.buf);
    free(path);

    return return_code;
}

static int cmd_send_object(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);

    int return_code = 1;
    char *path = NULL;
    byte_array_t value = { 0 };
    astarte_object_entry_t *entries = NULL;
    size_t entries_length = 0;

    LOG_INF("Send object command handler");

    // First parameter is the command name (maybe?)
    shell_helpers_skip_param(&argv, &argc);

    // Second parameter is the interface name
    const astarte_interface_t *interface = shell_helpers_next_interface_param(&argv, &argc);
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");

    // Third parameter is the path
    path = shell_helpers_next_alloc_string_param(&argv, &argc);
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");

    // Fourth parameter is the payload
    value = shell_helpers_next_alloc_base64_param(&argv, &argc);
    CHECK_GOTO(value.len == 0, cleanup, "Invalid object parameter passed");
    CHECK_GOTO(
        parse_bson_to_alloc_astarte_object(interface, path, &value, &entries, &entries_length) != 0,
        cleanup, "Could not parse the BSON to an Astarte object");

    // Fifth parameter is the timestamp
    optional_int64_t timestamp = shell_helpers_next_timestamp_param(&argv, &argc);

    // Set the property using the Astarte device
    CHECK_GOTO(device_send_object(interface->name, path, entries, entries_length, timestamp) != 0,
        cleanup, "Failed to send object to astarte");

    LOG_INF("Object datastream sent");
    return_code = 0;

cleanup:
    astarte_object_entries_destroy_deserialized(entries, entries_length);
    free((void *) value.buf);
    free(path);

    return return_code;
}

static int cmd_send_property_set(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);

    int return_code = 1;
    char *path = NULL;
    byte_array_t value = { 0 };
    astarte_data_t data = { 0 };

    LOG_INF("Send property set command handler");

    // First parameter is the command name (maybe?)
    shell_helpers_skip_param(&argv, &argc);

    // Second parameter is the interface name
    const astarte_interface_t *interface = shell_helpers_next_interface_param(&argv, &argc);
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");

    // Third parameter is the path
    path = shell_helpers_next_alloc_string_param(&argv, &argc);
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");

    // Fourth parameter is the payload
    value = shell_helpers_next_alloc_base64_param(&argv, &argc);
    CHECK_GOTO(value.len == 0, cleanup, "Invalid individual parameter passed");
    CHECK_GOTO(parse_bson_to_alloc_astarte_invividual(interface, path, &value, &data) != 0, cleanup,
        "Could not parse the BSON to an Astarte individual");

    // Tansmit the payload using the Astarte device
    CHECK_GOTO(device_set_property(interface->name, path, data) != 0, cleanup,
        "Failed to set property to astarte");

    LOG_INF("InterfaceTestingPropertySetTestElement set");
    return_code = 0;

cleanup:
    data_destroy_deserialized(data);
    free((void *) value.buf);
    free(path);

    return return_code;
}

static int cmd_send_property_unset(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);

    int return_code = 1;
    char *path = NULL;

    LOG_INF("Send property unset command handler");

    // First parameter is the command name (maybe?)
    shell_helpers_skip_param(&argv, &argc);

    // Second parameter is the interface name
    const astarte_interface_t *interface = shell_helpers_next_interface_param(&argv, &argc);
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");

    // Third parameter is the path
    path = shell_helpers_next_alloc_string_param(&argv, &argc);
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");

    // Tansmit the payload using the Astarte device
    CHECK_GOTO(device_unset_property(interface->name, path) != 0, cleanup,
        "Failed to unset property to astarte");

    LOG_INF("InterfaceTestingPropertySetTestElement unset");
    return_code = 0;

cleanup:
    free(path);

    return return_code;
}

static int cmd_expect_individual(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);

    char *path = NULL;
    byte_array_t value = { 0 };
    astarte_data_t data = { 0 };

    LOG_INF("Expect individual command handler");

    // First parameter is the command name (maybe?)
    shell_helpers_skip_param(&argv, &argc);

    // Second parameter is the interface name
    const astarte_interface_t *interface = shell_helpers_next_interface_param(&argv, &argc);
    CHECK_GOTO(!interface, error, "Invalid interface name passed");

    // Third parameter is the path
    path = shell_helpers_next_alloc_string_param(&argv, &argc);
    CHECK_GOTO(!path, error, "Invalid path parameter passed");

    // Fourth parameter is the payload
    value = shell_helpers_next_alloc_base64_param(&argv, &argc);
    CHECK_GOTO(value.len == 0, error, "Invalid individual parameter passed");
    CHECK_GOTO(parse_bson_to_alloc_astarte_invividual(interface, path, &value, &data) != 0, error,
        "Could not parse the BSON to an Astarte individual");

    // Fifth parameter is the timestamp
    optional_int64_t timestamp = shell_helpers_next_timestamp_param(&argv, &argc);

    // Add the individual data to the epected data list
    CHECK_GOTO(data_add_individual(interface, path, &data, &timestamp) != 0, error,
        "Could not add individual to expected data");

    LOG_INF("Individual added to the expected list.");

    free((void *) value.buf);
    return 0;

error:
    free(path);
    free((void *) value.buf);
    data_destroy_deserialized(data);

    return 1;
}

static int cmd_expect_object(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);

    char *path = NULL;
    byte_array_t value = { 0 };
    astarte_object_entry_t *entries = NULL;
    size_t entries_length = 0;

    LOG_INF("Expect object command handler");

    // First parameter is the command name (maybe?)
    shell_helpers_skip_param(&argv, &argc);

    // Second parameter is the interface name
    const astarte_interface_t *interface = shell_helpers_next_interface_param(&argv, &argc);
    CHECK_GOTO(!interface, error, "Invalid interface name passed");

    // Third parameter is the path
    path = shell_helpers_next_alloc_string_param(&argv, &argc);
    CHECK_GOTO(!path, error, "Invalid path parameter passed");

    // Fourth parameter is the payload
    value = shell_helpers_next_alloc_base64_param(&argv, &argc);
    CHECK_GOTO(value.len == 0, error, "Invalid object parameter passed");
    CHECK_GOTO(
        parse_bson_to_alloc_astarte_object(interface, path, &value, &entries, &entries_length) != 0,
        error, "Could not parse the BSON to an Astarte object");

    // Fifth parameter is the timestamp
    optional_int64_t timestamp = shell_helpers_next_timestamp_param(&argv, &argc);

    // Add the object data to the epected data list
    // NOTE: The bson parser perform only a shallow copy of some strings, so we need to pass
    // ownership of the value buffer to the expected data list, so it can be freed when the expected
    // data is cleared.
    CHECK_GOTO(data_add_object(interface, path, &value, entries, entries_length, &timestamp) != 0,
        error, "Could not add object to expected data");

    LOG_INF("Object added to the expected list.");

    return 0;

error:
    free(path);
    free((void *) value.buf);
    astarte_object_entries_destroy_deserialized(entries, entries_length);

    return 1;
}

static int cmd_expect_property_set(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);

    char *path = NULL;
    byte_array_t value = { 0 };
    astarte_data_t data = { 0 };

    LOG_INF("Expect set property command handler");

    // First parameter is the command name (maybe?)
    shell_helpers_skip_param(&argv, &argc);

    // Second parameter is the interface name
    const astarte_interface_t *interface = shell_helpers_next_interface_param(&argv, &argc);
    CHECK_GOTO(!interface, error, "Invalid interface name passed");

    // Third parameter is the path
    path = shell_helpers_next_alloc_string_param(&argv, &argc);
    CHECK_GOTO(!path, error, "Invalid path parameter passed");

    // Fourth parameter is the payload
    value = shell_helpers_next_alloc_base64_param(&argv, &argc);
    CHECK_GOTO(value.len == 0, error, "Invalid individual parameter passed");
    CHECK_GOTO(parse_bson_to_alloc_astarte_invividual(interface, path, &value, &data) != 0, error,
        "Could not parse the BSON to an Astarte individual");

    // Add the set property data to the expected data list
    CHECK_GOTO(data_add_set_property(interface, path, &data) != 0, error,
        "Could not add set property to expected data");

    LOG_INF("Set property added to the expected list.");

    free((void *) value.buf);
    return 0;

error:
    free(path);
    free((void *) value.buf);
    data_destroy_deserialized(data);

    return 1;
}

static int cmd_expect_property_unset(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(shell);

    char *path = NULL;

    LOG_INF("Expect unset property command handler");

    // First parameter is the command name (maybe?)
    shell_helpers_skip_param(&argv, &argc);

    // Second parameter is the interface name
    const astarte_interface_t *interface = shell_helpers_next_interface_param(&argv, &argc);
    CHECK_GOTO(!interface, error, "Invalid interface name passed");

    // Third parameter is the path
    path = shell_helpers_next_alloc_string_param(&argv, &argc);
    CHECK_GOTO(!path, error, "Invalid path parameter passed");

    // Add the unset property data to the expected data list
    CHECK_GOTO(data_add_unset_property(interface, path) != 0, error,
        "Could not add unset property to expected data");

    LOG_INF("Unset property added to the expected list.");

    return 0;

error:
    free(path);
    return 1;
}

static void shell_helpers_skip_param(char ***args, size_t *argc)
{
    if (*argc < 1) {
        // no more arguments
        return;
    }

    *args += 1;
    *argc -= 1;
}

static const astarte_interface_t *shell_helpers_next_interface_param(char ***args, size_t *argc)
{
    if (*argc < 1) {
        // no more arguments
        return NULL;
    }

    const char *const interface_name = (*args)[0];
    const astarte_interface_t *interface = data_get_interface(
        interface_name, strlen(interface_name));

    if (!interface) {
        // Interface not found
        LOG_ERR("Invalid interface name %s", interface_name);
        return NULL;
    }

    // move to the next parameter for caller
    *args += 1;
    *argc -= 1;
    return interface;
}

static char *shell_helpers_next_alloc_string_param(char ***args, size_t *argc)
{
    if (*argc < 1) {
        // no more arguments
        return NULL;
    }

    const char *const arg = (*args)[0];

    size_t arg_len = strlen(arg);
    char *const copied_arg = calloc(arg_len + 1, sizeof(char));
    CHECK_HALT(!copied_arg, "Could not allocate memory to copy string parameter");
    memcpy(copied_arg, arg, arg_len + 1);

    // move to the next parameter for caller
    *args += 1;
    *argc -= 1;
    return (char *) copied_arg;
}

static byte_array_t shell_helpers_next_alloc_base64_param(char ***args, size_t *argc)
{
    uint8_t *byte_array = NULL;

    if (*argc < 1) {
        // no more arguments
        goto error;
    }

    const char *const arg = (*args)[0];
    const size_t arg_len = strlen(arg);

    size_t byte_array_length = 0;
    int res = base64_decode(NULL, 0, &byte_array_length, arg, arg_len);
    if (byte_array_length == 0) {
        LOG_ERR("Error while computing base64 decode buffer length: %d", res);
        goto error;
    }

    LOG_DBG("The size of the decoded buffer is: %d", byte_array_length);

    byte_array = calloc(byte_array_length, sizeof(uint8_t));
    CHECK_HALT(!byte_array, "Out of memory");

    res = base64_decode(byte_array, byte_array_length, &byte_array_length, arg, arg_len);
    if (res != 0) {
        LOG_ERR("Error while decoding base64 argument %d", res);
        goto error;
    }

    // move to the next parameter for caller
    *args += 1;
    *argc -= 1;
    return (byte_array_t){ .buf = byte_array, .len = byte_array_length };

error:
    free(byte_array);
    return (byte_array_t){};
}

static optional_int64_t shell_helpers_next_timestamp_param(char ***args, size_t *argc)
{
    const int base = 10;

    if (*argc < 1) {
        // no more arguments
        return (optional_int64_t){};
    }

    const char *const arg = (*args)[0];
    const int64_t timestamp = (int64_t) strtoll(arg, NULL, base);

    // move to the next parameter for caller
    *args += 1;
    *argc -= 1;
    return (optional_int64_t){
        .value = timestamp,
        .present = true,
    };
}

static int parse_bson_to_alloc_astarte_invividual(
    const astarte_interface_t *interface, char *path, byte_array_t *buf, astarte_data_t *out_data)
{
    const astarte_mapping_t *mapping = NULL;
    astarte_result_t res = astarte_interface_get_mapping_from_path(interface, path, &mapping);
    CHECK_ASTARTE_OK_RET_1(
        res, "Error while searching for the mapping (%d) %s", res, astarte_result_to_name(res));

    CHECK_RET_1(!astarte_bson_deserializer_check_validity(buf->buf, buf->len),
        "Invalid BSON document in data");
    astarte_bson_document_t full_document = astarte_bson_deserializer_init_doc(buf->buf);
    astarte_bson_element_t v_elem = { 0 };
    CHECK_ASTARTE_OK_RET_1(astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem),
        "Cannot retrieve BSON value from data");

    CHECK_ASTARTE_OK_RET_1(data_deserialize(v_elem, mapping->type, out_data),
        "Couldn't deserialize received binary data into object entries");

    return 0;
}

static int parse_bson_to_alloc_astarte_object(const astarte_interface_t *interface, char *path,
    byte_array_t *buf, astarte_object_entry_t **out_entries, size_t *out_entries_length)
{
    CHECK_RET_1(!astarte_bson_deserializer_check_validity(buf->buf, buf->len),
        "Invalid BSON document in data");
    astarte_bson_document_t full_document = astarte_bson_deserializer_init_doc(buf->buf);
    astarte_bson_element_t v_elem = { 0 };
    CHECK_ASTARTE_OK_RET_1(astarte_bson_deserializer_element_lookup(full_document, "v", &v_elem),
        "Cannot retrieve BSON value from data");

    CHECK_ASTARTE_OK_RET_1(astarte_object_entries_deserialize(
                               v_elem, interface, path, out_entries, out_entries_length),
        "Couldn't deserialize received binary data into object entries");

    return 0;
}
