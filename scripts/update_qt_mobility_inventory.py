"""Derives Qt Mobility's public SDK surface from original qmake manifests."""

import argparse
import hashlib
import json
import re
from pathlib import Path

MODULES = {
    "Bearer": "bearer",
    "Contacts": "contacts",
    "Location": "location",
    "Messaging": "messaging",
    "MultimediaKit": "multimedia",
    "PublishSubscribe": "publishsubscribe",
    "Sensors": "sensors",
    "ServiceFramework": "serviceframework",
    "SystemInfo": "systeminfo",
    "Versit": "versit",
}

DEPENDENCIES = {
    "Bearer": ["QtGui", "QtNetwork"],
    "Contacts": ["QtGui"],
    "Location": ["QtCore"],
    "Messaging": ["QtGui"],
    "MultimediaKit": ["QtGui", "QtNetwork"],
    "PublishSubscribe": ["QtCore"],
    "Sensors": ["QtGui"],
    "ServiceFramework": ["QtSql"],
    "SystemInfo": ["QtGui", "QtNetwork"],
    "Versit": ["QtGui", "QtMobilityContacts"],
}

# Original public headers with missing direct includes. Preserve their bytes;
# the canary supplies the named public prerequisite explicitly.
HEADER_PREINCLUDES = {
    "QtContacts/qcontactringtone.h": ["QUrl"],
    "QtMessaging/qmessagedatacomparator.h": ["qmobilityglobal.h"],
}


def digest(path: Path) -> str:
    """Hashes one preserved source input."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _assignment_tokens(lines: list[str], variable: str) -> list[str]:
    result = []
    for index, line in enumerate(lines):
        if not re.search(rf"\b{variable}\s*\+?=", line):
            continue
        chunk = line.split("=", 1)[1]
        while chunk.rstrip().endswith("\\") and index + 1 < len(lines):
            index += 1
            chunk = chunk.rstrip()[:-1] + " " + lines[index]
        result.extend(re.findall(r"(?<![\w/])([\w/.-]+\.h)\b", chunk))
    return result


def public_headers(source: Path, directory: str) -> list[str]:
    """Reads only qmake PUBLIC_HEADERS, including included module .pri files."""
    module = source / "src" / directory
    headers = []
    for manifest in sorted(module.rglob("*")):
        if manifest.suffix not in (".pro", ".pri") or any(
            part in ("tests", "tsrc", "plugins") for part in manifest.parts
        ):
            continue
        headers.extend(
            _assignment_tokens(
                manifest.read_text(errors="replace").splitlines(),
                "PUBLIC_HEADERS",
            )
        )
    if directory == "sensors":
        lines = (module / "sensors.pro").read_text().splitlines()
        sensor_list = []
        for index, line in enumerate(lines):
            if not re.search(r"^SENSORS\s*=", line):
                continue
            chunk = line.split("=", 1)[1]
            while chunk.rstrip().endswith("\\") and index + 1 < len(lines):
                index += 1
                chunk = chunk.rstrip()[:-1] + " " + lines[index]
            sensor_list = re.findall(r"\bq[a-z]+\b", chunk)
            break
        headers.extend(f"{name}.h" for name in sensor_list)
    result = sorted(set(headers))
    if not result or any(not (module / path).is_file() for path in result):
        raise ValueError(f"Unresolved public Qt Mobility headers: {directory}")
    names = [Path(path).name.casefold() for path in result]
    if len(names) != len(set(names)):
        raise ValueError(
            f"Ambiguous flattened Qt Mobility headers: {directory}"
        )
    return result


def public_aliases(
    source: Path, directory: str, headers: list[str]
) -> list[dict]:
    """Reproduces bin/syncheaders aliases over reviewed PUBLIC_HEADERS only."""
    patterns = (
        re.compile(r"^\s*class\s+.*_EXPORT\s+([^\s:]+)"),
        re.compile(r"^\s*class\s+([^\s:<]+)"),
        re.compile(r"syncqtopia\s+header\s+([^\s:]+)"),
    )
    aliases = {}
    for header in headers:
        for line in (
            (source / "src" / directory / header).read_text().splitlines()
        ):
            for index, pattern in enumerate(patterns):
                match = pattern.search(line)
                if not match or (index == 1 and ";" in line):
                    continue
                name = match.group(1)
                if "#" not in name and not name.endswith("Private"):
                    previous = aliases.setdefault(name, Path(header).name)
                    if previous != Path(header).name:
                        raise ValueError(f"Ambiguous Qt Mobility alias: {name}")
                break
    return [
        {
            "include": f"Qt{directory_to_module(directory)}/{name}",
            "header": header,
        }
        for name, header in sorted(aliases.items())
    ]


def directory_to_module(directory: str) -> str:
    """Returns the original library spelling for a qmake source directory."""
    return next(
        name for name, source_dir in MODULES.items() if source_dir == directory
    )


def generate(workspace: Path) -> dict:
    """Connects frozen library definitions and public headers to targets."""
    source = workspace / "research/upstream/qtmobility"
    if (
        "VERSION = 1.0.3"
        not in (source / "src/s60installs/s60installs.pro").read_text()
    ):
        raise ValueError("Qt Mobility source release is not 1.0.3")
    modules = []
    for name, directory in MODULES.items():
        definition = source / f"src/s60installs/eabi/Qt{name}u.def"
        if not definition.is_file():
            raise ValueError(f"Missing frozen Qt Mobility definition: {name}")
        headers = public_headers(source, directory)
        modules.append(
            {
                "target": f"Symbian::QtMobility{name}",
                "dll": f"qt{name.lower()}.dll",
                "definition": str(definition.relative_to(source)),
                "definition_sha256": digest(definition),
                "public_header_manifests": sorted(
                    str(path.relative_to(source))
                    for path in (source / "src" / directory).rglob("*")
                    if path.suffix in (".pro", ".pri")
                    and "PUBLIC_HEADERS" in path.read_text(errors="replace")
                ),
                "headers": [
                    {
                        "source": f"src/{directory}/{path}",
                        "include": f"Qt{name}/{Path(path).name}",
                        "sha256": digest(source / "src" / directory / path),
                        **(
                            {
                                "preinclude": HEADER_PREINCLUDES[
                                    f"Qt{name}/{Path(path).name}"
                                ]
                            }
                            if f"Qt{name}/{Path(path).name}"
                            in HEADER_PREINCLUDES
                            else {}
                        ),
                    }
                    for path in headers
                ],
                "aliases": public_aliases(source, directory, headers),
                "dependencies": [
                    f"Symbian::{item}" for item in DEPENDENCIES[name]
                ],
                "classification": "public_standard_extension",
                "availability": "unknown_firmware",
            }
        )
    return {
        "schema": "symbian.qt-mobility/v1",
        "repository": "SymbianSource/oss.FCL.sf.mw.qtmobility",
        "revision": "a462a7d9493bb0bfb809c09d5c331498b47a2cd6",
        "version": "1.0.3",
        "licenses": ["LICENSE.LGPL", "LGPL_EXCEPTION.txt"],
        "packaging_manifest": "src/s60installs/s60installs.pro",
        "firmware_compatibility": (
            "Unknown; historical inclusion does not establish Belle FP2 "
            "version equivalence"
        ),
        "global_header": {
            "source": "src/global/qmobilityglobal.h",
            "include": "qmobilityglobal.h",
            "sha256": digest(source / "src/global/qmobilityglobal.h"),
        },
        "modules": modules,
    }


def main() -> None:
    """Writes or checks the reviewed source-backed inventory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, default=Path.cwd())
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    data = json.dumps(generate(args.workspace.resolve()), indent=2) + "\n"
    output = args.workspace / "research/native-sdk/qt-mobility.json"
    if args.check:
        if output.read_text() != data:
            raise SystemExit("Qt Mobility inventory is stale")
    else:
        output.write_text(data)


if __name__ == "__main__":
    main()
