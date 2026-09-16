"""Application configuration and DearPyGui layout data."""

import sys
from pathlib import Path
import yaml

if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
    # Running as a PyInstaller-bundled app: bundled data files (config/, assets/) live
    # under the extracted/bundle resource dir, not next to this source file.
    ROOT_DIR = Path(sys._MEIPASS)
else:
    SCRIPT_DIR = Path(__file__).resolve().parent
    ROOT_DIR = SCRIPT_DIR.parent
# DearPyGui viewport icons must be .ico on Windows, .ico or .png on mac.
ICON_PATH = ROOT_DIR / "assets" / ("icon.ico" if sys.platform == "win32" else "icon.png")
CONFIG_PATH = ROOT_DIR / "config" / "CONFIG.yaml"

with CONFIG_PATH.open("r", encoding="utf-8") as config_file:
    CONFIG = yaml.safe_load(config_file)

if sys.platform == "win32":
    # EnterCommand.ttf renders poorly on Windows; use the system UI font instead
    # (still loaded per-widget below so the configured pixel sizes keep applying).
    import os
    configured_font_path = Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts" / "segoeui.ttf"
else:
    configured_font_path = Path(CONFIG.get("font", "assets/EnterCommand.ttf"))
    if not configured_font_path.is_absolute():
        configured_font_path = ROOT_DIR / configured_font_path

group_label_size = int(CONFIG.get("group_label_size", 30))
knob_label_size = int(CONFIG.get("knob_label_size", 12))
button_label_size = int(CONFIG.get("button_label_size", 14))
app_version = str(CONFIG.get("app_version", "1.0"))
state_file_extension = CONFIG.get("state_file_extension", ".dgl")
if not state_file_extension.startswith("."):
    state_file_extension = f".{state_file_extension}"
ack_timeout_ms = int(CONFIG.get("ack_timeout_ms", 100))
debug_no_serial_timeout = bool(CONFIG.get("debug_no_serial_timeout", False))
num_inserts = int(CONFIG.get("num_inserts", 16))
insert_dropdown_pos = tuple(CONFIG.get("insert_dropdown_pos", [612, 150]))
logo_text = CONFIG.get("logo_text", "DIGILOG")
logo_pos = tuple(CONFIG.get("logo_pos", [20, 10]))
logo_size = int(CONFIG.get("logo_size", 42))
LOGO_FONT_PATH = ROOT_DIR / "assets" / "Distortion Dos Analogue.otf"


def color(key, default):
    value = CONFIG.get(key)
    return tuple(value) if value is not None else default


selected_window_top_bar_color = color("selected_window_top_bar_color", (54, 10, 142, 255))
selected_window_top_bar_light_color = color("selected_window_top_bar_light_color", (130, 80, 220, 255))
button_color = color("button_color", (70, 70, 90, 255))
button_light_color = color("button_light_color", (200, 200, 215, 255))
knob_color = color("knob_color", (120, 120, 120, 255))
knob_light_color = color("knob_light_color", (200, 200, 210, 255))
textbox_color = color("textbox_color", (30, 30, 40, 255))
textbox_light_color = color("textbox_light_color", (240, 240, 245, 255))


def color_from(item, key):
    return tuple(item[key]) if key in item else None


GROUPS = []
KNOB_CONFIG = {}
for group in CONFIG["groups"]:
    tags = []
    for knob in group.get("knobs", []) or []:
        tag = knob["tag"]
        KNOB_CONFIG[tag] = {
            "label": knob.get("label", tag),
            "pos": tuple(knob["pos"]),
            "ctrl_id": int(knob["ctrl_id"]),
            "min": float(knob["min"]),
            "max": float(knob["max"]),
            "default": float(knob["default"]),
            "color": color_from(knob, "color"),
            "light_color": color_from(knob, "light_color"),
            "indicator_color": color_from(knob, "indicator_color"),
        }
        tags.append(tag)
    GROUPS.append({"name": group["name"], "label_pos": tuple(group["label_pos"]), "tags": tags})

EXTRA_KNOB_TAGS = []
for knob in CONFIG.get("extra_knobs", []) or []:
    tag = knob["tag"]
    KNOB_CONFIG[tag] = {
        "label": knob.get("label", tag),
        "pos": tuple(knob["pos"]),
        "ctrl_id": int(knob["ctrl_id"]),
        "min": float(knob["min"]),
        "max": float(knob["max"]),
        "default": float(knob["default"]),
        "color": color_from(knob, "color"),
        "light_color": color_from(knob, "light_color"),
        "indicator_color": color_from(knob, "indicator_color"),
    }
    EXTRA_KNOB_TAGS.append(tag)

TOGGLE_CONFIG = []
CYCLE_CONFIG = []


def add_cycle(button):
    if not any(item["tag"] == button["tag"] for item in CYCLE_CONFIG):
        CYCLE_CONFIG.append({
            "tag": button["tag"],
            "label": button.get("label", button["tag"]),
            "sw_id": int(button.get("sw_id", 0)),
            "pos": tuple(button["pos"]) if button.get("pos") is not None else None,
            "size": int(button.get("size", 32)),
            "default": int(button.get("default", 0)),
            "options": button.get("options", ["Line", "Mic", "Hi-Z"]),
        })


def add_toggle(button):
    if any(item["tag"] == button["tag"] for item in TOGGLE_CONFIG):
        return
    if any(item["tag"] == button["tag"] for item in CYCLE_CONFIG):
        return
    TOGGLE_CONFIG.append({
        "tag": button["tag"],
        "label": button.get("label", button["tag"]),
        "sw_id": int(button["sw_id"]) if "sw_id" in button else None,
        "pos": tuple(button["pos"]) if button.get("pos") is not None else None,
        "size": int(button.get("size", 32)),
        "default": bool(button.get("default", False)),
        "on_color": tuple(button.get("on_color", (210, 200, 120, 255))),
        "on_light_color": color_from(button, "on_light_color"),
    })


for group in CONFIG["groups"]:
    for button in (group.get("buttons", []) or []) + (group.get("toggles", []) or []):
        (add_cycle if button.get("type") == "cycle" or "options" in button else add_toggle)(button)

for button in (CONFIG.get("toggles", []) or []) + (CONFIG.get("buttons", []) or []) + (CONFIG.get("extra_buttons", []) or []):
    (add_cycle if button.get("type") == "cycle" or "options" in button else add_toggle)(button)

FADER_CONFIG = None
if "fader" in CONFIG:
    fader = CONFIG["fader"]
    FADER_CONFIG = {
        "tag": fader["tag"],
        "ctrl_id": int(fader.get("ctrl_id", 26)),
        "label": fader.get("label", fader["tag"]),
        "pos": tuple(fader["pos"]),
        "width": fader.get("width", 20),
        "height": fader.get("height", 160),
        "min": float(fader["min"]),
        "max": float(fader["max"]),
        "default": float(fader["default"]),
        "color": color_from(fader, "color"),
        "light_color": color_from(fader, "light_color"),
        "indicator_color": color_from(fader, "indicator_color"),
    }

KNOB_TAGS = list(KNOB_CONFIG)
VERTICAL_BARS = [
    {
        "pos": tuple(bar["pos"]),
        "length": float(bar["length"]),
        "thickness": float(bar.get("thickness", 2)),
        "color": tuple(bar.get("color", (150, 150, 150, 255))),
    }
    for bar in CONFIG.get("vertical_bars", []) or []
]

num_channels = int(CONFIG.get("num_channels", 4))
CHANNEL_OPTIONS = [f"Channel {index + 1}" for index in range(num_channels)]
