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

### Adding the module as a dependency to an application

To add this module as a dependency to an application the application's `west.yml` manifest file
should be modified in the following way.
First, a new remote should be added:
```yml
  remotes:
    # ... other remotes ...
    - name: secomind
      url-base: https://github.com/secomind
```
Second, a new entry should be added to the projects list:
```yml
  projects:
    # ... other projects ...
    - name: astarte-device-sdk-zephyr
      remote: secomind
      repo-path: astarte-device-sdk-zephyr.git
      path: astarte-device-sdk-zephyr
      revision: release-0.5
      west-commands: scripts/west-commands.yml
```
Remember to run `west update` after performing changes to the manifest file.

In addition, an addition to the `Kconfig` file of the application is necessary.
```
rsource "../../astarte-device-sdk-zephyr/Kconfig"
```
This will add the configuration options of the example module to the application.

### Adding the module as a standalone application

For development or evaluation purposes it can be useful to add this module as a stand alone
application in its own workspace.
This will make it possible to run the module samples without having to set up an additional
application.

#### Creating a new workspace, venv and cloning the example

The first step is to create a new workspace folder and a venv where `west` will reside.
As well as cloning this example repository inside such workspace.

```shell
# Create a venv and install the west tool
mkdir ~/zephyr-workspace
python3 -m venv ~/zephyr-workspace/.venv
source ~/zephyr-workspace/.venv/bin/activate
pip install west
# Clone the example application repo
git clone https://github.com/secomind/astarte-device-sdk-zephyr.git ~/zephyr-workspace/astarte-device-sdk-zephyr
```

#### Initializing the workspace

The second step is to initialize the west workspace, using the example repository as the manifest
repository.

```shell
# initialize my-workspace for the example-application (main branch)
cd ~/zephyr-workspace
west init -l astarte-device-sdk-zephyr
# update Zephyr modules
west update
```

#### Exporting zephyr environment and installing python dependencies

Perform some final configuration operations.

```shell
west zephyr-export
pip install -r ~/zephyr-workspace/zephyr/scripts/requirements.txt
pip install -r ~/zephyr-workspace/astarte-device-sdk-zephyr/scripts/requirements.txt
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

And the following command to run on an emulated board or the native simulator:
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

#### Unit testing

To run the unit tests use:
```shell
west twister -c -v --inline-logs -p unit_testing -T ./astarte-device-sdk-zephyr/unittests
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
    "C_Cpp.default.compilerPath": "${userHome}/zephyr-sdk-0.16.3/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc",
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

One more step is required. The python extension does not load at startup when opening VS Code
unless some
[specific activation](https://github.com/microsoft/vscode-python/blob/a5ab3b8c05e84670176aef8fe246ff0164707ac4/package.json#L66-L78)
events happens.
This means that for a zephyr workspace, that by default does not have any of such activation
events, the extension does not load and when opening a terminal the venv does not activate.
To fix this you can create an empty `mspythonconfig.json` file that will serve as an activation
event. However, note that this file might interfere when using Pyright in your project.

## Architectural documentation

Some additional documentation for the library architecture is available.
- [Astarte MQTT client](doc/architecture/astarte_mqtt_client.md) is a soft wrapper around the MQTT
  client library from Zephyr. It extends the functionality of the standard MQTT client.
