"""Compile portable application-menu assets with the selected SDK."""

import hashlib
import re
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

from symbian.native import require_native
from symbian.process import run
from symbian.project.configuration import ProjectConfiguration
from symbian.project.sdk import AppSdk, discover_sdk
from symbian.status import Code, StatusError

# e32lang.h TLanguage IDs; tags are BCP 47 spellings.
_LANGUAGES = {
    "en": 1,
    "fr": 2,
    "de": 3,
    "es": 4,
    "it": 5,
    "sv": 6,
    "da": 7,
    "no": 8,
    "fi": 9,
    "en-US": 10,
    "fr-CH": 11,
    "de-CH": 12,
    "pt": 13,
    "tr": 14,
    "is": 15,
    "ru": 16,
    "hu": 17,
    "nl": 18,
    "nl-BE": 19,
    "en-AU": 20,
    "fr-BE": 21,
    "de-AT": 22,
    "en-NZ": 23,
    "cs": 25,
    "sk": 26,
    "pl": 27,
    "sl": 28,
    "zh-TW": 29,
    "zh-HK": 30,
    "zh-CN": 31,
    "ja": 32,
    "th": 33,
}


def _texts(table: object, field: str) -> tuple[str, str]:
    if not isinstance(table, dict) or set(table) != {
        "caption",
        "short_caption",
    }:
        raise StatusError(
            Code.INVALID_ARGUMENT, f"{field} requires caption and short_caption"
        )
    result = []
    for key in ("caption", "short_caption"):
        value = table[key]
        if not isinstance(value, str) or not 1 <= len(value) <= 64:
            raise StatusError(
                Code.INVALID_ARGUMENT, f"{field}.{key}: 1..64 characters"
            )
        if any(not c.isprintable() or c in {'"', "\\"} for c in value):
            raise StatusError(
                Code.INVALID_ARGUMENT,
                f"{field}.{key}: printable text without quotes or backslashes",
            )
        result.append(value)
    return result[0], result[1]


def _resource(sdk: AppSdk, root: Path, kind: str, source: str) -> bytes:
    rss = root / f"{kind}.rss"
    rpp = root / f"{kind}.rpp"
    rsc = root / f"{kind}.rsc"
    rsg = root / f"{kind}.rsg"
    rss.write_text(source, encoding="utf-8")
    run(
        [
            str(sdk.c_compiler or sdk.compiler),
            "-E",
            "-P",
            "-x",
            "c",
            "-I",
            str(sdk.prefix / "include/platform"),
            str(rss),
            "-o",
            str(rpp),
        ],
        cwd=root,
    )
    run(
        [
            str(sdk.prefix / "bin/rcomp"),
            "-u",
            f"-s{rpp}",
            f"-o{rsc}",
            f"-h{rsg}",
        ],
        cwd=root,
    )
    return rsc.read_bytes()


def compile_registration(
    project: Path, application: dict, executable_name: str, uid3: int
) -> tuple[list[tuple[str, bytes]], dict[str, str]]:
    """Compile registration, locale resources and optional project SVG icon.

    Args:
        project: Project root.
        application: Parsed application TOML table.
        executable_name: Package executable filename.
        uid3: Executable UID3.

    Returns:
        Ordered SIS targets and bytes, plus source asset SHA-256 digests.
    """
    fields = {"caption", "short_caption", "icon", "localizations"}
    if (
        not isinstance(application, dict)
        or not {"caption", "short_caption"} <= set(application)
        or set(application) - fields
    ):
        raise StatusError(Code.INVALID_ARGUMENT, "Invalid [application] fields")
    fallback = _texts(
        {key: application[key] for key in ("caption", "short_caption")},
        "application",
    )
    localization = application.get("localizations", {})
    if not isinstance(localization, dict) or any(
        tag not in _LANGUAGES for tag in localization
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Unsupported application locale"
        )
    locales = sorted(
        (
            (_LANGUAGES[tag], _texts(value, f"application.localizations.{tag}"))
            for tag, value in localization.items()
        ),
        key=lambda item: item[0],
    )
    stem = Path(executable_name).stem
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]{0,59}", stem):
        raise StatusError(Code.INVALID_ARGUMENT, "Invalid executable name")
    sdk = (
        ProjectConfiguration.load(project).sdk
        if (project / "sdk-location.json").is_file()
        else AppSdk.load(discover_sdk())
    )
    if (
        not (sdk.prefix / "bin/rcomp").is_file()
        or not (sdk.prefix / "include/platform/AppInfo.rh").is_file()
    ):
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "Selected SDK lacks resource tools; export an updated SDK",
        )
    hashes: dict[str, str] = {}
    icon_bytes = None
    icon = application.get("icon")
    if icon is not None:
        if not isinstance(icon, str) or not icon or Path(icon).is_absolute():
            raise StatusError(
                Code.INVALID_ARGUMENT,
                "application.icon must be a project-relative SVG path",
            )
        icon_path = (project / icon).resolve()
        if (
            not icon_path.is_relative_to(project.resolve())
            or not icon_path.is_file()
        ):
            raise StatusError(
                Code.INVALID_ARGUMENT, "Icon is missing or outside project"
            )
        if icon_path.suffix.lower() != ".svg":
            raise StatusError(Code.INVALID_ARGUMENT, "Icon must be SVG")
        with icon_path.open("rb") as stream:
            icon_bytes = stream.read(1024 * 1024 + 1)
        if len(icon_bytes) > 1024 * 1024:
            raise StatusError(Code.RESOURCE_EXHAUSTED, "SVG icon exceeds 1 MiB")
        try:
            root = ET.fromstring(icon_bytes)
        except ET.ParseError as error:
            raise StatusError(
                Code.INVALID_ARGUMENT, "Invalid SVG icon XML"
            ) from error
        if root.tag != "{http://www.w3.org/2000/svg}svg":
            raise StatusError(Code.INVALID_ARGUMENT, "Icon root must be SVG")
        hashes[icon] = hashlib.sha256(icon_bytes).hexdigest()
    assets = []
    with tempfile.TemporaryDirectory(prefix="symbian-registration-") as temp:
        root = Path(temp)
        registration = (
            "CHARACTER_SET UTF8\n"
            "#include <AppInfo.rh>\n"
            "UID2 KUidAppRegistrationResourceFile\n"
            f"UID3 0x{uid3:08X}\n"
            "RESOURCE APP_REGISTRATION_INFO\n"
            "{\n"
            f'  app_file = "{stem}";\n'
            '  localisable_resource_file = "\\\\resource\\\\apps\\\\'
            f'{stem}_loc";\n'
            "}\n"
        )
        assets.append(
            (
                f"!:\\private\\10003a3f\\import\\apps\\{stem}_reg.rsc",
                _resource(sdk, root, "reg", registration),
            )
        )
        for number, (caption, short_caption) in [(0, fallback), *locales]:
            icon_line = (
                f'    icon_file = "\\\\resource\\\\apps\\\\{stem}.mif";\n'
                if icon_bytes is not None
                else ""
            )
            source = (
                "CHARACTER_SET UTF8\n"
                "#include <AppInfo.rh>\n"
                "RESOURCE LOCALISABLE_APP_INFO\n"
                "{\n"
                f'  short_caption = "{short_caption}";\n'
                "  caption_and_icon = CAPTION_AND_ICON_INFO\n"
                "  {\n"
                f'    caption = "{caption}";\n'
                "    number_of_icons = 0;\n"
                f"{icon_line}"
                "  };\n"
                "}\n"
            )
            extension = "rsc" if number == 0 else f"r{number:02d}"
            assets.append(
                (
                    f"!:\\resource\\apps\\{stem}_loc.{extension}",
                    _resource(sdk, root, f"loc{number:02d}", source),
                )
            )
    if icon_bytes is not None:
        assets.append(
            (
                f"!:\\resource\\apps\\{stem}.mif",
                require_native().build_svg_mif(icon_bytes),
            )
        )
    return assets, hashes
