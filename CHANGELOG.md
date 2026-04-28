<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.1.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- Added `esp_wrover_kit` board support to the sample application.

### Changed
- The `astarte_data_from_*` functions that take the input data as pointers have had their signature modified. The new signature accepts constant pointers in place of standard pointers. This makes it clearer that the functions do not modify the original user supplied data.
- The `astarte_data_to_*` functions that return the extracted data as a pointer have had their signature modified. The new signature uses constant pointers for the output parameter. This makes it clearer that the output data should not be directly modified by the user.
- Prefixed all included west extensions with `astarte-` to prevent naming conflicts with user-created extensions.
- Renamed the `generate-interfaces` west extension command to `astarte-interfaces`.
- Reduced the default static HTTP receive buffer size from 4096 to 1024 bytes. This remains configurable via `ASTARTE_DEVICE_SDK_ADVANCED_HTTP_RCV_BUFFER_SIZE`.
- Refactored internal MbedTLS usage to leverage the [PSA crypto API](https://docs.zephyrproject.org/latest/services/crypto/psa_crypto.html). This introduces no public API changes but requires the PSA Crypto API to be enabled in the project's Kconfig options.
- Changed the default value of `CONFIG_GET_CONFIG_FROM_FLASH` to `n` for the `frdm_rw612` board in the sample application.

### Removed
- Support for Zephyr versions older than 4.3.x, including 3.7.x (LTS3).

### Fixed
- Corrected the storage partition size for the `frdm_rw612` board sample.
- Optimized persistent storage write operations to significantly reduce overhead, resolving associated write failures.
- Removed a conflicting keyword from the library, restoring compatibility for C++ compilers.

## [0.9.0] - 2025-07-22
### Added
- Support for Zephyr 4.2.0.
- Support for the [NXP frdm RW612](https://docs.zephyrproject.org/latest/boards/nxp/frdm_rw612/doc/index.html) board in the examples.

## [0.8.0] - 2025-03-19
### Added
- Support for Zephyr 4.1.0.
- Function `astarte_device_force_disconnect` forces the device disconnection from an Astarte instance discarding any pending QoS 1/2 messages.

### Changed
- The `astarte_device_disconnect` function waits for all pending QoS 1/2 messages to be correctly processed with a configurable a timeout.
- For coherence with other SDKs renamed the object `astarte_individual_t` to `astarte_data_t`.
- For coherence with other SDKs renamed the functions `astarte_device_stream_individual` and `astarte_device_stream_aggregated` to `astarte_device_send_individual` and `astarte_device_send_object`.

## [0.7.2] - 2024-10-23
### Changed
- All the library samples have been merged into a single sample called `astarte_app`.

### Removed
- Dependency to `CONFIG_SYS_HASH_FUNC32` which is not required.

### Fixed
- Memory leaks in kv_storage driver and device creation process.
- Destroy function properly frees all cached MQTT messages.

## [0.7.1] - 2024-09-11
### Fixed
- Connection callback is correctly called also when session present is true.

## [0.7.0] - 2024-08-05
### Added
- Support for Zephyr 3.7.
- Support for MQTT sessions. The device will retain MQTT session information in between connections.
- Device ID generation utilities. The functions `astarte_device_id_generate_random` and `astarte_device_id_generate_deterministic` can be added to generate a valid device ID to connect a device to Astarte.
- Method `astarte_device_get_property`. This method can be used to read values of properties stored in persistent storage.
- Interface generation target that can be enabled by following [build time interfaces definition generation] (README.md#build-time-interface-definitions-generation).

### Changed
- Dependencies for the Astarte device SDK library are specified using `depends on` instead of `select`.
- `ASTARTE_PAIRING_DEVICE_ID_LEN` has been renamed to `ASTARTE_DEVICE_ID_LEN`.

### Removed
- Support for Zephyr 3.6 in the code samples and tests.
- The UUID utilities. As their only purpose was to generate device IDs and have been replaced by the `device_id.h` utilities.
- The configuration option `CONFIG_ASTARTE_DEVICE_SDK_GENERATE_DEVICE_ID`. Generating a device ID should be performed using the `device_id.h` utilities that are always available.

## [0.6.1] - 2024-06-28
### Fixed
- Device state is changed to CONNECTED or DISCONNECTED before calling the appropriate callback.

## [0.6.0] - 2024-06-11
### Added
- Device ID as a field in the `astarte_device_config_t` and a parameter of the `astarte_pairing_register_device` function. This makes it possible to specify a device ID at runtime.
- Utilities functions to convert an UUID to a base64 and base64 url and filename safe string.
- Preprocessor define `ASTARTE_PAIRING_DEVICE_ID_LEN` representing the fixed length of a device ID string.

### Changed
- Renamed the `astarte_value_t` type to `astarte_individual_t` to better reflect its content.
- Renamed the `astarte_value_pair_t` type to `astarte_object_entry_t` to better reflect its content.
- Renamed the `mqtt_connected_timeout_ms` field to `mqtt_poll_timeout_ms` in the `astarte_device_config_t` struct.

### Removed
- Kconfig option `CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID`. Device ID should be defined using the `astarte_device_config_t` struct during device initialization.
- The `session_present` flag from the struct `astarte_device_connection_event_t`.

## [0.5.0] - 2024-04-10
### Added
- Method to stream device owned object aggregated datastreams.
- Methods to set and unset device owned properties.
- Mapping type `astarte_mapping_t`. To be used to store Astarte mapping definitions.
- West command to generate interfaces definitions from `.json` files.
- Macro `ASTARTE_UUID_STR_LEN` specifying the length in characters of the string representation of an UUID.

### Fixed
- Incorrect hardcoding of MQTT client ID. The client ID is now unique for each Astarte device in a single Astarte instance.

### Changed
- The `astarte_device_data_cbk_t` callback has been replaced by three separate callbacks. One for set properties, one for unset properties, one for datastreams individual, and one for datastreams objects. Each callback can be enabled independently from the others using the `astarte_device_config_t` struct.
- All the data callback events now contain a generic `astarte_device_data_event_t`, placed inside a callback-specific event containing additional information. The unset property callback is an exception as it gives the user an `astarte_device_data_event_t` directly.
- The received data is provided to the user in the form of an `astarte_value_t` or an array of `astarte_value_pair_t` instead of a BSON document.
- The transmission of data to Astarte has been moved outside of the reception callbacks and into a separate thread.

### Removed
- BSON serializer utilities from `bson_serializer.h`.

## [0.5.0-alpha] - 2024-03-08
### Added
- Initial SDK release, supports pairing and publish/receive of basic Astarte types.
