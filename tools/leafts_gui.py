"""
LeafTS Studio — a GUI tool to explore LeafTS on your embedded board.
=================
• Select a board → generate a ready-to-use C project
• Connect via UART (COM port) or TCP (emulator)
• Terminal: type LeafTS commands directly in the app

Dependencies: PyQt6, pyserial
"""

from __future__ import annotations

import json
import shutil
import socket
import subprocess
import sys
import threading
import time
from datetime import datetime
from pathlib import Path

import serial
import serial.tools.list_ports
from PyQt6.QtCore import QEvent, Qt, QThread, pyqtSignal
from PyQt6.QtGui import QColor, QFont, QKeySequence, QShortcut, QTextCursor
from PyQt6.QtWidgets import (
    QApplication, QComboBox, QFileDialog, QGroupBox, QHBoxLayout,
    QLabel, QLineEdit, QListWidget, QListWidgetItem, QMainWindow,
    QPushButton, QSizePolicy, QSplitter, QSpinBox, QStatusBar,
    QTabWidget, QTextEdit, QVBoxLayout, QWidget, QFrame, QMessageBox,
)

# ---------------------------------------------------------------------------
# Paths — work both in dev (plain .py) and frozen (PyInstaller .exe)
# ---------------------------------------------------------------------------
def _base_dir() -> Path:
    """Return the directory that contains bundled data files.

    • Frozen exe  → sys._MEIPASS  (temp dir where PyInstaller extracts files)
    • Dev mode    → two levels up from this script  (project root)
    """
    if getattr(sys, "frozen", False):
        return Path(sys._MEIPASS)      # type: ignore[attr-defined]
    return Path(__file__).parent.parent


def _resolve_server_exe() -> Path:
    """Resolve the best leafts_uart executable path for current mode."""
    base = _base_dir()
    if getattr(sys, "frozen", False):
        return base / "leafts_uart.exe"

    candidates = [
        base / "out" / "build" / "virtual" / "leafts_uart.exe",
        base / "build" / "leafts_uart.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def _normalize_wire_command(cmd: str) -> str:
    """Translate user-friendly SQL-like syntax to wire protocol understood by all server versions."""
    raw = cmd.strip()
    if not raw:
        return raw

    lowered = raw.lower()
    if lowered in ("select", "select latest"):
        return "latest"
    if lowered in ("select *", "select all"):
        return "list"
    if lowered == "select count(*)":
        return "count"
    if lowered == "select min(value)":
        return "get_min"
    if lowered == "select max(value)":
        return "get_max"
    if lowered == "select avg(value)":
        return "get_avg"
    if lowered in ("delete from leafts", "truncate table leafts"):
        return "erase"

    ts_range = lowered.split()
    if len(ts_range) == 7 and ts_range[:5] == ["select", "*", "where", "timestamp", "between"]:
        return f"get_range {ts_range[5]} {ts_range[6]}"

    sql_limit = lowered.split()
    if len(sql_limit) == 4 and sql_limit[:3] == ["select", "*", "limit"]:
        return f"get_last {sql_limit[3]}"

    parts = raw.split()
    if len(parts) >= 2 and parts[0].lower() in ("insert", "append"):
        # Keep native command order expected by current backend:
        # insert/append <value> [timestamp|date]
        return raw

    return raw

BASE_DIR    = _base_dir()
BOARDS_JSON = BASE_DIR / "boards" / "boards.json"
SERVER_EXE  = _resolve_server_exe()

TCP_HOST = "127.0.0.1"
TCP_PORT = 5555

# ---------------------------------------------------------------------------
# Board → HAL metadata
# ---------------------------------------------------------------------------
FAMILY_META: dict[str, dict] = {
    "STM32L4": {"fn": "stm32l4_flash_init", "h": "stm32/hal_stm32l4_flash.h", "c": "stm32/hal_stm32l4_flash.c"},
    "STM32F1": {"fn": "stm32f1_flash_init", "h": "stm32/hal_stm32f1_flash.h", "c": "stm32/hal_stm32f1_flash.c"},
    "STM32F2": {"fn": "stm32f2_flash_init", "h": "stm32/hal_stm32f2_flash.h", "c": "stm32/hal_stm32f2_flash.c"},
    "STM32F3": {"fn": "stm32f3_flash_init", "h": "stm32/hal_stm32f3_flash.h", "c": "stm32/hal_stm32f3_flash.c"},
    "STM32F4": {"fn": "stm32f4_flash_init", "h": "stm32/hal_stm32f4_flash.h", "c": "stm32/hal_stm32f4_flash.c"},
    "STM32F7": {"fn": "stm32f7_flash_init", "h": "stm32/hal_stm32f7_flash.h", "c": "stm32/hal_stm32f7_flash.c"},
    "STM32H7": {"fn": "stm32h7_flash_init", "h": "stm32/hal_stm32h7_flash.h", "c": "stm32/hal_stm32h7_flash.c"},
    "STM32G0": {"fn": "stm32g0_flash_init", "h": "stm32/hal_stm32g0_flash.h", "c": "stm32/hal_stm32g0_flash.c"},
    "STM32G4": {"fn": "stm32g4_flash_init", "h": "stm32/hal_stm32g4_flash.h", "c": "stm32/hal_stm32g4_flash.c"},
    "STM32L1": {"fn": "stm32l1_flash_init", "h": "stm32/hal_stm32l1_flash.h", "c": "stm32/hal_stm32l1_flash.c"},
    "STM32L5": {"fn": "stm32l5_flash_init", "h": "stm32/hal_stm32l5_flash.h", "c": "stm32/hal_stm32l5_flash.c"},
    "STM32WB": {"fn": "stm32wb_flash_init", "h": "stm32/hal_stm32wb_flash.h", "c": "stm32/hal_stm32wb_flash.c"},
    "STM32WL": {"fn": "stm32wl_flash_init", "h": "stm32/hal_stm32wl_flash.h", "c": "stm32/hal_stm32wl_flash.c"},
    "nRF52":   {"fn": "nrf52_flash_init",   "h": "nrf/hal_nrf52_flash.h",     "c": "nrf/hal_nrf52_flash.c"},
    "RP2040":  {"fn": "rp2040_flash_init",  "h": "rp/hal_rp2040_flash.h",      "c": "rp/hal_rp2040_flash.c"},
    "RP2350":  {"fn": "rp2040_flash_init",  "h": "rp/hal_rp2040_flash.h",      "c": "rp/hal_rp2040_flash.c"},
    "ESP32":   {"fn": "esp32_flash_init",   "h": "esp32/hal_esp32_flash.h",    "c": "esp32/hal_esp32_flash.c"},
    "ATSAMD":  {"fn": "samd_flash_init",    "h": "hal_samd_flash.h",   "c": "hal_samd_flash.c"},
    "AVR":     {"fn": "avr_flash_init",     "h": "hal_avr_flash.h",    "c": "hal_avr_flash.c"},
}

# Files always included in a generated project
COMMON_FILES = [
    ("include/leafts.h",       "include/leafts.h"),
    ("include/uart_handler.h", "include/uart_handler.h"),
    ("hal/hal_flash.h",        "hal/hal_flash.h"),
    ("hal/hal_uart.h",         "hal/hal_uart.h"),
    ("src/leafts.c",           "src/leafts.c"),
    ("src/uart_handler.c",     "src/uart_handler.c"),
]

# ---------------------------------------------------------------------------
# Code generation helpers
# ---------------------------------------------------------------------------

def _flash_region(b: dict) -> tuple[int, int]:
    base = b.get("flash_base", 0)
    size = b.get("flash_size", 65536)
    if base != 0:
        return base, size
    # NOR-style (ESP32, RP2040 etc.) — reserve first 512 KB for firmware
    db_base = 0x80000
    db_size = max(size - 0x80000, b.get("page_size", 256) * 4)
    return db_base, db_size


def make_snippet(b: dict) -> str:
    meta   = FAMILY_META.get(b["family"], {"fn": "hal_flash_init", "h": "hal_flash.h", "c": ""})
    db_base, db_size = _flash_region(b)
    return (
        f'#include "hal/{meta["h"]}"\n'
        f'#include "include/leafts.h"\n\n'
        f'hal_flash_t flash;\n'
        f'{meta["fn"]}(&flash, 0x{db_base:08X}UL, {db_size}U);\n\n'
        f'leafts_db_t db;\n'
        f'leafts_init(&db, &flash, 0x{db_base:08X}UL, {db_size}U);'
    )


def make_main_c(b: dict) -> str:
    meta     = FAMILY_META.get(b["family"], {"fn": "hal_flash_init", "h": "hal_flash.h", "c": ""})
    db_base, db_size = _flash_region(b)
    flash_kb = b["flash_size"] // 1024
    return (
        f'// Generated by LeafTS Studio\n'
        f'// Board  : {b["name"]}\n'
        f'// MCU    : {b.get("mcu","?").upper()}\n'
        f'// Family : {b["family"]}\n'
        f'// Flash  : {flash_kb} KB  (page={b["page_size"]} B, sectors={b["sector_count"]})\n'
        f'//\n'
        f'// LeafTS region: 0x{db_base:08X} — 0x{db_base + db_size:08X}  ({db_size // 1024} KB)\n'
        f'//\n'
        f'// HOW TO USE:\n'
        f'//   1. Add hal/, include/, src/ to your build system.\n'
        f'//   2. Implement board_uart_init() for your hardware.\n'
        f'//   3. Compile and flash.\n'
        f'\n'
        f'#include <stdint.h>\n'
        f'#include <string.h>\n'
        f'#include "hal/{meta["h"]}"\n'
        f'#include "hal/hal_uart.h"\n'
        f'#include "include/leafts.h"\n'
        f'#include "include/uart_handler.h"\n'
        f'\n'
        f'// ── Implement UART init for your board ────────────────────────────────────────\n'
        f'// Example STM32L4:\n'
        f'//   #include "hal/hal_stm32l4_uart.h"\n'
        f'//   stm32l4_uart_init(&uart);\n'
        f'static void board_uart_init(hal_uart_t *uart)\n'
        f'{{\n'
        f'    (void)uart;\n'
        f'    // TODO: initialise hardware UART here\n'
        f'}}\n'
        f'// ─────────────────────────────────────────────────────────────────────────────\n'
        f'\n'
        f'static hal_flash_t flash;\n'
        f'static hal_uart_t  uart;\n'
        f'static leafts_db_t db;\n'
        f'\n'
        f'int main(void)\n'
        f'{{\n'
        f'    {meta["fn"]}(&flash, 0x{db_base:08X}UL, {db_size}U);\n'
        f'    leafts_init(&db, &flash, 0x{db_base:08X}UL, {db_size}U);\n'
        f'    board_uart_init(&uart);\n'
        f'\n'
        f'    char    line[128];\n'
        f'    uint8_t ch;\n'
        f'    uint32_t len = 0;\n'
        f'\n'
        f'    while (1)\n'
        f'    {{\n'
        f'        if (!uart.receive) continue;\n'
        f'        if (uart.receive(&ch, 1, 10000) != 0) continue;\n'
        f'\n'
        f'        if (ch == \'\\r\' || ch == \'\\n\')\n'
        f'        {{\n'
        f'            if (len == 0) continue;\n'
        f'            line[len] = \'\\0\';\n'
        f'            uart_handler_process(line, &db, &uart);\n'
        f'            len = 0;\n'
        f'        }}\n'
        f'        else if (ch == 0x7F || ch == \'\\b\')\n'
        f'        {{\n'
        f'            if (len > 0) len--;\n'
        f'        }}\n'
        f'        else if (len < sizeof(line) - 1)\n'
        f'        {{\n'
        f'            line[len++] = (char)ch;\n'
        f'        }}\n'
        f'    }}\n'
        f'}}\n'
    )


def generate_project(b: dict, dest: Path) -> list[str]:
    """Copy all needed files to dest. Returns list of relative paths (or '[MISSING] ...')."""
    meta   = FAMILY_META.get(b["family"], {"fn": "hal_flash_init", "h": "hal_flash.h", "c": ""})
    copied: list[str] = []

    def cp(src_rel: str, dst_rel: str):
        src = BASE_DIR / src_rel
        dst = dest / dst_rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        if src.exists():
            shutil.copy2(src, dst)
            copied.append(dst_rel)
        else:
            copied.append(f"[MISSING] {src_rel}")

    for src_rel, dst_rel in COMMON_FILES:
        cp(src_rel, dst_rel)

    cp(f'hal/{meta["h"]}', f'hal/{meta["h"]}')
    if meta["c"]:
        cp(f'src/{meta["c"]}', f'src/{meta["c"]}')

    (dest / "main.c").write_text(make_main_c(b), encoding="utf-8")
    copied.append("main.c  ← generated")

    return copied


# ---------------------------------------------------------------------------
# Backends
# ---------------------------------------------------------------------------
class Backend(QThread):
    connected    = pyqtSignal()
    disconnected = pyqtSignal(str)
    response     = pyqtSignal(str)

    def stop(self): ...
    def send(self, cmd: str): ...


class TcpBackend(Backend):
    def __init__(self):
        super().__init__()
        self._sock: socket.socket | None = None
        self._proc: subprocess.Popen | None = None
        self._stop_ev = threading.Event()

    def run(self):
        self._start_server()
        self._connect_socket()
        self._read_loop()

    def _start_server(self):
        if not SERVER_EXE.exists():
            self.disconnected.emit(
                f"Could not find {SERVER_EXE.name}. "
                "Build it: cmake --build build --target leafts_uart"
            )
            return
        try:
            self._proc = subprocess.Popen(
                [str(SERVER_EXE)],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            time.sleep(0.5)
        except OSError as e:
            self.disconnected.emit(f"Server start error: {e}")

    def _connect_socket(self):
        for _ in range(12):
            if self._stop_ev.is_set():
                return
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.settimeout(2.0)
                s.connect((TCP_HOST, TCP_PORT))
                s.settimeout(0.1)
                self._sock = s
                self.connected.emit()
                return
            except OSError:
                time.sleep(0.3)
        self.disconnected.emit("Cannot connect to leafts_uart.exe (port 5555).")

    def _read_loop(self):
        buf = b""
        while not self._stop_ev.is_set():
            if self._sock is None:
                time.sleep(0.2)
                continue
            try:
                chunk = self._sock.recv(1024)
                if not chunk:
                    self.disconnected.emit("Server disconnected.")
                    return
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    self.response.emit(line.decode(errors="replace").rstrip())
            except socket.timeout:
                continue
            except OSError as e:
                if not self._stop_ev.is_set():
                    self.disconnected.emit(f"TCP error: {e}")
                return

    def send(self, cmd: str):
        if self._sock:
            try:
                self._sock.sendall((cmd.strip() + "\n").encode())
            except OSError:
                self.disconnected.emit("TCP connection lost.")

    def stop(self):
        self._stop_ev.set()
        if self._sock:
            try: self._sock.close()
            except: pass
        if self._proc:
            self._proc.terminate()
            try: self._proc.wait(timeout=2)
            except: pass


class SerialBackend(Backend):
    def __init__(self, port: str, baud: int):
        super().__init__()
        self._port = port
        self._baud = baud
        self._ser: serial.Serial | None = None
        self._stop_ev = threading.Event()
        import queue
        self._tx_queue: queue.Queue[bytes] = queue.Queue()

    def run(self):
        import queue
        try:
            self._ser = serial.Serial(self._port, self._baud, timeout=0.05)
            time.sleep(0.1)
            self.connected.emit()
        except serial.SerialException as e:
            self.disconnected.emit(f"Cannot open {self._port}: {e}")
            return

        buf = b""
        while not self._stop_ev.is_set():
            try:
                # drain outgoing queue first (non-
                # ocking)
                while True:
                    try:
                        data = self._tx_queue.get_nowait()
                        self._ser.write(data)
                    except queue.Empty:
                        break

                # read incoming
                chunk = self._ser.read(256)
                if chunk:
                    buf += chunk
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        self.response.emit(line.decode(errors="replace").rstrip("\r"))
            except serial.SerialException as e:
                if not self._stop_ev.is_set():
                    self.disconnected.emit(f"UART error: {e}")
                return

    def send(self, cmd: str):
        # called from UI thread — just enqueue, run() does the actual write
        self._tx_queue.put((cmd.strip() + "\r\n").encode())

    def stop(self):
        self._stop_ev.set()
        if self._ser:
            try: self._ser.close()
            except: pass


# ---------------------------------------------------------------------------
# Styles
# ---------------------------------------------------------------------------
DARK_TERM = """
QTextEdit {
    background-color: #0a0f14;
    color: #d8dee9;
    font-family: 'JetBrains Mono', 'Cascadia Code', 'Fira Code', Consolas, monospace;
    font-size: 12px;
    border: 1px solid #1f2a33;
    border-radius: 8px;
    padding: 8px;
    selection-background-color: #2f81f7;
    selection-color: #f0f6fc;
}
"""
SNIPPET_STYLE = """
QTextEdit {
    background-color: #161b22;
    color: #cdd6f4;
    font-family: Consolas, 'Courier New', monospace;
    font-size: 11px;
    border: 1px solid #30363d;
    border-radius: 4px;
    padding: 6px;
}
"""
APP_STYLE = """
QMainWindow, QWidget { background-color: #0d1117; color: #e6edf3; }
QSplitter::handle { background-color: #30363d; width: 2px; }

QGroupBox {
    border: 1px solid #21262d;
    border-radius: 6px;
    margin-top: 10px;
    padding-top: 8px;
    font-weight: bold;
    color: #484f58;
    font-size: 11px;
}
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; }

QLineEdit {
    background-color: #161b22;
    color: #e6edf3;
    border: 1px solid #30363d;
    border-radius: 6px;
    padding: 5px 10px;
    font-size: 13px;
}
QLineEdit:focus { border-color: #58a6ff; }

QComboBox {
    background-color: #161b22;
    color: #e6edf3;
    border: 1px solid #30363d;
    border-radius: 6px;
    padding: 4px 10px;
    font-size: 12px;
    min-height: 24px;
}
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background-color: #161b22;
    color: #e6edf3;
    selection-background-color: #1f6feb;
    border: 1px solid #30363d;
}

QListWidget {
    background-color: #0d1117;
    color: #e6edf3;
    border: 1px solid #21262d;
    border-radius: 6px;
    font-size: 12px;
    outline: none;
}
QListWidget::item { padding: 5px 8px; border-bottom: 1px solid #161b22; }
QListWidget::item:selected { background-color: #1f6feb; color: #fff; }
QListWidget::item:hover { background-color: #161b22; }

QPushButton {
    background-color: #21262d;
    color: #e6edf3;
    border: 1px solid #30363d;
    border-radius: 6px;
    padding: 6px 14px;
    font-size: 12px;
}
QPushButton:hover { background-color: #30363d; border-color: #8b949e; }
QPushButton:pressed { background-color: #0d1117; }
QPushButton:disabled { color: #484f58; border-color: #21262d; }

QPushButton#genBtn {
    background-color: #1a7f37;
    border-color: #2ea043;
    color: #d2f8d6;
    font-size: 13px;
    font-weight: bold;
    padding: 8px 14px;
}
QPushButton#genBtn:hover { background-color: #2ea043; }

QPushButton#sendBtn {
    background-color: #1f6feb;
    border-color: #388bfd;
    color: #e6edf3;
    font-weight: bold;
}
QPushButton#sendBtn:hover { background-color: #388bfd; }

QPushButton#connBtn {
    background-color: #1a7f37;
    border-color: #2ea043;
    color: #d2f8d6;
    font-weight: bold;
    min-width: 90px;
}
QPushButton#connBtn:hover { background-color: #2ea043; }
QPushButton#connBtn[connected="true"] {
    background-color: #6e1a1a;
    border-color: #da3633;
    color: #ffd1cf;
}
QPushButton#connBtn[connected="true"]:hover { background-color: #da3633; }

QPushButton#clearBtn  { color: #484f58; font-size: 11px; padding: 4px 10px; }
QPushButton#refreshBtn { color: #484f58; font-size: 11px; padding: 4px 8px; max-width: 34px; }
QPushButton#copyBtn {
    background-color: #1f6feb;
    border-color: #388bfd;
    color: #e6edf3;
    font-size: 11px;
    padding: 4px 10px;
    font-weight: bold;
}
QPushButton#copyBtn:hover { background-color: #388bfd; }

QTabWidget::pane {
    border: 1px solid #30363d;
    border-top: none;
    background-color: #161b22;
    border-radius: 0 0 6px 6px;
}
QTabBar::tab {
    background-color: #0d1117;
    color: #8b949e;
    border: 1px solid #30363d;
    border-bottom: none;
    border-radius: 6px 6px 0 0;
    padding: 5px 16px;
    font-size: 12px;
    margin-right: 2px;
}
QTabBar::tab:selected { background-color: #161b22; color: #e6edf3; }
QTabBar::tab:hover:!selected { color: #c9d1d9; }

QLabel#statusOk   { color: #3fb950; font-weight: bold; font-size: 12px; }
QLabel#statusErr  { color: #f85149; font-weight: bold; font-size: 12px; }
QLabel#statusWait { color: #d29922; font-weight: bold; font-size: 12px; }
QLabel#boardTitle { color: #58a6ff; font-size: 15px; font-weight: bold; }

QScrollBar:vertical { background: #0d1117; width: 8px; border: none; }
QScrollBar::handle:vertical { background: #30363d; border-radius: 4px; min-height: 20px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: #0d1117; height: 8px; border: none; }
QScrollBar::handle:horizontal { background: #30363d; border-radius: 4px; min-width: 20px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal,
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar::corner { background: #0d1117; }

QFrame#sep { background-color: #21262d; max-height: 1px; min-height: 1px; }

QSpinBox {
    background-color: #161b22;
    color: #e6edf3;
    border: 1px solid #30363d;
    border-radius: 6px;
    padding: 4px 6px;
    font-size: 12px;
}
"""


# ---------------------------------------------------------------------------
# Board panel
# ---------------------------------------------------------------------------
class BoardPanel(QWidget):
    board_selected = pyqtSignal(dict)

    def __init__(self):
        super().__init__()
        self._current_board: dict | None = None
        self.boards: list[dict] = []
        self.filtered: list[dict] = []
        self._build_ui()
        self._load_boards()

    def _build_ui(self):
        lay = QVBoxLayout(self)
        lay.setContentsMargins(12, 12, 8, 12)
        lay.setSpacing(8)

        title = QLabel("LeafTS Studio")
        title.setObjectName("boardTitle")
        lay.addWidget(title)

        sub = QLabel("Select a board · generate a project · connect via UART or TCP")
        sub.setStyleSheet("color: #484f58; font-size: 11px;")
        sub.setWordWrap(True)
        lay.addWidget(sub)

        sep = QFrame(); sep.setObjectName("sep"); sep.setFrameShape(QFrame.Shape.HLine)
        lay.addWidget(sep)

        self.search = QLineEdit()
        self.search.setPlaceholderText("Search board, MCU, vendor...")
        self.search.textChanged.connect(self._filter)
        lay.addWidget(self.search)

        self.family_box = QComboBox()
        self.family_box.addItem("All families")
        self.family_box.currentIndexChanged.connect(self._filter)
        lay.addWidget(self.family_box)

        self.lst = QListWidget()
        self.lst.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.lst.currentItemChanged.connect(self._on_select)
        lay.addWidget(self.lst, stretch=1)

        self.count_lbl = QLabel()
        self.count_lbl.setStyleSheet("color: #484f58; font-size: 10px;")
        lay.addWidget(self.count_lbl)

        # Details
        grp = QGroupBox("Details")
        gl = QVBoxLayout(grp); gl.setSpacing(3)
        self.lbl_name  = QLabel("—")
        self.lbl_mcu   = QLabel("—")
        self.lbl_flash = QLabel("—")
        self.lbl_page  = QLabel("—")
        for lbl in (self.lbl_name, self.lbl_mcu, self.lbl_flash, self.lbl_page):
            lbl.setWordWrap(True)
            lbl.setStyleSheet("color: #8b949e; font-size: 11px;")
            gl.addWidget(lbl)
        lay.addWidget(grp)

        # Snippet
        grp2 = QGroupBox("Init snippet")
        gl2 = QVBoxLayout(grp2); gl2.setSpacing(4)
        self.snippet = QTextEdit()
        self.snippet.setReadOnly(True)
        self.snippet.setStyleSheet(SNIPPET_STYLE)
        self.snippet.setMaximumHeight(120)
        self.snippet.setPlaceholderText("← select a board")
        gl2.addWidget(self.snippet)
        copy_row = QHBoxLayout()
        copy_btn = QPushButton("Copy snippet"); copy_btn.setObjectName("copyBtn")
        copy_btn.clicked.connect(self._copy_snippet)
        copy_row.addWidget(copy_btn); copy_row.addStretch()
        gl2.addLayout(copy_row)
        lay.addWidget(grp2)

        # Generate button
        self.gen_btn = QPushButton("Generate C project...")
        self.gen_btn.setObjectName("genBtn")
        self.gen_btn.setEnabled(False)
        self.gen_btn.setToolTip(
            "Generates main.c and copies HAL + LeafTS files to the selected folder.\n"
            "Then add the files to your build system (Makefile / CMake / STM32CubeIDE)."
        )
        self.gen_btn.clicked.connect(self._generate)
        lay.addWidget(self.gen_btn)

    def _load_boards(self):
        try:
            with open(BOARDS_JSON, encoding="utf-8") as f:
                data = json.load(f)
            self.boards = data.get("boards", [])
        except Exception as e:
            self.count_lbl.setText(f"Error loading boards.json: {e}")
            return
        families = sorted({b["family"] for b in self.boards})
        self.family_box.blockSignals(True)
        for fam in families:
            self.family_box.addItem(fam)
        self.family_box.blockSignals(False)
        self._filter()

    def _filter(self):
        q = self.search.text().strip().lower()
        fam = self.family_box.currentText()
        all_fam = self.family_box.currentIndex() == 0
        self.filtered = [
            b for b in self.boards
            if (all_fam or b["family"] == fam)
            and (not q or q in b["name"].lower()
                       or q in b.get("mcu", "").lower()
                       or q in b["family"].lower()
                       or q in b.get("vendor", "").lower())
        ]
        self.lst.blockSignals(True)
        self.lst.clear()
        for b in self.filtered:
            flash_kb = b.get("flash_size", 0) // 1024
            item = QListWidgetItem(f"{b['name']}  [{flash_kb} KB]")
            item.setData(Qt.ItemDataRole.UserRole, b)
            self.lst.addItem(item)
        self.lst.blockSignals(False)
        self.count_lbl.setText(f"{len(self.filtered)} / {len(self.boards)} boards")

    def _on_select(self, cur: QListWidgetItem | None, _prev):
        if cur is None:
            return
        b: dict = cur.data(Qt.ItemDataRole.UserRole)
        self._current_board = b
        flash_size = b.get("flash_size", 0)
        flash_kb = flash_size // 1024
        self.lbl_name.setText(f"<b>{b['name']}</b>")
        self.lbl_mcu.setText(f"MCU: {b.get('mcu','?').upper()}  ·  {b.get('family','?')}")
        self.lbl_flash.setText(f"Flash: {flash_kb} KB  ·  {flash_size:,} B")
        self.lbl_page.setText(f"Page: {b.get('page_size','?')} B  ·  Sectors: {b.get('sector_count','?')}")
        self.snippet.setPlainText(make_snippet(b))
        self.gen_btn.setEnabled(True)
        self.board_selected.emit(b)

    def _copy_snippet(self):
        t = self.snippet.toPlainText()
        if t:
            QApplication.clipboard().setText(t)

    def _generate(self):
        if not self._current_board:
            return
        b = self._current_board
        dest_str = QFileDialog.getExistingDirectory(
            self, f"Select destination folder — {b['name']}",
            str(Path.home()), QFileDialog.Option.ShowDirsOnly,
        )
        if not dest_str:
            return
        dest = Path(dest_str)
        try:
            copied = generate_project(b, dest)
        except Exception as e:
            QMessageBox.critical(self, "Generation error", str(e))
            return

        ok      = [f for f in copied if not f.startswith("[MISSING]")]
        missing = [f for f in copied if f.startswith("[MISSING]")]
        lines = [
            f"<b>{b['name']}</b>",
            f"Folder: <code>{dest}</code>",
            "",
            f"Copied files ({len(ok)}):",
        ] + [f"&nbsp;&nbsp;✓ {f}" for f in ok]
        if missing:
            lines += ["", "Missing files (HAL not implemented):"]
            lines += [f"&nbsp;&nbsp;✗ {f}" for f in missing]
        dlg = QMessageBox(self)
        dlg.setWindowTitle("Project generated ✓")
        dlg.setTextFormat(Qt.TextFormat.RichText)
        dlg.setText("<br>".join(lines))
        dlg.setIcon(QMessageBox.Icon.Information)
        dlg.exec()


# ---------------------------------------------------------------------------
# Connection bar
# ---------------------------------------------------------------------------
class ConnectionBar(QWidget):
    connect_tcp    = pyqtSignal()
    connect_serial = pyqtSignal(str, int)
    disconnect_req = pyqtSignal()

    def __init__(self):
        super().__init__()
        self._connected = False
        self._build_ui()

    def _build_ui(self):
        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(0)

        self.tabs = QTabWidget()
        self.tabs.setMaximumHeight(78)

        # TCP tab
        tcp_w  = QWidget()
        tcp_l  = QHBoxLayout(tcp_w)
        tcp_l.setContentsMargins(10, 6, 10, 6)
        tcp_l.setSpacing(8)
        tcp_l.addWidget(QLabel("Host:"))
        self.tcp_host = QLineEdit(TCP_HOST)
        self.tcp_host.setMaximumWidth(120)
        tcp_l.addWidget(self.tcp_host)
        tcp_l.addWidget(QLabel("Port:"))
        self.tcp_port = QSpinBox()
        self.tcp_port.setRange(1024, 65535)
        self.tcp_port.setValue(TCP_PORT)
        self.tcp_port.setMaximumWidth(75)
        tcp_l.addWidget(self.tcp_port)
        hint = QLabel("(leafts_uart.exe starts automatically)")
        hint.setStyleSheet("color: #484f58; font-size: 11px;")
        tcp_l.addWidget(hint)
        tcp_l.addStretch()
        self.tcp_btn = QPushButton("Connect")
        self.tcp_btn.setObjectName("connBtn")
        self.tcp_btn.setProperty("connected", "false")
        self.tcp_btn.clicked.connect(self._on_tcp_btn)
        tcp_l.addWidget(self.tcp_btn)
        self.tabs.addTab(tcp_w, "TCP (emulator)")

        # Serial tab
        ser_w  = QWidget()
        ser_l  = QHBoxLayout(ser_w)
        ser_l.setContentsMargins(10, 6, 10, 6)
        ser_l.setSpacing(8)
        ser_l.addWidget(QLabel("COM Port:"))
        self.ser_port = QComboBox()
        self.ser_port.setMinimumWidth(110)
        ser_l.addWidget(self.ser_port)
        ref_btn = QPushButton("↻")
        ref_btn.setObjectName("refreshBtn")
        ref_btn.setToolTip("Refresh COM ports")
        ref_btn.clicked.connect(self._refresh)
        ser_l.addWidget(ref_btn)
        ser_l.addWidget(QLabel("Baud:"))
        self.ser_baud = QComboBox()
        self.ser_baud.setMaximumWidth(90)
        for r in ["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"]:
            self.ser_baud.addItem(r)
        self.ser_baud.setCurrentText("115200")
        ser_l.addWidget(self.ser_baud)
        ser_l.addWidget(QLabel("8N1"))
        ser_l.addStretch()
        self.ser_btn = QPushButton("Connect")
        self.ser_btn.setObjectName("connBtn")
        self.ser_btn.setProperty("connected", "false")
        self.ser_btn.clicked.connect(self._on_ser_btn)
        ser_l.addWidget(self.ser_btn)
        self.tabs.addTab(ser_w, "UART (hardware)")

        lay.addWidget(self.tabs)
        self._refresh()

    def _refresh(self):
        self.ser_port.clear()
        ports = [p.device for p in serial.tools.list_ports.comports()]
        if ports:
            for p in ports:
                self.ser_port.addItem(p)
        else:
            self.ser_port.addItem("(no COM ports)")

    def _on_tcp_btn(self):
        if self._connected:
            self.disconnect_req.emit()
        else:
            self.connect_tcp.emit()

    def _on_ser_btn(self):
        if self._connected:
            self.disconnect_req.emit()
        else:
            port = self.ser_port.currentText()
            if port.startswith("("):
                return
            self.connect_serial.emit(port, int(self.ser_baud.currentText()))

    def set_connected(self, ok: bool):
        self._connected = ok
        val = "true" if ok else "false"
        lbl_on  = "Disconnect"
        lbl_off = "Connect"
        for btn in (self.tcp_btn, self.ser_btn):
            btn.setProperty("connected", val)
            btn.setText(lbl_on if ok else lbl_off)
            btn.style().unpolish(btn)
            btn.style().polish(btn)

    def current_mode(self) -> str:
        return "tcp" if self.tabs.currentIndex() == 0 else "serial"


# ---------------------------------------------------------------------------
# Terminal panel
# ---------------------------------------------------------------------------
class TerminalPanel(QWidget):
    command_entered = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self._history: list[str] = []
        self._hist_idx = -1
        self._prompt_user = "user"
        self._prompt_host = "leafts"
        self._build_ui()

    @staticmethod
    def _clock() -> str:
        return datetime.now().strftime("%H:%M:%S")

    def _build_ui(self):
        lay = QVBoxLayout(self)
        lay.setContentsMargins(8, 10, 10, 10)
        lay.setSpacing(6)

        # Header
        hdr = QHBoxLayout()
        t = QLabel("Terminal  /  Neo Shell")
        t.setStyleSheet("color: #7aa2f7; font-size: 14px; font-weight: bold;")
        hdr.addWidget(t); hdr.addStretch()
        self.status_lbl = QLabel("Disconnected")
        self.status_lbl.setObjectName("statusErr")
        hdr.addWidget(self.status_lbl)
        lay.addLayout(hdr)

        self.board_lbl = QLabel("← select a board from the list on the left")
        self.board_lbl.setStyleSheet("color: #484f58; font-size: 11px; font-style: italic;")
        lay.addWidget(self.board_lbl)

        # Connection bar (embedded)
        self.conn_bar = ConnectionBar()
        lay.addWidget(self.conn_bar)

        sep = QFrame(); sep.setObjectName("sep"); sep.setFrameShape(QFrame.Shape.HLine)
        lay.addWidget(sep)

        # Output
        self.output = QTextEdit()
        self.output.setReadOnly(True)
        self.output.setStyleSheet(DARK_TERM)
        self.output.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        lay.addWidget(self.output, stretch=1)
        self._render_banner()

        # Input row
        inp = QHBoxLayout(); inp.setSpacing(6)
        self.prompt_lbl = QLabel(f"{self._prompt_user}@{self._prompt_host}")
        self.prompt_lbl.setStyleSheet(
            "color: #7ee787; "
            "font-family: 'JetBrains Mono', 'Cascadia Code', Consolas, monospace; "
            "font-size: 13px; font-weight: bold;")
        inp.addWidget(self.prompt_lbl)
        self.input = QLineEdit()
        self.input.setPlaceholderText("run a query... (Enter = send, Up/Down = history)")
        self.input.setFont(QFont("Consolas", 13))
        self.input.returnPressed.connect(self._send)
        self.input.installEventFilter(self)
        inp.addWidget(self.input)
        send_btn = QPushButton("Send"); send_btn.setObjectName("sendBtn")
        send_btn.clicked.connect(self._send)
        inp.addWidget(send_btn)
        lay.addLayout(inp)

        hint = QLabel("Write help to get a list of available commands. This terminal is not a full shell, but it supports basic line editing and command history navigation with Up/Down arrows.")
        hint.setStyleSheet("color: #484f58; font-size: 10px;")
        hint.setWordWrap(True)
        lay.addWidget(hint)

    def _render_banner(self):
        art = [
            "                @@@@@@@@@@@@@@@@@@@",
            "            @@@                   @",
            "          @@                    @@@            _                 __ _____ ____  ",
            "         @                      @             | |    ___   __ _ / _|_   _/ ___| ",
            "       @@                    @@@@             | |   / _ \\ /_ ` | |_  | | \\___ \\ ",
            "       @             @       @                | |__|  __/|(_|  |  _| | |  ___) | ",
            "      @@          @@@        @                |_____\\___| \\__,_|_|   |_| |____/ ",
            "      @         @@        @@@@",
            "      @       @@          @                    ULTRA-MINIMAL EMBEDDED DATABASE",
            "      @@     @@           @",              
            "       @   @@            @@",
            "          @@            @@",
            "         @            @@@",              
            "        @  @@@@@@@@@@@",
            "       @",
            "      @",
            "     @@",
            "",
        ]
        self.output.setTextColor(QColor("#3fb950"))
        for line in art:
            self.output.append(line)
        self.output.setTextColor(QColor("#9ece6a"))
        self.output.append(f"[{self._clock()}] Welcome to LeafTS Studio shell")
        self.output.append("Write help to get a list of available commands.")
        self.output.setTextColor(QColor("#d8dee9"))
        self.output.moveCursor(QTextCursor.MoveOperation.End)

    def eventFilter(self, obj, ev):
        if obj is self.input and ev.type() == QEvent.Type.KeyPress:
            k = ev.key()
            if k == Qt.Key.Key_Up:   self._nav(-1); return True
            if k == Qt.Key.Key_Down: self._nav(1);  return True
        return super().eventFilter(obj, ev)

    def _nav(self, d: int):
        if not self._history: return
        self._hist_idx = max(0, min(len(self._history) - 1, self._hist_idx + d))
        self.input.setText(self._history[self._hist_idx])

    def _send(self):
        cmd = self.input.text().strip()
        if not cmd: return
        if not self._history or self._history[-1] != cmd:
            self._history.append(cmd)
        self._hist_idx = len(self._history)

        if cmd.lower() == "clear":
            self.output.clear()
            self.input.clear()
            return

        self.output.setTextColor(QColor("#7ee787"))
        self.output.append(f"[{self._clock()}] {self.prompt_lbl.text()} {cmd}")
        self.output.setTextColor(QColor("#d8dee9"))
        self.input.clear()
        self.command_entered.emit(cmd)

    def append_response(self, line: str):
        line = line.strip()
        if not line:
            return

        # Suppress noisy acknowledgements with no useful content.
        if line == "OK":
            return

        shown = line
        color = "#7dcfff"

        if line.startswith("OK "):
            payload = line[3:].strip()
            # Hide list headers like "OK 12" and show only actual data lines.
            if payload.isdigit():
                return
            shown = payload
            color = "#9ece6a"
        elif line.startswith("ERR "):
            shown = f"Error: {line[4:].strip()}"
            color = "#f7768e"

        self.output.setTextColor(QColor(color))
        self.output.append(f"[{self._clock()}] {shown}")
        self.output.setTextColor(QColor("#d8dee9"))
        self.output.moveCursor(QTextCursor.MoveOperation.End)

    def append_system(self, msg: str, color: str = "#d29922"):
        self.output.setTextColor(QColor(color))
        self.output.append(f"[{self._clock()}] [system] {msg}")
        self.output.setTextColor(QColor("#d8dee9"))
        self.output.moveCursor(QTextCursor.MoveOperation.End)

    def set_status(self, state: str):
        m = {"ok": ("statusOk", "Connected"),
             "err": ("statusErr", "Disconnected"),
             "wait": ("statusWait", "Connecting...")}
        obj, txt = m.get(state, ("statusErr", "Disconnected"))
        self.status_lbl.setObjectName(obj)
        self.status_lbl.setText(txt)
        self.status_lbl.style().unpolish(self.status_lbl)
        self.status_lbl.style().polish(self.status_lbl)

    def set_board(self, b: dict):
        self.board_lbl.setText(
            f"{b['name']}  ·  {b.get('mcu','').upper()}  "
            f"·  {b['flash_size'] // 1024} KB  ·  {b['family']}"
        )
        self.board_lbl.setStyleSheet("color: #8b949e; font-size: 11px;")

    def focus_input(self): self.input.setFocus()


# ---------------------------------------------------------------------------
# Main window
# ---------------------------------------------------------------------------
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("LeafTS Studio")
        self.resize(1220, 780)
        self._backend: Backend | None = None

        spl = QSplitter(Qt.Orientation.Horizontal)
        spl.setHandleWidth(2)
        self.board_panel    = BoardPanel()
        self.terminal_panel = TerminalPanel()
        spl.addWidget(self.board_panel)
        spl.addWidget(self.terminal_panel)
        spl.setSizes([390, 830])
        spl.setStretchFactor(1, 1)
        self.setCentralWidget(spl)

        sb = QStatusBar()
        sb.setStyleSheet("QStatusBar { color: #30363d; font-size: 10px; }")
        self.setStatusBar(sb)
        sb.showMessage(f"boards.json: {BOARDS_JSON}  ·  server: {SERVER_EXE}")

        self.board_panel.board_selected.connect(self.terminal_panel.set_board)

        cb = self.terminal_panel.conn_bar
        cb.connect_tcp.connect(self._start_tcp)
        cb.connect_serial.connect(self._start_serial)
        cb.disconnect_req.connect(self._disconnect)

        self.terminal_panel.command_entered.connect(self._send)

        QShortcut(QKeySequence("Ctrl+L"), self).activated.connect(
            self.terminal_panel.output.clear)

        self.terminal_panel.append_system(
            "Select a connection mode above and click ▶ Connect.", "#484f58")
        self.terminal_panel.focus_input()

    def _start_tcp(self):
        self._stop_backend()
        self.terminal_panel.set_status("wait")
        self.terminal_panel.append_system("Starting leafts_uart.exe...", "#d29922")
        self._backend = TcpBackend()
        self._wire()
        self._backend.start()

    def _start_serial(self, port: str, baud: int):
        self._stop_backend()
        self.terminal_panel.set_status("wait")
        self.terminal_panel.append_system(f"Connecting to {port}  @  {baud}...", "#d29922")
        self._backend = SerialBackend(port, baud)
        self._wire()
        self._backend.start()

    def _wire(self):
        self._backend.connected.connect(self._on_ok)
        self._backend.disconnected.connect(self._on_err)
        self._backend.response.connect(self.terminal_panel.append_response)

    def _disconnect(self):
        self._stop_backend()
        self.terminal_panel.set_status("err")
        self.terminal_panel.conn_bar.set_connected(False)
        self.terminal_panel.append_system("Disconnected.", "#f85149")

    def _stop_backend(self):
        if self._backend:
            self._backend.stop()
            self._backend.wait(2000)
            self._backend = None

    def _on_ok(self):
        self.terminal_panel.set_status("ok")
        self.terminal_panel.conn_bar.set_connected(True)
        mode = self.terminal_panel.conn_bar.current_mode()
        self.terminal_panel.append_system(
            "Connected to leafts_uart.exe  (RAM flash)" if mode == "tcp"
            else "Connected via UART.", "#3fb950")
        self.terminal_panel.append_system("Type 'status' to check the database.", "#56d364")

    def _on_err(self, reason: str):
        self.terminal_panel.set_status("err")
        self.terminal_panel.conn_bar.set_connected(False)
        self.terminal_panel.append_system(f"Disconnected: {reason}", "#f85149")

    def _send(self, cmd: str):
        if self._backend:
            self._backend.send(_normalize_wire_command(cmd))
        else:
            self.terminal_panel.append_system("Not connected — click ▶ Connect.", "#d29922")
            self.terminal_panel.append_response("ERR not_connected")

    def closeEvent(self, ev):
        self._stop_backend()
        super().closeEvent(ev)


# ---------------------------------------------------------------------------
def main():
    try:
        app = QApplication(sys.argv)
        app.setStyleSheet(APP_STYLE)
        app.setApplicationName("LeafTS Studio")
        window = MainWindow()
        window.show()
        sys.exit(app.exec())
    except Exception as exc:
        import traceback
        # Show error in a message box so it doesn't silently disappear
        try:
            from PyQt6.QtWidgets import QMessageBox
            _app = QApplication.instance() or QApplication(sys.argv)
            QMessageBox.critical(None, "LeafTS — błąd krytyczny",
                                 f"{type(exc).__name__}: {exc}\n\n"
                                 + traceback.format_exc())
        except Exception:
            # Last resort: write to file next to exe / script
            crash_path = Path(sys.executable).parent / "leafts_crash.txt"
            try:
                crash_path.write_text(traceback.format_exc(), encoding="utf-8")
            except Exception:
                pass
        sys.exit(1)

if __name__ == "__main__":
    main()
