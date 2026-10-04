"""Language-aware token spans for console result viewers."""

from pygments import lex
from pygments.lexers import get_lexer_by_name
from pygments.token import Token

LANGUAGES = {
    "JSON": "json",
    "XML": "xml",
    "Python": "python",
    "C++": "cpp",
    "TOML": "toml",
    "Plain text": None,
}


def highlighted_spans(text: str, language: str) -> list[tuple[str, str]]:
    """Return exact text fragments with stable semantic colour classes."""
    alias = LANGUAGES.get(language)
    if alias is None:
        return [("plain", text)]
    lexer = get_lexer_by_name(alias, stripnl=False, ensurenl=False)
    spans = []
    for token_type, value in lex(text, lexer):
        if token_type in Token.Comment:
            category = "comment"
        elif token_type in Token.Error:
            category = "error"
        elif token_type in Token.Literal.String:
            category = "string"
        elif token_type in Token.Literal.Number:
            category = "number"
        elif token_type in Token.Keyword:
            category = "keyword"
        elif token_type in Token.Name.Tag:
            category = "tag"
        elif token_type in Token.Name.Attribute:
            category = "attribute"
        elif token_type in Token.Operator or token_type in Token.Punctuation:
            category = "operator"
        else:
            category = "plain"
        spans.append((category, value))
    return spans
