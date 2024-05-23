# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

"""clean.py

West extension that can be used to clean temporary build artifacts.

Checked using pylint with the following command (pip install pylint):
python -m pylint --rcfile=./scripts/.pylintrc ./scripts/*.py
Formatted using black with the following command (pip install black):
python -m black --line-length 100 ./scripts/*.py
"""
import shutil
from pathlib import Path

from colored import fore, stylize
from west import log  # use this for user output
from west.commands import WestCommand  # your extension must subclass this

# from west import log  # use this for user output

static_name = "clean"
static_help = "Clean build artifacts"
static_description = """Convenience wrapper to clean temporary build artifacts."""


class WestCommandClean(WestCommand):
    """Extension of the WestCommand class, specific for this command."""

    def __init__(self):
        super().__init__(static_name, static_help, static_description)

    def do_add_parser(self, parser_adder):
        """
        This function can be used to add custom options for this command.

        Allows you full control over the type of argparse handling you want.

        Parameters
        ----------
        parser_adder : Any
            Parser adder generated by a call to `argparse.ArgumentParser.add_subparsers()`.

        Returns
        -------
        argparse.ArgumentParser
            The argument parser for this command.
        """
        parser = parser_adder.add_parser(self.name, help=self.help, description=self.description)

        return parser  # gets stored as self.parser

    # pylint: disable-next=arguments-renamed,unused-argument
    def do_run(self, args, unknown_args):
        """
        Function called when the user runs the custom command, e.g.:

          $ west clean

        Parameters
        ----------
        args : Any
            Arguments pre parsed by the parser defined by `do_add_parser()`.
        unknown_args : Any
            Extra unknown arguments.
        """
        workspace_path = Path(self.topdir)
        module_path = workspace_path.joinpath("astarte-device-sdk-zephyr")
        build_dirs = (
            [
                workspace_path.joinpath("build"),
                module_path.joinpath("build"),
                module_path.joinpath("doc").joinpath("_build"),
            ]
            + list(Path(workspace_path).glob("twister-out*"))
            + list(Path(module_path).glob("twister-out*"))
        )
        for build_dir in build_dirs:
            if build_dir.is_dir():
                log.inf(stylize(f"Removing directory: {build_dir}", fore("cyan")))
                shutil.rmtree(build_dir)
