# AE Utility Pack

This folder contains the lightweight After Effects-inspired utility scripts used by Artifact.

## Scripts

- `quick_rename_layers.py`
  - Batch-renames the current layer selection.
  - Arguments: `prefix`, `base_name`, `suffix`, `start_index`, `padding`, `rename_selected_only`.

- `clean_layers.py`
  - Clears common layer clutter from the current selection.
  - Arguments: `clear_parent`, `clear_effects`, `clear_markers`, `clear_expressions`, `clear_labels`, `preserve_locked_layers`.

- `trim_comp_to_content.py`
  - Trims the comp frame range to layer content.
  - Arguments: `trim_mode`, `padding_frames`, `sync_work_area`, `respect_locked_layers`.
  - If no layers are selected, the script falls back to the current layer for `selectedLayers`.

## Trim Modes

- `selectedLayers`
- `allLayers`
- `visibleLayers`
