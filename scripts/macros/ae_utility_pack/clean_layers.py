"""Clean selected layers by removing safe, repeatable clutter."""

from __future__ import annotations

import artifact


def run(
    clear_parent: bool = True,
    clear_effects: bool = True,
    clear_markers: bool = True,
    clear_expressions: bool = True,
    clear_labels: bool = True,
    preserve_locked_layers: bool = True,
) -> str:
    """Clean the current selection without touching source media."""
    return artifact.clean_selected_layers(
        clear_parent,
        clear_effects,
        clear_markers,
        clear_expressions,
        clear_labels,
        preserve_locked_layers,
    )


if __name__ == "__main__":
    print(run())
