<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->
<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

## Samples organization

The sample app in this folder contains code to send all data types supported by astarte.
The sample folder also contains:
- Board specific overlays and configurations. Contained in the `astarte_app/boards` folder.

Some configuration and cmake utilites are stored in a shared `common` folder that does not contain
any valid zephyr application. This folder also contains common Astarte interfaces shared by the
samples and the tests. These interfaces have been designed to be generic in order for the
`astarte_app` sample to demonstrate as much functionality as possible.
The interfaces are defined in JSON files contained in the `common/interfaces` folder.
In addition to the JSON version of the interfaces, an auto-generated version of the same interfaces
is contained in the `generated_interfaces` header/source files. Those files have been generated
running the `west generate-interfaces` command and should not be modified manually.

### Sample behaviour

The sample contains three threads:
- A master application thread that will handle Ethernet/Wifi reconnection.
- A secondary thread that will manage the Astarte device and handle reception of data from Astarte.
- The third thread that sends data to astarte of the configured types.

The sample will connect to Astarte and remain connected for the time it takes to send the data.
Intervals between send of different types of data can be configured.
Additionally if no data type transmission is enabled the device will be connected for a specified
timeout to allow reception of test messages.
After the operational time of the device has concluded, the device will disconnect and the sample
will terminate.

Take a look a the [Kconfig](astarte_app/Kconfig) file or use menuconfig to test out different transmission
types and timeouts.

## Samples configuration

### Configuration for demonstration non-TLS capable Astarte

This option assumes you are using this example with an Astarte instance similar to the
one explained in the
[Astarte in 5 minutes](https://docs.astarte-platform.org/astarte/latest/010-astarte_in_5_minutes.html)
tutorial.

The following entries should be modified in the `proj.conf` file.
```conf
CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME="<HOSTNAME>"
CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP=y
CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT=y
CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME="<REALM_NAME>"

CONFIG_DEVICE_ID="<DEVICE_ID>"
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
CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME="<HOSTNAME>"
CONFIG_ASTARTE_DEVICE_SDK_HTTPS_CA_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_MQTTS_CA_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG=2
CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME="<REALM_NAME>"

CONFIG_DEVICE_ID="<DEVICE_ID>"
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
You can have a `private.conf` file in the [astarte_app](https://github.com/astarte-platform/astarte-device-sdk-zephyr/tree/master/samples/astarte_app).
This file is specified in the `.gitignore` and won't be committed.

To configure your secrets you can add the following to `samples/astarte_app/private.conf`
And properly replace `...` with your private data.
```conf
# Astarte secrets
CONFIG_ASTARTE_DEVICE_SDK_PAIRING_JWT="..."
# Sample secrets
CONFIG_CREDENTIAL_SECRET="..."
```

### Use native_sim with net-tools

The net-setup.sh script can setup an ethernet interface to the host. This net-setup.sh script will need to be run as a root user.

```
./net-setup.sh --config nat.conf
```

## Astarte app sample overview

This samle shows the basic operation an average user would perform using an astarte device:
- **How to opionally register the device**
- **How connection is established and how callbacks can be used to receive data or assess
the connection status.** The samples configures callback for each type of data that can be received from astarte.
Received values are logged using the logging module.
- **How different types of data can be sent trough the device.**

The trasmission is divided in four steps that can be enabled indipendently to test out the
device and server behaviour. Kconfig values control the compilation of these steps, here
are presented in the order in which they are executed:
- [registration](#Registration): Calls the sdk api to register a device given a pairing jwt token.
- [datastream individual send](#Individuals_reception): Transmit individual data to astarte.
- [datastream object send](#Objects_reception): Transmit object data to astarte.
- [properties set/unset individual](#Properties_reception): set and unset individual properties.

Each one of those steps has a corresponding configuration menu that lets you enable
and disable it's compilation. It is also possible to configure a timeout in second to
wait for before sending each type of data.
If no transmission is enabled it is possible to configure a timeout to wait for
before disconnecting the device and stopping the sample. This allows the device
to log messages received from astarte.
Run menuconfig to try out different values.

## Sample configurable steps

### Registration

This step can be enabled through the `Registration` menu and shows how to register a new device to
a remote Astarte instance using the pairing APIs provided in this library. The credential secret
created will be stored in the NVS.
A pairing JWT is needed and the device must have no credential secret already associated. You can
wipe it from the web interface of astarte if needed.

### Individuals transmission

This transmission step can be enabled trough the `Individual transmission` menu.
This step shows how transmission of individual datastreams can be performed using the provided APIs.
Reception of individuals is always enabled and to test it you can follow the next section.

### Reception of server individual data

By leveraging [`astartectl`](https://github.com/astarte-platform/astartectl) we can send test
data from Astarte to the device. Then observe the reception callback being triggered on the device.
We recommend storing the JWT in a shell variable to access it with ease.
```bash
TOKEN=<appengine_token>
```

The syntax to use the send-data subcommand is the following:
```sh
astartectl appengine \
    --token $TOKEN --realm-name "<REALM>" --astarte-url "<API_URL>" \
    devices send-data <DEVICE_ID> \
    org.astarteplatform.zephyr.examples.ServerDatastream \
    /boolean_endpoint true
```

Where:
- `<REALM>` is your realm name
- `<DEVICE_ID>` is the device ID to send the data to
- `<API_URL>` is the Astarte api endpoint

By changing the endpoint and payload in the astartectl command other data types can be
transmitted to the device.
```sh
astartectl appengine \
    --token $TOKEN --realm-name "<REALM>" --astarte-url "<API_URL>" \
    devices send-data <DEVICE_ID> \
    org.astarteplatform.zephyr.examples.ServerDatastream \
    /stringarray_endpoint '["hello", "world!"]'
```
Check out all the endpoints available in the interfaces provided with the sample.

### Objects transmission

This transmission step can be enabled trough the `Object transmission` menu.
This step shows how transmission of aggregate datastreams can be performed using the provided APIs.
Reception of objects is always enabled and to test it you can follow the next section.

### Reception of objects test data

By leveraging [`astartectl`](https://github.com/astarte-platform/astartectl) we can send test
data to the device.
We recommend storing the JWT in a shell variable to access it with ease.
```bash
TOKEN=<appengine_token>
```

The syntax to use the send-data subcommand is the following:
```sh
astartectl appengine \
    --token $TOKEN --realm-name "<REALM>" --astarte-url "<API_URL>" \
    devices send-data <DEVICE_ID> \
    org.astarteplatform.zephyr.examples.ServerAggregate \
    /sensor11 \
    '{"double_endpoint": 459.432, "integer_endpoint": 32, "boolean_endpoint": true, \
    "longinteger_endpoint": 45993543534, "string_endpoint": "some value", \
    "binaryblob_endpoint": "aGVsbG8gd29ybGQ=", "doublearray_endpoint": [23.45, 543.12, 33.1, 0.1], \
    "booleanarray_endpoint": [true, false], "stringarray_endpoint": ["hello", "world"], \
    "binaryblobarray_endpoint": ["aGVsbG8gd29ybGQ=", "aGVsbG8gd29ybGQ="]}'
```

Where:
- `<REALM>` is your realm name
- `<DEVICE_ID>` is the device ID to send the data to
- `<API_URL>` is the Astarte api endpoint

### Properties transmission

This transmission step can be enabled trough the `Property transmission` menu.
This step shows how to set and unset properties using the provided APIs.
Reception of properties set/unset is always enabled and to test it you can follow the next section.

### Reception of properties test data

By leveraging [`astartectl`](https://github.com/astarte-platform/astartectl) we can send test
data to the device.
We recommend storing the JWT in a shell variable to access it with ease.
```bash
TOKEN=<appengine_token>
```

The syntax to use the send-data subcommand is the following:
```sh
astartectl appengine \
    --token $TOKEN --realm-name "<REALM>" --astarte-url "<API_URL>" \
    devices send-data <DEVICE_ID> \
    org.astarteplatform.zephyr.examples.ServerProperty \
    /sensor11/integer_endpoint <VALUE>
```

> As before by changing the endpoint and payload in the astartectl command other data types can be
> transmitted to the device.

Where:
- `<REALM>` is your realm name
- `<DEVICE_ID>` is the device ID to send the data to
- `<API_URL>` is the Astarte api endpoint
- `<VALUE>` is the new value for the property
