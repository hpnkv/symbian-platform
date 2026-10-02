"""Serializable application preferences and resolved SDK dependencies."""

from pathlib import Path
from typing import Literal

from pydantic import BaseModel, ConfigDict, Field, ValidationError

from symbian.project.sdk import AppSdk
from symbian.status import Code, StatusError


class Preferences(BaseModel):
    """User selections shared by the wizard and deterministic generation."""

    model_config = ConfigDict(extra="forbid", frozen=True)

    name: str = Field(pattern=r"^[a-z][a-z0-9_]{0,47}$")
    uid3: int = Field(ge=0xE0000000, le=0xEFFFFFFF)
    architecture: Literal["armv5t", "armv6"] = "armv6"
    # Projects made before the stackless profile remain on their chosen
    # portable runtime when their generated configuration is refreshed.
    timer_tasks: bool = False
    ide: Literal["intellij", "none"] = "intellij"
    port: int = Field(default=24690, ge=1024, le=65535)


class ProjectConfiguration(BaseModel):
    """Settings saved with each generated application project."""

    model_config = ConfigDict(extra="forbid", frozen=True)

    preferences: Preferences
    sdk_location: Path = Field(alias="sdk")

    @property
    def sdk(self) -> AppSdk:
        """Loads the SDK from the local setting's resolved directory."""
        return AppSdk.load(self.sdk_location / "sdk.json")

    @classmethod
    def load(cls, project: Path) -> "ProjectConfiguration":
        """Loads project settings with canonical validation failures."""
        try:
            import json

            settings = json.loads(
                (project / "symbian-project.json").read_text()
            )
            location = project / "sdk-location.json"
            if location.exists():
                prefix = Path(json.loads(location.read_text())["sdk"])
            elif isinstance(settings.get("sdk"), dict):
                prefix = Path(settings["sdk"]["prefix"])
            else:
                prefix = Path(settings["sdk"])
            if not prefix.is_absolute():
                prefix = (project / prefix).resolve()
            if "architecture" not in settings["preferences"]:
                # Old generated projects explicitly selected the v5T toolchain.
                settings["preferences"]["architecture"] = "armv5t"
            settings["sdk"] = str(prefix)
            return cls.model_validate(settings)
        except (
            ValidationError,
            UnicodeError,
            ValueError,
            KeyError,
            TypeError,
        ) as error:
            raise StatusError(Code.INVALID_ARGUMENT, str(error)) from error
