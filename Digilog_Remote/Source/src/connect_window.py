"""Connect window: serial port connection UI."""
import sys
import dearpygui.dearpygui as dpg
import app_state

WINDOW_TAG = "console"
MENU_TAG = "menu_win_connect"

DEFAULT_COM_PORT = "COM3" if sys.platform == "win32" else "/dev/tty.usbmodem11203"
DEFAULT_BAUD_RATE = "921600"


def _saved_or_default(key, default):
    value = app_state.USER_SETTINGS.get(key)
    value = str(value).strip() if value is not None else ""
    return value or default


def connect_callback(sender, app_data):
    port = dpg.get_value("com_port_input")
    try:
        baud = int(dpg.get_value("baud_input"))
    except ValueError:
        app_state.update_status("Status: Error - Invalid Baud Rate")
        return

    app_state.save_user_settings()
    if app_state.on_connect_handler:
        app_state.on_connect_handler(port, baud)
    else:
        app_state.update_status(f"Status: Connect requested ({port} @ {baud})")


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
    with dpg.item_handler_registry(tag="connect_field_focus_handler"):
        dpg.add_item_deactivated_after_edit_handler(callback=lambda s, a, u: app_state.save_user_settings())

    with dpg.window(tag=WINDOW_TAG, label="Connect", width=360, height=130, pos=[480, 590], on_close=on_close):
        dpg.add_input_text(label="COM Port", default_value=_saved_or_default("com_port", DEFAULT_COM_PORT), tag="com_port_input")
        # always start at the required baud rate, regardless of any older saved value
        dpg.add_input_text(label="Baud Rate", default_value=DEFAULT_BAUD_RATE, tag="baud_input")
        dpg.add_button(label="Connect", callback=connect_callback)
        dpg.add_text("Status: Disconnected", tag="status_text")

    dpg.bind_item_handler_registry("com_port_input", "connect_field_focus_handler")
    dpg.bind_item_handler_registry("baud_input", "connect_field_focus_handler")
