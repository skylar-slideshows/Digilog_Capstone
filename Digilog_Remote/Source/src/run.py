"""
Digilog Remote Orchestrator
Entry point that coordinates GUI (window.py) and STM32 Serial USART (stm32_serial.py).
"""
import queue
import sys
import threading
from stm32_serial import STM32Serial
import window
import app_state
import console_monitor_window
from config import ack_timeout_ms

ACK_MAX_RETRIES = 5

def knob_value_to_uint16(value: float, minimum: float, maximum: float) -> int:
    if maximum <= minimum:
        raise ValueError("Knob maximum must be greater than minimum")
    normalized = (value - minimum) / (maximum - minimum)
    return round(max(0.0, min(1.0, normalized)) * 0xFFFF)

def handle_incoming_command(command_text: str):
    """Parse a $sw/$ud/$chnm/$chct line echoed back by the STM32 and mirror it in the GUI."""
    parts = command_text.split(" ")
    if len(parts) < 2:
        return
    cmd = parts[0]
    try:
        if cmd == "ud" and len(parts) >= 4:
            app_state.apply_incoming_knob_update(int(parts[1]), int(parts[2]), int(parts[3]))
        elif cmd == "sw" and len(parts) >= 4:
            app_state.apply_incoming_switch_update(int(parts[1]), int(parts[2]), parts[3])
        elif cmd == "chnm":
            name = " ".join(parts[2:]) if len(parts) > 2 else ""
            app_state.apply_incoming_channel_name(int(parts[1]), name)
        elif cmd == "chct":
            app_state.apply_incoming_channel_count(int(parts[1]))
    except (ValueError, IndexError):
        pass

def main():
    # Instantiate the STM32 USART serial communicator
    stm32 = STM32Serial()
    rx_buffer = [""]

    # Switch ("sw") sends are queued and worked off one at a time by a single background thread,
    # so a mute/solo/cycle press that mirrors to several linked/selected channels sends them
    # staggered (each waiting for its own ack) instead of racing each other on the shared serial
    # connection. The GUI already updates instantly on its own; only the wire traffic trails behind.
    switch_queue = queue.Queue()

    def switch_worker():
        while True:
            channel, sw_id, value = switch_queue.get()
            try:
                success, msg, _ = stm32.send_switch_update(
                    channel, sw_id, value, wait_ack=True, timeout=ack_timeout_ms / 1000, max_retries=ACK_MAX_RETRIES
                )
                if not success:
                    app_state.update_status(f"Status: {msg}")
                    app_state.show_no_connection_popup()
            finally:
                switch_queue.task_done()

    threading.Thread(target=switch_worker, daemon=True).start()

    # Knob ("ud") sends - both live and final - are queued and worked off one at a time
    # by a single background thread. Sending live updates synchronously would block the
    # main thread (which now also drives rendering and callbacks under manual callback
    # management), causing stutter/glitches with the STM32 connected; spawning one thread
    # per final send raced multiple mirrored channels' ack-waits against each other.
    knob_queue = queue.Queue()

    def knob_worker():
        while True:
            channel, ctrl_id, knob_value, is_final = knob_queue.get()
            try:
                if is_final:
                    success, msg, _ = stm32.send_knob_update(
                        channel, ctrl_id, knob_value, wait_ack=True, timeout=ack_timeout_ms / 1000, max_retries=ACK_MAX_RETRIES
                    )
                    if not success:
                        app_state.update_status(f"Status: {msg}")
                        app_state.show_no_connection_popup()
                else:
                    stm32.send_knob_update(channel, ctrl_id, knob_value)
            finally:
                knob_queue.task_done()

    threading.Thread(target=knob_worker, daemon=True).start()

    # Define callbacks for serial events
    def on_serial_receive(data: bytes):
        try:
            msg = data.decode("utf-8", errors="replace")
            rx_color = (20, 20, 20, 255) if app_state.toggle_values.get("light_theme", False) else (255, 255, 255, 255)
            console_monitor_window.append_console_log(msg, color=rx_color)
        except Exception:
            return
        rx_buffer[0] += msg
        while "\n" in rx_buffer[0]:
            line, rx_buffer[0] = rx_buffer[0].split("\n", 1)
            line = line.strip("\r").strip()
            if line.startswith("$"):
                handle_incoming_command(line[1:])

    def on_serial_error(err_msg: str):
        app_state.update_status(f"Status: {err_msg}")

    def on_serial_status(status_msg: str):
        app_state.update_status(f"Status: {status_msg}")

    def on_serial_debug_log(text: str, color: tuple):
        console_monitor_window.append_console_log(text, color=color)

    stm32.on_receive = on_serial_receive
    stm32.on_error = on_serial_error
    stm32.on_status_change = on_serial_status
    stm32.on_debug_log = on_serial_debug_log

    # Define handlers for UI events
    def handle_connect(port: str, baud: int):
        success, msg = stm32.connect(port=port, baudrate=baud)
        app_state.set_stm32_connected(success)
        app_state.update_status(f"Status: {msg}")

    def handle_knob_change(channel: int, ctrl_id: int, value: float, minimum: float, maximum: float, is_final: bool = False):
        knob_value = knob_value_to_uint16(value, minimum, maximum)
        knob_queue.put((channel, ctrl_id, knob_value, is_final))

    def handle_button_change(channel: int | list[int], sw_id: int | None, value: bool | int | str | None, tag: str):
        if tag in ("sel", "sel_link", "sel_unlink"):
            suffix = {"sel_link": "link", "sel_unlink": "unlink"}.get(tag, "")
            def send_select():
                success, msg, _ = stm32.send_channel_select(
                    channel, suffix=suffix, wait_ack=True,
                    timeout=ack_timeout_ms / 1000, max_retries=ACK_MAX_RETRIES
                )
                if not success:
                    app_state.update_status(f"Status: {msg}")
                    app_state.show_no_connection_popup()
            threading.Thread(target=send_select, daemon=True).start()
            return

        switch_queue.put((channel, sw_id, value))

    def handle_channel_name_change(channel: int, name: str):
        def send_name():
            success, msg, _ = stm32.send_channel_name(
                channel, name, wait_ack=True, timeout=ack_timeout_ms / 1000, max_retries=ACK_MAX_RETRIES
            )
            if not success:
                app_state.update_status(f"Status: {msg}")
                app_state.show_no_connection_popup()
        threading.Thread(target=send_name, daemon=True).start()

    def handle_state_loaded(
        channel_names: list[tuple[int, str]],
        knob_updates: list[tuple[int, int, float, float, float]],
        switch_updates: list[tuple[int, int, bool | int | str]],
    ):
        def send_sequence():
            timeout = ack_timeout_ms / 1000
            for channel, name in channel_names:
                success, msg, _ = stm32.send_channel_name(
                    channel, name, wait_ack=True, timeout=timeout, max_retries=ACK_MAX_RETRIES
                )
                if not success:
                    app_state.update_status(f"Status: {msg}")
                    app_state.show_no_connection_popup()
                    return
            for channel, ctrl_id, value, minimum, maximum in knob_updates:
                success, msg, _ = stm32.send_knob_update(
                    channel, ctrl_id, knob_value_to_uint16(value, minimum, maximum),
                    wait_ack=True, timeout=timeout, max_retries=ACK_MAX_RETRIES
                )
                if not success:
                    app_state.update_status(f"Status: {msg}")
                    app_state.show_no_connection_popup()
                    return
            for channel, sw_id, value in switch_updates:
                success, msg, _ = stm32.send_switch_update(
                    channel, sw_id, value, wait_ack=True, timeout=timeout, max_retries=ACK_MAX_RETRIES
                )
                if not success:
                    app_state.update_status(f"Status: {msg}")
                    app_state.show_no_connection_popup()
                    return
        threading.Thread(target=send_sequence, daemon=True).start()

    def handle_sync_to_console(result_callback):
        def send_md():
            success, msg, _ = stm32.send_with_ack("md 0", timeout=ack_timeout_ms / 1000, max_retries=ACK_MAX_RETRIES)
            if not success:
                app_state.update_status(f"Status: {msg}")
                app_state.show_no_connection_popup()
            result_callback(success)
        threading.Thread(target=send_md, daemon=True).start()

    # Register serial handlers with the shared app state
    app_state.set_serial_handlers(
        on_connect=handle_connect,
        on_knob_change=handle_knob_change,
        on_button_change=handle_button_change,
        on_channel_name_change=handle_channel_name_change,
        on_state_loaded=handle_state_loaded,
        on_sync_to_console=handle_sync_to_console,
    )

    # Build and launch the GUI
    try:
        window.build_gui()
        window.start_gui()
    finally:
        stm32.disconnect()
        window.cleanup_gui()

if __name__ == "__main__":
    main()
