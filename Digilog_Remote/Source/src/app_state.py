"""Shared application state: config-derived data, channel model, theming, serial
handler plumbing, and persistence. Every window module reads/writes through this
module instead of importing each other, which keeps the window modules decoupled."""

import time
from pathlib import Path

import dearpygui.dearpygui as dpg

import state
import user_settings
from config import (
    CHANNEL_OPTIONS as _CHANNEL_OPTIONS, CYCLE_CONFIG, EXTRA_KNOB_TAGS, FADER_CONFIG,
    GROUPS, KNOB_CONFIG, KNOB_TAGS, TOGGLE_CONFIG,
    button_color, button_light_color, knob_color, knob_light_color,
    num_channels as _num_channels, num_inserts,
    selected_window_top_bar_color, selected_window_top_bar_light_color,
    state_file_extension, textbox_color, textbox_light_color,
)

# --- External handler callbacks registered by the orchestrator (run.py) ---
on_connect_handler = None
on_knob_change_handler = None
on_button_change_handler = None
on_channel_name_change_handler = None
on_state_loaded_handler = None
on_sync_to_console_handler = None


def set_serial_handlers(on_connect=None, on_knob_change=None, on_button_change=None,
                         on_channel_name_change=None, on_state_loaded=None, on_sync_to_console=None):
    global on_connect_handler, on_knob_change_handler
    global on_button_change_handler, on_channel_name_change_handler, on_state_loaded_handler, on_sync_to_console_handler
    on_connect_handler = on_connect
    on_knob_change_handler = on_knob_change
    on_button_change_handler = on_button_change
    on_channel_name_change_handler = on_channel_name_change
    on_state_loaded_handler = on_state_loaded
    on_sync_to_console_handler = on_sync_to_console


def update_status(text: str):
    if dpg.does_item_exist("status_text"):
        dpg.set_value("status_text", text)


# Whether the app currently believes it has a working connection to the STM32 - set True on a
# successful connect, and False on a failed connect or an ack-waited send exhausting its retries.
# Never actively probed/pinged; other windows can react to changes via register_connection_change_hook.
stm32_connected = False
_connection_change_hooks = []


def register_connection_change_hook(fn):
    _connection_change_hooks.append(fn)


def set_stm32_connected(value: bool):
    global stm32_connected
    if stm32_connected == value:
        return
    stm32_connected = value
    for hook in _connection_change_hooks:
        hook(value)


def show_no_connection_popup():
    """Show the modal "no connection" alert (raised when an ack-waited send exhausts its retries)."""
    set_stm32_connected(False)
    if not dpg.does_item_exist("no_connection_popup"):
        return
    width, height = 320, 130
    viewport_width = dpg.get_viewport_client_width()
    viewport_height = dpg.get_viewport_client_height()
    dpg.configure_item(
        "no_connection_popup",
        show=True,
        pos=[max(0, (viewport_width - width) // 2), max(0, (viewport_height - height) // 2)],
    )


# --- Knob-change debouncing: notify immediately, then re-send once the value settles ---
_knob_last_change = {}
_knob_final_generation = {}
# Keys (channel, tag) waiting on their settle-check, each mapped to (tag, channel_name). Polled by
# a single self-rescheduling frame callback rather than one dpg.set_frame_callback per key - several
# linked/selected channels updating in the same tick would otherwise all target the same future
# frame number, and registering more than one callback for that frame crashes this DPG build.
_knob_pending_keys = {}
_knob_poll_scheduled = False


def _knob_cfg_for_tag(tag):
    cfg = KNOB_CONFIG.get(tag)
    if cfg is None and FADER_CONFIG and tag == FADER_CONFIG["tag"]:
        cfg = FADER_CONFIG
    return cfg


def _send_final_knob_value(key, tag, channel_name, generation):
    if _knob_final_generation.get(key) != generation:
        return
    cfg = _knob_cfg_for_tag(tag)
    if cfg is None or "ctrl_id" not in cfg or channel_name not in CHANNEL_OPTIONS:
        return
    # read the stored value rather than dpg.get_value(tag) - "tag" may belong to a per-channel
    # widget (e.g. the Channels tab) rather than the single shared Control-tab widget
    value = channel_states.get(channel_name, {}).get("knobs", {}).get(tag)
    if value is None:
        return
    on_knob_change_handler(CHANNEL_OPTIONS.index(channel_name), cfg["ctrl_id"], value, cfg["min"], cfg["max"], is_final=True)


def _poll_pending_knob_finals():
    global _knob_poll_scheduled
    now = time.monotonic()
    for key in [k for k in _knob_pending_keys if now - _knob_last_change.get(k, 0.0) >= 0.5]:
        tag, channel_name = _knob_pending_keys.pop(key)
        _send_final_knob_value(key, tag, channel_name, _knob_final_generation.get(key))
    if _knob_pending_keys:
        dpg.set_frame_callback(dpg.get_frame_count() + 1, _poll_pending_knob_finals)
    else:
        _knob_poll_scheduled = False


def notify_knob_change(tag, value, ctrl_id, minimum, maximum, channel=None):
    global _knob_poll_scheduled
    if not on_knob_change_handler:
        return
    channel_name = channel or current_channel
    key = (channel_name, tag)
    _knob_last_change[key] = time.monotonic()
    _knob_final_generation[key] = _knob_final_generation.get(key, 0) + 1
    on_knob_change_handler(CHANNEL_OPTIONS.index(channel_name), ctrl_id, value, minimum, maximum, is_final=False)
    _knob_pending_keys[key] = (tag, channel_name)
    if not _knob_poll_scheduled:
        _knob_poll_scheduled = True
        dpg.set_frame_callback(dpg.get_frame_count() + 1, _poll_pending_knob_finals)


# channels_window registers mirror_knob_delta_to_others here, so a knob update arriving from the
# STM32 itself (a physical knob turn) mirrors to linked/selected channels the same way a GUI drag
# does - including each mirrored channel getting its own 500ms-later debounced final send.
_incoming_knob_mirror_hooks = []


def register_incoming_knob_mirror_hook(fn):
    _incoming_knob_mirror_hooks.append(fn)


# --- Channel model ---
CHANNEL_OPTIONS = list(_CHANNEL_OPTIONS)
num_channels = _num_channels
current_channel = CHANNEL_OPTIONS[0]

# Sane upper bound so a garbled/malicious $chct value can't exhaust memory.
MAX_CHANNELS = 256


def default_channel_state(idx):
    k_vals = {tag: KNOB_CONFIG[tag]["default"] for tag in KNOB_TAGS}
    if FADER_CONFIG:
        k_vals[FADER_CONFIG["tag"]] = FADER_CONFIG["default"]
    t_vals = {toggle["tag"]: toggle["default"] for toggle in TOGGLE_CONFIG if toggle["tag"] != "light_theme"}
    c_vals = {btn["tag"]: btn["default"] for btn in CYCLE_CONFIG}
    return {
        "name": f"Channel {idx + 1}",
        "knobs": k_vals,
        "toggles": t_vals,
        "cycles": c_vals,
        "insert": None,
        "chain": None,
    }


channel_states = {CHANNEL_OPTIONS[i]: default_channel_state(i) for i in range(num_channels)}
knob_values = {tag: KNOB_CONFIG[tag]["default"] for tag in KNOB_TAGS}
if FADER_CONFIG:
    knob_values[FADER_CONFIG["tag"]] = FADER_CONFIG["default"]
cycle_values = {btn["tag"]: btn["default"] for btn in CYCLE_CONFIG}
toggle_values = {toggle["tag"]: toggle["default"] for toggle in TOGGLE_CONFIG}

USER_SETTINGS = user_settings.load_settings()
toggle_values["light_theme"] = bool(USER_SETTINGS.get("light_theme", toggle_values.get("light_theme", False)))
stereo_pairs = list(USER_SETTINGS.get("stereo_pairs", []))
chains = list(USER_SETTINGS.get("chains", []))

current_state_file_path = None

TEMPLATES_DIR = user_settings.SETTINGS_DIR / "Templates"
DEFAULT_MIX_FILE_NAME = f"DEFAULT{state_file_extension}"
DEFAULT_TEMPLATE_FILE_NAME = f"MixTemplate{state_file_extension}"
DEFAULT_PROFILE_FILE_NAME = "Profile.yaml"
PROFILE_DEFAULT_DIR = user_settings.SETTINGS_DIR

# Sub-window modules register callbacks here to run whenever the active channel changes,
# so app_state doesn't need to import any window module back (which would create a cycle).
_channel_change_hooks = []


def register_channel_change_hook(fn):
    _channel_change_hooks.append(fn)


# Same pattern as _channel_change_hooks, but for whenever the stereo_pairs list is mutated.
_stereo_pairs_change_hooks = []


def register_stereo_pairs_change_hook(fn):
    _stereo_pairs_change_hooks.append(fn)


def notify_stereo_pairs_changed():
    for hook in _stereo_pairs_change_hooks:
        hook()
    notify_inserts_changed()


# Same pattern again, but for whenever the chains list is mutated.
_chains_change_hooks = []


def register_chains_change_hook(fn):
    _chains_change_hooks.append(fn)


def notify_chains_changed():
    for hook in _chains_change_hooks:
        hook()
    notify_inserts_changed()


def release_deleted_chain(deleted_index):
    """Fix up every channel's chain selection after chains[deleted_index] is removed from the list:
    channels using it are cleared, channels using a later chain have their index shifted down."""
    for ch_state in channel_states.values():
        idx = ch_state.get("chain")
        if idx is None:
            continue
        if idx == deleted_index:
            ch_state["chain"] = None
        elif idx > deleted_index:
            ch_state["chain"] = idx - 1


# Fires for ANY edit made within the Inserts tab (insert enable/name, stereo pairs, chains),
# so the tab can show a "Sync to Console" button whenever the STM32 has gone stale.
_inserts_change_hooks = []


def register_inserts_change_hook(fn):
    _inserts_change_hooks.append(fn)


def notify_inserts_changed():
    for hook in _inserts_change_hooks:
        hook()


def _set_channel_count(n, preserve=True):
    """Resize CHANNEL_OPTIONS/channel_states to n channels, reusing existing channel state when preserve=True."""
    global CHANNEL_OPTIONS, channel_states, current_channel, num_channels
    n = max(1, min(MAX_CHANNELS, int(n)))
    new_options = [f"Channel {i+1}" for i in range(n)]
    new_states = {}
    for i, ch in enumerate(new_options):
        if preserve and ch in channel_states:
            new_states[ch] = channel_states[ch]
        else:
            new_states[ch] = default_channel_state(i)
    CHANNEL_OPTIONS = new_options
    channel_states = new_states
    num_channels = n
    if current_channel not in CHANNEL_OPTIONS:
        current_channel = CHANNEL_OPTIONS[0]


def update_channel_dropdown():
    if dpg.does_item_exist("channel_dropdown"):
        display_names = [channel_states[ch]["name"] for ch in CHANNEL_OPTIONS]
        curr_name = channel_states.get(current_channel, {}).get("name", current_channel)
        dpg.configure_item("channel_dropdown", items=display_names)
        dpg.set_value("channel_dropdown", curr_name)


def update_cycle_display(tag):
    if tag == "input_src":
        state_idx = cycle_values.get("input_src", 0)
        is_light = toggle_values.get("light_theme", False)
        for i in range(3):
            is_on = (i == state_idx)
            if is_on:
                fill_col = (255, 255, 255, 255)
                border_col = (40, 40, 40, 255) if is_light else (255, 255, 255, 255)
                txt_col = (20, 20, 20, 255) if is_light else (255, 255, 255, 255)
            else:
                fill_col = (180, 180, 190, 255) if is_light else (55, 55, 55, 255)
                border_col = (140, 140, 150, 255) if is_light else (80, 80, 80, 255)
                txt_col = (100, 100, 110, 255) if is_light else (140, 140, 140, 255)
            if dpg.does_item_exist(f"input_src_led_{i}"):
                dpg.configure_item(f"input_src_led_{i}", fill=fill_col, color=border_col)
            if dpg.does_item_exist(f"input_src_text_{i}"):
                dpg.configure_item(f"input_src_text_{i}", color=txt_col)


def save_current_channel_state():
    k_vals = {tag: dpg.get_value(tag) for tag in KNOB_TAGS if dpg.does_item_exist(tag)}
    if FADER_CONFIG and dpg.does_item_exist(FADER_CONFIG["tag"]):
        k_vals[FADER_CONFIG["tag"]] = dpg.get_value(FADER_CONFIG["tag"])
    t_vals = {toggle["tag"]: toggle_values[toggle["tag"]] for toggle in TOGGLE_CONFIG if toggle["tag"] != "light_theme"}
    c_vals = {btn["tag"]: cycle_values.get(btn["tag"], btn["default"]) for btn in CYCLE_CONFIG}
    ch_name = dpg.get_value("channel_name_input") if dpg.does_item_exist("channel_name_input") else channel_states.get(current_channel, {}).get("name", current_channel)
    channel_states[current_channel] = {
        "name": ch_name,
        "knobs": k_vals,
        "toggles": t_vals,
        "cycles": c_vals,
        "insert": channel_states.get(current_channel, {}).get("insert"),
        "chain": channel_states.get(current_channel, {}).get("chain"),
    }


def apply_channel_state(channel_name):
    global current_channel
    current_channel = channel_name
    idx = CHANNEL_OPTIONS.index(channel_name) if channel_name in CHANNEL_OPTIONS else 0
    st = channel_states.get(channel_name, default_channel_state(idx))
    for tag, value in st.get("knobs", {}).items():
        if dpg.does_item_exist(tag):
            dpg.set_value(tag, value)
            knob_values[tag] = value
    for tag, value in st.get("toggles", {}).items():
        if tag in toggle_values and tag != "light_theme":
            toggle_values[tag] = bool(value)
            if dpg.does_item_exist(tag):
                active_theme = f"{tag}_active_theme"
                dpg.bind_item_theme(tag, active_theme if toggle_values[tag] else "toggle_theme")
    for tag, value in st.get("cycles", {}).items():
        if tag in cycle_values:
            cycle_values[tag] = int(value)
            update_cycle_display(tag)
    if dpg.does_item_exist("channel_name_input"):
        dpg.set_value("channel_name_input", st.get("name", channel_name))
    update_channel_dropdown()
    for hook in _channel_change_hooks:
        hook()


def apply_incoming_channel_count(n: int):
    """Apply an incoming $chct command from the STM32, resizing the number of channels."""
    if n < 1:
        return
    _set_channel_count(n, preserve=True)
    apply_channel_state(current_channel)


def _uint16_to_knob_value(raw_value, minimum, maximum):
    raw_value = max(0, min(0xFFFF, int(raw_value)))
    return minimum + (raw_value / 0xFFFF) * (maximum - minimum)


def apply_incoming_knob_update(channel: int, ctrl_id: int, raw_value: int):
    """Apply an incoming $ud command from the STM32, updating state and the GUI if visible."""
    if not (0 <= channel < len(CHANNEL_OPTIONS)):
        return
    tag, cfg = next(((t, c) for t, c in KNOB_CONFIG.items() if c["ctrl_id"] == ctrl_id), (None, None))
    if cfg is None and FADER_CONFIG and FADER_CONFIG["ctrl_id"] == ctrl_id:
        tag, cfg = FADER_CONFIG["tag"], FADER_CONFIG
    if cfg is None:
        return
    value = _uint16_to_knob_value(raw_value, cfg["min"], cfg["max"])
    channel_key = CHANNEL_OPTIONS[channel]
    prev_value = channel_states.get(channel_key, {}).get("knobs", {}).get(tag)
    if channel_key in channel_states:
        channel_states[channel_key]["knobs"][tag] = value
    if channel_key == current_channel:
        knob_values[tag] = value
        if dpg.does_item_exist(tag):
            dpg.set_value(tag, value)
    if prev_value is not None and value != prev_value:
        for hook in _incoming_knob_mirror_hooks:
            hook(channel_key, tag, value - prev_value, cfg["ctrl_id"], cfg["min"], cfg["max"])


def apply_incoming_switch_update(channel: int, sw_id: int, raw_value):
    """Apply an incoming $sw command from the STM32, updating state and the GUI if visible."""
    if not (0 <= channel < len(CHANNEL_OPTIONS)):
        return
    channel_key = CHANNEL_OPTIONS[channel]

    cycle_cfg = next((c for c in CYCLE_CONFIG if c["sw_id"] == sw_id), None)
    if cycle_cfg is not None:
        tag = cycle_cfg["tag"]
        text = str(raw_value)
        try:
            idx = int(text, 2) if len(text) > 1 else int(text)
        except ValueError:
            return
        idx = idx % len(cycle_cfg["options"])
        if channel_key in channel_states:
            channel_states[channel_key].setdefault("cycles", {})[tag] = idx
        if channel_key == current_channel:
            cycle_values[tag] = idx
            update_cycle_display(tag)
        return

    toggle_cfg = next((t for t in TOGGLE_CONFIG if t["sw_id"] == sw_id), None)
    if toggle_cfg is not None and toggle_cfg["tag"] != "light_theme":
        tag = toggle_cfg["tag"]
        is_on = str(raw_value) in ("1", "01", "true", "True")
        if channel_key in channel_states:
            channel_states[channel_key].setdefault("toggles", {})[tag] = is_on
        if channel_key == current_channel:
            toggle_values[tag] = is_on
            if dpg.does_item_exist(tag):
                active_theme = f"{tag}_active_theme"
                dpg.bind_item_theme(tag, active_theme if is_on else "toggle_theme")


def apply_incoming_channel_name(channel: int, name: str):
    """Apply an incoming $chnm command from the STM32, updating state and the GUI if visible."""
    if not (0 <= channel < len(CHANNEL_OPTIONS)):
        return
    channel_key = CHANNEL_OPTIONS[channel]
    if channel_key in channel_states:
        channel_states[channel_key]["name"] = name
    if channel_key == current_channel and dpg.does_item_exist("channel_name_input"):
        dpg.set_value("channel_name_input", name)
    update_channel_dropdown()
    for hook in _channel_change_hooks:
        hook()


def _knob_gui_value_to_uint16(tag, value):
    """Convert a GUI-range knob value to the 0-65535 range stored in state files."""
    cfg = _knob_cfg_for_tag(tag)
    if cfg is None or cfg["max"] <= cfg["min"]:
        return value
    normalized = (value - cfg["min"]) / (cfg["max"] - cfg["min"])
    return round(max(0.0, min(1.0, normalized)) * 0xFFFF)


def _knob_uint16_value_to_gui(tag, value):
    """Convert a 0-65535 state file value back to the knob's configured GUI range."""
    cfg = _knob_cfg_for_tag(tag)
    if cfg is None:
        return value
    normalized = max(0.0, min(1.0, value / 0xFFFF))
    return cfg["min"] + normalized * (cfg["max"] - cfg["min"])


def save_state_file(file_path):
    global current_state_file_path
    save_current_channel_state()
    channels_to_save = {
        ch: {**data, "knobs": {tag: _knob_gui_value_to_uint16(tag, value) for tag, value in data["knobs"].items()}}
        for ch, data in channel_states.items()
    }
    file_path = state.save_state(
        file_path,
        channels_to_save,
        current_channel,
        state_file_extension,
    )
    current_state_file_path = file_path
    return file_path


def load_state_file(file_path):
    global current_state_file_path
    file_path, state_document = state.load_state(file_path, state_file_extension)

    loaded_channels = state_document["channels"]
    _set_channel_count(len(loaded_channels), preserve=False)
    for ch in CHANNEL_OPTIONS:
        ch_data = loaded_channels[ch]
        channel_states[ch] = {
            "name": ch_data["name"],
            "knobs": {tag: _knob_uint16_value_to_gui(tag, value) for tag, value in ch_data["knobs"].items()},
            "toggles": dict(ch_data["toggles"]),
            "cycles": dict(ch_data["cycles"]),
            "insert": ch_data.get("insert"),
            "chain": ch_data.get("chain"),
        }

    # Light/dark theme is only ever loaded from UserSettings.yaml, never from a .dgl file.
    apply_channel_state(state_document["current_channel"])

    current_state_file_path = file_path

    if on_state_loaded_handler:
        channel_names = []
        knob_updates = []
        switch_updates = []
        for ch in CHANNEL_OPTIONS:
            idx = CHANNEL_OPTIONS.index(ch)
            ch_data = channel_states[ch]
            channel_names.append((idx, ch_data["name"]))
            for tag, value in ch_data["knobs"].items():
                cfg = KNOB_CONFIG.get(tag)
                if cfg is None and FADER_CONFIG and tag == FADER_CONFIG["tag"]:
                    cfg = FADER_CONFIG
                if cfg is not None and "ctrl_id" in cfg:
                    knob_updates.append((idx, cfg["ctrl_id"], value, cfg["min"], cfg["max"]))
            for tag, value in ch_data["toggles"].items():
                toggle_cfg = next((t for t in TOGGLE_CONFIG if t["tag"] == tag), None)
                if toggle_cfg is not None and toggle_cfg["sw_id"] is not None:
                    switch_updates.append((idx, toggle_cfg["sw_id"], int(bool(value))))
            for tag, value in ch_data["cycles"].items():
                cycle_cfg = next((c for c in CYCLE_CONFIG if c["tag"] == tag), None)
                if cycle_cfg is not None:
                    switch_updates.append((idx, cycle_cfg["sw_id"], format(int(value), "02b")))
        on_state_loaded_handler(channel_names, knob_updates, switch_updates)

    return file_path


def save_as_default_file():
    """Save the current mix straight to DEFAULT.dgl in the settings folder, no dialog."""
    user_settings.SETTINGS_DIR.mkdir(parents=True, exist_ok=True)
    return save_state_file(user_settings.SETTINGS_DIR / DEFAULT_MIX_FILE_NAME)


def list_template_files():
    """Return the .dgl files in the Templates folder, sorted by name."""
    if not TEMPLATES_DIR.exists():
        return []
    return sorted(TEMPLATES_DIR.glob(f"*{state_file_extension}"), key=lambda p: p.name.lower())


def get_enabled_insert_options():
    return [
        (i, dpg.get_value(f"insert_name_{i}") or f"Port {i} Insert")
        for i in range(1, num_inserts + 1)
        if dpg.does_item_exist(f"insert_enable_{i}") and dpg.get_value(f"insert_enable_{i}")
    ]


def _collect_current_settings():
    inserts_data = [
        {
            "enabled": bool(dpg.get_value(f"insert_enable_{i}")) if dpg.does_item_exist(f"insert_enable_{i}") else False,
            "name": dpg.get_value(f"insert_name_{i}") if dpg.does_item_exist(f"insert_name_{i}") else "",
        }
        for i in range(1, num_inserts + 1)
    ]
    com_port = dpg.get_value("com_port_input") if dpg.does_item_exist("com_port_input") else USER_SETTINGS.get("com_port", "")
    baud_rate = dpg.get_value("baud_input") if dpg.does_item_exist("baud_input") else USER_SETTINGS.get("baud_rate", "")
    programmer_path = dpg.get_value("programmer_path_input") if dpg.does_item_exist("programmer_path_input") else USER_SETTINGS.get("programmer_path", "")
    firmware_path = dpg.get_value("firmware_path_input") if dpg.does_item_exist("firmware_path_input") else USER_SETTINGS.get("firmware_path", "")
    fallback_bin_path = dpg.get_value("fallback_bin_path_input") if dpg.does_item_exist("fallback_bin_path_input") else USER_SETTINGS.get("fallback_bin_path", "")
    fallback_volume = dpg.get_value("fallback_volume_input") if dpg.does_item_exist("fallback_volume_input") else USER_SETTINGS.get("fallback_volume", "")
    return {
        "light_theme": toggle_values.get("light_theme", False),
        "num_inserts": num_inserts,
        "inserts": inserts_data,
        "stereo_pairs": stereo_pairs,
        "chains": chains,
        "com_port": com_port,
        "baud_rate": baud_rate,
        "programmer_path": programmer_path,
        "firmware_path": firmware_path,
        "fallback_bin_path": fallback_bin_path,
        "fallback_volume": fallback_volume,
    }


def save_user_settings():
    user_settings.save_settings(_collect_current_settings())


def export_settings_file(file_path):
    """Write a copy of the current console settings profile to an arbitrary path."""
    user_settings.save_settings_to(file_path, _collect_current_settings())
    return Path(file_path)


def import_settings_file(file_path):
    """Load a console settings profile from an arbitrary path and apply it live."""
    global stereo_pairs, chains
    imported = user_settings.load_settings_from(file_path)

    is_light = bool(imported.get("light_theme", toggle_values.get("light_theme", False)))
    toggle_values["light_theme"] = is_light
    update_theme(is_light)

    stereo_pairs = list(imported.get("stereo_pairs", []))
    chains = list(imported.get("chains", []))
    # a fully-replaced chains list may no longer have an entry at whatever index a channel had selected
    for ch_state in channel_states.values():
        chain_idx = ch_state.get("chain")
        if chain_idx is not None and not (0 <= chain_idx < len(chains)):
            ch_state["chain"] = None

    for i, insert_data in enumerate(imported.get("inserts", []), start=1):
        if i > num_inserts:
            break
        enabled = bool(insert_data.get("enabled", False))
        name = insert_data.get("name", "")
        if dpg.does_item_exist(f"insert_enable_{i}"):
            dpg.set_value(f"insert_enable_{i}", enabled)
        if dpg.does_item_exist(f"insert_name_{i}"):
            dpg.configure_item(f"insert_name_{i}", enabled=enabled)
            dpg.set_value(f"insert_name_{i}", name)
        if dpg.does_item_exist(f"insert_click_catcher_{i}"):
            dpg.configure_item(f"insert_click_catcher_{i}", show=not enabled)

    if dpg.does_item_exist("com_port_input"):
        dpg.set_value("com_port_input", imported.get("com_port", ""))
    if dpg.does_item_exist("baud_input"):
        dpg.set_value("baud_input", str(imported.get("baud_rate", "")))
    if dpg.does_item_exist("programmer_path_input"):
        dpg.set_value("programmer_path_input", imported.get("programmer_path", ""))
    if dpg.does_item_exist("firmware_path_input"):
        dpg.set_value("firmware_path_input", imported.get("firmware_path", ""))
    if dpg.does_item_exist("fallback_bin_path_input"):
        dpg.set_value("fallback_bin_path_input", imported.get("fallback_bin_path", ""))
    if dpg.does_item_exist("fallback_volume_input"):
        dpg.set_value("fallback_volume_input", imported.get("fallback_volume", ""))

    notify_stereo_pairs_changed()
    notify_chains_changed()
    save_user_settings()
    return Path(file_path)


# --- Theming (global theme affects every window, so it lives here rather than in window.py) ---

def adjust_color(col, amount, is_light=False):
    delta = -amount if is_light else amount
    return tuple(max(0, min(255, c + delta)) for c in col[:3]) + ((col[3],) if len(col) == 4 else ())


def apply_knob_theme(tag, widget_type, color, light_color, indicator_color, is_light=False, grab_theme_col=dpg.mvThemeCol_SliderGrabActive):
    if is_light:
        effective_color = light_color if light_color is not None else knob_light_color
    else:
        effective_color = color if color is not None else knob_color

    theme_tag = f"{tag}_theme"
    if not dpg.does_item_exist(theme_tag):
        with dpg.theme(tag=theme_tag):
            with dpg.theme_component(widget_type):
                if effective_color is not None:
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBg, effective_color, tag=f"{tag}_theme_bg")
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgHovered, effective_color, tag=f"{tag}_theme_bg_hov")
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgActive, effective_color, tag=f"{tag}_theme_bg_act")
                if indicator_color is not None:
                    dpg.add_theme_color(grab_theme_col, indicator_color, tag=f"{tag}_theme_grab")
                    if grab_theme_col == dpg.mvThemeCol_SliderGrab:
                        dpg.add_theme_color(dpg.mvThemeCol_SliderGrabActive, indicator_color, tag=f"{tag}_theme_grab_act")
    else:
        if dpg.does_item_exist(f"{tag}_theme_bg"):
            dpg.set_value(f"{tag}_theme_bg", effective_color)
            dpg.set_value(f"{tag}_theme_bg_hov", effective_color)
            dpg.set_value(f"{tag}_theme_bg_act", effective_color)

    if dpg.does_item_exist(tag):
        dpg.bind_item_theme(tag, theme_tag)


def update_all_knob_themes(is_light=False):
    for group in GROUPS:
        for tag in group["tags"]:
            cfg = KNOB_CONFIG[tag]
            apply_knob_theme(tag, dpg.mvKnobFloat, cfg["color"], cfg["light_color"], cfg["indicator_color"], is_light=is_light)
    for tag in EXTRA_KNOB_TAGS:
        cfg = KNOB_CONFIG[tag]
        apply_knob_theme(tag, dpg.mvKnobFloat, cfg["color"], cfg["light_color"], cfg["indicator_color"], is_light=is_light)
    if FADER_CONFIG:
        tag = FADER_CONFIG["tag"]
        apply_knob_theme(tag, dpg.mvSliderFloat, FADER_CONFIG["color"], FADER_CONFIG["light_color"], FADER_CONFIG["indicator_color"], is_light=is_light, grab_theme_col=dpg.mvThemeCol_SliderGrab)


def create_initial_themes():
    with dpg.theme(tag="global_theme"):
        with dpg.theme_component(dpg.mvAll):
            dpg.add_theme_color(dpg.mvThemeCol_TitleBgActive, selected_window_top_bar_color, tag="gt_topbar")
            dpg.add_theme_color(dpg.mvThemeCol_TitleBgCollapsed, (35, 35, 35, 255), tag="gt_titlebgcollapsed")
            dpg.add_theme_color(dpg.mvThemeCol_WindowBg, (15, 15, 15, 255), tag="gt_winbg")
            dpg.add_theme_color(dpg.mvThemeCol_ChildBg, (15, 15, 15, 255), tag="gt_childbg")
            dpg.add_theme_color(dpg.mvThemeCol_PopupBg, (25, 25, 25, 255), tag="gt_popupbg")
            dpg.add_theme_color(dpg.mvThemeCol_Text, (255, 255, 255, 255), tag="gt_text")
            dpg.add_theme_color(dpg.mvThemeCol_TitleBg, (35, 35, 35, 255), tag="gt_titlebg")

            # Top file / menu bar
            dpg.add_theme_color(dpg.mvThemeCol_MenuBarBg, (25, 25, 25, 255), tag="gt_menubarbg")

            # Frame backgrounds for InputText, Checkbox, Combo, FileDialog text/selectors
            dpg.add_theme_color(dpg.mvThemeCol_FrameBg, textbox_color, tag="gt_framebg")
            dpg.add_theme_color(dpg.mvThemeCol_FrameBgHovered, adjust_color(textbox_color, 15, False), tag="gt_framebg_hov")
            dpg.add_theme_color(dpg.mvThemeCol_FrameBgActive, adjust_color(textbox_color, 30, False), tag="gt_framebg_act")

            # Buttons (Combo dropdown arrow button, file dialog buttons, standalone buttons, etc.)
            dpg.add_theme_color(dpg.mvThemeCol_Button, button_color, tag="gt_btn")
            dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, adjust_color(button_color, 20, False), tag="gt_btn_hov")
            dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, adjust_color(button_color, 40, False), tag="gt_btn_act")

            # Header colors for dropdown items, menu item hovers, file dialog tables
            dpg.add_theme_color(dpg.mvThemeCol_Header, (50, 50, 70, 255), tag="gt_header")
            dpg.add_theme_color(dpg.mvThemeCol_HeaderHovered, (70, 70, 95, 255), tag="gt_header_hov")
            dpg.add_theme_color(dpg.mvThemeCol_HeaderActive, (90, 90, 115, 255), tag="gt_header_act")

            # Table rows & headers in FileDialog / Tables
            dpg.add_theme_color(dpg.mvThemeCol_TableRowBg, (20, 20, 25, 255), tag="gt_tablerow")
            dpg.add_theme_color(dpg.mvThemeCol_TableRowBgAlt, (30, 30, 35, 255), tag="gt_tablerowalt")
            dpg.add_theme_color(dpg.mvThemeCol_TableHeaderBg, (40, 40, 50, 255), tag="gt_tableheader")
            dpg.add_theme_color(dpg.mvThemeCol_TableBorderStrong, (60, 60, 70, 255), tag="gt_tableborder_str")
            dpg.add_theme_color(dpg.mvThemeCol_TableBorderLight, (45, 45, 55, 255), tag="gt_tableborder_lt")

            # Scrollbars
            dpg.add_theme_color(dpg.mvThemeCol_ScrollbarBg, (25, 25, 30, 255), tag="gt_scrollbg")
            dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrab, (70, 70, 90, 255), tag="gt_scrollgrab")
            dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabHovered, (90, 90, 110, 255), tag="gt_scrollgrab_hov")
            dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabActive, (110, 110, 130, 255), tag="gt_scrollgrab_act")

            # Checkmark color
            dpg.add_theme_color(dpg.mvThemeCol_CheckMark, selected_window_top_bar_color, tag="gt_checkmark")

        with dpg.theme_component(dpg.mvKnobFloat):
            dpg.add_theme_color(dpg.mvThemeCol_FrameBg, knob_color, tag="gt_knob")

    dpg.bind_theme("global_theme")

    with dpg.theme(tag="toggle_theme"):
        with dpg.theme_component(dpg.mvButton):
            dpg.add_theme_color(dpg.mvThemeCol_Button, button_color, tag="tt_btn")
            dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, adjust_color(button_color, 20, False), tag="tt_btn_hov")
            dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, adjust_color(button_color, 40, False), tag="tt_btn_act")

    # Top-bar tab buttons (Control / Console Settings): selected uses the app's accent color,
    # unselected is just a slight shade off the menu bar background.
    with dpg.theme(tag="top_tab_selected_theme"):
        with dpg.theme_component(dpg.mvButton):
            dpg.add_theme_color(dpg.mvThemeCol_Button, selected_window_top_bar_color, tag="tts_btn")
            dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, adjust_color(selected_window_top_bar_color, 15, False), tag="tts_btn_hov")
            dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, adjust_color(selected_window_top_bar_color, 30, False), tag="tts_btn_act")

    with dpg.theme(tag="top_tab_unselected_theme"):
        with dpg.theme_component(dpg.mvButton):
            dpg.add_theme_color(dpg.mvThemeCol_Button, (35, 35, 35, 255), tag="ttu_btn")
            dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, (45, 45, 45, 255), tag="ttu_btn_hov")
            dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, (55, 55, 55, 255), tag="ttu_btn_act")

    for toggle in TOGGLE_CONFIG:
        tag = toggle["tag"]
        with dpg.theme(tag=f"{tag}_active_theme"):
            with dpg.theme_component(dpg.mvButton):
                dpg.add_theme_color(dpg.mvThemeCol_Button, toggle["on_color"], tag=f"{tag}_active_theme_col")
                dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, toggle["on_color"])
                dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, toggle["on_color"])


def update_theme(is_light=False):
    top_bar_col = selected_window_top_bar_light_color if is_light else selected_window_top_bar_color
    btn_col = button_light_color if is_light else button_color
    txtbox_col = textbox_light_color if is_light else textbox_color
    knb_col = knob_light_color if is_light else knob_color
    menubar_col = (235, 235, 240, 255) if is_light else (25, 25, 25, 255)
    header_col = (210, 210, 225, 255) if is_light else (50, 50, 70, 255)
    win_bg = (255, 255, 255, 255) if is_light else (15, 15, 15, 255)
    popup_bg = (245, 245, 245, 255) if is_light else (25, 25, 25, 255)

    if dpg.does_item_exist("gt_topbar"):
        title_bg_col = (220, 220, 220, 255) if is_light else (35, 35, 35, 255)
        dpg.set_value("gt_topbar", top_bar_col)
        dpg.set_value("gt_titlebgcollapsed", title_bg_col)
        dpg.set_value("gt_winbg", win_bg)
        dpg.set_value("gt_childbg", win_bg)
        dpg.set_value("gt_popupbg", popup_bg)
        dpg.set_value("gt_text", (20, 20, 20, 255) if is_light else (255, 255, 255, 255))
        dpg.set_value("gt_titlebg", title_bg_col)

        dpg.set_value("gt_menubarbg", menubar_col)

        dpg.set_value("gt_framebg", txtbox_col)
        dpg.set_value("gt_framebg_hov", adjust_color(txtbox_col, 15, is_light))
        dpg.set_value("gt_framebg_act", adjust_color(txtbox_col, 30, is_light))

        dpg.set_value("gt_header", header_col)
        dpg.set_value("gt_header_hov", adjust_color(header_col, 15, is_light))
        dpg.set_value("gt_header_act", adjust_color(header_col, 30, is_light))

        dpg.set_value("gt_tablerow", (250, 250, 252, 255) if is_light else (20, 20, 25, 255))
        dpg.set_value("gt_tablerowalt", (240, 240, 245, 255) if is_light else (30, 30, 35, 255))
        dpg.set_value("gt_tableheader", (225, 225, 235, 255) if is_light else (40, 40, 50, 255))
        dpg.set_value("gt_tableborder_str", (200, 200, 215, 255) if is_light else (60, 60, 70, 255))
        dpg.set_value("gt_tableborder_lt", (220, 220, 230, 255) if is_light else (45, 45, 55, 255))

        dpg.set_value("gt_scrollbg", (240, 240, 245, 255) if is_light else (25, 25, 30, 255))
        dpg.set_value("gt_scrollgrab", (200, 200, 210, 255) if is_light else (70, 70, 90, 255))
        dpg.set_value("gt_scrollgrab_hov", (180, 180, 195, 255) if is_light else (90, 90, 110, 255))
        dpg.set_value("gt_scrollgrab_act", (160, 160, 180, 255) if is_light else (110, 110, 130, 255))

        dpg.set_value("gt_checkmark", top_bar_col)

        dpg.set_value("gt_btn", btn_col)
        dpg.set_value("gt_btn_hov", adjust_color(btn_col, 20, is_light))
        dpg.set_value("gt_btn_act", adjust_color(btn_col, 40, is_light))

        dpg.set_value("tt_btn", btn_col)
        dpg.set_value("tt_btn_hov", adjust_color(btn_col, 20, is_light))
        dpg.set_value("tt_btn_act", adjust_color(btn_col, 40, is_light))

        dpg.set_value("gt_knob", knb_col)

    # The file dialog's extension-highlight color isn't part of a theme, so it must be set directly.
    ext_col = (20, 20, 20, 255) if is_light else (255, 255, 255, 255)
    if dpg.does_item_exist("save_dialog_ext"):
        dpg.configure_item("save_dialog_ext", color=ext_col)
    if dpg.does_item_exist("open_dialog_ext"):
        dpg.configure_item("open_dialog_ext", color=ext_col)

    if dpg.does_item_exist("tts_btn"):
        dpg.set_value("tts_btn", top_bar_col)
        dpg.set_value("tts_btn_hov", adjust_color(top_bar_col, 15, is_light))
        dpg.set_value("tts_btn_act", adjust_color(top_bar_col, 30, is_light))

        unselected_col = adjust_color(menubar_col, 12, is_light)
        dpg.set_value("ttu_btn", unselected_col)
        dpg.set_value("ttu_btn_hov", adjust_color(menubar_col, 22, is_light))
        dpg.set_value("ttu_btn_act", adjust_color(menubar_col, 32, is_light))

    for toggle in TOGGLE_CONFIG:
        tag = toggle["tag"]
        on_col = toggle["on_light_color"] if (is_light and toggle["on_light_color"] is not None) else toggle["on_color"]
        if dpg.does_item_exist(f"{tag}_active_theme_col"):
            dpg.set_value(f"{tag}_active_theme_col", on_col)

    update_all_knob_themes(is_light)
    for cycle_btn in CYCLE_CONFIG:
        update_cycle_display(cycle_btn["tag"])
