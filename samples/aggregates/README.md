<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Aggregates sample

This sample shows how reception of aggregate datastreams can be performed
using the provided APIs.

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
    org.astarteplatform.zephyr.examples.ServerAggregate \
    /11 \
    '{"longinteger_endpoint":45993543534, "booleanarray_endpoint":[false,false,true,true]}'
```

Where:
- `<REALM>` is your realm name
- `<DEVICE_ID>` is the device ID to send the data to
- `<API_URL>` is the Astarte api endpoint

