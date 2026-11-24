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

extra = sys.argv[4:]   # e.g. hdd.img stick.img
if mode == "cd":
    media = ["-cdrom", iso, "-boot", "d"]
elif mode == "usb":
    media = ["-drive", f"file={iso},format=raw,if=ide"]
elif mode == "install":   # CD + blank IDE disk + FAT USB stick
    media = ["-drive", f"file={extra[0]},format=raw,if=ide,index=0", "-cdrom", iso, "-boot", "d",
             "-device", "qemu-xhci,id=xhci", "-drive", f"if=none,id=stick,format=raw,file={extra[1]}",
             "-device", "usb-storage,bus=xhci.0,drive=stick"]
elif mode == "gamecd":   # boot CD + a second, data-only ISO with games
    media = ["-drive", f"file={iso},media=cdrom,if=ide,index=2", "-drive", f"file={extra[0]},media=cdrom,if=ide,index=3", "-boot", "d"]
elif mode in ("net", "net8139"):
    model = "e1000" if mode == "net" else "rtl8139"
    media = ["-cdrom", iso, "-boot", "d", "-nic", f"user,model={model}"]
else:                     # "hdd": boot the installed hard disk only
    media = ["-drive", f"file={extra[0]},format=raw,if=ide,index=0", "-boot", "c"]
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


KEYS = {" ": "spc", "\n": "ret", ".": "dot", "-": "minus", ":": "shift-semicolon", "/": "slash"}


def type_text(t):
    for ch in t:
        cmd("sendkey " + KEYS.get(ch, ch))


if mode == "gamecd":
    time.sleep(10)
    type_text("ls\n")
    time.sleep(1.5)
    shot("01_ls")
    type_text("cd game.swf\n")
    time.sleep(4)
    shot("02_play")
    cmd("quit")
    qemu.wait(timeout=10)
    sys.exit(0)

if mode in ("net", "net8139"):
    time.sleep(9)
    type_text("net\n")
    time.sleep(8)
    shot("01_net")
    type_text("cls\n")
    time.sleep(1)
    type_text("ping 10.0.2.2\n")
    time.sleep(6)
    type_text("get http://10.0.2.2:8000/netlogo.swf\n")
    time.sleep(6)
    type_text("ls\n")
    time.sleep(1.5)
    shot("02_get")
    if mode == "net":
        type_text("cls\n")
        time.sleep(1)
        type_text("get http://example.com/\n")
        time.sleep(10)
        shot("03_web")
        type_text("netlogo\n")
        time.sleep(4)
        shot("04_play")
    cmd("quit")
    qemu.wait(timeout=10)
    sys.exit(0)

if mode in ("install", "hdd"):
    time.sleep(9)
    shot("01_ready")
    type_text("ls\n")
    time.sleep(1.5)
    shot("02_ls")
    type_text("usbdiag\n")
    time.sleep(1.5)
    shot("02b_mem")
    if mode == "hdd":
        type_text("cls\n")
        time.sleep(1)
        type_text("help\n")
        time.sleep(1.5)
        type_text("install\n")
        time.sleep(1.5)
        shot("03_no_install")
    if mode == "install":
        type_text("usb catch.swf\n")
        time.sleep(4)
        shot("03_usb_game")
        cmd("sendkey f12")
        time.sleep(1.5)
        type_text("install\n")
        time.sleep(1.5)
        shot("04_install_prompt")
        type_text("yes\n")
        time.sleep(8)
        shot("05_installed")
    cmd("quit")
    qemu.wait(timeout=10)
    sys.exit(0)

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
type_text("mem\n")
time.sleep(1.5)
shot("08_mem")
cmd("sendkey up")
time.sleep(0.5)
cmd("sendkey ret")
time.sleep(1.5)
shot("09_history")
type_text("catch\n")
time.sleep(4)
shot("10_catch")
cmd("sendkey right 900")
time.sleep(1.5)
for _ in range(10):
    cmd("mouse_move -39 -25")
time.sleep(1)
shot("11_hover")
cmd("mouse_button 1")
time.sleep(0.4)
cmd("mouse_button 0")
time.sleep(2)
shot("12_after_click")
cmd("sendkey f12")
time.sleep(1.5)
shot("13_back")
cmd("quit")
qemu.wait(timeout=10)
