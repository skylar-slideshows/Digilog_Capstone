import sys
import time
import re
import threading
from typing import Callable, Optional, List
import serial
import serial.tools.list_ports

from config import debug_no_serial_timeout

# Matches an ack reply from the STM32: "$ack <original command text>^"
_ACK_PATTERN = re.compile(r"\$ack (.*?)\^")

_DEBUG_SENT_COLOR = (80, 220, 120, 255)
_DEBUG_NO_ACK_COLOR = (220, 80, 80, 255)


class STM32Serial:
    """
    Framework for serial USART communication with an STM32 microcontroller.
    Provides connection management, asynchronous background reading, and byte/string transmission.
    """

    def __init__(self, port: str = "", baudrate: int = 921600, timeout: float = 1.0):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self._ser: Optional[serial.Serial] = None
        self._rx_thread: Optional[threading.Thread] = None
        self._running = False
        self._lock = threading.Lock()

        # Ack tracking: incoming "$ack <message>^" replies are scanned out of the raw
        # receive stream independently of on_receive, so callers can wait for a specific ack.
        self._ack_rx_buffer = ""
        self._ack_lock = threading.Lock()
        self._ack_event = threading.Event()
        self._last_ack_message: Optional[str] = None

        # Callbacks for asynchronous serial events
        self.on_receive: Optional[Callable[[bytes], None]] = None
        self.on_error: Optional[Callable[[str], None]] = None
        self.on_status_change: Optional[Callable[[str], None]] = None
        # Debug-only console logging (see debug_no_serial_timeout): (text, color) -> None
        self.on_debug_log: Optional[Callable[[str, tuple], None]] = None

    @staticmethod
    def available_ports() -> List[str]:
        """List all available serial COM / tty ports on the system."""
        return [p.device for p in serial.tools.list_ports.comports()]

    @property
    def is_connected(self) -> bool:
        """Return True if the serial port is currently open."""
        return self._ser is not None and self._ser.is_open

    def connect(self, port: str, baudrate: int = 921600) -> tuple[bool, str]:
        """
        Connect to the STM32 USART serial port.
        Returns a tuple of (success: bool, status_message: str).
        """
        self.port = port
        self.baudrate = baudrate
        self.disconnect()

        try:
            self._ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=self.timeout
            )
            self._running = True
            self._rx_thread = threading.Thread(target=self._listen_loop, daemon=True)
            self._rx_thread.start()
            msg = f"Connected to {self.port} @ {self.baudrate}"
            if self.on_status_change:
                self.on_status_change(msg)
            return True, msg
        except Exception as e:
            msg = f"Error connecting to {self.port}: {e}"
            if self.on_error:
                self.on_error(msg)
            return False, msg

    def disconnect(self):
        """Disconnect the serial port and terminate the listener thread."""
        self._running = False
        if self._rx_thread and self._rx_thread.is_alive():
            self._rx_thread.join(timeout=0.5)
        self._rx_thread = None

        with self._lock:
            if self._ser and self._ser.is_open:
                try:
                    self._ser.close()
                except Exception:
                    pass
            self._ser = None

    def send(self, data: bytes | str, wrap: bool = True) -> tuple[bool, str, int]:
        """
        Send raw bytes or string payload to STM32 over USART.
        String payloads are wrapped as "%<data>%" so the STM32 can frame each command, unless wrap=False.
        Returns a tuple of (success: bool, status_message: str, bytes_written: int).
        """
        if not self.is_connected or not self._ser:
            msg = "Error - Port not open"
            if self.on_error:
                self.on_error(msg)
            return False, msg, 0

        if isinstance(data, str):
            wrapped = f"%{data}%" if wrap else data
            payload = wrapped.encode('utf-8')
        else:
            wrapped = None
            payload = data

        with self._lock:
            try:
                written = self._ser.write(payload)
                self._ser.flush()
                msg = f"Sent {written} bytes"
                if debug_no_serial_timeout and wrapped is not None and self.on_debug_log:
                    self.on_debug_log(f"> {wrapped}\n", _DEBUG_SENT_COLOR)
                return True, msg, written
            except Exception as e:
                msg = f"Error sending data: {e}"
                if self.on_error:
                    self.on_error(msg)
                return False, msg, 0

    def send_knob_update(self, channel: int, ctrl_id: int, value: int, wait_ack: bool = False,
                          timeout: float = 0.1, max_retries: int = 5) -> tuple[bool, str, int]:
        """Send a knob update using the STM32 protocol: ud <channel> <ctrl_id> <uint16>."""
        if not 0 <= channel:
            return False, "Error - Invalid channel", 0
        if not 0 <= ctrl_id:
            return False, "Error - Invalid ctrl_id", 0
        if not 0 <= value <= 0xFFFF:
            return False, "Error - Knob value must be an unsigned 16-bit integer", 0
        data = f"ud {channel} {ctrl_id} {value}"
        if wait_ack:
            return self.send_with_ack(data, timeout=timeout, max_retries=max_retries)
        return self.send(data)

    def send_switch_update(self, channel: int, sw_id: int, value: bool | int | str, wait_ack: bool = False,
                           timeout: float = 0.1, max_retries: int = 5) -> tuple[bool, str, int]:
        """Send a switch update using the STM32 protocol: sw <channel> <sw_id> <value>."""
        if channel < 0 or sw_id < 0:
            return False, "Error - Invalid channel or sw_id", 0
        if isinstance(value, bool):
            value = int(value)
        elif isinstance(value, int) and value in (0, 1):
            value = str(value)
        elif isinstance(value, str) and value in ("00", "01", "10"):
            pass
        else:
            return False, "Error - Switch value must be 0, 1, 00, 01, or 10", 0
        data = f"sw {channel} {sw_id} {value}"
        if wait_ack:
            return self.send_with_ack(data, timeout=timeout, max_retries=max_retries)
        return self.send(data)

    def send_channel_select(self, channel: int | list[int], suffix: str = "", wait_ack: bool = False,
                            timeout: float = 0.1, max_retries: int = 5) -> tuple[bool, str, int]:
        """Send a channel select command: 'sel <channel>' for a single channel, or
        'sel <a>:<b>:<c>' (in selection order) for a multi/range-select. suffix, if given
        (e.g. "link"/"unlink"), is appended as a word before the closing frame."""
        if isinstance(channel, (list, tuple)):
            if not channel or any(c < 0 for c in channel):
                return False, "Error - Invalid channel", 0
            payload = ":".join(str(c) for c in channel)
        else:
            if channel < 0:
                return False, "Error - Invalid channel", 0
            payload = str(channel)
        if suffix:
            payload += f" {suffix}"
        data = f"sel {payload}"
        if wait_ack:
            return self.send_with_ack(data, timeout=timeout, max_retries=max_retries)
        return self.send(data)

    def send_channel_name(self, channel: int, name: str, wait_ack: bool = False,
                          timeout: float = 0.1, max_retries: int = 5) -> tuple[bool, str, int]:
        """Send a channel name update using the STM32 protocol: chnm <channel> <name>."""
        if channel < 0:
            return False, "Error - Invalid channel", 0
        data = f"chnm {channel} {name}"
        if wait_ack:
            return self.send_with_ack(data, timeout=timeout, max_retries=max_retries)
        return self.send(data)

    def send_with_ack(self, data: str, timeout: float = 0.1, max_retries: int = 5) -> tuple[bool, str, int]:
        """
        Send a string command and wait for a matching "$ack <data>^" reply from the STM32,
        resending on timeout up to max_retries times. Returns as soon as the matching ack
        arrives (no fixed delay). If no matching ack arrives after max_retries attempts,
        returns a failure result and sends nothing further - unless debug_no_serial_timeout
        is set, in which case the ack check still runs (200ms/5 retries, logging "no ack" in
        red) but a missing ack is treated as non-fatal so the caller can keep going.
        """
        debug = debug_no_serial_timeout
        if debug:
            timeout, max_retries = 0.2, 5
        expected = data.strip()
        last_msg = "Error - Port not open"
        last_written = 0
        for _ in range(max_retries):
            with self._ack_lock:
                self._ack_event.clear()
                self._last_ack_message = None
            success, last_msg, last_written = self.send(data)
            if not success:
                return False, last_msg, last_written
            deadline = time.monotonic() + timeout
            acked = False
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0 or not self._ack_event.wait(remaining):
                    break
                with self._ack_lock:
                    received = self._last_ack_message
                    self._ack_event.clear()
                if received == expected:
                    acked = True
                    break
                # Stale/mismatched ack - keep waiting until this attempt's deadline.
            if acked:
                return True, "Ack received", last_written
            if debug and self.on_debug_log:
                self.on_debug_log(f"No ack: {data}\n", _DEBUG_NO_ACK_COLOR)
        if debug:
            return True, "Debug - no ack, continuing", last_written
        msg = f"Error - No ack after {max_retries} attempts"
        if self.on_error:
            self.on_error(msg)
        return False, msg, last_written

    def _process_ack_data(self, text: str):
        """Scan newly received text for "$ack <message>^" replies, independent of on_receive."""
        with self._ack_lock:
            self._ack_rx_buffer += text
            last_end = 0
            for match in _ACK_PATTERN.finditer(self._ack_rx_buffer):
                self._last_ack_message = match.group(1).strip()
                last_end = match.end()
                self._ack_event.set()
            if last_end:
                self._ack_rx_buffer = self._ack_rx_buffer[last_end:]
            elif len(self._ack_rx_buffer) > 2048:
                self._ack_rx_buffer = self._ack_rx_buffer[-2048:]

    def _listen_loop(self):
        """Background thread worker for reading incoming USART messages from STM32."""
        while self._running and self._ser and self._ser.is_open:
            try:
                if self._ser.in_waiting > 0:
                    with self._lock:
                        data = self._ser.read(self._ser.in_waiting)
                    if data:
                        self._process_ack_data(data.decode("utf-8", errors="replace"))
                        if self.on_receive:
                            self.on_receive(data)
                else:
                    time.sleep(0.01)
            except Exception as e:
                if self._running and self.on_error:
                    self.on_error(f"Serial read error: {e}")
                time.sleep(0.05)
