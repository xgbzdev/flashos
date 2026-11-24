#!/usr/bin/env python3
"""Generate the FlashOS demo movies.  Copyright (c) 2026 FlashOS. All rights reserved.

  bounce.swf     plain (FWS) movie: three red squares bouncing
  flashlogo.swf  compressed (CWS) movie: curves, radial + linear gradients,
                 RGBA colours, outlines and a spinning bolt inside a sprite

Usage: make_demo_swf.py OUTPUT_DIR
"""
import math
import os
import struct
import sys
import zlib

TW = 20


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


def ubits(*vals):
    return max(max(v.bit_length() for v in vals), 1)


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


def matrix(tx=0, ty=0, sx=1.0, sy=1.0, r0=0.0, r1=0.0):
    b = Bits()
    if sx != 1.0 or sy != 1.0:
        a, d = int(sx * 65536), int(sy * 65536)
        n = sbits(a, d)
        b.u(1, 1); b.u(n, 5); b.s(a, n); b.s(d, n)
    else:
        b.u(0, 1)
    if r0 or r1:
        p, q = int(r0 * 65536), int(r1 * 65536)
        n = sbits(p, q)
        b.u(1, 1); b.u(n, 5); b.s(p, n); b.s(q, n)
    else:
        b.u(0, 1)
    tx, ty = int(tx), int(ty)
    n = sbits(tx, ty) if (tx or ty) else 0
    b.u(n, 5)
    if n:
        b.s(tx, n); b.s(ty, n)
    return b.bytes()


def rot_matrix(tx, ty, angle, scale=1.0):
    c, s = math.cos(angle) * scale, math.sin(angle) * scale
    return matrix(tx, ty, c, c, s, -s)


def color(c, alpha):
    return bytes(c[:3]) + (bytes([c[3] if len(c) > 3 else 255]) if alpha else b"")


def fill_solid(c, alpha=False):
    return bytes([0x00]) + color(c, alpha)


def fill_gradient(kind, mat_bytes, stops, alpha=False):
    out = bytes([kind]) + mat_bytes + bytes([len(stops)])
    for ratio, c in stops:
        out += bytes([ratio]) + color(c, alpha)
    return out


def shape_body(shape_id, bounds, fills, lines, paths, alpha=False):
    """paths: list of dicts {move:(x,y), f0, f1, ln, segs:[('L',dx,dy) | ('C',cx,cy,ax,ay)]}"""
    body = struct.pack("<H", shape_id) + rect(*bounds)
    body += bytes([len(fills)]) + b"".join(fills)
    body += bytes([len(lines)])
    for w, c in lines:
        body += struct.pack("<H", w) + color(c, alpha)
    fb, lb = ubits(len(fills)), ubits(len(lines)) if lines else 0
    b = Bits()
    b.u(fb, 4); b.u(lb, 4)
    for p in paths:
        x, y = p["move"]
        b.u(0, 1)
        b.u(0, 1)                       # new styles
        b.u(1 if lb else 0, 1)          # line style
        b.u(1, 1)                       # fill style 1
        b.u(1, 1)                       # fill style 0
        b.u(1, 1)                       # move to
        n = sbits(x, y)
        b.u(n, 5); b.s(x, n); b.s(y, n)
        b.u(p.get("f0", 0), fb)
        b.u(p.get("f1", 0), fb)
        if lb:
            b.u(p.get("ln", 0), lb)
        for seg in p["segs"]:
            if seg[0] == "L":
                dx, dy = seg[1], seg[2]
                n = max(sbits(dx, dy), 2)
                b.u(1, 1); b.u(1, 1); b.u(n - 2, 4); b.u(1, 1); b.s(dx, n); b.s(dy, n)
            else:
                vals = seg[1:]
                n = max(sbits(*vals), 2)
                b.u(1, 1); b.u(0, 1); b.u(n - 2, 4)
                for v in vals:
                    b.s(v, n)
    b.u(0, 6)
    return body + b.bytes()


def place_new(depth, char_id, mat_bytes):
    return tag(26, bytes([0x06]) + struct.pack("<HH", depth, char_id) + mat_bytes)


def place_move(depth, mat_bytes):
    return tag(26, bytes([0x05]) + struct.pack("<H", depth) + mat_bytes)


def header(sig, w, h, fps, frames, tags):
    tail = rect(0, w * TW, 0, h * TW) + struct.pack("<BBH", 0, fps, frames)
    if sig == b"FWS":
        return b"FWS" + bytes([10]) + struct.pack("<I", 8 + len(tail) + len(tags)) + tail + tags
    raw = tail + tags
    return b"CWS" + bytes([10]) + struct.pack("<I", 8 + len(raw)) + zlib.compress(raw, 9)


# ------------------------------------------------------------------ bounce.swf
def square(shape_id, size, rgb):
    s = size * TW
    return tag(2, shape_body(shape_id, (0, s, 0, s), [fill_solid(rgb)], [],
                             [{"move": (0, 0), "f1": 1, "segs": [("L", s, 0), ("L", 0, s), ("L", -s, 0), ("L", 0, -s)]}]))


def bounce():
    W, H, frames = 550, 400, 120
    sq = [(80, (255, 0, 0), 40, 60, 6, 4), (50, (200, 0, 0), 300, 200, -5, 7), (30, (255, 80, 80), 150, 300, 8, -6)]
    tags = tag(69, struct.pack("<I", 0)) + tag(9, bytes([0, 0, 0]))
    for i, (size, rgb, *_r) in enumerate(sq):
        tags += square(i + 1, size, rgb)
    state = [list(s[2:]) for s in sq]
    for f in range(frames):
        for i, (size, *_r) in enumerate(sq):
            x, y, vx, vy = state[i]
            if f:
                x += vx
                if x < 0 or x > W - size:
                    vx, x = -vx, max(0, min(W - size, x))
                y += vy
                if y < 0 or y > H - size:
                    vy, y = -vy, max(0, min(H - size, y))
                state[i] = [x, y, vx, vy]
            m = matrix(x * TW, y * TW)
            tags += place_new(i + 1, i + 1, m) if f == 0 else place_move(i + 1, m)
        tags += tag(1, b"")
    tags += tag(0, b"")
    return header(b"FWS", W, H, 30, frames, tags)


# --------------------------------------------------------------- flashlogo.swf
def circle_segs(r):
    """8 quadratic curves around a circle of radius r starting at (r, 0)."""
    segs, k = [], 1 / math.cos(math.pi / 8)
    px, py = r, 0
    for i in range(1, 9):
        a0, a1 = (i - 1) * math.pi / 4, i * math.pi / 4
        am = (a0 + a1) / 2
        cx, cy = round(r * k * math.cos(am)), round(r * k * math.sin(am))
        ax, ay = round(r * math.cos(a1)), round(r * math.sin(a1))
        segs.append(("C", cx - px, cy - py, ax - cx, ay - cy))
        px, py = ax, ay
    return segs


def flashlogo():
    W, H, frames = 550, 400, 90
    R = 130 * TW
    tags = tag(69, struct.pack("<I", 0)) + tag(9, bytes([10, 0, 0]))

    # 1: glowing disc - radial gradient, RGBA, red outline (DefineShape3)
    grad_m = matrix(0, 0, R / 16384, R / 16384)
    disc = shape_body(1, (-R - 200, R + 200, -R - 200, R + 200),
                      [fill_gradient(0x12, grad_m, [(0, (255, 90, 60, 255)), (150, (200, 0, 0, 255)), (255, (60, 0, 0, 255))], True)],
                      [(8 * TW, (255, 40, 40, 255))],
                      [{"move": (R, 0), "f1": 1, "ln": 1, "segs": circle_segs(R)}], True)
    tags += tag(32, disc)

    # 2: lightning bolt (DefineShape, straight edges), centred on its origin
    pts = [(-30, -110), (45, -110), (10, -20), (55, -20), (-35, 115), (-5, 15), (-50, 15)]
    pts = [(x * TW, y * TW) for x, y in pts]
    segs, (sx, sy) = [], pts[0]
    for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1]):
        segs.append(("L", bx - ax, by - ay))
    bolt = shape_body(2, (-60 * TW, 60 * TW, -120 * TW, 120 * TW), [fill_solid((255, 235, 200))], [(3 * TW, (120, 0, 0))],
                      [{"move": (sx, sy), "f1": 1, "ln": 1, "segs": segs}])
    tags += tag(2, bolt)

    # 3: sprite - the bolt wobbling on its own 30-frame timeline
    sp = b""
    for f in range(30):
        ang = math.sin(f / 30 * 2 * math.pi) * 0.35
        m = rot_matrix(0, 0, ang)
        sp += place_new(1, 2, m) if f == 0 else place_move(1, m)
        sp += tag(1, b"")
    sp += tag(0, b"")
    tags += tag(39, struct.pack("<HH", 3, 30) + sp)

    # 4: floor bar - linear gradient, transparent ends (DefineShape3)
    bw, bh = 460 * TW, 14 * TW
    lin_m = matrix(0, 0, bw / 32768, 1.0)
    bar = shape_body(4, (-bw // 2, bw // 2, -bh // 2, bh // 2),
                     [fill_gradient(0x10, lin_m, [(0, (255, 0, 0, 0)), (128, (255, 30, 30, 255)), (255, (255, 0, 0, 0))], True)], [],
                     [{"move": (-bw // 2, -bh // 2), "f1": 1, "segs": [("L", bw, 0), ("L", 0, bh), ("L", -bw, 0), ("L", 0, -bh)]}], True)
    tags += tag(32, bar)

    cx, cy = W // 2 * TW, 185 * TW
    for f in range(frames):
        pulse = 1.0 + 0.06 * math.sin(f / frames * 4 * math.pi)
        if f == 0:
            tags += place_new(1, 1, matrix(cx, cy, pulse, pulse))
            tags += place_new(2, 3, matrix(cx, cy))
            tags += place_new(3, 4, matrix(cx, 360 * TW))
        else:
            tags += place_move(1, matrix(cx, cy, pulse, pulse))
        tags += tag(1, b"")
    tags += tag(0, b"")
    return header(b"CWS", W, H, 30, frames, tags)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    for name, data in (("bounce.swf", bounce()), ("flashlogo.swf", flashlogo())):
        with open(os.path.join(out, name), "wb") as fh:
            fh.write(data)
        print(f"wrote {name} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
