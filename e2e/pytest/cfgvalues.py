# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

from typing import Optional
import os
from pathlib import Path

from dotenv import dotenv_values


class MissingConfigError(Exception):
    """Custom exception for missing configuration values."""

    def __init__(self, key: str) -> None:
        super().__init__(f"Missing required configuration value for key: {key}")


class CfgValues:
    """
    Test configuration class. Imports the needed configuration values from the .config build file
    """

    def __init__(self, config_file_path: str) -> None:
        prj_config: dict[str, Optional[str]] = dotenv_values(config_file_path)

        self.realm: str = self._get_config_value(prj_config, "CONFIG_ASTARTE_DEVICE_SDK_REALM_NAME")
        self.device_id: str = self._get_config_value(prj_config, "CONFIG_DEVICE_ID")
        self.appengine_url: str = self._get_config_value(prj_config, "CONFIG_E2E_APPENGINE_URL")
        self.appengine_token: str = self._get_config_value(prj_config, "CONFIG_E2E_APPENGINE_TOKEN")
        self.appengine_cert: Path = Path(os.path.abspath(__file__)).parent.parent.joinpath(
            self._get_config_value(prj_config, "CONFIG_TLS_CERTIFICATE_PATH")
        )

    @staticmethod
    def _get_config_value(config: dict[str, Optional[str]], key: str) -> str:
        value = config.get(key)

        if value is None or not value:
            raise MissingConfigError(key)
        return value
