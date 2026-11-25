// ByroTT 1.0 - made to look like its Scratch mock-up (Byrott_1.sb3)
// Copyright (c) 2026 FlashOS. All rights reserved.
// Fonts: GNU Unifont (SIL Open Font License 1.1); DejaVu Sans Condensed Bold
// for the Loading screen (Bitstream Vera licence).

var W = 1024, H = 768;
var FONT = "Unifont", FONT_B = "Unifont", MONO = "Unifont", BIG = "DejaVu Sans Condensed Bold";
// white windows with black lines on a black screen, an orange start button
var INK = 0x000000, GREY = 0x555555, WHITE = 0xFFFFFF, LIGHT = 0xEEEEEE;
var PANEL = 0xFFFFFF, EDGE = 0x000000, FIELD = 0xFFFFFF, BODY = 0xFFFFFF, SHADOW = 0x404040;
var ORANGE = 0xFF8C00, MENU = 0xFFBF6E, MENU_HI = 0xFFD9A6;
// window themes: bar colour, second colour (Byrott fades), name
var THEMES = {"default": [0xB4FFFF, 0xB4FFFF, "Default"], orange: [0xFFC400, 0xFFC400, "Orange"],
              green: [0x22EE66, 0x22EE66, "Green"], blue: [0x1E7BFF, 0x1E7BFF, "Blue"],
              byrott: [0xFFA030, 0xE8302A, "Byrott"]};
var THEME_NAMES = ["default", "orange", "green", "blue", "byrott"];
// pointer colours: fill, outline
var CURSORS = ["white", "black", "red", "green"];
var CURSOR_COLS = {white: [0xFFFFFF, 0x000000], black: [0x000000, 0xFFFFFF], red: [0xFF2020, 0xFFFFFF], green: [0x20E040, 0xFFFFFF]};
var CURSOR_NAMES = {white: "White", black: "Black", red: "Red", green: "Green"};
var cfg = {wallpaper: "pyramids.jpg", theme: "default", cursor: "white", volume: "80", muted: "0"};
var DEFAULT_WALL = "pyramids.jpg";
var C1, C2, C3;            // window theme: bar, bar fade, pale (selections)

function loadCfg() {
    var s = FlashOS.read("byrott.cfg");
    if (s != null) {
        var lines = s.split("\n");
        for (var i = 0; i < lines.length; i++) {
            var k = lines[i].indexOf("=");
            if (k > 0) cfg[lines[i].substr(0, k)] = lines[i].substr(k + 1);
        }
    }
    if (!THEMES[cfg.theme]) cfg.theme = "default";
    if (!CURSOR_COLS[cfg.cursor]) cfg.cursor = "white";
    applyColor();
}
function saveCfg() {
    FlashOS.write("byrott.cfg", "wallpaper=" + cfg.wallpaper + "\ntheme=" + cfg.theme + "\ncursor=" + cfg.cursor +
                  "\nvolume=" + cfg.volume + "\nmuted=" + cfg.muted + "\n");
}
function mix(a, b, t) {
    var c = 0, sh = 16;
    for (var k = 0; k < 3; k++) { var x = (a >> sh) & 255, y = (b >> sh) & 255; c |= Math.round(x + (y - x) * t) << sh; sh -= 8; }
    return c;
}
function applyColor() { var c = THEMES[cfg.theme]; C1 = c[0]; C2 = c[1]; C3 = mix(C1, 0xFFFFFF, 0.6); }
// text on the bar: black on the light themes, white on blue and Byrott
function onC1() { return (cfg.theme == "blue" || cfg.theme == "byrott") ? WHITE : INK; }

var depthN = 1000;
function nd() { depthN++; return depthN; }

// ---------------------------------------------------------------- drawing
function fmt(size, color, bold, align, font) {
    var f = new TextFormat();
    f.font = font ? font : FONT;
    // Unifont is a 16 px pixel font: keep it at 16 or 32 so it stays sharp
    if (f.font == FONT) size = size < 24 ? 16 : (size < 48 ? 32 : 64);
    f.size = size;
    f.color = color;
    f.align = align ? align : "left";
    return f;
}
function label(mc, name, depth, x, y, w, h, text, size, color, bold, align, font) {
    mc.createTextField(name, depth, x, y, w, h);
    var t = mc[name];
    t.embedFonts = true;
    t.selectable = false;
    t.setNewTextFormat(fmt(size, color, bold, align, font));
    t.text = text;
    return t;
}
function input(mc, name, depth, x, y, w, h, size, font) {
    mc.createTextField(name, depth, x, y, w, h);
    var t = mc[name];
    t.embedFonts = true;
    t.type = "input";
    t.border = true;
    t.borderColor = EDGE;
    t.background = true;
    t.backgroundColor = FIELD;
    t.setNewTextFormat(fmt(size, INK, false, "left", font));
    t.text = "";
    return t;
}
function rrect(mc, x, y, w, h, r) {
    mc.moveTo(x, y); mc.lineTo(x + w, y); mc.lineTo(x + w, y + h); mc.lineTo(x, y + h); mc.lineTo(x, y);
}
function box(mc, x, y, w, h, fill, line, lw, r) {
    if (lw > 0) mc.lineStyle(lw, line, 100); else mc.lineStyle();
    if (fill >= 0) mc.beginFill(fill, 100);
    rrect(mc, x, y, w, h, 0);
    if (fill >= 0) mc.endFill();
    mc.lineStyle();
}
// a black frame (the mock-up draws everything with black lines)
function bevel(mc, x, y, w, h, raised) {
    box(mc, x, y, w, 2, INK, 0, 0, 0); box(mc, x, y + h - 2, w, 2, INK, 0, 0, 0);
    box(mc, x, y, 2, h, INK, 0, 0, 0); box(mc, x + w - 2, y, 2, h, INK, 0, 0, 0);
}
// a title bar in the theme colour (Byrott fades from orange to red)
function titleBar(mc, x, y, w, h) {
    if (C1 == C2) { box(mc, x, y, w, h, C1, 0, 0, 0); return; }
    var n = 10;
    for (var i = 0; i < n; i++)
        box(mc, x, y + Math.floor(i * h / n), w, Math.ceil(h / n) + 1, mix(C1, C2, i / (n - 1)), 0, 0, 0);
}
function circle(mc, cx, cy, r, fill) {
    mc.beginFill(fill, 100);
    mc.moveTo(cx + r, cy);
    for (var i = 1; i <= 24; i++) { var a = i / 24 * Math.PI * 2; mc.lineTo(cx + Math.cos(a) * r, cy + Math.sin(a) * r); }
    mc.endFill();
}
// the close box of a title bar: a white square with an X
function closeBox(mc, x, y, s) {
    box(mc, x, y, s, s, WHITE, INK, 2, 0);
    mc.lineStyle(2, INK, 100);
    mc.moveTo(x + 4, y + 4); mc.lineTo(x + s - 4, y + s - 4);
    mc.moveTo(x + s - 4, y + 4); mc.lineTo(x + 4, y + s - 4);
    mc.lineStyle();
}

// pictures kept in the system files
var bmpCache = {};
function sysBitmap(name) {
    if (bmpCache[name] == undefined) bmpCache[name] = FlashOS.loadBitmap(name);
    return bmpCache[name];
}
function logo(parent, name, depth, x, y, size, col) {
    var file = size > 140 ? "logo_big.png" : (size > 44 ? "logo_mid.png" : (size > 30 ? "logo_small.png" : "logo_tiny.png"));
    var b = sysBitmap(file);
    var mc = parent.createEmptyMovieClip(name, depth);
    mc._x = x; mc._y = y;
    if (!b) return mc;
    mc.attachBitmap(b, 1, "auto", true);
    mc._xscale = mc._yscale = size / b.width * 100;
    if (col != undefined && col != WHITE) new Color(mc).setRGB(col);
    return mc;
}
// icons from the mock-up: icon_<kind>.png (64 px) and icon_<kind>_s.png (32 px)
function icon(parent, name, depth, x, y, kind, small) {
    var mc = parent.createEmptyMovieClip(name, depth);
    mc._x = x; mc._y = y;
    var b = sysBitmap("icon_" + kind + (small ? "_s" : "") + ".png");
    if (b) mc.attachBitmap(b, 1, "auto", true);
    return mc;
}
function picture(parent, name, depth, x, y, file) {
    var mc = parent.createEmptyMovieClip(name, depth);
    mc._x = x; mc._y = y;
    var b = sysBitmap(file);
    if (b) mc.attachBitmap(b, 1, "auto", true);
    return mc;
}

// ---------------------------------------------------------------- buttons
// style 0 white box, 1 theme colour, 2 flat (lists), 3 danger (red)
function button(parent, name, depth, x, y, w, h, text, onClick, style, size) {
    var b = parent.createEmptyMovieClip(name, depth);
    b._x = x; b._y = y; b.w = w; b.h = h; b.style = style; b.on = false; b.enabledFlag = true;
    b.bg = b.createEmptyMovieClip("bg", 1);
    b.icon = b.createEmptyMovieClip("icon", 2);
    size = 16;
    var tc = style == 3 ? WHITE : (style == 1 ? onC1() : INK);
    b.lab = label(b, "lab", 3, 0, Math.round((h - 18) / 2), w, 22, text, size, tc, false, style == 2 ? "left" : "center");
    if (style == 2) b.lab._x = 12;
    b.paint = function (state) {
        var g = this.bg;
        g.clear();
        if (this.style == 1) box(g, 0, 0, this.w, this.h, state == 2 ? mix(C1, 0, 0.2) : C1, INK, 2, 0);
        else if (this.style == 3) box(g, 0, 0, this.w, this.h, state > 0 ? 0xE02020 : 0xC00000, INK, 2, 0);
        else if (this.style == 2) {
            box(g, 0, 0, this.w, this.h, this.on ? C3 : LIGHT, INK, this.on ? 2 : 0, 0);
            g._alpha = (state > 0 || this.on) ? 100 : 0;
        } else box(g, 0, 0, this.w, this.h, state == 2 ? 0xD8D8D8 : (state == 1 ? LIGHT : WHITE), INK, 2, 0);
    };
    b.setOn = function (v) { this.on = v; this.paint(0); };
    b.paint(0);
    b.onRollOver = function () { this.paint(1); };
    b.onRollOut = function () { this.paint(0); };
    b.onDragOut = function () { this.paint(0); };
    b.onPress = function () { this.paint(2); };
    b.onRelease = function () { this.paint(1); if (onClick) onClick(this); };
    return b;
}

// ---------------------------------------------------------------- layers
var wallClip = _root.createEmptyMovieClip("wall", 1);
var deskIcons = _root.createEmptyMovieClip("icons", 2);
var winLayer = _root.createEmptyMovieClip("wins", 10);
var taskbar = _root.createEmptyMovieClip("taskbar", 50);     // the start button
var menuLayer = _root.createEmptyMovieClip("menu", 60);
var dialogLayer = _root.createEmptyMovieClip("dialog", 70);
var splash = _root.createEmptyMovieClip("splash", 80);
var cursorClip = _root.createEmptyMovieClip("cursor", 100);

// ---------------------------------------------------------------- dialogs
// a small white window with a bar, like the mock-up's uninstall question
var dialogKeys = null;
function closeDialog() {
    dialogLayer.panel.removeMovieClip();
    dialogLayer.dim.removeMovieClip();
    dialogKeys = null;
}
function ask(title, msg, labels, done, danger) {
    closeMenu();
    var d = dialogLayer.createEmptyMovieClip("dim", 1);
    box(d, 0, 0, W, H, 0x000000, 0, 0, 0);
    d._alpha = 50;
    d.onPress = function () {};
    d.useHandCursor = false;
    var p = dialogLayer.createEmptyMovieClip("panel", 2);
    var pw = 600, ph = 260;
    p._x = (W - pw) / 2; p._y = (H - ph) / 2 - 30;
    var g = p.createEmptyMovieClip("g", 1);
    titleBar(g, 0, 0, pw, 28);
    box(g, 0, 30, pw, ph - 30, WHITE, INK, 2, 0);
    label(p, "t", 2, 8, 5, pw - 20, 22, title, 16, onC1(), false);
    var m = label(p, "m", 3, 28, 56, pw - 56, 120, msg, 16, INK, false);
    m.multiline = true;
    m.wordWrap = true;
    m.text = msg;
    var bw = 130, gap = 24, x = pw - 28 - labels.length * bw - (labels.length - 1) * gap;
    if (labels.length == 2) x = 40;
    for (var i = 0; i < labels.length; i++) {
        var bx = labels.length == 2 ? (i == 0 ? 40 : pw - 40 - bw) : x + i * (bw + gap);
        var b = button(p, "b" + i, 10 + i, bx, ph - 66, bw, 44, labels[i], null, 0, 16);
        b.idx = i;
        b.onRelease = function () { var k = this.idx; closeDialog(); if (done) done(k); };
    }
    dialogKeys = function (code) {
        if (code == 13) { closeDialog(); if (done) done(0); }
        if (code == 27) { closeDialog(); if (done) done(labels.length - 1); }
    };
}
function message(title, msg) { ask(title, msg, ["OK"], null); }

// ---------------------------------------------------------------- app windows
// One app at a time, in a window on pure black: a bar in the theme colour
// with the title and a close box, then a white body (black for the Command
// Runner and the Viewer). The desktop comes back when it is closed.
var wins = {};          // the open app, by id (at most one)
var winOrder = [];
var winDepth = 10;
var curApp = null;
var WBAR = 30, WGAP = 2, WPAD = 10;

function focusWin(w) {}
function makeWindow(id, title, ww, hh, dark, onClose) {
    if (curApp && curApp.id == id) return null;
    if (curApp) closeWindow(curApp);
    closeMenu();
    var w = winLayer.createEmptyMovieClip("w_" + id, ++winDepth);
    w.id = id; w.w = ww; w.h = hh; w.title = title; w.onCloseFn = onClose;
    var ow = ww + WPAD * 2, oh = WBAR + WGAP + hh + WPAD * 2;
    var wx = Math.round((W - ow) / 2), wy = Math.max(0, Math.round((H - oh) / 2));
    w.frame = w.createEmptyMovieClip("frame", 1);
    box(w.frame, 0, 0, W, H, 0x000000, 0, 0, 0);
    w.frame.onPress = function () {
        var f = Selection.getFocus();
        if (f == null || f.indexOf("w_" + w.id) < 0) Selection.setFocus(w.focusField ? w.focusField : null);
    };
    w.frame.useHandCursor = false;
    w.barClip = w.createEmptyMovieClip("bar", 2);
    w.bodyBg = w.createEmptyMovieClip("bodybg", 3);
    box(w.bodyBg, wx, wy + WBAR + WGAP, ow, hh + WPAD * 2, dark ? 0x000000 : BODY, dark ? 0x606060 : INK, 2, 0);
    w.titleTxt = label(w, "tt", 5, wx + 8, wy + 6, ow - 60, 22, title, 16, onC1(), false);
    w.body = w.createEmptyMovieClip("body", 10);
    w.body._x = wx + WPAD;
    w.body._y = wy + WBAR + WGAP + WPAD;
    w.paintBar = function () {
        this.barClip.clear();
        titleBar(this.barClip, wx, wy, ow, WBAR);
        closeBox(this.barClip, wx + ow - 30, wy + 4, 22);
        this.titleTxt.textColor = onC1();
    };
    var x = w.createEmptyMovieClip("x", 20);         // the close box takes the clicks
    box(x, wx + ow - 32, wy, 32, WBAR, 0xFFFFFF, 0, 0, 0);
    x._alpha = 0;
    x.onRelease = function () { closeWindow(w); };
    w.xb = x;
    w.paintBar();
    wins = {};
    wins[id] = w;
    winOrder = [w];
    curApp = w;
    taskbar._visible = false;
    deskIcons._visible = false;
    wallClip._visible = false;
    return w;
}
function closeWindow(w) {
    if (!w) return;
    if (w.onCloseFn) w.onCloseFn();
    if (Selection.getFocus() != null && Selection.getFocus().indexOf("w_" + w.id) >= 0) Selection.setFocus(null);
    w.removeMovieClip();
    if (curApp == w) { curApp = null; wins = {}; winOrder = []; }
    taskbar._visible = true;
    deskIcons._visible = true;
    wallClip._visible = true;
}
function repaintWindows() { for (var key in wins) wins[key].paintBar(); }

// ---------------------------------------------------------------- wallpaper
var wallCache = {};
function wallBitmap(name) {
    if (!wallCache[name]) wallCache[name] = FlashOS.loadBitmap(name);
    return wallCache[name];
}
function setWallpaper(name) {
    var b = wallBitmap(name);
    if (!b) {
        b = wallBitmap("logo-blue.jpg");
        if (!b) { wallClip.clear(); box(wallClip, 0, 0, W, H, 0x1A3A80, 0, 0, 0); return; }
        name = "logo-blue.jpg";
    }
    cfg.wallpaper = name;
    wallClip.attachBitmap(b, 1, "auto", true);
    var bm = wallClip.getInstanceAtDepth(1);
    var s = Math.max(W / b.width, H / b.height);
    bm._xscale = bm._yscale = s * 100;
    bm._x = (W - b.width * s) / 2;
    bm._y = (H - b.height * s) / 2;
}
function systemWallpapers() {
    var all = FlashOS.files("jpg,jpeg,png,gif,bmp"), out = [];
    for (var i = 0; i < all.length; i++) if (all[i].where == "system" && ext(all[i].name) == "jpg") out.push(all[i].name);
    return out;
}

// ---------------------------------------------------------------- desktop
// like the mock-up: the start button at the top left, the icons under it
var APPS = [["files", "File Manager"], ["ball", "Ball Game V1.0"]];
// the start menu
var MENU_APPS = [["settings", "Settings"], ["web", "Internet\n Navigator"], ["term", "Command\nRunner\n(FlashOS)"], ["notepad", "Notepad"]];
function launch(id) {
    closeMenu();
    if (id == "term") openTerminal("");
    else if (id == "files") openFiles();
    else if (id == "web") openWeb();
    else if (id == "notepad") openNotepad(null);
    else if (id == "settings") openSettings("");
    else if (id == "ball") runSWF("ballgame.swf", "");
}
function buildIcons() {
    for (var i = 0; i < APPS.length; i++) {
        var ic = deskIcons["i" + i];
        if (ic) continue;
        ic = deskIcons.createEmptyMovieClip("i" + i, i + 1);
        ic._x = 12; ic._y = 96 + i * 112;
        ic.hl = ic.createEmptyMovieClip("hl", 1);
        box(ic.hl, 0, 0, 150, 100, 0xFFFFFF, 0, 0, 0);
        ic.hl._alpha = 0;
        icon(ic, "g", 2, 43, 4, APPS[i][0], false);
        ic.sh = label(ic, "sh", 3, 1, 73, 150, 22, APPS[i][1], 16, INK, false, "center");
        ic.t = label(ic, "t", 4, 0, 72, 150, 22, APPS[i][1], 16, WHITE, false, "center");
        ic.app = APPS[i][0];
        ic.onRollOver = function () { this.hl._alpha = 25; };
        ic.onRollOut = function () { this.hl._alpha = 0; };
        ic.onRelease = function () { this.hl._alpha = 0; launch(this.app); };   // one click, like the mock-up
    }
}

// ---------------------------------------------------------------- the start button
var startBtn;
function paintTaskbar() {
    if (!startBtn) {
        startBtn = taskbar.createEmptyMovieClip("start", 3);
        startBtn._x = 12; startBtn._y = 12;
        startBtn.g = startBtn.createEmptyMovieClip("g", 1);
        picture(startBtn, "pic", 2, 0, 0, "start_btn.png");
        startBtn.onRelease = function () { if (menuOpen) closeMenu(); else openStart(); };
    }
    startBtn.g.clear();
    box(startBtn.g, 0, 0, 64, 60, ORANGE, 0, 0, 0);
}
function updateTasks() {}
function two(n) { return n < 10 ? "0" + n : "" + n; }
var DAYS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
var MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
function tick() {}

// ---------------------------------------------------------------- volume
function applyVolume() {
    if (FlashOS.setVolume) { FlashOS.setVolume(Number(cfg.volume)); FlashOS.setMuted(cfg.muted == "1"); }
}
function closeVolume() {}

// ---------------------------------------------------------------- start menu
// an orange panel that drops down from the start button
var menuOpen = false, MENU_W = 330;
function closeMenu() {
    if (!menuOpen) return;
    menuLayer.panel.removeMovieClip();
    menuLayer.catcher.removeMovieClip();
    menuOpen = false;
}
function menuRow(p, name, depth, y, h, kind, text, onClick) {
    var r = p.createEmptyMovieClip(name, depth);
    r._y = y;
    r.hl = r.createEmptyMovieClip("hl", 1);
    box(r.hl, 0, 0, MENU_W, h, MENU_HI, 0, 0, 0);
    r.hl._alpha = 0;
    icon(r, "ic", 2, 8, Math.round((h - 64) / 2), kind, false);
    var t = label(r, "t", 4, 82, 0, MENU_W - 84, h + 40, "", 32, 0xFFFFFF, false);
    t.multiline = true;
    t.text = text;
    t._y = Math.round((h - text.split("\n").length * 34) / 2) - 2;
    var sh = label(r, "sh", 3, 84, t._y + 2, MENU_W - 84, h + 40, "", 32, 0xC86400, false);
    sh.multiline = true;
    sh.text = text;
    r.onRollOver = function () { this.hl._alpha = 100; };
    r.onRollOut = function () { this.hl._alpha = 0; };
    r.onRelease = onClick;
    return r;
}
function openStart() {
    menuOpen = true;
    var c = menuLayer.createEmptyMovieClip("catcher", 1);
    box(c, 0, 0, W, H, 0xFFFFFF, 0, 0, 0);
    c._alpha = 0;
    c.onPress = function () { closeMenu(); };
    c.useHandCursor = false;
    var p = menuLayer.createEmptyMovieClip("panel", 2);
    var pw = MENU_W;
    p._x = 12; p._y = 72;
    var g = p.createEmptyMovieClip("g", 1);
    var y = 6, i;
    for (i = 0; i < MENU_APPS.length; i++) {
        var hh = Math.max(72, MENU_APPS[i][1].split("\n").length * 34 + 8);
        var r = menuRow(p, "a" + i, 10 + i, y, hh, MENU_APPS[i][0], MENU_APPS[i][1], null);
        r.app = MENU_APPS[i][0];
        r.onRelease = function () { launch(this.app); };
        y += hh;
    }
    // Volume: the speaker mutes, the slider sets the level
    var v = p.createEmptyMovieClip("vol", 30);
    v._y = y;
    var vi = icon(v, "ic", 2, 8, 4, cfg.muted == "1" ? "mute" : "volume", false);
    label(v, "sh", 3, 84, 2, 200, 40, "Volume", 32, 0xC86400, false);
    label(v, "t", 4, 82, 0, 200, 40, "Volume", 32, 0xFFFFFF, false);
    var pct = label(v, "pct", 5, 236, 12, 80, 22, "", 16, 0x7A3A00, false, "right");
    var TX = 84, TY = 46, TW = 226;
    var tr = v.createEmptyMovieClip("tr", 6);
    box(tr, TX, TY, TW, 12, WHITE, INK, 2, 0);
    var fill = v.createEmptyMovieClip("fill", 7);
    var th = v.createEmptyMovieClip("th", 8);
    box(th, -7, -6, 14, 24, ORANGE, INK, 2, 0);
    th._y = TY;
    function place() {
        var n = Number(cfg.volume);
        th._x = TX + n / 100 * TW;
        fill.clear();
        box(fill, TX + 2, TY + 2, Math.max(0, n / 100 * TW - 2), 8, cfg.muted == "1" ? 0xAAAAAA : ORANGE, 0, 0, 0);
        pct.text = cfg.muted == "1" ? "Mute" : cfg.volume + "%";
    }
    function setFromMouse() {
        var n = Math.round((v._xmouse - TX) / TW * 100);
        if (n < 0) n = 0;
        if (n > 100) n = 100;
        cfg.volume = "" + n;
        cfg.muted = "0";
        applyVolume();
        place();
    }
    place();
    var drag = v.createEmptyMovieClip("drag", 9);
    box(drag, TX - 10, TY - 10, TW + 20, 34, 0xFFFFFF, 0, 0, 0);
    drag._alpha = 0;
    drag.onPress = function () { setFromMouse(); v.onEnterFrame = function () { setFromMouse(); }; };
    drag.onRelease = drag.onReleaseOutside = function () { v.onEnterFrame = null; saveCfg(); };
    drag.useHandCursor = false;
    vi.onRelease = function () {
        cfg.muted = cfg.muted == "1" ? "0" : "1";
        applyVolume();
        saveCfg();
        var b = sysBitmap(cfg.muted == "1" ? "icon_mute.png" : "icon_volume.png");
        if (b) this.attachBitmap(b, 1, "auto", true);
        place();
    };
    y += 76;
    var rs = menuRow(p, "rs", 40, y, 72, "restart", "Restart", function () { closeMenu(); FlashOS.restart(); });
    y += 72;
    var sd = menuRow(p, "sd", 41, y, 72, "power", "Shut Down", function () { closeMenu(); FlashOS.shutdown(); });
    y += 78;
    g.clear();
    box(g, 0, 0, pw, y, MENU, 0, 0, 0);
}

// ---------------------------------------------------------------- cursor
var cursorGfx = cursorClip.createEmptyMovieClip("g", 1);
function drawCursor(kind, g) {
    g.clear();
    var c = CURSOR_COLS[kind] ? CURSOR_COLS[kind] : CURSOR_COLS.white;
    g.lineStyle(2, c[1], 100);
    g.beginFill(c[0], 100);
    g.moveTo(0, 0); g.lineTo(0, 22); g.lineTo(5, 17); g.lineTo(10, 26); g.lineTo(14, 24);
    g.lineTo(9, 15); g.lineTo(16, 15); g.lineTo(0, 0);
    g.endFill();
    g.lineStyle();
}
function applyCursor() {
    Mouse.hide();
    cursorClip._visible = true;
    drawCursor(cfg.cursor, cursorGfx);
    var c = CURSOR_COLS[cfg.cursor] ? CURSOR_COLS[cfg.cursor] : CURSOR_COLS.white;
    if (FlashOS.cursorColor) FlashOS.cursorColor(c[0], c[1]);   // the pointer of SWFs played in a window
}
var mouseL = {};
mouseL.onMouseMove = function () { cursorClip._x = _root._xmouse; cursorClip._y = _root._ymouse; };
Mouse.addListener(mouseL);

// ---------------------------------------------------------------- running SWFs (one at a time, in a window)
function runSWF(name, back) {
    FlashOS.runSWF(name, back, C1);
}

// ================================================================ TERMINAL
var termLines = [], termHist = [], termHi = 0, termOut, termIn;
function tprint(s) {
    var parts = s.split("\n");
    for (var i = 0; i < parts.length; i++) termLines.push(parts[i]);
    while (termLines.length > 300) termLines.shift();
    if (termOut) { termOut.text = termLines.join("\n"); termOut.scroll = termOut.maxscroll; }
}
function openTerminal(msg) {
    var w = makeWindow("term", "Command Runner", 900, 600, true, function () { termOut = null; termIn = null; });
    if (!w) return;
    var b = w.body;
    if (termLines.length == 0) { termLines.push("Digit An Command For Run Program"); termLines.push("Or Insert an Flash-disk"); termLines.push("(type HELP for the commands)"); termLines.push(""); }
    termOut = label(b, "out", 1, 0, 0, 900, 560, "", 16, 0xFFFFFF, false, "left", MONO);
    termOut.multiline = true;
    termOut.wordWrap = true;
    label(b, "pr", 2, 0, 574, 50, 22, "dev/", 16, 0xFFFFFF, false, "left", MONO);
    termIn = b.createTextField("in", 3, 32, 574, 866, 22);
    termIn = b["in"];
    termIn.embedFonts = true;
    termIn.type = "input";
    termIn.setNewTextFormat(fmt(16, 0xFFFFFF, false, "left", MONO));
    termIn.maxChars = 100;
    termIn.text = "";
    termIn.maxChars = 120;
    w.focusField = termIn;
    if (msg != "") tprint(msg);
    termOut.text = termLines.join("\n");
    termOut.scroll = termOut.maxscroll;
    Selection.setFocus(termIn);
}
function findFile(n) {
    var all = FlashOS.files(""), low = n.toLowerCase();
    for (var i = 0; i < all.length; i++) if (all[i].name.toLowerCase() == low) return all[i];
    for (i = 0; i < all.length; i++) if (all[i].name.toLowerCase() == low + ".swf") return all[i];
    return null;
}
function termCommand(line) {
    tprint("dev/" + line);
    var sp = line.indexOf(" ");
    var cmd = (sp < 0 ? line : line.substr(0, sp)).toLowerCase(), arg = sp < 0 ? "" : line.substr(sp + 1);
    if (cmd == "") return;
    if (cmd == "help") tprint("  LS            list files\n  RUN <file>    play a SWF in a window (or just type its name)\n  OPEN <file>   open a file in a ByroTT app\n  FILES, INTERNET, NOTEPAD, SETTINGS, BALL   open an app\n  THEME <default|orange|green|blue|byrott>   window theme\n  MEM           free memory\n  DATE          date and time\n  ABOUT         about ByroTT\n  CLS           clear the screen\n  EXIT          close the terminal");
    else if (cmd == "files") openFiles();
    else if (cmd == "internet" || cmd == "web") openWeb();
    else if (cmd == "notepad") openNotepad(null);
    else if (cmd == "settings") openSettings("");
    else if (cmd == "ball") launch("ball");
    else if (cmd == "theme" || cmd == "color" || cmd == "colour") {
        var cn = arg.toLowerCase();
        if (!THEMES[cn]) tprint("  Themes: default, orange, green, blue, byrott");
        else { cfg.theme = cn; applyColor(); saveCfg(); repaintWindows(); tprint("  Windows theme: " + THEMES[cn][2]); }
    }
    else if (cmd == "ls" || cmd == "dir") {
        var all = FlashOS.files("");
        for (var i = 0; i < all.length; i++) {
            if (all[i].where == "system" && isSystemArt(all[i].name)) continue;
            var n = all[i].name;
            while (n.length < 30) n += " ";
            var s = "" + Math.ceil(all[i].size / 1024);
            while (s.length < 7) s = " " + s;
            tprint("  " + n + s + " KB  " + all[i].where.toUpperCase());
        }
        if (all.length == 0) tprint("  No files.");
    } else if (cmd == "cls" || cmd == "clear") { termLines = []; tprint(""); }
    else if (cmd == "mem") tprint("  " + Math.round(FlashOS.memoryKB() / 1024) + " MB free for apps.");
    else if (cmd == "date" || cmd == "time") { var d = new Date(); tprint("  " + DAYS[d.getDay()] + " " + d.getDate() + " " + MONTHS[d.getMonth()] + " " + d.getFullYear() + "  " + two(d.getHours()) + ":" + two(d.getMinutes()) + ":" + two(d.getSeconds())); }
    else if (cmd == "about" || cmd == "ver") tprint("  ByroTT 1.0 on FlashOS " + FlashOS.version + " (ByroTT edition).\n  Files are saved on the " + (FlashOS.storage() == "disk" ? "hard disk." : "memory (not installed)."));
    else if (cmd == "exit") closeWindow(wins["term"]);
    else if (cmd == "open") { var f = findFile(arg); if (f) openFile(f.name); else tprint("  File not found: " + arg); }
    else if (cmd == "run" || cmd == "play") termRun(arg);
    else termRun(line);
}
function termRun(name) {
    if (name == "") { tprint("  Usage: RUN <file.swf>"); return; }
    var f = findFile(name);
    if (!f) { tprint("  Unknown command or file: " + name + "   (type HELP or LS)"); return; }
    if (f.name.toLowerCase().lastIndexOf(".swf") != f.name.length - 4) {
        tprint("  ACCESS DENIED: " + f.name + " is not a SWF file.");
        return;
    }
    tprint("  Loading " + f.name + " ...");
    FlashOS.runSWF(f.name, "terminal|" + f.name, C1);
}

// ================================================================ NOTEPAD
var noteName = "", noteTF = null, noteDirty = false;
function openNotepad(file) {
    var w = wins["notepad"];
    if (!w) {
        w = makeWindow("notepad", "Notepad", 940, 620, false, function () { noteTF = null; });
        var b = w.body;
        button(b, "new", 1, 0, 8, 90, 34, "New", function () { noteTF.text = ""; noteName = ""; noteDirty = false; noteTitle(); }, 0, 15);
        button(b, "save", 2, 96, 8, 90, 34, "Save", function () { noteSave(false); }, 1, 15);
        button(b, "saveas", 3, 192, 8, 110, 34, "Save as...", function () { noteSave(true); }, 0, 15);
        b.createTextField("txt", 4, 0, 52, 940, 568);
        noteTF = b.txt;
        noteTF.embedFonts = true;
        noteTF.type = "input";
        noteTF.multiline = true;
        noteTF.wordWrap = true;
        noteTF.border = true;
        noteTF.background = true;
        noteTF.backgroundColor = FIELD;
        noteTF.borderColor = EDGE;
        noteTF.setNewTextFormat(fmt(18, INK, false));
        noteTF.text = "";
        noteTF.onChanged = function () { if (!noteDirty) { noteDirty = true; noteTitle(); } };
        w.focusField = noteTF;
    } else focusWin(w);
    if (file) {
        var s = FlashOS.read(file);
        if (s == null) { message("Notepad", "Sorry, " + file + " could not be read."); return; }
        noteTF.text = s;
        noteName = file;
        noteDirty = false;
    }
    noteTitle();
    Selection.setFocus(noteTF);
}
function noteTitle() {
    var w = wins["notepad"];
    if (w) w.titleTxt.text = (noteDirty ? "* " : "") + (noteName == "" ? "untitled.txt" : noteName) + " - Notepad";
}
function noteSave(askName) {
    if (askName || noteName == "") {
        prompt("Save as", "File name:", noteName == "" ? "note.txt" : noteName, function (n) {
            if (n.toLowerCase().lastIndexOf(".txt") != n.length - 4) n += ".txt";
            noteName = n;
            noteSave(false);
        });
        return;
    }
    if (FlashOS.write(noteName, noteTF.text)) { noteDirty = false; noteTitle(); }
    else message("Notepad", "Sorry, the file could not be saved.");
}
// a small question with a text box
function prompt(title, text, def, done) {
    ask(title, text, ["OK", "Cancel"], function (k) { if (k == 0) done(dialogLayer.answer); });
    var p = dialogLayer.panel;
    var f = input(p, "inp", 30, 24, 104, 492, 36, 18);
    f.text = def;
    f.maxChars = 40;
    dialogLayer.answer = def;
    f.onChanged = function () { dialogLayer.answer = f.text; };
    Selection.setFocus(f);
    Selection.setSelection(0, def.length);
}

// ================================================================ VIEWER (pictures and videos)
var vid = null;
function openViewer(file) {
    if (wins["viewer"]) closeWindow(wins["viewer"]);
    var w = makeWindow("viewer", file, 820, 620, true, function () { if (vid) { vid.close(); vid = null; } });
    var b = w.body;
    var low = file.toLowerCase();
    var pic = b.createEmptyMovieClip("pic", 1);
    if (low.lastIndexOf(".flv") == low.length - 4) {
        vid = FlashOS.openVideo(file);
        if (!vid) { label(b, "e", 2, 20, 200, 780, 40, "Sorry, ByroTT cannot play this video.", 18, WHITE, false, "center"); return; }
        pic.attachBitmap(vid.bitmap, 1, "auto", true);
        fitInto(pic, vid.width, vid.height, 8, 8, 804, 520);
        var pb = button(b, "play", 3, 12, 536, 110, 38, "Pause", function () { if (vid.playing) { vid.pause(); pb.lab.text = "Play"; } else { vid.play(); pb.lab.text = "Pause"; } }, 1, 15);
        button(b, "stop", 4, 128, 536, 90, 38, "Stop", function () { vid.stop(); pb.lab.text = "Play"; }, 0, 15);
        var tt = label(b, "time", 5, 600, 542, 200, 28, "", 16, WHITE, true, "right");
        b.onEnterFrame = function () { if (vid) tt.text = Math.floor(vid.time) + " / " + Math.floor(vid.duration) + " s"; };
        vid.play();
        return;
    }
    var bmp = FlashOS.loadBitmap(file);
    if (!bmp) { label(b, "e", 2, 20, 200, 780, 40, "Sorry, ByroTT cannot show this picture.", 18, WHITE, false, "center"); return; }
    pic.attachBitmap(bmp, 1, "auto", true);
    fitInto(pic, bmp.width, bmp.height, 8, 8, 804, 524);
    button(b, "wall", 3, 12, 540, 200, 36, "Use as wallpaper", function () { setWallpaper(file); saveCfg(); }, 1, 15);
    label(b, "info", 4, 400, 546, 400, 26, bmp.width + " x " + bmp.height + " pixels", 15, 0xB8C4CC, false, "right");
}
function fitInto(mc, w, h, x, y, bw, bh) {
    var s = Math.min(bw / w, bh / h);
    if (s > 4) s = 4;
    mc._xscale = mc._yscale = s * 100;
    mc._x = x + (bw - w * s) / 2;
    mc._y = y + (bh - h * s) / 2;
}

function ext(n) { var k = n.lastIndexOf("."); return k < 0 ? "" : n.substr(k + 1).toLowerCase(); }
function openFile(n) {
    var e = ext(n);
    if (e == "swf") runSWF(n, "files");
    else if (e == "txt" || e == "log" || e == "md" || e == "cfg" || e == "csv" || e == "ini") openNotepad(n);
    else if (e == "jpg" || e == "jpeg" || e == "png" || e == "gif" || e == "bmp" || e == "flv") openViewer(n);
    else if (e == "htm" || e == "html") { openWeb(); webShowHtml(FlashOS.read(n), n); }
    else message("Files", "ByroTT has no app for ." + e + " files.");
}

// ================================================================ FILES
var filesList = [], filesSel = -1, filesFirst = 0, filesPlace = "all";
var PLACES = [["all", "Everything"], ["disk", "Hard disk"], ["cd", "CD"], ["usb", "USB stick"], ["system", "ByroTT system"], ["net", "Downloads"]];
// the mock-up's File Manager: three folders first
function openFiles() {
    var w = makeWindow("files", "File Manager", 880, 580, false, null);
    if (!w) return;
    filesHome(w);
}
function gbFree() {
    if (FlashOS.storage() != "disk") return "Memory (not installed)";
    var gb = Math.floor(FlashOS.freeKB() / 1048576 * 100) / 100, t = "" + gb, k = t.indexOf(".");
    if (k < 0) t += ",00"; else { t = t.substr(0, k) + "," + t.substr(k + 1); if (t.length - k < 3) t += "0"; }
    return "Hard Disk (" + t + ")GB free";
}
function filesPage(w) {
    if (w.body.home) w.body.home.removeMovieClip();
    var c = w.body.createEmptyMovieClip("home", 200);
    box(c, -10, -10, 900, 600, WHITE, 0, 0, 0);
    return c;
}
function filesFolder(c, name, depth, y, kind, text, onClick) {
    var r = c.createEmptyMovieClip(name, depth);
    r._x = 10; r._y = y;
    r.hl = r.createEmptyMovieClip("hl", 1);
    box(r.hl, 0, 0, 840, 80, C3, 0, 0, 0);
    r.hl._alpha = 0;
    icon(r, "ic", 2, 6, 8, kind, false);
    label(r, "t", 3, 86, 22, 740, 36, text, 32, INK, false);
    r.onRollOver = function () { this.hl._alpha = 100; };
    r.onRollOut = function () { this.hl._alpha = 0; };
    r.onRelease = onClick;
}
function filesHome(w) {
    var c = filesPage(w);
    filesFolder(c, "hd", 1, 10, "files", gbFree(), function () { w.body.home.removeMovieClip(); filesBuildList(w); });
    filesFolder(c, "xb", 2, 100, "files", "X Boot", function () {
        filesNote(w, "Unable to View this folder\nThis folder contains\nSystem's Boot Factor, if you\ndelete them system may not be able\nto boot properly or Potentially\nBrick your PC", null);
    });
    filesFolder(c, "ad", 3, 190, "admin", "Admin", function () { filesPrivate(w); });
}
function filesPrivate(w) {
    var c = filesPage(w);
    var fr = c.createEmptyMovieClip("fr", 1);
    box(fr, 10, 10, 840, 540, WHITE, INK, 3, 0);
    filesFolder(c, "pf", 2, 30, "admin", "Private Folder", function () {
        filesNote(w, "Nothing to see here.\n(click to return\nto main folder)", null);
    });
}
function filesNote(w, text) {
    var c = filesPage(w);
    var fr = c.createEmptyMovieClip("fr", 1);
    box(fr, 10, 10, 840, 540, WHITE, INK, 3, 0);
    var t = label(c, "t", 2, 40, 0, 780, 400, "", 32, INK, false, "center");
    t.multiline = true;
    t.text = text;
    t._y = Math.round(280 - text.split("\n").length * 17);
    c.onRelease = function () { filesHome(w); };
}
function filesBuildList(w) {
    var b = w.body;
    var side = b.createEmptyMovieClip("side", 1);
    box(side, 0, 0, 190, 540, LIGHT, INK, 2, 0);
    w.places = [];
    for (var i = 0; i < PLACES.length; i++) {
        var pb = button(b, "p" + i, 10 + i, 8, 12 + i * 44, 174, 40, PLACES[i][1], null, 2, 15);
        pb.place = PLACES[i][0];
        pb.onRelease = function () { filesPlace = this.place; filesSel = -1; filesFirst = 0; filesReload(); };
        w.places.push(pb);
    }
    var y = 488;
    button(b, "back", 29, 8, y, 174, 38, "< Back", function () { closeWindow(w); openFiles(); }, 0, 16);
    button(b, "open", 30, 206, y, 100, 38, "Open", function () { if (filesSel >= 0) openFile(filesList[filesSel].name); }, 1, 15);
    button(b, "copy", 31, 312, y, 160, 38, "Copy to disk", function () { filesCopy(); }, 0, 15);
    button(b, "del", 32, 478, y, 100, 38, "Delete", function () { filesDelete(); }, 0, 15);
    button(b, "up", 33, 760, y, 50, 38, "^", function () { if (filesFirst > 0) { filesFirst -= 10; if (filesFirst < 0) filesFirst = 0; filesDraw(); } }, 0, 16);
    button(b, "dn", 34, 816, y, 50, 38, "v", function () { if (filesFirst + 10 < filesList.length) { filesFirst += 10; filesDraw(); } }, 0, 15);
    w.info = label(b, "info", 35, 590, y + 8, 166, 24, "", 13, GREY, false, "right");
    var head = b.createEmptyMovieClip("head", 40);
    box(head, 200, 8, 668, 30, LIGHT, INK, 2, 0);
    label(b, "h1", 41, 250, 12, 300, 24, "Name", 14, GREY, true);
    label(b, "h2", 42, 600, 12, 100, 24, "Size", 14, GREY, true, "right");
    label(b, "h3", 43, 730, 12, 120, 24, "Where", 14, GREY, true);
    filesReload();
}
function filesReload() {
    var all = FlashOS.files("");
    filesList = [];
    for (var i = 0; i < all.length; i++) {
        var wh = all[i].where == "memory" ? "disk" : all[i].where;
        if ((filesPlace == "all" || wh == filesPlace) && !(all[i].where == "system" && isSystemArt(all[i].name))) filesList.push(all[i]);
    }
    var w = wins["files"];
    if (!w) return;
    for (var k = 0; k < w.places.length; k++) w.places[k].setOn(w.places[k].place == filesPlace);
    filesDraw();
}
function kb(n) { if (n < 1024) return n + " B"; if (n < 1048576) return Math.round(n / 1024) + " KB"; return Math.round(n / 104857.6) / 10 + " MB"; }
var WHERE = {disk: "Hard disk", memory: "Memory", cd: "CD", usb: "USB", system: "ByroTT", net: "Download"};
function fileKind(n) {
    var e = ext(n);
    if (e == "swf") return "swf";
    if (e == "txt" || e == "log" || e == "md" || e == "cfg" || e == "ini" || e == "csv") return "txt";
    if (e == "jpg" || e == "jpeg" || e == "png" || e == "gif" || e == "bmp") return "img";
    if (e == "flv") return "video";
    if (e == "iso") return "disc";
    return "file";
}
// the pictures ByroTT itself is drawn with are not shown as files
function isSystemArt(n) { return n.indexOf("icon_") == 0 || n.indexOf("logo_") == 0 || n == "start_btn.png" || n == "web_offline.png"; }
function filesDraw() {
    var w = wins["files"];
    if (!w) return;
    var b = w.body;
    for (var r = 0; r < 10; r++) {
        var i = filesFirst + r;
        var row = b["r" + r];
        if (!row) {
            row = b.createEmptyMovieClip("r" + r, 50 + r);
            row._x = 200; row._y = 44 + r * 44;
            row.bg = row.createEmptyMovieClip("bg", 1);
            row.ic = null;
            row.n = label(row, "n", 3, 50, 9, 330, 26, "", 16, INK, false);
            row.s = label(row, "s", 4, 380, 10, 120, 24, "", 14, GREY, false, "right");
            row.wh = label(row, "wh", 5, 530, 10, 130, 24, "", 14, GREY, false);
            row.r = r;
            row.last = 0;
            row.onRelease = function () {
                var k = filesFirst + this.r;
                if (k >= filesList.length) return;
                if (k == filesSel && getTimer() - this.last < 500) { openFile(filesList[k].name); return; }
                filesSel = k;
                this.last = getTimer();
                filesDraw();
            };
        }
        row.bg.clear();
        if (i < filesList.length) {
            var f = filesList[i];
            box(row.bg, 0, 0, 668, 42, i == filesSel ? C3 : (r % 2 ? 0xEDEDED : 0xF7F7F7), i == filesSel ? C1 : 0, i == filesSel ? 2 : 0, 0);
            icon(row, "ic", 2, 10, 5, fileKind(f.name), true);
            row.n.text = f.name;
            row.s.text = kb(f.size);
            row.wh.text = WHERE[f.where] ? WHERE[f.where] : f.where;
            row._visible = true;
        } else row._visible = false;
    }
    w.info.text = filesList.length + (filesList.length == 1 ? " file" : " files");
}
function filesCopy() {
    if (filesSel < 0) { message("Files", "Pick a file first."); return; }
    var f = filesList[filesSel];
    if (f.where == "disk" || f.where == "memory") { message("Files", f.name + " is already on the hard disk."); return; }
    if (FlashOS.copyFile(f.name, f.name)) { message("Files", f.name + " was copied to the hard disk."); filesReload(); }
    else message("Files", "Sorry, " + f.name + " could not be copied. The disk may be full.");
}
function filesDelete() {
    if (filesSel < 0) { message("Files", "Pick a file first."); return; }
    var f = filesList[filesSel];
    if (f.where != "disk" && f.where != "memory") { message("Files", "Only files on the hard disk can be deleted. " + f.name + " is on the " + (WHERE[f.where] ? WHERE[f.where] : f.where) + "."); return; }
    ask("Delete " + f.name + "?", "Do you really want to delete " + f.name + "? This cannot be undone.", ["Delete", "Cancel"], function (k) {
        if (k == 0) { FlashOS.remove(f.name); filesSel = -1; filesReload(); }
    }, true);
}

// ================================================================ SETTINGS
// like the mock-up: one list, click a row to change it
function wallName(f) { var n = f.substr(0, f.lastIndexOf(".")); return n.substr(0, 1).toUpperCase() + n.substr(1); }
function openSettings(pg) {
    if (!wins["settings"]) makeWindow("settings", "System Settings", 760, 520, false, null);
    settingsPage("");
}
function setRow(c, name, depth, y, kind, text, onClick) {
    var r = c.createEmptyMovieClip(name, depth);
    r._y = y;
    r.hl = r.createEmptyMovieClip("hl", 1);
    var t = label(r, "t", 3, 82, 0, 660, 160, "", 32, INK, false);
    t.multiline = true;
    t.text = text;
    var lines = text.split("\n").length;
    var h = Math.max(72, lines * 34 + 8);
    box(r.hl, 0, 0, 760, h, C3, 0, 0, 0);
    r.hl._alpha = 0;
    icon(r, "ic", 2, 6, Math.round((h - 64) / 2), kind, false);
    t._y = Math.round((h - lines * 34) / 2) - 2;
    r.h = h;
    if (onClick) {
        r.onRollOver = function () { this.hl._alpha = 100; };
        r.onRollOut = function () { this.hl._alpha = 0; };
        r.onRelease = onClick;
    } else r.useHandCursor = false;
    return r;
}
function nextOf(list, v) { for (var i = 0; i < list.length; i++) if (list[i] == v) return list[(i + 1) % list.length]; return list[0]; }
function settingsPage(pg) {
    var w = wins["settings"];
    if (!w) return;
    var c = w.body.createEmptyMovieClip("page", 50);   // replaces the old page
    var y = 0, r;
    r = setRow(c, "cur", 1, y, "pointer", "Change Pointer Color\n(" + CURSOR_NAMES[cfg.cursor] + ")", function () {
        cfg.cursor = nextOf(CURSORS, cfg.cursor); applyCursor(); saveCfg(); settingsPage("");
    });
    y += r.h + 4;
    r = setRow(c, "thm", 2, y, "theme", "Change Windows Theme\n(" + THEMES[cfg.theme][2] + ")", function () {
        cfg.theme = nextOf(THEME_NAMES, cfg.theme); applyColor(); saveCfg(); repaintWindows(); settingsPage("");
    });
    y += r.h + 4;
    r = setRow(c, "wal", 3, y, "wall", "Change Wallpaper\n(" + wallName(cfg.wallpaper) + ")", function () {
        setWallpaper(nextOf(systemWallpapers(), cfg.wallpaper)); saveCfg(); settingsPage("");
    });
    y += r.h + 4;
    r = setRow(c, "del", 4, y, "delete", "Delete Byrott", function () { uninstallNow(); });
    y += r.h + 4;
    r = setRow(c, "abt", 5, y, "about", "About Byrott 1.0\nByrott Is Not Affilated\nWith FlashOS At All\n(c) 2000 Byrott/FlashOS", null);
}
function paintFrameColors(w) { w.paintBar(); }
// the mock-up's uninstall: a question, "Deleting...", then "You Can Reboot"
function uninstallNow() {
    ask("Delete Byrott", "Do You Want Uninstall This OS,\nIf You Uninstall it it maybe use FlashOS\nUnusable, Do you want Proceed?",
        ["Yes", "No"], function (k) {
            if (k != 0) return;
            ask("Delete Byrott", "Deleting... PLEASE DO NOT SHUT DOWN!", [], null);
            setTimeout(function () {
                var r = FlashOS.uninstall();
                closeDialog();
                if (r == "ok") {
                    FlashOS.remove("byrott.cfg");
                    ask("Delete Byrott", "You Can Reboot Your PC Safely Now\n\nWhat Files Got Deleted:\nSystem Settings Files, Temp Files, Boot Manager\n(your personal files are kept)",
                        ["Reboot"], function () { FlashOS.restart(); });
                } else if (r != "cancelled") message("Delete Byrott", "Byrott could not be deleted: " + r + ".");
            }, 300);
        });
}

// ================================================================ INTERNET
var WEB_W = 900, WEB_VIEW = 470;
var START_PAGE = "<title>ByroTT start page</title><h1>Welcome to the Internet</h1>" +

    "<form action=\"http://frogfind.com/\"><p><b>Search the web with FrogFind:</b> <input name=\"q\"> <input type=\"submit\" value=\"Search\"></p></form>" +
    "<ul><li><a href=\"http://frogfind.com/\">FrogFind</a> - a search engine that makes modern pages simple</li>" +
    "<li><a href=\"http://68k.news/\">68k.news</a> - today's news for vintage computers</li>" +
    "<li><a href=\"http://info.cern.ch/\">info.cern.ch</a> - the first web site ever</li>" +
    "<li><a href=\"http://example.com/\">example.com</a></li></ul>" +
    "<p>Tip: click a link, or type an address at the top and press Enter.</p>";
var STYLE = [[16, INK, FONT], [32, INK, FONT], [32, INK, FONT], [16, INK, FONT], [16, 0x0000CC, FONT], [16, INK, MONO], [16, GREY, FONT], [16, INK, FONT]];
var charW = [], lineHt = [], measured = false;
var webItems, webLines, webScroll = 0, webUrl = "", webHist = [], webTitle = "", webForms = [];
var webPage = null, webBar = null, webThumb = null, webStatus = null, webUrlF = null, netOk = false;

function measureFonts() {
    if (measured) return;
    measured = true;
    _root.createTextField("measure", 9999, -2000, -2000, 2000, 100);
    var m = _root.measure;
    m.embedFonts = true;
    m.selectable = false;
    for (var s = 0; s < STYLE.length; s++) {
        var f = fmt(STYLE[s][0], 0, false, "left", STYLE[s][2]);
        m.setNewTextFormat(f);
        var arr = [];
        for (var c = 0; c < 256; c++) {
            if (c < 32 || (c >= 127 && c < 160)) { arr[c] = 0; continue; }
            var ch = String.fromCharCode(c);
            m.text = ch + ch + ch + ch + ch + ch + ch + ch + ch + ch;
            arr[c] = (m.textWidth + 0.5) / 10;
        }
        charW[s] = arr;
        lineHt[s] = Math.round(STYLE[s][0] * 1.45);
    }
}
function textW(s, st) {
    var w = 0, cw = charW[st];
    for (var i = 0; i < s.length; i++) { var c = s.charCodeAt(i); w += c < 256 ? cw[c] : cw[63]; }
    return w;
}

// ---- HTML to words
var ENT = {amp: "&", lt: "<", gt: ">", quot: "\"", apos: "'", nbsp: " ", copy: "©", reg: "®", mdash: "-", ndash: "-",
           hellip: "...", rsquo: "'", lsquo: "'", ldquo: "\"", rdquo: "\"", laquo: "«", raquo: "»", middot: "·",
           eacute: "é", egrave: "è", agrave: "à", ograve: "ò", ugrave: "ù", igrave: "ì", euro: "EUR", bull: "·"};
function ents(s) {
    if (s.indexOf("&") < 0) return s;
    var out = "", i = 0;
    while (i < s.length) {
        var a = s.indexOf("&", i);
        if (a < 0) { out += s.substr(i); break; }
        out += s.substring(i, a);
        var b = s.indexOf(";", a);
        if (b < 0 || b - a > 9) { out += "&"; i = a + 1; continue; }
        var e = s.substring(a + 1, b), r = null;
        if (e.charAt(0) == "#") {
            var code = e.charAt(1) == "x" || e.charAt(1) == "X" ? parseInt(e.substr(2), 16) : parseInt(e.substr(1), 10);
            r = code < 256 ? String.fromCharCode(code) : (code == 8217 || code == 8216 ? "'" : (code == 8220 || code == 8221 ? "\"" : (code == 8211 || code == 8212 ? "-" : "?")));
        } else r = ENT[e.toLowerCase()];
        if (r == null) { out += "&"; i = a + 1; continue; }
        out += r;
        i = b + 1;
    }
    return out;
}
function attr(tag, name) {
    var low = tag.toLowerCase(), k = 0;
    while (true) {
        k = low.indexOf(name + "=", k);
        if (k < 0) return null;
        var pc = low.charAt(k - 1);
        if (k > 0 && (pc == " " || pc == "\n" || pc == "\t" || pc == "\r")) break;
        k++;
    }
    var v = k + name.length + 1, q = tag.charAt(v), end;
    if (q == "\"" || q == "'") { end = tag.indexOf(q, v + 1); if (end < 0) end = tag.length; return ents(tag.substring(v + 1, end)); }
    end = v;
    while (end < tag.length && tag.charAt(end) != " " && tag.charAt(end) != ">") end++;
    return ents(tag.substring(v, end));
}
var BREAKS = {br: 1, div: 1, tr: 1, table: 1, ul: 1, ol: 1, section: 1, article: 1, header: 1, footer: 1, nav: 1, center: 1, blockquote: 1, dl: 1, dt: 1, dd: 1, form: 1, main: 1, aside: 1, figure: 1};
function parseHtml(html) {
    var items = [], i = 0, n = html.length, bold = 0, href = null, pre = 0, skip = 0, head = 0, inTitle = false, form = null, space = false;
    webTitle = "";
    webForms = [];
    function word(s, st) { items.push({t: "w", s: s, st: st, href: href, sp: space}); space = true; }
    while (i < n) {
        var lt = html.indexOf("<", i);
        if (lt < 0) lt = n;
        if (lt > i && skip == 0) {
            var txt = html.substring(i, lt);
            if (inTitle) webTitle += txt;
            else {
                txt = ents(txt);
                var st = pre ? 5 : (head == 1 ? 1 : (head == 2 ? 2 : (head >= 3 ? 7 : (href != null ? 4 : (bold > 0 ? 3 : 0)))));
                if (href != null && head == 0 && pre == 0) st = 4;
                if (pre) {
                    var pl = txt.split("\n");
                    for (var q = 0; q < pl.length; q++) {
                        if (q > 0) items.push({t: "br"});
                        if (pl[q].length) { space = false; word(pl[q].split("\t").join("    "), 5); }
                    }
                } else {
                    var c0 = txt.charAt(0);
                    if (c0 == " " || c0 == "\n" || c0 == "\r" || c0 == "\t") space = true;
                    var ws = txt.split("\n").join(" ").split("\r").join(" ").split("\t").join(" ").split(" ");
                    var first = true;
                    for (var k = 0; k < ws.length; k++) {
                        if (ws[k] == "") continue;
                        first = false;
                        word(ws[k], st);
                    }
                    var cl = txt.charAt(txt.length - 1);
                    space = cl == " " || cl == "\n" || cl == "\r" || cl == "\t";
                }
            }
        }
        if (lt >= n) break;
        if (html.substr(lt, 4) == "<!--") { var ce = html.indexOf("-->", lt + 4); i = ce < 0 ? n : ce + 3; continue; }
        var gt = html.indexOf(">", lt);
        if (gt < 0) break;
        var tag = html.substring(lt + 1, gt);
        i = gt + 1;
        var close = tag.charAt(0) == "/";
        var nm = (close ? tag.substr(1) : tag).toLowerCase();
        var e = 0;
        while (e < nm.length && nm.charAt(e) != " " && nm.charAt(e) != "\n" && nm.charAt(e) != "/" && nm.charAt(e) != "\t") e++;
        nm = nm.substr(0, e);
        if (nm == "script" || nm == "style" || nm == "noscript" || nm == "svg") { skip += close ? -1 : 1; if (skip < 0) skip = 0; continue; }
        if (nm == "title") { inTitle = !close; continue; }
        if (skip) continue;
        if (nm == "h1" || nm == "h2" || nm == "h3" || nm == "h4" || nm == "h5" || nm == "h6") {
            items.push({t: "para"});
            head = close ? 0 : parseInt(nm.substr(1));
            space = false;
        } else if (nm == "p") { items.push({t: "para"}); space = false; }
        else if (BREAKS[nm]) { items.push({t: "br"}); space = false; if (nm == "form") form = close ? null : {action: attr(tag, "action"), hidden: []}; if (form && !close) webForms.push(form); }
        else if (nm == "b" || nm == "strong") bold += close ? -1 : 1;
        else if (nm == "a") href = close ? null : attr(tag, "href");
        else if (nm == "li" && !close) { items.push({t: "br"}); space = false; word("·", bold ? 3 : 0); }
        else if (nm == "hr") items.push({t: "hr"});
        else if (nm == "pre") { pre = close ? 0 : 1; items.push({t: "br"}); }
        else if (nm == "td" || nm == "th") { if (!close) space = true; }
        else if (nm == "img" && !close) { var alt = attr(tag, "alt"); if (alt != null && alt != "") word("[" + alt + "]", 6); }
        else if (nm == "input" && !close) {
            var ty = attr(tag, "type");
            ty = ty == null ? "text" : ty.toLowerCase();
            var nmv = attr(tag, "name"), val = attr(tag, "value");
            if (ty == "hidden") { if (form) form.hidden.push([nmv, val == null ? "" : val]); }
            else if (ty == "submit" || ty == "button" || ty == "image") items.push({t: "btn", label: val == null ? "Go" : val, form: form});
            else if (ty == "text" || ty == "search" || ty == "email" || ty == "url" || ty == "number") items.push({t: "in", name: nmv, value: val == null ? "" : val, form: form});
        } else if (nm == "button" && !close) items.push({t: "btn", label: "Search", form: form});
        else if (nm == "textarea" && !close) items.push({t: "in", name: attr(tag, "name"), value: "", form: form});
    }
    return items;
}

// ---- words to lines
function layoutPage(items) {
    var lines = [], segs = [], x = 0, maxW = WEB_W - 70, h = lineHt[0], lastBlank = true;
    function flush() {
        if (segs.length == 0) { return; }
        lines.push({segs: segs, h: h});
        segs = []; x = 0; h = lineHt[0]; lastBlank = false;
    }
    for (var i = 0; i < items.length; i++) {
        var it = items[i];
        if (it.t == "br") { if (segs.length) flush(); }
        else if (it.t == "para") { flush(); if (!lastBlank) { lines.push({segs: [], h: 10}); lastBlank = true; } }
        else if (it.t == "hr") { flush(); lines.push({segs: [], h: 18, hr: true}); }
        else if (it.t == "w") {
            var st = it.st, ww = textW(it.s, st), sp = (it.sp && x > 0) ? charW[st][32] : 0;
            if (x > 0 && x + sp + ww > maxW) { flush(); sp = 0; }
            var last = segs.length ? segs[segs.length - 1] : null;
            if (last && last.kind == "t" && last.st == st && last.href == it.href) { last.text += (sp > 0 ? " " : "") + it.s; last.w = x + sp + ww - last.x; }
            else segs.push({kind: "t", x: x + sp, w: ww, text: it.s, st: st, href: it.href});
            x += sp + ww;
            if (lineHt[st] > h) h = lineHt[st];
        } else if (it.t == "in") {
            if (x > 0 && x + 310 > maxW) flush();
            segs.push({kind: "in", x: x + 4, w: 300, name: it.name, value: it.value, form: it.form});
            x += 310;
            if (h < 36) h = 36;
        } else if (it.t == "btn") {
            var bw = textW(it.label, 3) + 34;
            if (x > 0 && x + bw + 8 > maxW) flush();
            segs.push({kind: "btn", x: x + 6, w: bw, label: it.label, form: it.form});
            x += bw + 10;
            if (h < 36) h = 36;
        }
    }
    flush();
    var y = 0;
    for (var k = 0; k < lines.length; k++) { lines[k].y = y; y += lines[k].h; }
    return lines;
}

// ---- drawing only the lines in view
function webRender() {
    if (!webPage) return;
    var total = webLines.length ? webLines[webLines.length - 1].y + webLines[webLines.length - 1].h : 0;
    var maxS = Math.max(0, total - WEB_VIEW);
    if (webScroll > maxS) webScroll = maxS;
    if (webScroll < 0) webScroll = 0;
    for (var i = 0; i < webLines.length; i++) {
        var L = webLines[i];
        var vis = L.y >= webScroll && L.y + L.h <= webScroll + WEB_VIEW;
        if (vis && !L.mc) webMakeLine(L, i);
        if (L.mc) { L.mc._visible = vis; L.mc._y = L.y - webScroll; }
    }
    webThumb._visible = maxS > 0;
    if (maxS > 0) {
        var th = Math.max(30, WEB_VIEW * WEB_VIEW / total);
        webThumb.clear();
        box(webThumb, 0, 0, 12, th, C1, 0, 0, 0);
        webThumb._y = (WEB_VIEW - th) * webScroll / maxS;
    }
}
function webMakeLine(L, idx) {
    var mc = webPage.createEmptyMovieClip("l" + idx, idx + 1);
    L.mc = mc;
    if (L.hr) { box(mc, 0, 8, WEB_W - 40, 1, EDGE, 0, 0, 0); return; }
    for (var s = 0; s < L.segs.length; s++) {
        var g = L.segs[s];
        if (g.kind == "t") {
            var st = STYLE[g.st];
            if (g.href != null) {
                var lk = mc.createEmptyMovieClip("a" + s, s + 1);
                lk._x = g.x;
                var t = label(lk, "t", 1, -2, L.h - lineHt[g.st] - 2, g.w * 1.04 + 16, lineHt[g.st] + 4, g.text, st[0], st[1], false, "left", st[2]);
                box(lk, 0, L.h - 4, g.w, 1, 0x1A4FC0, 0, 0, 0);
                lk.href = g.href;
                lk.onRelease = function () { webGo(webResolve(this.href)); };
            } else label(mc, "t" + s, s + 1, g.x - 2, L.h - lineHt[g.st] - 2, g.w * 1.04 + 16, lineHt[g.st] + 4, g.text, st[0], st[1], false, "left", st[2]);
        } else if (g.kind == "in") {
            var f = input(mc, "i" + s, s + 1, g.x, 2, g.w, 32, 16);
            f.text = g.value;
            g.field = f;
        } else {
            var fo = g.form, segs = L.segs;
            var b = button(mc, "b" + s, s + 1, g.x, 1, g.w, 34, g.label, null, 1, 15);
            b.form = fo;
            b.onRelease = function () { webSubmit(this.form); };
        }
    }
}
function webSubmit(form) {
    var action = form && form.action ? form.action : webUrl;
    var q = "";
    for (var i = 0; i < webLines.length; i++) {
        var segs = webLines[i].segs;
        for (var s = 0; s < segs.length; s++)
            if (segs[s].kind == "in" && segs[s].form == form && segs[s].name) {
                var v = segs[s].field ? segs[s].field.text : segs[s].value;
                q += (q == "" ? "" : "&") + segs[s].name + "=" + escape(v).split("%20").join("+");
            }
    }
    if (form) for (var h = 0; h < form.hidden.length; h++) if (form.hidden[h][0]) q += (q == "" ? "" : "&") + form.hidden[h][0] + "=" + escape(form.hidden[h][1]);
    var base = webResolve(action);
    var k = base.indexOf("?");
    if (k >= 0) base = base.substr(0, k);
    webGo(base + "?" + q);
}
function webResolve(h) {
    if (h == null) return webUrl;
    var low = h.toLowerCase();
    if (low.indexOf("http://") == 0 || low.indexOf("https://") == 0) return h;
    if (h.indexOf("//") == 0) return "http:" + h;
    var u = webUrl, p = u.indexOf("/", 7);
    var root = p < 0 ? u : u.substr(0, p);
    if (h.charAt(0) == "/") return root + h;
    if (h.charAt(0) == "#") return webUrl;
    var path = p < 0 ? "/" : u.substr(p);
    var qk = path.indexOf("?");
    if (qk >= 0) path = path.substr(0, qk);
    if (h.charAt(0) == "?") return root + path + h;
    return root + path.substr(0, path.lastIndexOf("/") + 1) + h;
}

function webShowHtml(html, url) {
    if (wins["web"] && wins["web"].body.offline) wins["web"].body.offline.removeMovieClip();
    if (!wins["web"]) openWeb();
    if (webPage) webPage.removeMovieClip();
    webPage = wins["web"].body.createEmptyMovieClip("page", 5);
    webPage._x = 20; webPage._y = 56;
    measureFonts();
    if (html.indexOf("<") < 0) html = "<pre>" + html.split("&").join("&amp;").split("<").join("&lt;") + "</pre>";
    webItems = parseHtml(html);
    webLines = layoutPage(webItems);
    webScroll = 0;
    webUrl = url;
    webUrlF.text = url;
    var w = wins["web"];
    w.titleTxt.text = (webTitle != "" ? ents(webTitle) : url) + " - Internet Navigator 1.0";
    webRender();
}
function webError(url, msg) {
    webShowHtml("<h2>Sorry, this page cannot be opened</h2><p>" + msg + "</p><p><a href=\"byrott:start\">Back to the start page</a></p>", url);
}
function webGo(url) {
    if (url == "byrott:start") { webShowHtml(START_PAGE, "byrott:start"); return; }
    if (url.toLowerCase().indexOf("https://") == 0) {
        webError(url, "This site uses https://, which ByroTT cannot open. Try the same site with http://, or search for it with <a href=\"http://frogfind.com/\">FrogFind</a>.");
        return;
    }
    if (url.toLowerCase().indexOf("http://") != 0) url = "http://" + url;
    if (webUrl != "" && webUrl != url) webHist.push(webUrl);
    webStatus.text = "Loading " + url + " ...";
    webUrlF.text = url;
    setTimeout(function () { webFetch(url); }, 60);   // let the status show first
}
function webFetch(url) {
    if (!netOk) {
        var n = FlashOS.netConnect();
        if (n != "ok") {
            webStatus.text = "";
            webOffline(n);
            return;
        }
        netOk = true;
    }
    var r = FlashOS.httpGet(url);
    webStatus.text = "";
    if (r.status == 0) { webError(url, "The page could not be loaded: " + r.error + "."); return; }
    if (r.status >= 400) { webError(url, "The server answered with error " + r.status + "."); return; }
    webShowHtml(r.body, url);
    webStatus.text = "Done";
}
// the mock-up's "no internet" page
function webOffline(why) {
    webShowHtml("<h1>Please Connect Your PC to an FlashRouter</h1>", "byrott:offline");
    var w = wins["web"];
    if (!w) return;
    var pic = picture(w.body, "offline", 60, 160, 150, "web_offline.png");
    label(pic, "why", 1, 0, 270, 600, 22, "(" + why + ". VirtualBox: Settings, Network, NAT.)", 16, GREY, false);
}
function openWeb() {
    var w = makeWindow("web", "Internet Navigator 1.0", WEB_W + 20, 620, false, function () { webPage = null; });
    if (!w) return;
    var b = w.body;
    var tb = b.createEmptyMovieClip("tb", 1);
    box(tb, 0, 0, WEB_W + 20, 50, LIGHT, INK, 2, 0);
    button(b, "back", 2, 10, 8, 70, 34, "Back", function () { if (webHist.length) { var u = webHist.pop(); webUrl = ""; webGo(u); webHist.pop(); } }, 0, 15);
    button(b, "home", 3, 86, 8, 70, 34, "Home", function () { webGo("byrott:start"); }, 0, 15);
    webUrlF = input(b, "url", 4, 166, 8, WEB_W - 250, 34, 16);
    button(b, "go", 6, WEB_W - 76, 8, 86, 34, "Go", function () { webGo(webUrlF.text); }, 1, 15);
    var bar = b.createEmptyMovieClip("bar", 7);
    bar._x = WEB_W + 2; bar._y = 56;
    box(bar, 0, 0, 12, WEB_VIEW, 0xC8C8C8, 0, 0, 0);
    webThumb = bar.createEmptyMovieClip("th", 1);
    bar.onPress = function () { webScroll += (this._ymouse > webThumb._y ? 1 : -1) * (WEB_VIEW - 40); webRender(); };
    webStatus = label(b, "st", 8, 12, 542, 600, 24, "", 13, GREY, false);
    var dn = button(b, "dn", 9, WEB_W - 76, 534, 86, 32, "Down", function () { webScroll += WEB_VIEW - 60; webRender(); }, 0, 14);
    var up = button(b, "up", 10, WEB_W - 168, 534, 86, 32, "Up", function () { webScroll -= WEB_VIEW - 60; webRender(); }, 0, 14);
    w.focusField = null;
    webHist = [];
    webUrl = "";
    webShowHtml(START_PAGE, "byrott:start");
}

// ================================================================ keyboard
var keyL = {};
keyL.onKeyDown = function () {
    var code = Key.getCode();
    if (dialogKeys) { dialogKeys(code); return; }
    if (menuOpen && code == 27) { closeMenu(); return; }
    var f = Selection.getFocus();
    if (Key.isDown(17) && code == 84) { openTerminal(""); return; }   // Ctrl+T
    if (termIn && f != null && f.indexOf("in") >= 0 && f.indexOf("w_term") >= 0) {
        if (code == 13) {
            var line = termIn.text;
            termIn.text = "";
            if (line != "") { termHist.push(line); termHi = termHist.length; }
            termCommand(line);
        } else if (code == 38 && termHist.length) { termHi = Math.max(0, termHi - 1); termIn.text = termHist[termHi]; Selection.setSelection(termIn.length, termIn.length); }
        else if (code == 40 && termHist.length) { termHi = Math.min(termHist.length, termHi + 1); termIn.text = termHi < termHist.length ? termHist[termHi] : ""; }
        return;
    }
    if (webPage && f != null && f.indexOf("w_web") >= 0) {
        if (code == 13) {
            if (f.indexOf("url") >= 0) webGo(webUrlF.text);
            else for (var i = 0; i < webForms.length; i++) { webSubmit(webForms[i]); break; }
        }
        return;
    }
    if (webPage && f == null && wins["web"] && wins["web"].getDepth() == winDepth) {
        if (code == 40) { webScroll += 40; webRender(); }
        if (code == 38) { webScroll -= 40; webRender(); }
        if (code == 34 || code == 32) { webScroll += WEB_VIEW - 60; webRender(); }
        if (code == 33) { webScroll -= WEB_VIEW - 60; webRender(); }
    }
};
Key.addListener(keyL);

// ================================================================ start
function startDesktop() {
    loadCfg();
    setWallpaper(cfg.wallpaper);
    buildIcons();
    paintTaskbar();
    applyVolume();
    applyCursor();
    tick();
    var arg = FlashOS.launchArg ? FlashOS.launchArg() : "";
    var parts = arg.split("|");
    if (parts[0] == "terminal") {
        var msg = parts.length > 1 && parts[1] != "" ? "  " + parts[1] + " finished." : "";
        if (parts.length > 2) msg = "  " + parts[1] + ": " + parts[2];
        openTerminal(msg);
    } else if (parts[0] == "files") {
        openFiles();
        if (parts.length > 2) message("Files", parts[1] + ": " + parts[2]);
    }
}

// loading screen: "Loading..." on black, then the desktop (no animation)
var sp = splash;
box(sp, 0, 0, W, H, 0x000000, 0, 0, 0);
// tall, narrow white letters in the middle, like an old console
var spC = sp.createEmptyMovieClip("lt", 2);
var spT = label(spC, "t", 1, 0, 0, 640, 120, "Loading...", 84, WHITE, true, "center", BIG);
spC._xscale = 78; spC._yscale = 125;
spC._x = Math.round((W - 640 * 0.78) / 2); spC._y = Math.round(H / 2 - 70);
sp.onPress = function () {};
sp.useHandCursor = false;
cursorClip._visible = false;
var spT0 = getTimer(), spDone = false;
sp.onEnterFrame = function () {
    if (!spDone && getTimer() - spT0 > 150) { spDone = true; startDesktop(); cursorClip._visible = false; }
    if (spDone && getTimer() - spT0 > 1500) { sp.removeMovieClip(); cursorClip._visible = true; }
};
