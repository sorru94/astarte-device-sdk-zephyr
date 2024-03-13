<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Properties sample

This sample shows how to set and unset device of properties as well as how to receive server
properties updates.

## Required configuration

Refer to the
[generic samples readme](https://github.com/secomind/astarte-device-sdk-zephyr/tree/master/samples/README.md)
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
    org.astarteplatform.zephyr.examples.ServerProperty \
    /sensor11/integer_endpoint <VALUE>
```

Where:
- `<REALM>` is your realm name
- `<DEVICE_ID>` is the device ID to send the data to
- `<API_URL>` is the Astarte api endpoint
- `<VALUE>` is the new value for the property

The device is configured to react to four different property values in different ways.
All other values will be ignored.
- `40` The device sets some device owned properties
- `41` The device sets some device owned properties
- `42` The device unsets the device owned properties set with `40`
- `43` The device unsets the device owned properties set with `41`
