# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

import base64
from datetime import datetime

import bson


def encode_shell_bson(value: object):
    payload = {"v": value}
    bson_payload = bson.dumps(payload)
    return base64.b64encode(bson_payload).decode("ascii")


def compare_data(received, expected):
    # Helper function to compare received and expected data
    if isinstance(received, list) and isinstance(expected, list):
        return all(compare_data(r, e) for r, e in zip(received, expected))

    if isinstance(received, bytes) and isinstance(expected, bytes):
        return received == expected

    if isinstance(received, datetime) and isinstance(expected, datetime):
        return received == expected

    return received == expected
