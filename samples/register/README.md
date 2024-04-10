<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Registration sample

This sample shows how to register a new device to a remote Astarte instance using the pairing
APIs provided in this library.
It furthermore shows how connection is established and how callbacks can be used to verify
the connection status.

## Required configuration

This example needs to be configured to work with a testing/demonstration Astarte instance or
a fully deployed Astarte instance.
The configuration can be added in the `prj.conf` file of this example.

All usecases require setting the WiFi SSID and password to valid values.

### Configuration for testing or demonstration

This option assumes you are using this example with an Astarte instance similar to the
one explained in the
[Astarte in 5 minutes](https://docs.astarte-platform.org/astarte/latest/010-astarte_in_5_minutes.html)
tutorial.

The following entries should be modified in the `proj.conf` file.
```conf
CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID="<DEVICE_ID>"
CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME="<HOSTNAME>"
CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP=y
CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT=y
CONFIG_ASTARTE_DEVICE_SDK_PAIRING_JWT="<JWT>"
CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME="<REALM_NAME>"
```
Where `<DEVICE_ID>` is the device ID of the new device you would like to register, `<HOSTNAME>` is
the hostname for your Astarte instance, `<JWT>` is a valid pairing JWT for the Astarte instance and
`<REALM_NAME>` is the name of your testing realm.

### Configuration for fully functional Astarte

This option assumes you are using a fully deployed Astarte instance with valid certificates from
an official certificate authority.

```conf
CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID="<DEVICE_ID>"
CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME="<HOSTNAME>"
CONFIG_ASTARTE_DEVICE_SDK_HTTPS_CA_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_MQTTS_CA_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG=2
CONFIG_ASTARTE_DEVICE_SDK_PAIRING_JWT="<JWT>"
CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME="<REALM_NAME>"
```
Where `<DEVICE_ID>` is the device ID of the new device you would like to register, `<HOSTNAME>` is
the hostname for your Astarte instance, `<JWT>` is a valid pairing JWT for the Astarte instance and
`<REALM_NAME>` is the name of your testing realm.

In addition, the file `ca_certificates.h` should be modified, placing in the `ca_certificate_root`
array a valid CA certificate in the PEM format.
