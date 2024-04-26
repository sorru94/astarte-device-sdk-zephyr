<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Astarte MQTT client

Astarte MQTT client is a soft wrapper around the MQTT client library from Zephyr. It extends the
functionality of the standard MQTT client.

## Automatic reconnection

The Astarte MQTT client implements an automatic reconnection policy in cases where connectivity
with the MQTT broker is lost.
An exponential backoff is used to limit reconnection attempts. The backoff time will start from
the user configurable polling timeout and end at the minimum between the user configurable polling
timeout and twice the keep alive time for the MQTT connection.

```mermaid
%%{init: {"flowchart": {"rankSpacing": 100, "padding": 60}} }%%
flowchart TD
    classDef globStructs stroke:#A52A2A

    DISCONNECTED --> |Connection request| CONNECTING
    CONNECTING --> |CONNACK reception| CONNECTED
    CONNECTING --> |Disconnection request| DISCONNECTING
    CONNECTING --> |Disconnection event/ poll failure / connection timeout elapsed| CONNECTION_ERROR
    CONNECTED --> |Disconnection request| DISCONNECTING
    CONNECTED --> |Disconnection event / poll failure| CONNECTION_ERROR
    DISCONNECTING --> |Disconnection event| DISCONNECTED
    CONNECTION_ERROR --> |Connection request / backoff period finished| CONNECTING
    CONNECTION_ERROR --> |Disconnection request| DISCONNECTED
```
