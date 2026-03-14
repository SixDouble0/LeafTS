import socket
import sys
import threading
import time

try:
    import serial
except ImportError:
    print("ERROR: pyserial not installed.  Run: pip install pyserial")
    sys.exit(1)

TCP_HOST = "127.0.0.1"
TCP_PORT = 5555

COM_PORT = sys.argv[1] if len(sys.argv) > 1 else "COM6"
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

def connect_tcp() -> socket.socket:
    print(f"[bridge] Looking for Renode on {TCP_HOST}:{TCP_PORT}...")
    for attempt in range(15):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(2.0)
            s.connect((TCP_HOST, TCP_PORT))
            s.settimeout(0.1)
            print(f"[bridge] Connected to Renode via TCP!")
            return s
        except OSError:
            time.sleep(1)
    print("ERROR: Renode not found.")
    sys.exit(1)


def tcp_to_serial(sock: socket.socket, ser: serial.Serial, stop: threading.Event):
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
    print(f"[bridge] Opening serial port {COM_PORT} @ {BAUD} baud ...")
    try:
        ser = serial.Serial(COM_PORT, BAUD, timeout=0.05)
    except serial.SerialException as e:
        print(f"ERROR: Cannot open {COM_PORT}: {e}")
        print("Make sure com0com is enabled and has paired ports like COM5 - COM6")
        sys.exit(1)

    sock = connect_tcp()

    stop = threading.Event()
    t1 = threading.Thread(target=tcp_to_serial, args=(sock, ser, stop), daemon=True)
    t2 = threading.Thread(target=serial_to_tcp, args=(ser, sock, stop), daemon=True)
    t1.start()
    t2.start()

    print(f"\n[bridge] Ready! Bridging Renode <---> {COM_PORT}.")
    print(f"         In your GUI, select UART connection ---> {COM_PORT.replace('6','5')} ---> Connect\n")
    try:
        while not stop.is_set():
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\n[bridge] Stopping...")
    finally:
        stop.set()
        ser.close()
        sock.close()
        print("[bridge] Exited.")
if __name__ == "__main__":
    main()