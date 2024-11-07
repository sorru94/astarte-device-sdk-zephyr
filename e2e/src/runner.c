/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "runner.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/fatal.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/bitarray.h>
#include <zephyr/sys/util.h>

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/individual.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/mapping.h>
#include <astarte_device_sdk/object.h>
#include <astarte_device_sdk/pairing.h>
#include <astarte_device_sdk/result.h>
#include <astarte_device_sdk/util.h>
#include <individual_private.h>
#include <interface_private.h>
#include <object_private.h>

#include "idata.h"
#include "log.h"
#include "utilities.h"

#include "astarte_generated_interfaces.h"
#include "utils.h"

/************************************************
 * Static command handler declarations
 ***********************************************/

static int cmd_expect_object_handler(const struct shell *sh, size_t argc, char **argv);
static int cmd_expect_individual_handler(const struct shell *sh, size_t argc, char **argv);
static int cmd_expect_property_set_handler(const struct shell *sh, size_t argc, char **argv);
static int cmd_expect_property_unset_handler(const struct shell *sh, size_t argc, char **argv);
static int cmd_expect_verify_handler(const struct shell *sh, size_t argc, char **argv);

static int cmd_send_individual_handler(const struct shell *sh, size_t argc, char **argv);
static int cmd_send_object_handler(const struct shell *sh, size_t argc, char **argv);
static int cmd_send_property_set_handler(const struct shell *sh, size_t argc, char **argv);
static int cmd_send_property_unset_handler(const struct shell *sh, size_t argc, char **argv);

static int cmd_disconnect(const struct shell *sh, size_t argc, char **argv);

/************************************************
 * Shell commands declaration
 ***********************************************/

SHELL_STATIC_SUBCMD_SET_CREATE(expect_property_subcommand,
    SHELL_CMD_ARG(set, NULL,
        "Expect a property with the data passed as argument."
        " This command expects <interface_name> <path> <bson_value>",
        cmd_expect_property_set_handler, 4, 0),
    SHELL_CMD_ARG(unset, NULL,
        "Expect an unset of the property with the data passed as argument."
        " This command expects <interface_name> <path>",
        cmd_expect_property_unset_handler, 3, 0),
    SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(expect_subcommand,
    SHELL_CMD_ARG(individual, NULL,
        "Expect an individual property from the device with the data passed as argument."
        " This command expects <interface_name> <path> <bson_value> <optional_timestamp>",
        cmd_expect_individual_handler, 4, 1),
    SHELL_CMD_ARG(object, NULL,
        "Expect an object with the data passed as argument."
        " This command expects <interface_name> <path> <bson_value> <optional_timestamp>",
        cmd_expect_object_handler, 4, 1),
    SHELL_CMD(property, &expect_property_subcommand, "Expect a property.", NULL),
    SHELL_CMD(verify, NULL, "Check that all the expected messages got received.",
        cmd_expect_verify_handler),
    SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(send_property_subcommand,
    SHELL_CMD_ARG(set, NULL,
        "Set a property with the data passed as argument."
        " This command expects <interface_name> <path> <bson_value>",
        cmd_send_property_set_handler, 4, 0),
    SHELL_CMD_ARG(unset, NULL,
        "Unset a property with the data passed as argument."
        " This command expects <interface_name> <path>",
        cmd_send_property_unset_handler, 3, 0),
    SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(send_subcommand,
    SHELL_CMD_ARG(individual, NULL,
        "Send an individual property from the device with the data passed as argument."
        " This command expects <interface_name> <path> <bson_value> <optional_timestamp>",
        cmd_send_individual_handler, 4, 1),
    SHELL_CMD_ARG(object, NULL,
        "Send an object from the device with the data passed as argument."
        " This command expects <interface_name> <path> <bson_value> <optional_timestamp>",
        cmd_send_object_handler, 4, 1),
    SHELL_CMD(property, &send_property_subcommand, "Handle send of property interfaces subcommand.",
        NULL),
    SHELL_SUBCMD_SET_END);

// TODO if the return code of a command is 1 i would expect the e2etest to fail currently however
// this is not the case and everything continues as normal
SHELL_CMD_REGISTER(expect, &expect_subcommand, "Set the data expected from the server", NULL);
SHELL_CMD_REGISTER(send, &send_subcommand, "Send device data", NULL);
SHELL_CMD_REGISTER(
    disconnect, NULL, "Disconnect the device and end the executable", cmd_disconnect);

/************************************************
 * Constants, static variables and defines
 ***********************************************/

#define MAIN_THREAD_SLEEP_MS 500

LOG_MODULE_REGISTER(runner, CONFIG_RUNNER_LOG_LEVEL); // NOLINT

K_THREAD_STACK_DEFINE(device_thread_stack_area, CONFIG_DEVICE_THREAD_STACK_SIZE);
static struct k_thread device_thread_data;

enum e2e_thread_flags
{
    DEVICE_CONNECTED_FLAG = 0,
    THREAD_TERMINATION_FLAG,
};
static atomic_t device_thread_flags;

const astarte_interface_t *interfaces[] = {
    &org_astarte_platform_zephyr_e2etest_DeviceAggregate,
    &org_astarte_platform_zephyr_e2etest_DeviceDatastream,
    &org_astarte_platform_zephyr_e2etest_DeviceProperty,
    &org_astarte_platform_zephyr_e2etest_ServerAggregate,
    &org_astarte_platform_zephyr_e2etest_ServerDatastream,
    &org_astarte_platform_zephyr_e2etest_ServerProperty,
};

static astarte_device_handle_t device_handle;

// written by expect command hadlers read by the device callback functions
static e2e_idata_t expected_data;
static size_t expected_data_count = 0;
static size_t unexpected_data_count = 0;
K_MUTEX_DEFINE(expected_data_mutex);

K_SEM_DEFINE(e2e_run_semaphore, 1, 1);

/************************************************
 * Static functions declaration
 ***********************************************/

static void device_thread_entry_point(void *device_handle, void *unused1, void *unused2);
// these callbacks handle device_thread_flags
static void connection_callback(astarte_device_connection_event_t event);
static void disconnection_callback(astarte_device_disconnection_event_t event);
// --
// these functions read device_thread_flags and wait appropriately
static void wait_for_connection();
static void wait_for_disconnection();
// --
static void device_setup();
static int parse_alloc_astarte_invividual(const astarte_interface_t *interface, char *path,
    e2e_byte_array *buf, astarte_individual_t *out_individual);
static int parse_alloc_astarte_object(const astarte_interface_t *interface, char *path,
    e2e_byte_array *buf, astarte_object_entry_t **entries, size_t *entries_length);
// device data callbacks
static void device_individual_callback(astarte_device_datastream_individual_event_t event);
static void device_object_callback(astarte_device_datastream_object_event_t event);
static void device_property_set_callback(astarte_device_property_set_event_t event);
static void device_property_unset_callback(astarte_device_data_event_t event);

/************************************************
 * Global functions definition
 ***********************************************/

// guarded by the semaphore e2e_run_semaphore
void run_e2e_test()
{
    // only one instance of the e2e tests can run at a time
    CHECK_HALT(
        k_sem_take(&e2e_run_semaphore, K_FOREVER) != 0, "Could not wait for the test semaphore");

    LOG_INF("Running e2e test"); // NOLINT
    // initialize idata, no need to lock since no shell is active currenlty
    expected_data = idata_init();
    // sets up the global device_handle
    device_setup();

    LOG_INF("Spawning a new thread to poll data from the Astarte device."); // NOLINT
    k_thread_create(&device_thread_data, device_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_thread_stack_area), device_thread_entry_point, device_handle,
        NULL, NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);

    // wait while device connects
    wait_for_connection();

    // we are ready to send and receive data
    // the shell will get stopped in the disconnect command
    shell_start(shell_backend_uart_get_ptr());
    shell_print(shell_backend_uart_get_ptr(), "Device shell ready");

    // the disconnection happens when the "disconnect" shell command gets called
    wait_for_disconnection();

    idata_free(expected_data);

    // give control to next instance if needed
    k_sem_give(&e2e_run_semaphore);
}

/************************************************
 * Static functions definitions
 ***********************************************/

static void device_setup()
{
    astarte_device_config_t config = {
        .device_id = CONFIG_DEVICE_ID,
        .cred_secr = CONFIG_CREDENTIAL_SECRET,
        .interfaces = interfaces,
        .interfaces_size = ARRAY_SIZE(interfaces),
        .http_timeout_ms = CONFIG_HTTP_TIMEOUT_MS,
        .mqtt_connection_timeout_ms = CONFIG_MQTT_CONNECTION_TIMEOUT_MS,
        .mqtt_poll_timeout_ms = CONFIG_MQTT_POLL_TIMEOUT_MS,
        .datastream_individual_cbk = device_individual_callback,
        .datastream_object_cbk = device_object_callback,
        .property_set_cbk = device_property_set_callback,
        .property_unset_cbk = device_property_unset_callback,
        .connection_cbk = connection_callback,
        .disconnection_cbk = disconnection_callback,
    };

    LOG_INF("Creating static astarte_device by calling astarte_device_new."); // NOLINT
    CHECK_ASTARTE_OK_HALT(
        astarte_device_new(&config, &device_handle), "Astarte device creation failure.");
}

static void device_individual_callback(astarte_device_datastream_individual_event_t event)
{
    LOG_INF("Individual datastream callback");

    CHECK_HALT(
        k_mutex_lock(&expected_data_mutex, K_FOREVER) != 0, "Could not lock expected data mutex");
    if (idata_is_empty(&expected_data)) {
        LOG_INF("Idata is empty not performing any check");
        goto unlock;
    }

    e2e_individual_data_t *individual = { 0 };
    // inizialization
    idata_get_individual(&expected_data, event.data_event.interface_name, &individual);

    for (; individual != NULL;
        idata_get_individual(&expected_data, event.data_event.interface_name, &individual)) {
        if (strcmp(individual->path, event.data_event.path) != 0) {
            // skip element if on a different path
            continue;
        }

        LOG_DBG("Comparing values\nExpected:");
        utils_log_astarte_individual(individual->individual);
        LOG_DBG("Received:");
        utils_log_astarte_individual(event.individual);

        if (!astarte_individual_equal(&individual->individual, &event.individual)) {
            LOG_ERR("Unexpected element received on path '%s'", individual->path);
            unexpected_data_count += 1;
            goto unlock;
        }

        LOG_INF("Received expected value on '%s' '%s'", event.data_event.interface_name,
            individual->path);

        utils_log_astarte_individual(event.individual);

        expected_data_count += 1;
        idata_remove_individual(individual);

        goto unlock;
    }

    LOG_ERR("No more expected individual but got data on interface '%s'",
        event.data_event.interface_name);

unlock:
    k_mutex_unlock(&expected_data_mutex);
}

static void device_object_callback(astarte_device_datastream_object_event_t event)
{
    LOG_INF("Object datastream callback");

    CHECK_HALT(
        k_mutex_lock(&expected_data_mutex, K_FOREVER) != 0, "Could not lock expected data mutex");
    if (idata_is_empty(&expected_data)) {
        LOG_INF("Idata is empty not performing any check");
        goto unlock;
    }

    e2e_object_entry_array_t received = {
        .buf = event.entries,
        .len = event.entries_len,
    };

    e2e_object_data_t *object = NULL;
    // inizialization
    idata_get_object(&expected_data, event.data_event.interface_name, &object);

    for (; object != NULL;
        idata_get_object(&expected_data, event.data_event.interface_name, &object)) {
        if (strcmp(object->path, event.data_event.path) != 0) {
            // skip element if on a different path
            continue;
        }

        if (!astarte_object_equal(&object->entries, &received)) {
            LOG_ERR("Unexpected element received on path '%s'", object->path);
            unexpected_data_count += 1;
            goto unlock;
        }

        LOG_INF(
            "Received expected value on '%s' '%s'", event.data_event.interface_name, object->path);

        expected_data_count += 1;
        idata_remove_object(object);

        goto unlock;
    }

    LOG_ERR("No more expected individual but got data on interface '%s'",
        event.data_event.interface_name);

unlock:
    k_mutex_unlock(&expected_data_mutex);
}

static void device_property_set_callback(astarte_device_property_set_event_t event)
{
    LOG_INF("Property set callback");

    CHECK_HALT(
        k_mutex_lock(&expected_data_mutex, K_FOREVER) != 0, "Could not lock expected data mutex");
    if (idata_is_empty(&expected_data)) {
        LOG_INF("Idata is empty not performing any check");
        goto unlock;
    }

    e2e_property_data_t *property = NULL;
    // inizialization
    idata_get_property(&expected_data, event.data_event.interface_name, &property);

    for (; property != NULL;
        idata_get_property(&expected_data, event.data_event.interface_name, &property)) {
        if (strcmp(property->path, event.data_event.path) != 0) {
            // skip element if on a different path
            continue;
        }

        LOG_DBG("Comparing values\nExpected:");
        utils_log_astarte_individual(property->individual);
        LOG_DBG("Received:");
        utils_log_astarte_individual(event.individual);

        if (!astarte_individual_equal(&property->individual, &event.individual)) {
            LOG_ERR("Unexpected element received on path '%s'", property->path);
            unexpected_data_count += 1;
            goto unlock;
        }

        LOG_INF("Received expected value on '%s' '%s'", event.data_event.interface_name,
            property->path);

        utils_log_astarte_individual(event.individual);

        expected_data_count += 1;
        idata_remove_property(property);

        goto unlock;
    }

    LOG_ERR("No more expected individual but got data on interface '%s'",
        event.data_event.interface_name);

unlock:
    k_mutex_unlock(&expected_data_mutex);
}

static void device_property_unset_callback(astarte_device_data_event_t event)
{
    LOG_INF("Property unset callback");

    CHECK_HALT(
        k_mutex_lock(&expected_data_mutex, K_FOREVER) != 0, "Could not lock expected data mutex");
    if (idata_is_empty(&expected_data)) {
        LOG_INF("Idata is empty not performing any check");
        goto unlock;
    }

    e2e_property_data_t *property = NULL;
    // inizialization
    idata_get_property(&expected_data, event.interface_name, &property);

    for (; property != NULL; idata_get_property(&expected_data, event.interface_name, &property)) {
        if (strcmp(property->path, event.path) != 0) {
            // skip element if on a different path
            continue;
        }

        if (!property->unset) {
            LOG_ERR("Unexpected unset received on path '%s'", property->path);
            unexpected_data_count += 1;
            goto unlock;
        }

        LOG_INF("Received expected unset on '%s' '%s'", event.interface_name, property->path);

        expected_data_count += 1;
        idata_remove_property(property);

        goto unlock;
    }

    LOG_ERR("No more expected unsets but got data on interface '%s'", event.interface_name);

unlock:
    k_mutex_unlock(&expected_data_mutex);
}

// start expect commands handler
static int cmd_expect_individual_handler(const struct shell *sh, size_t argc, char **argv)
{
    LOG_INF("Expect individual command handler"); // NOLINT

    // null variables definition
    char *path = { 0 };
    e2e_byte_array individual_value = { 0 };
    astarte_individual_t individual = { 0 };

    // ignore the first parameter since it's the name of the command itself
    skip_parameter(&argv, &argc);
    const astarte_interface_t *interface = next_interface_parameter(
        &argv, &argc, interfaces, ARRAY_SIZE(interfaces));
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
    path = next_alloc_string_parameter(&argv, &argc);
    // In theory this can't fail since we are not performing checks... and the number of argument is
    // checked
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");
    individual_value = next_alloc_base64_parameter(&argv, &argc);
    CHECK_GOTO(individual_value.len == 0, cleanup, "Invalid individual parameter passed");
    e2e_timestamp_option_t timestamp = next_timestamp_parameter(&argv, &argc);

    CHECK_GOTO(parse_alloc_astarte_invividual(interface, path, &individual_value, &individual) != 0,
        cleanup, "Could not parse and allocate astarte individual");

    CHECK_GOTO(k_mutex_lock(&expected_data_mutex, K_FOREVER) != 0, cleanup,
        "Could not lock expected data mutex");

    // path and individual will be freed by the idata_unit free function
    CHECK_GOTO(idata_add_individual(&expected_data, interface,
                   (e2e_individual_data_t) {
                       .individual = individual,
                       .path = path,
                       .timestamp = timestamp,
                   })
            != 0,
        cleanup, "Could not insert individual in idata list");

    k_mutex_unlock(&expected_data_mutex);

    // should get freed even if no errors occur because it is not stored anywhere and it's not
    // needed
    free(individual_value.buf);
    return 0;

cleanup:
    free(path);
    free(individual_value.buf);
    astarte_individual_destroy_deserialized(individual);
    return 1;
}

static int cmd_expect_object_handler(const struct shell *sh, size_t argc, char **argv)
{
    LOG_INF("Expect object command handler"); // NOLINT

    // null variables definition
    char *path = { 0 };
    e2e_byte_array object_bytes = { 0 };
    astarte_object_entry_t *entries = { 0 };
    size_t entries_length = { 0 };

    // ignore the first parameter since it's the name of the command itself
    skip_parameter(&argv, &argc);
    const astarte_interface_t *interface = next_interface_parameter(
        &argv, &argc, interfaces, ARRAY_SIZE(interfaces));
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
    path = next_alloc_string_parameter(&argv, &argc);
    // In theory this can't fail since we are not performing checks and the number of argument is
    // checked
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");
    object_bytes = next_alloc_base64_parameter(&argv, &argc);
    CHECK_GOTO(object_bytes.len == 0, cleanup, "Invalid object parameter passed");
    e2e_timestamp_option_t timestamp = next_timestamp_parameter(&argv, &argc);

    CHECK_GOTO(
        parse_alloc_astarte_object(interface, path, &object_bytes, &entries, &entries_length) != 0,
        cleanup, "Could not parse and allocate astarte object entries");

    CHECK_GOTO(k_mutex_lock(&expected_data_mutex, K_FOREVER) != 0, cleanup,
        "Could not lock expected data mutex");

    ASTARTE_LOG_INF("Adding object: ");
    astarte_object_print(&(e2e_object_entry_array_t) {
        .buf = entries,
        .len = entries_length,
    });

    // path, object_bytes and object_entries will be freed by the idata_unit free function
    CHECK_GOTO(idata_add_object(&expected_data, interface, (e2e_object_data_t) {
             .entries = {
                 .buf = entries,
                 .len = entries_length,
             },
            .path = path,
            // do not free in case of objects since entries keys are pointing to this buffer
            .object_bytes = object_bytes,
            .timestamp = timestamp,
         }) != 0, cleanup, "Could not add object entry to idata list");

    k_mutex_unlock(&expected_data_mutex);

    return 0;

cleanup:
    free(path);
    free(object_bytes.buf);
    astarte_object_entries_destroy_deserialized(entries, entries_length);
    return 1;
}

static int cmd_expect_property_set_handler(const struct shell *sh, size_t argc, char **argv)
{
    LOG_INF("Expect set property command handler"); // NOLINT

    // null variables definition
    char *path = { 0 };
    e2e_byte_array property_value = { 0 };
    astarte_individual_t individual = { 0 };

    // ignore the first parameter since it's the name of the command itself
    skip_parameter(&argv, &argc);
    const astarte_interface_t *interface = next_interface_parameter(
        &argv, &argc, interfaces, ARRAY_SIZE(interfaces));
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
    path = next_alloc_string_parameter(&argv, &argc);
    // In theory this can't fail since we are not performing checks... and the number of argument is
    // checked
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");
    property_value = next_alloc_base64_parameter(&argv, &argc);
    CHECK_GOTO(property_value.len == 0, cleanup, "Invalid individual parameter passed");

    CHECK_GOTO(parse_alloc_astarte_invividual(interface, path, &property_value, &individual) != 0,
        cleanup, "Could not deserialize and allocate astarte individual");

    CHECK_GOTO(k_mutex_lock(&expected_data_mutex, K_FOREVER) != 0, cleanup,
        "Could not lock expected data mutex");

    // path and individual will be freed by the idata_unit free function
    CHECK_GOTO(idata_add_property(&expected_data, interface,
                   (e2e_property_data_t) {
                       .individual = individual,
                       .path = path,
                   })
            != 0,
        cleanup, "Could not add property to idata list");

    k_mutex_unlock(&expected_data_mutex);

    // should get freed even if no errors occur because it is not stored anywhere and it's not
    // needed
    free(property_value.buf);
    return 0;

cleanup:
    free(path);
    free(property_value.buf);
    astarte_individual_destroy_deserialized(individual);
    return 1;
}

static int cmd_expect_property_unset_handler(const struct shell *sh, size_t argc, char **argv)
{
    LOG_INF("Expect unset property command handler"); // NOLINT

    // null variables definition
    char *path = { 0 };

    // ignore the first parameter since it's the name of the command itself
    skip_parameter(&argv, &argc);
    const astarte_interface_t *interface = next_interface_parameter(
        &argv, &argc, interfaces, ARRAY_SIZE(interfaces));
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
    path = next_alloc_string_parameter(&argv, &argc);
    // In theory this can't fail since we are not performing checks... and the number of argument is
    // checked
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");

    CHECK_GOTO(k_mutex_lock(&expected_data_mutex, K_FOREVER) != 0, cleanup,
        "Could not lock expected data mutex");

    CHECK_GOTO(idata_add_property(&expected_data, interface,
                   (e2e_property_data_t) {
                       .path = path,
                       .unset = true,
                   })
            != 0,
        cleanup, "Could not add property to idata list");

    k_mutex_unlock(&expected_data_mutex);

    return 0;

cleanup:
    free(path);
    return 1;
}

static int cmd_expect_verify_handler(const struct shell *sh, size_t argc, char **argv)
{
    wait_for_connection();

    LOG_INF("Expect verify command handler"); // NOLINT

    size_t num = 0;

    CHECK_RET_1(
        k_mutex_lock(&expected_data_mutex, K_FOREVER) != 0, "Could not lock expected data mutex");

    e2e_idata_unit_t *expected_iter = idata_iter(&expected_data);

    while (expected_iter) {
        ASTARTE_LOG_ERR(
            "Expected interface '%s' data not received", expected_iter->interface->name);
        idata_unit_log(expected_iter);

        num += 1;
        expected_iter = idata_iter_next(&expected_data, expected_iter);
    }

    if (num == 0) {
        ASTARTE_LOG_DBG("No more expected data");

        if (unexpected_data_count > 0) {
            ASTARTE_LOG_DBG("Received %zu units of unexpected data", expected_data_count);
            shell_print(sh, "Received unexpected data");
        } else if (expected_data_count > 0) {
            ASTARTE_LOG_DBG("Received %zu units of data", expected_data_count);
            shell_print(sh, "All expected data received");
        } else {
            ASTARTE_LOG_WRN("No data was received but there was no expected data");
            shell_print(sh, "No data was expected and no data was received");
        }
    } else {
        shell_print(sh, "Missing %zu expected units of data", num);
    }

    // reset expected and unexpected data counter
    unexpected_data_count = 0;
    expected_data_count = 0;
    k_mutex_unlock(&expected_data_mutex);

    return 0;
}

// start send commands handlers
static int cmd_send_individual_handler(const struct shell *sh, size_t argc, char **argv)
{
    wait_for_connection();

    LOG_INF("Send individual command handler"); // NOLINT

    int return_code = 1;

    // null variables definition
    char *path = { 0 };
    e2e_byte_array individual_value = { 0 };
    astarte_individual_t individual = { 0 };

    // ignore the first parameter since it's the name of the command itself
    skip_parameter(&argv, &argc);
    const astarte_interface_t *interface = next_interface_parameter(
        &argv, &argc, interfaces, ARRAY_SIZE(interfaces));
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
    path = next_alloc_string_parameter(&argv, &argc);
    // In theory this can't fail since we are not performing checks... and the number of argument is
    // checked
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");
    individual_value = next_alloc_base64_parameter(&argv, &argc);
    CHECK_GOTO(individual_value.len == 0, cleanup, "Invalid individual parameter passed");
    e2e_timestamp_option_t timestamp = next_timestamp_parameter(&argv, &argc);

    CHECK_GOTO(parse_alloc_astarte_invividual(interface, path, &individual_value, &individual) != 0,
        cleanup, "Could not parse and allocate astarte individual");

    astarte_result_t res = { 0 };
    if (timestamp.present) {
        res = astarte_device_stream_individual(
            device_handle, interface->name, path, individual, &timestamp.value);
    } else {
        res = astarte_device_stream_individual(
            device_handle, interface->name, path, individual, NULL);
    }
    CHECK_ASTARTE_OK_GOTO(res, cleanup, "Failed to send individual to astarte");

    shell_print(sh, "Sent individual");
    return_code = 0;

cleanup:
    astarte_individual_destroy_deserialized(individual);
    free(individual_value.buf);
    free(path);

    return return_code;
}

static int cmd_send_object_handler(const struct shell *sh, size_t argc, char **argv)
{
    wait_for_connection();

    LOG_INF("Send object command handler"); // NOLINT

    int return_code = 1;

    // null variables definition
    char *path = { 0 };
    e2e_byte_array object_bytes = { 0 };
    astarte_object_entry_t *entries = { 0 };
    size_t entries_length = { 0 };

    // ignore the first parameter since it's the name of the command itself
    skip_parameter(&argv, &argc);
    const astarte_interface_t *interface = next_interface_parameter(
        &argv, &argc, interfaces, ARRAY_SIZE(interfaces));
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
    path = next_alloc_string_parameter(&argv, &argc);
    // In theory this can't fail since we are not performing checks and the number of argument is
    // checked
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");
    object_bytes = next_alloc_base64_parameter(&argv, &argc);
    CHECK_GOTO(object_bytes.len == 0, cleanup, "Invalid object parameter passed");
    e2e_timestamp_option_t timestamp = next_timestamp_parameter(&argv, &argc);

    CHECK_GOTO(
        parse_alloc_astarte_object(interface, path, &object_bytes, &entries, &entries_length) != 0,
        cleanup, "Could not parse and allocate astarte object entries");

    astarte_result_t res = { 0 };
    if (timestamp.present) {
        res = astarte_device_stream_aggregated(
            device_handle, interface->name, path, entries, entries_length, &timestamp.value);
    } else {
        res = astarte_device_stream_aggregated(
            device_handle, interface->name, path, entries, entries_length, NULL);
    }
    CHECK_ASTARTE_OK_GOTO(res, cleanup, "Failed to send object to astarte");

    shell_print(sh, "Sent object");
    return_code = 0;

cleanup:
    astarte_object_entries_destroy_deserialized(entries, entries_length);
    free(object_bytes.buf);
    free(path);

    return return_code;
}

static int cmd_send_property_set_handler(const struct shell *sh, size_t argc, char **argv)
{
    wait_for_connection();

    int return_code = 1;

    // null variables definition
    char *path = { 0 };
    e2e_byte_array property_value = { 0 };
    astarte_individual_t individual = { 0 };

    LOG_INF("Set property command handler"); // NOLINT

    // ignore the first parameter since it's the name of the command itself
    skip_parameter(&argv, &argc);
    const astarte_interface_t *interface = next_interface_parameter(
        &argv, &argc, interfaces, ARRAY_SIZE(interfaces));
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
    path = next_alloc_string_parameter(&argv, &argc);
    // In theory this can't fail since we are not performing checks... and the number of argument is
    // checked
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");
    property_value = next_alloc_base64_parameter(&argv, &argc);
    CHECK_GOTO(property_value.len == 0, cleanup, "Invalid individual parameter passed");

    CHECK_GOTO(parse_alloc_astarte_invividual(interface, path, &property_value, &individual) != 0,
        cleanup, "Could not parse and allocate individual");

    astarte_result_t res
        = astarte_device_set_property(device_handle, interface->name, path, individual);
    CHECK_ASTARTE_OK_GOTO(res, cleanup, "Failed to send set property to astarte");

    shell_print(sh, "Property set");
    return_code = 0;

cleanup:
    astarte_individual_destroy_deserialized(individual);
    free(property_value.buf);
    free(path);

    return return_code;
}

static int cmd_send_property_unset_handler(const struct shell *sh, size_t argc, char **argv)
{
    wait_for_connection();

    LOG_INF("Unset property command handler"); // NOLINT

    int return_code = 1;

    // null variables definition
    char *path = { 0 };

    // ignore the first parameter since it's the name of the command itself
    skip_parameter(&argv, &argc);
    const astarte_interface_t *interface = next_interface_parameter(
        &argv, &argc, interfaces, ARRAY_SIZE(interfaces));
    CHECK_GOTO(!interface, cleanup, "Invalid interface name passed");
    path = next_alloc_string_parameter(&argv, &argc);
    // In theory this can't fail since we are not performing checks... and the number of argument is
    // checked
    CHECK_GOTO(!path, cleanup, "Invalid path parameter passed");

    astarte_result_t res = astarte_device_unset_property(device_handle, interface->name, path);
    CHECK_ASTARTE_OK_GOTO(res, cleanup, "Failed to send set property to astarte");

    shell_print(sh, "Property unset");
    return_code = 0;

cleanup:
    free(path);

    return return_code;
}

static int cmd_disconnect(const struct shell *sh, size_t argc, char **argv)
{
    wait_for_connection();

    LOG_INF("Disconnect command handler"); // NOLINT

    LOG_INF("Stopping and joining the astarte device polling thread."); // NOLINT
    atomic_set_bit(&device_thread_flags, THREAD_TERMINATION_FLAG);
    CHECK_HALT(k_thread_join(&device_thread_data, K_FOREVER) != 0,
        "Failed in waiting for the Astarte thread to terminate.");

    LOG_INF("Destroing Astarte device and freeing resources."); // NOLINT
    CHECK_ASTARTE_OK_HALT(astarte_device_destroy(device_handle, K_SECONDS(10), false),
        "Astarte device destruction failure.");

    wait_for_disconnection();

    shell_print(sh, "Disconnected, closing shell...");
    shell_stop(sh);

    return 0;
}

static void device_thread_entry_point(void *device_handle, void *unused1, void *unused2)
{
    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);

    astarte_device_handle_t device = (astarte_device_handle_t) device_handle;

    LOG_INF("Starting e2e device thread."); // NOLINT

    CHECK_ASTARTE_OK_HALT(astarte_device_connect(device), "Astarte device connection failure.");

    while (!atomic_test_bit(&device_thread_flags, THREAD_TERMINATION_FLAG)) {
        k_timepoint_t timepoint = sys_timepoint_calc(K_MSEC(CONFIG_DEVICE_POLL_PERIOD_MS));

        astarte_result_t res = astarte_device_poll(device);
        CHECK_HALT(res != ASTARTE_RESULT_TIMEOUT && res != ASTARTE_RESULT_OK,
            "Astarte device poll failure.");

        k_sleep(sys_timepoint_timeout(timepoint));
    }

    LOG_INF("Exiting from the polling thread."); // NOLINT
}

static void connection_callback(astarte_device_connection_event_t event)
{
    (void) event;
    LOG_INF("Astarte device connected"); // NOLINT
    atomic_set_bit(&device_thread_flags, DEVICE_CONNECTED_FLAG);
}

static void disconnection_callback(astarte_device_disconnection_event_t event)
{
    (void) event;
    LOG_INF("Astarte device disconnected"); // NOLINT
    atomic_clear_bit(&device_thread_flags, DEVICE_CONNECTED_FLAG);
}

static void wait_for_connection()
{
    while (!atomic_test_bit(&device_thread_flags, DEVICE_CONNECTED_FLAG)) {
        k_sleep(K_MSEC(MAIN_THREAD_SLEEP_MS));
    }
}

static void wait_for_disconnection()
{
    while (atomic_test_bit(&device_thread_flags, DEVICE_CONNECTED_FLAG)) {
        k_sleep(K_MSEC(MAIN_THREAD_SLEEP_MS));
    }
}

// this also implicitly checks that the passed path is valid
static int parse_alloc_astarte_invividual(const astarte_interface_t *interface, char *path,
    e2e_byte_array *buf, astarte_individual_t *out_individual)
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

    CHECK_ASTARTE_OK_RET_1(astarte_individual_deserialize(v_elem, mapping->type, out_individual),
        "Couldn't deserialize received binary data into object entries");

    return 0;
}

// this also implicitly checks that the passed path is valid
static int parse_alloc_astarte_object(const astarte_interface_t *interface, char *path,
    e2e_byte_array *buf, astarte_object_entry_t **out_entries, size_t *out_entries_length)
{
    // Since the function expects a bson element we need to receive a "v" value like it would be
    // sent to astarte
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
