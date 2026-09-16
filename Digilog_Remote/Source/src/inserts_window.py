"""Inserts tab content: enable/name insert ports, per-channel insert selection popup, and
the Create Stereo Pair dialog launcher. Built into the primary window's "Inserts" tab
by window.py rather than owning its own dpg.window."""
import time
import dearpygui.dearpygui as dpg
import app_state
import chains_dialog
import stereo_pair_dialog
from config import num_inserts, configured_font_path, button_label_size

_ROW_HEIGHT = 26
_TOP_PAD = 60
_SECTION_GAP = 15          # gap before each collapsible section header
_SECTION_HEADER_HEIGHT = 24
_SECTION_ROW_HEIGHT = 26
_SECTION_ADD_BTN_SIZE = 24

_SYNC_BTN_TAG = "sync_to_console_btn"
_SYNC_DONE_DISPLAY_SECONDS = 1.5
# Sync-to-console button state machine: hidden -> ready -> syncing -> done -> hidden.
# Any inserts-tab change while syncing/done bumps _sync_generation and jumps straight back to
# ready, so a stale in-flight ack result (still tagged with the old generation) is just ignored.
_sync_state = "hidden"
_sync_generation = 0
_sync_done_since = 0.0

_DISABLE_CONFIRM_WIDTH = 320
_DISABLE_CONFIRM_BASE_HEIGHT = 90
_DISABLE_CONFIRM_ROW_HEIGHT = 20
# Insert number awaiting the disable-confirmation dialog's answer, or None.
_pending_disable_insert = None

# Which channel the shared insert/chain selection popup currently targets - set by whichever
# button (the Control tab's, or a Channels tab row's) opened it. Falls back to current_channel.
_popup_channel = None
# Other modules (channels_window) that want to know when a channel's insert/chain label changed.
_insert_dropdown_refresh_hooks = []


def register_insert_dropdown_refresh_hook(fn):
    _insert_dropdown_refresh_hooks.append(fn)


def _notify_insert_dropdowns_changed():
    for hook in _insert_dropdown_refresh_hooks:
        hook()


def _popup_channel_key():
    return _popup_channel if _popup_channel is not None else app_state.current_channel


def _channel_insert_ids(ch_state):
    """Every insert port a channel's current selection occupies: either its single insert, or
    every insert belonging to its selected chain."""
    ids = set()
    if ch_state.get("insert") is not None:
        ids.add(ch_state["insert"])
    chain_idx = ch_state.get("chain")
    if chain_idx is not None and 0 <= chain_idx < len(app_state.chains):
        ids.update(app_state.chains[chain_idx].get("inserts", []))
    return ids


def insert_is_enabled(insert_num):
    return insert_num is not None and dpg.does_item_exist(f"insert_enable_{insert_num}") and dpg.get_value(f"insert_enable_{insert_num}")


def _chains_using_insert(insert_num):
    return [chain.get("name") or f"Chain {i + 1}" for i, chain in enumerate(app_state.chains)
            if insert_num in chain.get("inserts", [])]


def _stereo_pairs_using_insert(insert_num):
    return [pair.get("name") or f"Stereo Pair {i + 1}" for i, pair in enumerate(app_state.stereo_pairs)
            if pair.get("left") == insert_num or pair.get("right") == insert_num]


def _chain_is_broken(chain):
    return any(not insert_is_enabled(i) for i in chain.get("inserts", []))


def _pair_is_broken(pair):
    return not insert_is_enabled(pair.get("left")) or not insert_is_enabled(pair.get("right"))


def insert_enable_callback(sender, app_data, user_data):
    if not app_data:
        # disabling a port that a chain/stereo pair still references would silently break it -
        # confirm with the user first instead, vetoing the checkbox until they answer
        chain_names = _chains_using_insert(user_data)
        pair_names = _stereo_pairs_using_insert(user_data)
        if chain_names or pair_names:
            dpg.set_value(f"insert_enable_{user_data}", True)
            _show_disable_confirm_dialog(user_data, chain_names, pair_names)
            return
    _apply_insert_enabled(user_data, app_data)


def _apply_insert_enabled(user_data, app_data):
    dpg.configure_item(f"insert_name_{user_data}", enabled=app_data)
    # keep the invisible click-catcher in sync: only shown (on top of the box) while it's disabled
    if dpg.does_item_exist(f"insert_click_catcher_{user_data}"):
        dpg.configure_item(f"insert_click_catcher_{user_data}", show=not app_data)
    dpg.set_value(f"insert_name_{user_data}", f"Port {user_data} Insert" if app_data else "")
    if not app_data:
        # freeing up a disabled insert releases it from whichever channel had it selected directly,
        # and from whichever channel had a chain containing it selected. Chains/stereo pairs that
        # reference this port keep the reference - they just show up as broken (see refresh_*_list).
        for ch_state in app_state.channel_states.values():
            if ch_state.get("insert") == user_data:
                ch_state["insert"] = None
            chain_idx = ch_state.get("chain")
            if chain_idx is not None and 0 <= chain_idx < len(app_state.chains) and \
                    user_data in app_state.chains[chain_idx].get("inserts", []):
                ch_state["chain"] = None
    refresh_insert_select_popup()
    update_insert_dropdown_button()
    refresh_insert_usage_labels()
    refresh_stereo_pairs_list()
    refresh_chains_list()
    _notify_insert_dropdowns_changed()
    app_state.save_user_settings()
    app_state.notify_inserts_changed()


def _show_disable_confirm_dialog(insert_num, chain_names, pair_names):
    global _pending_disable_insert
    _pending_disable_insert = insert_num
    dpg.delete_item("insert_disable_confirm_list", children_only=True)
    for name in chain_names:
        dpg.add_text(f"- Chain: {name}", parent="insert_disable_confirm_list")
    for name in pair_names:
        dpg.add_text(f"- Stereo Pair: {name}", parent="insert_disable_confirm_list")
    num_rows = len(chain_names) + len(pair_names)
    height = _DISABLE_CONFIRM_BASE_HEIGHT + num_rows * _DISABLE_CONFIRM_ROW_HEIGHT
    viewport_width = dpg.get_viewport_client_width()
    viewport_height = dpg.get_viewport_client_height()
    dpg.configure_item(
        "insert_disable_confirm_dialog",
        show=True,
        height=height,
        pos=[max(0, (viewport_width - _DISABLE_CONFIRM_WIDTH) // 2), max(0, (viewport_height - height) // 2)],
    )


def _disable_confirm_ok_callback(sender=None, app_data=None):
    global _pending_disable_insert
    insert_num = _pending_disable_insert
    _pending_disable_insert = None
    dpg.hide_item("insert_disable_confirm_dialog")
    if insert_num is None:
        return
    dpg.set_value(f"insert_enable_{insert_num}", False)
    _apply_insert_enabled(insert_num, False)


def _disable_confirm_cancel_callback(sender=None, app_data=None):
    global _pending_disable_insert
    _pending_disable_insert = None
    dpg.hide_item("insert_disable_confirm_dialog")
    # the checkbox was already vetoed back to True when the dialog was raised - nothing else to do


def build_disable_confirm_dialog():
    with dpg.window(tag="insert_disable_confirm_dialog", label="Disable Insert?", modal=True, show=False,
                     no_resize=True, width=_DISABLE_CONFIRM_WIDTH, height=_DISABLE_CONFIRM_BASE_HEIGHT):
        dpg.add_text("Disabling this insert will break the following:", wrap=_DISABLE_CONFIRM_WIDTH - 20)
        with dpg.group(tag="insert_disable_confirm_list"):
            pass
        with dpg.group(horizontal=True):
            dpg.add_button(label="OK", callback=_disable_confirm_ok_callback)
            dpg.add_button(label="Cancel", callback=_disable_confirm_cancel_callback)


def insert_select_callback(sender, app_data, user_data):
    target_channel = _popup_channel_key()
    if target_channel in app_state.channel_states:
        # picking an insert (or [No Insert]) already in use elsewhere is blocked by disabling that selectable
        ch_state = app_state.channel_states[target_channel]
        ch_state["insert"] = user_data
        ch_state["chain"] = None
    dpg.hide_item("insert_select_popup")
    refresh_insert_select_popup()
    update_insert_dropdown_button()
    refresh_insert_usage_labels()
    _notify_insert_dropdowns_changed()


def chain_select_callback(sender, app_data, user_data):
    target_channel = _popup_channel_key()
    if target_channel in app_state.channel_states:
        # picking a chain already in use elsewhere is blocked by disabling that selectable
        ch_state = app_state.channel_states[target_channel]
        ch_state["chain"] = user_data
        ch_state["insert"] = None
    dpg.hide_item("insert_select_popup")
    refresh_insert_select_popup()
    update_insert_dropdown_button()
    refresh_insert_usage_labels()
    _notify_insert_dropdowns_changed()


def refresh_insert_select_popup():
    if not dpg.does_item_exist("insert_select_popup"):
        return
    target_channel = _popup_channel_key()
    ch_state = app_state.channel_states.get(target_channel, {})
    current_insert_sel = ch_state.get("insert")
    current_chain_sel = ch_state.get("chain")
    any_enabled = False
    for i in range(1, num_inserts + 1):
        tag = f"insert_select_{i}"
        is_enabled = dpg.does_item_exist(f"insert_enable_{i}") and dpg.get_value(f"insert_enable_{i}")
        if not is_enabled:
            dpg.configure_item(tag, show=False)
            continue
        any_enabled = True
        used_elsewhere = any(
            ch != target_channel and i in _channel_insert_ids(other_state)
            for ch, other_state in app_state.channel_states.items()
        )
        name = dpg.get_value(f"insert_name_{i}") or f"Port {i} Insert"
        is_sel = i == current_insert_sel
        dpg.configure_item(tag, label=name, show=True, enabled=(used_elsewhere is False or is_sel))
        dpg.set_value(tag, is_sel)
    dpg.configure_item("insert_select_none", show=any_enabled)
    dpg.set_value("insert_select_none", current_insert_sel is None and current_chain_sel is None)
    _refresh_chain_select_rows(current_chain_sel, target_channel)


def _refresh_chain_select_rows(current_chain_sel, target_channel):
    if not dpg.does_item_exist("insert_select_chains_group"):
        return
    dpg.delete_item("insert_select_chains_group", children_only=True)
    any_chains = False
    for idx, chain in enumerate(app_state.chains):
        chain_inserts = chain.get("inserts", [])
        if not chain_inserts:
            continue
        any_chains = True
        is_sel = idx == current_chain_sel
        used_elsewhere = any(
            ch != target_channel and set(chain_inserts) & _channel_insert_ids(other_state)
            for ch, other_state in app_state.channel_states.items()
        )
        name = chain.get("name") or f"Chain {idx + 1}"
        dpg.add_selectable(label=name, width=200, parent="insert_select_chains_group", callback=chain_select_callback,
                           user_data=idx, default_value=is_sel, enabled=(used_elsewhere is False or is_sel))
    dpg.configure_item("insert_select_chains_section", show=any_chains)


def insert_label_for_channel(channel_key):
    """Return (label, is_broken) for whatever insert/chain the given channel currently has selected."""
    ch_state = app_state.channel_states.get(channel_key, {})
    current_sel = ch_state.get("insert")
    current_chain = ch_state.get("chain")
    any_enabled = any(
        dpg.does_item_exist(f"insert_enable_{i}") and dpg.get_value(f"insert_enable_{i}")
        for i in range(1, num_inserts + 1)
    )
    if current_chain is not None and 0 <= current_chain < len(app_state.chains):
        chain = app_state.chains[current_chain]
        return (chain.get("name") or f"Chain {current_chain + 1}"), _chain_is_broken(chain)
    if current_sel is not None and dpg.does_item_exist(f"insert_name_{current_sel}"):
        return (dpg.get_value(f"insert_name_{current_sel}") or f"Port {current_sel} Insert"), False
    if any_enabled:
        return "[No Insert]", False
    return "[No Inserts Enabled]", False


def update_insert_dropdown_button():
    if not dpg.does_item_exist("insert_dropdown_btn"):
        return
    label, is_broken = insert_label_for_channel(app_state.current_channel)
    dpg.configure_item("insert_dropdown_btn", label=label)
    dpg.bind_item_theme("insert_dropdown_btn", "broken_link_theme" if is_broken else 0)


def open_insert_popup(channel_key, anchor_btn_tag):
    """Open the shared insert/chain selection popup targeting channel_key, anchored below anchor_btn_tag."""
    global _popup_channel
    _popup_channel = channel_key
    refresh_insert_select_popup()
    # rect_min/rect_max are in viewport (screen) space, unlike get_item_pos which is parent-relative
    btn_min = dpg.get_item_rect_min(anchor_btn_tag)
    btn_max = dpg.get_item_rect_max(anchor_btn_tag)
    dpg.configure_item("insert_select_popup", show=True, pos=[btn_min[0], btn_max[1]])


def insert_dropdown_btn_callback(sender, app_data):
    open_insert_popup(app_state.current_channel, "insert_dropdown_btn")


def insert_click_catcher_callback(sender, app_data, user_data):
    # A disabled InputText still hit-tests mouse clicks in this DearPyGui/ImGui build, and letting
    # ImGui activate/InputTextEx on a disabled box segfaults. An invisible button sits on top of the
    # box while it's disabled and intercepts the click instead, so the InputText is never activated.
    insert_num = user_data
    item_tag = f"insert_name_{insert_num}"
    dpg.set_value(f"insert_enable_{insert_num}", True)
    dpg.configure_item(item_tag, enabled=True)
    dpg.configure_item(f"insert_click_catcher_{insert_num}", show=False)
    refresh_insert_select_popup()
    update_insert_dropdown_button()
    refresh_insert_usage_labels()
    _notify_insert_dropdowns_changed()
    app_state.save_user_settings()
    app_state.notify_inserts_changed()
    # give the enabled state a full render cycle to settle before focusing the now-enabled box
    dpg.set_frame_callback(dpg.get_frame_count() + 1, lambda: dpg.focus_item(item_tag))


def insert_input_edited_callback(sender, app_data, user_data):
    item_tag = app_data
    if isinstance(item_tag, str) and item_tag.startswith("insert_name_"):
        refresh_insert_select_popup()
        update_insert_dropdown_button()
        refresh_insert_usage_labels()
        _notify_insert_dropdowns_changed()
        app_state.save_user_settings()
        app_state.notify_inserts_changed()


def _insert_usage_label(insert_num):
    """Return the '<channel>' or '<channel> - <chain>:<position>' text for whichever channel is
    using this insert (directly or via a chain), or None if it's unused."""
    for ch, ch_state in app_state.channel_states.items():
        name = ch_state.get("name") or ch
        if ch_state.get("insert") == insert_num:
            return name
        chain_idx = ch_state.get("chain")
        if chain_idx is not None and 0 <= chain_idx < len(app_state.chains):
            chain_inserts = app_state.chains[chain_idx].get("inserts", [])
            if insert_num in chain_inserts:
                chain_name = app_state.chains[chain_idx].get("name") or f"Chain {chain_idx + 1}"
                return f"{name} - {chain_name}:{chain_inserts.index(insert_num) + 1}"
    return None


def refresh_insert_usage_labels():
    for insert_num in range(1, num_inserts + 1):
        tag = f"insert_used_by_{insert_num}"
        if not dpg.does_item_exist(tag):
            continue
        label = _insert_usage_label(insert_num)
        dpg.set_value(tag, f"[{label}]" if label else "")


def add_insert_dropdown_button(pos, font=None):
    """Called by window.py to place the per-channel insert-selection button inside the primary window."""
    dpg.add_button(
        label="[No Inserts Enabled]",
        tag="insert_dropdown_btn",
        pos=pos,
        width=95,
        callback=insert_dropdown_btn_callback,
    )
    if font is not None:
        dpg.bind_item_font("insert_dropdown_btn", font)


def _ensure_click_catcher_theme():
    # fully transparent button theme so the click-catcher overlay is invisible but still clickable
    if dpg.does_item_exist("insert_click_catcher_theme"):
        return
    with dpg.theme(tag="insert_click_catcher_theme"):
        with dpg.theme_component(dpg.mvButton):
            dpg.add_theme_color(dpg.mvThemeCol_Button, [0, 0, 0, 0])
            dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, [0, 0, 0, 0])
            dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, [0, 0, 0, 0])
            dpg.add_theme_color(dpg.mvThemeCol_Text, [0, 0, 0, 0])
            dpg.add_theme_style(dpg.mvStyleVar_FrameBorderSize, 0)


def _ensure_broken_link_theme():
    # DPG has no italic font loaded, so a broken chain/pair is only marked with red text
    if dpg.does_item_exist("broken_link_theme"):
        return
    with dpg.theme(tag="broken_link_theme"):
        with dpg.theme_component(dpg.mvButton):
            dpg.add_theme_color(dpg.mvThemeCol_Text, [220, 60, 60, 255])


def _ensure_sync_btn_theme():
    if dpg.does_item_exist("sync_btn_done_theme"):
        return
    with dpg.theme(tag="sync_btn_done_theme"):
        with dpg.theme_component(dpg.mvButton):
            dpg.add_theme_color(dpg.mvThemeCol_Text, [80, 220, 120, 255])


def _show_sync_ready():
    global _sync_state
    _sync_state = "ready"
    if dpg.does_item_exist(_SYNC_BTN_TAG):
        dpg.bind_item_theme(_SYNC_BTN_TAG, 0)
        dpg.configure_item(_SYNC_BTN_TAG, show=True, enabled=True, label="Sync to Console")


def _on_inserts_changed():
    global _sync_generation
    # Bump the generation so any in-flight sync's result (or done-timer) is treated as stale, and
    # jump straight back to "ready" - this interrupts "Syncing..."/"Done!" instantly.
    _sync_generation += 1
    _show_sync_ready()


def _check_sync_done_expired(generation):
    global _sync_state
    if generation != _sync_generation or _sync_state != "done":
        return
    if time.monotonic() - _sync_done_since < _SYNC_DONE_DISPLAY_SECONDS:
        dpg.set_frame_callback(dpg.get_frame_count() + 1, lambda: _check_sync_done_expired(generation))
        return
    _sync_state = "hidden"
    if dpg.does_item_exist(_SYNC_BTN_TAG):
        dpg.configure_item(_SYNC_BTN_TAG, show=False)


def _handle_sync_result(success, generation):
    global _sync_state, _sync_done_since
    if generation != _sync_generation:
        return   # superseded by a newer change/sync attempt - the button has already moved on
    if not dpg.does_item_exist(_SYNC_BTN_TAG):
        return
    if success:
        _sync_state = "done"
        _sync_done_since = time.monotonic()
        dpg.configure_item(_SYNC_BTN_TAG, label="Done!")
        dpg.bind_item_theme(_SYNC_BTN_TAG, "sync_btn_done_theme")
        dpg.set_frame_callback(dpg.get_frame_count() + 1, lambda: _check_sync_done_expired(generation))
    else:
        # Failed/no ack after retries - leave it ready so the user can retry.
        _show_sync_ready()


def sync_to_console_callback(sender=None, app_data=None):
    global _sync_state, _sync_generation
    if _sync_state != "ready":
        return
    _sync_generation += 1
    generation = _sync_generation
    _sync_state = "syncing"
    dpg.bind_item_theme(_SYNC_BTN_TAG, 0)
    dpg.configure_item(_SYNC_BTN_TAG, enabled=False, label="Syncing...")
    if app_state.on_sync_to_console_handler:
        app_state.on_sync_to_console_handler(lambda success: _handle_sync_result(success, generation))
    else:
        _handle_sync_result(False, generation)


def _ensure_chains_heading_font():
    # no bold weight is bundled for the UI font, so a larger size is used to read as a heading instead
    if dpg.does_item_exist("insert_select_chains_heading_font") or not configured_font_path.exists():
        return
    with dpg.font_registry():
        dpg.add_font(str(configured_font_path), button_label_size + 4, tag="insert_select_chains_heading_font")


def build_insert_select_popup():
    # fixed selectable widths keep this autosize popup stable - a selectable left to stretch to fill
    # the available width feeds back into the window's autosize calculation and grows without bound
    _ensure_chains_heading_font()
    with dpg.window(tag="insert_select_popup", show=False, popup=True, no_title_bar=True, autosize=True):
        dpg.add_selectable(label="[No Insert]", tag="insert_select_none", width=200, callback=insert_select_callback, user_data=None)
        for insert_num in range(1, num_inserts + 1):
            dpg.add_selectable(label="", tag=f"insert_select_{insert_num}", width=200, show=False,
                               callback=insert_select_callback, user_data=insert_num)
        with dpg.group(tag="insert_select_chains_section", show=False):
            dpg.add_separator()
            dpg.add_text("Chains", tag="insert_select_chains_heading")
            with dpg.group(tag="insert_select_chains_group"):
                pass
    if dpg.does_item_exist("insert_select_chains_heading_font"):
        dpg.bind_item_font("insert_select_chains_heading", "insert_select_chains_heading_font")


def _stereo_section_header_y():
    return _TOP_PAD + _ROW_HEIGHT * num_inserts + _SECTION_GAP


def _stereo_header_open():
    return dpg.does_item_exist("stereo_pairs_header") and dpg.get_value("stereo_pairs_header")


def _chains_header_open():
    return dpg.does_item_exist("chains_header") and dpg.get_value("chains_header")


def _section_extent(is_open, num_items):
    # header row, plus (when expanded) one row per item and one for the trailing '+' button
    if not is_open:
        return _SECTION_HEADER_HEIGHT
    return _SECTION_HEADER_HEIGHT + (num_items + 1) * _SECTION_ROW_HEIGHT


def _chains_section_header_y():
    stereo_extent = _section_extent(_stereo_header_open(), len(app_state.stereo_pairs))
    return _stereo_section_header_y() + stereo_extent + _SECTION_GAP


def _relayout_sections():
    # the Chains section follows the Stereo Pairs section, so it must shift whenever that one resizes
    if dpg.does_item_exist("chains_header"):
        dpg.configure_item("chains_header", pos=[10, _chains_section_header_y()])


def stereo_header_toggled_callback(sender=None, app_data=None):
    _relayout_sections()


def chains_header_toggled_callback(sender=None, app_data=None):
    _relayout_sections()


def refresh_stereo_pairs_list():
    if not dpg.does_item_exist("stereo_pairs_list_group"):
        return
    dpg.delete_item("stereo_pairs_list_group", children_only=True)
    for i, pair in enumerate(app_state.stereo_pairs):
        btn_tag = f"stereo_pair_list_btn_{i}"
        dpg.add_button(label=pair.get("name", f"Stereo Pair {i + 1}"), width=200, tag=btn_tag,
                       parent="stereo_pairs_list_group", callback=stereo_pair_dialog.open_edit_dialog, user_data=i)
        dpg.bind_item_theme(btn_tag, "broken_link_theme" if _pair_is_broken(pair) else 0)
    # the add button always trails the last pair in the list (or sits alone when the list is empty)
    dpg.add_button(label="+", width=_SECTION_ADD_BTN_SIZE, height=_SECTION_ADD_BTN_SIZE,
                   parent="stereo_pairs_list_group", callback=stereo_pair_dialog.open_dialog)
    _relayout_sections()


def refresh_chains_list():
    if not dpg.does_item_exist("chains_list_group"):
        return
    dpg.delete_item("chains_list_group", children_only=True)
    for i, chain in enumerate(app_state.chains):
        btn_tag = f"chain_list_btn_{i}"
        dpg.add_button(label=chain.get("name", f"Chain {i + 1}"), width=200, tag=btn_tag,
                       parent="chains_list_group", callback=chains_dialog.open_edit_dialog, user_data=i)
        dpg.bind_item_theme(btn_tag, "broken_link_theme" if _chain_is_broken(chain) else 0)
    # the add button always trails the last chain in the list (or sits alone when the list is empty)
    dpg.add_button(label="+", width=_SECTION_ADD_BTN_SIZE, height=_SECTION_ADD_BTN_SIZE,
                   parent="chains_list_group", callback=chains_dialog.open_dialog)
    _relayout_sections()


def build_tab_content():
    loaded_inserts = app_state.USER_SETTINGS.get("inserts", [])
    _ensure_click_catcher_theme()
    _ensure_sync_btn_theme()
    _ensure_broken_link_theme()
    build_disable_confirm_dialog()
    dpg.add_button(label="Sync to Console", tag=_SYNC_BTN_TAG, pos=[10, 28], width=160, height=24,
                   show=False, callback=sync_to_console_callback)
    with dpg.item_handler_registry(tag="insert_input_focus_handler"):
        dpg.add_item_deactivated_after_edit_handler(callback=insert_input_edited_callback)
    with dpg.item_handler_registry(tag="stereo_header_toggle_handler"):
        dpg.add_item_toggled_open_handler(callback=stereo_header_toggled_callback)
    with dpg.item_handler_registry(tag="chains_header_toggle_handler"):
        dpg.add_item_toggled_open_handler(callback=chains_header_toggled_callback)

    stereo_header_y = _stereo_section_header_y()
    for insert_num in range(1, num_inserts + 1):
        row_y = _TOP_PAD + (insert_num - 1) * _ROW_HEIGHT
        saved_entry = loaded_inserts[insert_num - 1] if insert_num - 1 < len(loaded_inserts) else {}
        saved_enabled = bool(saved_entry.get("enabled", False))
        saved_name = saved_entry.get("name", "") if saved_enabled else ""
        # fixed pixel positions (rather than an auto-layout group) keep the number column aligned regardless of digit count
        dpg.add_checkbox(tag=f"insert_enable_{insert_num}", pos=[10, row_y], default_value=saved_enabled, callback=insert_enable_callback, user_data=insert_num)
        dpg.add_text(f"{insert_num}", pos=[35, row_y + 3])
        # transparent button overlay that catches clicks while the box is disabled (see insert_click_catcher_callback).
        # ImGui gives hover/click priority to whichever overlapping item is processed FIRST each frame, so this
        # must be added before the input text below it in order to actually intercept the click.
        catcher_tag = f"insert_click_catcher_{insert_num}"
        dpg.add_button(label="", tag=catcher_tag, pos=[60, row_y], width=200, height=24,
                       show=not saved_enabled, callback=insert_click_catcher_callback, user_data=insert_num)
        dpg.bind_item_theme(catcher_tag, "insert_click_catcher_theme")
        dpg.add_input_text(tag=f"insert_name_{insert_num}", pos=[60, row_y], width=200, enabled=saved_enabled, default_value=saved_name)
        dpg.bind_item_handler_registry(f"insert_name_{insert_num}", "insert_input_focus_handler")
        dpg.add_text("", tag=f"insert_used_by_{insert_num}", pos=[270, row_y + 3])

    with dpg.collapsing_header(label="Stereo Pairs", tag="stereo_pairs_header", pos=[10, stereo_header_y], default_open=True):
        with dpg.group(tag="stereo_pairs_list_group"):
            pass
    dpg.bind_item_handler_registry("stereo_pairs_header", "stereo_header_toggle_handler")

    with dpg.collapsing_header(label="Chains", tag="chains_header", pos=[10, 0], default_open=True):
        with dpg.group(tag="chains_list_group"):
            pass
    dpg.bind_item_handler_registry("chains_header", "chains_header_toggle_handler")

    chains_dialog.build()
    stereo_pair_dialog.build()
    refresh_insert_select_popup()
    update_insert_dropdown_button()
    refresh_insert_usage_labels()
    refresh_stereo_pairs_list()
    refresh_chains_list()
    _notify_insert_dropdowns_changed()


# Keep the primary window's insert dropdown label in sync whenever the active channel changes.
app_state.register_channel_change_hook(update_insert_dropdown_button)
app_state.register_channel_change_hook(refresh_insert_usage_labels)
app_state.register_channel_change_hook(_notify_insert_dropdowns_changed)
app_state.register_stereo_pairs_change_hook(refresh_stereo_pairs_list)
app_state.register_chains_change_hook(refresh_chains_list)
# Chain name/membership edits (and deletions) can change what's shown as using a given insert,
# or what the current channel's dropdown button/popup should display.
app_state.register_chains_change_hook(refresh_insert_usage_labels)
app_state.register_chains_change_hook(update_insert_dropdown_button)
app_state.register_chains_change_hook(refresh_insert_select_popup)
app_state.register_chains_change_hook(_notify_insert_dropdowns_changed)
app_state.register_inserts_change_hook(_on_inserts_changed)
