"""
LeafTS Python Client
Connects to leafts_uart.exe via TCP socket (virtual UART).
On real hardware: swap socket for pyserial - same protocol, same commands.
"""

import socket
import sys
import subprocess
import time
import argparse
from pathlib import Path

# SERVER CONNECTION DEFAULTS - OVERRIDE WITH --host / --port ARGUMENTS
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 5555

# PATH TO SERVER EXECUTABLE - RELATIVE TO THIS SCRIPT'S LOCATION
SCRIPT_DIR  = Path(__file__).parent
SERVER_EXE  = SCRIPT_DIR / ".." / "build" / "leafts_uart.exe"

HELP_TEXT = """
Commands:
    insert <value>               insert with auto timestamp (RTC/system)
    insert <value> <timestamp>   insert with manual ts      (alias: append)
    select                       get the most recent record (alias: latest)
    select *                     list all records           (alias: list)
    select count(*)              count records              (alias: count)
    select min(value)            record with min value      (alias: get_min)
    select max(value)            record with max value      (alias: get_max)
    select avg(value)            average value              (alias: get_avg)
    select * limit <n>           latest N records           (alias: get_last <n>)
    select * where timestamp between <from> <to>           (alias: get_range <from> <to>)
    delete from leafts           erase all records          (alias: erase)
    truncate table leafts        erase all records          (alias: erase)
  get_last <n>                 get last N records          (e.g. get_last 5)
  get_range <from> <to>        get records in ts range     (e.g. get_range 1000 1120)
  get_min                      get record with lowest value
  get_max                      get record with highest value
  status                       show record count and capacity
  erase                        erase all records from flash
  help                         show this message
  exit                         disconnect and quit
"""


def start_server() -> subprocess.Popen:
    """Start leafts_uart.exe as a background subprocess."""
    exe = SERVER_EXE.resolve()

    if not exe.exists():
        print(f"[ERROR] Server not found: {exe}")
        print("        Run: cmake --build build --target leafts_uart")
        sys.exit(1)

    # START SERVER - STDOUT GOES TO OUR TERMINAL SO USER SEES [leafts] MESSAGES
    proc = subprocess.Popen([str(exe)])

    # GIVE SERVER TIME TO OPEN THE SOCKET BEFORE WE TRY TO CONNECT
    time.sleep(0.5)

    return proc



def connect(host: str, port: int) -> socket.socket:
    """Create TCP connection to leafts_uart server."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    sock.connect((host, port))
    return sock


def send_command(sock: socket.socket, command: str) -> list[str]:
    """
    Send one command line and collect the response.
    For 'list': reads count from first line then reads that many extra lines.
    Returns list of response lines (stripped).
    """
    sock.sendall((command.strip() + "\n").encode())

    lines = []

    # READ FIRST RESPONSE LINE
    first_line = read_line(sock)
    lines.append(first_line)

    # LIST-LIKE COMMANDS RETURN "OK <count>" THEN <count> EXTRA LINES
    normalized = command.strip().lower()
    cmd_name = normalized.split()[0] if normalized else ""
    is_list_like = (
        cmd_name in ("list", "get_last", "get_range")
        or normalized in ("select *", "select all")
    )
    if first_line.startswith("OK") and is_list_like:
        parts = first_line.split()
        if len(parts) == 2:
            count = int(parts[1])
            for _ in range(count):
                lines.append(read_line(sock))

    return lines


def read_line(sock: socket.socket) -> str:
    """Read bytes from socket until newline, return stripped string."""
    buf = b""
    while True:
        ch = sock.recv(1)
        if not ch or ch == b"\n":
            break
        if ch != b"\r":
            buf += ch
    return buf.decode().strip()


def print_response(command: str, lines: list[str]) -> None:
    """Format and print server response in a readable way."""

    if not lines:
        return

    first = lines[0]

    # ERROR RESPONSE
    if first.startswith("ERR"):
        print(f"  [ERROR] {first}")
        return

    normalized = command.strip().lower()
    cmd = normalized.split()[0] if normalized else ""

    # LATEST / GET_MIN / GET_MAX - PARSE timestamp + value
    if normalized in ("latest", "select", "select latest") or cmd in ("get_min", "get_max"):
        parts = first.split()
        if len(parts) == 3:
            print(f"  timestamp : {parts[1]}")
            print(f"  value     : {parts[2]}")
        return

    # LIST / GET_LAST / GET_RANGE - PRINT TABLE
    if cmd in ("list", "get_last", "get_range") or normalized in ("select *", "select all"):
        parts = first.split()
        count = int(parts[1]) if len(parts) == 2 else 0
        if count == 0:
            print("  (no records)")
            return
        print(f"  {'index':<8} {'timestamp':<14} {'value'}")
        print(f"  {'-'*8} {'-'*14} {'-'*10}")
        for i, record_line in enumerate(lines[1:]):
            rparts = record_line.split()
            if len(rparts) == 2:
                print(f"  {i:<8} {rparts[0]:<14} {rparts[1]}")
        return

    # STATUS - PARSE count= capacity=
    if cmd == "status":
        parts = first.replace("OK", "").split()
        for part in parts:
            key, _, val = part.partition("=")
            print(f"  {key:<12}: {val}")
        return

    # DEFAULT - JUST PRINT OK
    if first == "OK":
        print("  OK")
    else:
        for line in lines:
            print(f"  {line}")



def main() -> None:
    # PARSE COMMAND LINE ARGUMENTS
    parser = argparse.ArgumentParser(description="LeafTS interactive client")
    parser.add_argument("--host", default=DEFAULT_HOST, help=f"Server host (default: {DEFAULT_HOST})")
    parser.add_argument("--port", default=DEFAULT_PORT, type=int, help=f"Server port (default: {DEFAULT_PORT})")
    args = parser.parse_args()

    # AUTO-START SERVER - NO NEED TO OPEN A SECOND TERMINAL
    server_proc = start_server()

    print(f"LeafTS client - connecting to {args.host}:{args.port}...")

    try:
        sock = connect(args.host, args.port)
    except (ConnectionRefusedError, TimeoutError):
        print(f"[ERROR] Cannot connect to {args.host}:{args.port}")
        print("        Make sure leafts_uart.exe is running first.")
        sys.exit(1)

    print("Connected. Type 'help' for available commands.\n")

    while True:
        try:
            raw = input("leafts> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nDisconnecting...")
            break

        if not raw:
            continue

        if raw in ("exit", "quit"):
            print("Disconnecting...")
            break

        if raw == "help":
            print(HELP_TEXT)
            continue

        try:
            response_lines = send_command(sock, raw)
            print_response(raw, response_lines)
        except (OSError, TimeoutError) as e:
            print(f"[ERROR] Connection lost: {e}")
            break

    sock.close()

    # KILL SERVER PROCESS WHEN CLIENT EXITS
    server_proc.terminate()
    server_proc.wait()


if __name__ == "__main__":
    main()
