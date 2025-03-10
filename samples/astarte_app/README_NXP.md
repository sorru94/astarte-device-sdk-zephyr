<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Clea Astarte IoT devices using Zephyr RTOS

[![License badge](https://img.shields.io/badge/License-Apache%202.0-red)](https://www.apache.org/licenses/LICENSE-2.0.txt)
![Language badge](https://img.shields.io/badge/Language-C-yellow)
![Language badge](https://img.shields.io/badge/Language-C++-yellow)
[![Board badge](https://img.shields.io/badge/Board-EVK&ndash;MIMXRT1064-blue)](https://www.nxp.com/pip/MIMXRT1064-EVK)
[![Category badge](https://img.shields.io/badge/Category-CLOUD%20CONNECTED%20DEVICES-yellowgreen)](https://mcuxpresso.nxp.com/appcodehub?search=cloud%20connected%20devices)
[![Toolchain badge](https://img.shields.io/badge/Toolchain-VS%20CODE-orange)](https://github.com/nxp-mcuxpresso/vscode-for-mcux/wiki)

Clea Astarte is an open source IoT platform focused on data management. It takes care of everything
from collecting data from devices to delivering data to end-user applications.
This demo application shows how to create an Astarte device using the Zephyr RTOS framework on
the i.MX RT1064 evaluation kit.

For more information on Astarte check out the
[Astarte documentation](https://docs.astarte-platform.org/).

## Table of Contents
1. [Software](#step1)
2. [Hardware](#step2)
3. [Setup](#step3)
4. [Results](#step4)
5. [Support](#step5)
6. [Release Notes](#step6)

## 1. Software<a name="step1"></a>

This demo application is intended to be built using VS Code through the
[MCUXpresso](https://www.nxp.com/design/design-center/software/embedded-software/mcuxpresso-for-visual-studio-code:MCUXPRESSO-VSC)
extension. It furthermore requires the Zephyr developer dependencies for MCUXpresso to be
installed, this can be done through the
[MCUXpresso Installer](https://github.com/nxp-mcuxpresso/vscode-for-mcux/wiki/Dependency-Installation).

The application demonstrates the use of the
[Astarte device SDK for Zephyr](https://github.com/astarte-platform/astarte-device-sdk-zephyr) at
version **0.7.2** in conjunction with [Zephyr RTOS](https://github.com/zephyrproject-rtos/zephyr) at
version **4.1.0**.

This application heavily relies on the
[Astarte quick instance](https://docs.astarte-platform.org/device-sdks/common/astarte_quick_instance.html)
tutorial. Ensure all its requirements are satisfied and dependencies installed.

## 2. Hardware<a name="step2"></a>

This demo application is provided for use with the NXP MIMXRT1064-EVK board.
However, the application can be run on a wide range of boards with minimal configuration changes.
The minimum requirements for this application are the following:
- At least 250KB of RAM
- At least 500KB of FLASH storage
- An Ethernet interface

Additionally, each board should provide a dedicated flash partition for Astarte.

## 3. Setup<a name="step3"></a>

### 3.1 Step 1

You will need an Astarte instance to which the device will connect.
The easiest way to set one up is to follow the
[Astarte quick instance](https://docs.astarte-platform.org/device-sdks/common/astarte_quick_instance.html)
tutorial.
The tutorial guides you through setting up an Astarte instance on a local host machine. Make sure
both the host machine and the device board are be connected to the same LAN.

### 3.2 Step 2

In this step, we will install in Astarte the interfaces required for our device.
The interfaces for this demo application can be found in this repository in the `interfaces` folder.
Each interface is defined in a `.json` file. They should be installed in the Astarte instance prior
to the device connection with Astarte.

To install the three interfaces in the Astarte instance, open the Astarte dashboard, navigate to the
interfaces tab and click on install new interface.
You can copy and paste the content of the JSON files for each interface in the right box overwriting
the default template.

### 3.3 Step 3

An Astarte instance can have many devices connected to it at the same time. Where each device is
uniquely identified within a realm by a
[device ID](https://docs.astarte-platform.org/astarte/latest/010-design_principles.html#device-id).
Which is a 128-bit identifier expressed as a base 64 URL encoded string.

You can use any device ID of your choice for the device in this application
(for example `V_zv6ThCCtXWveQ8mPjsKg`).

### 3.4 Step 4

Each device is associated with an Astarte instance through a process called registration.
There are two standard device registration procedures:
- Off-board registration. The device will be registered manually in the Astarte instance.
  Astarte will issue a device-specific credential secret that will be copied on the device and used
  for authentication.
- On-board registration. The Astarte device for Zephyr provides APIs for registration using a JWT
  token issued by Astarte that should be manually installed on the device.

This demo application supports both. A selection of the preferred one can be operated through the
Kconfig of the demo.

To register a new device manually start by opening the dashboard and navigate to the devices tab.
Click on register a new device, there you can input your own device ID or generate a random one.
Click on register device, this will register the device and give you a credentials secret.
The credentials secret will be used by the device to authenticate with Astarte.
Copy it somewhere safe as it will be used in the next steps.

If you prefer to use on-board registration you can use the all-access access token you generated
in the Astarte quick instance tutorial (the same one you used to access the dashboard).

### 3.5 Step 5

The configuration on the Astarte side is now complete. Let's then configure this demo application.
We will need to change some values from the application Kconfig. This can be done through the
menuconfig or by changing the `prj.conf` file.

We should configure the device to connect to our Astarte instance with the correct credentials.
The following is an extract of the configuration for the `prj.conf`.
```conf
# Configuration for the Astarte device SDK
CONFIG_ASTARTE_DEVICE_SDK=y
CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME="HOSTNAME"
CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP=y
CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_MQTT=y
CONFIG_ASTARTE_DEVICE_SDK_CLIENT_CERT_TAG=1
CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME="REALM_NAME"
# CONFIG_ASTARTE_DEVICE_SDK_PAIRING_JWT="ACCESS_TOKEN" # Only required using on-board registration

# Configuration for the sample
CONFIG_DEVICE_ID="DEVICE_ID"
CONFIG_CREDENTIAL_SECRET="CREDENTIAL_SECRET" # Only required for off-board registration

CONFIG_DEVICE_REGISTRATION=n # Set to yes for on-board registration, no otherwise
CONFIG_DEVICE_INDIVIDUAL_TRANSMISSION=y
CONFIG_DEVICE_OBJECT_TRANSMISSION=y
CONFIG_DEVICE_PROPERTY_SET_TRANSMISSION=y
CONFIG_DEVICE_PROPERTY_UNSET_TRANSMISSION=y
```

Configure the device to connect to our Astarte instance by changing the following values:
- `HOSTNAME` is the hostname for the Astarte instance
- `REALM_NAME` is the name of the Astarte realm
- `DEVICE_ID` is the device identifier used to connect to Astarte
- `CREDENTIAL_SECRET` is the credential secret for the device obtained during off-board
  registration. This option should be set only when the device is registered manually on Astarte.
- `ACCESS_TOKEN` is the access token for the device to be used for on-board registration. This
  option should be set only when the device is registered on-board.

Additionally, the sample can be fine-tuned enabling or disabling some features.
- `CONFIG_DEVICE_REGISTRATION` when activated the device will perform onboard registration using
  the access token.
- `CONFIG_DEVICE_INDIVIDUAL_TRANSMISSION`, `CONFIG_DEVICE_OBJECT_TRANSMISSION`,
  `CONFIG_DEVICE_PROPERTY_SET_TRANSMISSION` and `CONFIG_DEVICE_PROPERTY_UNSET_TRANSMISSION` enable
  or disable various transmission types.

## 4. Results<a name="step4"></a>

Upon running the code the demo application will display its progress on the serial monitor.
The device will first connect to Astarte performing an onboard device registration if configured
to do so or using the credential secret if the registration has been performed off-board.
It will then transmit some data while listening for incoming messages from Astarte.
Once the transmission is complete the device will gracefully terminate the connection and be
destroyed.

## 5. Support<a name="step5"></a>

For more information about Astarte, refer to the
[Astarte documentation](https://docs.astarte-platform.org/) or explore the related projects on
[GitHub](https://github.com/astarte-platform).

Questions regarding the content/correctness of this example can be entered as Issues within this
GitHub repository.

## 6. Release Notes<a name="step6"></a>
| Version | Description / Update                           | Date                        |
|:-------:|------------------------------------------------|----------------------------:|
| 1.0     | Initial release on Application Code Hub        | October 23<sup>rd</sup> 2024 |

## Licensing

This project is licensed under Apache 2.0 license. Find our more at
[www.apache.org](https://www.apache.org/licenses/LICENSE-2.0.).
