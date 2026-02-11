<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Astarte device sample for Zephyr

This sample application demonstrates how to use the Astarte Device SDK for Zephyr to connect a device, manage transmission types, and handle data reception.

**Features demonstrated**:
- Connecting to Astarte (Ethernet or WiFi).
- Registering a device using a pairing token.
- Transmitting individual datastreams, object datastreams, and properties.
- Receiving data and property updates from Astarte.
- Persisting credentials in a separate flash partition (optional).

## Prerequisites

Before starting, ensure you have the following tools installed:
- **Zephyr SDK**: A working Zephyr development environment.
- **[astartectl](https://github.com/astarte-platform/astartectl)**: To send test data from your host machine to the device.
- **Python**: Required for west and optional flash tools.
- **[littlefs-python](https://pypi.org/project/littlefs-python/)** (Optional): Required only if using the flash configuration feature.

## Configuration

You can configure the sample using `Kconfig` (menuconfig) or by editing `prj.conf`.

### Connectivity (WiFi vs. Ethernet)

**Ethernet** is enabled by default.

**WiFi** requires specifying an extra configuration file and credentials:
- Add `prj-wifi.conf` to your build command (see *Building* section).
- Configure your credentials in `prj.conf` (or `private.conf`):
  ```conf
  CONFIG_WIFI_SSID="<YOUR_SSID>"
  CONFIG_WIFI_PASSWORD="<YOUR_PASSWORD>"
  ```

**Note for NXP Users**: You may need to fetch binary blobs for WiFi: `west blobs fetch hal_nxp`

### Astarte connection

Choose **one** of the following configurations based on your Astarte instance.

#### Option A: Quick instance (non-TLS / developer mode)

Use this for local testing with a default [Astarte Quick Instance](https://docs.astarte-platform.org/device-sdks/common/astarte_quick_instance.html).

Modify `prj.conf`:
```conf
CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME="<HOSTNAME>"
CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP=y
CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT=y
CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME="<REALM_NAME>"

CONFIG_DEVICE_ID="<DEVICE_ID>"
CONFIG_CREDENTIAL_SECRET="<CREDENTIAL_SECRET>"
```

#### Option B: Production (fully TLS capable)

Use this for production Astarte instances with valid CA certificates.
1. Modify `prj.conf`:
   ```conf
   CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME="<HOSTNAME>"
   CONFIG_ASTARTE_DEVICE_SDK_HTTPS_CA_CERT_TAG=1
   CONFIG_ASTARTE_DEVICE_SDK_MQTTS_CA_CERT_TAG=1
   CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG=2
   CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME="<REALM_NAME>"

   CONFIG_DEVICE_ID="<DEVICE_ID>"
   CONFIG_CREDENTIAL_SECRET="<CREDENTIAL_SECRET>"
   ```
2. Update `ca_certificates.h`: Place your valid CA certificate (PEM format) in the `ca_certificate_root` array.

### Managing secrets

#### Method 1: `private.conf` (recommended for development)

Avoid committing secrets to version control by creating a `samples/astarte_app/private.conf` file (ignored by git).
```conf
# samples/astarte_app/private.conf
CONFIG_ASTARTE_DEVICE_SDK_PAIRING_JWT="..."
CONFIG_CREDENTIAL_SECRET="..."
CONFIG_WIFI_PASSWORD="..."
```

#### Method 2: Flash partition (advanced/production)

Store credentials in a dedicated flash partition so the application binary remains generic.
1. Enable `CONFIG_GET_CONFIG_FROM_FLASH=y` in Kconfig.
2. Update `config_lfs/configuration.json` with your secrets.
3. Generate the partition binary:
   ```bash
   littlefs-python create samples/astarte_app/config_lfs/ samples/astarte_app/lfs.bin --block-size 4096 --block-count 6
   ```
4. Flash `lfs.bin` to the correct address for your board.
   **⚠️ IMPORTANT**: The address `0xBE20000` is specific to the **NXP FRDM RW612**. If using a different board, check your Device Tree (`.dts`) or partition map to find the correct address for the storage partition. Flashing to the wrong address may brick your device.
   *Example for FRDM RW612 via JLink:*
   ```bash
   JLinkExe -Device RW612 -if SWD -Speed 4000
   connect
   LoadBin samples/astarte_app/lfs.bin 0xBE20000
   reset
   ```

### Building the sample

Run the following `west` commands from your Zephyr workspace.

**Standard build (Ethernet):**
```bash
west build -b <your_board_name> samples/astarte_app
```
**WiFi build:**
```bash
west build -b <your_board_name> samples/astarte_app -- -DEXTRA_CONF_FILE="prj-wifi.conf"
```
**Zephyr LTS Build:** If using the LTS version of Zephyr, add the LTS config file:
```bash
west build -b <your_board_name> samples/astarte_app -- -DCONF_FILE="zephyr-lts.conf"
```
**Native Sim (Net-tools):** If using native_sim, set up the host interface first:
```bash
./net-setup.sh --config nat.conf
west build -b native_sim samples/astarte_app
```

### Testing with Astarte
Once the device is running, use `astartectl` to simulate server-side interactions.

**Setup Environment Variable:** Save your AppEngine token to simplify commands:
```bash
export TOKEN=<your_appengine_token>
```

#### A. Receive individual data
Send a value to the device's `ServerDatastream` interface.
```bash
astartectl appengine \
    --token $TOKEN --realm-name "<REALM>" --astarte-url "<API_URL>" \
    devices send-data <DEVICE_ID> \
    org.astarteplatform.zephyr.examples.ServerDatastream \
    /boolean_endpoint true
```
#### B. Receive object data
Send an aggregated object to the device.
```bash
astartectl appengine \
    --token $TOKEN --realm-name "<REALM>" --astarte-url "<API_URL>" \
    devices send-data <DEVICE_ID> \
    org.astarteplatform.zephyr.examples.ServerAggregate \
    /sensor11 \
    '{"double_endpoint": 459.432, "integer_endpoint": 32, "boolean_endpoint": true, "string_endpoint": "test"}'
```
#### C. Set properties
Update a property on the device.
```bash
astartectl appengine \
    --token $TOKEN --realm-name "<REALM>" --astarte-url "<API_URL>" \
    devices send-data <DEVICE_ID> \
    org.astarteplatform.zephyr.examples.ServerProperty \
    /sensor11/integer_endpoint 100
```

### Architecture & organization

#### Application Structure:

- **Master thread**: Handles network connectivity (Ethernet/WiFi) and reconnection logic.
- **Astarte thread**: Manages the MQTT connection, TLS, and handles incoming data callbacks.
- **Data thread**: Generates and sends dummy data (Individuals, Objects, Properties) based on Kconfig settings.
