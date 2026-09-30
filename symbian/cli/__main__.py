"""Entry point for read-only research and experimental host builds."""

import argparse
import json
import sys
from pathlib import Path

from symbian import device, preservation, toolchain
from symbian.analysis import inspect_elf
from symbian.doctor import doctor
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
    build = commands.add_parser("build", help="Build an ARM object experiment")
    build.add_argument("--project", type=Path, default=Path.cwd())
    build.add_argument("--output", type=Path, default=Path(".symbian/build"))
    build.add_argument("--compiler", default="clang++")
    inspect = commands.add_parser("inspect", help="Inspect ELF32 metadata")
    inspect.add_argument("artifact", type=Path)
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
        return toolchain.probe(args.output, args.compiler)
    if args.command == "build":
        return toolchain.build(args.project, args.output, args.compiler)
    if args.command == "inspect":
        return {"format": "elf32", "metadata": inspect_elf(args.artifact)}
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
