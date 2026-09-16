"""Create/Edit Stereo Pair modal dialogs, launched from the Inserts window."""
import dearpygui.dearpygui as dpg
import app_state

WINDOW_TAG = "stereo_pair_dialog"

# Marks a combo's displayed value as a disabled-but-still-referenced insert placeholder.
_DISABLED_MARKER = " (Disabled)"

# Index into app_state.stereo_pairs currently being edited, or None while creating a new pair.
_editing_index = None
# Whether the Delete button is waiting for its confirming second click.
_delete_confirm_pending = False


def _reset_delete_confirm():
    global _delete_confirm_pending
    _delete_confirm_pending = False
    dpg.configure_item("stereo_pair_delete_btn", label="Delete")


def _insert_enabled(insert_num):
    return insert_num is not None and dpg.does_item_exist(f"insert_enable_{insert_num}") and dpg.get_value(f"insert_enable_{insert_num}")


def _disabled_label(insert_num):
    if insert_num is None or _insert_enabled(insert_num):
        return None
    return f"Insert {insert_num}{_DISABLED_MARKER}"


def open_dialog(sender=None, app_data=None):
    global _editing_index
    _editing_index = None
    _reset_delete_confirm()
    dpg.configure_item(WINDOW_TAG, label="Create Stereo Pair")
    dpg.configure_item("stereo_pair_create_btn", label="Create")
    dpg.configure_item("stereo_pair_delete_btn", show=False)
    dpg.set_value("stereo_pair_name_input", "Unnamed Stereo Pair")
    dpg.set_value("stereo_pair_left_combo", "[Select]")
    dpg.set_value("stereo_pair_right_combo", "[Select]")
    refresh_combos()
    _show_centered(WINDOW_TAG, 300, 210)


def open_edit_dialog(sender=None, app_data=None, user_data=None):
    global _editing_index
    _editing_index = user_data
    _reset_delete_confirm()
    pair = app_state.stereo_pairs[_editing_index]
    insert_names = {i: name for i, name in app_state.get_enabled_insert_options()}
    dpg.configure_item(WINDOW_TAG, label="Edit Stereo Pair")
    dpg.configure_item("stereo_pair_create_btn", label="Save")
    dpg.configure_item("stereo_pair_delete_btn", show=True)
    dpg.set_value("stereo_pair_name_input", pair.get("name", ""))
    dpg.set_value("stereo_pair_left_combo", _disabled_label(pair.get("left")) or insert_names.get(pair.get("left"), "[Select]"))
    dpg.set_value("stereo_pair_right_combo", _disabled_label(pair.get("right")) or insert_names.get(pair.get("right"), "[Select]"))
    refresh_combos()
    _show_centered(WINDOW_TAG, 300, 210)


def _show_centered(tag, width, height):
    viewport_width = dpg.get_viewport_client_width()
    viewport_height = dpg.get_viewport_client_height()
    dpg.configure_item(
        tag,
        show=True,
        pos=[max(0, (viewport_width - width) // 2), max(0, (viewport_height - height) // 2)],
    )


def refresh_combos():
    if not dpg.does_item_exist(WINDOW_TAG):
        return
    options = app_state.get_enabled_insert_options()
    left_val = dpg.get_value("stereo_pair_left_combo")
    right_val = dpg.get_value("stereo_pair_right_combo")
    # each side's list excludes whatever the other side currently has selected
    left_items = ["[Select]"] + [name for _, name in options if name != right_val]
    right_items = ["[Select]"] + [name for _, name in options if name != left_val]
    # a disabled-but-still-referenced insert keeps its placeholder item instead of being silently cleared
    if left_val.endswith(_DISABLED_MARKER) and left_val not in left_items:
        left_items.append(left_val)
    if right_val.endswith(_DISABLED_MARKER) and right_val not in right_items:
        right_items.append(right_val)
    dpg.configure_item("stereo_pair_left_combo", items=left_items)
    dpg.configure_item("stereo_pair_right_combo", items=right_items)
    if left_val not in left_items:
        dpg.set_value("stereo_pair_left_combo", "[Select]")
    if right_val not in right_items:
        dpg.set_value("stereo_pair_right_combo", "[Select]")
    dpg.bind_item_theme("stereo_pair_left_combo",
                        "stereo_pair_disabled_combo_theme" if dpg.get_value("stereo_pair_left_combo").endswith(_DISABLED_MARKER) else 0)
    dpg.bind_item_theme("stereo_pair_right_combo",
                        "stereo_pair_disabled_combo_theme" if dpg.get_value("stereo_pair_right_combo").endswith(_DISABLED_MARKER) else 0)
    update_create_button_state()


def field_changed_callback(sender=None, app_data=None):
    refresh_combos()


def update_create_button_state():
    if not dpg.does_item_exist("stereo_pair_create_btn"):
        return
    name = (dpg.get_value("stereo_pair_name_input") or "").strip()
    left = dpg.get_value("stereo_pair_left_combo")
    right = dpg.get_value("stereo_pair_right_combo")
    valid = bool(name) and left != "[Select]" and right != "[Select]" and left != right
    dpg.configure_item("stereo_pair_create_btn", enabled=valid)
    dpg.bind_item_theme("stereo_pair_create_btn", 0 if valid else "dialog_create_btn_disabled_theme")


def save_callback(sender=None, app_data=None):
    name = (dpg.get_value("stereo_pair_name_input") or "").strip()
    left_name = dpg.get_value("stereo_pair_left_combo")
    right_name = dpg.get_value("stereo_pair_right_combo")
    if not name or left_name == "[Select]" or right_name == "[Select]" or left_name == right_name:
        return
    name_to_insert = {insert_name: i for i, insert_name in app_state.get_enabled_insert_options()}
    # a still-disabled placeholder wasn't changed by the user, so keep its original insert reference
    prior = app_state.stereo_pairs[_editing_index] if _editing_index is not None else {}
    left = name_to_insert.get(left_name)
    if left is None and left_name.endswith(_DISABLED_MARKER):
        left = prior.get("left")
    right = name_to_insert.get(right_name)
    if right is None and right_name.endswith(_DISABLED_MARKER):
        right = prior.get("right")
    pair = {
        "name": name,
        "left": left,
        "right": right,
    }
    if _editing_index is None:
        app_state.stereo_pairs.append(pair)
        status = f"Status: Created stereo pair '{name}'"
    else:
        app_state.stereo_pairs[_editing_index] = pair
        status = f"Status: Saved stereo pair '{name}'"
    app_state.save_user_settings()
    app_state.notify_stereo_pairs_changed()
    dpg.hide_item(WINDOW_TAG)
    app_state.update_status(status)


def delete_button_callback(sender=None, app_data=None):
    global _delete_confirm_pending
    if _editing_index is None:
        return
    if not _delete_confirm_pending:
        _delete_confirm_pending = True
        dpg.configure_item("stereo_pair_delete_btn", label="Are you sure?")
        return
    name = app_state.stereo_pairs[_editing_index].get("name", "stereo pair")
    del app_state.stereo_pairs[_editing_index]
    app_state.save_user_settings()
    app_state.notify_stereo_pairs_changed()
    _reset_delete_confirm()
    dpg.hide_item(WINDOW_TAG)
    app_state.update_status(f"Status: Deleted stereo pair '{name}'")


def cancel_callback(sender=None, app_data=None):
    _reset_delete_confirm()
    dpg.hide_item(WINDOW_TAG)


def _ensure_delete_button_theme():
    if dpg.does_item_exist("stereo_pair_delete_btn_theme"):
        return
    with dpg.theme(tag="stereo_pair_delete_btn_theme"):
        with dpg.theme_component(dpg.mvButton):
            dpg.add_theme_color(dpg.mvThemeCol_Text, [220, 50, 50, 255])


def _ensure_disabled_combo_theme():
    # no italic font is loaded, so a disabled-but-referenced insert is marked with red text only
    if dpg.does_item_exist("stereo_pair_disabled_combo_theme"):
        return
    with dpg.theme(tag="stereo_pair_disabled_combo_theme"):
        with dpg.theme_component(dpg.mvCombo):
            dpg.add_theme_color(dpg.mvThemeCol_Text, [220, 60, 60, 255])


def _ensure_disabled_create_btn_theme():
    # Shared with chains_dialog.py's Create/Save button - DPG's built-in disabled dimming
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
    _ensure_disabled_combo_theme()
    _ensure_disabled_create_btn_theme()
    with dpg.window(tag=WINDOW_TAG, label="Create Stereo Pair", modal=True, show=False, width=300, height=210, no_resize=True):
        dpg.add_input_text(tag="stereo_pair_name_input", default_value="Unnamed Stereo Pair", width=260, callback=field_changed_callback)
        dpg.add_text("Left Channel")
        dpg.add_combo(tag="stereo_pair_left_combo", items=["[Select]"], default_value="[Select]", width=260, callback=field_changed_callback)
        dpg.add_text("Right Channel")
        dpg.add_combo(tag="stereo_pair_right_combo", items=["[Select]"], default_value="[Select]", width=260, callback=field_changed_callback)
        with dpg.group(horizontal=True):
            dpg.add_button(label="Delete", tag="stereo_pair_delete_btn", show=False, callback=delete_button_callback)
            dpg.bind_item_theme("stereo_pair_delete_btn", "stereo_pair_delete_btn_theme")
            dpg.add_spacer(width=60)
            dpg.add_button(label="Create", tag="stereo_pair_create_btn", enabled=False, callback=save_callback)
            dpg.bind_item_theme("stereo_pair_create_btn", "dialog_create_btn_disabled_theme")
            dpg.add_button(label="Cancel", callback=cancel_callback)
