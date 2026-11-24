#!/usr/bin/env python3
"""Boot FlashOS in QEMU headless, type commands and take screenshots.
Copyright (c) 2026 FlashOS. All rights reserved.

Usage: qemu_test.py ISO OUTDIR [cd|usb]
"""
import os
import socket
import subprocess
import sys
import time

iso, outdir = sys.argv[1], sys.argv[2]
mode = sys.argv[3] if len(sys.argv) > 3 else "cd"
os.makedirs(outdir, exist_ok=True)
sock_path = f"/tmp/flashos-qmp-{mode}.sock"
if os.path.exists(sock_path):
    os.unlink(sock_path)

media = ["-cdrom", iso, "-boot", "d"] if mode == "cd" else ["-drive", f"file={iso},format=raw,if=ide"]
qemu = subprocess.Popen(["qemu-system-i386", "-m", "256", "-vga", "std", "-display", "none",
                         "-monitor", f"unix:{sock_path},server,nowait"] + media)
for _ in range(100):
    if os.path.exists(sock_path):
        break
    time.sleep(0.1)
mon = socket.socket(socket.AF_UNIX)
mon.connect(sock_path)
mon.settimeout(1)


def cmd(c):
    mon.sendall((c + "\n").encode())
    time.sleep(0.15)
    try:
        mon.recv(65536)
    except socket.timeout:
        pass


def shot(name):
    path = os.path.abspath(os.path.join(outdir, f"{mode}_{name}.ppm"))
    cmd(f"screendump {path}")
    time.sleep(0.5)
    try:
        from PIL import Image
        Image.open(path).save(path[:-4] + ".png")
        os.unlink(path)
    except Exception as e:  # keep the .ppm if Pillow is missing
        print("convert failed:", e)
    print("screenshot", name)


KEYS = {" ": "spc", "\n": "ret", ".": "dot", "-": "minus"}


def type_text(t):
    for ch in t:
        cmd("sendkey " + KEYS.get(ch, ch))


time.sleep(1.2)
shot("01_booting")
time.sleep(6)
shot("02_ready")
type_text("ls\n")
time.sleep(1.5)
shot("03_ls")
type_text("readme.txt\n")
time.sleep(1.5)
shot("04_denied")
type_text("run flashlogo\n")
time.sleep(5)
shot("05_flashlogo")
cmd("sendkey esc")
time.sleep(1.5)
type_text("bounce.swf\n")
time.sleep(4)
shot("06_bounce")
cmd("sendkey esc")
time.sleep(1.5)
type_text("about\n")
time.sleep(1.5)
shot("07_about")
cmd("quit")
qemu.wait(timeout=10)
