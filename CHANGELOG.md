<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.1.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [0.8.0] - Unreleased
### Added
- Support for Zephyr 4.0.0.
- Function `astarte_device_force_disconnect` forces the device disconnection from an Astarte
  instance discarding any pending QoS 1/2 messages.

### Changed
- The `astarte_device_disconnect` function waits for all pending QoS 1/2 messages to be correctly
  processed with a configurable a timeout.

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
- Device ID generation utilities. The functions `astarte_device_id_generate_random` and
  `astarte_device_id_generate_deterministic` can be added to generate a valid device ID to connect
  a device to Astarte.
- Method `astarte_device_get_property`. This method can be used to read values of properties
  stored in persistent storage.
- Interface generation target that can be enabled by following
  [build time interfaces definition generation]
  (README.md#build-time-interface-definitions-generation).

### Changed
- Dependencies for the Astarte device SDK library are specified using `depends on` instead of
  `select`.
- `ASTARTE_PAIRING_DEVICE_ID_LEN` has been renamed to `ASTARTE_DEVICE_ID_LEN`.

### Removed
- Support for Zephyr 3.6 in the code samples and tests.
- The UUID utilities. As their only purpose was to generate device IDs and have been replaced by
  the `device_id.h` utilities.
- The configuration option `CONFIG_ASTARTE_DEVICE_SDK_GENERATE_DEVICE_ID`. Generating a device ID
  should be performed using the `device_id.h` utilities that are always available.

## [0.6.1] - 2024-06-28
### Fixed
- Device state is changed to CONNECTED or DISCONNECTED before calling the appropriate callback.

## [0.6.0] - 2024-06-11
### Added
- Device ID as a field in the `astarte_device_config_t` and a parameter of the
  `astarte_pairing_register_device` function. This makes it possible to specify a device ID at
  runtime.
- Utilities functions to convert an UUID to a base64 and base64 url and filename safe string.
- Preprocessor define `ASTARTE_PAIRING_DEVICE_ID_LEN` representing the fixed length of a
  device ID string.

### Changed
- Renamed the `astarte_value_t` type to `astarte_individual_t` to better reflect its content.
- Renamed the `astarte_value_pair_t` type to `astarte_object_entry_t` to better reflect its content.
- Renamed the `mqtt_connected_timeout_ms` field to `mqtt_poll_timeout_ms` in the
  `astarte_device_config_t` struct.

### Removed
- Kconfig option `CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID`. Device ID should be defined using the
  `astarte_device_config_t` struct during device initialization.
- The `session_present` flag from the struct `astarte_device_connection_event_t`.

## [0.5.0] - 2024-04-10
### Added
- Method to stream device owned object aggregated datastreams.
- Methods to set and unset device owned properties.
- Mapping type `astarte_mapping_t`. To be used to store Astarte mapping definitions.
- West command to generate interfaces definitions from `.json` files.
- Macro `ASTARTE_UUID_STR_LEN` specifying the length in characters of the string representation of
  an UUID.

### Fixed
- Incorrect hardcoding of MQTT client ID. The client ID is now unique for each Astarte device in a
  single Astarte instance.

### Changed
- The `astarte_device_data_cbk_t` callback has been replaced by three separate callbacks.
  One for set properties, one for unset properties, one for datastreams individual, and one for
  datastreams objects.
  Each callback can be enabled independently from the others using the `astarte_device_config_t`
  struct.
- All the data callback events now contain a generic `astarte_device_data_event_t`, placed inside a
  callback-specific event containing additional information. The unset property callback is an
  exception as it gives the user an `astarte_device_data_event_t` directly.
- The received data is provided to the user in the form of an `astarte_value_t` or an array of
  `astarte_value_pair_t` instead of a BSON document.
- The transmission of data to Astarte has been moved outside of the reception callbacks and into a
  separate thread.

### Removed
- BSON serializer utilities from `bson_serializer.h`.

## [0.5.0-alpha] - 2024-03-08
### Added
- Initial SDK release, supports pairing and publish/receive of basic Astarte types.
