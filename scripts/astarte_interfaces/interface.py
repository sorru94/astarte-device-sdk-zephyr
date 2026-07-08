# (C) Copyright 2026, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

# python -m pylint --rcfile=./scripts/.pylintrc ./scripts/astarte_interfaces/*.py
# python -m black --line-length 100 ./scripts/astarte_interfaces/*.py
# mypy ./scripts/astarte_interfaces/

from __future__ import annotations

import re
from typing import Any

from .mapping import Mapping

OWNERSHIP_DEVICE = "device"
OWNERSHIP_SERVER = "server"

name_regex = re.compile(
    r"^([a-zA-Z][a-zA-Z0-9]*\.([a-zA-Z0-9][a-zA-Z0-9-]*\.)*)?[a-zA-Z][a-zA-Z0-9]*$"
)


class Interface:
    """
    Class that represents an Interface definition

    Interfaces are a core concept of Astarte which defines how data is exchanged between Astarte
    and its peers. They are not to be intended as OOP interfaces, but rather as the following
    definition:

    In Astarte each interface has an owner, can represent either a continuous data stream or a
    snapshot of a set of properties, and can be either aggregated into an object or be an
    independent set of individual members.

    Attributes
    ----------
        name: str
            Interface name
        version_major: int
            Interface version major number
        version_minor: int
            Interface version minor number
        type: str
            Interface type
        ownership: str
            Interface ownership
        aggregation: str
            Interface aggregation policy
        mappings: dict[str, Mapping]
            Interface mapping dictionary, keys are the endpoint of each mapping
    """

    def __init__(self, interface_definition: dict[str, Any]):
        """
        Parameters
        ----------
        interface_definition: dict
            An Astarte Interface definition in the form of a Python dictionary. Usually obtained
            by using json.loads on an Interface file.

        Raises
        ------
        ValueError
            if both version_major and version_minor numbers are set to 0,
            or if required fields are missing or invalid.
        """

        name = interface_definition.get("interface_name")
        if not isinstance(name, str):
            raise ValueError("Interface name is a required interface field and should be a string.")
        if name_regex.match(name) is None:
            raise ValueError(f"Interface name is not correctly formatted: {name}")
        self.name: str = name

        version_major = interface_definition.get("version_major")
        if not isinstance(version_major, int) or isinstance(version_major, bool):
            raise ValueError(
                "Major version is a required interface field and should be an integer."
            )
        self.version_major: int = version_major

        version_minor = interface_definition.get("version_minor")
        if not isinstance(version_minor, int) or isinstance(version_minor, bool):
            raise ValueError(
                "Minor version is a required interface field and should be an integer."
            )
        self.version_minor: int = version_minor

        if not self.version_major and not self.version_minor:
            raise ValueError(f"Both Major and Minor versions set to 0 for interface {self.name}")

        interface_type = interface_definition.get("type")
        if not isinstance(interface_type, str) or interface_type not in {
            "datastream",
            "properties",
        }:
            raise ValueError("Interface type can be one of 'datastream' and 'properties'.")
        self.type: str = interface_type

        ownership = interface_definition.get("ownership")
        if not isinstance(ownership, str) or ownership not in (OWNERSHIP_DEVICE, OWNERSHIP_SERVER):
            raise ValueError(
                f"Interface ownership can be one of '{OWNERSHIP_DEVICE}' and '{OWNERSHIP_SERVER}'."
            )
        self.ownership: str = ownership

        aggregation = interface_definition.get("aggregation", "individual")
        if not isinstance(aggregation, str) or aggregation not in {"individual", "object"}:
            raise ValueError(f"Invalid aggregation type for interface {self.name}.")
        self.aggregation: str = aggregation

        if self.type == "properties" and self.aggregation == "object":
            raise ValueError(
                "Invalid aggregation type 'object', properties can only be 'individual'."
            )

        if "mappings" not in interface_definition:
            raise ValueError(f"'mappings' field is required for interface {self.name}.")
        raw_mappings = interface_definition["mappings"]
        if not isinstance(raw_mappings, list) or not raw_mappings:
            raise ValueError(f"'mappings' must contain a non-empty list for interface {self.name}.")
        self.mappings: dict[str, Mapping] = {}
        for mapping_definition in raw_mappings:
            mapping = Mapping(mapping_definition, self.type == "datastream")
            if mapping.endpoint in self.mappings:
                raise ValueError(
                    f"Duplicated mapping {mapping.endpoint} for interface {self.name}."
                )
            self.mappings[mapping.endpoint] = mapping

        if self.aggregation == "object":
            obj_fields = [
                (m.explicit_timestamp, m.reliability, m.retention, m.expiry)
                for m in self.mappings.values()
            ]
            if len(set(obj_fields)) != 1:
                raise ValueError(
                    "All the optional mappings for objects should have the same "
                    "explicit_timestamp, reliability, retention, and expiry fields."
                )

    def is_aggregation_object(self) -> bool:
        """
        Check if the current Interface is a datastream with aggregation object
        Returns
        -------
        bool
            True if aggregation: object
        """
        return self.aggregation == "object"

    def is_server_owned(self) -> bool:
        """
        Check the Interface ownership
        Returns
        -------
        bool
            True if ownership: server
        """
        return self.ownership == OWNERSHIP_SERVER

    def is_type_properties(self) -> bool:
        """
        Check the Interface type
        Returns
        -------
        bool
            True if type: properties
        """
        return self.type == "properties"
