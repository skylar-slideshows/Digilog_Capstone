"""Persistence for current Digilog channel state files."""

import json
from pathlib import Path


def save_state(file_path, channels, current_channel, extension):
    """Save the current-format state document and return its normalized path.
    Light/dark theme is a machine-local preference (UserSettings.yaml), not part of a .dgl file."""
    path = Path(file_path)
    if path.suffix.lower() != extension.lower():
        path = path.with_suffix(extension)

    document = {
        "version": 2,
        "current_channel": current_channel,
        "channels": channels,
    }
    with path.open("w", encoding="utf-8") as state_file:
        json.dump(document, state_file, indent=2)
    return path


def load_state(file_path, extension):
    """Load a current-format state document after validating its extension."""
    path = Path(file_path)
    if path.suffix.lower() != extension.lower():
        raise ValueError(f"File must have {extension} extension")
    with path.open("r", encoding="utf-8") as state_file:
        return path, json.load(state_file)
