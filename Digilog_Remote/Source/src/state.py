"""Persistence for current Digilog channel state files."""

import json
from pathlib import Path


# Version 4 (the console master's format): "channels" is the static mix, as before; "master" holds the
# master section (aux return levels and whether each is in the mix); "automation" holds control moves
# recorded on the console against the DAW's SMPTE timecode, in time order:
#   "automation": [ [ms, channel (1-16 or "master"), "<tag>", value], ... ]
# The app doesn't edit either: it keeps them as loaded and writes them back, so re-saving a mix made on
# the console loses nothing. Version 2 and 3 files still load.
STATE_VERSION = 4


def save_state(file_path, channels, current_channel, extension, master=None, automation=None):
    """Save the current-format state document and return its normalized path.
    master and automation are written back as they were loaded (None: no master section; no moves).
    Light/dark theme is a machine-local preference (UserSettings.yaml), not part of a .dgl file."""
    path = Path(file_path)
    if path.suffix.lower() != extension.lower():
        path = path.with_suffix(extension)

    document = {
        "version": STATE_VERSION,
        "current_channel": current_channel,
        "channels": channels,
    }
    if master is not None:
        document["master"] = master
    document["automation"] = automation if automation is not None else []
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
