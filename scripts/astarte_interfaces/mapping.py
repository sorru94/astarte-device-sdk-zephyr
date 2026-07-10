# (C) Copyright 2026, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

# python -m pylint --rcfile=./scripts/.pylintrc ./scripts/astarte_interfaces/*.py
# python -m black --line-length 100 ./scripts/astarte_interfaces/*.py
# mypy ./scripts/astarte_interfaces/

from __future__ import annotations

import builtins
import re
from datetime import datetime
from typing import List, Union, Any, NamedTuple

# Astarte Types definition
IntList = List[int]
FloatList = List[float]
StringList = List[str]
BsonList = List[bytes]
BoolList = List[bool]
DatetimeList = List[datetime]
MapType = Union[
    int,
    float,
    str,
    bytes,
    bool,
    datetime,
    IntList,
    FloatList,
    StringList,
    BsonList,
    BoolList,
    DatetimeList,
]


class AstarteTypesLookupElement(NamedTuple):
    """Lookup table element for mapping Astarte type to Python type"""

    type: builtins.type[Any]
    subtype: builtins.type[Any] | None


astarte_types_lookup: dict[str, AstarteTypesLookupElement] = {
    "integer": AstarteTypesLookupElement(int, None),
    "longinteger": AstarteTypesLookupElement(int, None),
    "double": AstarteTypesLookupElement(float, None),
    "string": AstarteTypesLookupElement(str, None),
    "binaryblob": AstarteTypesLookupElement(bytes, None),
    "boolean": AstarteTypesLookupElement(bool, None),
    "datetime": AstarteTypesLookupElement(datetime, None),
    "integerarray": AstarteTypesLookupElement(list, int),
    "longintegerarray": AstarteTypesLookupElement(list, int),
    "doublearray": AstarteTypesLookupElement(list, float),
    "stringarray": AstarteTypesLookupElement(list, str),
    "binaryblobarray": AstarteTypesLookupElement(list, bytes),
    "booleanarray": AstarteTypesLookupElement(list, bool),
    "datetimearray": AstarteTypesLookupElement(list, datetime),
}

# Mapping quality of service
QOS_MAP: dict[str, int] = {"unreliable": 0, "guaranteed": 1, "unique": 2}

endpoint_regex = re.compile(r"^(\/(%{([a-zA-Z_]+[a-zA-Z0-9_]*)}|[a-zA-Z_]+[a-zA-Z0-9_]*)){1,64}$")


class Mapping:
    """
    Class that represent a data Mapping
    Mappings are designed around REST controller semantics: each mapping describes an endpoint
    which is resolved to a path, it is strongly typed, and can have additional options. Just like
    in REST controllers, Endpoints can be parametrized to build REST-like collection and trees.
    Parameters are identified by %{parameterName}, with each endpoint supporting any number of
    parameters (see
    `Limitations <https://docs.astarte-platform.org/snapshot/030-interface.html#limitations>`_).

    Attributes
    ----------
    endpoint: str
        Path of the Mapping
    type: str
        Type of the Mapping (see notes)
    reliability:
        Reliability level of the Mapping (see notes)
    explicit_timestamp: bool
        Flag that defines if the Mapping requires a timestamp associated to the Payload before send.
    retention:
        Retention policy for datastreams (see notes)
    expiry:
        Expiry policy for datastreams with retention "stored" (see notes)
    allow_unset:
        Allow unsetting for properties

    Notes
    -----
        **Supported data types**

        The following types are supported:

        * double: A double-precision floating-point number as specified by binary64, by the IEEE
            754 standard (NaNs and other non-numerical values are not supported).
        * integer: A signed 32 bit integer.
        * boolean: Either true or false, adhering to JSON boolean type.
        * longinteger: A signed 64-bit integer (please note that longinteger is represented as a
            string by default in JSON-based APIs.).
        * string: An UTF-8 string, at most 65536 bytes long.
        * binaryblob: An arbitrary sequence of any byte that should be shorter than 64 KiB. (
            binaryblob is represented as a base64 string by default in JSON-based APIs.).
        * datetime: A UTC timestamp, internally represented as milliseconds since 1st Jan 1970
            using a signed 64 bits integer. (datetime is represented as an ISO 8601 string by
            default in JSON based APIs.)
        * doublearray, integerarray, booleanarray, longintegerarray, stringarray,
            binaryblobarray, datetimearray: A list of values, represented as a JSON Array.
            Arrays can have up to 1024 items and each item must respect the limits of its scalar
            type (i.e. each string in a stringarray must be at most 65535 bytes long, each binary
            blob in a binaryblobarray must be shorter than 64 KiB.)

        **Quality of Service**

        Data messages QoS is chosen according to mapping settings, such as reliability.
        Properties are always published using QoS 2.

        ============== ============== ===
        INTERFACE TYPE RELIABILITY    QOS
        ============== ============== ===
        properties     always unique	2
        datastream	   unreliable	    0
        datastream	   guaranteed	    1
        datastream	   unique	        2
        ============== ============== ===

        **Retention and Expiry**

        Defines whether the sent data should be discarded if the transport is temporarily uncapable
        of delivering it (discard) or should be kept in a cache in memory (volatile) or on disk
        (stored), and guaranteed to be delivered in the timeframe defined by the expiry.
    """

    def __init__(self, mapping_definition: dict[str, Any], is_datastream: bool):
        """
        Parameters
        ----------
        mapping_definition: dict[str, Any]
            Mapping from the mappings array of an Astarte Interface definition in the form of a
            Python dictionary. Usually obtained by using json.loads() on an Interface file.
        is_datastream: bool
            True when the mapping belongs to a datastream interface, false otherwise.
        """
        endpoint_val = mapping_definition.get("endpoint")
        if not isinstance(endpoint_val, str):
            raise ValueError("endpoint is a required mapping field and should be a string.")
        if endpoint_regex.match(endpoint_val) is None:
            raise ValueError(f"The following endpoint is not correctly formatted {endpoint_val}.")
        self.endpoint: str = endpoint_val

        type_val = mapping_definition.get("type")
        if not isinstance(type_val, str) or type_val not in astarte_types_lookup:
            raise ValueError(
                "type is a required mapping field and should match one of the allowed types."
            )
        self.type: str = type_val

        if is_datastream:
            reliability_str = mapping_definition.get("reliability", "unreliable")
            if not isinstance(reliability_str, str):
                raise ValueError("reliability field must be a string.")

            reliability_val = QOS_MAP.get(reliability_str)
            if reliability_val is None:
                raise ValueError(
                    "reliability must be one of: 'unreliable', 'guaranteed' or 'unique'."
                )
            self.reliability: int = reliability_val

            explicit_timestamp_val = mapping_definition.get("explicit_timestamp", False)
            if not isinstance(explicit_timestamp_val, bool):
                raise ValueError("Explicit timestamp should have a boolean value.")
            self.explicit_timestamp: bool = explicit_timestamp_val

            retention_val = mapping_definition.get("retention", "discard")
            if not isinstance(retention_val, str) or retention_val not in (
                "discard",
                "volatile",
                "stored",
            ):
                raise ValueError("retention must be one of: 'discard', 'volatile' or 'stored'.")
            self.retention: str = retention_val

            if self.retention != "stored" and ("expiry" in mapping_definition):
                raise ValueError(
                    "Expiry is only meaningful for datastreams with retention 'stored'."
                )

            expiry_val = mapping_definition.get("expiry", 0)
            if not isinstance(expiry_val, int) or expiry_val < 0:
                raise ValueError("Expiry should be a positive integer.")
            self.expiry: int = expiry_val

            if "allow_unset" in mapping_definition:
                raise ValueError("Field 'allow_unset' has no meaning for datastreams.")

        else:
            invalid_prop_keys = ("explicit_timestamp", "reliability", "retention", "expiry")
            if any(k in mapping_definition for k in invalid_prop_keys):
                raise ValueError(f"Fields {invalid_prop_keys} have no meaning for properties.")

            allow_unset_val = mapping_definition.get("allow_unset", False)
            if not isinstance(allow_unset_val, bool):
                raise ValueError("Allow unset should have a boolean value.")
            self.allow_unset: bool = allow_unset_val
