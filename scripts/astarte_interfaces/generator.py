# (C) Copyright 2026, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

# python -m pylint --rcfile=./scripts/.pylintrc ./scripts/astarte_interfaces/*.py
# python -m black --line-length 100 ./scripts/astarte_interfaces/*.py
# mypy ./scripts/astarte_interfaces/

import json
import os
import sys
from pathlib import Path
from string import Template

from colored import fore, stylize
from west import log

from .interface import Interface

reliability_lookup = {
    0: "UNRELIABLE",
    1: "GUARANTEED",
    2: "UNIQUE",
}

interface_header_template = Template(r"""/**
 * @file ${output_filename}.h
 * @brief Contains automatically generated interfaces.
 *
 * @warning Do not modify this file manually.
 *
 * @details The generated structures contain all information regarding each interface.
 * and are automatically generated from the json interfaces definitions.
 */

// clang-format off

// NOLINTNEXTLINE This guard is clear enough.
#ifndef ${output_filename_cap}_H
#define ${output_filename_cap}_H

#include <astarte_device_sdk/interface.h>
#include <astarte_device_sdk/mapping.h>

// Interface names should resemble as closely as possible their respective .json file names.
// NOLINTBEGIN(readability-identifier-naming)
${interfaces_declarations}
// NOLINTEND(readability-identifier-naming)

#endif /* ${output_filename_cap}_H */
""")

interface_source_template = Template(r"""/**
 * @file ${output_filename}.c
 * @brief Contains automatically generated interfaces.
 *
 * @warning Do not modify this file manually.
 */

// NOLINENUMBERLINT

// clang-format off

#include "${output_filename}.h"

// Interface names should resemble as closely as possible their respective .json file names.
// NOLINTBEGIN(readability-identifier-naming)
${interfaces_declarations}

// NOLINTEND(readability-identifier-naming)
""")

interface_declaration_template = Template(
    r"""/** @brief Automatically generated interface declaration. */
extern const astarte_interface_t ${interface_name_sc};"""
)

interface_definition_template = Template(r"""
/** @brief Automatically generated mapping definition. */
static const astarte_mapping_t ${interface_name_sc}_mappings[${mappings_number}] = {
${mappings}
};

/** @brief Automatically generated interface definition. */
const astarte_interface_t ${interface_name_sc} = {
    .name = "${interface_name}",
    .major_version = ${version_major},
    .minor_version = ${version_minor},
    .type = ${type},
    .ownership = ${ownership},
    .aggregation = ${aggregation},
    .mappings = ${interface_name_sc}_mappings,
    .mappings_length = ${mappings_number}U,
};""")


def _build_mapping_struct(mapping) -> str:
    """
    Helper to dynamically construct fields for a Mapping instance.

    Parameters
    ----------
    mapping : Mapping
        The mapping instance to evaluate and extract fields from.

    Returns
    -------
    str
        A formatted C string representing the mapping struct definition.
    """
    mapping_fields = [
        f'.endpoint = "{mapping.endpoint}"',
        f".type = ASTARTE_MAPPING_TYPE_{mapping.type.upper()}",
    ]

    if hasattr(mapping, "reliability"):
        rel_str = reliability_lookup[mapping.reliability]
        mapping_fields.append(f".reliability = ASTARTE_MAPPING_RELIABILITY_{rel_str}")

    if hasattr(mapping, "explicit_timestamp"):
        exp_ts_str = "true" if mapping.explicit_timestamp else "false"
        mapping_fields.append(f".explicit_timestamp = {exp_ts_str}")

    if hasattr(mapping, "required"):
        req_str = "true" if mapping.required else "false"
        mapping_fields.append(f".required = {req_str}")

    if hasattr(mapping, "retention"):
        mapping_fields.append(f".retention = ASTARTE_MAPPING_RETENTION_{mapping.retention.upper()}")

    if hasattr(mapping, "expiry") and mapping.retention == "stored":
        mapping_fields.append(f".expiry = {mapping.expiry}")

    if hasattr(mapping, "allow_unset"):
        allow_uns_str = "true" if mapping.allow_unset else "false"
        mapping_fields.append(f".allow_unset = {allow_uns_str}")

    fields_str = ",\n        ".join(mapping_fields)
    return f"    {{\n        {fields_str},\n    }},"


def _handle_file_output(output_dir: Path, output_fn: str, check: bool, header: str, source: str):
    """
    Helper to write or check output files.

    Parameters
    ----------
    output_dir : Path
        Folder where the generated files will be saved or checked.
    output_fn : str
        Output file name without extension.
    check : bool
        Check if previously generated interfaces are up to date.
    header : str
        The generated C header content to write or compare against.
    source : str
        The generated C source content to write or compare against.
    """
    generated_header = output_dir.joinpath(f"{output_fn}.h")
    generated_source = output_dir.joinpath(f"{output_fn}.c")

    if check:
        if not output_dir.exists():
            log.err(stylize("Check failed: non existant output directory", fore("yellow")))
            sys.exit(1)

        with open(generated_header, "r", encoding="utf-8") as generated_fp:
            if generated_fp.read() != header:
                log.err(stylize("Check failed: header is not up to date", fore("yellow")))
                sys.exit(1)

        with open(generated_source, "r", encoding="utf-8") as generated_fp:
            if generated_fp.read() != source:
                log.err(stylize("Check failed: source is not up to date", fore("yellow")))
                sys.exit(1)
    else:
        if not output_dir.exists():
            os.makedirs(output_dir)

        with open(generated_header, "w", encoding="utf-8") as generated_fp:
            generated_fp.write(header)

        with open(generated_source, "w", encoding="utf-8") as generated_fp:
            generated_fp.write(source)


def generate_interfaces(interfaces_dir: Path, output_dir: Path, output_fn: str, check: bool):
    """
    Generate the C files defining a set of interfaces starting from .json definitions.

    Parameters
    ----------
    interfaces_dir : Path
        Folder in which to search for .json files.
    output_dir : Path
        Folder where the generated files will be saved.
    output_fn : str
        Output file name without extension.
    check : bool
        Check if previously generated interfaces are up to date.
    """

    interfaces_declarations = []
    interfaces_structs = []
    for interface_file in sorted([i for i in interfaces_dir.iterdir() if i.suffix == ".json"]):
        with open(interface_file, "r", encoding="utf-8") as interface_fp:
            interface_json = json.load(interface_fp)
            interface = Interface(interface_json)

            mappings_struct = [_build_mapping_struct(m) for m in interface.mappings.values()]

            itype = "ASTARTE_INTERFACE_" + (
                "TYPE_PROPERTIES" if interface.is_type_properties() else "TYPE_DATASTREAM"
            )
            iownership = "ASTARTE_INTERFACE_" + (
                "OWNERSHIP_SERVER" if interface.is_server_owned() else "OWNERSHIP_DEVICE"
            )
            iaggregation = "ASTARTE_INTERFACE_" + (
                "AGGREGATION_OBJECT"
                if interface.is_aggregation_object()
                else "AGGREGATION_INDIVIDUAL"
            )
            interface_struct = interface_definition_template.substitute(
                mappings_number=len(interface.mappings),
                interface_name_sc=interface.name.replace(".", "_").replace("-", "_"),
                interface_name=interface.name,
                version_major=interface.version_major,
                version_minor=interface.version_minor,
                type=itype,
                ownership=iownership,
                aggregation=iaggregation,
                mappings="\n".join(mappings_struct),
            )
            interfaces_structs.append(interface_struct)

            interface_declaration = interface_declaration_template.substitute(
                interface_name_sc=interface.name.replace(".", "_").replace("-", "_")
            )
            interfaces_declarations.append(interface_declaration)

    interfaces_header = interface_header_template.substitute(
        output_filename=output_fn,
        output_filename_cap=output_fn.upper(),
        interfaces_declarations="\n".join(interfaces_declarations),
    )

    interfaces_source = interface_source_template.substitute(
        output_filename=output_fn, interfaces_declarations="\n".join(interfaces_structs)
    )

    _handle_file_output(output_dir, output_fn, check, interfaces_header, interfaces_source)
