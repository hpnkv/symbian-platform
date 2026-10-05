"""CMake file API and Ninja orchestration for ARM application projects."""

import json
import re
from collections.abc import Mapping
from pathlib import Path

from pydantic import BaseModel, ConfigDict

from symbian.process import run
from symbian.status import Code, StatusError

TOOLCHAIN = Path(__file__).parent / "cmake/symbian-arm.cmake"


class Target(BaseModel):
    """Configured executable and the inputs declared by its CMake graph."""

    model_config = ConfigDict(frozen=True)

    artifact: Path
    inputs: frozenset[Path]
    compile_groups: list[dict]
    link_fragments: list[dict]


def _target(tree: Path, project: Path, name: str) -> Target:
    reply = tree / ".cmake/api/v1/reply"

    def read(filename: str) -> dict:
        if Path(filename).name != filename:
            raise ValueError("Invalid CMake reply filename")
        return json.loads((reply / filename).read_text(encoding="utf-8"))

    try:
        index = read(max(path.name for path in reply.glob("index-*.json")))
        if index["cmake"]["generator"]["name"] != "Ninja":
            raise StatusError(
                Code.FAILED_PRECONDITION, "ARM projects require Ninja"
            )
        replies = index["reply"]["client-symbian-platform"]
        model = read(replies["codemodel-v2"]["jsonFile"])
        files = read(replies["cmakeFiles-v1"]["jsonFile"])
        targets = model["configurations"][0]["targets"]
        matches = [target for target in targets if target["name"] == name]
        if len(matches) != 1:
            raise StatusError(
                Code.NOT_FOUND, f"CMake executable target not found: {name}"
            )
        target = read(matches[0]["jsonFile"])
        if target["type"] != "EXECUTABLE" or len(target["artifacts"]) != 1:
            raise StatusError(
                Code.FAILED_PRECONDITION, "Expected one executable artifact"
            )
        artifact = (tree / target["artifacts"][0]["path"]).resolve()
        if not artifact.is_relative_to(tree):
            raise StatusError(
                Code.FAILED_PRECONDITION, "Artifact must stay in the build tree"
            )
        inputs = {
            (project / entry["path"]).resolve()
            for entry in [*files["inputs"], *target["sources"]]
            if not entry.get("isGenerated") and not entry.get("isCMake")
        }
        inputs.update(
            (project / filename).resolve()
            for filename in ("symbian.toml", "CMakePresets.json")
        )
        for filename in ("sdk-location.json", "symbian-project.json"):
            if (project / filename).is_file():
                inputs.add((project / filename).resolve())
        user_presets = project / "CMakeUserPresets.json"
        if user_presets.is_file():
            inputs.add(user_presets.resolve())
        return Target(
            artifact=artifact,
            inputs=frozenset(inputs),
            compile_groups=target["compileGroups"],
            link_fragments=target["link"]["commandFragments"],
        )
    except (
        OSError,
        UnicodeError,
        json.JSONDecodeError,
        KeyError,
        TypeError,
        ValueError,
        IndexError,
    ) as error:
        raise StatusError(
            Code.DATA_LOSS, f"Invalid CMake file API reply: {error}"
        ) from error


def configure(
    project: Path,
    tree: Path,
    name: str,
    preset: str,
    cmake: str,
    ninja: str,
    compiler: str,
    linker: str,
    import_proxies: tuple[Path, ...] = (),
    architecture: str = "armv6",
    cmake_variables: Mapping[str, str] | None = None,
) -> Target:
    """Configures a Ninja tree and reads CMake's declared executable graph."""
    query = tree / ".cmake/api/v1/query/client-symbian-platform"
    query.mkdir(parents=True, exist_ok=True)
    for kind in ("codemodel-v2", "cmakeFiles-v1"):
        (query / kind).touch()
    fresh = []
    cache = tree / "CMakeCache.txt"
    ar = Path(compiler).parent / "llvm-ar"
    ranlib = Path(compiler).parent / "llvm-ranlib"
    if ar.is_file() != ranlib.is_file():
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "SDK must supply both llvm-ar and llvm-ranlib for ARM archives",
        )
    archive_tools = (
        {"ar": str(ar), "ranlib": str(ranlib)} if ar.is_file() else {}
    )
    variables = cmake_variables or {}
    if any(
        not re.fullmatch(r"[A-Z][A-Z0-9_]{0,63}", name)
        or not value
        or any(character in value for character in "\0\r\n;")
        for name, value in variables.items()
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Invalid CMake cache variables"
        )
    identity = json.dumps(
        {
            "architecture": architecture,
            "compiler": compiler,
            "compiler_version": run([compiler, "--version"], cwd=project),
            "linker": linker,
            "linker_version": run([linker, "--version"], cwd=project),
            "cmake_variables": dict(sorted(variables.items())),
            **archive_tools,
            **{
                f"{name}_version": run([path, "--version"], cwd=project)
                for name, path in archive_tools.items()
            },
        },
        sort_keys=True,
    )
    identity_path = tree / "symbian-toolchain.json"
    if cache.is_file():
        previous = re.search(
            r"^CMAKE_CXX_COMPILER:[^=]+=(.*)$",
            cache.read_text(encoding="utf-8"),
            re.MULTILINE,
        )
        if (
            (previous and previous[1] != compiler)
            or not identity_path.is_file()
            or identity_path.read_text() != identity
        ):
            # CMake's implicit compiler-change restart drops supplied cache
            # variables, including the SDK/preset values. Start this owned
            # generated tree freshly with the full command instead. A stable
            # driver path can also select a new compiler after an Xcode or
            # SDK update; discard objects produced by the previous version.
            fresh = ["--fresh"]
    run(
        [
            cmake,
            *fresh,
            "--preset",
            preset,
            "-S",
            str(project),
            "-B",
            str(tree),
            f"-DCMAKE_TOOLCHAIN_FILE={TOOLCHAIN}",
            f"-DSYMBIAN_TARGET_ARCH={architecture}",
            f"-DCMAKE_CXX_COMPILER={compiler}",
            f"-DCMAKE_LINKER={linker}",
            *(
                [
                    f"-DCMAKE_AR={archive_tools['ar']}",
                    f"-DCMAKE_RANLIB={archive_tools['ranlib']}",
                ]
                if archive_tools
                else []
            ),
            f"-DCMAKE_MAKE_PROGRAM={ninja}",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            "-DSYMBIAN_IMPORT_PROXIES="
            + ";".join(str(path) for path in import_proxies),
            *(f"-D{name}={value}" for name, value in sorted(variables.items())),
        ],
        cwd=project,
        timeout=120,
    )
    identity_path.write_text(identity)
    return _target(tree, project, name)


def build(tree: Path, name: str, cmake: str) -> str:
    """Builds the named executable using its configured Ninja rules."""
    return run(
        [cmake, "--build", str(tree), "--target", name],
        cwd=tree,
        timeout=600,
    )


def dependencies(tree: Path, artifact: Path, ninja: str) -> frozenset[Path]:
    """Collects graph inputs, including compiler-discovered local headers."""
    version = run([ninja, "--version"], cwd=tree)
    match = re.match(r"(\d+)\.(\d+)", version)
    if match is None or tuple(map(int, match.groups())) < (1, 12):
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "SDK input tracking requires Ninja 1.12 or later; "
            "install it with uv tool install ninja and update PATH",
        )
    listing = run(
        [
            ninja,
            "-C",
            str(tree),
            "-t",
            "inputs",
            "-0",
            "-E",
            str(artifact.relative_to(tree)),
        ],
        cwd=tree,
    )
    paths = {(tree / path).resolve() for path in listing.split("\0") if path}
    # `inputs` contains declared graph edges. Compiler-discovered headers
    # remain in Ninja's dependency log and are available through `deps`.
    log = run([ninja, "-C", str(tree), "-t", "deps"], cwd=tree)
    active = False
    expected = remaining = 0
    for line in [*log.splitlines(), ""]:
        if not line:
            if remaining:
                raise StatusError(
                    Code.DATA_LOSS, "Incomplete Ninja deps record"
                )
            continue
        match = re.fullmatch(
            r"(.*): #deps (\d+), deps mtime \d+ \((VALID|STALE)\)", line
        )
        if match:
            if remaining:
                raise StatusError(
                    Code.DATA_LOSS, "Incomplete Ninja deps record"
                )
            active = (tree / match[1]).resolve() in paths
            expected = remaining = int(match[2])
            if active and match[3] != "VALID":
                raise StatusError(
                    Code.FAILED_PRECONDITION, "Stale Ninja dependency evidence"
                )
        elif line.startswith("    ") and remaining:
            if active:
                paths.add((tree / line[4:]).resolve())
            remaining -= 1
        else:
            raise StatusError(
                Code.DATA_LOSS, f"Unrecognized Ninja deps record ({expected})"
            )
    # Object files and order-only build directories are generated inputs.
    return frozenset(
        path
        for path in paths
        if path.is_file() and not path.is_relative_to(tree)
    )
