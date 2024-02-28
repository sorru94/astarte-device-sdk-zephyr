<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

## Common sample configuration

The examples needs to be configured to work with a testing/demonstration Astarte instance or
a fully deployed Astarte instance.
We assume a device with a known device id has been manually registered in the Astarte instance.
The credential secret obtained through the registration should be added to the configuration.
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
CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_DISABLE_OR_IGNORE_TLS=y
CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME="<REALM_NAME>"

CONFIG_CREDENTIAL_SECRET="<CREDENTIAL_SECRET>"
```
Where `<DEVICE_ID>` is the device ID of the device you would like to use in the sample, `<HOSTNAME>`
is the hostname for your Astarte instance, `<REALM_NAME>` is the name of your testing realm and
`<CREDENTIAL_SECRET>` is the credential secret obtained through the manual registration.

### Configuration for fully functional Astarte

This option assumes you are using a fully deployed Astarte instance with valid certificates from
an official certificate authority.

```conf
CONFIG_ASTARTE_DEVICE_SDK_DEVICE_ID="<DEVICE_ID>"
CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME="<HOSTNAME>"
CONFIG_ASTARTE_DEVICE_SDK_CA_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG=2
CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME="<REALM_NAME>"

CONFIG_CREDENTIAL_SECRET="<CREDENTIAL_SECRET>"
```
Where `<DEVICE_ID>` is the device ID of the device you would like to use in the sample, `<HOSTNAME>`
is the hostname for your Astarte instance, `<REALM_NAME>` is the name of your testing realm and
`<CREDENTIAL_SECRET>` is the credential secret obtained through the manual registration.

In addition, the file `ca_certificates.h` should be modified, placing in the `ca_certificate_root`
array a valid CA certificate in the PEM format.

#### Build with TLS enables

To build the sample and enable tls you also need to pass the extra config file `../common/overlay-tls.conf`.
To do that you can run:
```sh
west build -b <BOARD> <SAMPLES_SUBDIRECTORY> -- -DEXTRA_CONF_FILE=../common/overlay-tls.conf
```

As an example you could run this command while in the root of the repo:
```sh
west build -b esp_wrover_kit samples/datastreams -- -DEXTRA_CONF_FILE=../common/overlay-tls.conf
```

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
