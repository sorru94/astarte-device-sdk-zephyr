<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Datastreams sample

This sample shows how transmission and reception of individual datastreams can be performed
using the provided APIs.

## Required configuration

Refer to the
[generic samples readme]
(https://github.com/secomind/astarte-device-sdk-zephyr/tree/master/samples/README.md)
to configure correctly the device.

## Manually send test data

By leveraging [`astartectl`](https://github.com/astarte-platform/astartectl) we can send test
data to the device.
We recomend storing the JWT in a shell variable to access it with ease.
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

Another option, to see a simple "hello world" message displayed on the serial port connected to the
device is:
```sh
astartectl appengine \
    --token $TOKEN --realm-name "<REALM>" --astarte-url "<API_URL>" \
    devices send-data <DEVICE_ID> \
    org.astarteplatform.zephyr.examples.ServerDatastream \
    /stringarray_endpoint '["hello", "world!"]'
```
