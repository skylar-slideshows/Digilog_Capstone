"""Connect window: serial port connection UI."""
import dearpygui.dearpygui as dpg
import app_state
from stm32_serial import STM32Serial

WINDOW_TAG = "console"
MENU_TAG = "menu_win_connect"

# An empty port means "find the console": its USB port is recognised by its USB IDs (CONFIG.yaml)
DEFAULT_COM_PORT = ""
DEFAULT_BAUD_RATE = "921600"  # a bucket's UART needs it; the console's USB port ignores it


def _saved_or_default(key, default):
    value = app_state.USER_SETTINGS.get(key)
    value = str(value).strip() if value is not None else ""
    return value or default


def _initial_port():
    """The console's USB port if it's plugged in, otherwise the port used last time."""
    return STM32Serial.find_console_port() or _saved_or_default("com_port", DEFAULT_COM_PORT)


def find_console_callback(sender=None, app_data=None):
    port = STM32Serial.find_console_port()
    if port:
        dpg.set_value("com_port_input", port)
        app_state.save_user_settings()
        app_state.update_status(f"Status: Found the console on {port}")
    else:
        app_state.update_status("Status: No Digilog console found - is it plugged in?")


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
        dpg.add_input_text(label="COM Port", default_value=_initial_port(), tag="com_port_input",
                           hint="empty: find the console")
        # always start at the required baud rate, regardless of any older saved value
        dpg.add_input_text(label="Baud Rate", default_value=DEFAULT_BAUD_RATE, tag="baud_input")
        with dpg.tooltip("baud_input"):
            dpg.add_text("Only used by a bucket's UART; the console's USB port ignores it")
        with dpg.group(horizontal=True):
            dpg.add_button(label="Connect", callback=connect_callback)
            dpg.add_button(label="Find Console", callback=find_console_callback)
        dpg.add_text("Status: Disconnected", tag="status_text")

    dpg.bind_item_handler_registry("com_port_input", "connect_field_focus_handler")
    dpg.bind_item_handler_registry("baud_input", "connect_field_focus_handler")
