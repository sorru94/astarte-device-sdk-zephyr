<!--
Copyright 2024 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

# Astarte Device SDK Zephyr

Astarte device library for the Zephyr framework.

## Getting started

Before getting started, make sure you have a proper Zephyr development environment.
Follow the official
[Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html).

### Using the library as a dependency in an application

To add this module as a dependency to an application the application's `west.yml` manifest file
should be modified in the following way.
First, a new remote should be added:
```yml
  remotes:
    # ... other remotes ...
    - name: astarte-platform
      url-base: https://github.com/astarte-platform
```
Second, a new entry should be added to the projects list:
```yml
  projects:
    # ... other projects ...
    - name: astarte-device-sdk-zephyr
      remote: astarte-platform
      repo-path: astarte-device-sdk-zephyr.git
      path: astarte-device-sdk-zephyr
      revision: v0.9.0
      west-commands: scripts/west-commands.yml
      import: true
```
Remember to run `west update` after performing changes to the manifest file.

In addition, the Astarte device library `Kconfig` should be added to the `Kconfig` file of the
application.
```
rsource "../../astarte-device-sdk-zephyr/Kconfig"
```
This will add the configuration options of the example module to the application.

### Using the library as a standalone application

For development or evaluation purposes the library can be used as a stand alone application in its
own workspace.
This will make it possible to build and execute the module samples without having to set up an
extra application.

#### Creating a new workspace, venv and install west

Start by creating a new workspace folder and a venv where `west` will reside.

```shell
mkdir ~/astarte-zephyrproject && cd ~/astarte-zephyrproject
python3 -m venv .venv
source .venv/bin/activate
pip install west
```

#### Initializing the workspace

The west workspace should be then intialized, using the Astarte libary repository as the manifest
repository.

```shell
west init -m git@github.com:astarte-platform/astarte-device-sdk-zephyr --mr v0.9.0
west update
```

#### Exporting zephyr environment and installing python dependencies

Lastly some dependencies will need to be installed for all west extensions to work correctly.

```shell
west zephyr-export
west packages pip --install
west packages pip --install -- -r ./astarte-device-sdk-zephyr/scripts/requirements.txt
```
#### Fetching binary blobs for ESP32

If building for an esp32 the binary blobs will need to be downloaded before building the
application.

```shell
west blobs fetch hal_espressif
```

Note: this command should be re-run when updating Zephyr to a newer version

#### Building and running a sample application

To build one of the samples applications, run the following command:

```shell
west build -p always -b <BOARD> ./astarte-device-sdk-zephyr/samples/<SAMPLE>
```

Where `<BOARD>` is one of the [boards](https://docs.zephyrproject.org/latest/boards/index.html)
supported by Zephyr and `<SAMPLE>` is one of the samples contained in the `samples` folder.

Once you have built the application, run the following command to flash it on the ESP board:
```shell
west flash
```

Or the following command to run on an emulated board or the native simulator:
```shell
west build -t run
```

#### One time configuration

While usually the configuration setting are set in a `prj.conf` file, during development it might
be useful to set them manually.
To do so run the following command:
```shell
west build -t menuconfig
```
This command will require the application to have already been build to function correctly.
After changing the configuration, the application can be flashed directly using `west flash`.

#### Reading serial monitor

The standard configuration for all the samples include settings for printing the logs through the
serial port connected to the host.

## Unit testing

A set of unit test is present under the `tests` directory of this project.
They can be run using twister.
```shell
west twister -c -T ./astarte-device-sdk-zephyr/tests
```

## West extension commands

### Interface definitions generation

The standard format of an Astarte interface is a JSON file as defined in the
[Astarte documentation](https://docs.astarte-platform.org/astarte/latest/030-interface.html).
However, an Astarte device on Zephyr uses C structures to define Astarte interfaces.
Translating an interface from a JSON definition to a C definition for Zephyr can be a repetitive
and error-prone process.

For this reason, an extension command for `west` has been created to facilitate this procedure.
The `generate-interfaces` extension command accepts as input one or more JSON interface definitions
and automatically generates the corresponding C source code.
Run `west generate-interfaces --help` to learn about the generation options.

#### Build time interface definitions generation

It's also possible to automatically generate the interface definition structs.
Generated files will be located in the build directory:
`build/zephyr/include/generated/astarte_generated_interfaces.*`.

This allows an easier management of generated files that won't need to be committed to the vcs.
Enabling the feature is straight forward and requires setting two configuration values in
the project `prj.conf` file:
```plain
CONFIG_ASTARTE_DEVICE_SDK_CODE_GENERATION=y
CONFIG_ASTARTE_DEVICE_SDK_CODE_GENERATION_INTERFACE_DIRECTORY="<interface jsons directory>"
```
Replace `<interface jsons directory>` with the relative path to the interfaces directory.

After adding the two properties you can run a build targeting `astarte_generate_interfaces`.
```sh
west build -b native_sim -t astarte_generate_interfaces .
```
or simply launch a build of the project.

After this you'll be able to include the generated header file:
```c
#include "astarte_generated_interfaces.h"
```

For a complete example you can take a look at `e2e/prj.conf` and the `interfaces/` directory.
The generated interfaces are included in `e2e/src/runner.c`.

### Code formatting

The code in this module is formatted using `clang-format`.
An extension command for `west` has been created to facilitate formatting.
Run `west format --help` to learn about the formatting options.

### Static code analysis

[CodeChecker](https://codechecker.readthedocs.io/en/latest/) is the one of the
[two](https://docs.zephyrproject.org/latest/develop/sca/index.html) natively supported static
analysis tools in `west`.
It can be configured to run different static checkers, such as `clang-tidy` and `cppcheck`.

An extension command for `west` has been created to facilitate running static analysis with
`clang-tidy`.
Run `west static --help` to read what this command performs.

### Build doxygen documentation

The dependencies for generating the doxygen documentation are:
- `doxygen <= 1.9.6`
- `graphviz`

An extension command for `west` has been created to facilitate generating the documentation.
Run `west docs --help` for more information on how to generate the documentation.

## VS Code integration

Here are some quick tips to configure VS Code with a Zephyr workspace.
Make sure the following extensions are installed:
- C/C++ Extension Pack
- Python

Create the file `./.vscode/settings.json` and fill it with the following content:
```json
{
    // Hush CMake
	"cmake.configureOnOpen": false,

    // Configure Python so that each new terminal will activate the venv
    // defaultInterpreterPath is not always be picked up, backup cmd: 'Python: select interpreter'
    "python.defaultInterpreterPath": "${workspaceFolder}/.venv/bin/python",
    "python.terminal.activateEnvironment": true,
    "python.terminal.activateEnvInCurrentTerminal": true,

    // IntelliSense
    "C_Cpp.default.compilerPath": "${userHome}/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc",
    "C_Cpp.default.compileCommands": "${workspaceFolder}/build/compile_commands.json",
    "C_Cpp.autoAddFileAssociations": false,
    "C_Cpp.debugShortcut": false
}
```
The first setting will avoid noisy popups from the CMake extension.
The python related settings will ensure that each time the integrated terminal is open, the
venv containing `west` will be automatically activated.
The C/C++ settings will configure intellisense. Those setting might be slightly different
for your specific Zephyr SDK installation as versions might change.
Also, the compile commands path assumes `west build` gets run from the root of the zephyr
workspace.

## Architectural documentation

Some additional documentation for the library architecture is available.
- [Astarte MQTT client](doc/architecture/Astarte-MQTT-client-architecture.md) is a soft wrapper
  around the MQTT client library from Zephyr. It extends the functionality of the standard MQTT
  client.
- [Astarte device](doc/architecture/Astarte-device-architecture.md) is the main entry point for any
  interaction with Astarte.
