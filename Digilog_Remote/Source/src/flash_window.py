"""Flash Firmware window: invokes STM32_Programmer_CLI over SWD/ST-Link to program the
STM32 from a picked .elf/.bin file - the GUI equivalent of running the CLI by hand."""
import glob
import shutil
import subprocess
import sys
import threading
from pathlib import Path

import dearpygui.dearpygui as dpg
import app_state

WINDOW_TAG = "flash_firmware_window"
MENU_TAG = "menu_win_flash_firmware"

# STM32CubeProgrammer's default Windows install doesn't put the CLI on PATH like it typically
# is on mac/Linux, so point at its default install location there instead.
DEFAULT_PROGRAMMER_PATH = (
    r"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
    if sys.platform == "win32" else "STM32_Programmer_CLI"
)
_SUCCESS_COLOR = (120, 200, 120, 255)
_ERROR_COLOR = (220, 80, 80, 255)
_IDLE_TEXTS = ("Status: Idle", "STM32 Disconnected")
# Nucleo/Discovery boards with an on-board ST-LINK V2-1/V3 (DAPLink) mount as a USB mass-storage
# drive named after the board - copying a .bin onto it triggers the on-board bootloader to flash.
DEFAULT_FALLBACK_VOLUME = "NOD_G474RE"


def _candidate_programmer_paths():
    """Common STM32CubeProgrammer install locations - a GUI-launched app's PATH usually lacks
    whatever a login shell's profile adds, so "STM32_Programmer_CLI not found" is common even
    when it's installed and runs fine from a terminal."""
    if sys.platform == "win32":
        return [DEFAULT_PROGRAMMER_PATH]
    home = str(Path.home())
    patterns = [
        "/opt/ST/STM32CubeCLT_*/STM32CubeProgrammer/bin/STM32_Programmer_CLI",
        home + "/Library/Application Support/stm32cube/bundles/programmer/*/bin/STM32_Programmer_CLI",
        "/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI",
    ]
    found = []
    for pattern in patterns:
        found.extend(sorted(glob.glob(pattern), reverse=True))
    return found


def _resolve_programmer_path(configured_path):
    """Prefer whatever path is actually configured/typed if it exists; otherwise search known
    install locations; otherwise fall back to the bare command name and hope it's on PATH."""
    configured_path = (configured_path or "").strip()
    if configured_path and Path(configured_path).exists():
        return configured_path
    for candidate in _candidate_programmer_paths():
        if Path(candidate).exists():
            return candidate
    return configured_path or DEFAULT_PROGRAMMER_PATH


def _find_mounted_volume(volume_name):
    """Return the mount path of a USB mass-storage drive with this exact volume name, or None
    if it isn't currently plugged in/mounted."""
    volume_name = (volume_name or "").strip()
    if not volume_name:
        return None
    if sys.platform == "win32":
        import ctypes
        name_buf = ctypes.create_unicode_buffer(261)
        for letter in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
            root = f"{letter}:\\"
            if not Path(root).exists():
                continue
            if ctypes.windll.kernel32.GetVolumeInformationW(root, name_buf, len(name_buf), None, None, None, None, 0):
                if name_buf.value == volume_name:
                    return root
        return None
    candidate = Path("/Volumes") / volume_name
    return str(candidate) if candidate.is_dir() else None


def _saved_or_default(key, default):
    value = app_state.USER_SETTINGS.get(key)
    value = str(value).strip() if value is not None else ""
    return value or default


def _set_status(text, color=None):
    if not dpg.does_item_exist("flash_status_text"):
        return
    dpg.set_value("flash_status_text", text)
    dpg.configure_item("flash_status_text", color=color or (255, 255, 255, 255))


def _refresh_connection_status():
    # only touch the "at rest" states - never clobber an in-progress/just-finished flash message
    if dpg.does_item_exist("flash_status_text") and dpg.get_value("flash_status_text") not in _IDLE_TEXTS:
        return
    if app_state.stm32_connected:
        _set_status("Status: Idle")
    else:
        _set_status("STM32 Disconnected", _ERROR_COLOR)


def _run_flash(programmer_path, firmware_path, fallback_bin_path, fallback_volume):
    resolved_path = _resolve_programmer_path(programmer_path)
    cmd = [
        resolved_path, "-c", "port=SWD", "mode=UR", "reset=SWrst", "freq=480",
        "-w", firmware_path, "-rst", "-run",
    ]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    except FileNotFoundError:
        _fall_back_to_copy(programmer_path, firmware_path, fallback_bin_path, fallback_volume)
        return
    except subprocess.TimeoutExpired:
        _set_status("Status: Flash timed out.", _ERROR_COLOR)
        return
    finally:
        if dpg.does_item_exist("flash_button"):
            dpg.configure_item("flash_button", enabled=True, label="Flash")

    if result.returncode == 0:
        _set_status("Status: Flash succeeded.", _SUCCESS_COLOR)
    else:
        # STM32_Programmer_CLI writes its actual error to stdout, not stderr
        last_line = next((line for line in reversed((result.stdout or result.stderr or "").splitlines()) if line.strip()), "unknown error")
        _set_status(f"Status: Flash failed - {last_line.strip()}", _ERROR_COLOR)


def _fall_back_to_copy(programmer_path, firmware_path, fallback_bin_path, fallback_volume):
    """Only runs if STM32_Programmer_CLI genuinely couldn't be found/launched. Drag-and-drop
    flashing needs a raw .bin (not .elf) - use the explicit fallback file if set, else fall back
    to the main firmware file only if it's already a .bin."""
    bin_path = fallback_bin_path or (firmware_path if firmware_path.lower().endswith(".bin") else "")
    if not bin_path:
        _set_status(f"Status: '{programmer_path}' not found - pick a fallback .bin file to enable drag-and-drop flashing.", _ERROR_COLOR)
        return
    dest_dir = _find_mounted_volume(fallback_volume)
    if not dest_dir:
        _set_status(f"Status: '{programmer_path}' not found and '{fallback_volume}' isn't mounted - plug in the board.", _ERROR_COLOR)
        return
    dest = Path(dest_dir) / Path(bin_path).name
    try:
        shutil.copy2(bin_path, dest)
    except OSError as error:
        _set_status(f"Status: CLI not found and copy fallback failed - {error}", _ERROR_COLOR)
        return
    _set_status(f"Status: CLI not found - copied {dest.name} to {fallback_volume}.", _SUCCESS_COLOR)


def flash_callback(sender=None, app_data=None):
    programmer_path = (dpg.get_value("programmer_path_input") or "").strip() or DEFAULT_PROGRAMMER_PATH
    firmware_path = (dpg.get_value("firmware_path_input") or "").strip()
    fallback_bin_path = (dpg.get_value("fallback_bin_path_input") or "").strip()
    fallback_volume = (dpg.get_value("fallback_volume_input") or "").strip()
    if not firmware_path:
        _set_status("Status: Choose a firmware file first.", _ERROR_COLOR)
        return
    app_state.save_user_settings()
    dpg.configure_item("flash_button", enabled=False, label="Flashing...")
    _set_status("Status: Flashing...")
    threading.Thread(target=_run_flash, args=(programmer_path, firmware_path, fallback_bin_path, fallback_volume), daemon=True).start()


def _open_native_file_dialog(initial_dir=None, title="Choose Firmware File", filetypes=None):
    """Windows-only native picker, matching window.py's save/open dialog pattern."""
    try:
        import tkinter as tk
        from tkinter import filedialog
        root = tk.Tk()
        root.withdraw()
        root.attributes("-topmost", True)
        kwargs = {"initialdir": str(initial_dir)} if initial_dir else {}
        res = filedialog.askopenfilename(
            title=title,
            filetypes=filetypes or [("Firmware Files", "*.elf *.bin"), ("All Files", "*.*")],
            **kwargs,
        )
        root.destroy()
        return res if res else None
    except Exception as e:
        print(f"Native Windows dialog error: {e}")
        return None


def browse_firmware_callback(sender=None, app_data=None):
    # open at the most recently used firmware file's folder, like the other file dialogs do
    current = (dpg.get_value("firmware_path_input") or "").strip()
    initial_dir = Path(current).parent if current else None
    if sys.platform == "win32":
        selected = _open_native_file_dialog(initial_dir)
        if selected:
            dpg.set_value("firmware_path_input", selected)
            app_state.save_user_settings()
        return
    if initial_dir:
        dpg.configure_item("firmware_file_dialog", default_path=str(initial_dir))
    dpg.show_item("firmware_file_dialog")


def firmware_file_selected_callback(sender, app_data):
    path = app_data.get("file_path_name", "")
    if path:
        dpg.set_value("firmware_path_input", path)
        app_state.save_user_settings()


def browse_fallback_bin_callback(sender=None, app_data=None):
    current = (dpg.get_value("fallback_bin_path_input") or "").strip()
    initial_dir = Path(current).parent if current else None
    if sys.platform == "win32":
        selected = _open_native_file_dialog(initial_dir, title="Choose Fallback .bin File", filetypes=[("Binary Files", "*.bin"), ("All Files", "*.*")])
        if selected:
            dpg.set_value("fallback_bin_path_input", selected)
            app_state.save_user_settings()
        return
    if initial_dir:
        dpg.configure_item("fallback_bin_file_dialog", default_path=str(initial_dir))
    dpg.show_item("fallback_bin_file_dialog")


def fallback_bin_file_selected_callback(sender, app_data):
    path = app_data.get("file_path_name", "")
    if path:
        dpg.set_value("fallback_bin_path_input", path)
        app_state.save_user_settings()


def toggle_window_action(sender=None, app_data=None):
    if isinstance(app_data, bool):
        should_show = app_data
    elif sender == MENU_TAG and dpg.does_item_exist(MENU_TAG):
        should_show = dpg.get_value(MENU_TAG)
    else:
        should_show = not dpg.is_item_shown(WINDOW_TAG)

    if should_show:
        _refresh_connection_status()
        dpg.show_item(WINDOW_TAG)
    else:
        dpg.hide_item(WINDOW_TAG)


def build(on_close):
    with dpg.item_handler_registry(tag="flash_field_focus_handler"):
        dpg.add_item_deactivated_after_edit_handler(callback=lambda s, a, u: app_state.save_user_settings())

    with dpg.file_dialog(tag="firmware_file_dialog", directory_selector=False, show=False,
                          callback=firmware_file_selected_callback, modal=True, width=700, height=400):
        dpg.add_file_extension(".elf", tag="firmware_file_dialog_elf")
        dpg.add_file_extension(".bin", tag="firmware_file_dialog_bin")
        dpg.add_file_extension(".*", tag="firmware_file_dialog_all")

    with dpg.file_dialog(tag="fallback_bin_file_dialog", directory_selector=False, show=False,
                          callback=fallback_bin_file_selected_callback, modal=True, width=700, height=400):
        dpg.add_file_extension(".bin", tag="fallback_bin_file_dialog_bin")
        dpg.add_file_extension(".*", tag="fallback_bin_file_dialog_all")

    with dpg.window(tag=WINDOW_TAG, label="Flash Firmware", width=440, height=250, pos=[480, 590], show=False, on_close=on_close):
        dpg.add_input_text(label="Programmer CLI", default_value=_saved_or_default("programmer_path", DEFAULT_PROGRAMMER_PATH),
                            tag="programmer_path_input", width=250)
        with dpg.group(horizontal=True):
            dpg.add_input_text(tag="firmware_path_input", default_value=_saved_or_default("firmware_path", ""),
                                width=300, readonly=True)
            dpg.add_button(label="Browse...", callback=browse_firmware_callback)
        dpg.add_button(label="Flash", tag="flash_button", callback=flash_callback)
        dpg.add_text("Status: Idle", tag="flash_status_text")
        dpg.add_separator()
        dpg.add_text("Fallback: copy to on-board ST-LINK/DAPLink drive if the CLI isn't found")
        with dpg.group(horizontal=True):
            dpg.add_input_text(tag="fallback_bin_path_input", default_value=_saved_or_default("fallback_bin_path", ""),
                                width=300, readonly=True)
            dpg.add_button(label="Browse...", callback=browse_fallback_bin_callback)
        dpg.add_input_text(label="Drive Name", default_value=_saved_or_default("fallback_volume", DEFAULT_FALLBACK_VOLUME),
                            tag="fallback_volume_input", width=150)

    dpg.bind_item_handler_registry("programmer_path_input", "flash_field_focus_handler")
    dpg.bind_item_handler_registry("fallback_volume_input", "flash_field_focus_handler")
    dpg.bind_item_theme("firmware_file_dialog", "global_theme")
    dpg.bind_item_theme("fallback_bin_file_dialog", "global_theme")
    _refresh_connection_status()


app_state.register_connection_change_hook(lambda connected: _refresh_connection_status())

