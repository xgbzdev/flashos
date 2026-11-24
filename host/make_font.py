#!/usr/bin/env python3
"""Stand-in 8x16 font for the host simulator only (the real OS uses the
PC's own BIOS font). Usage: make_font.py OUT.bin"""
import sys
from PIL import Image, ImageDraw, ImageFont

font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 13)
out = bytearray(4096)
for ch in range(32, 127):
    img = Image.new("L", (8, 16), 0)
    ImageDraw.Draw(img).text((0, 0), chr(ch), fill=255, font=font)
    for y in range(16):
        bits = 0
        for x in range(8):
            if img.getpixel((x, y)) > 100:
                bits |= 0x80 >> x
        out[ch * 16 + y] = bits
open(sys.argv[1], "wb").write(out)
