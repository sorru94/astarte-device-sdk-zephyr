<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Astarte device

A more in depth description of the architecture of the Astarte device is present in this section.

## Device connectivity state machine

The device connectivity state machine ensures that the connect and disconnect callbacks are
triggered in the correct moments. Furthermore it ensures no transmission to Astarte can be
performed before connectivity has been achieved.

```mermaid
%%{init: {"flowchart": {"rankSpacing": 100, "padding": 60}} }%%
flowchart TD
    classDef globStructs stroke:#A52A2A

    DISCONNECTED --> |MQTT disconnection event| DISCONNECTED
    DISCONNECTED --> |Connection request / MQTT connection event| CONNECTING
    CONNECTING --> |MQTT disconnection event| DISCONNECTED
    CONNECTING --> |MQTT connection event| CONNECTING
    CONNECTING --> |Handshake msg delivered| CONNECTED
    CONNECTED --> |MQTT disconnection event| DISCONNECTED
    CONNECTED --> |MQTT connection event| CONNECTING
```
