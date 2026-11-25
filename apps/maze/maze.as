// MAZE 3D for FlashOS
// A first-person maze: find the green EXIT door, collect the gems.
// Copyright (c) 2026 FlashOS. All rights reserved.
// Font: Press Start 2P by CodeMan38, SIL Open Font License 1.1.

var FONT = "Press Start 2P";
var RW = 256, RH = 160, SC = 4;           // 3D view: 256 x 160 pixels, shown 4x
var TS = 64;                              // texture size
var NLEVELS = 5;

var THEMES = [
    {name: "BRICK CELLAR", wall: 0xB04A32, mortar: 0x5A3A30, kind: "brick", ceil1: 0x1A1210, ceil2: 0x3A2A22, floor1: 0x2A2420, floor2: 0x6A6058, fog: 0x0A0806, gem: 0x30E0FF},
    {name: "STONE HALLS", wall: 0x8A8C90, mortar: 0x44464C, kind: "stone", ceil1: 0x10141C, ceil2: 0x2A3240, floor1: 0x2A2018, floor2: 0x6A5440, fog: 0x06080C, gem: 0xFFD040},
    {name: "WOOD CABIN", wall: 0x9A6430, mortar: 0x4A2C12, kind: "wood", ceil1: 0x1A1008, ceil2: 0x3C2814, floor1: 0x1C2A14, floor2: 0x4A6A30, fog: 0x080502, gem: 0xFF5080},
    {name: "ICE CAVES", wall: 0x9AD8F0, mortar: 0x4A8CB0, kind: "ice", ceil1: 0x061830, ceil2: 0x1A4870, floor1: 0x50708A, floor2: 0xD8F0FF, fog: 0x0A1C30, gem: 0x60FF80},
    {name: "CRYSTAL CORE", wall: 0x9040D0, mortar: 0x401060, kind: "crystal", ceil1: 0x08020E, ceil2: 0x2A0A40, floor1: 0x1A0628, floor2: 0x5A2070, fog: 0x0C0214, gem: 0xFFFFFF}
];

// ---------------------------------------------------------------- random numbers (same mazes every time)
var seed = 1;
function rnd() { seed = (seed * 16807) % 2147483647; return seed / 2147483647; }
function rint(n) { return (rnd() * n) | 0; }

// ---------------------------------------------------------------- colours
function rgb(r, g, b) {
    r = r < 0 ? 0 : (r > 255 ? 255 : r | 0);
    g = g < 0 ? 0 : (g > 255 ? 255 : g | 0);
    b = b < 0 ? 0 : (b > 255 ? 255 : b | 0);
    return (r << 16) | (g << 8) | b;
}
function shade(c, f) { return rgb(((c >> 16) & 255) * f, ((c >> 8) & 255) * f, (c & 255) * f); }
function mix(a, b, t) {
    return rgb(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t, ((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t, (a & 255) * (1 - t) + (b & 255) * t);
}

// ---------------------------------------------------------------- textures
var R = new flash.geom.Rectangle(0, 0, 1, 1);
function rect(bmp, x, y, w, h, c) { R.x = x; R.y = y; R.width = w; R.height = h; bmp.fillRect(R, c); }
function noise(bmp, n, amt) {
    for (var i = 0; i < n; i++) {
        var x = rint(TS), y = rint(TS);
        bmp.setPixel(x, y, shade(bmp.getPixel(x, y), 1 + (rnd() - 0.5) * amt));
    }
}

function wallTexture(t, decor) {
    var b = new flash.display.BitmapData(TS, TS, false, t.mortar);
    var row, i, x, c;
    if (t.kind == "brick") {
        for (row = 0; row < 8; row++) {
            var off = (row % 2) * 16;
            for (i = -1; i < 3; i++) rect(b, i * 32 + off + 1, row * 8 + 1, 30, 6, shade(t.wall, 0.8 + rnd() * 0.4));
        }
        noise(b, 900, 0.35);
    } else if (t.kind == "stone") {
        var ys = [0, 18, 34, 50, 64];
        for (row = 0; row < 4; row++) {
            x = rint(20) - 20;
            while (x < TS) {
                var w = 18 + rint(18);
                rect(b, x + 1, ys[row] + 1, w - 2, ys[row + 1] - ys[row] - 2, shade(t.wall, 0.75 + rnd() * 0.45));
                x += w;
            }
        }
        noise(b, 1400, 0.45);
    } else if (t.kind == "wood") {
        for (i = 0; i < 4; i++) {
            c = shade(t.wall, 0.8 + rnd() * 0.35);
            rect(b, i * 16 + 1, 0, 14, TS, c);
            for (var g = 0; g < 5; g++) {           // grain
                var gx = i * 16 + 2 + rint(12), gy = rint(TS), gl = 8 + rint(24);
                rect(b, gx, gy, 1, gl, shade(c, 0.78));
            }
            rect(b, i * 16 + 7, 10 + rint(40), 2, 2, shade(c, 0.5)); // nail
        }
        noise(b, 600, 0.25);
    } else if (t.kind == "ice") {
        for (row = 0; row < 2; row++)
            for (i = 0; i < 2; i++) {
                c = shade(t.wall, 0.85 + rnd() * 0.25);
                rect(b, i * 32 + 1, row * 32 + 1, 30, 30, c);
                rect(b, i * 32 + 3, row * 32 + 3, 12, 2, 0xFFFFFF);
                rect(b, i * 32 + 3, row * 32 + 3, 2, 10, 0xFFFFFF);
            }
        noise(b, 500, 0.2);
    } else {
        for (i = 0; i < TS; i++) {                // diagonal crystal facets
            for (var j = 0; j < TS; j += 2) {
                var f = ((i + j) % 32 < 16) ? 1.15 : 0.8;
                if (((i * 3 + j * 5) % 23) == 0) f = 1.6;
                rect(b, i, j, 1, 2, shade(t.wall, f * (0.9 + ((i * 7 + j) % 5) * 0.04)));
            }
        }
        noise(b, 400, 0.3);
    }
    if (decor == 1) {                            // a torch
        rect(b, 29, 30, 6, 22, 0x4A2A14);
        rect(b, 27, 24, 10, 8, 0xFFB020);
        rect(b, 29, 18, 6, 8, 0xFFE060);
        rect(b, 31, 14, 2, 5, 0xFFF4C0);
    } else if (decor == 2) {                     // a banner
        rect(b, 18, 6, 28, 40, 0x7A0E18);
        rect(b, 20, 8, 24, 36, 0xC02030);
        rect(b, 28, 16, 8, 8, 0xFFD040);
        rect(b, 18, 46, 6, 6, 0x7A0E18); rect(b, 40, 46, 6, 6, 0x7A0E18);
    }
    return b;
}

function exitTexture() {
    var b = new flash.display.BitmapData(TS, TS, false, 0x203020);
    rect(b, 6, 4, 52, 60, 0x0C1A0C);
    rect(b, 9, 7, 46, 57, 0x1E9A3C);
    rect(b, 13, 12, 17, 22, 0x2CC050); rect(b, 34, 12, 17, 22, 0x2CC050);
    rect(b, 13, 38, 17, 22, 0x2CC050); rect(b, 34, 38, 17, 22, 0x2CC050);
    rect(b, 46, 34, 5, 5, 0xFFE060);         // handle
    // glowing arrow sign above
    rect(b, 22, 0, 20, 6, 0x60FF90);
    rect(b, 26, 1, 12, 4, 0xFFFFFF);
    noise(b, 300, 0.25);
    return b;
}

function gemTexture(col) {
    var b = new flash.display.BitmapData(32, 32, true, 0x00000000);
    for (var y = 0; y < 30; y++) {
        var hw = y < 10 ? y + 4 : (30 - y) * 0.7;
        hw = hw | 0;
        if (hw < 1) continue;
        var light = y < 10 ? 1.35 : 0.9 - (y - 10) * 0.02;
        rect(b, 16 - hw, y + 1, hw, 1, 0xFF000000 | shade(col, light));
        rect(b, 16, y + 1, hw, 1, 0xFF000000 | shade(col, light * 0.78));
    }
    rect(b, 11, 4, 3, 3, 0xFFFFFFFF);
    return b;
}

// ---------------------------------------------------------------- levels
var map, mw, mh, seen, gems, exitX, exitY, startX, startY, startA;
var level = 0, theme, texs, gemTex, bg;
var totalTime = 0, totalGems = 0;

function makeLevel(n) {
    seed = 7919 * (n + 1) + 17;
    var cw = 6 + n * 2, ch = 6 + n * 2;
    mw = cw * 2 + 1; mh = ch * 2 + 1;
    map = [];
    var i, x, y;
    for (i = 0; i < mw * mh; i++) map[i] = 1;
    // carve a perfect maze (depth first)
    var stack = [0], visited = [];
    for (i = 0; i < cw * ch; i++) visited[i] = false;
    visited[0] = true;
    map[1 * mw + 1] = 0;
    var dxs = [1, -1, 0, 0], dys = [0, 0, 1, -1];
    while (stack.length > 0) {
        var c = stack[stack.length - 1];
        var cx = c % cw, cy = (c / cw) | 0;
        var opts = [];
        for (var d = 0; d < 4; d++) {
            var nx = cx + dxs[d], ny = cy + dys[d];
            if (nx >= 0 && ny >= 0 && nx < cw && ny < ch && !visited[ny * cw + nx]) opts.push(d);
        }
        if (opts.length == 0) { stack.pop(); continue; }
        var k = opts[rint(opts.length)];
        var tx = cx + dxs[k], ty = cy + dys[k];
        visited[ty * cw + tx] = true;
        map[(cy * 2 + 1 + dys[k]) * mw + (cx * 2 + 1 + dxs[k])] = 0;
        map[(ty * 2 + 1) * mw + (tx * 2 + 1)] = 0;
        stack.push(ty * cw + tx);
    }
    // a few shortcuts so it is not a single path
    var holes = n * 3;
    for (i = 0; i < holes * 20 && holes > 0; i++) {
        x = 1 + rint(mw - 2); y = 1 + rint(mh - 2);
        if (map[y * mw + x] == 1 && ((x % 2 == 0 && y % 2 == 1 && map[y * mw + x - 1] == 0 && map[y * mw + x + 1] == 0) ||
                                     (x % 2 == 1 && y % 2 == 0 && map[(y - 1) * mw + x] == 0 && map[(y + 1) * mw + x] == 0))) {
            map[y * mw + x] = 0;
            holes--;
        }
    }
    // decorations on some walls
    for (i = 0; i < mw * mh; i++) if (map[i] == 1 && rnd() < 0.07) map[i] = 2 + rint(2);
    // distances from the start (breadth first) to place the exit and gems
    var dist = [];
    for (i = 0; i < mw * mh; i++) dist[i] = -1;
    var q = [mw + 1], qh = 0;
    dist[mw + 1] = 0;
    while (qh < q.length) {
        var p = q[qh++];
        var nb = [p + 1, p - 1, p + mw, p - mw];
        for (i = 0; i < 4; i++) if (map[nb[i]] == 0 && dist[nb[i]] < 0) { dist[nb[i]] = dist[p] + 1; q.push(nb[i]); }
    }
    // exit: on the outer wall next to the farthest edge cell
    var best = -1, bestCell = 0;
    for (i = 0; i < mw * mh; i++) {
        x = i % mw; y = (i / mw) | 0;
        if (dist[i] > best && map[i] == 0 && (x == 1 || y == 1 || x == mw - 2 || y == mh - 2) && i != mw + 1) { best = dist[i]; bestCell = i; }
    }
    x = bestCell % mw; y = (bestCell / mw) | 0;
    if (x == mw - 2) { exitX = mw - 1; exitY = y; }
    else if (y == mh - 2) { exitX = x; exitY = mh - 1; }
    else if (x == 1) { exitX = 0; exitY = y; }
    else { exitX = x; exitY = 0; }
    map[exitY * mw + exitX] = 9;
    // gems in dead ends, far from the start
    var ends = [];
    for (i = 0; i < mw * mh; i++) {
        if (map[i] != 0 || i == mw + 1 || i == bestCell) continue;
        var walls = (map[i + 1] != 0 ? 1 : 0) + (map[i - 1] != 0 ? 1 : 0) + (map[i + mw] != 0 ? 1 : 0) + (map[i - mw] != 0 ? 1 : 0);
        if (walls >= 3 && dist[i] > 4) ends.push(i);
    }
    gems = [];
    var want = 3 + n;
    while (gems.length < want && ends.length > 0) {
        var ek = rint(ends.length), e = ends[ek];
        ends[ek] = ends[ends.length - 1];
        ends.pop();
        gems.push({x: (e % mw) + 0.5, y: ((e / mw) | 0) + 0.5, taken: false});
    }
    // start: top left cell, looking down an open corridor
    startX = 1.5; startY = 1.5;
    startA = map[mw + 2] == 0 ? 0 : Math.PI / 2;   // east if open, else south
    seen = [];
    for (i = 0; i < mw * mh; i++) seen[i] = false;
}

function makeTheme(n) {
    theme = THEMES[n];
    texs = [];
    texs[1] = wallTexture(theme, 0);
    texs[2] = wallTexture(theme, 1);
    texs[3] = wallTexture(theme, 2);
    texs[9] = exitTexture();
    gemTex = gemTexture(theme.gem);
    // sky and floor with a fade into the distance
    bg = new flash.display.BitmapData(RW, RH, false, 0);
    for (var y = 0; y < RH / 2; y++) {
        var t = y / (RH / 2);
        rect(bg, 0, y, RW, 1, mix(theme.ceil2, theme.ceil1, 1 - (1 - t) * (1 - t)));
        rect(bg, 0, RH - 1 - y, RW, 1, mix(theme.floor2, theme.floor1, 1 - (1 - t) * (1 - t)));
    }
    rect(bg, 0, RH / 2 - 1, RW, 2, theme.fog);
}

// ---------------------------------------------------------------- 3D renderer (ray casting)
var screen = new flash.display.BitmapData(RW, RH, false, 0);
var zbuf = [];
var M = new flash.geom.Matrix(1, 0, 0, 1, 0, 0);
var CT = new flash.geom.ColorTransform(1, 1, 1, 1, 0, 0, 0, 0);
var CLIP = new flash.geom.Rectangle(0, 0, 1, RH);
var FULL = new flash.geom.Rectangle(0, 0, RW, RH);
var ORIGIN = new flash.geom.Point(0, 0);
var posX = 1.5, posY = 1.5, dirX = 1, dirY = 0, planeX = 0, planeY = 0.66, angle = 0;
var bob = 0;

function setAngle(a) {
    angle = a;
    dirX = Math.cos(a); dirY = Math.sin(a);
    planeX = -dirY * 0.66; planeY = dirX * 0.66;
}

function render() {
    var scr = screen, m = map, w = mw, tx = texs, zb = zbuf, mat = M, ct = CT, clip = CLIP;
    var px = posX, py = posY, dx = dirX, dy = dirY, plx = planeX, ply = planeY;
    var fog = theme.fog, fr = (fog >> 16) & 255, fg = (fog >> 8) & 255, fb = fog & 255;
    var half = RH / 2 + bob;
    scr.copyPixels(bg, FULL, ORIGIN);
    for (var x = 0; x < RW; x++) {
        var cam = 2 * x / RW - 1;
        var rdx = dx + plx * cam, rdy = dy + ply * cam;
        var mx = px | 0, my = py | 0;
        var ddx = rdx == 0 ? 1e30 : 1 / rdx, ddy = rdy == 0 ? 1e30 : 1 / rdy;
        if (ddx < 0) ddx = -ddx;
        if (ddy < 0) ddy = -ddy;
        var sx, sy, sdx, sdy;
        if (rdx < 0) { sx = -1; sdx = (px - mx) * ddx; } else { sx = 1; sdx = (mx + 1 - px) * ddx; }
        if (rdy < 0) { sy = -1; sdy = (py - my) * ddy; } else { sy = 1; sdy = (my + 1 - py) * ddy; }
        var hit = 0, side = 0;
        while (hit == 0) {
            if (sdx < sdy) { sdx += ddx; mx += sx; side = 0; }
            else { sdy += ddy; my += sy; side = 1; }
            hit = m[my * w + mx];
        }
        var perp = side == 0 ? sdx - ddx : sdy - ddy;
        if (perp < 0.05) perp = 0.05;
        zb[x] = perp;
        var lh = RH / perp;
        var wx = side == 0 ? py + perp * rdy : px + perp * rdx;
        wx -= wx | 0;
        var t = (wx * TS) | 0;
        if ((side == 0 && rdx > 0) || (side == 1 && rdy < 0)) t = TS - 1 - t;
        // light: darker with distance and on one side of each wall
        var l = 1.25 / (1 + perp * perp * 0.045);
        if (l > 1) l = 1;
        if (side == 1) l *= 0.72;
        if (hit == 9) l = l * 0.5 + 0.5;
        var f = 1 - l;
        ct.redMultiplier = l; ct.greenMultiplier = l; ct.blueMultiplier = l;
        ct.redOffset = fr * f; ct.greenOffset = fg * f; ct.blueOffset = fb * f;
        mat.d = lh / TS;
        mat.tx = x - t;
        mat.ty = half - lh / 2;
        clip.x = x;
        scr.draw(tx[hit], mat, ct, null, clip);
    }
    drawGems(px, py, dx, dy, plx, ply, half);
}

// gems: flat sprites that face the player, hidden behind walls with the depth buffer
function drawGems(px, py, dx, dy, plx, ply, half) {
    var inv = 1 / (plx * dy - dx * ply);
    var order = [];
    var i, g;
    for (i = 0; i < gems.length; i++) {
        g = gems[i];
        if (g.taken) continue;
        g.d = (g.x - px) * (g.x - px) + (g.y - py) * (g.y - py);
        order.push(g);
    }
    for (i = 1; i < order.length; i++) {          // farthest first
        g = order[i];
        var j = i - 1;
        while (j >= 0 && order[j].d < g.d) { order[j + 1] = order[j]; j--; }
        order[j + 1] = g;
    }
    var spin = getTimer() / 300;
    for (i = 0; i < order.length; i++) {
        g = order[i];
        var rx = g.x - px, ry = g.y - py;
        var tX = inv * (dy * rx - dx * ry), tY = inv * (-ply * rx + plx * ry);
        if (tY < 0.15) continue;
        var cx = (RW / 2) * (1 + tX / tY);
        var size = (RH / tY) * 0.4;
        var wscale = 0.55 + 0.45 * Math.abs(Math.cos(spin + i));   // turning
        var w = size * wscale;
        var x0 = Math.ceil(cx - w / 2), x1 = Math.floor(cx + w / 2);
        if (x1 < 0 || x0 >= RW) continue;
        if (x0 < 0) x0 = 0;
        if (x1 > RW - 1) x1 = RW - 1;
        var l = 1.4 / (1 + tY * tY * 0.04);
        if (l > 1) l = 1;
        CT.redMultiplier = l; CT.greenMultiplier = l; CT.blueMultiplier = l;
        CT.redOffset = 0; CT.greenOffset = 0; CT.blueOffset = 0;
        M.d = size / 32;
        var ma = w / 32;
        M.a = ma;
        M.tx = cx - w / 2;
        M.ty = half + RH / tY * 0.12 - size / 2 + Math.sin(spin * 1.7 + i) * size * 0.08;
        // draw the visible runs of columns
        var x = x0;
        while (x <= x1) {
            while (x <= x1 && zbuf[x] < tY) x++;
            var start = x;
            while (x <= x1 && zbuf[x] >= tY) x++;
            if (x > start) {
                CLIP.x = start;
                CLIP.width = x - start;
                screen.draw(gemTex, M, CT, null, CLIP);
            }
        }
        M.a = 1;
        CLIP.width = 1;
    }
}

// ---------------------------------------------------------------- screen layout
var view = _root.createEmptyMovieClip("view", 1);
view.attachBitmap(screen, 1, "never", false);
view._xscale = view._yscale = SC * 100;

var hud = _root.createEmptyMovieClip("hud", 5);
hud._y = RH * SC;
function box(mc, x, y, w, h, fill) {
    mc.beginFill(fill, 100);
    mc.moveTo(x, y); mc.lineTo(x + w, y); mc.lineTo(x + w, y + h); mc.lineTo(x, y + h); mc.lineTo(x, y);
    mc.endFill();
}
box(hud, 0, 0, 1024, 128, 0x101828);
box(hud, 0, 0, 1024, 6, 0x30E0FF);
box(hud, 0, 6, 1024, 4, 0x1A6A80);

function text(mc, name, depth, x, y, w, h, size, color, align) {
    mc.createTextField(name, depth, x, y, w, h);
    var t = mc[name];
    t.embedFonts = true;
    t.selectable = false;
    var f = new TextFormat();
    f.font = FONT; f.size = size; f.color = color; f.align = align ? align : "left";
    t.setNewTextFormat(f);
    t.text = "";
    return t;
}
var hudLevel = text(hud, "lv", 10, 24, 26, 420, 24, 16, 0x30E0FF);
var hudName = text(hud, "nm", 11, 24, 58, 420, 24, 16, 0xFFFFFF);
var hudTime = text(hud, "tm", 12, 450, 26, 300, 24, 16, 0xFFD040);
var hudGems = text(hud, "gm", 13, 450, 58, 300, 24, 16, 0x60FF80);
var hudHelp = text(hud, "hp", 14, 24, 92, 980, 24, 8, 0x8090A8);
var HELP = "ARROWS / W A S D: MOVE    M: MAP    ESC: PAUSE    F12: QUIT";
hudHelp.text = HELP;
var hudFps = text(hud, "fps", 15, 860, 92, 140, 24, 8, 0x506070, "right");

// compass
var compass = hud.createEmptyMovieClip("compass", 20);
compass._x = 900; compass._y = 52;
box(compass, -34, -34, 68, 68, 0x1A2438);
var needle = compass.createEmptyMovieClip("n", 1);
needle.beginFill(0xFF4040, 100); needle.moveTo(0, -26); needle.lineTo(7, 0); needle.lineTo(-7, 0); needle.lineTo(0, -26); needle.endFill();
needle.beginFill(0xE0E0E0, 100); needle.moveTo(0, 26); needle.lineTo(7, 0); needle.lineTo(-7, 0); needle.lineTo(0, 26); needle.endFill();
text(compass, "nn", 2, -8, -48, 16, 16, 8, 0xFF8080, "center").text = "N";

// minimap (top right of the view)
var mapClip = _root.createEmptyMovieClip("mapclip", 6);
var mini = null, CELL = 6, mapOn = true;
var mapDot = mapClip.createEmptyMovieClip("dot", 5);
mapDot.beginFill(0xFFFF40, 100); mapDot.moveTo(6, 0); mapDot.lineTo(-4, -4); mapDot.lineTo(-4, 4); mapDot.lineTo(6, 0); mapDot.endFill();

function setupMap() {
    mini = new flash.display.BitmapData(mw * CELL, mh * CELL, true, 0x80000000);
    mapClip.attachBitmap(mini, 1, "never", false);
    mapClip._x = 1024 - mw * CELL - 16;
    mapClip._y = 16;
}
function reveal() {
    var cx = posX | 0, cy = posY | 0;
    for (var y = cy - 2; y <= cy + 2; y++) {
        for (var x = cx - 2; x <= cx + 2; x++) {
            if (x < 0 || y < 0 || x >= mw || y >= mh) continue;
            var i = y * mw + x;
            if (seen[i]) continue;
            seen[i] = true;
            var v = map[i];
            var c = v == 0 ? 0xE0283040 : (v == 9 ? 0xFF30FF70 : (0xF0000000 | shade(theme.wall, 0.9)));
            rect(mini, x * CELL, y * CELL, CELL, CELL, c);
        }
    }
}
function drawMapGems() {
    for (var i = 0; i < gems.length; i++) {
        var g = gems[i];
        var gx = g.x | 0, gy = g.y | 0;
        if (seen[gy * mw + gx]) rect(mini, gx * CELL + 1, gy * CELL + 1, CELL - 2, CELL - 2, g.taken ? 0xE0283040 : (0xFF000000 | theme.gem));
    }
}

// overlay for titles and messages
var over = _root.createEmptyMovieClip("over", 10);
var overBg = over.createEmptyMovieClip("bg", 1);
box(overBg, 0, 0, 1024, 768, 0x000000);
overBg._alpha = 55;
var bigShadow = text(over, "bs", 2, 0, 0, 1024, 100, 64, 0x103040, "center");
var big = text(over, "bt", 3, 0, 0, 1024, 100, 64, 0x30E0FF, "center");
var sub = text(over, "st", 4, 0, 0, 1024, 40, 24, 0xFFFFFF, "center");
var body = text(over, "bd", 5, 112, 0, 800, 300, 16, 0xC0D0E0, "center");
body.multiline = true;
body.wordWrap = true;
var blink = text(over, "bl", 6, 0, 660, 1024, 40, 16, 0xFFD040, "center");

function showOverlay(title, subtitle, info, prompt, titleY) {
    over._visible = true;
    big.text = title; bigShadow.text = title;
    big._y = titleY; bigShadow._y = titleY + 8; bigShadow._x = 8;
    sub.text = subtitle; sub._y = titleY + 110;
    body.text = info; body._y = titleY + 170;
    blink.text = prompt;
}

// ---------------------------------------------------------------- demo walk behind the title
var path = [], pathI = 0;
// shortest way from cell a to any cell in targets (an object used as a set)
function bfsPath(a, targets, goal) {
    var prev = [], i;
    for (i = 0; i < mw * mh; i++) prev[i] = -2;
    var q = [a], qh = 0, end = -1;
    prev[a] = -1;
    while (qh < q.length) {
        var p = q[qh++];
        if (targets[p]) { end = p; break; }
        var nb = [p + 1, p - 1, p + mw, p - mw];
        for (i = 0; i < 4; i++) {
            var n = nb[i];
            if ((map[n] == 0 || n == goal) && prev[n] == -2) { prev[n] = p; q.push(n); }
        }
    }
    var out = [];
    while (end >= 0) { out.unshift(end); end = prev[end]; }
    return out;
}
// the demo visits the nearest gem again and again, then goes to the door
function computePath() {
    var at = (posY | 0) * mw + (posX | 0), goal = exitY * mw + exitX;
    var left = {}, n = 0, i, k;
    for (i = 0; i < gems.length; i++) if (!gems[i].taken) { left[((gems[i].y | 0) * mw + (gems[i].x | 0))] = true; n++; }
    path = [at];
    while (n > 0) {
        var part = bfsPath(at, left, goal);
        if (part.length == 0) break;
        for (k = 1; k < part.length; k++) path.push(part[k]);
        at = part[part.length - 1];
        left[at] = false;
        n--;
    }
    var door = {};
    door[goal] = true;
    var last = bfsPath(at, door, goal);
    for (k = 1; k < last.length; k++) path.push(last[k]);
    pathI = 1;
}
function autoWalk(dt) {
    if (pathI >= path.length - 1) {          // at the door
        if (state == "demo") { if (level + 1 < NLEVELS) startDemo(level + 1); else title(); return; }
        posX = startX; posY = startY; setAngle(startA); pathI = 1;
        return;
    }
    var c = path[pathI];
    var tx = (c % mw) + 0.5, ty = ((c / mw) | 0) + 0.5;
    var want = Math.atan2(ty - posY, tx - posX);
    var d = want - angle;
    while (d > Math.PI) d -= Math.PI * 2;
    while (d < -Math.PI) d += Math.PI * 2;
    var turn = 2.2 * dt;
    if (d > turn) d = turn;
    if (d < -turn) d = -turn;
    setAngle(angle + d);
    if (Math.abs(want - angle) < 0.6 || Math.abs(want - angle) > Math.PI * 2 - 0.6) {
        var sp = (state == "demo" ? 2.4 : 1.6) * dt;
        posX += dirX * sp; posY += dirY * sp;
    }
    if ((tx - posX) * (tx - posX) + (ty - posY) * (ty - posY) < 0.04) pathI++;
}

// ---------------------------------------------------------------- game states
var state = "title";       // title, play, pause, won, done
var levelStart = 0, levelTime = 0, pausedAt = 0, gotGems = 0;
var lastT = getTimer(), lastSec = -1, frames = 0, fpsT = getTimer(), showFps = false;
var bestTimes = [];

function two(n) { return n < 10 ? "0" + n : "" + n; }
function clock(ms) { var s = (ms / 1000) | 0; return two((s / 60) | 0) + ":" + two(s % 60); }

function startLevel(n) {
    level = n;
    makeLevel(n);
    makeTheme(n);
    setupMap();
    posX = startX; posY = startY;
    setAngle(startA);
    gotGems = 0;
    levelStart = getTimer();
    lastSec = -1;
    reveal();
    drawMapGems();
    hudLevel.text = "LEVEL " + (n + 1) + "/" + NLEVELS;
    hudName.text = theme.name;
    updateHud();
    over._visible = false;
    mapClip._visible = mapOn;
    hudHelp.text = HELP;
    state = "play";
}
function startDemo(n) {
    startLevel(n);
    state = "demo";
    computePath();
    hudHelp.text = "DEMO - PRESS ANY KEY TO STOP";
}

function updateHud() {
    hudGems.text = "GEMS " + gotGems + "/" + gems.length;
    var ms = getTimer() - levelStart;
    lastSec = (ms / 1000) | 0;
    hudTime.text = "TIME " + clock(ms);
}

function title() {
    state = "title";
    level = 0;
    totalTime = 0;
    totalGems = 0;
    makeLevel(0);
    makeTheme(0);
    setupMap();
    mapClip._visible = false;
    posX = startX; posY = startY;
    setAngle(startA);
    computePath();
    hudLevel.text = "LEVEL -/" + NLEVELS;
    hudName.text = "";
    hudTime.text = "";
    hudGems.text = "";
    showOverlay("MAZE 3D", "FIND THE GREEN EXIT DOOR",
        "COLLECT THE GEMS ON THE WAY.\n\nUP / DOWN OR W / S: WALK\nLEFT / RIGHT: TURN\nA / D: STEP SIDEWAYS   SHIFT: RUN\nM: MAP\n\nPRESS 1 - 5 TO PICK A LEVEL\nPRESS 0 TO WATCH THE DEMO", "PRESS ENTER TO START", 80);
}

function finishLevel() {
    var t = getTimer() - levelStart;
    totalTime += t;
    totalGems += gotGems;
    if (!bestTimes[level] || t < bestTimes[level]) bestTimes[level] = t;
    if (level + 1 >= NLEVELS) {
        state = "done";
        showOverlay("YOU ESCAPED!", "ALL " + NLEVELS + " MAZES DONE",
            "TOTAL TIME  " + clock(totalTime) + "\n\nGEMS  " + totalGems, "PRESS ENTER", 160);
        return;
    }
    state = "won";
    var allGems = gotGems == gems.length;
    showOverlay("LEVEL " + (level + 1) + " DONE!", allGems ? "ALL GEMS FOUND!" : "YOU FOUND THE EXIT",
        "TIME  " + clock(t) + "\n\nGEMS  " + gotGems + " / " + gems.length + "\n\nNEXT: " + THEMES[level + 1].name, "PRESS ENTER FOR LEVEL " + (level + 2), 160);
}

// ---------------------------------------------------------------- movement
function free(x, y) {
    var v = map[(y | 0) * mw + (x | 0)];
    return v == 0;
}
function tryMove(nx, ny) {
    var r = 0.22;
    var sx = nx > posX ? r : -r, sy = ny > posY ? r : -r;
    if (map[(posY | 0) * mw + ((nx + sx) | 0)] == 9 || map[((ny + sy) | 0) * mw + (posX | 0)] == 9) { finishLevel(); return; }
    if (free(nx + sx, posY - r) && free(nx + sx, posY + r)) posX = nx;
    if (free(posX - r, ny + sy) && free(posX + r, ny + sy)) posY = ny;
}

var keyL = {};
keyL.onKeyDown = function () {
    var c = Key.getCode();
    if (state == "title") {
        if (c == 13) startLevel(0);
        if (c >= 49 && c <= 48 + NLEVELS) startLevel(c - 49);
        if (c == 48) startDemo(0);
        return;
    }
    if (state == "demo") { title(); return; }
    if (state == "won") { if (c == 13) startLevel(level + 1); return; }
    if (state == "done") { if (c == 13) title(); return; }
    if (state == "pause") {
        if (c == 13 || c == 27) { levelStart += getTimer() - pausedAt; over._visible = false; state = "play"; }
        else if (c == 81) Mouse.hide();
title();
        return;
    }
    if (c == 77) { mapOn = !mapOn; mapClip._visible = mapOn; }
    if (c == 70) { showFps = !showFps; if (!showFps) hudFps.text = ""; }
    if (c == 27) {
        state = "pause";
        pausedAt = getTimer();
        showOverlay("PAUSED", theme.name, "", "ENTER: GO ON     Q: BACK TO THE TITLE", 200);
    }
};
Key.addListener(keyL);

_root.onEnterFrame = function () {
    var now = getTimer();
    var dt = (now - lastT) / 1000;
    lastT = now;
    if (dt > 0.1) dt = 0.1;
    frames++;
    if (showFps && now - fpsT >= 1000) { hudFps.text = frames + " FPS"; frames = 0; fpsT = now; }
    else if (!showFps && now - fpsT >= 1000) { frames = 0; fpsT = now; }

    if (state == "title") {
        autoWalk(dt);
        bob = 0;
        render();
        blink._visible = ((now / 500) | 0) % 2 == 0;
        return;
    }
    if (state != "play" && state != "demo") { blink._visible = ((now / 500) | 0) % 2 == 0; return; }
    if (state == "demo") {
        autoWalk(dt);
        if (state != "demo") return;
        bob = Math.sin(now / 90) * 2;
    } else {

    var run = Key.isDown(16) ? 1.7 : 1;
    var sp = 2.6 * run * dt, turn = 2.4 * dt;
    var moving = false;
    if (Key.isDown(37)) setAngle(angle - turn);
    if (Key.isDown(39)) setAngle(angle + turn);
    if (Key.isDown(38) || Key.isDown(87)) { tryMove(posX + dirX * sp, posY + dirY * sp); moving = true; }
    if (state != "play") return;
    if (Key.isDown(40) || Key.isDown(83)) { tryMove(posX - dirX * sp, posY - dirY * sp); moving = true; }
    if (Key.isDown(65)) { tryMove(posX + dirY * sp, posY - dirX * sp); moving = true; }
    if (Key.isDown(68)) { tryMove(posX - dirY * sp, posY + dirX * sp); moving = true; }
    if (state != "play") return;
    bob = moving ? Math.sin(now / 90 * run) * 2 : bob * 0.8;
    }

    // gems
    for (var i = 0; i < gems.length; i++) {
        var g = gems[i];
        if (!g.taken && (g.x - posX) * (g.x - posX) + (g.y - posY) * (g.y - posY) < 0.2) {
            g.taken = true;
            gotGems++;
            updateHud();
            drawMapGems();
        }
    }
    reveal();
    if (mapOn) {
        drawMapGems();
        mapDot._x = posX * CELL;
        mapDot._y = posY * CELL;
        mapDot._rotation = angle * 180 / Math.PI;
    }
    needle._rotation = -angle * 180 / Math.PI - 90;
    if ((((now - levelStart) / 1000) | 0) != lastSec) updateHud();
    render();
};

Mouse.hide();
title();
