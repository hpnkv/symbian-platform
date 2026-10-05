"""Places original-header API metadata after functional documentation.

Doxygen runs this input filter without changing the licensed header snapshots.
Source browsing continues to show the original files.
"""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from filter_examples import format_examples  # noqa: E402

_METADATA = re.compile(
    r"^\s*\*?\s*[@\\](?:publishedAll|publishedPartner|released|removed|"
    r"internalAll|internalComponent|internalConponent|internalTechnology|prototype|capability)\b[^\n]*$"
)


def reorder_comment(match: re.Match[str]) -> str:
    """Moves publication, capability and API-status tags to an end paragraph."""
    comment = match.group(0)
    body = comment[3:-2]
    lines = body.splitlines(keepends=True)
    metadata = [
        line.strip().lstrip("*").strip()
        for line in lines
        if _METADATA.fullmatch(line.strip())
    ]
    if not metadata:
        return comment
    prose = "".join(
        line for line in lines if not _METADATA.fullmatch(line.strip())
    )
    # Explicitly leave the first prose sentence available to AUTOBRIEF. A par
    # command makes status a detailed section even for metadata-only comments.
    return (
        comment[:3]
        + prose.rstrip()
        + "\n\n\\par API status\n"
        + ("\n".join(metadata) + "\n*/")
    )


def filter_comments(source: str) -> str:
    """Reorders metadata in block documentation, preserving declarations."""
    return re.sub(r"/\*[*!].*?\*/", reorder_comment, source, flags=re.DOTALL)


if __name__ == "__main__":
    source = (
        Path(sys.argv[1]).read_bytes().decode("utf-8", errors="surrogateescape")
    )
    sys.stdout.buffer.write(
        format_examples(filter_comments(source)).encode(
            "utf-8", errors="surrogateescape"
        )
    )
