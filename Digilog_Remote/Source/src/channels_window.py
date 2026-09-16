"""Channels tab content: one tall narrow column per channel with its name, pan knob, fader,
insert/chain dropdown, and channel number. The name/pan/fader/insert widgets are truly linked
to the Control tab's and Inserts tab's shared widgets for whichever channel they represent -
editing either copy updates the other and sends the same UART messages, keyed to that row's
channel rather than always the Control tab's current channel."""
import dearpygui.dearpygui as dpg
import app_state
import inserts_window
from config import FADER_CONFIG, KNOB_CONFIG, TOGGLE_CONFIG, configured_font_path

_ROW_WIDTH = 100
_ROW_HEIGHT = 560
_PAN_SIZE = 60
_FADER_WIDTH = 30
_FADER_HEIGHT = 220
_INSERT_DROPDOWN_WIDTH = 80
_BIG_NUMBER_SIZE = 32

_NAME_POS = [12, 8]
# clickable solo/mute buttons just under the name box (yellow/red when on, grey when off),
# linked to the Control tab's SOLO/MUTE buttons the same way SEL is - each is sized to nearly
# half the row's width, with a small gap between them
_MUTE_SOLO_WIDTH = 34
_MUTE_SOLO_HEIGHT = 34
_SOLO_POS = [8, 40]
_MUTE_POS = [_ROW_WIDTH - 8 - _MUTE_SOLO_WIDTH, 40]
_SOLO_SW_ID = next((t["sw_id"] for t in TOGGLE_CONFIG if t["tag"] == "solo"), None)
_MUTE_SW_ID = next((t["sw_id"] for t in TOGGLE_CONFIG if t["tag"] == "mute"), None)
# both offset 30px down from their natural spot below the name box; the fader is also offset
# 8px further right than the pan knob
_PAN_POS = [32, 150]
_FADER_POS = [35, 210]
# below the bottom of the fader (210 + 220 = 430)
_INSERT_DROPDOWN_POS = [10, 440]
_CHANNEL_NUM_POS = [38, 478]
# just under the big channel number
_LINK_MARKER_POS = [42, 518]
# rows start 40px lower than the tab's top edge - they were getting clipped by the top bar
_ROWS_POS = [10, 50]
# below the bottom of the rows (50 + 560 = 610)
_LINK_BTN_POS = [10, 620]
_CHAIN_ICON_SIZE = (18, 12)
_CHAIN_ICON_COLOR = [160, 160, 160, 255]

_PAN_CFG = KNOB_CONFIG.get("pan")

# Selected rows (by index), in click order - the order matters for the multi-select "sel" payload.
_selected_rows = []
# Last plain/Cmd-or-Ctrl click, used as the start of a Shift-click range; None until a first click.
_select_anchor = None
# The single channel the user most recently clicked/selected (regardless of modifiers) - used as
# the fallback selection right after unlinking a group.
_last_clicked_idx = None
# Link groups made via the "Link Channels" button - each is a set of channel indices linked
# together. A channel can only belong to one group; linking overlapping selections merges them.
_link_groups = []


def _name_tag(i):
    return f"channels_name_{i}"


def _pan_tag(i):
    return f"channels_pan_{i}"


def _fader_tag(i):
    return f"channels_fader_{i}"


def _row_tag(i):
    return f"channels_row_{i}"


def _insert_dropdown_tag(i):
    return f"channels_insert_dropdown_{i}"


def _channel_num_tag(i):
    return f"channels_num_{i}"


def _link_marker_tag(i):
    return f"channels_link_marker_{i}"


def _solo_tag(i):
    return f"channels_solo_{i}"


def _mute_tag(i):
    return f"channels_mute_{i}"


def pan_knob_tags():
    """Called by window.py's vertical-drag override so it also recognizes these per-row knobs."""
    return [_pan_tag(i) for i in range(len(app_state.CHANNEL_OPTIONS))]


def apply_vertical_drag_to_pan(tag, value):
    """Called by window.py's vertical-drag override for a Channels-tab row's pan knob, driving
    the send directly since the knob's own native callback is horizontal-only."""
    tags = pan_knob_tags()
    if tag not in tags:
        return
    idx = tags.index(tag)
    if idx < len(app_state.CHANNEL_OPTIONS):
        _pan_changed_callback(tag, value, app_state.CHANNEL_OPTIONS[idx])


def channel_link_group(idx):
    """Return the set of channel indices linked with idx (including idx itself), or None."""
    for group in _link_groups:
        if idx in group:
            return group
    return None


def effective_mirror_group(idx):
    """The set of channel indices (including idx) whose controls should stay in sync with idx's:
    the union of its persistent link group (if any) and the current multi-selection (if idx is
    part of one) - so a linked group plus extra unlinked channels selected alongside it all move
    together as one temporary group, not just the linked members on their own."""
    group = channel_link_group(idx)
    selection = set(_selected_rows) if (idx in _selected_rows and len(_selected_rows) > 1) else None
    if group and selection:
        return group | selection
    return group or selection


def mirror_targets(channel_key):
    """Other channel keys (excluding channel_key) whose controls should mirror a change made
    on channel_key, per effective_mirror_group. Empty if channel_key isn't grouped/selected."""
    if channel_key not in app_state.CHANNEL_OPTIONS:
        return []
    idx = app_state.CHANNEL_OPTIONS.index(channel_key)
    group = effective_mirror_group(idx)
    if not group:
        return []
    return [app_state.CHANNEL_OPTIONS[i] for i in sorted(group) if i != idx]


def _is_neighbor_pair(group):
    """True if group is exactly two adjacent channel indices (e.g. channels 3 & 4)."""
    if not group or len(group) != 2:
        return False
    a, b = sorted(group)
    return b - a == 1


def _apply_mirrored_knob_value(ch, tag, value, ctrl_id, minimum, maximum):
    if ch in app_state.channel_states:
        app_state.channel_states[ch]["knobs"][tag] = value
    if ch == app_state.current_channel:
        app_state.knob_values[tag] = value
        if dpg.does_item_exist(tag):
            dpg.set_value(tag, value)
    sync_knob_to_channel(ch, tag, value)
    app_state.notify_knob_change(tag, value, ctrl_id, minimum, maximum, channel=ch)


def mirror_knob_delta_to_others(source_channel_key, tag, delta, ctrl_id, minimum, maximum):
    """Apply the same relative change (not the source's absolute value) to every other channel
    in source_channel_key's linked group or temporary multi-selection, clamped to min/max, and
    send a "ud" for each of them. Channels with different starting values stay offset from each
    other instead of snapping together; one hitting an edge just clips and loses that headroom.

    Special case: for the pan knob only, if the group/selection is exactly two neighboring
    channels (e.g. 3 & 4), the other channel's pan is kept as the *opposite* value (negated)
    instead of an offset - turning one to -33 puts the other at +33."""
    if delta == 0:
        return
    targets = mirror_targets(source_channel_key)
    if not targets:
        return
    if tag == "pan" and source_channel_key in app_state.CHANNEL_OPTIONS:
        source_idx = app_state.CHANNEL_OPTIONS.index(source_channel_key)
        if _is_neighbor_pair(effective_mirror_group(source_idx)):
            new_value = app_state.channel_states.get(source_channel_key, {}).get("knobs", {}).get(tag)
            if new_value is not None:
                opposite = max(minimum, min(maximum, -new_value))
                for ch in targets:
                    _apply_mirrored_knob_value(ch, tag, opposite, ctrl_id, minimum, maximum)
                return
    for ch in targets:
        current = app_state.channel_states.get(ch, {}).get("knobs", {}).get(tag, minimum)
        value = max(minimum, min(maximum, current + delta))
        _apply_mirrored_knob_value(ch, tag, value, ctrl_id, minimum, maximum)


def mirror_toggle_to_others(source_channel_key, tag, sw_id, is_on):
    """Apply the same toggle state to every other channel in the group/selection and send a
    "sw" for each of them."""
    for ch in mirror_targets(source_channel_key):
        if ch in app_state.channel_states and "toggles" in app_state.channel_states[ch]:
            app_state.channel_states[ch]["toggles"][tag] = is_on
        if ch == app_state.current_channel:
            # keep the Control tab's own button in sync too, in case it's a *mirror target*
            # rather than the channel that was actually clicked
            app_state.toggle_values[tag] = is_on
            if dpg.does_item_exist(tag):
                dpg.bind_item_theme(tag, f"{tag}_active_theme" if is_on else "toggle_theme")
        if app_state.on_button_change_handler and sw_id is not None:
            app_state.on_button_change_handler(app_state.CHANNEL_OPTIONS.index(ch), sw_id, int(is_on), tag)
    if tag in ("mute", "solo"):
        refresh_mute_solo_indicators()


def mirror_cycle_to_others(source_channel_key, tag, sw_id, index_value):
    """Apply the same cycle-button state to every other channel in the group/selection and send
    a "sw" for each of them."""
    state_bits = format(index_value, "02b")
    for ch in mirror_targets(source_channel_key):
        if ch in app_state.channel_states:
            app_state.channel_states[ch].setdefault("cycles", {})[tag] = index_value
        if app_state.on_button_change_handler:
            app_state.on_button_change_handler(app_state.CHANNEL_OPTIONS.index(ch), sw_id, state_bits, tag)


def add_chain_link_icon(pos, tag, show=True):
    """Draw a small grey chain-link icon (two overlapping rings) at pos. Used both for a
    Channels tab row's marker and for the Control tab's linked-channel indicator."""
    with dpg.drawlist(tag=tag, width=_CHAIN_ICON_SIZE[0], height=_CHAIN_ICON_SIZE[1], pos=pos, show=show):
        dpg.draw_circle((5, 6), 5, color=_CHAIN_ICON_COLOR, thickness=2)
        dpg.draw_circle((12, 6), 5, color=_CHAIN_ICON_COLOR, thickness=2)


def _name_edited_callback(sender, app_data, user_data):
    ch = user_data
    # %, $, ^ are the wire protocol's framing/delimiter characters and must never appear in a channel name.
    if any(c in app_data for c in ("%", "$", "^")):
        app_data = "".join(c for c in app_data if c not in ("%", "$", "^"))
        dpg.set_value(sender, app_data)
    if ch in app_state.channel_states:
        app_state.channel_states[ch]["name"] = app_data
    app_state.update_channel_dropdown()
    if ch == app_state.current_channel and dpg.does_item_exist("channel_name_input"):
        dpg.set_value("channel_name_input", app_data)


def _name_deactivated_callback(sender, app_data, user_data):
    # shared item_handler_registry callbacks report the triggering item via app_data, not sender
    item_tag = app_data
    if not (isinstance(item_tag, str) and item_tag.startswith("channels_name_")):
        return
    idx = int(item_tag.rsplit("_", 1)[-1])
    if idx >= len(app_state.CHANNEL_OPTIONS):
        return
    ch = app_state.CHANNEL_OPTIONS[idx]
    # Only send the name once the user is done editing (clicks away / tabs out / presses Enter).
    name = dpg.get_value(item_tag)
    if not name.strip():
        name = ch
        dpg.set_value(item_tag, name)
        if ch in app_state.channel_states:
            app_state.channel_states[ch]["name"] = name
        app_state.update_channel_dropdown()
        if ch == app_state.current_channel and dpg.does_item_exist("channel_name_input"):
            dpg.set_value("channel_name_input", name)
    if app_state.on_channel_name_change_handler:
        app_state.on_channel_name_change_handler(idx, name)


def _pan_changed_callback(sender, app_data, user_data):
    ch = user_data
    prev_value = app_state.channel_states.get(ch, {}).get("knobs", {}).get("pan", app_data)
    if ch in app_state.channel_states:
        app_state.channel_states[ch]["knobs"]["pan"] = app_data
    if ch == app_state.current_channel:
        app_state.knob_values["pan"] = app_data
        if dpg.does_item_exist("pan"):
            dpg.set_value("pan", app_data)
    if _PAN_CFG is not None:
        # the canonical "pan" tag (not this row's own widget tag) is what app_state's debounced
        # final-value send looks up in KNOB_CONFIG and in channel_states[ch]["knobs"]
        app_state.notify_knob_change("pan", app_data, _PAN_CFG["ctrl_id"], _PAN_CFG["min"], _PAN_CFG["max"], channel=ch)
        mirror_knob_delta_to_others(ch, "pan", app_data - prev_value, _PAN_CFG["ctrl_id"], _PAN_CFG["min"], _PAN_CFG["max"])


def _fader_changed_callback(sender, app_data, user_data):
    ch = user_data
    tag = FADER_CONFIG["tag"]
    prev_value = app_state.channel_states.get(ch, {}).get("knobs", {}).get(tag, app_data)
    if ch in app_state.channel_states:
        app_state.channel_states[ch]["knobs"][tag] = app_data
    if ch == app_state.current_channel:
        app_state.knob_values[tag] = app_data
        if dpg.does_item_exist(tag):
            dpg.set_value(tag, app_data)
    # the canonical fader tag (not this row's own widget tag) is what app_state's debounced
    # final-value send looks up in KNOB_CONFIG/FADER_CONFIG and in channel_states[ch]["knobs"]
    app_state.notify_knob_change(tag, app_data, FADER_CONFIG["ctrl_id"], FADER_CONFIG["min"], FADER_CONFIG["max"], channel=ch)
    mirror_knob_delta_to_others(ch, tag, app_data - prev_value, FADER_CONFIG["ctrl_id"], FADER_CONFIG["min"], FADER_CONFIG["max"])


def sync_knob_to_channel(channel_key, tag, value):
    """Set a specific channel's Channels-tab pan/fader widget (if it has one for this tag)."""
    if channel_key not in app_state.CHANNEL_OPTIONS:
        return
    idx = app_state.CHANNEL_OPTIONS.index(channel_key)
    if FADER_CONFIG and tag == FADER_CONFIG["tag"] and dpg.does_item_exist(_fader_tag(idx)):
        dpg.set_value(_fader_tag(idx), value)
    elif tag == "pan" and dpg.does_item_exist(_pan_tag(idx)):
        dpg.set_value(_pan_tag(idx), value)


def sync_knob_from_control(tag, value):
    """Called by window.py's Control-tab knob_callback so the current channel's row mirrors it."""
    sync_knob_to_channel(app_state.current_channel, tag, value)


def sync_name_from_control(channel_key, name):
    """Called by window.py's Control-tab channel-name callbacks so the matching row mirrors it."""
    if channel_key not in app_state.CHANNEL_OPTIONS:
        return
    idx = app_state.CHANNEL_OPTIONS.index(channel_key)
    if dpg.does_item_exist(_name_tag(idx)):
        dpg.set_value(_name_tag(idx), name)


def _insert_dropdown_clicked(sender, app_data, user_data):
    inserts_window.open_insert_popup(user_data, sender)


def refresh_insert_dropdowns():
    for i, ch in enumerate(app_state.CHANNEL_OPTIONS):
        tag = _insert_dropdown_tag(i)
        if not dpg.does_item_exist(tag):
            continue
        label, is_broken = inserts_window.insert_label_for_channel(ch)
        dpg.configure_item(tag, label=label)
        dpg.bind_item_theme(tag, "broken_link_theme" if is_broken else 0)


def _ensure_big_number_font():
    if dpg.does_item_exist("channels_big_number_font") or not configured_font_path.exists():
        return
    with dpg.font_registry():
        dpg.add_font(str(configured_font_path), _BIG_NUMBER_SIZE, tag="channels_big_number_font")


def _ensure_selected_row_theme():
    if dpg.does_item_exist("channels_row_selected_theme"):
        return
    with dpg.theme(tag="channels_row_selected_theme"):
        with dpg.theme_component(dpg.mvChildWindow):
            dpg.add_theme_color(dpg.mvThemeCol_Border, [230, 200, 40, 255])
            dpg.add_theme_style(dpg.mvStyleVar_ChildBorderSize, 4)


def refresh_mute_solo_indicators():
    # reuse the exact themes Control tab's own MUTE/SOLO toggle buttons use (grey off, on_color
    # on) instead of maintaining separate colors here
    for i, ch in enumerate(app_state.CHANNEL_OPTIONS):
        toggles = app_state.channel_states.get(ch, {}).get("toggles", {})
        if dpg.does_item_exist(_mute_tag(i)):
            dpg.bind_item_theme(_mute_tag(i), "mute_active_theme" if toggles.get("mute") else "toggle_theme")
        if dpg.does_item_exist(_solo_tag(i)):
            dpg.bind_item_theme(_solo_tag(i), "solo_active_theme" if toggles.get("solo") else "toggle_theme")


def _mute_solo_clicked_callback(sender, app_data, user_data):
    ch, tag, sw_id = user_data
    toggles = app_state.channel_states.setdefault(ch, {}).setdefault("toggles", {})
    new_value = not toggles.get(tag, False)
    toggles[tag] = new_value
    if ch == app_state.current_channel:
        app_state.toggle_values[tag] = new_value
        if dpg.does_item_exist(tag):
            dpg.bind_item_theme(tag, f"{tag}_active_theme" if new_value else "toggle_theme")
    if app_state.on_button_change_handler and sw_id is not None:
        app_state.on_button_change_handler(app_state.CHANNEL_OPTIONS.index(ch), sw_id, int(new_value), tag)
    mirror_toggle_to_others(ch, tag, sw_id, new_value)  # also refreshes every row's mute/solo theme



def _row_clicked_callback(sender=None, app_data=None):
    # DPG child windows don't support a clicked item_handler, so a global mouse-click handler
    # checks hover instead: the row must be hovered but none of ITS OWN controls may be
    for i in range(len(app_state.CHANNEL_OPTIONS)):
        row_tag = _row_tag(i)
        if not (dpg.does_item_exist(row_tag) and dpg.is_item_hovered(row_tag)):
            continue
        control_tags = [_name_tag(i), _pan_tag(i), _fader_tag(i), _insert_dropdown_tag(i), _solo_tag(i), _mute_tag(i)]
        if any(dpg.does_item_exist(t) and dpg.is_item_hovered(t) for t in control_tags):
            return
        _select_row(i)
        return


def _multi_select_mod_down():
    # Cmd on mac, Ctrl on Windows/Linux. This DPG build uses ImGui's 512+ named-key range for
    # its mvKey_* constants (confirmed via mvKey_Tab==512), and ImGui's macOS behaviors config
    # swaps physical Ctrl/Cmd(Super) onto io.KeyCtrl/io.KeySuper - so mvKey_ModCtrl alone already
    # reflects Cmd on mac and Ctrl elsewhere. mvKey_LWin/RWin are excluded: their values (343/347)
    # don't fit this build's key numbering at all and reading them is unreliable/always-on.
    return any(dpg.is_key_down(k) for k in (dpg.mvKey_ModCtrl, dpg.mvKey_LControl, dpg.mvKey_RControl))


def _range_select_mod_down():
    return any(dpg.is_key_down(k) for k in (dpg.mvKey_ModShift, dpg.mvKey_LShift, dpg.mvKey_RShift))


def _apply_row_selection_themes():
    for i in range(len(app_state.CHANNEL_OPTIONS)):
        tag = _row_tag(i)
        if dpg.does_item_exist(tag):
            dpg.bind_item_theme(tag, "channels_row_selected_theme" if i in _selected_rows else 0)


def _send_channel_select():
    if not (app_state.on_button_change_handler and _selected_rows):
        return
    # a lone selection sends a plain channel number; 2+ send them colon-joined, in selection order
    payload = _selected_rows[0] if len(_selected_rows) == 1 else list(_selected_rows)
    app_state.on_button_change_handler(payload, None, None, "sel")


def _expand_with_linked_groups(indices):
    """If any channel in indices belongs to a linked group, pull in the rest of that group too -
    selecting any member of a linked group always selects the whole group."""
    indices = list(indices)
    expanded = list(indices)
    for idx in indices:
        group = channel_link_group(idx)
        if group:
            for member in sorted(group):
                if member not in expanded:
                    expanded.append(member)
    return expanded


def _selected_group_match():
    """Return the link group if the current selection is exactly one whole existing group."""
    if len(_selected_rows) < 2:
        return None
    selected_set = set(_selected_rows)
    for group in _link_groups:
        if group == selected_set:
            return group
    return None


def _refresh_link_button():
    if not dpg.does_item_exist("channels_link_btn"):
        return
    if _selected_group_match() is not None:
        dpg.configure_item("channels_link_btn", label="Unlink Channels", show=True)
    elif len(_selected_rows) > 1:
        dpg.configure_item("channels_link_btn", label="Link Channels", show=True)
    else:
        dpg.configure_item("channels_link_btn", show=False)


def _refresh_link_markers():
    for i in range(len(app_state.CHANNEL_OPTIONS)):
        tag = _link_marker_tag(i)
        if dpg.does_item_exist(tag):
            dpg.configure_item(tag, show=channel_link_group(i) is not None)
    refresh_control_link_indicator()


def refresh_control_link_indicator():
    if not dpg.does_item_exist("control_link_icon"):
        return
    idx = app_state.CHANNEL_OPTIONS.index(app_state.current_channel) if app_state.current_channel in app_state.CHANNEL_OPTIONS else None
    group = channel_link_group(idx) if idx is not None else None
    dpg.configure_item("control_link_icon", show=group is not None)
    if group is not None:
        dpg.set_value("control_link_text", "(" + ", ".join(str(i + 1) for i in sorted(group)) + ")")
    dpg.configure_item("control_link_text", show=group is not None)


def refresh_control_sel_indicator():
    """Lights up the Control tab's SEL button iff its current channel is in the selection."""
    if not dpg.does_item_exist("sel"):
        return
    if app_state.current_channel not in app_state.CHANNEL_OPTIONS:
        return
    idx = app_state.CHANNEL_OPTIONS.index(app_state.current_channel)
    is_selected = idx in _selected_rows
    app_state.toggle_values["sel"] = is_selected
    dpg.bind_item_theme("sel", "sel_active_theme" if is_selected else "toggle_theme")


def _link_channels_callback(sender=None, app_data=None):
    group = _selected_group_match()
    if group is not None:
        _unlink_group(group)
    else:
        _link_selected_channels()


def _link_selected_channels():
    if len(_selected_rows) <= 1:
        return
    new_group = set(_selected_rows)
    # linking a channel that's already in another group merges the two groups together
    remaining = []
    for group in _link_groups:
        if group & new_group:
            new_group |= group
        else:
            remaining.append(group)
    remaining.append(new_group)
    _link_groups[:] = remaining
    _refresh_link_markers()
    _refresh_link_button()
    # same "sel" pipeline as a normal multi-select, just with "link" tacked onto the command
    if app_state.on_button_change_handler:
        app_state.on_button_change_handler(list(_selected_rows), None, None, "sel_link")


def _unlink_group(group):
    global _selected_rows, _select_anchor
    if group in _link_groups:
        _link_groups.remove(group)
    if app_state.on_button_change_handler:
        app_state.on_button_change_handler(sorted(group), None, None, "sel_unlink")
    _refresh_link_markers()
    # only the channel the user most recently clicked stays selected
    fallback = _last_clicked_idx if _last_clicked_idx in group else next(iter(sorted(group)), None)
    if fallback is not None:
        _selected_rows = [fallback]
        _select_anchor = fallback
        _finalize_selection()


def _select_row(idx):
    global _selected_rows, _select_anchor, _last_clicked_idx
    _last_clicked_idx = idx
    if _range_select_mod_down() and _select_anchor is not None:
        lo, hi = sorted((_select_anchor, idx))
        _selected_rows = _expand_with_linked_groups(range(lo, hi + 1))
        # the anchor stays put so further Shift-clicks keep extending the range from the same spot
    elif _multi_select_mod_down():
        group = channel_link_group(idx) or {idx}
        if idx in _selected_rows:
            # deselecting one member of a linked group deselects the whole group
            _selected_rows = [i for i in _selected_rows if i not in group]
        else:
            for member in sorted(group):
                if member not in _selected_rows:
                    _selected_rows.append(member)
        _select_anchor = idx
    else:
        _selected_rows = _expand_with_linked_groups([idx])
        _select_anchor = idx
    # safety net: whatever branch ran above, a linked channel must never end up selected alone
    _selected_rows = _expand_with_linked_groups(_selected_rows)
    _finalize_selection()


def _finalize_selection():
    _apply_row_selection_themes()
    _refresh_link_button()
    _send_channel_select()
    refresh_control_sel_indicator()


def select_current_channel_only(sender=None, app_data=None):
    """Called by the Control tab's SEL button: select (only) its current channel, like a plain
    row click, so the Channels tab's yellow border and this SEL button always agree."""
    global _selected_rows, _select_anchor, _last_clicked_idx
    if app_state.current_channel not in app_state.CHANNEL_OPTIONS:
        return
    idx = app_state.CHANNEL_OPTIONS.index(app_state.current_channel)
    _last_clicked_idx = idx
    _selected_rows = _expand_with_linked_groups([idx])
    _select_anchor = idx
    _finalize_selection()


def select_all_channels():
    global _selected_rows, _select_anchor, _last_clicked_idx
    if not app_state.CHANNEL_OPTIONS:
        return
    _selected_rows = list(range(len(app_state.CHANNEL_OPTIONS)))
    _select_anchor = _selected_rows[-1]
    _last_clicked_idx = _select_anchor
    _finalize_selection()


def _select_all_key_handler(sender=None, app_data=None):
    # only steal Cmd/Ctrl+A while the Channels tab is actually the visible one, and not while a
    # channel name text box has focus (so its normal "select all text" behavior still works)
    if not (dpg.does_item_exist("tab_channels_content") and dpg.is_item_shown("tab_channels_content")):
        return
    if not _multi_select_mod_down():
        return
    for i in range(len(app_state.CHANNEL_OPTIONS)):
        tag = _name_tag(i)
        if dpg.does_item_exist(tag) and dpg.is_item_active(tag):
            return
    select_all_channels()


def _build_rows(channels):
    dpg.delete_item("channels_rows_group", children_only=True)
    for i, ch in enumerate(channels):
        st = app_state.channel_states.get(ch, {})
        with dpg.child_window(tag=_row_tag(i), parent="channels_rows_group", width=_ROW_WIDTH, height=_ROW_HEIGHT):
            dpg.add_input_text(tag=_name_tag(i), pos=_NAME_POS, default_value=st.get("name", ch), width=_ROW_WIDTH - 24,
                               callback=_name_edited_callback, user_data=ch)
            dpg.bind_item_handler_registry(_name_tag(i), "channels_name_focus_handler")
            dpg.add_button(label="S", tag=_solo_tag(i), pos=_SOLO_POS, width=_MUTE_SOLO_WIDTH, height=_MUTE_SOLO_HEIGHT,
                           callback=_mute_solo_clicked_callback, user_data=(ch, "solo", _SOLO_SW_ID))
            dpg.add_button(label="M", tag=_mute_tag(i), pos=_MUTE_POS, width=_MUTE_SOLO_WIDTH, height=_MUTE_SOLO_HEIGHT,
                           callback=_mute_solo_clicked_callback, user_data=(ch, "mute", _MUTE_SW_ID))
            if _PAN_CFG is not None:
                dpg.add_knob_float(tag=_pan_tag(i), label=_PAN_CFG["label"], pos=_PAN_POS, width=_PAN_SIZE, height=_PAN_SIZE,
                                   default_value=st.get("knobs", {}).get("pan", _PAN_CFG["default"]),
                                   min_value=_PAN_CFG["min"], max_value=_PAN_CFG["max"],
                                   callback=_pan_changed_callback, user_data=ch)
            if FADER_CONFIG is not None:
                dpg.add_slider_float(tag=_fader_tag(i), label="", pos=_FADER_POS, width=_FADER_WIDTH, height=_FADER_HEIGHT, vertical=True,
                                     default_value=st.get("knobs", {}).get(FADER_CONFIG["tag"], FADER_CONFIG["default"]),
                                     min_value=FADER_CONFIG["min"], max_value=FADER_CONFIG["max"],
                                     callback=_fader_changed_callback, user_data=ch)
            dpg.add_button(tag=_insert_dropdown_tag(i), label="[No Inserts Enabled]", pos=_INSERT_DROPDOWN_POS,
                           width=_INSERT_DROPDOWN_WIDTH, callback=_insert_dropdown_clicked, user_data=ch)
            num_text = dpg.add_text(str(i + 1), tag=_channel_num_tag(i), pos=_CHANNEL_NUM_POS)
            if dpg.does_item_exist("channels_big_number_font"):
                dpg.bind_item_font(num_text, "channels_big_number_font")
            add_chain_link_icon(_LINK_MARKER_POS, _link_marker_tag(i))
            dpg.configure_item(_link_marker_tag(i), show=channel_link_group(i) is not None)
        dpg.bind_item_theme(_row_tag(i), "channels_row_selected_theme" if i in _selected_rows else 0)
    refresh_insert_dropdowns()
    refresh_mute_solo_indicators()


def refresh_channels_tab():
    if not dpg.does_item_exist("channels_rows_group"):
        return
    channels = app_state.CHANNEL_OPTIONS
    existing_rows = dpg.get_item_children("channels_rows_group", slot=1) or []
    if len(existing_rows) != len(channels):
        _build_rows(channels)
        return
    for i, ch in enumerate(channels):
        st = app_state.channel_states.get(ch, {})
        if dpg.does_item_exist(_name_tag(i)):
            dpg.set_value(_name_tag(i), st.get("name", ch))
        if _PAN_CFG is not None and dpg.does_item_exist(_pan_tag(i)):
            dpg.set_value(_pan_tag(i), st.get("knobs", {}).get("pan", _PAN_CFG["default"]))
        if FADER_CONFIG is not None and dpg.does_item_exist(_fader_tag(i)):
            dpg.set_value(_fader_tag(i), st.get("knobs", {}).get(FADER_CONFIG["tag"], FADER_CONFIG["default"]))
    refresh_insert_dropdowns()
    refresh_mute_solo_indicators()


def build_tab_content():
    _ensure_big_number_font()
    _ensure_selected_row_theme()
    with dpg.item_handler_registry(tag="channels_name_focus_handler"):
        dpg.add_item_deactivated_after_edit_handler(callback=_name_deactivated_callback)
    with dpg.handler_registry():
        dpg.add_mouse_click_handler(callback=_row_clicked_callback)
        dpg.add_key_press_handler(key=dpg.mvKey_A, callback=_select_all_key_handler)
    with dpg.group(tag="channels_rows_group", horizontal=True, pos=_ROWS_POS):
        pass
    _build_rows(app_state.CHANNEL_OPTIONS)
    dpg.add_button(label="Link Channels", tag="channels_link_btn", pos=_LINK_BTN_POS, show=False, callback=_link_channels_callback)
    refresh_control_link_indicator()
    refresh_control_sel_indicator()


app_state.register_channel_change_hook(refresh_channels_tab)
app_state.register_channel_change_hook(refresh_control_link_indicator)
app_state.register_channel_change_hook(refresh_control_sel_indicator)
app_state.register_channel_change_hook(refresh_mute_solo_indicators)
inserts_window.register_insert_dropdown_refresh_hook(refresh_insert_dropdowns)
app_state.register_incoming_knob_mirror_hook(mirror_knob_delta_to_others)

