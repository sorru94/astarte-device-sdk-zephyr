<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

## Samples organization

All the samples contained in this folder share some source code and configuration settings.
The common components for all the samples can be found in the `common` folder.

Some of the common settings are:
- Board specific overlays and configurations. Contained in the `common/boards` folder.
- Common source code for generic Ethernet/Wifi connectivity, TLS settings plus some utilities for
  generation of standard datasets. Contained in the `common/include` and `common/src` folders.
- Common Astarte interfaces shared by the samples. These interfaces have been designed to be generic
  in order for the samples to demonstrate as much functionality as possible.
  The interfaces are definedi in JSON filed sontained in the `common/interfaces` folder.
  In addition to the JSON version of the interfaces, an auto-generated version of the same interfaces
  is contained in the `generated_interfaces` header/source files. Those files have been generated
  running the `west generate-interfaces` command and should not be modified manually.
- A generic `Kconfig` file defines some configuration flags used by all the samples.
- A `prj.conf` file contains configuration settings common for all the samples.

### Common samples behaviour

All the samples behave in a similar manner.
Each sample contains two threads:
- A master application thread that will handle Ethernet/Wifi reconnection and trasmit data to
  Astarte if needed.
- A secondary thread that will manage the Astarte device and handle reception of data from Astarte.

The two threads are configured to be at the same priority.

Each sample will connect to Astarte and remain connected for a configurable amount of time.
Furthermore, samples intended to demonstrate a specific interface type such as datastream or
property will transmit a predefined set of data after a configurable intervall of time.

After the operational time of the device has concluded, the device will disconnect and the sample
will terminate.

## Samples configuration

### Configuration for demonstration non-TLS capable Astarte

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
CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME="<REALM_NAME>"

CONFIG_CREDENTIAL_SECRET="<CREDENTIAL_SECRET>"
```
Where `<DEVICE_ID>` is the device ID of the device you would like to use in the sample, `<HOSTNAME>`
is the hostname for your Astarte instance, `<REALM_NAME>` is the name of your testing realm and
`<CREDENTIAL_SECRET>` is the credential secret obtained through the manual registration.

### Configuration for fully TLS capable Astarte

This option assumes you are using a fully deployed Astarte instance with valid certificates from
an official certificate authority. All the samples assume the root CA certificate for the MQTT
broker to be the same as the root CA certificate for all HTTPs APIs.

```conf
CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID="<DEVICE_ID>"
CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME="<HOSTNAME>"
CONFIG_ASTARTE_DEVICE_SDK_HTTPS_CA_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_MQTTS_CA_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG=2
CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME="<REALM_NAME>"

CONFIG_CREDENTIAL_SECRET="<CREDENTIAL_SECRET>"
```
Where `<DEVICE_ID>` is the device ID of the device you would like to use in the sample, `<HOSTNAME>`
is the hostname for your Astarte instance, `<REALM_NAME>` is the name of your testing realm and
`<CREDENTIAL_SECRET>` is the credential secret obtained through the manual registration.

In addition, the file `ca_certificates.h` should be modified, placing in the `ca_certificate_root`
array a valid CA certificate in the PEM format.

### Configuration of secrets in ignored files

Some of the data configured in the `prj.conf` files could be private.
Data like wifi passwords or even the astarte credential secret could be configured in a `private.conf` file.
You can have a `private.conf` file for every of the [samples](https://github.com/secomind/astarte-device-sdk-zephyr/tree/master/samples).
You can also have a generic `private.conf` defined inside the [common directory](https://github.com/secomind/astarte-device-sdk-zephyr/tree/master/samples/common)
this configuration will be applied to every sample and it's useful if for you want to the wifi configuration applied to all the examples.

To configure the wifi in a file that is ignored by git you can add the following to `samples/common/private.conf`
```conf
# WiFi credentials
CONFIG_WIFI_SSID=""
CONFIG_WIFI_PASSWORD=""
```
