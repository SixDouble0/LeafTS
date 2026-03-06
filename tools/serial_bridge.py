"""
serial_bridge.py  —  Virtual UART test helper
==============================================
Sits on one end of a com0com virtual pair (default: COM6) and bridges it to
leafts_uart.exe via TCP:5555.

Usage:
    python tools/serial_bridge.py          # COM6 @ 115200 (defaults)
    python tools/serial_bridge.py COM6 115200

Then in the GUI:  UART tab → COM5 → Connect
"""

import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

# ── Config ─────────────────────────────────────────────────────────────────
ROOT       = Path(__file__).parent.parent
SERVER_EXE = ROOT / "build" / "leafts_uart.exe"
TCP_HOST   = "127.0.0.1"
TCP_PORT   = 5555

COM_PORT = sys.argv[1] if len(sys.argv) > 1 else "COM6"
BAUD     = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
# ───────────────────────────────────────────────────────────────────────────

try:
    import serial
except ImportError:
    print("ERROR: pyserial not installed.  Run: pip install pyserial")
    sys.exit(1)


def start_server() -> subprocess.Popen:
    if not SERVER_EXE.exists():
        print(f"ERROR: {SERVER_EXE} not found. Build the project first.")
        sys.exit(1)
    print(f"[bridge] Starting {SERVER_EXE.name} ...")
    proc = subprocess.Popen(
        [str(SERVER_EXE)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    time.sleep(0.6)
    return proc


def connect_tcp() -> socket.socket:
    for attempt in range(15):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(2.0)
            s.connect((TCP_HOST, TCP_PORT))
            s.settimeout(0.1)
            print(f"[bridge] TCP connected to leafts_uart.exe on port {TCP_PORT}")
            return s
        except OSError:
            time.sleep(0.3)
    print("ERROR: Cannot connect to leafts_uart.exe.")
    sys.exit(1)


def tcp_to_serial(sock: socket.socket, ser: serial.Serial, stop: threading.Event):
    """Forward TCP → COM port."""
    while not stop.is_set():
        try:
            data = sock.recv(256)
            if not data:
                break
            ser.write(data)
        except socket.timeout:
            continue
        except OSError:
            break
    stop.set()


def serial_to_tcp(ser: serial.Serial, sock: socket.socket, stop: threading.Event):
    """Forward COM port → TCP, normalising \r\n → \n."""
    buf = b""
    while not stop.is_set():
        try:
            data = ser.read(256)
            if data:
                buf += data
                # emit complete lines with \r stripped
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    sock.sendall(line.rstrip(b"\r") + b"\n")
        except serial.SerialException:
            break
        except OSError:
            break
    stop.set()


def main():
    proc = start_server()

    print(f"[bridge] Opening {COM_PORT} @ {BAUD} baud ...")
    try:
        ser = serial.Serial(COM_PORT, BAUD, timeout=0.05)
    except serial.SerialException as e:
        print(f"ERROR: Cannot open {COM_PORT}: {e}")
        proc.terminate()
        sys.exit(1)
    print(f"[bridge] {COM_PORT} open.")

    sock = connect_tcp()

    stop = threading.Event()
    t1 = threading.Thread(target=tcp_to_serial, args=(sock, ser, stop), daemon=True)
    t2 = threading.Thread(target=serial_to_tcp, args=(ser, sock, stop), daemon=True)
    t1.start()
    t2.start()

    print(f"\n[bridge] Ready.")
    print(f"         In the GUI: UART tab → {COM_PORT.replace('6','5')} → Connect\n")
    print("         Press Ctrl+C to stop.\n")

    try:
        while not stop.is_set():
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\n[bridge] Stopping...")
    finally:
        stop.set()
        ser.close()
        sock.close()
        proc.terminate()
        print("[bridge] Done.")


if __name__ == "__main__":
    main()
