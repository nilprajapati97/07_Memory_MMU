#!/usr/bin/env python3
"""
pty_loopback.py — creates a pty slave, prints its path, then echoes
every received byte back (UART loopback simulation).
Usage: python3 pty_loopback.py
"""
import os, sys, termios, tty, select, signal

master_fd, slave_fd = os.openpty()
slave_name = os.ttyname(slave_fd)

# Raw mode on master
attrs = termios.tcgetattr(master_fd)
tty.setraw(master_fd)

print(slave_name, flush=True)   # first line = pty path for the test binary

def _exit(sig, frame):
    os.close(master_fd)
    os.close(slave_fd)
    sys.exit(0)

signal.signal(signal.SIGTERM, _exit)
signal.signal(signal.SIGINT,  _exit)

while True:
    r, _, _ = select.select([master_fd], [], [], 0.1)
    if r:
        try:
            data = os.read(master_fd, 256)
            os.write(master_fd, data)   # echo back = loopback
        except OSError:
            break
