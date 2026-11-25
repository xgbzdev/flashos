// ByroTT Setup (Byr.swf)
// Copyright (c) 2026 FlashOS. All rights reserved.

var W = 1024, H = 768;
var FONT = "Fredoka", FONT_B = "Fredoka SemiBold";
var C1 = 0x1EC8DC, C2 = 0x0E9FB0, INK = 0x1C2B36, GREY = 0x6E7C86, WHITE = 0xFFFFFF;

function fmt(size, color, bold, align) {
    var f = new TextFormat();
    f.font = bold ? FONT_B : FONT; f.size = size; f.color = color; f.align = align ? align : "left";
    return f;
}
function label(mc, name, depth, x, y, w, h, text, size, color, bold, align) {
    mc.createTextField(name, depth, x, y, w, h);
    var t = mc[name];
    t.embedFonts = true; t.selectable = false; t.multiline = true; t.wordWrap = true;
    t.setNewTextFormat(fmt(size, color, bold, align));
    t.text = text;
    return t;
}
function rrect(mc, x, y, w, h, r) {
    mc.moveTo(x + r, y); mc.lineTo(x + w - r, y); mc.curveTo(x + w, y, x + w, y + r);
    mc.lineTo(x + w, y + h - r); mc.curveTo(x + w, y + h, x + w - r, y + h);
    mc.lineTo(x + r, y + h); mc.curveTo(x, y + h, x, y + h - r);
    mc.lineTo(x, y + r); mc.curveTo(x, y, x + r, y);
}
function box(mc, x, y, w, h, fill, line, lw, r) {
    if (lw > 0) mc.lineStyle(lw, line, 100); else mc.lineStyle();
    mc.beginFill(fill, 100);
    rrect(mc, x, y, w, h, r);
    mc.endFill();
    mc.lineStyle();
}
function circle(mc, cx, cy, r, fill) {
    mc.beginFill(fill, 100);
    mc.moveTo(cx + r, cy);
    for (var i = 1; i <= 24; i++) { var a = i / 24 * Math.PI * 2; mc.lineTo(cx + Math.cos(a) * r, cy + Math.sin(a) * r); }
    mc.endFill();
}
function bird(g, x, y, s, col) {
    circle(g, x, y, 14 * s, col);
    circle(g, x + 12 * s, y - 10 * s, 9 * s, col);
    g.beginFill(0xFFB020, 100);
    g.moveTo(x + 19 * s, y - 13 * s); g.lineTo(x + 30 * s, y - 9 * s); g.lineTo(x + 19 * s, y - 5 * s); g.lineTo(x + 19 * s, y - 13 * s);
    g.endFill();
    circle(g, x + 14 * s, y - 12 * s, 2.6 * s, WHITE);
    circle(g, x + 14.8 * s, y - 12 * s, 1.4 * s, 0x10202A);
    g.beginFill(0xFFFFFF, 35);
    g.moveTo(x - 10 * s, y - 2 * s); g.curveTo(x - 2 * s, y - 14 * s, x + 6 * s, y - 2 * s); g.curveTo(x - 2 * s, y + 6 * s, x - 10 * s, y - 2 * s);
    g.endFill();
    g.beginFill(col, 100);
    g.moveTo(x - 12 * s, y - 4 * s); g.lineTo(x - 26 * s, y - 12 * s); g.lineTo(x - 20 * s, y + 2 * s); g.lineTo(x - 12 * s, y + 6 * s);
    g.endFill();
}
function button(parent, name, depth, x, y, w, h, text, onClick, primary) {
    var b = parent.createEmptyMovieClip(name, depth);
    b._x = x; b._y = y; b.w = w; b.h = h; b.primary = primary;
    b.bg = b.createEmptyMovieClip("bg", 1);
    b.lab = label(b, "lab", 2, 0, (h - 24) / 2, w, 30, text, 18, primary ? WHITE : INK, true, "center");
    b.paint = function (st) {
        this.bg.clear();
        if (this.primary) box(this.bg, 0, 0, this.w, this.h, st > 0 ? C2 : C1, 0, 0, 12);
        else box(this.bg, 0, 0, this.w, this.h, st > 0 ? 0xEEF4F6 : WHITE, st > 0 ? C1 : 0xC9D3DA, 2, 12);
    };
    b.paint(0);
    b.onRollOver = function () { this.paint(1); };
    b.onRollOut = function () { this.paint(0); };
    b.onRelease = function () { onClick(); };
    return b;
}

// ---------------------------------------------------------------- background: the pyramids
var bg = _root.createEmptyMovieClip("bg", 1);
var pic = FlashOS.loadBitmap("pyramids.jpg");
if (!pic) pic = FlashOS.loadBitmap("sky.jpg");
if (pic) {
    bg.attachBitmap(pic, 1, "auto", true);
    var s = Math.max(W / pic.width, H / pic.height);
    bg._xscale = bg._yscale = s * 100;
} else box(bg, 0, 0, W, H, 0x3A7BD5, 0, 0, 0);
var shade = _root.createEmptyMovieClip("shade", 2);
box(shade, 0, 0, W, H, 0x000000, 0, 0, 0);
shade._alpha = 25;

// the setup window
var PW = 680, PH = 470;
var win = _root.createEmptyMovieClip("win", 5);
win._x = (W - PW) / 2; win._y = (H - PH) / 2;
var sh = win.createEmptyMovieClip("sh", 1);
box(sh, 8, 10, PW, PH, 0x000000, 0, 0, 20);
sh._alpha = 30;
var fr = win.createEmptyMovieClip("fr", 2);
box(fr, 0, 0, PW, PH, WHITE, 0, 0, 20);
box(fr, 0, 0, PW, 64, C1, 0, 0, 20);
box(fr, 0, 40, PW, 24, C1, 0, 0, 0);
bird(fr, 40, 36, 0.95, WHITE);
label(win, "title", 3, 80, 14, 500, 40, "ByroTT Setup", 26, WHITE, true);
var stepTxt = label(win, "step", 4, PW - 200, 22, 180, 30, "", 15, WHITE, false, "right");
var page = null, primary = null, secondary = null;

function show(n) {
    if (page) page.removeMovieClip();
    page = win.createEmptyMovieClip("page", 10);
    page._y = 64;
    var p = page;
    if (n == 1) {
        stepTxt.text = "Step 1 of 3";
        var big = p.createEmptyMovieClip("bird", 1);
        bird(big, 110, 150, 3.0, C1);
        label(p, "h", 2, 230, 40, 420, 40, "Welcome to ByroTT!", 28, INK, true);
        label(p, "t", 3, 230, 90, 420, 200, "ByroTT is a bird themed desktop with windows, a terminal for SWF games, " +
              "files, the internet, notepad and settings.\n\nThis setup installs ByroTT on your hard disk.", 17, INK, false);
        secondary = function () { FlashOS.exit(); };
        primary = function () { show(2); };
        button(p, "cancel", 10, PW - 330, PH - 64 - 70, 140, 48, "Cancel", secondary, false);
        button(p, "next", 11, PW - 176, PH - 64 - 70, 150, 48, "Next", primary, true);
    } else if (n == 2) {
        stepTxt.text = "Step 2 of 3";
        label(p, "h", 2, 40, 24, 600, 40, "Before you install", 26, INK, true);
        label(p, "t", 3, 40, 76, 600, 260,
              "·  The computer will start ByroTT instead of the FlashOS terminal.\n" +
              "·  A copy of the original FlashOS stays inside ByroTT. You can uninstall ByroTT any time in Settings, and FlashOS comes back.\n" +
              "·  Your saved files (texts, drawings) are kept.\n" +
              "·  Hold F8 while the computer starts to get the FlashOS terminal.", 17, INK, false);
        secondary = function () { show(1); };
        primary = function () { install(); };
        button(p, "back", 10, PW - 330, PH - 64 - 70, 140, 48, "Back", secondary, false);
        button(p, "go", 11, PW - 176, PH - 64 - 70, 150, 48, "Install", primary, true);
    } else if (n == 3) {
        stepTxt.text = "Step 3 of 3";
        var ok = p.createEmptyMovieClip("ok", 1);
        circle(ok, 110, 140, 70, 0x2FAE5B);
        ok.lineStyle(14, WHITE, 100); ok.moveTo(76, 142); ok.lineTo(102, 168); ok.lineTo(148, 112); ok.lineStyle();
        label(p, "h", 2, 230, 40, 420, 40, "ByroTT is installed!", 28, INK, true);
        label(p, "t", 3, 230, 92, 420, 220, "Take the ByroTT CD out of the drive (VirtualBox: Settings, Storage, remove ByroTT.iso), " +
              "then press Restart.\n\nThe computer will start ByroTT.", 17, INK, false);
        secondary = null;
        primary = function () { FlashOS.restart(); };
        button(p, "rs", 11, PW - 176, PH - 64 - 70, 150, 48, "Restart", primary, true);
    }
}
function failed(msg) {
    if (page) page.removeMovieClip();
    page = win.createEmptyMovieClip("page", 10);
    page._y = 64;
    stepTxt.text = "";
    label(page, "h", 2, 40, 30, 600, 40, "ByroTT was not installed", 26, 0xC0302A, true);
    label(page, "t", 3, 40, 84, 600, 220, msg, 17, INK, false);
    secondary = function () { show(2); };
    primary = function () { FlashOS.exit(); };
    button(page, "back", 10, PW - 330, PH - 64 - 70, 140, 48, "Back", secondary, false);
    button(page, "quit", 11, PW - 176, PH - 64 - 70, 150, 48, "Close", primary, true);
}
function install() {
    if (!FlashOS.install) {
        failed("This FlashOS cannot install other systems.\n\nStart the computer from the ByroTT CD: in VirtualBox put ByroTT.iso in the CD drive and start the virtual machine, then type Byr.swf.");
        return;
    }
    var r = FlashOS.install();
    if (r == "ok") show(3);
    else if (r == "cancelled") show(2);
    else failed("Reason: " + r + ".");
}
// Enter = the blue button, Esc = the other one
var keys = {};
keys.onKeyDown = function () {
    var c = Key.getCode();
    if (c == 13 && primary) primary();
    else if (c == 27 && secondary) secondary();
};
Key.addListener(keys);
show(1);
