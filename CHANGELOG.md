<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.1.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

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