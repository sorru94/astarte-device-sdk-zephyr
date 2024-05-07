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

## Subscription retry procedure

The Astarte MQTT client implements a retry mechanism to ensure subscription messages are properly
retransmitted.

```mermaid
flowchart TD
    classDef globStructs stroke:#A52A2A

    subgraph Retain message
    S(Send subscription msg)
    W(Waiting for SUBACK)
    R(Resend subscription msg)
    end
    E(End)

    S --> W
    W --> |Timeout| R
    R --> W
    W --> |Received SUBACK| E
```

## Publish retry procedure

The Astarte MQTT client implements a retry mechanism to ensure publish messages with QoS > 0 are
properly retransmitted.

### QoS 1

```mermaid
flowchart TD
    classDef globStructs stroke:#A52A2A

    subgraph Retain message
    S(Send publish msg)
    W(Waiting for PUBACK)
    R(Resend publish msg)
    end
    E(End)

    S --> W
    W --> |Timeout| R
    R --> W
    W --> |Received PUBACK| E
```

### QoS 2

```mermaid
flowchart TD
    classDef globStructs stroke:#A52A2A

    subgraph Retain message
    SPUBLISH(Send publish msg)
    WPUBREC(Waiting for PUBREC)
    RPUBLISH(Resend publish msg)
    end
    SPUBREL(Send PUBREL msg)
    WPUBCOMP(Waiting for PUBCOMP)
    RPUBREL(Resend PUBREL msg)
    E(End)

    SPUBLISH --> WPUBREC
    WPUBREC --> |Timeout| RPUBLISH
    RPUBLISH --> WPUBREC
    WPUBREC --> |Received PUBREC| SPUBREL
    SPUBREL --> WPUBCOMP
    WPUBCOMP --> |Timeout| RPUBREL
    RPUBREL --> WPUBCOMP
    WPUBCOMP --> |Received PUBCOMP| E
```

## Reception of QoS 1 publishes

Upon the reception of a QoS 1 publish message the Astarte MQTT driver automatically transmit a
PUBACK message back to the MQTT broker.

## Reception of QoS 2 messages

QoS 2 publishes require a more complex control flow in the receiver compared to lower QoS levels.

```mermaid
flowchart TD
    classDef globStructs stroke:#A52A2A

    subgraph Retain message
    START(Start)
    SPUBREC(Send PUBREC msg)
    end
    WPUBREL(Waiting for PUBREL)
    RPUBREC(Resend PUBREC msg)
    SPUBCOMP(Send PUBCOMP msg)
    E(End)

    START --> |Received publish| SPUBREC
    SPUBREC --> WPUBREL
    WPUBREL --> |Timeout| RPUBREC
    RPUBREC --> WPUBREL
    WPUBREL --> |Received PUBREC| SPUBCOMP
    SPUBCOMP --> E
```
