/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/arpa/inet.h>

#if (!defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP)                                  \
    || !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT))
#include "ca_certificates.h"
#include <zephyr/net/tls_credentials.h>
#endif

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL); // NOLINT

#include <astarte_device_sdk/device.h>
#include <astarte_device_sdk/individual.h>
#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/mapping.h>
#include <astarte_device_sdk/pairing.h>

#if defined(CONFIG_WIFI)
#include "wifi.h"
#else
#include "eth.h"
#endif
#include "utils.h"

#include "generated_interfaces.h"

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

BUILD_ASSERT(sizeof(CONFIG_DEVICE_ID) == ASTARTE_DEVICE_ID_LEN + 1,
    "Missing device ID in datastreams example");
BUILD_ASSERT(sizeof(CONFIG_CREDENTIAL_SECRET) == ASTARTE_PAIRING_CRED_SECR_LEN + 1,
    "Missing credential secret in datastreams example");

/************************************************
 * Constants, static variables and defines
 ***********************************************/

#define MAIN_THREAD_SLEEP_MS 500

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
K_MSGQ_DEFINE(device_msgq, sizeof(astarte_device_handle_t), 6, 1);

#define DEVICE_MANAGEMENT_THREAD_FLAGS_TERMINATION 1U
static atomic_t device_management_thread_flags;

K_THREAD_STACK_DEFINE(device_instantiation_thread_stack_area, CONFIG_DEVICE_RX_THREAD_STACK_SIZE);
static struct k_thread device_instantiation_thread_data;
K_THREAD_STACK_DEFINE(device_connection_thread_stack_area, CONFIG_DEVICE_RX_THREAD_STACK_SIZE);
static struct k_thread device_connection_thread_data;
K_THREAD_STACK_DEFINE(device_polling_thread_stack_area, CONFIG_DEVICE_RX_THREAD_STACK_SIZE);
static struct k_thread device_polling_thread_data;
K_THREAD_STACK_DEFINE(device_disconnection_thread_stack_area, CONFIG_DEVICE_RX_THREAD_STACK_SIZE);
static struct k_thread device_disconnection_thread_data;
K_THREAD_STACK_DEFINE(device_destruction_thread_stack_area, CONFIG_DEVICE_RX_THREAD_STACK_SIZE);
static struct k_thread device_destruction_thread_data;

K_THREAD_STACK_DEFINE(device_tx_thread_stack_area, CONFIG_DEVICE_TX_THREAD_STACK_SIZE);
static struct k_thread device_tx_thread_data;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

/************************************************
 * Static functions declaration
 ***********************************************/

/**
 * @brief Entry point for the Astarte device reception thread.
 *
 * @param arg1 Unused argument.
 * @param arg2 Unused parameter.
 * @param arg3 Unused parameter.
 */
static void device_instantiation_thread_entry_point(void *arg1, void *arg2, void *arg3);
static void device_connection_thread_entry_point(void *arg1, void *arg2, void *arg3);
static void device_polling_thread_entry_point(void *arg1, void *arg2, void *arg3);
static void device_disconnection_thread_entry_point(void *arg1, void *arg2, void *arg3);
static void device_destruction_thread_entry_point(void *arg1, void *arg2, void *arg3);
/**
 * @brief Entry point for the Astarte device transmission thread.
 *
 * @param arg1 Unused argument.
 * @param arg2 Unused argument.
 * @param arg3 Unused argument.
 */
static void device_tx_thread_entry_point(void *arg1, void *arg2, void *arg3);
/**
 * @brief Callback handler for Astarte connection events.
 *
 * @param event Astarte device connection event.
 */
static void connection_callback(astarte_device_connection_event_t event);
/**
 * @brief Callback handler for Astarte disconnection events.
 *
 * @param event Astarte device disconnection event.
 */
static void disconnection_callback(astarte_device_disconnection_event_t event);
/**
 * @brief Callback handler for Astarte datastream individual event.
 *
 * @param event Astarte device datastream individual event.
 */
static void datastream_individual_callback(astarte_device_datastream_individual_event_t event);

/************************************************
 * Global functions definition
 ***********************************************/

#define PTR_STR_MAXLEN (sizeof(void *) * 2 + 2)
void thread_foreach_cbk(const struct k_thread *thread, void *user_data)
{
    int err = 0;
    k_tid_t thread_id = (k_tid_t) thread;
    const char *name = k_thread_name_get(thread_id);
    char hexname[PTR_STR_MAXLEN + 1];
    if (!name || name[0] == '\0') {
        name = hexname;
        snprintk(hexname, sizeof(hexname), "%p", (void *) thread);
    }

    size_t unused = 0U;
    err = k_thread_stack_space_get(thread, &unused);
    if (err) {
        LOG_ERR("Could not read the unused space for thread %s", name); // NOLINT
        unused = 0;
    }

    if ((strncmp(name, "INSTANTIATION_THREAD", strlen("INSTANTIATION_THREAD")) == 0)
        || (strncmp(name, "CONNECTION_THREAD", strlen("CONNECTION_THREAD")) == 0)
        || (strncmp(name, "POLLING_THREAD", strlen("POLLING_THREAD")) == 0)
        || (strncmp(name, "DISCONNECTION_THREAD", strlen("DISCONNECTION_THREAD")) == 0)
        || (strncmp(name, "DESTRUCTION_THREAD", strlen("DESTRUCTION_THREAD")) == 0)
        || (strncmp(name, "TX_THREAD", strlen("TX_THREAD")) == 0)) {
        LOG_WRN("Thread %s of stack size: %u and delta: %u", name, thread->stack_info.size,
            thread->stack_info.delta);
        LOG_WRN("Stack unused: %u Bytes, stack used %u (%f)", unused,
            thread->stack_info.size - unused,
            100.0 * (double) (thread->stack_info.size - unused) / (double) thread->stack_info.size);
    }
}

int main(void)
{
    LOG_INF("Astarte device sample"); // NOLINT
    LOG_INF("Board: %s", CONFIG_BOARD); // NOLINT
    k_thread_name_set(NULL, "MAIN_THREAD");

    // Initialize WiFi/Ethernet driver
#if defined(CONFIG_WIFI)
    LOG_INF("Initializing WiFi driver."); // NOLINT
    wifi_init();
#else
    LOG_INF("Initializing Ethernet driver."); // NOLINT
    if (eth_connect() != 0) {
        LOG_ERR("Connectivity intialization failed!"); // NOLINT
        return -1;
    }
#endif

    // Add TLS certificate if required
#if (!defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP)                                  \
    || !defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT))
    tls_credential_add(CONFIG_ASTARTE_DEVICE_SDK_HTTPS_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
        ca_certificate_root, sizeof(ca_certificate_root));
#endif

    // Spawn a new thread for the Astarte device
    k_tid_t instantiation_tid
        = k_thread_create(&device_instantiation_thread_data, device_instantiation_thread_stack_area,
            K_THREAD_STACK_SIZEOF(device_instantiation_thread_stack_area),
            device_instantiation_thread_entry_point, NULL, NULL, NULL,
            CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(instantiation_tid, "INSTANTIATION_THREAD");

    // Wait for a predefined operational time.
    k_timepoint_t disconnect_timepoint
        = sys_timepoint_calc(K_SECONDS(CONFIG_DEVICE_OPERATIONAL_TIME_SECONDS));
    while (!K_TIMEOUT_EQ(sys_timepoint_timeout(disconnect_timepoint), K_NO_WAIT)) {
        // Ensure the connectivity is still present
#if defined(CONFIG_WIFI)
        wifi_poll();
#else
        eth_poll();
#endif
        k_sleep(K_MSEC(MAIN_THREAD_SLEEP_MS));
    }

    // Signal to the Astarte thread that is should terminate.
    atomic_set_bit(&device_management_thread_flags, DEVICE_MANAGEMENT_THREAD_FLAGS_TERMINATION);

    // Wait for the Astarte thread to terminate.
    if (k_thread_join(&device_instantiation_thread_data, K_FOREVER) != 0) {
        LOG_ERR("Failed in waiting for the Astarte thread to terminate."); // NOLINT
    }

    LOG_INF("Astarte device sample finished."); // NOLINT
    k_sleep(K_MSEC(MSEC_PER_SEC));

    return 0;
}

/************************************************
 * Static functions definitions
 ***********************************************/

static void device_instantiation_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    k_thread_foreach_unlocked(thread_foreach_cbk, NULL);

    astarte_result_t res = ASTARTE_RESULT_OK;

    // Create a new instance of an Astarte device
    char device_id[ASTARTE_DEVICE_ID_LEN + 1] = CONFIG_DEVICE_ID;
    char cred_secr[ASTARTE_PAIRING_CRED_SECR_LEN + 1] = CONFIG_CREDENTIAL_SECRET;

    const astarte_interface_t *interfaces[]
        = { &org_astarteplatform_zephyr_examples_DeviceDatastream,
              &org_astarteplatform_zephyr_examples_ServerDatastream };

    astarte_device_config_t device_config = { 0 };
    device_config.http_timeout_ms = CONFIG_HTTP_TIMEOUT_MS;
    device_config.mqtt_connection_timeout_ms = CONFIG_MQTT_CONNECTION_TIMEOUT_MS;
    device_config.mqtt_poll_timeout_ms = CONFIG_MQTT_POLL_TIMEOUT_MS;
    device_config.connection_cbk = connection_callback;
    device_config.disconnection_cbk = disconnection_callback;
    device_config.datastream_individual_cbk = datastream_individual_callback;
    device_config.interfaces = interfaces;
    device_config.interfaces_size = ARRAY_SIZE(interfaces);
    memcpy(device_config.device_id, device_id, sizeof(device_id));
    memcpy(device_config.cred_secr, cred_secr, sizeof(cred_secr));

    astarte_device_handle_t device = NULL;
    res = astarte_device_new(&device_config, &device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device creation failure."); // NOLINT
        return;
    }

    k_thread_foreach_unlocked(thread_foreach_cbk, NULL);

    // Add the message to the queue for the connect thread
    k_msgq_put(&device_msgq, (void *) &device, K_FOREVER);
    k_tid_t connection_tid
        = k_thread_create(&device_connection_thread_data, device_connection_thread_stack_area,
            K_THREAD_STACK_SIZEOF(device_connection_thread_stack_area),
            device_connection_thread_entry_point, NULL, NULL, NULL,
            CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(connection_tid, "CONNECTION_THREAD");
}
static void device_connection_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    astarte_device_handle_t device = NULL;
    k_msgq_get(&device_msgq, (void *) &device, K_FOREVER);

    k_thread_foreach_unlocked(thread_foreach_cbk, NULL);

    astarte_result_t res = ASTARTE_RESULT_OK;

    res = astarte_device_connect(device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device connection failure."); // NOLINT
        return;
    }

    k_thread_foreach_unlocked(thread_foreach_cbk, NULL);

    // Add the message to the queue for the polling and transmission thread
    k_msgq_put(&device_msgq, (void *) &device, K_FOREVER);
    k_msgq_put(&device_msgq, (void *) &device, K_FOREVER);
    k_tid_t polling_tid
        = k_thread_create(&device_polling_thread_data, device_polling_thread_stack_area,
            K_THREAD_STACK_SIZEOF(device_polling_thread_stack_area),
            device_polling_thread_entry_point, NULL, NULL, NULL,
            CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(polling_tid, "POLLING_THREAD");
    k_tid_t tx_tid = k_thread_create(&device_tx_thread_data, device_tx_thread_stack_area,
        K_THREAD_STACK_SIZEOF(device_tx_thread_stack_area), device_tx_thread_entry_point, NULL,
        NULL, NULL, CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(tx_tid, "TX_THREAD");
}
static void device_polling_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    astarte_device_handle_t device = NULL;
    k_msgq_get(&device_msgq, (void *) &device, K_FOREVER);

    k_thread_foreach_unlocked(thread_foreach_cbk, NULL);

    astarte_result_t res = ASTARTE_RESULT_OK;

    while (!atomic_test_bit(
        &device_management_thread_flags, DEVICE_MANAGEMENT_THREAD_FLAGS_TERMINATION)) {
        k_timepoint_t timepoint = sys_timepoint_calc(K_MSEC(CONFIG_DEVICE_POLL_PERIOD_MS));

        res = astarte_device_poll(device);
        if (res != ASTARTE_RESULT_OK) {
            LOG_ERR("Astarte device poll failure."); // NOLINT
            return;
        }

        k_sleep(sys_timepoint_timeout(timepoint));
    }

    k_thread_foreach_unlocked(thread_foreach_cbk, NULL);

    // Add the message to the queue for the disconnection thread
    k_msgq_put(&device_msgq, (void *) &device, K_FOREVER);
    k_tid_t disconnection_tid
        = k_thread_create(&device_disconnection_thread_data, device_disconnection_thread_stack_area,
            K_THREAD_STACK_SIZEOF(device_disconnection_thread_stack_area),
            device_disconnection_thread_entry_point, NULL, NULL, NULL,
            CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(disconnection_tid, "DISCONNECTION_THREAD");
}
static void device_disconnection_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    astarte_device_handle_t device = NULL;
    k_msgq_get(&device_msgq, (void *) &device, K_FOREVER);

    k_thread_foreach_unlocked(thread_foreach_cbk, NULL);

    astarte_result_t res = ASTARTE_RESULT_OK;

    LOG_INF("End of loop, disconnection imminent."); // NOLINT
    res = astarte_device_disconnect(device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device disconnection failure."); // NOLINT
        return;
    }

    k_thread_foreach_unlocked(thread_foreach_cbk, NULL);

    // Add the message to the queue for the destruction thread
    k_msgq_put(&device_msgq, (void *) &device, K_FOREVER);
    k_tid_t destruction_tid
        = k_thread_create(&device_destruction_thread_data, device_destruction_thread_stack_area,
            K_THREAD_STACK_SIZEOF(device_destruction_thread_stack_area),
            device_destruction_thread_entry_point, NULL, NULL, NULL,
            CONFIG_DEVICE_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(destruction_tid, "DESTRUCTION_THREAD");
}
static void device_destruction_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    astarte_device_handle_t device = NULL;
    k_msgq_get(&device_msgq, (void *) &device, K_FOREVER);

    k_thread_foreach_unlocked(thread_foreach_cbk, NULL);

    astarte_result_t res = ASTARTE_RESULT_OK;

    LOG_INF("Astarte device will now be destroyed."); // NOLINT
    res = astarte_device_destroy(device);
    if (res != ASTARTE_RESULT_OK) {
        LOG_ERR("Astarte device destroy failure."); // NOLINT
        return;
    }

    k_thread_foreach_unlocked(thread_foreach_cbk, NULL);
}

static void device_tx_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    astarte_device_handle_t device = NULL;
    k_msgq_get(&device_msgq, (void *) &device, K_FOREVER);

    k_thread_foreach_unlocked(thread_foreach_cbk, NULL);

    astarte_result_t res = ASTARTE_RESULT_OK;

    k_sleep(K_SECONDS(CONFIG_DEVICE_TRANSMISSION_DELAY_SECONDS));

    LOG_INF("Transmitting some data using the Astarte device."); // NOLINT

    const char *interface_name = org_astarteplatform_zephyr_examples_DeviceDatastream.name;

    astarte_individual_t individuals[]
        = { astarte_individual_from_binaryblob(
                (void *) utils_binary_blob_data, ARRAY_SIZE(utils_binary_blob_data)),
              astarte_individual_from_binaryblob_array((const void **) utils_binary_blobs_data,
                  (size_t *) utils_binary_blobs_sizes_data, ARRAY_SIZE(utils_binary_blobs_data)),
              astarte_individual_from_boolean(utils_boolean_data),
              astarte_individual_from_boolean_array(
                  (bool *) utils_boolean_array_data, ARRAY_SIZE(utils_boolean_array_data)),
              astarte_individual_from_datetime(utils_unix_time_data),
              astarte_individual_from_datetime_array(
                  (int64_t *) utils_unix_time_array_data, ARRAY_SIZE(utils_unix_time_array_data)),
              astarte_individual_from_double(utils_double_data),
              astarte_individual_from_double_array(
                  (double *) utils_double_array_data, ARRAY_SIZE(utils_double_array_data)),
              astarte_individual_from_integer(utils_integer_data),
              astarte_individual_from_integer_array(
                  (int32_t *) utils_integer_array_data, ARRAY_SIZE(utils_integer_array_data)),
              astarte_individual_from_longinteger(utils_longinteger_data),
              astarte_individual_from_longinteger_array((int64_t *) utils_longinteger_array_data,
                  ARRAY_SIZE(utils_longinteger_array_data)),
              astarte_individual_from_string(utils_string_data),
              astarte_individual_from_string_array(
                  (const char **) utils_string_array_data, ARRAY_SIZE(utils_string_array_data)) };

    const char *paths[] = {
        "/binaryblob_endpoint",
        "/binaryblobarray_endpoint",
        "/boolean_endpoint",
        "/booleanarray_endpoint",
        "/datetime_endpoint",
        "/datetimearray_endpoint",
        "/double_endpoint",
        "/doublearray_endpoint",
        "/integer_endpoint",
        "/integerarray_endpoint",
        "/longinteger_endpoint",
        "/longintegerarray_endpoint",
        "/string_endpoint",
        "/stringarray_endpoint",
    };
    const int64_t tms = 1714748755;

    for (size_t i = 0; i < MIN(ARRAY_SIZE(individuals), ARRAY_SIZE(paths)); i++) {
        LOG_INF("Stream on %s:", paths[i]); // NOLINT
        utils_log_astarte_individual(individuals[i]);
        res = astarte_device_stream_individual(
            device, interface_name, paths[i], individuals[i], &tms);
        if (res != ASTARTE_RESULT_OK) {
            LOG_INF("Astarte device transmission failure."); // NOLINT
        }
    }

    LOG_INF("Transmission completed."); // NOLINT

    k_thread_foreach_unlocked(thread_foreach_cbk, NULL);
}

static void connection_callback(astarte_device_connection_event_t event)
{
    ARG_UNUSED(event);
    LOG_INF("Astarte device connected."); // NOLINT
}

static void disconnection_callback(astarte_device_disconnection_event_t event)
{
    ARG_UNUSED(event);
    LOG_INF("Astarte device disconnected"); // NOLINT
}

static void datastream_individual_callback(astarte_device_datastream_individual_event_t event)
{
    const char *interface_name = event.data_event.interface_name;
    const char *path = event.data_event.path;
    astarte_individual_t individual = event.individual;

    LOG_INF("Datastream individual event, interface: %s, path: %s", interface_name, path); // NOLINT

    utils_log_astarte_individual(individual);
}
