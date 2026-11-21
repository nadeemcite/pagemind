"""Append the reader's serial output to a file; reconnect when the port disappears.

The reader restarts itself after an AI flow that turned Wi-Fi on, which drops the USB port.
    .venv/bin/python serial_log.py /dev/cu.usbmodem101 out.txt 1500
"""
import sys
import time

import serial

port, out, secs = sys.argv[1], sys.argv[2], float(sys.argv[3])
end = time.time() + secs
with open(out, "ab", buffering=0) as f:
    while time.time() < end:
        try:
            with serial.Serial(port, 115200, timeout=0.5) as s:
                f.write(b"\n=== connected %s ===\n" % time.strftime("%H:%M:%S").encode())
                while time.time() < end:
                    data = s.read(4096)
                    if data:
                        f.write(data)
        except (serial.SerialException, OSError):
            time.sleep(0.5)
