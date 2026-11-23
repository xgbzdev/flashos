#!/usr/bin/env python3
"""Generate a small demo SWF for FlashOS: red squares bouncing on black.

Pure-Python SWF writer (uncompressed FWS, version 10, no ActionScript).
Usage: make_demo_swf.py OUTPUT.swf
"""
import struct
import sys

TW = 20  # twips per pixel
W, H, FPS, FRAMES = 550, 400, 30, 120


class Bits:
    def __init__(self):
        self.bits = []

    def u(self, v, n):
        for i in range(n - 1, -1, -1):
            self.bits.append((v >> i) & 1)

    def s(self, v, n):
        self.u(v & ((1 << n) - 1), n)

    def bytes(self):
        b = self.bits + [0] * (-len(self.bits) % 8)
        return bytes(int("".join(map(str, b[i:i + 8])), 2) for i in range(0, len(b), 8))


def sbits(*vals):
    return max(max((v.bit_length() if v >= 0 else (~v).bit_length()) + 1 for v in vals), 1)


def rect(x0, x1, y0, y1):
    n = sbits(x0, x1, y0, y1)
    b = Bits()
    b.u(n, 5)
    for v in (x0, x1, y0, y1):
        b.s(v, n)
    return b.bytes()


def tag(code, body):
    if len(body) < 0x3F:
        return struct.pack("<H", (code << 6) | len(body)) + body
    return struct.pack("<HI", (code << 6) | 0x3F, len(body)) + body


def define_square(shape_id, size_px, rgb):
    s = size_px * TW
    body = struct.pack("<H", shape_id) + rect(0, s, 0, s)
    body += bytes([1, 0x00]) + bytes(rgb)  # 1 solid fill style (RGB)
    body += bytes([0])                      # 0 line styles
    b = Bits()
    b.u(1, 4)  # NumFillBits
    b.u(0, 4)  # NumLineBits
    # StyleChangeRecord: moveTo(0,0), fillStyle1 = 1
    b.u(0, 1); b.u(0, 1); b.u(0, 1); b.u(1, 1); b.u(0, 1); b.u(1, 1)
    b.u(1, 5); b.s(0, 1); b.s(0, 1)
    b.u(1, 1)
    # four straight edges (general lines)
    for dx, dy in ((s, 0), (0, s), (-s, 0), (0, -s)):
        n = max(sbits(dx, dy), 2)
        b.u(1, 1); b.u(1, 1); b.u(n - 2, 4); b.u(1, 1)
        b.s(dx, n); b.s(dy, n)
    b.u(0, 6)  # EndShapeRecord
    return tag(2, body + b.bytes())


def matrix(tx, ty):
    b = Bits()
    b.u(0, 1)  # no scale
    b.u(0, 1)  # no rotate
    n = sbits(tx, ty)
    b.u(n, 5); b.s(tx, n); b.s(ty, n)
    return b.bytes()


def place(depth, char_id, x, y, first):
    flags = 0x06 if first else 0x05  # HasMatrix|HasCharacter  /  HasMatrix|Move
    body = bytes([flags]) + struct.pack("<H", depth)
    if first:
        body += struct.pack("<H", char_id)
    return tag(26, body + matrix(x * TW, y * TW))


def bounce(pos, vel, lo, hi):
    pos += vel
    if pos < lo or pos > hi:
        vel = -vel
        pos = max(lo, min(hi, pos))
    return pos, vel


def main(out):
    squares = [  # (size, colour, x, y, vx, vy)
        (80, (255, 0, 0), 40, 60, 6, 4),
        (50, (200, 0, 0), 300, 200, -5, 7),
        (30, (255, 80, 80), 150, 300, 8, -6),
    ]
    tags = tag(69, struct.pack("<I", 0))          # FileAttributes
    tags += tag(9, bytes([0, 0, 0]))              # black background
    for i, (size, rgb, *_rest) in enumerate(squares):
        tags += define_square(i + 1, size, rgb)
    state = [list(sq[2:]) for sq in squares]
    for f in range(FRAMES):
        for i, (size, *_r) in enumerate(squares):
            x, y, vx, vy = state[i]
            if f:
                x, vx = bounce(x, vx, 0, W - size)
                y, vy = bounce(y, vy, 0, H - size)
                state[i] = [x, y, vx, vy]
            tags += place(i + 1, i + 1, x, y, f == 0)
        tags += tag(1, b"")                       # ShowFrame
    tags += tag(0, b"")                           # End

    header_tail = rect(0, W * TW, 0, H * TW) + struct.pack("<BBH", 0, FPS, FRAMES)
    total = 8 + len(header_tail) + len(tags)
    data = b"FWS" + bytes([10]) + struct.pack("<I", total) + header_tail + tags
    with open(out, "wb") as fh:
        fh.write(data)
    print(f"wrote {out} ({len(data)} bytes, {FRAMES} frames)")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "bounce.swf")
