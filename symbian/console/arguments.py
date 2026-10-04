"""Convert validated guided-form values into public CLI arguments."""

import shlex
from collections.abc import Mapping

from symbian.console.models import CommandSpec


def argument_tokens(
    specification: CommandSpec, values: Mapping[str, str | bool]
) -> tuple[str, ...]:
    """Preserve catalog order and repeatable option semantics."""
    tokens: list[str] = []
    for argument in specification.arguments:
        value = values.get(argument.name, "")
        if argument.kind == "flag":
            if value:
                tokens.append(argument.flags[-1])
            continue
        entered = str(value).strip()
        if not entered:
            continue
        items = shlex.split(entered) if argument.repeatable else [entered]
        if argument.positional:
            tokens.extend(items)
        elif argument.repeatable:
            for item in items:
                tokens.extend((argument.flags[-1], item))
        else:
            tokens.extend((argument.flags[-1], entered))
    return tuple(tokens)
