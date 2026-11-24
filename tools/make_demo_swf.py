#!/usr/bin/env python3
"""Generate the FlashOS demo movies.  Copyright (c) 2026 FlashOS. All rights reserved.

  bounce.swf     plain (FWS) movie: three red squares bouncing
  flashlogo.swf  compressed (CWS) movie: curves, radial + linear gradients,
                 RGBA colours, outlines and a spinning bolt inside a sprite
  catch.swf      ActionScript game: buttons, keyboard, mouse, attachMovie,
                 hitTest, setInterval, drawing API, Color, text fields

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


# ------------------------------------------------------------------ AVM1 assembler
class Reg:
    def __init__(self, n):
        self.n = n


UNDEF = object()


class AS:
    """Tiny ActionScript 1/2 bytecode assembler."""

    def __init__(self):
        self.code = []          # list of (kind, data)
        self.n = 0

    def op(self, code, payload=b""):
        if code >= 0x80:
            self.code.append(("raw", bytes([code]) + struct.pack("<H", len(payload)) + payload))
        else:
            self.code.append(("raw", bytes([code])))
        return self

    def push(self, *vals):
        out = b""
        for v in vals:
            if isinstance(v, Reg):
                out += bytes([4, v.n])
            elif v is UNDEF:
                out += bytes([3])
            elif v is None:
                out += bytes([2])
            elif isinstance(v, bool):
                out += bytes([5, 1 if v else 0])
            elif isinstance(v, int) and -2**31 <= v < 2**31:
                out += bytes([7]) + struct.pack("<i", v)
            elif isinstance(v, (int, float)):
                d = struct.pack("<d", float(v))
                out += bytes([6]) + d[4:8] + d[0:4]
            else:
                out += bytes([0]) + v.encode() + b"\0"
        return self.op(0x96, out)

    def label(self, name):
        self.code.append(("label", name))
        return self

    def _branch(self, code, name):
        self.code.append(("branch", (code, name)))
        return self

    def jump(self, name): return self._branch(0x99, name)
    def if_(self, name): return self._branch(0x9D, name)

    def func(self, name, params, body):
        b = body.bytes()
        payload = name.encode() + b"\0" + struct.pack("<H", len(params))
        for p in params:
            payload += p.encode() + b"\0"
        payload += struct.pack("<H", len(b))
        self.code.append(("raw", bytes([0x9B]) + struct.pack("<H", len(payload)) + payload + b))
        return self

    # common ops
    def getvar(self, n): return self.push(n).op(0x1C)
    def setvar(self): return self.op(0x1D)
    def getmember(self): return self.op(0x4E)
    def setmember(self): return self.op(0x4F)
    def callmethod(self): return self.op(0x52)
    def callfunc(self): return self.op(0x3D)
    def pop(self): return self.op(0x17)
    def add(self): return self.op(0x47)
    def sub(self): return self.op(0x0B)
    def mul(self): return self.op(0x0C)
    def div(self): return self.op(0x0D)
    def less(self): return self.op(0x48)
    def greater(self): return self.op(0x67)
    def eq(self): return self.op(0x49)
    def not_(self): return self.op(0x12)
    def new(self): return self.op(0x40)
    def stop(self): return self.op(0x07)
    def dup(self): return self.op(0x4C)
    def random(self): return self.op(0x30)
    def define_local(self): return self.op(0x3C)

    def bytes(self):
        # two passes: sizes then offsets
        sizes = []
        for kind, d in self.code:
            sizes.append(len(d) if kind == "raw" else (0 if kind == "label" else 5))
        pos, labels = 0, {}
        for (kind, d), sz in zip(self.code, sizes):
            if kind == "label":
                labels[d] = pos
            pos += sz
        out, pos = b"", 0
        for (kind, d), sz in zip(self.code, sizes):
            if kind == "raw":
                out += d
            elif kind == "branch":
                code, name = d
                off = labels[name] - (pos + 5)
                out += bytes([code]) + struct.pack("<Hh", 2, off)
            pos += sz
        return out + b"\0"

    def call(self, obj_path, method, *args):
        """obj.method(args) - obj_path pushes the object (callable that takes self)"""
        for a in reversed(args):
            a(self) if callable(a) else self.push(a)
        self.push(len(args))
        obj_path(self)
        self.push(method).callmethod()
        return self


def V(name):
    return lambda a: a.getvar(name)


def M(objf, member):
    return lambda a: (objf(a), a.push(member), a.getmember())


def edit_text(char_id, bounds, var, height, rgba, text=""):
    flags = 0x0100 | 0x0400 | 0x0800 | 0x0010  # HasFont, HasTextColor, ReadOnly, NoSelect
    if text:
        flags |= 0x8000
    body = struct.pack("<H", char_id) + rect(*bounds) + struct.pack(">H", flags)
    body += struct.pack("<HH", 1, height) + bytes(rgba)
    body += var.encode() + b"\0"
    if text:
        body += text.encode() + b"\0"
    return tag(37, body)


def device_font(font_id, name="_sans"):
    return tag(48, struct.pack("<H", font_id) + bytes([0, 1, len(name)]) + name.encode() + struct.pack("<H", 0) + struct.pack("<H", 0))


def rect_shape(shape_id, w, h, rgb, line=None, cx=0, cy=0):
    x0, y0 = -cx * TW, -cy * TW
    fills = [fill_solid(rgb)]
    lines = [line] if line else []
    return tag(2, shape_body(shape_id, (x0, x0 + w * TW, y0, y0 + h * TW), fills, lines,
                             [{"move": (x0, y0), "f1": 1, "ln": 1 if line else 0,
                               "segs": [("L", w * TW, 0), ("L", 0, h * TW), ("L", -w * TW, 0), ("L", 0, -h * TW)]}]))


def catch_game():
    """Interactive ActionScript demo: catch the falling balls with the paddle."""
    W, H = 550, 400
    tags = tag(69, struct.pack("<I", 0)) + tag(9, bytes([20, 0, 0]))
    tags += device_font(1)
    # 2: ball shape (circle r=15) and 10: ball sprite exported as "ball"
    R = 15 * TW
    tags += tag(2, shape_body(2, (-R, R, -R, R), [fill_solid((255, 60, 40))], [(2 * TW, (255, 255, 255))],
                              [{"move": (R, 0), "f1": 1, "ln": 1, "segs": circle_segs(R)}]))
    tags += tag(39, struct.pack("<HH", 10, 1) + place_new(1, 2, matrix()) + tag(1, b"") + tag(0, b""))
    tags += tag(56, struct.pack("<H", 1) + struct.pack("<H", 10) + b"ball\0")
    # 3: paddle shape, 11: paddle sprite
    tags += rect_shape(3, 100, 16, (255, 255, 255), cx=50, cy=8)
    tags += tag(39, struct.pack("<HH", 11, 1) + place_new(1, 3, matrix()) + tag(1, b"") + tag(0, b""))
    # 4: follower shape (small ring), 12: follower sprite
    r2 = 8 * TW
    tags += tag(2, shape_body(4, (-r2, r2, -r2, r2), [fill_solid((255, 200, 0))], [],
                              [{"move": (r2, 0), "f1": 1, "segs": circle_segs(r2)}]))
    tags += tag(39, struct.pack("<HH", 12, 1) + place_new(1, 4, matrix()) + tag(1, b"") + tag(0, b""))
    # 5,6,7: button states, 20: "+10" button
    tags += rect_shape(5, 90, 30, (160, 0, 0), line=(2 * TW, (255, 255, 255)))
    tags += rect_shape(6, 90, 30, (230, 30, 30), line=(2 * TW, (255, 255, 255)))
    tags += rect_shape(7, 90, 30, (255, 160, 0), line=(2 * TW, (255, 255, 255)))
    rec = b""
    for states, sid in ((0x01, 5), (0x02, 6), (0x04, 7), (0x08, 5)):
        rec += bytes([states]) + struct.pack("<HH", sid, 1) + matrix() + bytes([0])  # empty cxform
    rec += bytes([0])
    act = AS()
    act.push("_root").getvar("_root").push("score").getmember()  # placeholder (unused)
    act = AS()
    act.getvar("_root").push("score").getvar("_root").push("score").getmember().push(10).add().setmember()
    acode = act.bytes()
    cond = struct.pack("<HH", 0, 0x0008) + acode
    btn = struct.pack("<H", 20) + bytes([0]) + struct.pack("<H", 2 + len(rec)) + rec + cond
    tags += tag(34, btn)
    # 30, 31: text fields
    tags += edit_text(30, (0, 300 * TW, 0, 30 * TW), "scoreLine", 18 * TW, (255, 255, 255, 255), "Score: 0")
    tags += edit_text(31, (0, 540 * TW, 0, 24 * TW), "", 12 * TW, (255, 140, 140, 255),
                      "Arrows: move paddle   Click +10   Space: reset   F12: quit")

    # player clip with onClipEvent(enterFrame): arrow keys move it
    ce = AS()
    for key, delta in ((37, -9), (39, 9)):
        ce.push(key, 1).getvar("Key").push("isDown").callmethod().not_().if_(f"skip{key}")
        ce.push("_x").getvar("_x").push(delta).add().setvar()
        ce.label(f"skip{key}")
    ce_code = ce.bytes()
    clipacts = struct.pack("<HI", 0, 0x02) + struct.pack("<II", 0x02, len(ce_code)) + ce_code + struct.pack("<I", 0)
    place_player = tag(26, bytes([0x86 | 0x20]) + struct.pack("<HH", 5, 11) + matrix(275 * TW, 370 * TW) +
                       b"player\0" + clipacts)

    # ---- frame script ----
    a = AS()
    a.stop()
    a.push("score", 0).setvar()
    # drawing API: a triangle badge
    a.call(lambda s: s.getvar("_root"), "createEmptyMovieClip", "drawing", 50).pop()
    d = lambda s: s.getvar("drawing")
    a.call(d, "lineStyle", 3, 0xFFCC00, 100).pop()
    a.call(d, "beginFill", 0xFF0000, 60).pop()
    a.call(d, "moveTo", 470, 40).pop()
    a.call(d, "lineTo", 530, 40).pop()
    a.call(d, "lineTo", 500, 90).pop()
    a.call(d, "lineTo", 470, 40).pop()
    a.call(d, "endFill").pop()

    # function fall() { this._y += this.speed; if (this.hitTest(_root.player)) {...} else if (this._y > 420) remove }
    fall = AS()
    fall.getvar("this").push("_y").getvar("this").push("_y").getmember().getvar("this").push("speed").getmember().add().setmember()
    fall.call(lambda s: s.getvar("this"), "hitTest", lambda s: (s.getvar("_root"), s.push("player"), s.getmember())).not_().if_("miss")
    fall.getvar("_root").push("score").getvar("_root").push("score").getmember().push(1).add().setmember()
    fall.push(0x00FF00)  # new Color(_root.player).setRGB(0x00FF00)
    fall.push(1)
    fall.getvar("_root").push("player").getmember().push(1).push("Color").new()
    fall.push("setRGB").callmethod().pop()
    fall.call(lambda s: s.getvar("this"), "removeMovieClip").pop()
    fall.jump("end")
    fall.label("miss")
    fall.getvar("this").push("_y").getmember().push(420).greater().not_().if_("end")
    fall.call(lambda s: s.getvar("this"), "removeMovieClip").pop()
    fall.label("end")
    a.func("fall", [], fall)

    # function spawn() { var d = _root.getNextHighestDepth(); var b = _root.attachMovie("ball", "b"+d, d); ... }
    sp = AS()
    sp.push("d").call(lambda s: s.getvar("_root"), "getNextHighestDepth").define_local()
    sp.push("b").call(lambda s: s.getvar("_root"), "attachMovie", "ball",
                      lambda s: (s.push("b"), s.getvar("d"), s.add()), lambda s: s.getvar("d")).define_local()
    sp.getvar("b").push("_x").push(500).random().push(25).add().setmember()
    sp.getvar("b").push("_y", -20).setmember()
    sp.getvar("b").push("speed").push(4).random().push(3).add().setmember()
    sp.getvar("b").push("onEnterFrame").getvar("fall").setmember()
    sp.push(0xFFFFFF, 1).getvar("_root").push("player").getmember().push(1).push("Color").new()  # reset paddle colour
    sp.push("setRGB").callmethod().pop()
    a.func("spawn", [], sp)
    # setInterval(spawn, 700)
    a.push(700).getvar("spawn").push(2).push("setInterval").callfunc().pop()

    # follower.onEnterFrame = function () { this._x += (_root._xmouse - this._x) / 5; ... }
    fo = AS()
    for prop, mp in (("_x", "_xmouse"), ("_y", "_ymouse")):
        fo.getvar("this").push(prop)
        fo.getvar("this").push(prop).getmember()
        fo.getvar("_root").push(mp).getmember().getvar("this").push(prop).getmember().sub().push(5).div()
        fo.add().setmember()
    a.getvar("follower").push("onEnterFrame")
    a.func("", [], fo)
    a.setmember()

    # _root.onEnterFrame = function () { scoreLine = "Score: " + score; }
    upd = AS()
    upd.getvar("_root").push("scoreLine").push("Score: ").getvar("_root").push("score").getmember().add().setmember()
    a.getvar("_root").push("onEnterFrame")
    a.func("", [], upd)
    a.setmember()

    # var kl = new Object(); kl.onKeyDown = function () { if (Key.getCode() == 32) _root.score = 0; }; Key.addListener(kl);
    a.push("kl", 0, "Object").new().setvar()
    kd = AS()
    kd.push(0).getvar("Key").push("getCode").callmethod().push(32).eq().not_().if_("no")
    kd.getvar("_root").push("score", 0).setmember()
    kd.label("no")
    a.getvar("kl").push("onKeyDown")
    a.func("", [], kd)
    a.setmember()
    a.call(lambda s: s.getvar("Key"), "addListener", lambda s: s.getvar("kl")).pop()

    tags += place_player
    tags += tag(26, bytes([0x06 | 0x20]) + struct.pack("<HH", 6, 12) + matrix(275 * TW, 200 * TW) + b"follower\0")
    tags += tag(26, bytes([0x06]) + struct.pack("<HH", 7, 20) + matrix(20 * TW, 50 * TW))
    tags += tag(26, bytes([0x06]) + struct.pack("<HH", 8, 30) + matrix(20 * TW, 12 * TW))
    tags += tag(26, bytes([0x06]) + struct.pack("<HH", 9, 31) + matrix(8 * TW, 378 * TW))
    tags += tag(12, a.bytes())
    tags += tag(1, b"") + tag(0, b"")
    return header(b"CWS", W, H, 30, 1, tags)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    for name, data in (("bounce.swf", bounce()), ("flashlogo.swf", flashlogo()), ("catch.swf", catch_game())):
        with open(os.path.join(out, name), "wb") as fh:
            fh.write(data)
        print(f"wrote {name} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
