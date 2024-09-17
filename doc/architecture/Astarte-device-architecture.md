<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Astarte device architecture

A more in depth description of the architecture of the Astarte device is present in this section.

## Device connectivity state machine

The device connectivity state machine ensures that the connect and disconnect callbacks are
triggered in the correct moments. Furthermore it ensures no transmission to Astarte can be
performed before connectivity has been achieved.

```mermaid
%% This is a mermaid graph. Your can render it using the live editor at: https://mermaid.live
flowchart TD
    DISCONNECTED --> |Connection request| MQTT_CONNECTING

    DISCONNECTED --> |MQTT connection event| START_HANDSHAKE
    MQTT_CONNECTING --> |MQTT connection event| START_HANDSHAKE
    END_HANDSHAKE --> |MQTT connection event| START_HANDSHAKE
    CONNECTED --> |MQTT connection event| START_HANDSHAKE
    HANDSHAKE_ERROR --> |MQTT connection event / Backoff expiration| START_HANDSHAKE

    START_HANDSHAKE --> |Internal error| HANDSHAKE_ERROR
    START_HANDSHAKE --> |Start handshake success| END_HANDSHAKE

    END_HANDSHAKE --> |Subsription failure / Internal error| HANDSHAKE_ERROR
    END_HANDSHAKE --> |Hanshake success| CONNECTED

    MQTT_CONNECTING --> |MQTT disconnection event| DISCONNECTED
    START_HANDSHAKE --> |MQTT disconnection event| DISCONNECTED
    END_HANDSHAKE --> |MQTT disconnection event| DISCONNECTED
    CONNECTED --> |MQTT disconnection event| DISCONNECTED
    HANDSHAKE_ERROR --> |MQTT disconnection event| DISCONNECTED
```
