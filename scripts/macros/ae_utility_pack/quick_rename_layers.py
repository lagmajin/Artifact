"""Rename selected layers using an AE-style batch naming pattern."""

from __future__ import annotations

import artifact


def run(
    prefix: str = "",
    base_name: str = "Layer",
    suffix: str = "",
    start_index: int = 1,
    padding: int = 0,
    rename_selected_only: bool = True,
) -> str:
    """Rename the current selection in selection order."""
    return artifact.rename_selected_layers(
        prefix,
        base_name,
        suffix,
        start_index,
        padding,
        rename_selected_only,
    )


if __name__ == "__main__":
    print(run())
