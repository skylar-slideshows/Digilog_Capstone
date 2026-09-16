"""Console Monitor window: raw serial console log."""
import dearpygui.dearpygui as dpg

WINDOW_TAG = "console_monitor"
MENU_TAG = "menu_win_console_monitor"

_console_entry_counter = 0
_console_last_entry_tag = None
_console_last_entry_color = None


def append_console_log(text: str, color=(255, 255, 255, 255)):
    global _console_entry_counter, _console_last_entry_tag, _console_last_entry_color
    if not dpg.does_item_exist("console_child") or not text:
        return
    if (_console_last_entry_tag is not None
            and _console_last_entry_color == color
            and not dpg.get_value(_console_last_entry_tag).endswith("\n")):
        dpg.set_value(_console_last_entry_tag, dpg.get_value(_console_last_entry_tag) + text)
    else:
        _console_entry_counter += 1
        _console_last_entry_tag = f"console_entry_{_console_entry_counter}"
        _console_last_entry_color = color
        dpg.add_text(text, parent="console_child", tag=_console_last_entry_tag,
                     color=color, wrap=0, indent=0)
    if dpg.does_item_exist("console_autoscroll") and dpg.get_value("console_autoscroll"):
        dpg.set_frame_callback(dpg.get_frame_count() + 1,
                               lambda: dpg.set_y_scroll("console_child", dpg.get_y_scroll_max("console_child")))


def clear_console_log():
    global _console_last_entry_tag, _console_last_entry_color
    if dpg.does_item_exist("console_child"):
        dpg.delete_item("console_child", children_only=True)
    _console_last_entry_tag = None
    _console_last_entry_color = None


def toggle_window_action(sender=None, app_data=None):
    if isinstance(app_data, bool):
        should_show = app_data
    elif sender == MENU_TAG and dpg.does_item_exist(MENU_TAG):
        should_show = dpg.get_value(MENU_TAG)
    else:
        should_show = not dpg.is_item_shown(WINDOW_TAG)

    if should_show:
        dpg.show_item(WINDOW_TAG)
    else:
        dpg.hide_item(WINDOW_TAG)


def build(on_close):
    with dpg.window(tag=WINDOW_TAG, label="Console Monitor", width=460, height=320, pos=[10, 690], collapsed=True, on_close=on_close):
        with dpg.child_window(tag="console_child", width=-1, height=-30, border=True):
            pass
        with dpg.group(horizontal=True):
            dpg.add_checkbox(label="Auto-scroll", default_value=True, tag="console_autoscroll")
            dpg.add_button(label="Clear", callback=clear_console_log)
