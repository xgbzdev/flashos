#!/usr/bin/env python3
"""ByroTT artwork: the logo at every size, and the logo wallpapers in colour
variants. The logo is the original hand-drawn sketch (art/logo_sketch.png,
white lines on black); its lines are thickened for each size so they stay
visible, then smoothed.

    python3 tools/byrott_art.py apps/byrott        # writes art/logo_*.png, wallpapers/logo-*.jpg

The logo is drawn as vectors (smooth Bezier feathers over a ring) with a
4x supersample, so it is sharp at 320 px and still readable at 24 px.
"""
import math, sys, os
from PIL import Image, ImageDraw, ImageFilter, ImageChops

# ---- the design, on a 1000 x 1000 grid ----------------------------------
RING_C = (548, 512)
RING_R = 330
# each feather: base, tip, bend (curves the middle line sideways), width.
# A feather is a closed outline, pointed at both ends, widest near its base.
FEATHERS = [
    # the wing raised up the left side, to the crest
    ((262, 880), (232, 112), -46, 64),
    # head and back, from the crest to the beak (right, outside the ring)
    ((232, 112), (888, 430), 74, 92),
    # three tail feathers fanning down through the ring
    ((352, 372), (402, 905), 34, 70),
    ((462, 404), (588, 925), 36, 70),
    ((570, 404), (776, 838), 36, 70),
]
EYE = (322, 196, 20)              # x, y, radius
ACCENT = (RING_R + 66, 212, 238)  # a short arc outside the ring: radius, from, to (degrees)


def bez(p0, p1, p2, p3, n=48):
    out = []
    for i in range(n + 1):
        t = i / n
        a = (1 - t) ** 3
        b = 3 * (1 - t) ** 2 * t
        c = 3 * (1 - t) * t * t
        d = t ** 3
        out.append((a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0],
                    a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1]))
    return out


def leaf(base, tip, bend, width, n=60):
    (x0, y0), (x1, y1) = base, tip
    dx, dy = x1 - x0, y1 - y0
    L = math.hypot(dx, dy)
    nx, ny = -dy / L, dx / L
    cx, cy = (x0 + x1) / 2 + nx * bend * 2, (y0 + y1) / 2 + ny * bend * 2
    left, right = [], []
    for i in range(n + 1):
        t = i / n
        # a point on the curved middle line, and the direction across it
        px = (1 - t) ** 2 * x0 + 2 * (1 - t) * t * cx + t * t * x1
        py = (1 - t) ** 2 * y0 + 2 * (1 - t) * t * cy + t * t * y1
        tx = 2 * (1 - t) * (cx - x0) + 2 * t * (x1 - cx)
        ty = 2 * (1 - t) * (cy - y0) + 2 * t * (y1 - cy)
        tl = math.hypot(tx, ty) or 1
        ox, oy = -ty / tl, tx / tl
        # widest at 35 %, pointed at both ends
        h = width / 2 * (math.sin(math.pi * (t ** 0.75)) ** 0.9)
        left.append((px + ox * h, py + oy * h))
        right.append((px - ox * h, py - oy * h))
    return left + right[::-1][1:]


def polyline(draw, pts, w, col):
    draw.line(pts, fill=col, width=int(w), joint="curve")
    r = w / 2
    for p in (pts[0], pts[-1]):
        draw.ellipse((p[0] - r, p[1] - r, p[0] + r, p[1] + r), fill=col)


def render_vector(size, stroke, simple=False):
    """white logo on transparent; stroke in 1000-grid units"""
    S = 4
    big = size * S
    k = big / 1000.0
    mask = Image.new("L", (big, big), 0)
    d = ImageDraw.Draw(mask)
    w = stroke * k
    gap = w * 0.75

    def P(pts):
        return [(x * k, y * k) for x, y in pts]

    cx, cy = RING_C
    ring = [(cx + RING_R * math.cos(a / 180 * math.pi), cy + RING_R * math.sin(a / 180 * math.pi)) for a in range(0, 361, 2)]
    polyline(d, P(ring), w, 255)
    ar, a0, a1 = ACCENT
    arc = [(cx + ar * math.cos(a / 180 * math.pi), cy + ar * math.sin(a / 180 * math.pi)) for a in range(a0, a1 + 1)]
    polyline(d, P(arc), w, 255)
    # feathers over the ring: a black halo first, so lines that cross leave a gap
    for i, (base, tip, bend, width) in enumerate(FEATHERS):
        if simple and i in (2, 4):
            continue   # tiny sizes: fewer feathers, so they do not blur together
        pts = P(leaf(base, tip, bend, width))
        d.polygon(pts, fill=0)
        polyline(d, pts + [pts[0]], w + gap * 2, 0)
        polyline(d, pts + [pts[0]], w, 255)
    ex, ey, er = EYE
    er = max(er * k, w * 0.75)
    if not simple:
        d.ellipse((ex * k - er, ey * k - er, ex * k + er, ey * k + er), fill=255)

    mask = mask.resize((size, size), Image.LANCZOS)
    out = Image.new("RGBA", (size, size), (255, 255, 255, 0))
    out.putalpha(mask)
    return out


SRC4 = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "apps", "byrott", "art", "logo_v4_src.png")


def render(size, grow=0, simple=False):
    """the 4.0 logo: a white outline (and ring) around a black bird, on
    transparent; grow thickens the white lines (in source pixels) for small
    sizes. Tint it by multiplying: the white takes the colour, black stays."""
    src = Image.open(SRC4)
    white, black = src.split()
    if grow > 1:
        white = white.filter(ImageFilter.MaxFilter(int(grow) | 1))
    # the JPEG left dark specks around the lines: remove them (opening), then
    # grow the fill a little so no gap is left where it meets the outline
    black = black.filter(ImageFilter.MinFilter(5)).filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MaxFilter(5))
    alpha = ImageChops.lighter(white, black)
    rgb = Image.merge("RGB", (white, white, white))          # white where the outline is, black elsewhere
    out = Image.merge("RGBA", (*rgb.split(), alpha))
    return out.resize((size, size), Image.LANCZOS)


def wallpaper(path, col, glow_col, bg_top, bg_bot, W=1024, H=768):
    img = Image.new("RGB", (W, H))
    px = img.load()
    for y in range(H):
        t = y / (H - 1)
        c = tuple(int(bg_top[i] * (1 - t) + bg_bot[i] * t) for i in range(3))
        for x in range(W):
            px[x, y] = c
    # a soft light behind the logo
    halo = Image.new("L", (W, H), 0)
    ImageDraw.Draw(halo).ellipse((W / 2 - 330, H / 2 - 340, W / 2 + 330, H / 2 + 320), fill=90)
    halo = halo.filter(ImageFilter.GaussianBlur(120))
    img = Image.composite(Image.new("RGB", (W, H), glow_col), img, halo)
    lg = render(560, 5)
    a = lg.getchannel("R").point(lambda v: v)              # the white lines only
    x0, y0 = (W - 560) // 2, (H - 560) // 2 - 10
    for blur, strength in ((26, 1.0), (9, 0.9)):
        g = Image.new("L", (W, H), 0)
        g.paste(a, (x0, y0))
        g = g.filter(ImageFilter.GaussianBlur(blur)).point(lambda v: min(255, int(v * 2.2 * strength)))
        img = Image.composite(Image.new("RGB", (W, H), glow_col), img, g)
    full = Image.new("L", (W, H), 0)
    full.paste(lg.getchannel("A"), (x0, y0))
    img = Image.composite(Image.new("RGB", (W, H), (0, 0, 0)), img, full)          # the black bird
    core = Image.new("L", (W, H), 0)
    core.paste(a, (x0, y0))
    img = Image.composite(Image.new("RGB", (W, H), col), img, core)                 # the lines
    img.save(path, quality=92)


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "apps/byrott"
    art = os.path.join(root, "art")
    render(784).save(os.path.join(art, "logo_master.png"))
    # thicker lines at small sizes, so they stay visible
    for name, size, grow in (("big", 320, 0), ("mid", 128, 9), ("small", 40, 31), ("tiny", 24, 45)):
        render(size, grow).save(os.path.join(art, "logo_%s.png" % name))
    walls = os.path.join(root, "wallpapers")
    for name, col, glow, top, bot in (
            ("cyan", (230, 255, 255), (30, 200, 220), (2, 18, 24), (0, 4, 8)),
            ("blue", (225, 235, 255), (47, 111, 224), (4, 10, 34), (0, 2, 10)),
            ("red", (255, 232, 228), (224, 68, 60), (30, 4, 4), (6, 0, 0)),
            ("green", (230, 255, 236), (47, 174, 91), (2, 24, 10), (0, 6, 2)),
            ("purple", (244, 232, 255), (150, 80, 230), (18, 6, 34), (4, 0, 10)),
            ("black", (255, 255, 255), (120, 120, 120), (14, 14, 14), (0, 0, 0))):
        wallpaper(os.path.join(walls, "logo-%s.jpg" % name), col, glow, top, bot)


if __name__ == "__main__" and len(sys.argv) < 3:
    main()


def catch_icon(size):
    """2000 style icon for the Catch game: a basket and a falling ball"""
    S = 8
    B = size * S
    im = Image.new("RGBA", (B, B), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    u = B / 48.0
    def U(*p):
        return [v * u for v in p]
    # shadow
    d.ellipse(U(6, 40, 42, 46), fill=(0, 0, 0, 70))
    # basket
    d.polygon(U(5, 24, 43, 24, 38, 42, 10, 42), fill=(200, 135, 58, 255), outline=(58, 36, 16, 255))
    d.line(U(5, 24, 43, 24, 38, 42, 10, 42, 5, 24), fill=(58, 36, 16, 255), width=int(2 * u))
    for k in (-2, -1, 0, 1, 2):
        d.line(U(24 + k * 8, 26, 24 + k * 6.5, 40), fill=(122, 78, 30, 255), width=int(1.5 * u))
    d.line(U(8, 31, 40, 31), fill=(122, 78, 30, 255), width=int(1.5 * u))
    d.rectangle(U(4, 21, 44, 25), fill=(160, 100, 40, 255), outline=(58, 36, 16, 255), width=int(1.5 * u))
    # the ball, red with a shine
    d.ellipse(U(15, 2, 33, 20), fill=(224, 68, 60, 255), outline=(90, 16, 14, 255), width=int(1.5 * u))
    d.ellipse(U(19, 5, 25, 11), fill=(255, 220, 215, 255))
    # motion lines
    for x in (12, 36):
        d.line(U(x, 3, x, 12), fill=(255, 255, 255, 200), width=int(1.5 * u))
    return im.resize((size, size), Image.LANCZOS)


if __name__ == "__main__" and len(sys.argv) > 2 and sys.argv[2] == "icons":
    catch_icon(48).save(os.path.join(sys.argv[1], "art", "icon_catch.png"))
    catch_icon(32).save(os.path.join(sys.argv[1], "art", "icon_catch_s.png"))
