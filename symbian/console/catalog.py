"""Derive console workflows from the SDK's canonical CLI parser."""

import argparse
from pathlib import Path

from symbian.console.models import CommandArgument, CommandCatalog, CommandSpec


def _argument(action: argparse.Action) -> CommandArgument:
    """Describe one user-settable argparse action without copying CLI rules."""
    option_flags = tuple(action.option_strings)
    positional = not option_flags
    is_flag = isinstance(
        action,
        (
            argparse._StoreTrueAction,
            argparse._StoreFalseAction,
            argparse._CountAction,
            argparse._StoreConstAction,
        ),
    )
    choices = tuple(str(value) for value in action.choices or ())
    default = action.default
    if default in (None, argparse.SUPPRESS) or isinstance(default, bool):
        displayed_default = None
    elif isinstance(default, (str, int, float, Path)):
        displayed_default = str(default)
    else:
        displayed_default = None
    label = (
        next(
            (flag for flag in reversed(option_flags) if flag.startswith("--")),
            option_flags[0],
        )
        if option_flags
        else action.dest.replace("_", " ")
    )
    return CommandArgument(
        name=action.dest,
        label=label,
        flags=option_flags,
        help=action.help if isinstance(action.help, str) else "",
        required=bool(action.required)
        or (positional and action.nargs not in ("?", "*")),
        kind="flag" if is_flag else "choice" if choices else "text",
        choices=choices,
        repeatable=isinstance(action, argparse._AppendAction)
        or action.nargs in ("+", "*"),
        positional=positional,
        default=displayed_default,
    )


def catalog() -> CommandCatalog:
    """Expose every runnable CLI leaf except this graphical frontend itself."""
    from symbian.cli.__main__ import _parser

    commands: list[CommandSpec] = []

    def visit(parser: argparse.ArgumentParser, path: tuple[str, ...]) -> None:
        nested = next(
            (
                action
                for action in parser._actions
                if isinstance(action, argparse._SubParsersAction)
            ),
            None,
        )
        if nested is not None:
            seen = set()
            for command, child in nested.choices.items():
                if id(child) in seen:
                    continue
                seen.add(id(child))
                visit(child, (*path, command))
            return
        if not path or path == ("console",):
            return
        arguments = tuple(
            _argument(action)
            for action in parser._actions
            if action.dest not in ("help", "output_format")
        )
        commands.append(
            CommandSpec(
                path=path,
                title=" ".join(path),
                category=path[0].replace("_", " ").title(),
                description=parser.description or "Run this SDK workflow.",
                arguments=arguments,
            )
        )

    visit(_parser(), ())
    return CommandCatalog(commands=tuple(commands))
