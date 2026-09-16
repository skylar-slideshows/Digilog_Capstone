"""Persistence for this-machine user preferences (as opposed to shipped CONFIG.yaml)."""

from pathlib import Path
import yaml

SETTINGS_DIR = Path.home() / "Documents" / "Digilog Remote"
SETTINGS_PATH = SETTINGS_DIR / "UserSettings.yaml"


def load_settings():
    if not SETTINGS_PATH.exists():
        return {}
    try:
        with SETTINGS_PATH.open("r", encoding="utf-8") as settings_file:
            return yaml.safe_load(settings_file) or {}
    except (OSError, yaml.YAMLError):
        return {}


def save_settings(settings):
    SETTINGS_DIR.mkdir(parents=True, exist_ok=True)
    with SETTINGS_PATH.open("w", encoding="utf-8") as settings_file:
        yaml.safe_dump(settings, settings_file)


def save_settings_to(path, settings):
    """Write a settings dict to an arbitrary path (used to export a console settings profile)."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as settings_file:
        yaml.safe_dump(settings, settings_file)


def load_settings_from(path):
    """Read a settings dict from an arbitrary path (used to import a console settings profile)."""
    with Path(path).open("r", encoding="utf-8") as settings_file:
        return yaml.safe_load(settings_file) or {}

