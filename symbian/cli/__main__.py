"""Entry point for read-only research and experimental host builds."""

import argparse
import json
import sys
from pathlib import Path

from symbian import device, packaging, preservation, toolchain
from symbian.analysis import inspect_elf
from symbian.doctor import doctor
from symbian.e32 import inspect_image
from symbian.status import Code, StatusError


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Native Symbian research tools"
    )
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("doctor", help="Report host tools and target readiness")
    compiler_commands = commands.add_parser("toolchain").add_subparsers(
        dest="toolchain_command", required=True
    )
    probe = compiler_commands.add_parser(
        "probe", help="Test ARM ELF generation"
    )
    probe.add_argument("--output", type=Path, default=Path(".symbian/probe"))
    probe.add_argument("--compiler", default="clang++")
    verify_probe = compiler_commands.add_parser(
        "verify-probe", help="Run independent E32, CPU and kernel checks"
    )
    verify_probe.add_argument("artifact", type=Path)
    verify_probe.add_argument(
        "--oracles-build", type=Path, default=Path("build/eka2l1")
    )
    verify_probe.add_argument(
        "--output", type=Path, default=Path(".symbian/probe-check")
    )
    verify_package = compiler_commands.add_parser(
        "verify-package", help="Check SIS installation and kernel execution"
    )
    verify_package.add_argument("package", type=Path)
    verify_package.add_argument("--executable", type=Path, required=True)
    verify_package.add_argument(
        "--oracles-build", type=Path, default=Path("build/eka2l1")
    )
    verify_package.add_argument(
        "--output", type=Path, default=Path(".symbian/package-check")
    )
    proxy = compiler_commands.add_parser(
        "import-proxy", help="Build selected DEF ordinal proxies"
    )
    proxy.add_argument("definition", type=Path)
    proxy.add_argument("--symbol", action="append", required=True)
    proxy.add_argument("--target-dll", required=True)
    proxy.add_argument(
        "--output", type=Path, default=Path(".symbian/import-proxy")
    )
    proxy.add_argument("--compiler", default="clang++")
    proxy.add_argument("--linker", default="ld.lld")
    proxy.add_argument("--headers", type=Path)
    build = commands.add_parser("build", help="Build an ARM/E32 experiment")
    build.add_argument("--project", type=Path, default=Path.cwd())
    build.add_argument("--output", type=Path, default=Path(".symbian/build"))
    build.add_argument("--compiler", default="clang++")
    build.add_argument("--linker", default="ld.lld")
    package = commands.add_parser(
        "package", help="Build an unsigned SISX experiment"
    )
    package.add_argument("--project", type=Path, default=Path.cwd())
    package.add_argument("--artifact", type=Path, required=True)
    package.add_argument(
        "--output", type=Path, default=Path(".symbian/package")
    )
    inspect = commands.add_parser(
        "inspect", help="Inspect native format metadata"
    )
    inspect.add_argument("artifact", type=Path)
    inspect.add_argument(
        "--format",
        choices=("elf32", "e32", "sis", "import-proxy"),
        default="elf32",
    )
    preserve_commands = commands.add_parser("preserve").add_subparsers(
        dest="preserve_command", required=True
    )
    create = preserve_commands.add_parser(
        "create", help="Copy and seal artifacts"
    )
    create.add_argument("source", type=Path)
    create.add_argument("archive", type=Path)
    create.add_argument("--provenance", default="unknown")
    verify = preserve_commands.add_parser("verify", help="Check archive hashes")
    verify.add_argument("archive", type=Path)
    verify.add_argument("--manifest-sha256")
    device_commands = commands.add_parser("device").add_subparsers(
        dest="device_command", required=True
    )
    policy = device_commands.add_parser(
        "policy", help="Describe operation authority"
    )
    policy.add_argument("operation")
    return parser


def _execute(args: argparse.Namespace) -> dict:
    if args.command == "doctor":
        return doctor()
    if args.command == "toolchain":
        if args.toolchain_command == "probe":
            return toolchain.probe(args.output, args.compiler)
        if args.toolchain_command == "import-proxy":
            from symbian.sdk import build_import_proxy

            return build_import_proxy(
                args.definition,
                args.symbol,
                args.target_dll,
                args.output,
                args.compiler,
                args.linker,
                args.headers,
            )
        if args.toolchain_command == "verify-package":
            from symbian.packaging.verification import verify_package

            return verify_package(
                args.package, args.executable, args.oracles_build, args.output
            )
        from symbian.toolchain.verification import verify_probe

        return verify_probe(args.artifact, args.oracles_build, args.output)
    if args.command == "build":
        return toolchain.build(
            args.project, args.output, args.compiler, args.linker
        )
    if args.command == "package":
        return packaging.package(args.project, args.artifact, args.output)
    if args.command == "inspect":
        from symbian.sdk import inspect_proxy

        inspector = {
            "elf32": inspect_elf,
            "e32": inspect_image,
            "sis": packaging.inspect_package,
            "import-proxy": inspect_proxy,
        }[args.format]
        return {"format": args.format, "metadata": inspector(args.artifact)}
    if args.command == "preserve":
        if args.preserve_command == "create":
            return preservation.create(
                args.source, args.archive, args.provenance
            )
        return preservation.verify(args.archive, args.manifest_sha256)
    return device.policy(args.operation)


def main(argv: list[str] | None = None) -> int:
    """Emits JSON with canonical status and returns a process exit code."""
    args = _parser().parse_args(argv)
    try:
        result = _execute(args)
        response = {
            "schema": "symbian.cli/v1",
            "status": {"code": 0, "name": "OK", "message": ""},
            "result": result,
        }
        exit_code = 0
    except (StatusError, OSError) as error:
        if isinstance(error, OSError):
            if isinstance(error, FileNotFoundError):
                code = Code.NOT_FOUND
            elif isinstance(error, PermissionError):
                code = Code.PERMISSION_DENIED
            else:
                code = Code.INTERNAL
            error = StatusError(code, str(error))
        response = {"schema": "symbian.cli/v1", "status": error.as_dict()}
        exit_code = 1
    print(json.dumps(response, sort_keys=True, ensure_ascii=True))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
