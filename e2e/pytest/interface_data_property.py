# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

from datetime import datetime
from typing import Any

from west import log

from conftest import TestcaseHelper
from interface_data import InterfaceData, Ownership
from http_requests import (
    prepare_transmit_data,
    post_server_data,
    decode_value,
    get_server_data,
    unset_server_property,
)
from test_utilities import compare_data, encode_shell_bson


class Property:
    def __init__(self, path: str, value: Any):
        self.path = path
        self.value = value


class PropertySet(InterfaceData):
    def __init__(self, interface: str, ownership: Ownership, properties: list[Property]):
        super().__init__(interface, ownership)
        self.properties = properties

    def _send_server_data(self, helper: TestcaseHelper):
        for prop in self.properties:
            value = prepare_transmit_data(prop.path.split("/")[-1], prop.value)
            post_server_data(helper.astarte_cfg, self.interface, prop.path, value)

    def _check_server_received_data(self, helper: TestcaseHelper, send_start: datetime):
        received_data = get_server_data(helper.astarte_cfg, self.interface)
        log.inf(f"Server property data {received_data}")

        # Retrieve and check properties
        for prop in self.properties:
            parameter = prop.path.split("/")[1]
            last_path_segment = prop.path.split("/")[-1]
            expected_value = prop.value
            got_value = decode_value(received_data[parameter][last_path_segment], last_path_segment)

            if not compare_data(got_value, expected_value):
                log.err(
                    f"Mismatch for {last_path_segment}: expected {expected_value} got {got_value}"
                )
                raise ValueError(f"Mismatch for {last_path_segment}")

    def _get_device_shell_commands(self, base_command: str) -> list[str]:
        result = []

        for prop in self.properties:
            path = prop.path
            bson_base64 = encode_shell_bson(prop.value)
            result.append(f"{base_command} property set {self.interface} {path} {bson_base64}")

        return result


class PropertyUnset(InterfaceData):
    def __init__(self, interface: str, ownership: Ownership, unset: list[str]):
        super().__init__(interface, ownership)
        self.unset = unset

    def _send_server_data(self, helper: TestcaseHelper):
        for prop in self.unset:
            unset_server_property(helper.astarte_cfg, self.interface, prop)

    def _check_server_received_data(self, helper: TestcaseHelper, send_start: datetime):
        received_data = get_server_data(helper.astarte_cfg, self.interface)
        log.inf(f"Server property unset data {received_data}")

        for prop in self.unset:
            paramter = prop.split("/")[1]
            endpoint = prop.split("/")[-1]
            try:
                parameter_object = received_data[paramter]
                got_value = decode_value(parameter_object, endpoint)
            except KeyError:
                # handled because completety unset interfaces do not return anything
                got_value = {}

            if got_value != {}:
                raise ValueError("Incorrect data stored on server")

    def _get_device_shell_commands(self, base_command: str) -> list[str]:
        result = []

        for path in self.unset:
            result.append(f"{base_command} property unset {self.interface} {path}")

        return result
