# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

from datetime import datetime, timedelta
from typing import Any

from west import log

from conftest import TestcaseHelper
from interface_data import InterfaceData, Ownership
from http_requests import prepare_transmit_data, post_server_data, decode_value, get_server_data
from test_utilities import compare_data, encode_shell_bson


class DatastreamMapping:
    def __init__(self, path: str, value: Any, timestamp: datetime | None = None):
        self.path = path
        self.timestamp = timestamp
        self.value = value


class Datastream(InterfaceData):
    def __init__(self, interface: str, ownership: Ownership, mappings: list[DatastreamMapping]):
        super().__init__(interface, ownership)
        self.mappings = mappings

    def _send_server_data(self, helper: TestcaseHelper):
        for mapping in self.mappings:
            path = mapping.path
            value = prepare_transmit_data(path.split("/")[-1], mapping.value)
            post_server_data(helper.astarte_cfg, self.interface, path, value)

    def _check_server_received_data(self, helper: TestcaseHelper, send_start: datetime):
        received_data = get_server_data(
            helper.astarte_cfg, self.interface, limit=1
        )  # , path=mapping.path, since_after=timestamp, to=(timestamp + timedelta(seconds=1))
        log.inf(f"Server individual data {received_data}")

        for mapping in self.mappings:
            last_path_segment = mapping.path.split("/")[-1]
            expected_value = mapping.value

            got_value = decode_value(received_data[last_path_segment]["value"], last_path_segment)

            if not compare_data(got_value, expected_value):
                log.err(f"Mismatch for {endpoint}: expected {expected_value} got {got_value}")
                raise ValueError(f"Mismatch for {endpoint}")

    def _get_device_shell_commands(self, base_command: str) -> list[str]:
        result = []

        for mapping in self.mappings:
            path = mapping.path
            bson_base64 = encode_shell_bson(mapping.value)

            if mapping.timestamp:
                unix_t = int(mapping.timestamp.timestamp() * 1000)
                command = (
                    f"{base_command} individual {self.interface} {path} {bson_base64} {unix_t}"
                )
            else:
                command = f"{base_command} individual {self.interface} {path} {bson_base64}"

            result.append(command)

        return result
