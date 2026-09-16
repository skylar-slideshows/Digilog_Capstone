"""Create Chain dialog, launched from the Inserts window."""
import dearpygui.dearpygui as dpg
import app_state

WINDOW_TAG = "chain_dialog"
_DIALOG_WIDTH = 300
_BASE_HEIGHT = 110   # title bar + name field + Create/Cancel row + padding
_ROW_HEIGHT = 30
_ACTION_BTN_SIZE = 24

# Insert numbers added to the chain being built in the currently-open dialog, in order.
_chain_inserts = []
# Insert number currently selected in the list (for the Up/Down/X buttons), or None.
_selected_insert = None
# Index into app_state.chains currently being edited, or None while creating a new chain.
_editing_index = None
# Whether the Delete button is waiting for its confirming second click.
_delete_confirm_pending = False


def _reset_delete_confirm():
    global _delete_confirm_pending
    _delete_confirm_pending = False
    dpg.configure_item("chain_delete_btn", label="Delete")


def _show_centered():
    viewport_width = dpg.get_viewport_client_width()
    viewport_height = dpg.get_viewport_client_height()
    dialog_height = dpg.get_item_configuration(WINDOW_TAG)["height"]
    dpg.configure_item(
        WINDOW_TAG,
        show=True,
        pos=[max(0, (viewport_width - _DIALOG_WIDTH) // 2), max(0, (viewport_height - dialog_height) // 2)],
    )


def open_dialog(sender=None, app_data=None):
    global _chain_inserts, _editing_index, _selected_insert
    _chain_inserts = []
    _editing_index = None
    _selected_insert = None
    _reset_delete_confirm()
    dpg.configure_item(WINDOW_TAG, label="Create Chain")
    dpg.configure_item("chain_create_btn", label="Create")
    dpg.configure_item("chain_delete_btn", show=False)
    dpg.set_value("chain_name_input", "Unnamed FX chain")
    _refresh_chain_rows()
    _show_centered()


def open_edit_dialog(sender=None, app_data=None, user_data=None):
    global _chain_inserts, _editing_index, _selected_insert
    _editing_index = user_data
    _selected_insert = None
    _reset_delete_confirm()
    chain = app_state.chains[_editing_index]
    _chain_inserts = list(chain.get("inserts", []))
    dpg.configure_item(WINDOW_TAG, label="Edit Chain")
    dpg.configure_item("chain_create_btn", label="Save")
    dpg.configure_item("chain_delete_btn", show=True)
    dpg.set_value("chain_name_input", chain.get("name", ""))
    _refresh_chain_rows()
    _show_centered()


def _available_insert_options():
    used = set(_chain_inserts)
    return [(i, name) for i, name in app_state.get_enabled_insert_options() if i not in used]


def _row_tag(insert_num):
    return f"chain_insert_row_{insert_num}"


def _insert_select_callback(sender=None, app_data=None, user_data=None):
    name_to_insert = {name: i for i, name in _available_insert_options()}
    insert_num = name_to_insert.get(app_data)
    if insert_num is None:
        return
    _chain_inserts.append(insert_num)
    _refresh_chain_rows()


def _row_click_callback(sender=None, app_data=None, user_data=None):
    global _selected_insert
    _selected_insert = user_data
    _refresh_chain_rows()


def _global_click_callback(sender=None, app_data=None):
    # Clicking anywhere that isn't a chain row or one of the Up/Down/X buttons deselects.
    global _selected_insert
    if _selected_insert is None:
        return
    if not dpg.does_item_exist(WINDOW_TAG) or not dpg.is_item_shown(WINDOW_TAG):
        return
    kept_targets = [_row_tag(n) for n in _chain_inserts] + [
        "chain_move_up_btn", "chain_move_down_btn", "chain_delete_insert_btn",
    ]
    if any(dpg.does_item_exist(t) and dpg.is_item_hovered(t) for t in kept_targets):
        return
    _selected_insert = None
    _refresh_chain_rows()


def _move_selected(direction):
    global _selected_insert
    if _selected_insert is None or _selected_insert not in _chain_inserts:
        return
    idx = _chain_inserts.index(_selected_insert)
    new_idx = idx + direction
    if not (0 <= new_idx < len(_chain_inserts)):
        return
    _chain_inserts[idx], _chain_inserts[new_idx] = _chain_inserts[new_idx], _chain_inserts[idx]
    _refresh_chain_rows()


def _move_up_callback(sender=None, app_data=None):
    _move_selected(-1)


def _move_down_callback(sender=None, app_data=None):
    _move_selected(1)


def _delete_selected_callback(sender=None, app_data=None):
    global _selected_insert
    if _selected_insert is None or _selected_insert not in _chain_inserts:
        return
    _chain_inserts.remove(_selected_insert)
    _selected_insert = None
    _refresh_chain_rows()


def _create_callback(sender=None, app_data=None):
    name = (dpg.get_value("chain_name_input") or "").strip() or "Unnamed FX chain"
    chain = {"name": name, "inserts": list(_chain_inserts)}
    if _editing_index is None:
        app_state.chains.append(chain)
        status = f"Status: Created chain '{name}'"
    else:
        app_state.chains[_editing_index] = chain
        status = f"Status: Saved chain '{name}'"
    app_state.save_user_settings()
    app_state.notify_chains_changed()
    dpg.hide_item(WINDOW_TAG)
    app_state.update_status(status)


def delete_button_callback(sender=None, app_data=None):
    global _delete_confirm_pending
    if _editing_index is None:
        return
    if not _delete_confirm_pending:
        _delete_confirm_pending = True
        dpg.configure_item("chain_delete_btn", label="Are you sure?")
        return
    name = app_state.chains[_editing_index].get("name", "chain")
    del app_state.chains[_editing_index]
    app_state.release_deleted_chain(_editing_index)
    app_state.save_user_settings()
    app_state.notify_chains_changed()
    _reset_delete_confirm()
    dpg.hide_item(WINDOW_TAG)
    app_state.update_status(f"Status: Deleted chain '{name}'")


def cancel_callback(sender=None, app_data=None):
    _reset_delete_confirm()
    dpg.hide_item(WINDOW_TAG)


def _refresh_chain_rows():
    if not dpg.does_item_exist("chain_inserts_group"):
        return
    dpg.delete_item("chain_inserts_group", children_only=True)
    insert_names = dict(app_state.get_enabled_insert_options())
    for insert_num in _chain_inserts:
        is_disabled = insert_num not in insert_names
        name = insert_names.get(insert_num) if not is_disabled else f"Insert {insert_num} (Disabled)"
        row_tag = _row_tag(insert_num)
        dpg.add_button(label=name, width=260, tag=row_tag, parent="chain_inserts_group",
                       callback=_row_click_callback, user_data=insert_num)
        is_selected = insert_num == _selected_insert
        if is_disabled:
            theme = "chain_row_disabled_selected_theme" if is_selected else "chain_row_disabled_theme"
        else:
            theme = "chain_row_selected_theme" if is_selected else 0
        dpg.bind_item_theme(row_tag, theme)

    remaining_names = [name for _, name in _available_insert_options()]
    has_combo = bool(remaining_names)
    if has_combo:
        placeholder = f"[Select Insert {len(_chain_inserts) + 1}]"
        dpg.add_combo(tag="chain_insert_combo", items=[placeholder] + remaining_names, default_value=placeholder,
                      width=260, parent="chain_inserts_group", callback=_insert_select_callback)

    with dpg.group(horizontal=True, parent="chain_inserts_group"):
        dpg.add_spacer(width=176)
        dpg.add_button(label="^", tag="chain_move_up_btn", width=_ACTION_BTN_SIZE, height=_ACTION_BTN_SIZE,
                       callback=_move_up_callback)
        dpg.add_button(label="v", tag="chain_move_down_btn", width=_ACTION_BTN_SIZE, height=_ACTION_BTN_SIZE,
                       callback=_move_down_callback)
        dpg.add_button(label="X", tag="chain_delete_insert_btn", width=_ACTION_BTN_SIZE, height=_ACTION_BTN_SIZE,
                       callback=_delete_selected_callback)

    if dpg.does_item_exist(WINDOW_TAG):
        num_rows = len(_chain_inserts) + (1 if has_combo else 0) + 1   # insert rows + optional combo + action-button row
        dpg.configure_item(WINDOW_TAG, height=_BASE_HEIGHT + num_rows * _ROW_HEIGHT)

    has_selection = _selected_insert is not None and _selected_insert in _chain_inserts
    dpg.configure_item("chain_move_up_btn", enabled=has_selection)
    dpg.configure_item("chain_move_down_btn", enabled=has_selection)
    dpg.configure_item("chain_delete_insert_btn", enabled=has_selection)
    _update_create_button_state()


def _update_create_button_state():
    if not dpg.does_item_exist("chain_create_btn"):
        return
    valid = len(_chain_inserts) > 0
    dpg.configure_item("chain_create_btn", enabled=valid)
    dpg.bind_item_theme("chain_create_btn", 0 if valid else "dialog_create_btn_disabled_theme")


def _ensure_delete_button_theme():
    if dpg.does_item_exist("chain_delete_btn_theme"):
        return
    with dpg.theme(tag="chain_delete_btn_theme"):
        with dpg.theme_component(dpg.mvButton):
            dpg.add_theme_color(dpg.mvThemeCol_Text, [220, 50, 50, 255])


def _ensure_row_selected_theme():
    if dpg.does_item_exist("chain_row_selected_theme"):
        return
    with dpg.theme(tag="chain_row_selected_theme"):
        with dpg.theme_component(dpg.mvButton):
            dpg.add_theme_color(dpg.mvThemeCol_Button, [110, 70, 200, 255])
            dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, [125, 85, 215, 255])
            dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, [140, 100, 230, 255])


def _ensure_row_disabled_theme():
    # no italic font is loaded, so a disabled insert's row is marked with red text only
    if dpg.does_item_exist("chain_row_disabled_theme"):
        return
    with dpg.theme(tag="chain_row_disabled_theme"):
        with dpg.theme_component(dpg.mvButton):
            dpg.add_theme_color(dpg.mvThemeCol_Text, [220, 60, 60, 255])
    with dpg.theme(tag="chain_row_disabled_selected_theme"):
        with dpg.theme_component(dpg.mvButton):
            dpg.add_theme_color(dpg.mvThemeCol_Button, [110, 70, 200, 255])
            dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, [125, 85, 215, 255])
            dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, [140, 100, 230, 255])
            dpg.add_theme_color(dpg.mvThemeCol_Text, [220, 60, 60, 255])


def _ensure_disabled_create_btn_theme():
    # Shared with stereo_pair_dialog.py's Create/Save button - DPG's built-in disabled dimming
    # isn't visible against this app's custom button theme, so grey it out explicitly.
    if dpg.does_item_exist("dialog_create_btn_disabled_theme"):
        return
    with dpg.theme(tag="dialog_create_btn_disabled_theme"):
        with dpg.theme_component(dpg.mvButton):
            dpg.add_theme_color(dpg.mvThemeCol_Button, [60, 60, 60, 255])
            dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, [60, 60, 60, 255])
            dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, [60, 60, 60, 255])
            dpg.add_theme_color(dpg.mvThemeCol_Text, [120, 120, 120, 255])


def build():
    _ensure_delete_button_theme()
    _ensure_row_selected_theme()
    _ensure_row_disabled_theme()
    _ensure_disabled_create_btn_theme()
    with dpg.handler_registry():
        dpg.add_mouse_click_handler(callback=_global_click_callback)

    with dpg.window(tag=WINDOW_TAG, label="Create Chain", modal=True, show=False, width=_DIALOG_WIDTH, height=_BASE_HEIGHT + _ROW_HEIGHT, no_resize=True):
        dpg.add_input_text(tag="chain_name_input", default_value="Unnamed FX chain", width=260)
        with dpg.group(tag="chain_inserts_group"):
            pass
        with dpg.group(horizontal=True):
            dpg.add_button(label="Delete", tag="chain_delete_btn", show=False, callback=delete_button_callback)
            dpg.bind_item_theme("chain_delete_btn", "chain_delete_btn_theme")
            dpg.add_spacer(width=60)
            dpg.add_button(label="Create", tag="chain_create_btn", enabled=False, callback=_create_callback)
            dpg.bind_item_theme("chain_create_btn", "dialog_create_btn_disabled_theme")
            dpg.add_button(label="Cancel", callback=cancel_callback)
