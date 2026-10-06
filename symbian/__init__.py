"""Native development and research tools for Symbian Belle."""

from pkgutil import extend_path

# Source workspaces use the current Python environment's compiled host module.
# Keep the checkout's Python files first while allowing its installed wheel to
# provide _native during CMake configuration.
__path__ = extend_path(__path__, __name__)
