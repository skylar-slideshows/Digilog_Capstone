"""Primary window: main mixer surface (knobs, toggles, channel strip), File/Window
menus, native macOS menu wiring, and application entry points. The primary window's
body is split into top-bar tab pages (Control / Channels / Inserts / Console Settings).
Every other floating window lives in its own module (connect_window, console_monitor_window,
inserts_window, stereo_pair_dialog) and shared state lives in app_state."""
import sys
import dearpygui.dearpygui as dpg

import app_state
import connect_window
import console_monitor_window
import console_settings_window
import channels_window
import flash_window
import inserts_window
from config import (
    CYCLE_CONFIG, EXTRA_KNOB_TAGS, FADER_CONFIG, GROUPS,
    ICON_PATH, KNOB_CONFIG, KNOB_TAGS, LOGO_FONT_PATH, TOGGLE_CONFIG, VERTICAL_BARS,
    app_version, button_label_size, configured_font_path, group_label_size,
    insert_dropdown_pos, knob_label_size, logo_pos, logo_size,
    logo_text, state_file_extension,
)

KNOB_PIXELS_PER_RANGE = 200.0
_knob_drag_state = {"active": False, "tag": None, "start_value": 0.0}


def set_macos_dock_icon(icon_path):
    if sys.platform != "darwin":
        return
    try:
        from AppKit import NSApplication, NSImage
        app = NSApplication.sharedApplication()
        image = NSImage.alloc().initByReferencingFile_(str(icon_path))
        app.setApplicationIconImage_(image)
        app.activateIgnoringOtherApps_(True)
    except Exception as e:
        print(f"Could not set dock icon: {e}")


def cycle_callback(sender, app_data, user_data):
    cfg = next((c for c in CYCLE_CONFIG if c["tag"] == sender), None)
    if not cfg:
        return
    num_opts = len(cfg["options"])
    app_state.cycle_values[sender] = (app_state.cycle_values.get(sender, 0) + 1) % num_opts
    if app_state.current_channel in app_state.channel_states:
        ch_state = app_state.channel_states[app_state.current_channel]
        ch_state.setdefault("cycles", {})[sender] = app_state.cycle_values[sender]
    if app_state.on_button_change_handler:
        state_bits = format(app_state.cycle_values[sender], "02b")
        app_state.on_button_change_handler(
            app_state.CHANNEL_OPTIONS.index(app_state.current_channel), cfg["sw_id"], state_bits, sender
        )
        channels_window.mirror_cycle_to_others(app_state.current_channel, sender, cfg["sw_id"], app_state.cycle_values[sender])
    app_state.update_cycle_display(sender)


def channel_name_callback(sender, app_data, user_data):
    # %, $, ^ are the wire protocol's framing/delimiter characters and must never appear in a channel name.
    if any(ch in app_data for ch in ("%", "$", "^")):
        app_data = "".join(ch for ch in app_data if ch not in ("%", "$", "^"))
        dpg.set_value(sender, app_data)
    if app_state.current_channel in app_state.channel_states:
        app_state.channel_states[app_state.current_channel]["name"] = app_data
        app_state.update_channel_dropdown()
        inserts_window.refresh_insert_usage_labels()
        channels_window.sync_name_from_control(app_state.current_channel, app_data)


def channel_name_deactivated_callback(sender, app_data, user_data):
    # Only send the name once the user is done editing (clicks away / tabs out / presses Enter).
    name = dpg.get_value("channel_name_input")
    idx = app_state.CHANNEL_OPTIONS.index(app_state.current_channel)
    if not name.strip():
        name = f"Channel {idx + 1}"
        dpg.set_value("channel_name_input", name)
        if app_state.current_channel in app_state.channel_states:
            app_state.channel_states[app_state.current_channel]["name"] = name
            app_state.update_channel_dropdown()
            inserts_window.refresh_insert_usage_labels()
            channels_window.sync_name_from_control(app_state.current_channel, name)
    if app_state.on_channel_name_change_handler:
        app_state.on_channel_name_change_handler(idx, name)


def channel_dropdown_callback(sender, app_data, user_data):
    display_names = [app_state.channel_states[ch]["name"] for ch in app_state.CHANNEL_OPTIONS]
    if app_data in display_names:
        idx = display_names.index(app_data)
        target_channel_key = app_state.CHANNEL_OPTIONS[idx]
    elif app_data in app_state.CHANNEL_OPTIONS:
        target_channel_key = app_data
    else:
        target_channel_key = app_state.CHANNEL_OPTIONS[0]

    app_state.save_current_channel_state()
    app_state.apply_channel_state(target_channel_key)


def knob_callback(sender, app_data, user_data):
    app_state.knob_values[sender] = app_data
    prev_value = None
    if app_state.current_channel in app_state.channel_states:
        prev_value = app_state.channel_states[app_state.current_channel]["knobs"].get(sender)
        app_state.channel_states[app_state.current_channel]["knobs"][sender] = app_data
    channels_window.sync_knob_from_control(sender, app_data)
    cfg = KNOB_CONFIG.get(sender)
    if cfg is None and FADER_CONFIG and sender == FADER_CONFIG["tag"]:
        cfg = FADER_CONFIG
    if cfg is not None and "ctrl_id" in cfg:
        app_state.notify_knob_change(sender, app_data, cfg["ctrl_id"], cfg["min"], cfg["max"])
        if prev_value is not None:
            channels_window.mirror_knob_delta_to_others(
                app_state.current_channel, sender, app_data - prev_value, cfg["ctrl_id"], cfg["min"], cfg["max"]
            )


def toggle_callback(sender, app_data):
    if sender == "sel":
        # SEL's lit state isn't an independent flip-flop - it mirrors whether the current channel
        # is selected in the Channels tab, so clicking it just (re)selects that one channel there.
        channels_window.select_current_channel_only()
        return
    app_state.toggle_values[sender] = not app_state.toggle_values[sender]
    active_theme = f"{sender}_active_theme"
    dpg.bind_item_theme(sender, active_theme if app_state.toggle_values[sender] else "toggle_theme")
    if sender in ("light_theme", "light", "light_theme_toggle"):
        app_state.update_theme(app_state.toggle_values[sender])
    elif app_state.current_channel in app_state.channel_states and "toggles" in app_state.channel_states[app_state.current_channel]:
        app_state.channel_states[app_state.current_channel]["toggles"][sender] = app_state.toggle_values[sender]
    toggle_cfg = next((toggle for toggle in TOGGLE_CONFIG if toggle["tag"] == sender), None)
    if app_state.on_button_change_handler and toggle_cfg and sender != "light_theme":
        channel = app_state.CHANNEL_OPTIONS.index(app_state.current_channel)
        if toggle_cfg["sw_id"] is not None:
            app_state.on_button_change_handler(channel, toggle_cfg["sw_id"], int(app_state.toggle_values[sender]), sender)
            channels_window.mirror_toggle_to_others(app_state.current_channel, sender, toggle_cfg["sw_id"], app_state.toggle_values[sender])


def knob_vertical_drag_callback(sender, app_data):
    # add_knob_float only supports horizontal (x) drag natively, so override with y-based control here.
    # app_data's delta is cumulative since the drag started, so track the starting value rather than accumulating.
    active_tag = next((tag for tag in KNOB_TAGS if dpg.is_item_active(tag)), None)
    cfg = KNOB_CONFIG.get(active_tag) if active_tag is not None else None
    if active_tag is None:
        # the Channels tab has its own per-channel copy of the "pan" knob, sharing its range
        active_tag = next((tag for tag in channels_window.pan_knob_tags() if dpg.is_item_active(tag)), None)
        cfg = KNOB_CONFIG.get("pan") if active_tag is not None else None
    if active_tag is None or cfg is None:
        _knob_drag_state["active"] = False
        return
    if not _knob_drag_state["active"] or _knob_drag_state["tag"] != active_tag:
        _knob_drag_state["active"] = True
        _knob_drag_state["tag"] = active_tag
        _knob_drag_state["start_value"] = dpg.get_value(active_tag)
    _, _, y_delta = app_data
    value = _knob_drag_state["start_value"] - (y_delta / KNOB_PIXELS_PER_RANGE) * (cfg["max"] - cfg["min"])
    value = max(cfg["min"], min(cfg["max"], value))
    dpg.set_value(active_tag, value)
    # send from here directly rather than relying on the knob's own native (horizontal-only)
    # changed callback - a fast, mostly-vertical flick can end before enough x movement ever
    # accumulates to fire it natively, silently dropping the whole turn.
    if active_tag in KNOB_TAGS:
        knob_callback(active_tag, value, None)
    else:
        channels_window.apply_vertical_drag_to_pan(active_tag, value)


# --- File menu actions (Save/Load .dgl channel presets) ---

def save_state_callback(sender, app_data):
    try:
        file_path = app_state.save_state_file(app_data["file_path_name"])
        app_state.update_status(f"Status: Saved {file_path.name}")
    except (OSError, TypeError, ValueError) as error:
        app_state.update_status(f"Status: Error saving - {error}")


def load_state_callback(sender, app_data):
    try:
        file_path = app_state.load_state_file(app_data["file_path_name"])
        app_state.update_status(f"Status: Opened {file_path.name}")
    except (OSError, TypeError, ValueError) as error:
        app_state.update_status(f"Status: Error opening - {error}")


def open_native_file_dialog(mode="open", default_extension=".dgl", initial_file="", initial_dir=None):
    if sys.platform == "win32":
        try:
            import tkinter as tk
            from tkinter import filedialog
            root = tk.Tk()
            root.withdraw()
            root.attributes("-topmost", True)
            ext_name = default_extension.lstrip(".")
            filetypes = [(f"{ext_name.upper()} Files (*{default_extension})", f"*{default_extension}"), ("All Files (*.*)", "*.*")]
            kwargs = {"filetypes": filetypes, "defaultextension": default_extension}
            if initial_dir:
                kwargs["initialdir"] = str(initial_dir)
            if mode == "open":
                res = filedialog.askopenfilename(title="Open Preset File", **kwargs)
            else:
                res = filedialog.asksaveasfilename(title="Save Preset File", initialfile=initial_file, **kwargs)
            root.destroy()
            return res if res else None
        except Exception as e:
            print(f"Native Windows dialog error: {e}")
            return None
    return None


def show_save_dialog_callback(sender=None, app_data=None):
    if sys.platform == "win32":
        init_file = app_state.current_state_file_path.name if app_state.current_state_file_path else f"Untitled{state_file_extension}"
        selected = open_native_file_dialog(mode="save", default_extension=state_file_extension, initial_file=init_file)
        if selected:
            save_state_callback(sender, {"file_path_name": selected})
        return
    app_state.update_status("Status: Choose a file to save")
    dpg.show_item("save_dialog")


def show_open_dialog_callback(sender=None, app_data=None):
    if sys.platform == "win32":
        selected = open_native_file_dialog(mode="open", default_extension=state_file_extension)
        if selected:
            load_state_callback(sender, {"file_path_name": selected})
        return
    app_state.update_status("Status: Choose a file to open")
    dpg.show_item("open_dialog")


def save_action():
    if app_state.current_state_file_path is not None:
        try:
            file_path = app_state.save_state_file(app_state.current_state_file_path)
            app_state.update_status(f"Status: Saved {file_path.name}")
        except (OSError, TypeError, ValueError) as error:
            app_state.update_status(f"Status: Error saving - {error}")
    else:
        show_save_dialog_callback()


def load_action():
    show_open_dialog_callback()


def save_a_copy_action():
    show_save_dialog_callback()


def save_as_default_action(sender=None, app_data=None):
    try:
        file_path = app_state.save_as_default_file()
        app_state.update_status(f"Status: Saved {file_path.name}")
    except (OSError, TypeError, ValueError) as error:
        app_state.update_status(f"Status: Error saving - {error}")


def save_as_template_action(sender=None, app_data=None):
    app_state.TEMPLATES_DIR.mkdir(parents=True, exist_ok=True)
    if sys.platform == "win32":
        selected = open_native_file_dialog(
            mode="save", default_extension=state_file_extension,
            initial_file=app_state.DEFAULT_TEMPLATE_FILE_NAME, initial_dir=app_state.TEMPLATES_DIR,
        )
        if selected:
            save_state_callback(sender, {"file_path_name": selected})
        return
    app_state.update_status("Status: Choose a template name")
    dpg.configure_item("save_template_dialog", default_path=str(app_state.TEMPLATES_DIR))
    dpg.show_item("save_template_dialog")


def load_template_file(file_path):
    try:
        loaded_path = app_state.load_state_file(file_path)
        app_state.update_status(f"Status: Opened {loaded_path.name}")
    except (OSError, TypeError, ValueError) as error:
        app_state.update_status(f"Status: Error opening - {error}")


_last_template_paths = None


def refresh_template_menu(sender=None, app_data=None, user_data=None):
    global _last_template_paths
    if not dpg.does_item_exist("load_template_menu"):
        return
    templates = app_state.list_template_files()
    if templates == _last_template_paths:
        return
    _last_template_paths = templates
    for child in dpg.get_item_children("load_template_menu", slot=1) or []:
        dpg.delete_item(child)
    if not templates:
        dpg.add_menu_item(label="(No templates found)", parent="load_template_menu", enabled=False)
        return
    for path in templates:
        dpg.add_menu_item(
            label=path.stem, parent="load_template_menu",
            callback=lambda s, a, u: load_template_file(u), user_data=path,
        )


def export_profile_callback(sender, app_data):
    try:
        file_path = app_state.export_settings_file(app_data["file_path_name"])
        app_state.update_status(f"Status: Exported {file_path.name}")
    except (OSError, TypeError, ValueError) as error:
        app_state.update_status(f"Status: Error exporting - {error}")


def import_profile_callback(sender, app_data):
    try:
        file_path = app_state.import_settings_file(app_data["file_path_name"])
        app_state.update_status(f"Status: Imported {file_path.name}")
    except (OSError, TypeError, ValueError) as error:
        app_state.update_status(f"Status: Error importing - {error}")


def export_console_profile_action(sender=None, app_data=None):
    if sys.platform == "win32":
        selected = open_native_file_dialog(
            mode="save", default_extension=".yaml",
            initial_file=app_state.DEFAULT_PROFILE_FILE_NAME, initial_dir=app_state.PROFILE_DEFAULT_DIR,
        )
        if selected:
            export_profile_callback(sender, {"file_path_name": selected})
        return
    app_state.PROFILE_DEFAULT_DIR.mkdir(parents=True, exist_ok=True)
    app_state.update_status("Status: Choose a location to export the profile")
    dpg.configure_item("export_profile_dialog", default_path=str(app_state.PROFILE_DEFAULT_DIR))
    dpg.show_item("export_profile_dialog")


def import_console_profile_action(sender=None, app_data=None):
    if sys.platform == "win32":
        selected = open_native_file_dialog(
            mode="open", default_extension=".yaml", initial_dir=app_state.PROFILE_DEFAULT_DIR,
        )
        if selected:
            import_profile_callback(sender, {"file_path_name": selected})
        return
    app_state.PROFILE_DEFAULT_DIR.mkdir(parents=True, exist_ok=True)
    app_state.update_status("Status: Choose a profile to import")
    dpg.configure_item("import_profile_dialog", default_path=str(app_state.PROFILE_DEFAULT_DIR))
    dpg.show_item("import_profile_dialog")


def toggle_light_theme_action(sender=None, app_data=None):
    if isinstance(app_data, bool):
        is_light = app_data
    elif sender == "menu_light_theme" and dpg.does_item_exist("menu_light_theme"):
        is_light = dpg.get_value("menu_light_theme")
    else:
        is_light = not app_state.toggle_values.get("light_theme", False)

    app_state.toggle_values["light_theme"] = is_light
    app_state.update_theme(is_light)
    app_state.save_user_settings()
    sync_all_menu_states()


def _toggle_and_sync(toggle_fn):
    def wrapper(sender=None, app_data=None):
        toggle_fn(sender, app_data)
        sync_all_menu_states()
    return wrapper


connect_toggle = _toggle_and_sync(connect_window.toggle_window_action)
console_monitor_toggle = _toggle_and_sync(console_monitor_window.toggle_window_action)
flash_toggle = _toggle_and_sync(flash_window.toggle_window_action)


_TOP_TABS = ("control", "channels", "inserts", "console_settings")
_TOP_TAB_CONTENT_TAGS = {
    "control": "tab_control_content",
    "channels": "tab_channels_content",
    "inserts": "tab_inserts_content",
    "console_settings": "tab_console_settings_content",
}
_TOP_BAR_RIGHT_MARGIN = 60
_TOP_BAR_BTNS = ("top_tab_control_btn", "top_tab_channels_btn", "top_tab_inserts_btn", "top_tab_console_settings_btn")
_TOP_TAB_BTN_TAGS = {
    "control": "top_tab_control_btn",
    "channels": "top_tab_channels_btn",
    "inserts": "top_tab_inserts_btn",
    "console_settings": "top_tab_console_settings_btn",
}


def select_top_tab(tab_name, sender=None, app_data=None):
    for name in _TOP_TABS:
        is_selected = name == tab_name
        dpg.configure_item(_TOP_TAB_CONTENT_TAGS[name], show=is_selected)
        dpg.bind_item_theme(_TOP_TAB_BTN_TAGS[name], "top_tab_selected_theme" if is_selected else "top_tab_unselected_theme")


def _position_top_bar_tabs(sender=None, app_data=None):
    """Resizes the spacer in front of the top-bar tab buttons so they sit flush against the
    right edge. Unlike absolute pos=, a spacer is normal menu bar layout flow - the same
    mechanism the File/Window menus (and the version text right after them) already use - so it
    stays put when the window is scrolled horizontally, not just when the viewport is resized."""
    if not (dpg.does_item_exist("top_tab_spacer") and dpg.does_item_exist("top_tab_control_btn")):
        return
    viewport_width = dpg.get_viewport_client_width()
    # neither mvMenu (File/Window) nor mvSpacer support rect queries in this DPG build, but the
    # first tab button (right after the spacer) does - back out how much space came before the
    # spacer (File/Window menus + version text) using its previous width and the button's
    # current rendered position
    current_spacer_width = dpg.get_item_configuration("top_tab_spacer").get("width", 0) or 0
    used_before_spacer = dpg.get_item_rect_min("top_tab_control_btn")[0] - current_spacer_width
    content_width = sum(dpg.get_item_rect_size(tag)[0] for tag in _TOP_BAR_BTNS)
    spacer_width = max(0, viewport_width - used_before_spacer - content_width - _TOP_BAR_RIGHT_MARGIN)
    dpg.configure_item("top_tab_spacer", width=spacer_width)


def window_on_close_callback(sender):
    sync_all_menu_states()


def sync_all_menu_states():
    is_light = app_state.toggle_values.get("light_theme", False)
    if dpg.does_item_exist("menu_light_theme"):
        dpg.set_value("menu_light_theme", is_light)
        dpg.configure_item("menu_light_theme", label="Light Mode" if is_light else "Dark Mode")

    if dpg.does_item_exist(connect_window.MENU_TAG) and dpg.does_item_exist(connect_window.WINDOW_TAG):
        dpg.set_value(connect_window.MENU_TAG, dpg.is_item_shown(connect_window.WINDOW_TAG))

    if dpg.does_item_exist(console_monitor_window.MENU_TAG) and dpg.does_item_exist(console_monitor_window.WINDOW_TAG):
        dpg.set_value(console_monitor_window.MENU_TAG, dpg.is_item_shown(console_monitor_window.WINDOW_TAG))

    if dpg.does_item_exist(flash_window.MENU_TAG) and dpg.does_item_exist(flash_window.WINDOW_TAG):
        dpg.set_value(flash_window.MENU_TAG, dpg.is_item_shown(flash_window.WINDOW_TAG))

    update_native_mac_menu_states()


_native_menu_handler = None
_native_items = {}


def setup_native_mac_menu():
    global _native_menu_handler, _native_items
    if sys.platform != "darwin":
        return
    try:
        from AppKit import NSApplication, NSMenu, NSMenuItem
        from Foundation import NSObject

        class NativeMenuHandler(NSObject):
            def saveClicked_(self, sender):
                save_action()

            def loadClicked_(self, sender):
                load_action()

            def saveCopyClicked_(self, sender):
                save_a_copy_action()

            def saveAsDefaultClicked_(self, sender):
                save_as_default_action()

            def saveAsTemplateClicked_(self, sender):
                save_as_template_action()

            def loadTemplateClicked_(self, sender):
                path = sender.representedObject()
                if path:
                    load_template_file(str(path))

            def exportProfileClicked_(self, sender):
                export_console_profile_action()

            def importProfileClicked_(self, sender):
                import_console_profile_action()

            def menuNeedsUpdate_(self, menu):
                if menu != _native_items.get("load_template_submenu"):
                    return
                menu.removeAllItems()
                templates = app_state.list_template_files()
                if not templates:
                    empty_item = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("(No templates found)", None, "")
                    empty_item.setEnabled_(False)
                    menu.addItem_(empty_item)
                    return
                for path in templates:
                    template_item = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_(path.stem, "loadTemplateClicked:", "")
                    template_item.setTarget_(self)
                    template_item.setRepresentedObject_(str(path))
                    menu.addItem_(template_item)

            def toggleThemeClicked_(self, sender):
                toggle_light_theme_action()

            def toggleConnectClicked_(self, sender):
                connect_toggle()

            def toggleConsoleMonitorClicked_(self, sender):
                console_monitor_toggle()

        app = NSApplication.sharedApplication()
        main_menu = app.mainMenu()
        if not main_menu:
            return

        file_menu = None
        for item in main_menu.itemArray():
            if item.title() == "File" or (item.submenu() and item.submenu().title() == "File"):
                file_menu = item.submenu()
                break

        if not file_menu:
            file_menu = NSMenu.alloc().initWithTitle_("File")
            file_menu_item = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("File", None, "")
            file_menu_item.setSubmenu_(file_menu)
            main_menu.insertItem_atIndex_(file_menu_item, 1)
        else:
            file_menu.removeAllItems()

        window_menu = None
        for item in main_menu.itemArray():
            if item.title() == "Window" or (item.submenu() and item.submenu().title() == "Window"):
                window_menu = item.submenu()
                break

        if not window_menu:
            window_menu = NSMenu.alloc().initWithTitle_("Window")
            window_menu_item = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("Window", None, "")
            window_menu_item.setSubmenu_(window_menu)
            idx = min(2, main_menu.numberOfItems())
            main_menu.insertItem_atIndex_(window_menu_item, idx)
        else:
            window_menu.removeAllItems()

        _native_menu_handler = NativeMenuHandler.alloc().init()

        item_save = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("Save", "saveClicked:", "s")
        item_save.setTarget_(_native_menu_handler)

        item_load = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("Load...", "loadClicked:", "o")
        item_load.setTarget_(_native_menu_handler)

        item_save_copy = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("Save a Copy...", "saveCopyClicked:", "S")
        item_save_copy.setTarget_(_native_menu_handler)

        item_save_default = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("Save Mix as Default", "saveAsDefaultClicked:", "")
        item_save_default.setTarget_(_native_menu_handler)

        item_save_template = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("Save Mix as Template...", "saveAsTemplateClicked:", "")
        item_save_template.setTarget_(_native_menu_handler)

        load_template_submenu = NSMenu.alloc().initWithTitle_("Load from Template")
        load_template_submenu.setDelegate_(_native_menu_handler)
        item_load_template = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("Load from Template", None, "")
        item_load_template.setSubmenu_(load_template_submenu)

        item_export_profile = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("Export Console Settings Profile...", "exportProfileClicked:", "")
        item_export_profile.setTarget_(_native_menu_handler)

        item_import_profile = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("Import Console Settings Profile...", "importProfileClicked:", "")
        item_import_profile.setTarget_(_native_menu_handler)

        is_light = app_state.toggle_values.get("light_theme", False)
        theme_label = "Light Mode" if is_light else "Dark Mode"
        item_theme = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_(theme_label, "toggleThemeClicked:", "t")
        item_theme.setTarget_(_native_menu_handler)

        file_menu.addItem_(item_save)
        file_menu.addItem_(item_load)
        file_menu.addItem_(item_save_copy)
        file_menu.addItem_(NSMenuItem.separatorItem())
        file_menu.addItem_(item_save_default)
        file_menu.addItem_(item_save_template)
        file_menu.addItem_(item_load_template)
        file_menu.addItem_(NSMenuItem.separatorItem())
        file_menu.addItem_(item_export_profile)
        file_menu.addItem_(item_import_profile)

        _native_items["load_template_submenu"] = load_template_submenu

        item_console_mon = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("Console Monitor", "toggleConsoleMonitorClicked:", "")
        item_console_mon.setTarget_(_native_menu_handler)

        item_connect = NSMenuItem.alloc().initWithTitle_action_keyEquivalent_("Connect", "toggleConnectClicked:", "")
        item_connect.setTarget_(_native_menu_handler)

        window_menu.addItem_(item_connect)
        window_menu.addItem_(item_console_mon)
        window_menu.addItem_(item_theme)

        _native_items["theme"] = item_theme
        _native_items["console_monitor"] = item_console_mon
        _native_items["connect"] = item_connect

        update_native_mac_menu_states()
    except Exception as e:
        print(f"Could not setup native mac menu: {e}")


def update_native_mac_menu_states():
    if sys.platform != "darwin" or not _native_items:
        return
    try:
        is_light = app_state.toggle_values.get("light_theme", False)
        if "theme" in _native_items:
            _native_items["theme"].setTitle_("Light Mode" if is_light else "Dark Mode")
            _native_items["theme"].setState_(1 if is_light else 0)

        if "console_monitor" in _native_items and dpg.does_item_exist(console_monitor_window.WINDOW_TAG):
            is_shown = dpg.is_item_shown(console_monitor_window.WINDOW_TAG)
            _native_items["console_monitor"].setState_(1 if is_shown else 0)

        if "connect" in _native_items and dpg.does_item_exist(connect_window.WINDOW_TAG):
            is_shown = dpg.is_item_shown(connect_window.WINDOW_TAG)
            _native_items["connect"].setState_(1 if is_shown else 0)
    except Exception as e:
        print(f"Could not update native mac menu states: {e}")


def build_gui():
    dpg.create_context()
    # Without this, DPG dispatches item/handler callbacks (button clicks, mouse drag
    # handlers, etc.) on its own internal background thread, racing with the main
    # thread's rendering - this corrupted internal state and crashed the app under load
    # (e.g. dragging a knob with several channels selected while connected to the STM32).
    # manual_callback_management + the render loop in start_gui() keep everything on the
    # main thread instead.
    dpg.configure_app(manual_callback_management=True)
    app_state.create_initial_themes()
    dpg.create_viewport(title="Digilog Remote", width=820, height=720, small_icon=str(ICON_PATH), large_icon=str(ICON_PATH))

    with dpg.handler_registry():
        dpg.add_mouse_drag_handler(callback=knob_vertical_drag_callback)

    with dpg.item_handler_registry(tag="channel_name_focus_handler"):
        dpg.add_item_deactivated_after_edit_handler(callback=channel_name_deactivated_callback)

    UI_FONT_PATH = configured_font_path
    group_font = None
    knob_font = None
    button_font = None
    logo_font = None
    with dpg.font_registry():
        if UI_FONT_PATH.exists():
            group_font = dpg.add_font(str(UI_FONT_PATH), group_label_size)
            knob_font = dpg.add_font(str(UI_FONT_PATH), knob_label_size)
            button_font = dpg.add_font(str(UI_FONT_PATH), button_label_size)
        if LOGO_FONT_PATH.exists():
            logo_font = dpg.add_font(str(LOGO_FONT_PATH), logo_size)

    with dpg.window(tag="primary", label="Digilog Remote", width=820, height=720, menubar=True):
        with dpg.menu_bar():
            with dpg.menu(label="File"):
                dpg.add_menu_item(label="Save", shortcut="Cmd+S", callback=lambda: save_action())
                dpg.add_menu_item(label="Load...", shortcut="Cmd+O", callback=lambda: load_action())
                dpg.add_menu_item(label="Save a Copy...", shortcut="Cmd+Shift+S", callback=lambda: save_a_copy_action())
                dpg.add_separator()
                dpg.add_menu_item(label="Save Mix as Default", callback=lambda: save_as_default_action())
                dpg.add_menu_item(label="Save Mix as Template...", callback=lambda: save_as_template_action())
                with dpg.menu(label="Load from Template", tag="load_template_menu"):
                    pass
                dpg.add_separator()
                dpg.add_menu_item(label="Export Console Settings Profile...", callback=lambda: export_console_profile_action())
                dpg.add_menu_item(label="Import Console Settings Profile...", callback=lambda: import_console_profile_action())
            with dpg.menu(label="Window", tag="window_menu"):
                dpg.add_menu_item(
                    label="Connect",
                    tag=connect_window.MENU_TAG,
                    check=True,
                    default_value=True,
                    callback=connect_toggle
                )
                dpg.add_menu_item(
                    label="Console Monitor",
                    tag=console_monitor_window.MENU_TAG,
                    check=True,
                    default_value=True,
                    callback=console_monitor_toggle
                )
                is_light_init = app_state.toggle_values.get("light_theme", False)
                dpg.add_menu_item(
                    label="Light Mode" if is_light_init else "Dark Mode",
                    tag="menu_light_theme",
                    check=True,
                    default_value=is_light_init,
                    shortcut="Cmd+T",
                    callback=toggle_light_theme_action
                )

            with dpg.menu(label="Firmware", tag="firmware_menu"):
                dpg.add_menu_item(
                    label="STM32 Firmware Programmer",
                    tag=flash_window.MENU_TAG,
                    check=True,
                    default_value=False,
                    callback=flash_toggle
                )

            dpg.add_text(f"v{app_version}", tag="app_version_text")

            # right-aligned top-bar tabs that switch the primary window's body between the mixer
            # controls, the Channels page, the Inserts config page, and the Console Settings page.
            # A resizable spacer (normal layout flow, like the File/Window menus) pushes them flush
            # to the right edge - unlike absolute pos=, this stays put when the window is scrolled
            # horizontally, and _position_top_bar_tabs() keeps its width current on resize.
            dpg.add_spacer(tag="top_tab_spacer", width=0)
            dpg.add_button(label="Control", tag="top_tab_control_btn", callback=lambda: select_top_tab("control"))
            dpg.add_button(label="Channels", tag="top_tab_channels_btn", callback=lambda: select_top_tab("channels"))
            dpg.add_button(label="Inserts", tag="top_tab_inserts_btn", callback=lambda: select_top_tab("inserts"))
            dpg.add_button(label="Console Settings", tag="top_tab_console_settings_btn",
                           callback=lambda: select_top_tab("console_settings"))
            dpg.bind_item_theme("top_tab_control_btn", "top_tab_selected_theme")
            dpg.bind_item_theme("top_tab_channels_btn", "top_tab_unselected_theme")
            dpg.bind_item_theme("top_tab_inserts_btn", "top_tab_unselected_theme")
            dpg.bind_item_theme("top_tab_console_settings_btn", "top_tab_unselected_theme")

        with dpg.group(tag="tab_control_content"):
            logo_item = dpg.add_text(logo_text, pos=logo_pos)
            if logo_font is not None:
                dpg.bind_item_font(logo_item, logo_font)

            init_display_names = [app_state.channel_states[ch]["name"] for ch in app_state.CHANNEL_OPTIONS]
            init_curr_name = app_state.channel_states.get(app_state.current_channel, {}).get("name", app_state.current_channel)
            dpg.add_combo(
                items=init_display_names,
                default_value=init_curr_name,
                tag="channel_dropdown",
                pos=[612, 70],
                width=95,
                callback=channel_dropdown_callback
            )
            dpg.add_input_text(
                default_value=app_state.channel_states[app_state.current_channel]["name"],
                tag="channel_name_input",
                pos=[612, 110],
                width=95,
                callback=channel_name_callback
            )
            dpg.bind_item_handler_registry("channel_name_input", "channel_name_focus_handler")
            channels_window.add_chain_link_icon([612, 142], "control_link_icon", show=False)
            dpg.add_text("", tag="control_link_text", pos=[634, 140], show=False)
            inserts_window.add_insert_dropdown_button(insert_dropdown_pos, font=button_font)
            if button_font is not None:
                dpg.bind_item_font("channel_dropdown", button_font)
                dpg.bind_item_font("channel_name_input", button_font)

            for toggle in TOGGLE_CONFIG:
                if toggle["tag"] == "light_theme" or toggle["pos"] is None:
                    continue
                dpg.add_button(label=toggle["label"], tag=toggle["tag"], pos=toggle["pos"],
                               width=toggle["size"], height=toggle["size"], callback=toggle_callback)
                if button_font is not None:
                    dpg.bind_item_font(toggle["tag"], button_font)

            for cycle_btn in CYCLE_CONFIG:
                if cycle_btn["pos"] is None:
                    continue
                dpg.add_button(label=cycle_btn["label"], tag=cycle_btn["tag"], pos=cycle_btn["pos"],
                               width=cycle_btn["size"], height=cycle_btn["size"], callback=cycle_callback)
                if button_font is not None:
                    dpg.bind_item_font(cycle_btn["tag"], button_font)
                dpg.bind_item_theme(cycle_btn["tag"], "toggle_theme")

            for group in GROUPS:
                group_label = dpg.add_text(group["name"], pos=group["label_pos"])
                if group_font is not None:
                    dpg.bind_item_font(group_label, group_font)
                for tag in group["tags"]:
                    cfg = KNOB_CONFIG[tag]
                    dpg.add_knob_float(label=cfg["label"], tag=tag, default_value=cfg["default"], min_value=cfg["min"], max_value=cfg["max"], pos=cfg["pos"], callback=knob_callback)
            for tag in EXTRA_KNOB_TAGS:
                cfg = KNOB_CONFIG[tag]
                dpg.add_knob_float(label=cfg["label"], tag=tag, default_value=cfg["default"], min_value=cfg["min"], max_value=cfg["max"], pos=cfg["pos"], callback=knob_callback)
            if FADER_CONFIG:
                dpg.add_slider_float(label=FADER_CONFIG["label"], tag=FADER_CONFIG["tag"], default_value=FADER_CONFIG["default"],
                                      min_value=FADER_CONFIG["min"], max_value=FADER_CONFIG["max"], pos=FADER_CONFIG["pos"],
                                      vertical=True, width=FADER_CONFIG["width"], height=FADER_CONFIG["height"], callback=knob_callback)
            with dpg.drawlist(width=800, height=650, pos=(0, 0)):
                for bar in VERTICAL_BARS:
                    x, y = bar["pos"]
                    dpg.draw_line((x, y), (x, y + bar["length"]), color=bar["color"], thickness=bar["thickness"])

                dpg.draw_circle((84, 57), 4, fill=(255, 255, 255, 255), color=(40, 40, 40, 255), thickness=1, tag="input_src_led_0")
                dpg.draw_text((74, 65), "LINE", size=11, color=(255, 255, 255, 255), tag="input_src_text_0")

                dpg.draw_circle((106, 57), 4, fill=(55, 55, 55, 255), color=(80, 80, 80, 255), thickness=1, tag="input_src_led_1")
                dpg.draw_text((98, 65), "MIC", size=11, color=(140, 140, 140, 255), tag="input_src_text_1")

                dpg.draw_circle((128, 57), 4, fill=(55, 55, 55, 255), color=(80, 80, 80, 255), thickness=1, tag="input_src_led_2")
                dpg.draw_text((117, 65), "HI-Z", size=11, color=(140, 140, 140, 255), tag="input_src_text_2")

        with dpg.group(tag="tab_channels_content", show=False):
            channels_window.build_tab_content()

        with dpg.group(tag="tab_inserts_content", show=False):
            inserts_window.build_tab_content()

        with dpg.group(tag="tab_console_settings_content", show=False):
            console_settings_window.build_tab_content()

    if knob_font is not None:
        dpg.bind_item_font("primary", knob_font)

    inserts_window.build_insert_select_popup()
    connect_window.build(on_close=window_on_close_callback)
    console_monitor_window.build(on_close=window_on_close_callback)
    flash_window.build(on_close=window_on_close_callback)

    with dpg.file_dialog(tag="save_dialog", directory_selector=False, show=False, callback=save_state_callback, modal=True, width=700, height=400, default_filename=f"Untitled{state_file_extension}"):
        dpg.add_file_extension(state_file_extension, tag="save_dialog_ext")

    with dpg.file_dialog(tag="open_dialog", directory_selector=False, show=False, callback=load_state_callback, modal=True, width=700, height=400):
        dpg.add_file_extension(state_file_extension, tag="open_dialog_ext")

    with dpg.file_dialog(tag="save_template_dialog", directory_selector=False, show=False, callback=save_state_callback, modal=True, width=700, height=400, default_filename=app_state.DEFAULT_TEMPLATE_FILE_NAME):
        dpg.add_file_extension(state_file_extension, tag="save_template_dialog_ext")

    with dpg.file_dialog(tag="export_profile_dialog", directory_selector=False, show=False, callback=export_profile_callback, modal=True, width=700, height=400, default_filename=app_state.DEFAULT_PROFILE_FILE_NAME):
        dpg.add_file_extension(".yaml", tag="export_profile_dialog_ext")

    with dpg.file_dialog(tag="import_profile_dialog", directory_selector=False, show=False, callback=import_profile_callback, modal=True, width=700, height=400):
        dpg.add_file_extension(".yaml", tag="import_profile_dialog_ext")

    with dpg.item_handler_registry(tag="load_template_menu_hover_handler"):
        dpg.add_item_hover_handler(callback=refresh_template_menu)
    dpg.bind_item_handler_registry("load_template_menu", "load_template_menu_hover_handler")
    refresh_template_menu()

    with dpg.window(tag="no_connection_popup", label="Connection Error", modal=True, show=False,
                     no_resize=True, no_collapse=True, width=320, height=130):
        dpg.add_text("No connection to device.\nCheck the cable/port and try loading again.")
        dpg.add_spacer(height=10)
        dpg.add_button(label="OK", width=-1, callback=lambda: dpg.hide_item("no_connection_popup"))
    dpg.bind_item_theme("no_connection_popup", "global_theme")

    dpg.bind_item_theme("save_dialog", "global_theme")
    dpg.bind_item_theme("open_dialog", "global_theme")
    dpg.bind_item_theme("save_template_dialog", "global_theme")
    dpg.bind_item_theme("export_profile_dialog", "global_theme")
    dpg.bind_item_theme("import_profile_dialog", "global_theme")



    app_state.update_theme(app_state.toggle_values.get("light_theme", False))


def start_gui():
    dpg.setup_dearpygui()
    dpg.show_viewport()
    dpg.set_primary_window("primary", True)
    set_macos_dock_icon(ICON_PATH)
    setup_native_mac_menu()
    dpg.set_frame_callback(1, _position_top_bar_tabs)
    dpg.set_viewport_resize_callback(_position_top_bar_tabs)
    # Manual render loop (paired with manual_callback_management above) so every
    # callback runs on the main thread, in lockstep with rendering.
    while dpg.is_dearpygui_running():
        dpg.run_callbacks(dpg.get_callback_queue())
        dpg.render_dearpygui_frame()


def cleanup_gui():
    dpg.destroy_context()

if __name__ == "__main__":
    import run
    run.main()
