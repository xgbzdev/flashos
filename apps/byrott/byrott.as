// ByroTT 3.0 - made to look like its Scratch mock-up (Byrott_1.sb3)
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
var cfg = {wallpaper: "pyramids-planet.jpg", theme: "default", cursor: "white", volume: "80", muted: "0", pass: "",
           dock: "files,web,notepad,music,store,settings", games: "", uname: "User", upic: "bird", upass: "", saver: "2"};
var DEFAULT_WALL = "pyramids-planet.jpg";
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
    var t = "";
    for (var k in cfg) t += k + "=" + cfg[k] + "\n";
    FlashOS.write("byrott.cfg", t);
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
// the 4.0 logo is a black bird with white lines: colour the lines only
function logoTint(mc, col) {
    new Color(mc).setTransform({ra: ((col >> 16) & 255) / 2.55, ga: ((col >> 8) & 255) / 2.55, ba: (col & 255) / 2.55,
                                aa: 100, rb: 0, gb: 0, bb: 0, ab: 0});
}
function logo(parent, name, depth, x, y, size, col) {
    var file = size > 140 ? "logo_big.png" : (size > 44 ? "logo_mid.png" : (size > 30 ? "logo_small.png" : "logo_tiny.png"));
    var b = sysBitmap(file);
    var mc = parent.createEmptyMovieClip(name, depth);
    mc._x = x; mc._y = y;
    if (!b) return mc;
    mc.attachBitmap(b, 1, "auto", true);
    mc._xscale = mc._yscale = size / b.width * 100;
    if (col != undefined && col != WHITE) logoTint(mc, col);
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
// ByroTT 3.0 multitasks: every app has its own window over the desktop.
// Drag a window by its title bar; "_" hides it (its button stays in the
// taskbar), the close box closes it. The window in front has a bar in the
// theme colour, the others a grey one.
var wins = {};          // the open windows, by app id
var winOrder = [];      // in the order they were opened
var winDepth = 10;
var curApp = null;      // the window in front
var winCount = 0;
var WBAR = 30, WGAP = 2, WPAD = 10;
var TOP_H = 44, DOCK_H = 84;            // the taskbar (top) and the dock (bottom)

function topWindow() {
    var best = null;
    for (var i = 0; i < winOrder.length; i++) {
        var w = winOrder[i];
        if (w._visible && (!best || w.getDepth() > best.getDepth())) best = w;
    }
    return best;
}
function focusWin(w) {
    if (!w) return;
    var old = curApp;
    curApp = w;
    w._visible = true;
    w.minimized = false;
    w.swapDepths(++winDepth);
    if (old && old != w && wins[old.id] == old) old.paintBar();
    w.paintBar();
    var f = Selection.getFocus();
    if (f == null || f.indexOf("w_" + w.id) < 0) Selection.setFocus(w.focusField ? w.focusField : null);
    updateTasks();
}
function minimizeWin(w) {
    w._visible = false;
    w.minimized = true;
    var f = Selection.getFocus();
    if (f != null && f.indexOf("w_" + w.id) >= 0) Selection.setFocus(null);
    if (curApp == w) { curApp = topWindow(); if (curApp) curApp.paintBar(); }
    updateTasks();
}
function makeWindow(id, title, ww, hh, dark, onClose) {
    if (wins[id]) { focusWin(wins[id]); return null; }
    closeMenu();
    closeCalendar();
    var w = winLayer.createEmptyMovieClip("w_" + id, ++winDepth);
    w.id = id; w.w = ww; w.h = hh; w.title = title; w.onCloseFn = onClose; w.minimized = false;
    var ow = ww + WPAD * 2, oh = WBAR + WGAP + hh + WPAD * 2;
    w.ow = ow; w.oh = oh;
    // a little down and right of the last one
    var room = H - TOP_H - DOCK_H, n = winCount++ % 6;
    var x = Math.round((W - ow) / 2) - 50 + n * 24, y = TOP_H + Math.max(0, Math.round((room - oh) / 2)) - 30 + n * 24;
    if (x + ow > W) x = W - ow;
    if (x < 0) x = 0;
    if (y + oh > H - DOCK_H) y = H - DOCK_H - oh;
    if (y < TOP_H) y = TOP_H;
    w._x = x; w._y = y;
    var shadow = w.createEmptyMovieClip("shadow", 0);
    box(shadow, 6, 6, ow, oh, 0x000000, 0, 0, 0);
    shadow._alpha = 35;
    w.frame = w.createEmptyMovieClip("frame", 1);
    box(w.frame, -2, -2, ow + 4, oh + 4, INK, 0, 0, 0);    // the black frame
    w.barClip = w.createEmptyMovieClip("bar", 2);
    w.bodyBg = w.createEmptyMovieClip("bodybg", 3);
    box(w.bodyBg, 0, WBAR + WGAP, ow, hh + WPAD * 2, dark ? 0x000000 : BODY, 0, 0, 0);
    w.titleTxt = label(w, "tt", 5, 8, 6, ow - 90, 22, title, 16, onC1(), false);
    w.body = w.createEmptyMovieClip("body", 10);
    w.body._x = WPAD;
    w.body._y = WBAR + WGAP + WPAD;
    w.paintBar = function () {
        var on = curApp == this;
        this.barClip.clear();
        if (on) titleBar(this.barClip, 0, 0, this.ow, WBAR);
        else box(this.barClip, 0, 0, this.ow, WBAR, 0xA8A8A8, 0, 0, 0);
        closeBox(this.barClip, this.ow - 28, 4, 22);
        // the minimize box: white with a line at the bottom
        box(this.barClip, this.ow - 56, 4, 22, 22, WHITE, INK, 2, 0);
        box(this.barClip, this.ow - 51, 19, 12, 3, INK, 0, 0, 0);
        this.titleTxt.textColor = on ? onC1() : 0x303030;
    };
    // the title bar drags the window
    var dr = w.createEmptyMovieClip("drag", 4);
    box(dr, 0, 0, ow - 60, WBAR, 0xFFFFFF, 0, 0, 0);
    dr._alpha = 0;
    dr.useHandCursor = false;
    dr.onPress = function () {
        focusWin(w);
        w.startDrag(false, 60 - ow, TOP_H, W - 60, H - WBAR - 8);
    };
    dr.onRelease = dr.onReleaseOutside = function () { w.stopDrag(); };
    var mn = w.createEmptyMovieClip("mn", 20);
    box(mn, ow - 58, 0, 28, WBAR, 0xFFFFFF, 0, 0, 0);
    mn._alpha = 0;
    mn.onRelease = function () { minimizeWin(w); };
    var x2 = w.createEmptyMovieClip("x", 21);
    box(x2, ow - 30, 0, 30, WBAR, 0xFFFFFF, 0, 0, 0);
    x2._alpha = 0;
    x2.onRelease = function () { closeWindow(w); };
    w.xb = x2;
    wins[id] = w;
    winOrder.push(w);
    focusWin(w);
    return w;
}
function closeWindow(w) {
    if (!w) return;
    if (w.onCloseFn) w.onCloseFn();
    if (Selection.getFocus() != null && Selection.getFocus().indexOf("w_" + w.id) >= 0) Selection.setFocus(null);
    if (wins[w.id] == w) delete wins[w.id];
    for (var i = 0; i < winOrder.length; i++) if (winOrder[i] == w) { winOrder.splice(i, 1); break; }
    w.removeMovieClip();
    if (curApp == w) { curApp = topWindow(); if (curApp) { curApp.paintBar(); var ff = curApp.focusField; if (ff) Selection.setFocus(ff); } }
    updateTasks();
}
function repaintWindows() { for (var i = 0; i < winOrder.length; i++) winOrder[i].paintBar(); }
function isFront(id) { return curApp != null && curApp == wins[id]; }

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

// ---------------------------------------------------------------- apps
// id: name, icon
var APPDEF = {files: ["File Manager", "files"], web: ["Internet Navigator", "web"], term: ["Command Runner", "term"],
              notepad: ["Notepad", "notepad"], paint: ["Paint", "paint"], music: ["Music Player", "music"],
              settings: ["Settings", "settings"], store: ["More Apps", "store"], ball: ["Ball Game", "ball"],
              bin: ["Recycle Bin", "bin"], snake: ["Snake", "snake"], mines: ["Minesweeper", "mines"],
              solitaire: ["Solitaire", "solitaire"], viewer: ["Viewer", "img"]};
var BUILTIN = ["files", "web", "term", "notepad", "paint", "music", "ball", "store", "settings", "bin"];
var STORE_GAMES = ["snake", "mines", "solitaire"];
// the apps on this computer: the built-in ones and the games from the Store
function installedApps() {
    var out = BUILTIN.slice(0), g = cfg.games == "" ? [] : cfg.games.split(",");
    for (var i = 0; i < g.length; i++) if (APPDEF[g[i]]) out.push(g[i]);
    return out;
}
function appIcon(id) { return APPDEF[id] ? (id == "bin" && binCount() > 0 ? "bin_full" : APPDEF[id][1]) : "file"; }
function launch(id) {
    closeMenu();
    if (wins[id] && id != "viewer") { focusWin(wins[id]); return; }
    if (id == "term") openTerminal("");
    else if (id == "files") openFiles();
    else if (id == "web") openWeb();
    else if (id == "notepad") openNotepad(null);
    else if (id == "paint") openPaint(null);
    else if (id == "settings") openSettings("");
    else if (id == "music") openMusic();
    else if (id == "store") openStore();
    else if (id == "bin") openBin();
    else if (id == "ball") openBall();
    else if (id == "snake") openSnake();
    else if (id == "mines") openMines();
    else if (id == "solitaire") openSolitaire();
}

// ---------------------------------------------------------------- desktop shortcuts
// icons on the desktop: apps or files. Drag them where you like; drop one
// on the Recycle Bin to take it off the desktop. Saved in byrott.desk.
var desk = [];          // {kind: "app" or "file", target, x, y}
function deskDefaults() {
    return [{kind: "app", target: "files", x: 12, y: 56}, {kind: "app", target: "ball", x: 12, y: 168},
            {kind: "app", target: "store", x: 12, y: 280}, {kind: "app", target: "bin", x: 12, y: 392}];
}
function deskLoad() {
    var t = FlashOS.read("byrott.desk");
    desk = [];
    if (t == null) { desk = deskDefaults(); return; }
    var ls = t.split("\n");
    for (var i = 0; i < ls.length; i++) {
        var p = ls[i].split("\t");
        if (p.length >= 4) desk.push({kind: p[0], target: p[1], x: Number(p[2]), y: Number(p[3])});
    }
}
function deskSave() {
    var t = "";
    for (var i = 0; i < desk.length; i++) t += desk[i].kind + "\t" + desk[i].target + "\t" + desk[i].x + "\t" + desk[i].y + "\n";
    FlashOS.write("byrott.desk", t);
}
function deskHas(kind, target) { for (var i = 0; i < desk.length; i++) if (desk[i].kind == kind && desk[i].target == target) return true; return false; }
// the first free place on the grid (columns of 112 px)
function deskFree() {
    for (var cx = 12; cx < W - 150; cx += 156)
        for (var cy = 56; cy < H - DOCK_H - 100; cy += 112) {
            var used = false;
            for (var i = 0; i < desk.length; i++) if (Math.abs(desk[i].x - cx) < 60 && Math.abs(desk[i].y - cy) < 60) used = true;
            if (!used) return [cx, cy];
        }
    return [12, 56];
}
function deskAdd(kind, target) {
    if (deskHas(kind, target)) return false;
    var p = deskFree();
    desk.push({kind: kind, target: target, x: p[0], y: p[1]});
    deskSave();
    buildIcons();
    return true;
}
function deskRemove(kind, target) {
    for (var i = 0; i < desk.length; i++) if (desk[i].kind == kind && desk[i].target == target) { desk.splice(i, 1); deskSave(); buildIcons(); return; }
}
function deskLabel(d) {
    if (d.kind == "app") return APPDEF[d.target] ? APPDEF[d.target][0] : d.target;
    var n = d.target, k = n.lastIndexOf("/");
    return k >= 0 ? n.substr(k + 1) : n;
}
function buildIcons() {
    // rebuild them all (cheap: a few clips)
    for (var k = 0; k < 64; k++) if (deskIcons["i" + k]) deskIcons["i" + k].removeMovieClip();
    for (var i = 0; i < desk.length && i < 64; i++) {
        var d = desk[i];
        var ic = deskIcons.createEmptyMovieClip("i" + i, i + 1);
        ic._x = d.x; ic._y = d.y;
        ic.hl = ic.createEmptyMovieClip("hl", 1);
        box(ic.hl, 0, 0, 150, 100, 0xFFFFFF, 0, 0, 0);
        ic.hl._alpha = 0;
        icon(ic, "g", 2, 43, 4, d.kind == "app" ? appIcon(d.target) : fileKind(d.target), false);
        if (d.kind == "file") { var ar = ic.createEmptyMovieClip("ar", 5); box(ar, 43, 50, 16, 16, WHITE, INK, 2, 0); ar.lineStyle(2, INK, 100); ar.moveTo(47, 62); ar.lineTo(55, 54); ar.moveTo(50, 54); ar.lineTo(55, 54); ar.lineTo(55, 59); }
        var lb = deskLabel(d);
        ic.sh = label(ic, "sh", 3, 1, 73, 150, 22, lb, 16, INK, false, "center");
        ic.t = label(ic, "t", 4, 0, 72, 150, 22, lb, 16, WHITE, false, "center");
        ic.d = d;
        ic.onRollOver = function () { this.hl._alpha = 25; };
        ic.onRollOut = function () { this.hl._alpha = 0; };
        ic.onPress = function () { this.sx = this._x; this.sy = this._y; this.swapDepths(200); this.startDrag(false, 0, TOP_H, W - 150, H - DOCK_H - 96); };
        ic.onRelease = ic.onReleaseOutside = function () {
            this.stopDrag();
            this.hl._alpha = 0;
            var d2 = this.d;
            if (Math.abs(this._x - this.sx) < 5 && Math.abs(this._y - this.sy) < 5) {     // a click: open it
                this._x = this.sx; this._y = this.sy;
                if (d2.kind == "app") launch(d2.target); else openFile(d2.target);
                return;
            }
            // dropped on the Recycle Bin: off the desktop
            for (var j = 0; j < desk.length; j++) {
                var o = desk[j];
                if (o != d2 && o.kind == "app" && o.target == "bin" && Math.abs(o.x - this._x) < 70 && Math.abs(o.y - this._y) < 60) {
                    if (d2.kind == "app" && d2.target == "bin") break;
                    deskRemove(d2.kind, d2.target);
                    return;
                }
            }
            d2.x = Math.round(this._x); d2.y = Math.round(this._y);
            deskSave();
        };
    }
}

// ---------------------------------------------------------------- the taskbar (top)
// the start button, a button for each open window, and the clock
var startBtn, tbBg, taskBtns = null;
function paintTaskbar() {
    if (!tbBg) {
        tbBg = taskbar.createEmptyMovieClip("bg", 1);
        startBtn = taskbar.createEmptyMovieClip("start", 3);
        startBtn._x = 4; startBtn._y = 4;
        startBtn.g = startBtn.createEmptyMovieClip("g", 1);
        startBtn.ar = startBtn.createEmptyMovieClip("ar", 2);   // its own clip: overlapping fills would cancel the click
        startBtn.onRelease = function () { if (menuOpen) closeMenu(); else openStart(); };
    }
    tbBg.clear();
    box(tbBg, 0, 0, W, TOP_H, WHITE, 0, 0, 0);
    box(tbBg, 0, TOP_H - 2, W, 2, INK, 0, 0, 0);
    var g = startBtn.g;
    g.clear();
    box(g, 0, 0, 44, 36, C1, INK, 2, 0);
    // the white down arrow of the mock-up's start button, in the theme's text colour
    g = startBtn.ar;
    g.clear();
    g.beginFill(onC1(), 100);
    g.moveTo(17, 6); g.lineTo(27, 6); g.lineTo(27, 18); g.lineTo(34, 18); g.lineTo(22, 31); g.lineTo(10, 18); g.lineTo(17, 18); g.lineTo(17, 6);
    g.endFill();
    updateTasks();
    paintDock();
}
function updateTasks() {
    if (!tbBg) return;
    if (taskBtns) taskBtns.removeMovieClip();
    taskBtns = taskbar.createEmptyMovieClip("tasks", 4);
    var n = winOrder.length;
    if (n == 0) return;
    var x0 = 56, room = W - 230 - x0, bw = Math.min(190, Math.floor(room / n) - 4);
    for (var i = 0; i < n; i++) {
        var w = winOrder[i];
        var b = taskBtns.createEmptyMovieClip("b" + i, i + 1);
        b._x = x0 + i * (bw + 4); b._y = 4;
        var on = curApp == w && !w.minimized;
        box(b, 0, 0, bw, 36, on ? C3 : (w.minimized ? 0xE0E0E0 : WHITE), INK, on ? 3 : 2, 0);
        icon(b, "ic", 1, 4, 2, appIcon(w.id), true);
        var t = w.titleTxt.text;
        label(b, "t", 2, 40, 9, bw - 44, 22, t, 16, w.minimized ? GREY : INK, false);
        b.w = w;
        b.onRelease = function () {
            if (curApp == this.w && !this.w.minimized) minimizeWin(this.w);
            else focusWin(this.w);
        };
    }
}
function two(n) { return n < 10 ? "0" + n : "" + n; }
var DAYS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
var MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
// the clock: at the right of the taskbar; click it for the calendar
var clockClip = null, clockT, dateT, lastMin = -1, calOpen = false, calMonth, calYear;
var MONTH_NAMES = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
function buildClock() {
    clockClip = taskbar.createEmptyMovieClip("clock", 5);
    clockClip._x = W - 172; clockClip._y = 4;
    box(clockClip, 0, 0, 168, 36, WHITE, INK, 2, 0);
    clockT = label(clockClip, "t", 1, 6, 9, 50, 22, "", 16, INK, false);
    dateT = label(clockClip, "d", 2, 52, 9, 112, 22, "", 16, GREY, false, "right");
    clockClip.onRelease = function () { if (calOpen) closeCalendar(); else openCalendar(); };
    lastMin = -1;
    tick();
}
function tick() {
    if (!clockClip) return;
    var d = new Date();
    if (d.getMinutes() == lastMin) return;
    lastMin = d.getMinutes();
    clockT.text = two(d.getHours()) + ":" + two(d.getMinutes());
    dateT.text = DAYS[d.getDay()] + " " + d.getDate() + " " + MONTHS[d.getMonth()];
}
setInterval(tick, 1000);
function closeCalendar() {
    if (!calOpen) return;
    menuLayer.cal.removeMovieClip();
    menuLayer.calCatch.removeMovieClip();
    calOpen = false;
}
function openCalendar() {
    closeMenu();
    var d = new Date();
    calMonth = d.getMonth(); calYear = d.getFullYear();
    calOpen = true;
    var c = menuLayer.createEmptyMovieClip("calCatch", 5);
    box(c, 0, 0, W, H, 0xFFFFFF, 0, 0, 0);
    c._alpha = 0;
    c.onPress = function () { closeCalendar(); };
    c.useHandCursor = false;
    drawCalendar();
}
function drawCalendar() {
    var p = menuLayer.createEmptyMovieClip("cal", 6);    // replaces the old one
    var pw = 300, ph = 300;
    p._x = W - 4 - pw; p._y = TOP_H;
    var g = p.createEmptyMovieClip("g", 1);
    titleBar(g, 0, 0, pw, 28);
    box(g, 0, 30, pw, ph - 30, WHITE, INK, 2, 0);
    label(p, "m", 2, 0, 5, pw, 22, MONTH_NAMES[calMonth] + " " + calYear, 16, onC1(), false, "center");
    button(p, "pv", 3, 4, 2, 28, 24, "<", function () { calMonth--; if (calMonth < 0) { calMonth = 11; calYear--; } drawCalendar(); }, 0, 16);
    button(p, "nx", 4, pw - 32, 2, 28, 24, ">", function () { calMonth++; if (calMonth > 11) { calMonth = 0; calYear++; } drawCalendar(); }, 0, 16);
    var heads = ["Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"], cw = 40, x0 = 10;
    for (var i = 0; i < 7; i++) label(p, "h" + i, 10 + i, x0 + i * cw, 38, cw, 22, heads[i], 16, i >= 5 ? 0xC00000 : GREY, false, "center");
    var first = new Date(calYear, calMonth, 1).getDay();          // 0 = Sunday
    first = (first + 6) % 7;                                       // 0 = Monday
    var days = new Date(calYear, calMonth + 1, 0).getDate();
    var now = new Date(), today = (now.getFullYear() == calYear && now.getMonth() == calMonth) ? now.getDate() : 0;
    var hl = p.createEmptyMovieClip("hl", 5);
    for (var dd = 1; dd <= days; dd++) {
        var k = first + dd - 1, cx = x0 + (k % 7) * cw, cy = 64 + Math.floor(k / 7) * 38;
        if (dd == today) box(hl, cx + 2, cy - 2, cw - 4, 30, C3, INK, 2, 0);
        label(p, "d" + dd, 20 + dd, cx, cy + 4, cw, 22, "" + dd, 16, (k % 7) >= 5 ? 0xC00000 : INK, false, "center");
    }
}

// ---------------------------------------------------------------- the dock (bottom)
// your favourite apps, in the theme colour; at least 4. The + button adds or
// removes apps; when they do not fit, a black arrow shows them all.
var dockClip = null, DOCK_MAX = 9;
function dockApps() {
    var l = cfg.dock == "" ? [] : cfg.dock.split(","), out = [], inst = installedApps();
    for (var i = 0; i < l.length; i++) for (var k = 0; k < inst.length; k++) if (inst[k] == l[i]) { out.push(l[i]); break; }
    return out;
}
function paintDock() {
    if (dockClip) dockClip.removeMovieClip();
    dockClip = taskbar.createEmptyMovieClip("dock", 10);
    var apps = dockApps(), more = apps.length > DOCK_MAX, shown = more ? DOCK_MAX - 1 : apps.length;
    var slot = 72, n = shown + 1 + (more ? 1 : 0);          // + the edit button (and the arrow)
    var dw = n * slot + 16, dx = 4, dy = H - DOCK_H + 6;          // the bottom left corner
    var g = dockClip.createEmptyMovieClip("g", 1);
    box(g, dx, dy, dw, 74, C1, INK, 2, 0);
    var tip = null;
    for (var i = 0; i < shown; i++) {
        var id = apps[i];
        var b = dockClip.createEmptyMovieClip("a" + i, 10 + i);
        b._x = dx + 8 + i * slot; b._y = dy + 2;
        b.hl = b.createEmptyMovieClip("hl", 1);
        box(b.hl, 0, 0, slot, 68, 0xFFFFFF, 0, 0, 0);
        b.hl._alpha = 0;
        icon(b, "ic", 2, 4, 1, appIcon(id), false);
        if (wins[id]) circle(b, slot / 2, 64, 3, onC1());         // running
        b.id = id;
        b.onRollOver = function () { this.hl._alpha = 30; dockTip(this); };
        b.onRollOut = function () { this.hl._alpha = 0; dockTip(null); };
        b.onRelease = function () { dockTip(null); launch(this.id); };
    }
    var ex = dx + 8 + shown * slot;
    if (more) {
        var ar = dockClip.createEmptyMovieClip("more", 50);
        ar._x = ex; ar._y = dy + 2;
        box(ar, 6, 8, slot - 12, 52, WHITE, INK, 2, 0);
        var tri = ar.createEmptyMovieClip("tri", 1);       // own clip: fills that overlap cancel clicks
        tri.beginFill(INK, 100); tri.moveTo(26, 18); tri.lineTo(48, 34); tri.lineTo(26, 50); tri.lineTo(26, 18); tri.endFill();
        ar.onRelease = function () { dockAll(); };
        ex += slot;
    }
    var ed = dockClip.createEmptyMovieClip("edit", 51);
    ed._x = ex; ed._y = dy + 2;
    box(ed, 14, 16, slot - 28, 36, WHITE, INK, 2, 0);
    label(ed, "t", 1, 14, 22, slot - 28, 22, "+", 16, INK, false, "center");
    ed.onRelease = function () { dockEdit(); };
}
function dockTip(b) {
    if (dockClip.tip) dockClip.tip.removeMovieClip();
    if (!b) return;
    var t = dockClip.createEmptyMovieClip("tip", 90), name = APPDEF[b.id][0];
    var tw = name.length * 8 + 16;
    t._x = Math.max(2, Math.round(b._x + 36 - tw / 2)); t._y = b._y - 34;
    box(t, 0, 0, tw, 26, WHITE, INK, 2, 0);
    label(t, "t", 1, 0, 4, tw, 22, name, 16, INK, false, "center");
}
// the arrow: every app of the dock
function dockAll() {
    closeMenu();
    menuOpen = true;
    var c = menuLayer.createEmptyMovieClip("catcher", 1);
    box(c, 0, 0, W, H, 0xFFFFFF, 0, 0, 0);
    c._alpha = 0;
    c.onPress = function () { closeMenu(); };
    c.useHandCursor = false;
    var apps = dockApps(), cols = 5, rows = Math.ceil(apps.length / cols);
    var p = menuLayer.createEmptyMovieClip("panel", 2);
    var pw = cols * 160 + 20, ph = rows * 100 + 50;
    p._x = 4; p._y = H - DOCK_H - ph;
    var g = p.createEmptyMovieClip("g", 1);
    box(g, 0, 0, pw, ph, C1, INK, 2, 0);
    label(p, "t", 2, 12, 8, 300, 22, "All dock apps", 16, onC1(), false);
    for (var i = 0; i < apps.length; i++) {
        var b = p.createEmptyMovieClip("a" + i, 10 + i);
        b._x = 10 + (i % cols) * 160; b._y = 40 + Math.floor(i / cols) * 100;
        b.hl = b.createEmptyMovieClip("hl", 1);
        box(b.hl, 0, 0, 160, 96, 0xFFFFFF, 0, 0, 0);
        b.hl._alpha = 0;
        icon(b, "ic", 2, 48, 4, appIcon(apps[i]), false);
        label(b, "t", 3, 0, 70, 160, 22, APPDEF[apps[i]][0], 16, onC1(), false, "center");
        b.id = apps[i];
        b.onRollOver = function () { this.hl._alpha = 30; };
        b.onRollOut = function () { this.hl._alpha = 0; };
        b.onRelease = function () { launch(this.id); };
    }
}
// add or remove apps (at least 4 stay)
function dockEdit() {
    closeMenu();
    var d = dialogLayer.createEmptyMovieClip("dim", 1);
    box(d, 0, 0, W, H, 0x000000, 0, 0, 0);
    d._alpha = 50;
    d.onPress = function () {};
    d.useHandCursor = false;
    var p = dialogLayer.createEmptyMovieClip("panel", 2);
    var apps = installedApps(), cols = 2, pw = 620, ph = 120 + Math.ceil(apps.length / cols) * 48;
    p._x = (W - pw) / 2; p._y = Math.max(TOP_H, (H - ph) / 2);
    var g = p.createEmptyMovieClip("g", 1);
    titleBar(g, 0, 0, pw, 28);
    box(g, 0, 30, pw, ph - 30, WHITE, INK, 2, 0);
    label(p, "t", 2, 8, 5, pw - 20, 22, "Dock: tick the apps you want (at least 4)", 16, onC1(), false);
    for (var i = 0; i < apps.length; i++) {
        var r = p.createEmptyMovieClip("r" + i, 10 + i);
        r._x = 20 + (i % cols) * 300; r._y = 44 + Math.floor(i / cols) * 48;
        r.cb = r.createEmptyMovieClip("cb", 1);
        icon(r, "ic", 2, 30, 4, appIcon(apps[i]), true);
        label(r, "t", 3, 68, 10, 220, 22, APPDEF[apps[i]][0], 16, INK, false);
        r.id = apps[i];
        r.paint = function () {
            this.cb.clear();
            box(this.cb, 0, 10, 20, 20, WHITE, INK, 2, 0);
            var on = false, l = dockApps();
            for (var k = 0; k < l.length; k++) if (l[k] == this.id) on = true;
            if (on) { this.cb.lineStyle(3, INK, 100); this.cb.moveTo(4, 20); this.cb.lineTo(9, 25); this.cb.lineTo(16, 14); this.cb.lineStyle(); }
        };
        r.paint();
        r.onRelease = function () {
            var l = dockApps(), at = -1;
            for (var k = 0; k < l.length; k++) if (l[k] == this.id) at = k;
            if (at >= 0) {
                if (l.length <= 4) { label(p, "warn", 60, 20, ph - 50, 360, 22, "The dock needs at least 4 apps.", 16, 0xC00000, false); return; }
                l.splice(at, 1);
            } else l.push(this.id);
            cfg.dock = l.join(",");
            saveCfg();
            paintDock();
            this.paint();
            if (p.warn) p.warn.text = "";
        };
    }
    button(p, "ok", 50, pw - 120, ph - 56, 100, 40, "OK", function () { closeDialog(); }, 1, 16);
    dialogKeys = function (code) { if (code == 13 || code == 27) closeDialog(); };
}

// ---------------------------------------------------------------- volume
function applyVolume() {
    if (FlashOS.setVolume) { FlashOS.setVolume(Number(cfg.volume)); FlashOS.setMuted(cfg.muted == "1"); }
}
function closeVolume() {}

// ---------------------------------------------------------------- start menu
// drops down from the start button, in the theme colour: your picture and
// name, the apps on the left; Volume, Log Off, Restart, Shut Down on the right
var menuOpen = false;
function closeMenu() {
    if (!menuOpen) return;
    menuLayer.panel.removeMovieClip();
    menuLayer.catcher.removeMovieClip();
    menuOpen = false;
}
function menuRow(p, name, depth, x, y, w, h, kind, text, onClick, col) {
    var r = p.createEmptyMovieClip(name, depth);
    r._x = x; r._y = y;
    r.hl = r.createEmptyMovieClip("hl", 1);
    box(r.hl, 0, 0, w, h, 0xFFFFFF, 0, 0, 0);
    r.hl._alpha = 0;
    icon(r, "ic", 2, 6, Math.round((h - 32) / 2), kind, true);
    label(r, "t", 4, 46, Math.round((h - 20) / 2), w - 50, 22, text, 16, col, false);
    r.onRollOver = function () { this.hl._alpha = 35; };
    r.onRollOut = function () { this.hl._alpha = 0; };
    r.onRelease = onClick;
    return r;
}
function openStart() {
    closeCalendar();
    menuOpen = true;
    var c = menuLayer.createEmptyMovieClip("catcher", 1);
    box(c, 0, 0, W, H, 0xFFFFFF, 0, 0, 0);
    c._alpha = 0;
    c.onPress = function () { closeMenu(); };
    c.useHandCursor = false;
    var p = menuLayer.createEmptyMovieClip("panel", 2);
    p._x = 4; p._y = TOP_H;
    var apps = installedApps(), LW = 300, RW = 230, rowH = 42;
    var ph = 72 + Math.max(apps.length * rowH + 12, 330);
    var g = p.createEmptyMovieClip("g", 1);
    box(g, 0, 0, LW + RW, ph, C1, INK, 2, 0);
    box(g, LW, 72, RW, ph - 74, C3, 0, 0, 0);
    box(g, 2, 70, LW + RW - 4, 2, INK, 0, 0, 0);
    // you
    var av = picture(p, "av", 2, 10, 8, "avatar_" + cfg.upic + ".png");
    av._xscale = av._yscale = 44;
    label(p, "un", 3, 76, 18, 400, 40, cfg.uname, 32, onC1(), false);
    for (var i = 0; i < apps.length; i++) {
        var r = menuRow(p, "a" + i, 10 + i, 4, 78 + i * rowH, LW - 8, rowH, appIcon(apps[i]), APPDEF[apps[i]][0], null, onC1());
        r.app = apps[i];
        r.onRelease = function () { launch(this.app); };
    }
    // Volume: the speaker mutes, the slider sets the level
    var v = p.createEmptyMovieClip("vol", 30);
    v._x = LW + 8; v._y = 82;
    var vi = icon(v, "ic", 2, 0, 0, cfg.muted == "1" ? "mute" : "volume", true);
    label(v, "t", 4, 40, 6, 120, 22, "Volume", 16, INK, false);
    var pct = label(v, "pct", 5, 140, 6, 70, 22, "", 16, INK, false, "right");
    var TX = 6, TY = 44, TW = 200;
    var tr = v.createEmptyMovieClip("tr", 6);
    box(tr, TX, TY, TW, 12, WHITE, INK, 2, 0);
    var fill = v.createEmptyMovieClip("fill", 7);
    var th = v.createEmptyMovieClip("th", 8);
    box(th, -7, -6, 14, 24, C1, INK, 2, 0);
    th._y = TY;
    function place() {
        var n = Number(cfg.volume);
        th._x = TX + n / 100 * TW;
        fill.clear();
        box(fill, TX + 2, TY + 2, Math.max(0, n / 100 * TW - 2), 8, cfg.muted == "1" ? 0xAAAAAA : C2, 0, 0, 0);
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
    drag.onRelease = drag.onReleaseOutside = function () { delete v.onEnterFrame; saveCfg(); };
    drag.useHandCursor = false;
    vi.onRelease = function () {
        cfg.muted = cfg.muted == "1" ? "0" : "1";
        applyVolume();
        saveCfg();
        var b = sysBitmap(cfg.muted == "1" ? "icon_mute_s.png" : "icon_volume_s.png");
        if (b) this.attachBitmap(b, 1, "auto", true);
        place();
    };
    var y = 160;
    menuRow(p, "lo", 40, LW + 4, y, RW - 8, rowH, "about", "Log Off", function () { closeMenu(); showLogin(); }, INK);
    menuRow(p, "rs", 41, LW + 4, y + rowH, RW - 8, rowH, "restart", "Restart", function () { closeMenu(); FlashOS.restart(); }, INK);
    menuRow(p, "sd", 42, LW + 4, y + rowH * 2, RW - 8, rowH, "power", "Shut Down", function () { closeMenu(); FlashOS.shutdown(); }, INK);
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
var mouseL = {}, idle = 0;
mouseL.onMouseMove = function () { cursorClip._x = _root._xmouse; cursorClip._y = _root._ymouse; idle = getTimer(); };
mouseL.onMouseDown = function () {
    idle = getTimer();
    if (menuOpen || calOpen || dialogLayer.panel || loginOn || saverOn) return;
    var mx = _root._xmouse, my = _root._ymouse, best = null;
    if (my < TOP_H || my > H - DOCK_H + 6) return;     // the taskbar and the dock
    for (var i = 0; i < winOrder.length; i++) {
        var w = winOrder[i];
        if (w._visible && mx >= w._x - 2 && mx < w._x + w.ow + 2 && my >= w._y - 2 && my < w._y + w.oh + 2)
            if (!best || w.getDepth() > best.getDepth()) best = w;
    }
    if (best && best != curApp) focusWin(best);
};
Mouse.addListener(mouseL);

// ---------------------------------------------------------------- running SWFs (one at a time, in a window)
function runSWF(name, back) {
    if (winOrder.length == 0) { FlashOS.runSWF(name, back == "" ? "desk" : back, C1); return; }
    ask("Play " + name + "?", name + " plays in its own window. ByroTT closes its windows meanwhile (save your work first).",
        ["Play", "Cancel"], function (k) { if (k == 0) FlashOS.runSWF(name, back == "" ? "desk" : back, C1); });
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
    var w = makeWindow("term", "Command Runner", 860, 560, true, function () { termOut = null; termIn = null; });
    if (!w) return;
    var b = w.body;
    if (termLines.length == 0) { termLines.push("Digit An Command For Run Program"); termLines.push("Or Insert an Flash-disk"); termLines.push("(type HELP for the commands)"); termLines.push(""); }
    termOut = label(b, "out", 1, 0, 0, 860, 520, "", 16, 0xFFFFFF, false, "left", MONO);
    termOut.multiline = true;
    termOut.wordWrap = true;
    label(b, "pr", 2, 0, 534, 50, 22, "dev/", 16, 0xFFFFFF, false, "left", MONO);
    termIn = b.createTextField("in", 3, 32, 534, 826, 22);
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
    for (i = 0; i < all.length; i++) {                 // "note" finds note.txt
        var nm = all[i].name.toLowerCase(), d = nm.lastIndexOf(".");
        if (d > 0 && nm.substr(0, d) == low) return all[i];
    }
    return null;
}
function termCommand(line) {
    tprint("dev/" + line);
    var sp = line.indexOf(" ");
    var cmd = (sp < 0 ? line : line.substr(0, sp)).toLowerCase(), arg = sp < 0 ? "" : line.substr(sp + 1);
    if (cmd == "") return;
    if (APPDEF[cmd] && cmd != "viewer") {              // an app by its name: FILES, SNAKE, MUSIC...
        var ins = installedApps(), ok = false;
        for (var q = 0; q < ins.length; q++) if (ins[q] == cmd) ok = true;
        if (ok) launch(cmd); else tprint("  " + APPDEF[cmd][0] + " is in More Apps (type STORE).");
        return;
    }
    if (cmd == "help") tprint("  LS            list files\n  RUN <file>    play a SWF in a window (or just type its name)\n  OPEN <file>   open a file in a ByroTT app\n  FILES, WEB, NOTEPAD, PAINT, MUSIC, STORE, SETTINGS, BALL, BIN...   open an app\n  PRINT <file or text>   print on the printer (LPT1)\n  DEL <file>    move a file to the Recycle Bin\n  THEME <default|orange|green|blue|byrott>   window theme\n  MEM           free memory\n  DATE          date and time\n  ABOUT         about ByroTT\n  CLS           clear the screen\n  EXIT          close the terminal");
    else if (cmd == "files") openFiles();
    else if (cmd == "internet" || cmd == "web") openWeb();
    else if (cmd == "notepad") openNotepad(null);
    else if (cmd == "settings") openSettings("");
    else if (cmd == "ball" || cmd == "ballgame") launch("ball");
    else if (cmd == "music") launch("music");
    else if (cmd == "del" || cmd == "delete") {        // DEL <file>: to the Recycle Bin
        var df = findFile(arg);
        if (!df) tprint("  File not found: " + arg);
        else if (df.where != "disk" && df.where != "memory") tprint("  Only files on the hard disk can be deleted.");
        else if (toBin(df.name)) tprint("  " + df.name + " is in the Recycle Bin."); else tprint("  Could not delete " + df.name + ".");
    }
    else if (cmd == "store") launch("store");
    else if (cmd == "paint") openPaint(null);
    else if (cmd == "print") {                 // PRINT <file>, or PRINT <some text>
        var pf = findFile(arg), txt = pf ? FlashOS.read(pf.name) : arg;
        if (txt == null || txt == "") tprint("  Usage: PRINT <text file> or PRINT <some text>");
        else { var pr = FlashOS.print(txt); tprint(pr == "ok" ? "  Printed." : "  Not printed: " + pr + "."); }
    }
    else if (cmd == "theme" || cmd == "color" || cmd == "colour") {
        var cn = arg.toLowerCase();
        if (!THEMES[cn]) tprint("  Themes: default, orange, green, blue, byrott");
        else { cfg.theme = cn; applyColor(); saveCfg(); repaintWindows(); tprint("  Windows theme: " + THEMES[cn][2]); }
    }
    else if (cmd == "ls" || cmd == "dir") {
        var all = FlashOS.files("");
        for (var i = 0; i < all.length; i++) {
            if (hiddenFile(all[i])) continue;
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
    else if (cmd == "about" || cmd == "ver") tprint("  ByroTT 3.0 on FlashOS " + FlashOS.version + " (ByroTT edition).\n  Files are saved on the " + (FlashOS.storage() == "disk" ? "hard disk." : "memory (not installed)."));
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
    runSWF(f.name, "terminal|" + f.name);
}

// ================================================================ NOTEPAD
var noteName = "", noteTF = null, noteDirty = false;
function openNotepad(file) {
    var w = wins["notepad"];
    if (!w) {
        w = makeWindow("notepad", "Notepad", 900, 560, false, function () { noteTF = null; });
        var b = w.body;
        button(b, "new", 1, 0, 8, 80, 34, "New", function () { noteTF.text = ""; noteName = ""; noteDirty = false; noteTitle(); }, 0, 15);
        button(b, "open", 5, 86, 8, 90, 34, "Open...", function () {
            pickFile("Open a text file", function (f) { return fileKind(f.name) == "txt"; }, function (n) { openNotepad(n); });
        }, 0, 15);
        button(b, "save", 2, 182, 8, 80, 34, "Save", function () { noteSave(false); }, 1, 15);
        button(b, "saveas", 3, 268, 8, 110, 34, "Save as...", function () { noteSave(true); }, 0, 15);
        button(b, "print", 6, 384, 8, 80, 34, "Print", function () { notePrint(); }, 0, 15);
        b.createTextField("txt", 4, 0, 52, 900, 508);
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
function notePrint() {
    if (noteTF.text == "") { message("Print", "There is nothing to print."); return; }
    ask("Print", "Print " + (noteName == "" ? "this text" : noteName) + " on the printer (LPT1)?", ["Print", "Cancel"], function (k) {
        if (k != 0) return;
        var r = FlashOS.print ? FlashOS.print(noteTF.text.split("\r").join("\n")) : "this FlashOS cannot print";
        if (r == "ok") message("Print", "Printed!");
        else message("Print", "Sorry, it could not be printed: " + r + ".");
    });
}
// a list of files to choose from (Open...): done(name)
function pickFile(title, want, done) {
    closeMenu();
    var all = FlashOS.files(""), list = [];
    for (var i = 0; i < all.length; i++) if (!hiddenFile(all[i]) && want(all[i])) list.push(all[i]);
    var d = dialogLayer.createEmptyMovieClip("dim", 1);
    box(d, 0, 0, W, H, 0x000000, 0, 0, 0);
    d._alpha = 50;
    d.onPress = function () {};
    d.useHandCursor = false;
    var p = dialogLayer.createEmptyMovieClip("panel", 2);
    var pw = 640, ph = 500, first = 0, sel = -1, ROWS = 9;
    p._x = (W - pw) / 2; p._y = (H - ph) / 2;
    var g = p.createEmptyMovieClip("g", 1);
    titleBar(g, 0, 0, pw, 28);
    box(g, 0, 30, pw, ph - 30, WHITE, INK, 2, 0);
    box(g, 20, 44, pw - 40, ROWS * 40 + 4, WHITE, INK, 2, 0);
    label(p, "t", 2, 8, 5, pw - 20, 22, title, 16, onC1(), false);
    var info = label(p, "info", 3, 20, ph - 58, 240, 22, list.length + (list.length == 1 ? " file" : " files"), 16, GREY, false);
    function draw() {
        for (var r = 0; r < ROWS; r++) {
            var row = p["r" + r];
            if (!row) {
                row = p.createEmptyMovieClip("r" + r, 10 + r);
                row._x = 22; row._y = 46 + r * 40;
                row.bg = row.createEmptyMovieClip("bg", 1);
                row.n = label(row, "n", 3, 44, 10, 380, 22, "", 16, INK, false);
                row.wh = label(row, "wh", 4, 430, 10, 160, 22, "", 16, GREY, false, "right");
                row.r = r;
                row.last = 0;
                row.onRelease = function () {
                    var k = first + this.r;
                    if (k >= list.length) return;
                    if (k == sel && getTimer() - this.last < 500) { closeDialog(); done(list[k].name); return; }
                    sel = k; this.last = getTimer(); draw();
                };
            }
            row.bg.clear();
            var k = first + r;
            if (k < list.length) {
                box(row.bg, 0, 0, pw - 44, 40, k == sel ? C3 : WHITE, 0, 0, 0);
                icon(row, "ic", 2, 6, 4, fileKind(list[k].name), true);
                row.n.text = list[k].name;
                row.wh.text = WHERE[list[k].where] ? WHERE[list[k].where] : list[k].where;
                row._visible = true;
            } else row._visible = false;
        }
    }
    draw();
    if (list.length == 0) label(p, "none", 40, 40, 200, pw - 80, 22, "No files of this kind.", 16, GREY, false, "center");
    button(p, "up", 30, pw - 300, ph - 62, 40, 40, "^", function () { if (first > 0) { first -= ROWS; if (first < 0) first = 0; draw(); } }, 0, 16);
    button(p, "dn", 31, pw - 256, ph - 62, 40, 40, "v", function () { if (first + ROWS < list.length) { first += ROWS; draw(); } }, 0, 16);
    button(p, "ok", 32, pw - 208, ph - 62, 90, 40, "Open", function () { if (sel >= 0) { closeDialog(); done(list[sel].name); } }, 1, 16);
    button(p, "no", 33, pw - 112, ph - 62, 90, 40, "Cancel", function () { closeDialog(); }, 0, 16);
    dialogKeys = function (code) {
        if (code == 27) closeDialog();
        if (code == 13 && sel >= 0) { closeDialog(); done(list[sel].name); }
    };
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

// ================================================================ PAINT
// one canvas for the whole session (pictures are big; they are made once)
var PW_ = 780, PH_ = 520;
var paintBmp = null, paintUndo = null, paintFull = null, paintOrigin = null;
var paintTool = "pencil", paintCol = 0x000000, paintSize = 3, paintName = "", paintDirty = false;
var PAINT_COLS = [0x000000, 0x808080, 0xC0C0C0, 0xFFFFFF, 0x800000, 0xFF0000, 0xFF8C00, 0xFFFF00,
                  0x008000, 0x22EE66, 0x008080, 0xB4FFFF, 0x000080, 0x1E7BFF, 0x800080, 0xFF6EC7];
var PAINT_TOOLS = [["pencil", "Pencil"], ["brush", "Brush"], ["eraser", "Eraser"], ["line", "Line"], ["box", "Box"], ["fill", "Fill"]];
function openPaint(file) {
    var w = makeWindow("paint", "Paint", 940, 572, false, null);
    if (!w) return;
    if (!paintBmp) {
        paintBmp = new flash.display.BitmapData(PW_, PH_, false, 0xFFFFFF);
        paintUndo = new flash.display.BitmapData(PW_, PH_, false, 0xFFFFFF);
        paintFull = new flash.geom.Rectangle(0, 0, PW_, PH_);
        paintOrigin = new flash.geom.Point(0, 0);
    }
    var b = w.body;
    // top: file buttons
    button(b, "new", 1, 140, 0, 70, 34, "New", function () { paintSnap(); paintBmp.fillRect(paintFull, 0xFFFFFF); paintName = ""; paintDirty = false; paintTitle(); }, 0, 16);
    button(b, "open", 2, 214, 0, 80, 34, "Open...", function () {
        pickFile("Open a picture", function (f) { return fileKind(f.name) == "img" || ext(f.name) == "bmp"; }, function (n) { paintLoad(n); });
    }, 0, 16);
    button(b, "save", 3, 298, 0, 70, 34, "Save", function () { paintSave(false, null); }, 1, 16);
    button(b, "saveas", 4, 372, 0, 100, 34, "Save as...", function () { paintSave(true, null); }, 0, 16);
    button(b, "undo", 5, 476, 0, 70, 34, "Undo", function () { paintUndoNow(); }, 0, 16);
    button(b, "wall", 6, 740, 0, 180, 34, "Use as wallpaper", function () {
        paintSave(paintName == "", function () { setWallpaper(paintName); saveCfg(); message("Paint", paintName + " is your wallpaper now."); });
    }, 0, 16);
    // left: tools, sizes, colours
    var side = b.createEmptyMovieClip("side", 7);
    box(side, 0, 0, 130, 572, LIGHT, INK, 2, 0);
    w.tools = [];
    for (var i = 0; i < PAINT_TOOLS.length; i++) {
        var t = button(b, "t" + i, 10 + i, 6, 6 + i * 38, 118, 34, PAINT_TOOLS[i][1], null, 2, 16);
        t.tool = PAINT_TOOLS[i][0];
        t.onRelease = function () { paintTool = this.tool; paintPaintSide(); };
        w.tools.push(t);
    }
    label(b, "szl", 20, 8, 240, 110, 22, "Size", 16, INK, false);
    w.sizes = [];
    var SZ = [1, 3, 8, 16];
    for (i = 0; i < SZ.length; i++) {
        var sb = button(b, "z" + i, 21 + i, 6 + i * 30, 262, 28, 28, "", null, 2, 16);
        sb.sz = SZ[i];
        circle(sb.icon, 14, 14, Math.max(1.5, SZ[i] / 2 + 0.5), INK);
        sb.onRelease = function () { paintSize = this.sz; paintPaintSide(); };
        w.sizes.push(sb);
    }
    label(b, "cl", 30, 8, 300, 110, 22, "Colour", 16, INK, false);
    w.cur = b.createEmptyMovieClip("cur", 31);
    w.cur._x = 8; w.cur._y = 324;
    for (i = 0; i < PAINT_COLS.length; i++) {
        var sw = b.createEmptyMovieClip("c" + i, 40 + i);
        sw._x = 8 + (i % 4) * 29; sw._y = 372 + Math.floor(i / 4) * 29;
        box(sw, 0, 0, 26, 26, PAINT_COLS[i], INK, 1, 0);
        sw.col = PAINT_COLS[i];
        sw.onRelease = function () { paintCol = this.col; paintPaintSide(); };
    }
    // the canvas
    var cv = b.createEmptyMovieClip("cv", 60);
    cv._x = 140; cv._y = 42;
    var frame = cv.createEmptyMovieClip("frame", 1);
    box(frame, -2, -2, PW_ + 4, PH_ + 4, INK, 0, 0, 0);
    var pic = cv.createEmptyMovieClip("pic", 2);
    pic.attachBitmap(paintBmp, 1, "never", false);
    var pre = cv.createEmptyMovieClip("pre", 3);     // line and box preview
    var x0, y0, lx, ly, down = false;
    function mx() { return Math.max(0, Math.min(PW_ - 1, Math.round(cv._xmouse))); }
    function my() { return Math.max(0, Math.min(PH_ - 1, Math.round(cv._ymouse))); }
    pic.onPress = function () {
        paintSnap();
        x0 = lx = mx(); y0 = ly = my();
        paintDirty = true; paintTitle();
        if (paintTool == "fill") { paintBmp.floodFill(x0, y0, paintCol); return; }
        down = true;
        if (paintTool == "pencil" || paintTool == "brush" || paintTool == "eraser") paintStroke(lx, ly, lx, ly);
        cv.onEnterFrame = function () {
            var x = mx(), y = my();
            if (paintTool == "line" || paintTool == "box") {
                pre.clear();
                pre.lineStyle(Math.max(1, paintSz()), paintCol, 100);
                if (paintTool == "line") { pre.moveTo(x0, y0); pre.lineTo(x, y); }
                else { pre.moveTo(x0, y0); pre.lineTo(x, y0); pre.lineTo(x, y); pre.lineTo(x0, y); pre.lineTo(x0, y0); }
                lx = x; ly = y;
            } else if (x != lx || y != ly) { paintStroke(lx, ly, x, y); lx = x; ly = y; }
        };
    };
    pic.onRelease = pic.onReleaseOutside = function () {
        if (!down) return;
        down = false;
        delete cv.onEnterFrame;
        pre.clear();
        var s = paintSz();
        if (paintTool == "line") paintBmp.drawLine(x0, y0, lx, ly, paintCol, s);
        if (paintTool == "box") {
            paintBmp.drawLine(x0, y0, lx, y0, paintCol, s); paintBmp.drawLine(lx, y0, lx, ly, paintCol, s);
            paintBmp.drawLine(lx, ly, x0, ly, paintCol, s); paintBmp.drawLine(x0, ly, x0, y0, paintCol, s);
        }
    };
    pic.useHandCursor = false;
    paintPaintSide();
    paintTitle();
    if (file) paintLoad(file);
}
function paintSz() { return paintTool == "pencil" ? Math.min(paintSize, 3) : (paintTool == "eraser" ? paintSize * 2 + 6 : paintSize * 2); }
function paintStroke(ax, ay, bx, by) { paintBmp.drawLine(ax, ay, bx, by, paintTool == "eraser" ? 0xFFFFFF : paintCol, paintSz()); }
function paintSnap() { paintUndo.copyPixels(paintBmp, paintFull, paintOrigin); }
function paintUndoNow() {
    // swap the picture and the undo copy, so Undo twice is Redo (no new memory)
    var w = wins["paint"];
    if (!w) return;
    if (!w.tmp) w.tmp = new flash.display.BitmapData(PW_, PH_, false, 0xFFFFFF);
    w.tmp.copyPixels(paintBmp, paintFull, paintOrigin);
    paintBmp.copyPixels(paintUndo, paintFull, paintOrigin);
    paintUndo.copyPixels(w.tmp, paintFull, paintOrigin);
}
function paintPaintSide() {
    var w = wins["paint"];
    if (!w) return;
    for (var i = 0; i < w.tools.length; i++) w.tools[i].setOn(w.tools[i].tool == paintTool);
    for (i = 0; i < w.sizes.length; i++) w.sizes[i].setOn(w.sizes[i].sz == paintSize);
    w.cur.clear();
    box(w.cur, 0, 0, 112, 40, paintCol, INK, 2, 0);
}
function paintTitle() {
    var w = wins["paint"];
    if (w) w.titleTxt.text = (paintDirty ? "* " : "") + (paintName == "" ? "untitled.bmp" : paintName) + " - Paint";
}
function paintLoad(n) {
    var bm = FlashOS.loadBitmap(n);
    if (!bm) { message("Paint", "Sorry, " + n + " could not be opened as a picture."); return; }
    paintSnap();
    paintBmp.fillRect(paintFull, 0xFFFFFF);
    paintBmp.copyPixels(bm, new flash.geom.Rectangle(0, 0, Math.min(bm.width, PW_), Math.min(bm.height, PH_)), paintOrigin);
    paintName = ext(n) == "bmp" ? n : n.substr(0, n.lastIndexOf(".")) + ".bmp";
    paintDirty = ext(n) != "bmp";
    paintTitle();
}
function paintSave(askName, then) {
    if (askName || paintName == "") {
        prompt("Save picture", "File name:", paintName == "" ? "drawing.bmp" : paintName, function (n) {
            if (n == "") return;
            if (ext(n) != "bmp") n += ".bmp";
            paintName = n;
            paintSave(false, then);
        });
        return;
    }
    if (FlashOS.saveBitmap(paintName, paintBmp)) { paintDirty = false; paintTitle(); if (then) then(); }
    else message("Paint", "Sorry, the picture could not be saved. The disk may be full.");
}

// ================================================================ VIEWER (pictures and videos)
var vid = null;
function openViewer(file) {
    if (wins["viewer"]) closeWindow(wins["viewer"]);
    var w = makeWindow("viewer", file, 820, 590, true, function () { if (vid) { vid.close(); vid = null; } });
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
    button(b, "edit", 6, 220, 540, 160, 36, "Edit in Paint", function () { openPaint(file); }, 0, 15);
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
    else if (e == "mp3") openMusic(n);
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
    filesFolder(c, "ad", 3, 190, "admin", "Admin", function () { adminCheck(function () { filesPrivate(w); }); });
}
// ---- Admin: a password (optional) and the Private Folder
function hashPass(p) {
    var h = 5381;
    p = "byrott:" + p;
    for (var i = 0; i < p.length; i++) h = ((h * 33) ^ p.charCodeAt(i)) % 2147483647;
    return "h" + h;
}
function askPassword(title, text, done) {
    prompt(title, text, "", done);
    var f = dialogLayer.panel.inp;
    f.password = true;
    f.text = "";
    dialogLayer.answer = "";
}
function adminCheck(then) {
    if (cfg.pass == "" || cfg.pass == undefined) { then(); return; }
    askPassword("Admin", "Password:", function (p) {
        if (hashPass(p) == cfg.pass) then();
        else message("Admin", "Wrong Password");
    });
}
function privateFiles() {
    var all = FlashOS.files(""), out = [];
    for (var i = 0; i < all.length; i++) if (all[i].name.indexOf(PRIV) == 0) out.push(all[i]);
    return out;
}
function filesPrivate(w) {
    var c = filesPage(w);
    var fr = c.createEmptyMovieClip("fr", 1);
    box(fr, 10, 10, 840, 540, WHITE, INK, 3, 0);
    var t = c.createEmptyMovieClip("t", 2);
    t._x = 10;
    icon(t, "ic", 1, 14, 18, "admin_s", false);
    label(t, "n", 2, 56, 22, 600, 36, "Private Folder", 32, INK, false);
    var list = privateFiles();
    if (list.length == 0) {
        var e = label(c, "e", 3, 40, 200, 780, 200, "", 32, INK, false, "center");
        e.multiline = true;
        e.text = "Nothing to see here.\n(click to return\nto main folder)";
        fr.onRelease = function () { filesHome(w); };
        label(c, "tip", 4, 40, 500, 780, 22, "Tip: Hard Disk > Make private puts a file here.", 16, GREY, false, "center");
        return;
    }
    for (var i = 0; i < list.length && i < 9; i++) {
        var r = c.createEmptyMovieClip("r" + i, 10 + i);
        r._x = 24; r._y = 76 + i * 48;
        r.hl = r.createEmptyMovieClip("hl", 1);
        box(r.hl, 0, 0, 812, 44, C3, 0, 0, 0);
        r.hl._alpha = 0;
        icon(r, "ic", 2, 6, 6, fileKind(list[i].name), true);
        label(r, "n", 3, 48, 12, 560, 22, list[i].name.substr(PRIV.length), 16, INK, false);
        r.file = list[i].name;
        r.onRollOver = function () { this.hl._alpha = 100; };
        r.onRollOut = function () { this.hl._alpha = 0; };
        r.onRelease = function () { openFile(this.file); };
        var pb = button(r, "pub", 4, 640, 6, 160, 32, "Make public", null, 0, 16);
        pb.file = list[i].name;
        pb.onRelease = function () {
            var n = this.file.substr(PRIV.length);
            if (FlashOS.copyFile(this.file, n)) { FlashOS.remove(this.file); filesPrivate(w); }
            else message("Files", "Sorry, the file could not be moved.");
        };
    }
    button(c, "back", 40, 24, 500, 160, 38, "< Back", function () { filesHome(w); }, 0, 16);
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
var filesDir = "";        // the folder open on the hard disk ("" = the top), ends with "/"
function filesBuildList(w) {
    var b = w.body;
    var side = b.createEmptyMovieClip("side", 1);
    box(side, 0, 0, 190, 580, LIGHT, INK, 2, 0);
    w.places = [];
    for (var i = 0; i < PLACES.length; i++) {
        var pb = button(b, "p" + i, 10 + i, 8, 12 + i * 44, 174, 40, PLACES[i][1], null, 2, 15);
        pb.place = PLACES[i][0];
        pb.onRelease = function () { filesPlace = this.place; filesDir = ""; filesSel = -1; filesFirst = 0; filesReload(); };
        w.places.push(pb);
    }
    button(b, "back", 29, 8, 532, 174, 38, "< Back", function () { closeWindow(w); openFiles(); }, 0, 16);
    // where we are, and Up for folders
    w.path = label(b, "path", 26, 204, 10, 560, 22, "", 16, INK, false);
    button(b, "updir", 27, 776, 2, 96, 34, "Up", function () { filesUp(); }, 0, 16);
    var y = 482;
    button(b, "open", 30, 200, y, 90, 38, "Open", function () { filesOpenSel(); }, 1, 15);
    button(b, "copy", 31, 294, y, 130, 38, "Copy to disk", function () { filesCopy(); }, 0, 15);
    button(b, "del", 32, 428, y, 90, 38, "Delete", function () { filesDelete(); }, 0, 15);
    button(b, "prv", 36, 522, y, 124, 38, "Make private", function () { filesMakePrivate(); }, 0, 15);
    button(b, "mv", 37, 650, y, 110, 38, "Move to...", function () { filesMove(); }, 0, 15);
    button(b, "nf", 38, 200, y + 46, 124, 38, "New folder", function () { filesNewFolder(); }, 0, 15);
    button(b, "dsk", 39, 328, y + 46, 150, 38, "Add to desktop", function () {
        if (filesSel < 0) { message("Files", "Pick a file first."); return; }
        var f = filesList[filesSel];
        if (f.folder) { message("Files", "Folders cannot go on the desktop, only files."); return; }
        if (deskAdd("file", f.name)) message("Files", f.name + " is on the desktop now."); else message("Files", f.name + " is already on the desktop.");
    }, 0, 15);
    button(b, "up", 33, 766, y, 50, 38, "^", function () { if (filesFirst > 0) { filesFirst -= 9; if (filesFirst < 0) filesFirst = 0; filesDraw(); } }, 0, 16);
    button(b, "dn", 34, 822, y, 50, 38, "v", function () { if (filesFirst + 9 < filesList.length) { filesFirst += 9; filesDraw(); } }, 0, 15);
    w.info = label(b, "info", 35, 600, y + 56, 272, 24, "", 13, GREY, false, "right");
    var head = b.createEmptyMovieClip("head", 40);
    box(head, 200, 40, 672, 30, LIGHT, INK, 2, 0);
    label(b, "h1", 41, 250, 44, 300, 24, "Name", 14, GREY, true);
    label(b, "h2", 42, 600, 44, 100, 24, "Size", 14, GREY, true, "right");
    label(b, "h3", 43, 730, 44, 120, 24, "Where", 14, GREY, true);
    filesReload();
}
function base(n) { var k = n.lastIndexOf("/"); return k >= 0 ? n.substr(k + 1) : n; }
function filesReload() {
    var all = FlashOS.files("");
    filesList = [];
    var folders = {}, fl = [];
    for (var i = 0; i < all.length; i++) {
        var f = all[i], wh = f.where == "memory" ? "disk" : f.where;
        if (f.name.indexOf("~bin/") == 0 || f.name.indexOf(PRIV) == 0) continue;
        if (filesPlace == "disk" && wh == "disk") {
            // only what is in the open folder; folders inside it show once
            if (f.name.indexOf(filesDir) != 0) continue;
            var rest = f.name.substr(filesDir.length), k = rest.indexOf("/");
            if (k >= 0) {
                var fn = rest.substr(0, k);
                if (!folders[fn]) { folders[fn] = true; fl.push({name: filesDir + fn + "/", label: fn, folder: true, where: "disk", size: 0}); }
                continue;
            }
            if (!hiddenFile(f)) filesList.push({name: f.name, label: rest, where: f.where, size: f.size});
        } else if ((filesPlace == "all" || wh == filesPlace) && !hiddenFile(f)) filesList.push({name: f.name, label: f.name, where: f.where, size: f.size});
    }
    filesList = fl.concat(filesList);
    var w = wins["files"];
    if (!w) return;
    for (var k2 = 0; k2 < w.places.length; k2++) w.places[k2].setOn(w.places[k2].place == filesPlace);
    var pn = "";
    for (k2 = 0; k2 < PLACES.length; k2++) if (PLACES[k2][0] == filesPlace) pn = PLACES[k2][1];
    w.path.text = pn + (filesPlace == "disk" ? " / " + filesDir.split("/").join(" / ") : "");
    w.body.updir._visible = filesPlace == "disk" && filesDir != "";
    filesDraw();
}
function filesUp() {
    if (filesDir == "") return;
    var p = filesDir.substr(0, filesDir.length - 1), k = p.lastIndexOf("/");
    filesDir = k >= 0 ? p.substr(0, k + 1) : "";
    filesSel = -1; filesFirst = 0;
    filesReload();
}
function filesOpenSel() {
    if (filesSel < 0) return;
    var f = filesList[filesSel];
    if (f.folder) { filesDir = f.name; filesSel = -1; filesFirst = 0; filesReload(); }
    else openFile(f.name);
}
function kb(n) { if (n < 1024) return n + " B"; if (n < 1048576) return Math.round(n / 1024) + " KB"; return Math.round(n / 104857.6) / 10 + " MB"; }
var WHERE = {disk: "Hard disk", memory: "Memory", cd: "CD", usb: "USB", system: "ByroTT", net: "Download"};
function fileKind(n) {
    var e = ext(n);
    if (e == "swf") return "swf";
    if (e == "txt" || e == "log" || e == "md" || e == "cfg" || e == "ini" || e == "csv") return "txt";
    if (e == "jpg" || e == "jpeg" || e == "png" || e == "gif" || e == "bmp") return "img";
    if (e == "flv") return "video";
    if (e == "mp3") return "music";
    if (e == "iso") return "disc";
    return "file";
}
// files the lists do not show: ByroTT's pictures and settings, private files,
// folder markers, the Recycle Bin, the Store's copies
function hiddenFile(f) {
    var n = f.name;
    return (f.where == "system" && isSystemArt(n)) || n.indexOf("byrott.") == 0 || n.indexOf(PRIV) == 0 || n.indexOf("~bin/") == 0 ||
           n.lastIndexOf("/.folder") == n.length - 8 && n.length >= 8 || n == "ballgame.hi";
}
var PRIV = "private_";
function isSystemArt(n) { return n.indexOf("icon_") == 0 || n.indexOf("logo_") == 0 || n.indexOf("avatar_") == 0 || n.indexOf("store_") == 0 || n == "start_btn.png" || n == "web_offline.png"; }
function filesDraw() {
    var w = wins["files"];
    if (!w) return;
    var b = w.body;
    for (var r = 0; r < 9; r++) {
        var i = filesFirst + r;
        var row = b["r" + r];
        if (!row) {
            row = b.createEmptyMovieClip("r" + r, 50 + r);
            row._x = 200; row._y = 74 + r * 44;
            row.bg = row.createEmptyMovieClip("bg", 1);
            row.n = label(row, "n", 3, 50, 9, 330, 26, "", 16, INK, false);
            row.s = label(row, "s", 4, 380, 10, 120, 24, "", 14, GREY, false, "right");
            row.wh = label(row, "wh", 5, 530, 10, 130, 24, "", 14, GREY, false);
            row.r = r;
            row.last = 0;
            row.onRelease = function () {
                var k = filesFirst + this.r;
                if (k >= filesList.length) return;
                if (k == filesSel && getTimer() - this.last < 500) { filesOpenSel(); return; }
                filesSel = k;
                this.last = getTimer();
                filesDraw();
            };
        }
        row.bg.clear();
        if (i < filesList.length) {
            var f = filesList[i];
            box(row.bg, 0, 0, 672, 42, i == filesSel ? C3 : (r % 2 ? 0xEDEDED : 0xF7F7F7), i == filesSel ? INK : 0, i == filesSel ? 2 : 0, 0);
            icon(row, "ic", 2, 10, 5, f.folder ? "files" : fileKind(f.name), true);
            row.n.text = f.label;
            row.s.text = f.folder ? "folder" : kb(f.size);
            row.wh.text = WHERE[f.where] ? WHERE[f.where] : f.where;
            row._visible = true;
        } else row._visible = false;
    }
    w.info.text = filesList.length + (filesList.length == 1 ? " item" : " items");
}
function onDisk(f) { return f.where == "disk" || f.where == "memory"; }
function filesCopy() {
    if (filesSel < 0) { message("Files", "Pick a file first."); return; }
    var f = filesList[filesSel];
    if (f.folder || onDisk(f)) { message("Files", f.label + " is already on the hard disk."); return; }
    if (FlashOS.copyFile(f.name, base(f.name))) { message("Files", f.name + " was copied to the hard disk."); filesReload(); }
    else message("Files", "Sorry, " + f.name + " could not be copied. The disk may be full.");
}
function moveFile(from, to) {
    if (from == to) return true;
    if (!FlashOS.copyFile(from, to)) return false;
    FlashOS.remove(from);
    for (var i = 0; i < desk.length; i++) if (desk[i].kind == "file" && desk[i].target == from) { desk[i].target = to; deskSave(); buildIcons(); }
    return true;
}
function filesMakePrivate() {
    if (filesSel < 0) { message("Files", "Pick a file first."); return; }
    var f = filesList[filesSel];
    if (f.folder || !onDisk(f)) { message("Files", "Only files on the hard disk can go in the Private Folder."); return; }
    if (!moveFile(f.name, PRIV + base(f.name))) { message("Files", "Sorry, " + f.name + " could not be moved."); return; }
    filesSel = -1;
    filesReload();
    message("Files", base(f.name) + " is now in Admin > Private Folder." + (cfg.pass == "" ? "\nTip: set an Admin Password in Settings." : ""));
}
function filesNewFolder() {
    if (filesPlace != "disk") { filesPlace = "disk"; filesDir = ""; }
    prompt("New folder", "Name of the new folder:", "New folder", function (n) {
        n = n.split("/").join("-");
        if (n == "") return;
        FlashOS.write(filesDir + n + "/.folder", "");
        filesReload();
    });
}
function filesMove() {
    if (filesSel < 0) { message("Files", "Pick a file first."); return; }
    var f = filesList[filesSel];
    if (f.folder || !onDisk(f)) { message("Files", "Only files on the hard disk can be moved."); return; }
    prompt("Move " + base(f.name), "To which folder? (for example Games or Games/Old;\nempty = the top of the hard disk)", "", function (d) {
        while (d.charAt(d.length - 1) == "/") d = d.substr(0, d.length - 1);
        var to = (d == "" ? "" : d + "/") + base(f.name);
        if (d != "") FlashOS.write(d + "/.folder", "");
        if (!moveFile(f.name, to)) message("Files", "Sorry, the file could not be moved.");
        filesSel = -1;
        filesReload();
    });
}
function filesDelete() {
    if (filesSel < 0) { message("Files", "Pick a file first."); return; }
    var f = filesList[filesSel];
    if (!f.folder && !onDisk(f)) { message("Files", "Only files on the hard disk can be deleted. " + f.name + " is on the " + (WHERE[f.where] ? WHERE[f.where] : f.where) + "."); return; }
    var what = f.folder ? "the folder " + f.label + " and everything in it" : f.label;
    ask("Delete", "Move " + what + " to the Recycle Bin?", ["Yes", "No"], function (k) {
        if (k != 0) return;
        if (f.folder) {
            var all = FlashOS.files("");
            for (var i = 0; i < all.length; i++)
                if (all[i].name.indexOf(f.name) == 0) { if (base(all[i].name) == ".folder") FlashOS.remove(all[i].name); else toBin(all[i].name); }
        } else toBin(f.name);
        filesSel = -1;
        filesReload();
    });
}

// ================================================================ RECYCLE BIN
// deleted files wait in "~bin/"; byrott.bin remembers where they came from
function binIndex() { var l = listLoad("byrott.bin"); return l == null ? [] : l; }
function binCount() { var n = 0, all = FlashOS.files(""); for (var i = 0; i < all.length; i++) if (all[i].name.indexOf("~bin/") == 0) n++; return n; }
function toBin(name) {
    var b = "~bin/" + base(name), k = 2;
    while (FlashOS.exists(b)) b = "~bin/" + (k++) + "-" + base(name);
    if (!moveFile(name, b)) return false;
    var l = binIndex();
    l.push([b, name]);
    listSave("byrott.bin", l);
    deskRemove("file", b);
    binChanged();
    return true;
}
function binChanged() { buildIcons(); paintDock(); if (wins["bin"]) binDraw(); }
var binSel = -1;
function openBin() {
    var w = makeWindow("bin", "Recycle Bin", 700, 480, false, null);
    if (!w) return;
    var b = w.body;
    var head = b.createEmptyMovieClip("head", 1);
    box(head, 0, 0, 700, 30, LIGHT, INK, 2, 0);
    label(b, "h1", 2, 50, 4, 300, 22, "Name", 16, GREY, false);
    label(b, "h2", 3, 360, 4, 300, 22, "Was in", 16, GREY, false);
    button(b, "rs", 4, 0, 436, 140, 40, "Restore", function () { binRestore(); }, 1, 16);
    button(b, "dl", 5, 146, 436, 180, 40, "Delete for ever", function () { binDelete(false); }, 0, 16);
    button(b, "em", 6, 500, 436, 200, 40, "Empty Recycle Bin", function () { binDelete(true); }, 3, 16);
    binSel = -1;
    binDraw();
}
function binItems() {
    var idx = binIndex(), all = FlashOS.files(""), out = [];
    for (var i = 0; i < all.length; i++) {
        var n = all[i].name;
        if (n.indexOf("~bin/") != 0) continue;
        var from = n.substr(5);
        for (var k = 0; k < idx.length; k++) if (idx[k][0] == n) from = idx[k][1];
        out.push([n, from]);
    }
    return out;
}
function binDraw() {
    var w = wins["bin"];
    if (!w) return;
    var c = w.body.createEmptyMovieClip("list", 20), items = binItems();
    if (items.length == 0) label(c, "e", 1, 0, 160, 700, 40, "The Recycle Bin is empty.", 32, GREY, false, "center");
    for (var i = 0; i < items.length && i < 9; i++) {
        var r = c.createEmptyMovieClip("r" + i, 10 + i);
        r._y = 34 + i * 44;
        box(r, 0, 0, 700, 42, i == binSel ? C3 : (i % 2 ? 0xEDEDED : 0xF7F7F7), i == binSel ? INK : 0, i == binSel ? 2 : 0, 0);
        icon(r, "ic", 1, 10, 5, fileKind(items[i][0]), true);
        label(r, "n", 2, 50, 11, 300, 22, base(items[i][1]), 16, INK, false);
        var wasIn = items[i][1].lastIndexOf("/") >= 0 ? items[i][1].substr(0, items[i][1].lastIndexOf("/")) : "Hard disk";
        label(r, "w", 3, 360, 11, 330, 22, wasIn, 16, GREY, false);
        r.k = i;
        r.onRelease = function () { binSel = this.k; binDraw(); };
    }
}
function binRestore() {
    var items = binItems();
    if (binSel < 0 || binSel >= items.length) { message("Recycle Bin", "Pick a file first."); return; }
    var it = items[binSel], to = it[1];
    while (FlashOS.exists(to)) to = "restored-" + to;
    if (to.lastIndexOf("/") >= 0) FlashOS.write(to.substr(0, to.lastIndexOf("/")) + "/.folder", "");
    if (!moveFile(it[0], to)) { message("Recycle Bin", "Sorry, it could not be restored."); return; }
    var l = binIndex();
    for (var k = 0; k < l.length; k++) if (l[k][0] == it[0]) { l.splice(k, 1); break; }
    listSave("byrott.bin", l);
    binSel = -1;
    binChanged();
    if (wins["files"]) filesReload();
}
function binDelete(all) {
    var items = binItems();
    if (!all && (binSel < 0 || binSel >= items.length)) { message("Recycle Bin", "Pick a file first."); return; }
    if (all && items.length == 0) return;
    ask("Recycle Bin", all ? "Delete all " + items.length + " files for ever?" : "Delete " + base(items[binSel][1]) + " for ever?", ["Yes", "No"], function (k) {
        if (k != 0) return;
        var l = binIndex();
        for (var i = 0; i < items.length; i++) if (all || i == binSel) {
            FlashOS.remove(items[i][0]);
            for (var j = 0; j < l.length; j++) if (l[j][0] == items[i][0]) { l.splice(j, 1); break; }
        }
        listSave("byrott.bin", l);
        binSel = -1;
        binChanged();
    });
}

// ================================================================ SETTINGS
// like the mock-up: one list, click a row to change it
function wallName(f) { var n = f.substr(0, f.lastIndexOf(".")); return n.substr(0, 1).toUpperCase() + n.substr(1); }
function openSettings(pg) {
    if (!wins["settings"]) makeWindow("settings", "System Settings", 760, 600, false, null);
    else focusWin(wins["settings"]);
    settingsPage("");
}
function setRow(c, name, depth, y, kind, text, onClick) {
    // a big first line (like the mock-up) and a small second one
    var r = c.createEmptyMovieClip(name, depth);
    r._y = y;
    r.hl = r.createEmptyMovieClip("hl", 1);
    var lines = text.split("\n");
    var h = Math.max(58, 40 + (lines.length - 1) * 18);
    box(r.hl, 0, 0, 760, h, C3, 0, 0, 0);
    r.hl._alpha = 0;
    var ic = icon(r, "ic", 2, 6, Math.round((h - 48) / 2), kind, false);
    ic._xscale = ic._yscale = 75;
    label(r, "t", 3, 66, 4, 680, 36, lines[0], 32, INK, false);
    if (lines.length > 1) {
        var t2 = label(r, "t2", 4, 68, 38, 680, 100, "", 16, GREY, false);
        t2.multiline = true;
        t2.text = lines.slice(1).join("\n");
    }
    r.h = h;
    if (onClick) {
        r.onRollOver = function () { this.hl._alpha = 100; };
        r.onRollOut = function () { this.hl._alpha = 0; };
        r.onRelease = onClick;
    } else r.useHandCursor = false;
    return r;
}
function nextOf(list, v) { for (var i = 0; i < list.length; i++) if (list[i] == v) return list[(i + 1) % list.length]; return list[0]; }
var SAVERS = ["0", "1", "2", "5", "10"];
function settingsPage(pg) {
    var w = wins["settings"];
    if (!w) return;
    var c = w.body.createEmptyMovieClip("page", 50);   // replaces the old page
    var y = 0, r, rows = [
        ["cur", "pointer", "Change Pointer Color\n(" + CURSOR_NAMES[cfg.cursor] + ")", function () { cfg.cursor = nextOf(CURSORS, cfg.cursor); applyCursor(); saveCfg(); settingsPage(""); }],
        ["thm", "theme", "Change Windows Theme\n(" + THEMES[cfg.theme][2] + ": windows, start menu and dock)", function () {
            cfg.theme = nextOf(THEME_NAMES, cfg.theme); applyColor(); saveCfg(); repaintWindows(); paintTaskbar(); settingsPage(""); }],
        ["wal", "wall", "Change Wallpaper\n(" + wallName(cfg.wallpaper) + ")", function () { setWallpaper(nextOf(systemWallpapers(), cfg.wallpaper)); saveCfg(); settingsPage(""); }],
        ["usr", "about", "User Account\n(" + cfg.uname + ", password " + (cfg.upass == "" ? "off" : "on") + ")", function () { userDialog(); }],
        ["svr", "wall", "Screensaver\n(" + (cfg.saver == "0" ? "Off" : "after " + cfg.saver + " minute" + (cfg.saver == "1" ? "" : "s")) + ")", function () { cfg.saver = nextOf(SAVERS, cfg.saver); saveCfg(); settingsPage(""); }],
        ["dck", "store", "Dock\n(" + dockApps().length + " apps: click to add or remove)", function () { dockEdit(); }],
        ["pw", "admin", "Admin Password\n(" + (cfg.pass == "" ? "Off" : "On") + ")", function () { setPassword(); }],
        ["del", "delete", "Delete Byrott", function () { uninstallNow(); }],
        ["abt", "about", "About Byrott 3.0\nByrott Is Not Affilated With FlashOS At All\n(c) 2000 Byrott/FlashOS", null]];
    for (var i = 0; i < rows.length; i++) {
        r = setRow(c, rows[i][0], i + 1, y, rows[i][1], rows[i][2], rows[i][3]);
        y += r.h + 2;
    }
}
function paintFrameColors(w) { w.paintBar(); }
function setPassword() {
    adminCheck(function () {
        askPassword("Admin Password", "New password for the Admin folder\n(leave it empty for no password):", function (p) {
            if (p == "") { cfg.pass = ""; saveCfg(); settingsPage(""); message("Admin Password", "The Admin folder has no password now."); return; }
            askPassword("Admin Password", "Type the new password again:", function (q) {
                if (q != p) { message("Admin Password", "The two passwords are not the same. Nothing changed."); return; }
                cfg.pass = hashPass(p);
                saveCfg();
                settingsPage("");
                message("Admin Password", "Password set. The Admin folder now asks for it.");
            });
        });
    });
}
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
var WEB_W = 900, WEB_VIEW = 440;
var START_PAGE = "<title>ByroTT start page</title><h1>Welcome to the Internet</h1>" +

    "<form action=\"http://frogfind.com/\"><p><b>Search the web with FrogFind:</b> <input name=\"q\"> <input type=\"submit\" value=\"Search\"></p></form>" +
    "<ul><li><a href=\"http://frogfind.com/\">FrogFind</a> - a search engine that makes modern pages simple</li>" +
    "<li><a href=\"http://68k.news/\">68k.news</a> - today's news for vintage computers</li>" +
    "<li><a href=\"http://info.cern.ch/\">info.cern.ch</a> - the first web site ever</li>" +
    "<li><a href=\"http://example.com/\">example.com</a></li></ul>" +
    "<p>Tip: click a link, or type an address at the top and press Enter. +Mark saves the page in your Bookmarks.</p>";
var STYLE = [[16, INK, FONT], [32, INK, FONT], [32, INK, FONT], [16, INK, FONT], [16, 0x0000CC, FONT], [16, INK, MONO], [16, GREY, FONT], [16, INK, FONT]];
var charW = [], lineHt = [], measured = false;
var webItems, webLines, webScroll = 0, webUrl = "", webHist = [], webTitle = "", webForms = [];
var webPage = null, webBar = null, webThumb = null, webStatus = null, webUrlF = null, netOk = false, webImgs = [];

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
    webImgs = [];
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
        else if (nm == "img" && !close) {
            var alt = attr(tag, "alt"), src = attr(tag, "src"), iw = parseInt(attr(tag, "width")), ih = parseInt(attr(tag, "height"));
            if (src != null && src != "" && webImgs.length < 12 && !(iw > 0 && iw < 8) && !(ih > 0 && ih < 8)) {
                var im = {t: "img", src: src, alt: alt, href: href, w: iw > 0 ? iw : 0, h: ih > 0 ? ih : 0, bmp: null, tried: false};
                items.push(im);
                webImgs.push(im);
                space = true;
            } else if (alt != null && alt != "") word("[" + alt + "]", 6);
        }
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
        } else if (it.t == "img") {
            if (!(it.w > 0 && it.h > 0)) continue;          // not loaded yet
            var sc = Math.min(1, maxW / it.w, 360 / it.h), dw = Math.round(it.w * sc), dh = Math.round(it.h * sc);
            if (x > 0 && x + dw + 8 > maxW) flush();
            segs.push({kind: "img", x: x + 4, w: dw, h: dh, img: it});
            x += dw + 8;
            if (h < dh + 6) h = dh + 6;
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
function webTotal() { return webLines.length ? webLines[webLines.length - 1].y + webLines[webLines.length - 1].h : 0; }
function webRender() {
    if (!webPage) return;
    var total = webTotal();
    var maxS = Math.max(0, total - WEB_VIEW);
    if (webScroll > maxS) webScroll = maxS;
    if (webScroll < 0) webScroll = 0;
    for (var i = 0; i < webLines.length; i++) {
        var L = webLines[i];
        var vis = L.y + L.h > webScroll && L.y < webScroll + WEB_VIEW;     // also lines cut by the edges
        if (vis && !L.mc) webMakeLine(L, i);
        if (L.mc) { L.mc._visible = vis; L.mc._y = L.y - webScroll; }
    }
    webThumb._visible = maxS > 0;
    if (maxS > 0) {
        var th = Math.max(30, Math.round(WEB_VIEW * WEB_VIEW / total));
        webThumb.clear();
        box(webThumb, 1, 0, 14, th, C1, INK, 1, 0);
        webThumb.h = th;
        webThumb._y = Math.round((WEB_VIEW - th) * webScroll / maxS);
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
        } else if (g.kind == "img") {
            var pc = mc.createEmptyMovieClip("p" + s, s + 1);
            pc._x = g.x; pc._y = L.h - g.h - 3;
            if (g.img.bmp) {
                var pb = pc.createEmptyMovieClip("b", 1);
                pb.attachBitmap(g.img.bmp, 1, "auto", true);
                pb._xscale = g.w / g.img.bmp.width * 100; pb._yscale = g.h / g.img.bmp.height * 100;
            } else {
                box(pc, 0, 0, g.w, g.h, LIGHT, EDGE, 1, 0);
                if (g.img.alt) label(pc, "a", 2, 4, 4, g.w - 8, 20, g.img.alt, 16, GREY, false);
            }
            if (g.img.href != null) { pc.href = g.img.href; pc.onRelease = function () { webGo(webResolve(this.href)); }; }
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
    if (low.indexOf("byrott:") == 0) return h;
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
    webPage.setMask(wins["web"].body.pmask);        // only the view shows
    measureFonts();
    if (html.indexOf("<") < 0) html = "<pre>" + html.split("&").join("&amp;").split("<").join("&lt;") + "</pre>";
    webGen++;
    if (FlashOS.webImagesReset) FlashOS.webImagesReset();      // the old page's pictures
    if (webStatus && url.indexOf("byrott:") == 0) webStatus.text = "";
    webItems = parseHtml(html);
    webLines = layoutPage(webItems);
    webScroll = 0;
    webUrl = url;
    webUrlF.text = url;
    var w = wins["web"];
    w.titleTxt.text = (webTitle != "" ? ents(webTitle) : url) + " - Internet Navigator 3.0";
    webRender();
    if (url.indexOf("http://") == 0) {
        webAddHistory(url, webTitle != "" ? ents(webTitle) : url);
        webLoadImages();
    }
}
// the page again, after a picture arrived (keeps the scroll position)
function webRelayout() {
    if (!webPage) return;
    var keep = webScroll;
    webPage.removeMovieClip();
    webPage = wins["web"].body.createEmptyMovieClip("page", 5);
    webPage._x = 20; webPage._y = 56;
    webPage.setMask(wins["web"].body.pmask);        // only the view shows
    webLines = layoutPage(webItems);
    webScroll = keep;
    webRender();
}
// pictures load one per frame, so the page can be read meanwhile
var webGen = 0;
function webLoadImages() {
    var gen = webGen, k = 0;
    if (!FlashOS.httpImage || webImgs.length == 0) return;
    var ld = wins["web"].body.createEmptyMovieClip("loader", 120);
    ld.onEnterFrame = function () {
        if (webGen != gen || !wins["web"]) { delete this.onEnterFrame; return; }
        if (k >= webImgs.length) { webStatus.text = "Done"; delete this.onEnterFrame; return; }
        var im = webImgs[k++];
        var u = webResolve(im.src);
        webStatus.text = "Picture " + k + " of " + webImgs.length + "...";
        if (u.toLowerCase().indexOf("http://") != 0) return;     // https pictures cannot be loaded
        var b = FlashOS.httpImage(u);
        if (webGen != gen) return;
        if (b && b.width >= 8 && b.height >= 8) {
            im.bmp = b;
            if (!(im.w > 0 && im.h > 0)) {                         // no size in the page: its own size
                if (im.w > 0) { im.h = Math.round(b.height * im.w / b.width); }
                else if (im.h > 0) { im.w = Math.round(b.width * im.h / b.height); }
                else { im.w = b.width; im.h = b.height; }
            }
            webRelayout();
        }
        else if (im.w > 0 && im.h > 0) webRelayout();
    };
}
// ---- bookmarks and history (saved on the disk)
function listLoad(file) {
    var t = FlashOS.read(file), out = [];
    if (t == null) return null;
    var ls = t.split("\n");
    for (var i = 0; i < ls.length; i++) { var k = ls[i].indexOf("\t"); if (k > 0) out.push([ls[i].substr(0, k), ls[i].substr(k + 1)]); }
    return out;
}
function listSave(file, l) {
    var t = "";
    for (var i = 0; i < l.length; i++) t += l[i][0] + "\t" + l[i][1] + "\n";
    FlashOS.write(file, t);
}
var webMarks = null, webHistory = null;
function marks() {
    if (webMarks == null) webMarks = listLoad("byrott.marks");
    if (webMarks == null) webMarks = [["http://frogfind.com/", "FrogFind"], ["http://68k.news/", "68k.news"], ["http://info.cern.ch/", "The first web site"]];
    return webMarks;
}
function history() {
    if (webHistory == null) webHistory = listLoad("byrott.history");
    if (webHistory == null) webHistory = [];
    return webHistory;
}
function webAddHistory(url, title) {
    var h = history();
    if (h.length && h[0][0] == url) return;
    h.unshift([url, title.split("\t").join(" ").split("\n").join(" ")]);
    while (h.length > 60) h.pop();
    listSave("byrott.history", h);
}
function webAddMark() {
    if (webUrl.indexOf("http://") != 0) { message("Bookmarks", "Only web pages (http://) can be bookmarked."); return; }
    var m = marks();
    for (var i = 0; i < m.length; i++) if (m[i][0] == webUrl) { message("Bookmarks", "This page is already in your bookmarks."); return; }
    m.push([webUrl, webTitle != "" ? ents(webTitle).split("\t").join(" ") : webUrl]);
    listSave("byrott.marks", m);
    message("Bookmarks", "Added to your bookmarks:\n" + m[m.length - 1][1]);
}
function esc(t) { return t.split("&").join("&amp;").split("<").join("&lt;"); }
function webMarksPage() {
    var m = marks(), h = "<title>Bookmarks</title><h1>Bookmarks</h1>";
    if (m.length == 0) h += "<p>No bookmarks yet. Open a page and press Mark.</p>";
    for (var i = 0; i < m.length; i++)
        h += "<p><a href=\"" + m[i][0] + "\">" + esc(m[i][1]) + "</a> <small>" + esc(m[i][0]) + "</small> <a href=\"byrott:unmark?" + i + "\">[remove]</a></p>";
    webShowHtml(h, "byrott:bookmarks");
}
function webHistoryPage() {
    var l = history(), h = "<title>History</title><h1>History</h1><p><a href=\"byrott:clearhistory\">[Clear history]</a></p>";
    if (l.length == 0) h += "<p>No pages visited yet.</p>";
    for (var i = 0; i < l.length; i++) h += "<a href=\"" + l[i][0] + "\">" + esc(l[i][1]) + "</a><br>";
    webShowHtml(h, "byrott:history");
}
function webError(url, msg) {
    webShowHtml("<h2>Sorry, this page cannot be opened</h2><p>" + msg + "</p><p><a href=\"byrott:start\">Back to the start page</a></p>", url);
}
function webGo(url) {
    if (url == "byrott:start") { webShowHtml(START_PAGE, "byrott:start"); return; }
    if (url == "byrott:bookmarks") { webMarksPage(); return; }
    if (url == "byrott:history") { webHistoryPage(); return; }
    if (url == "byrott:clearhistory") { webHistory = []; listSave("byrott.history", []); webHistoryPage(); return; }
    if (url.indexOf("byrott:unmark?") == 0) {
        var m = marks();
        m.splice(parseInt(url.substr(14)), 1);
        listSave("byrott.marks", m);
        webMarksPage();
        return;
    }
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
function webScrollBy(d) { webScroll += d; webRender(); }
function openWeb() {
    var w = makeWindow("web", "Internet Navigator 3.0", WEB_W + 20, 580, false, function () { webPage = null; });
    if (!w) return;
    var b = w.body;
    // the page (depth 5) scrolls under two white covers, so lines cut at the
    // top or bottom of the view are hidden there (the player has no masks)
    var top = b.createEmptyMovieClip("coverTop", 80);
    box(top, -8, -8, WEB_W + 36, 62, WHITE, 0, 0, 0);
    box(top, 0, 0, WEB_W + 20, 50, LIGHT, INK, 2, 0);
    var pm = b.createEmptyMovieClip("pmask", 4);    // the page view, a mask for the page
    box(pm, 0, 56, WEB_W, WEB_VIEW, 0xFFFFFF, 0, 0, 0);
    var bot = b.createEmptyMovieClip("coverBot", 81);
    box(bot, -8, 56 + WEB_VIEW, WEB_W + 36, 600 - WEB_VIEW - 48, WHITE, 0, 0, 0);
    box(bot, 0, 56 + WEB_VIEW, WEB_W + 20, 1, EDGE, 0, 0, 0);
    button(b, "back", 82, 8, 8, 56, 34, "Back", function () { if (webHist.length) { var u = webHist.pop(); webUrl = ""; webGo(u); webHist.pop(); } }, 0, 15);
    button(b, "home", 83, 68, 8, 56, 34, "Home", function () { webGo("byrott:start"); }, 0, 15);
    webUrlF = input(b, "url", 84, 130, 10, WEB_W - 426, 30, 16);
    button(b, "go", 85, WEB_W - 290, 8, 44, 34, "Go", function () { webGo(webUrlF.text); }, 1, 15);
    button(b, "mk", 86, WEB_W - 240, 8, 64, 34, "+Mark", function () { webAddMark(); }, 0, 15);
    button(b, "bm", 87, WEB_W - 170, 8, 92, 34, "Bookmarks", function () { webGo("byrott:bookmarks"); }, 0, 15);
    button(b, "hi", 88, WEB_W - 72, 8, 82, 34, "History", function () { webGo("byrott:history"); }, 0, 15);
    // the scroll bar: click above or below the thumb for a page, drag the thumb
    var bar = b.createEmptyMovieClip("bar", 89);
    bar._x = WEB_W + 2; bar._y = 56;
    var track = bar.createEmptyMovieClip("track", 1);
    box(track, 0, 0, 16, WEB_VIEW, LIGHT, INK, 1, 0);
    track.onPress = function () { webScrollBy((bar._ymouse > webThumb._y ? 1 : -1) * (WEB_VIEW - 40)); };
    track.useHandCursor = false;
    webThumb = bar.createEmptyMovieClip("th", 2);
    webThumb.onPress = function () {
        var grab = bar._ymouse - this._y;
        bar.onEnterFrame = function () {
            var total = webTotal(), maxS = Math.max(0, total - WEB_VIEW), room = WEB_VIEW - webThumb.h;
            if (room > 0) { webScroll = Math.round((bar._ymouse - grab) / room * maxS); webRender(); }
        };
    };
    webThumb.onRelease = webThumb.onReleaseOutside = function () { delete bar.onEnterFrame; };
    webThumb.useHandCursor = false;
    webStatus = label(b, "st", 90, 12, 64 + WEB_VIEW + 8, 600, 24, "", 13, GREY, false);
    button(b, "dn", 91, WEB_W - 76, 64 + WEB_VIEW, 86, 32, "Down", function () { webScrollBy(WEB_VIEW - 60); }, 0, 14);
    button(b, "up", 92, WEB_W - 168, 64 + WEB_VIEW, 86, 32, "Up", function () { webScrollBy(-(WEB_VIEW - 60)); }, 0, 14);
    w.focusField = null;
    webHist = [];
    webUrl = "";
    webShowHtml(START_PAGE, "byrott:start");
}
// the mouse wheel scrolls web pages (and the Command Runner)
var wheelL = {};
wheelL.onMouseWheel = function (delta) {
    idle = getTimer();
    if (webPage && isFront("web")) { webScrollBy(-delta * 16); return; }
    if (termOut && isFront("term")) { termOut.scroll -= delta; return; }
    if (noteTF && isFront("notepad")) { noteTF.scroll -= delta; }
};
Mouse.addListener(wheelL);

// ================================================================ MUSIC PLAYER
// plays the MP3 files on the disk, CD and USB; keeps playing when you hide it
var musicList = [], musicIdx = -1, musicFirst = 0, musicPaused = false;
function mmss(t) { t = Math.floor(t); return Math.floor(t / 60) + ":" + two(t % 60); }
function musicFiles() {
    var all = FlashOS.files(""), out = [];
    for (var i = 0; i < all.length; i++) if (ext(all[i].name) == "mp3" && !hiddenFile(all[i])) out.push(all[i].name);
    return out;
}
function openMusic(file) {
    var w = wins["music"];
    if (!w) {
        w = makeWindow("music", "Music Player", 600, 420, false, function () { FlashOS.musicStop(); musicIdx = -1; });
        var b = w.body;
        var top = b.createEmptyMovieClip("top", 1);
        box(top, 0, 0, 600, 120, 0x101010, INK, 2, 0);
        w.now = label(b, "now", 2, 14, 10, 572, 36, "No song", 32, 0x40FF60, false);
        w.time = label(b, "time", 3, 14, 50, 300, 22, "0:00 / 0:00", 16, 0x40FF60, false);
        var bar = b.createEmptyMovieClip("bar", 4);
        bar._x = 14; bar._y = 84;
        box(bar, 0, 0, 572, 18, 0x303030, 0x40FF60, 1, 0);
        w.fill = bar.createEmptyMovieClip("fill", 1);
        bar.onRelease = function () {           // click the bar to jump
            var st = FlashOS.musicState();
            if (st.len > 0) FlashOS.musicSeek(Math.round(this._xmouse / 572 * st.len));
        };
        button(b, "pv", 10, 0, 130, 80, 40, "|<", function () { musicGo(musicIdx - 1); }, 0, 16);
        w.pp = button(b, "pp", 11, 86, 130, 120, 40, "Play", function () { musicPlayPause(); }, 1, 16);
        button(b, "st", 12, 212, 130, 90, 40, "Stop", function () { FlashOS.musicStop(); musicPaused = false; w.pp.lab.text = "Play"; }, 0, 16);
        button(b, "nx", 13, 308, 130, 80, 40, ">|", function () { musicGo(musicIdx + 1); }, 0, 16);
        button(b, "up", 14, 480, 130, 50, 40, "^", function () { if (musicFirst > 0) { musicFirst -= 5; if (musicFirst < 0) musicFirst = 0; musicDraw(); } }, 0, 16);
        button(b, "dn", 15, 536, 130, 50, 40, "v", function () { if (musicFirst + 5 < musicList.length) { musicFirst += 5; musicDraw(); } }, 0, 16);
        var tick2 = 0;
        b.onEnterFrame = function () {
            if (++tick2 % 10) return;
            var st = FlashOS.musicState();
            w.time.text = mmss(st.pos) + " / " + mmss(st.len);
            w.fill.clear();
            if (st.len > 0) box(w.fill, 1, 1, Math.min(570, 570 * st.pos / st.len), 16, 0x40FF60, 0, 0, 0);
            if (st.done && !st.playing && musicIdx >= 0 && !musicPaused) musicGo(musicIdx + 1 < musicList.length ? musicIdx + 1 : 0);
        };
    } else focusWin(w);
    musicList = musicFiles();
    musicDraw();
    if (file) for (var i = 0; i < musicList.length; i++) if (musicList[i] == file) musicGo(i);
}
function musicGo(i) {
    var w = wins["music"];
    if (!w || musicList.length == 0) return;
    if (i < 0) i = musicList.length - 1;
    if (i >= musicList.length) i = 0;
    var r = FlashOS.musicPlay(musicList[i]);
    musicIdx = i;
    musicPaused = false;
    if (r != "ok") { w.now.text = "Cannot play: " + r; w.pp.lab.text = "Play"; }
    else { var n = base(musicList[i]); w.now.text = n.substr(0, n.length - 4); w.pp.lab.text = "Pause"; }
    musicDraw();
}
function musicPlayPause() {
    var w = wins["music"], st = FlashOS.musicState();
    if (musicIdx < 0 || (!st.playing && !musicPaused)) { musicGo(musicIdx < 0 ? 0 : musicIdx); return; }
    musicPaused = !musicPaused;
    FlashOS.musicPause(musicPaused);
    w.pp.lab.text = musicPaused ? "Play" : "Pause";
}
function musicDraw() {
    var w = wins["music"];
    if (!w) return;
    var c = w.body.createEmptyMovieClip("list", 20);
    c._y = 180;
    box(c, 0, 0, 600, 5 * 44 + 4, WHITE, INK, 2, 0);
    if (musicList.length == 0) label(c, "e", 1, 0, 90, 600, 22, "No MP3 files. Put some on the disk, a CD or a USB stick.", 16, GREY, false, "center");
    for (var r = 0; r < 5; r++) {
        var i = musicFirst + r;
        if (i >= musicList.length) break;
        var row = c.createEmptyMovieClip("r" + r, 10 + r);
        row._x = 2; row._y = 2 + r * 44;
        box(row, 0, 0, 596, 44, i == musicIdx ? C3 : (r % 2 ? 0xF4F4F4 : WHITE), 0, 0, 0);
        icon(row, "ic", 1, 8, 6, "music", true);
        label(row, "n", 2, 48, 12, 540, 22, base(musicList[i]), 16, INK, false);
        row.k = i;
        row.onRelease = function () { musicGo(this.k); };
    }
}

// ================================================================ MORE APPS (the store)
// games and apps to get with one click; "Get from the web" downloads an SWF
var STORE = [["snake", "Snake", "Eat the apples and grow; do not bite your tail!", "game"],
             ["mines", "Minesweeper", "Open all the squares without stepping on a mine.", "game"],
             ["solitaire", "Solitaire", "The card game: build the four piles from Ace to King.", "game"],
             ["maze", "Maze 3D", "Find the way out of 3D mazes, level after level (SWF).", "swf", "store_maze.swf", "maze3d.swf"],
             ["fnote", "Flash Notepad", "Texts, drawings, pictures and videos, in Comic style (SWF).", "swf", "store_flashnotepad.swf", "flashnotepad.swf"]];
function storeHas(it) {
    if (it[3] == "game") { var g = cfg.games.split(","); for (var i = 0; i < g.length; i++) if (g[i] == it[0]) return true; return false; }
    return FlashOS.exists(it[5]);
}
function openStore() {
    var w = makeWindow("store", "More Apps", 760, 590, false, null);
    if (!w) return;
    storeDraw();
}
function storeDraw() {
    var w = wins["store"];
    if (!w) return;
    var c = w.body.createEmptyMovieClip("page", 10);
    var hd = c.createEmptyMovieClip("hd", 1);
    titleBar(hd, 0, 0, 760, 44);
    label(c, "t", 2, 70, 6, 600, 36, "More Apps", 32, onC1(), false);
    icon(c, "si", 3, 16, 4, "store_s", false);
    for (var i = 0; i < STORE.length; i++) {
        var it = STORE[i], has = storeHas(it);
        var r = c.createEmptyMovieClip("r" + i, 10 + i);
        r._y = 52 + i * 84;
        box(r, 0, 0, 760, 80, i % 2 ? 0xF4F4F4 : WHITE, INK, 1, 0);
        icon(r, "ic", 1, 8, 8, it[3] == "game" ? it[0] : "swf", false);
        label(r, "n", 2, 84, 6, 440, 36, it[1], 32, INK, false);
        label(r, "d", 3, 86, 46, 470, 22, it[2], 16, GREY, false);
        r.it = it;
        if (has) {
            var ob = button(r, "open", 4, 560, 20, 90, 40, "Open", null, 1, 16);
            ob.it = it;
            ob.onRelease = function () { if (this.it[3] == "game") launch(this.it[0]); else runSWF(this.it[5], "desk"); };
            var rb = button(r, "rm", 5, 656, 20, 96, 40, "Remove", null, 0, 16);
            rb.it = it;
            rb.onRelease = function () { storeRemove(this.it); };
        } else {
            var gb = button(r, "get", 4, 620, 20, 132, 40, "Get", null, 1, 16);
            gb.it = it;
            gb.onRelease = function () { storeGet(this.it); };
        }
    }
    var y = 52 + STORE.length * 84 + 10;
    label(c, "wl", 40, 0, y, 760, 22, "Get from the web (an http:// address of an .swf file):", 16, INK, false);
    var u = input(c, "url", 41, 0, y + 28, 600, 30, 16);
    u.text = "http://";
    button(c, "dl", 42, 610, y + 24, 150, 38, "Download", function () { storeDownload(u.text); }, 0, 16);
}
function storeGet(it) {
    if (it[3] == "game") {
        cfg.games = cfg.games == "" ? it[0] : cfg.games + "," + it[0];
        saveCfg();
        deskAdd("app", it[0]);
    } else {
        if (!FlashOS.copyFile(it[4], it[5])) { message("More Apps", "Sorry, " + it[1] + " could not be installed. The disk may be full."); return; }
        deskAdd("file", it[5]);
    }
    storeDraw();
    paintDock();
    message("More Apps", it[1] + " is installed! It is on your desktop" + (it[3] == "game" ? " and in the start menu." : "."));
}
function storeRemove(it) {
    ask("More Apps", "Remove " + it[1] + "?", ["Yes", "No"], function (k) {
        if (k != 0) return;
        if (it[3] == "game") {
            var g = cfg.games.split(","), out = [];
            for (var i = 0; i < g.length; i++) if (g[i] != it[0] && g[i] != "") out.push(g[i]);
            cfg.games = out.join(",");
            var d = cfg.dock.split(","), d2 = [];
            for (i = 0; i < d.length; i++) if (d[i] != it[0]) d2.push(d[i]);
            if (d2.length >= 4) cfg.dock = d2.join(",");
            saveCfg();
            if (wins[it[0]]) closeWindow(wins[it[0]]);
            deskRemove("app", it[0]);
        } else {
            FlashOS.remove(it[5]);
            deskRemove("file", it[5]);
        }
        storeDraw();
        paintDock();
    });
}
function storeDownload(url) {
    if (url.toLowerCase().indexOf("http://") != 0) { message("More Apps", "Type an address that starts with http:// (https:// sites cannot be opened)."); return; }
    var name = base(url);
    var q = name.indexOf("?");
    if (q >= 0) name = name.substr(0, q);
    if (name == "" || ext(name) != "swf") { message("More Apps", "The address must end with the name of an .swf file."); return; }
    if (!netOk) { var n = FlashOS.netConnect(); if (n != "ok") { message("More Apps", "No internet connection: " + n + "."); return; } netOk = true; }
    var r = FlashOS.download(url, name);
    if (r != "ok") { message("More Apps", "Sorry, it could not be downloaded: " + r + "."); return; }
    deskAdd("file", name);
    message("More Apps", name + " was downloaded. It is on your desktop.");
}

// ================================================================ GAMES (they run inside ByroTT, so they multitask)
function gameKeys(code) {
    if (isFront("snake")) { snakeKey(code); return code >= 37 && code <= 40 || code == 32; }
    if (isFront("ball")) return ballKey(code);
    return false;
}
function gameLabel(mc, name, depth, x, y, w, h, text, size, color, align) {
    var t = label(mc, name, depth, x, y, w, h, "", size, color, false, align);
    t.multiline = true; t.wordWrap = true;
    t.text = text;
    return t;
}
function gameMsg(g, title, sub, w, h) {
    if (g.msg) g.msg.removeMovieClip();
    if (title == "") return;
    var m = g.createEmptyMovieClip("msg", 900);
    var x = Math.round((g.W - w) / 2), y = Math.round((g.H - h) / 2);
    box(m, x, y, w, h, WHITE, INK, 4, 0);
    gameLabel(m, "t", 1, x, y + 16, w, 40, title, 32, INK, "center");
    gameLabel(m, "s", 2, x + 16, y + 64, w - 32, h - 70, sub, 16, INK, "center");
}

// ---- Ball Game v2.0 (dodge the balls)
var BALL_HI = "ballgame.hi";
function openBall() {
    var w = makeWindow("ball", "Ball Game v2.0", 800, 540, false, function () { delete w.body.onEnterFrame; });
    if (!w) return;
    var g = w.body;
    var GW = 800, GH = 540, TOPY = 76, BAR_H = 22, BAR_Y = GH - 56, BAR_W0 = 130, R = 30;
    g.W = GW; g.H = GH;
    box(g, 0, 0, GW, GH, 0x990000, 0, 0, 0);
    gameLabel(g, "title", 2, 24, 18, 420, 40, "TRY DODGE THE BALL!", 32, WHITE);
    var scoreT = gameLabel(g, "score", 3, GW - 360, 12, 340, 22, "", 16, WHITE, "right");
    var levelT = gameLabel(g, "level", 4, GW - 360, 34, 340, 22, "", 16, 0xFFD040, "right");
    var powT = gameLabel(g, "pow", 5, 24, GH - 26, 740, 22, "", 16, WHITE);
    var bar = g.createEmptyMovieClip("bar", 10);
    var barW = BAR_W0, shield = false, bx = (GW - BAR_W0) / 2;
    function drawBar() {
        bar.clear();
        bar.lineStyle(4, shield ? 0x40A0FF : 0x000000, 100);
        bar.beginFill(shield ? 0xC8E4FF : 0xFFFFFF, 100);
        bar.moveTo(0, 0); bar.lineTo(barW, 0); bar.lineTo(barW, BAR_H); bar.lineTo(0, BAR_H); bar.lineTo(0, 0);
        bar.endFill();
        bar.lineStyle();
    }
    bar._y = BAR_Y;
    drawBar();
    var balls = [], pows = [], nextId = 0, t0 = 0, playing = false, lastMx = -1, useMouse = false;
    var level = 1, points = 0, slowUntil = 0, wideUntil = 0, nextPow = 0, lastTick = 0, naming = null;
    var scores = listLoad(BALL_HI);
    if (scores == null) scores = [];
    function best() { return scores.length ? Number(scores[0][1]) : 0; }
    function table() {
        var t = "BEST SCORES\n";
        if (scores.length == 0) return t + "(none yet)\n";
        for (var i = 0; i < scores.length; i++) t += (i + 1) + ". " + scores[i][0] + "  " + scores[i][1] + "\n";
        return t;
    }
    function hud() { scoreT.text = "POINTS " + points + "   BEST " + Math.max(best(), points); levelT.text = "LEVEL " + level; }
    function disc(mc, r, fill) {
        mc.lineStyle(5, INK, 100); mc.beginFill(fill, 100); mc.moveTo(r, 0);
        for (var i = 1; i <= 28; i++) { var a = i / 28 * Math.PI * 2; mc.lineTo(Math.cos(a) * r, Math.sin(a) * r); }
        mc.endFill(); mc.lineStyle();
    }
    function addBall() {
        var b = g.createEmptyMovieClip("ball" + nextId, 20 + (nextId % 200));
        nextId++;
        disc(b, R, WHITE);
        b._x = R + Math.random() * (GW - 2 * R); b._y = TOPY + R;
        var sp = 4.5 + level * 0.9, a = (0.25 + Math.random() * 0.5) * Math.PI;
        b.vx = Math.cos(a) * sp * (Math.random() < 0.5 ? -1 : 1); b.vy = Math.sin(a) * sp;
        balls.push(b);
    }
    var POWS = [["S", 0x40A0FF], ["T", 0x22CC55], ["W", 0xFFD040]];
    function addPow() {
        var k = Math.floor(Math.random() * 3);
        var p = g.createEmptyMovieClip("pow" + nextId, 230 + (nextId % 50));
        nextId++;
        box(p, -18, -18, 36, 36, POWS[k][1], INK, 3, 0);
        gameLabel(p, "t", 1, -18, -10, 36, 22, POWS[k][0], 16, INK, "center");
        p.kind = POWS[k][0]; p._x = 40 + Math.random() * (GW - 80); p._y = TOPY;
        pows.push(p);
    }
    function start() {
        for (var i = 0; i < balls.length; i++) balls[i].removeMovieClip();
        for (i = 0; i < pows.length; i++) pows[i].removeMovieClip();
        balls = []; pows = [];
        level = 1; points = 0; shield = false; barW = BAR_W0; slowUntil = 0; wideUntil = 0;
        drawBar(); addBall();
        t0 = getTimer(); lastTick = t0; nextPow = t0 + 6000;
        playing = true; powT.text = "";
        gameMsg(g, "", "");
    }
    function over() {
        playing = false;
        if (points > 0 && (scores.length < 5 || points > Number(scores[scores.length - 1][1]))) {
            gameMsg(g, "NEW BEST SCORE!", points + " POINTS\n\nTYPE YOUR NAME AND PRESS ENTER:", 460, 230);
            naming = input(g.msg, "nm", 3, 230, 330, 340, 28, 16);
            naming.maxChars = 12;
            Selection.setFocus(naming);
            w.focusField = naming;
        } else gameMsg(g, "THE BALL HIT YOU!", "LEVEL " + level + "   " + points + " POINTS\n\n" + table() + "\nPRESS SPACE OR CLICK TO PLAY AGAIN", 460, 330);
    }
    // keys from gameKeys (only when this window is in front)
    g.key = function (code) {
        if (naming) {
            if (code == 13) {
                var n = naming.text == "" ? "PLAYER" : naming.text.toUpperCase();
                naming = null; w.focusField = null; Selection.setFocus(null);
                scores.push([n, "" + points]);
                scores.sort(function (a, b) { return Number(b[1]) - Number(a[1]); });
                while (scores.length > 5) scores.pop();
                listSave(BALL_HI, scores);
                gameMsg(g, "SAVED!", "LEVEL " + level + "   " + points + " POINTS\n\n" + table() + "\nPRESS SPACE OR CLICK TO PLAY AGAIN", 460, 330);
            }
            return true;
        }
        if (code == 32 && !playing) { start(); return true; }
        return code == 37 || code == 39;
    };
    var hit = g.createEmptyMovieClip("hit", 1);
    box(hit, 0, 0, GW, GH, 0xFFFFFF, 0, 0, 0);
    hit._alpha = 0;
    hit.useHandCursor = false;
    hit.onPress = function () { if (!playing && !naming) start(); };
    g.onEnterFrame = function () {
        if (!isFront("ball")) return;              // paused while another window is in front
        if (g._xmouse != lastMx) { if (lastMx >= 0) useMouse = true; lastMx = g._xmouse; }
        if (Key.isDown(37)) { bx -= 11; useMouse = false; }
        if (Key.isDown(39)) { bx += 11; useMouse = false; }
        if (useMouse && g._xmouse >= 0 && g._xmouse <= GW && g._ymouse >= 0 && g._ymouse <= GH) bx = g._xmouse - barW / 2;
        if (bx < 0) bx = 0;
        if (bx > GW - barW) bx = GW - barW;
        bar._x = bx;
        if (!playing) return;
        var now = getTimer();
        while (now - lastTick >= 100) { lastTick += 100; points += level; }
        var nl = 1 + Math.floor((now - t0) / 15000);
        if (nl != level) { level = nl; if (balls.length < 6) addBall(); }
        if (now >= nextPow) { addPow(); nextPow = now + 7000 + Math.random() * 5000; }
        if (wideUntil && now > wideUntil) { wideUntil = 0; barW = BAR_W0; drawBar(); }
        var slow = now < slowUntil ? 0.5 : 1, st = "";
        if (shield) st += "SHIELD   ";
        if (slow < 1) st += "SLOW TIME " + Math.ceil((slowUntil - now) / 1000) + "   ";
        if (wideUntil) st += "WIDE BAR " + Math.ceil((wideUntil - now) / 1000);
        powT.text = st;
        hud();
        for (var p = pows.length - 1; p >= 0; p--) {
            var q = pows[p];
            q._y += 3;
            if (q._y + 18 >= BAR_Y && q._y - 18 <= BAR_Y + BAR_H && q._x + 18 >= bx && q._x - 18 <= bx + barW) {
                if (q.kind == "S") { shield = true; drawBar(); }
                if (q.kind == "T") slowUntil = now + 6000;
                if (q.kind == "W") { wideUntil = now + 8000; barW = 210; drawBar(); }
                q.removeMovieClip(); pows.splice(p, 1);
            } else if (q._y > GH + 20) { q.removeMovieClip(); pows.splice(p, 1); }
        }
        for (var i = 0; i < balls.length; i++) {
            var b = balls[i];
            b._x += b.vx * slow; b._y += b.vy * slow;
            if (b._x < R) { b._x = R; b.vx = -b.vx; }
            if (b._x > GW - R) { b._x = GW - R; b.vx = -b.vx; }
            if (b._y < TOPY + R) { b._y = TOPY + R; b.vy = -b.vy; }
            if (b._y > GH - R) { b._y = GH - R; b.vy = -b.vy; }
            var nx = Math.max(bx, Math.min(b._x, bx + barW)), ny = Math.max(BAR_Y, Math.min(b._y, BAR_Y + BAR_H));
            var dx = b._x - nx, dy = b._y - ny;
            if (dx * dx + dy * dy < (R - 2) * (R - 2)) {
                if (shield) { shield = false; drawBar(); b.vy = -Math.abs(b.vy); b._y = BAR_Y - R - 2; continue; }
                over();
                return;
            }
        }
    };
    hud();
    gameMsg(g, "BALL GAME v2.0", "ARROWS OR MOUSE MOVE THE WHITE BAR.\nDO NOT LET THE BALLS TOUCH IT!\nCATCH  S = SHIELD   T = SLOW TIME   W = WIDE BAR\n\n" + table() + "\nPRESS SPACE OR CLICK TO START", 520, 380);
}
function ballKey(code) { var w = wins["ball"]; return w ? w.body.key(code) : false; }

// ---- Snake
var snakeDir = 1, snakeNext = 1;
function openSnake() {
    var w = makeWindow("snake", "Snake", 600, 500, false, null);
    if (!w) return;
    var g = w.body, CS = 24, CW = 25, CH = 19, OY = 40;
    g.W = 600; g.H = 500;
    box(g, 0, 0, 600, 500, 0x1E8C3C, 0, 0, 0);
    box(g, 0, OY, CW * CS, CH * CS, 0x2FAE5B, INK, 2, 0);
    var sc = gameLabel(g, "sc", 2, 10, 8, 300, 24, "", 16, WHITE);
    var bt = gameLabel(g, "bt", 3, 290, 8, 300, 24, "", 16, WHITE, "right");
    var layer = g.createEmptyMovieClip("l", 10);
    var body, food, alive = false, score = 0, frames = 0, speed = 8;
    var best = cfg.snakebest == undefined ? 0 : Number(cfg.snakebest);
    function cell(x, y, col) { box(layer, x * CS + 1, OY + y * CS + 1, CS - 2, CS - 2, col, INK, 2, 0); }
    function placeFood() {
        while (true) {
            var fx = Math.floor(Math.random() * CW), fy = Math.floor(Math.random() * CH), ok = true;
            for (var i = 0; i < body.length; i++) if (body[i][0] == fx && body[i][1] == fy) ok = false;
            if (ok) { food = [fx, fy]; return; }
        }
    }
    function draw() {
        layer.clear();
        circle(layer, food[0] * CS + CS / 2, OY + food[1] * CS + CS / 2, CS / 2 - 2, 0xE02828);
        for (var i = 0; i < body.length; i++) cell(body[i][0], body[i][1], i == 0 ? 0xC8FF60 : 0x78E650);
        sc.text = "SCORE " + score;
        bt.text = "BEST " + best;
    }
    function start() {
        body = [[12, 9], [11, 9], [10, 9]];
        snakeDir = snakeNext = 1;          // 0 up, 1 right, 2 down, 3 left
        score = 0; speed = 8; alive = true;
        placeFood();
        gameMsg(g, "", "");
        draw();
    }
    g.start = start;
    g.alive = function () { return alive; };
    g.onEnterFrame = function () {
        if (!alive || !isFront("snake")) return;
        if (++frames % speed) return;
        if ((snakeNext + 2) % 4 != snakeDir) snakeDir = snakeNext;
        var h = body[0], nx = h[0] + (snakeDir == 1 ? 1 : snakeDir == 3 ? -1 : 0), ny = h[1] + (snakeDir == 2 ? 1 : snakeDir == 0 ? -1 : 0);
        var dead = nx < 0 || ny < 0 || nx >= CW || ny >= CH;
        for (var i = 0; i < body.length - 1 && !dead; i++) if (body[i][0] == nx && body[i][1] == ny) dead = true;
        if (dead) {
            alive = false;
            if (score > best) { best = score; cfg.snakebest = "" + best; saveCfg(); }
            gameMsg(g, "GAME OVER", "SCORE " + score + "   BEST " + best + "\n\nPRESS SPACE TO PLAY AGAIN", 400, 170);
            return;
        }
        body.unshift([nx, ny]);
        if (nx == food[0] && ny == food[1]) { score += 10; if (score % 50 == 0 && speed > 3) speed--; placeFood(); }
        else body.pop();
        draw();
    };
    body = [[12, 9], [11, 9], [10, 9]];
    food = [18, 9];
    draw();
    gameMsg(g, "SNAKE", "ARROW KEYS TURN THE SNAKE.\nEAT THE RED APPLES, DO NOT HIT THE WALLS OR YOURSELF.\n\nPRESS SPACE TO START", 460, 200);
}
function snakeKey(code) {
    var g = wins["snake"].body;
    if (code == 38) snakeNext = 0;
    if (code == 39) snakeNext = 1;
    if (code == 40) snakeNext = 2;
    if (code == 37) snakeNext = 3;
    if (code == 32 && !g.alive()) g.start();
}

// ---- Minesweeper
function minesAround(mine, N, i) {
    var x = i % N, y = Math.floor(i / N), c = 0;
    for (var dy = -1; dy <= 1; dy++) for (var dx = -1; dx <= 1; dx++) {
        if (!dx && !dy) continue;
        var nx = x + dx, ny = y + dy;
        if (nx >= 0 && ny >= 0 && nx < N && ny < N && mine[ny * N + nx]) c++;
    }
    return c;
}
function openMines() {
    var N = 16, M = 40, CS = 30;
    var w = makeWindow("mines", "Minesweeper", N * CS + 20, N * CS + 70, false, null);
    if (!w) return;
    var g = w.body;
    g.W = N * CS + 20; g.H = N * CS + 70;
    box(g, 0, 0, g.W, g.H, 0xC0C0C0, 0, 0, 0);
    var count = gameLabel(g, "c", 2, 10, 12, 120, 24, "", 16, 0xC00000);
    var timeT = gameLabel(g, "tm", 3, g.W - 130, 12, 120, 24, "", 16, 0xC00000, "right");
    var face = button(g, "face", 4, 250, 6, 110, 34, "New game", function () { reset(); }, 0, 16);
    var flagBtn = button(g, "flag", 5, 128, 6, 110, 34, "Flag: off", function () { flagMode = !flagMode; flagBtn.lab.text = flagMode ? "Flag: ON" : "Flag: off"; flagBtn.setOn(flagMode); }, 2, 16);
    var grid = g.createEmptyMovieClip("grid", 10);
    grid._x = 10; grid._y = 50;
    var mine, open, flag, near, started, over, flagMode = false, t0, opened;
    function reset() {
        mine = []; open = []; flag = []; near = [];
        for (var i = 0; i < N * N; i++) { mine[i] = false; open[i] = false; flag[i] = false; near[i] = 0; }
        started = false; over = false; opened = 0;
        timeT.text = "TIME 0";
        draw();
    }
    function layMines(safe) {
        var k = 0;
        while (k < M) {
            var i = Math.floor(Math.random() * N * N);
            if (mine[i] || Math.abs(i % N - safe % N) <= 1 && Math.abs(Math.floor(i / N) - Math.floor(safe / N)) <= 1) continue;
            mine[i] = true; k++;
        }
        for (i = 0; i < N * N; i++) near[i] = minesAround(mine, N, i);
    }
    var COLS = [0, 0x0000FF, 0x008000, 0xFF0000, 0x000080, 0x800000, 0x008080, 0x000000, 0x808080];
    function draw() {
        for (var i = 0; i < N * N; i++) {
            var c = grid["c" + i];
            if (!c) {
                c = grid.createEmptyMovieClip("c" + i, i + 1);
                c._x = (i % N) * CS; c._y = Math.floor(i / N) * CS;
                c.k = i;
                c.onRelease = function () { press(this.k, flagMode || Key.isDown(16)); };
            }
            c.clear();
            if (c.t) c.t.removeTextField();
            if (!c.fl) c.fl = c.createEmptyMovieClip("fl", 2);
            c.fl.clear();
            if (open[i]) {
                box(c, 0, 0, CS, CS, mine[i] ? 0xFF4040 : 0xE8E8E8, 0x808080, 1, 0);
                if (mine[i]) circle(c.fl, CS / 2, CS / 2, 8, INK);
                else if (near[i]) gameLabel(c, "t", 1, 0, 6, CS, 22, "" + near[i], 16, COLS[near[i]], "center");
            } else {
                box(c, 0, 0, CS, CS, 0xC0C0C0, 0, 0, 0);
                bevel(c.fl, 0, 0, CS, CS, true);
                box(c.fl, 0, 0, CS, 2, WHITE, 0, 0, 0); box(c.fl, 0, 0, 2, CS, WHITE, 0, 0, 0);
                if (flag[i]) { box(c.fl, 13, 6, 2, 18, INK, 0, 0, 0); c.fl.beginFill(0xFF0000, 100); c.fl.moveTo(15, 6); c.fl.lineTo(24, 10); c.fl.lineTo(15, 14); c.fl.endFill(); }
                if (over && mine[i] && !flag[i]) circle(c.fl, CS / 2, CS / 2, 8, INK);
            }
        }
        var f = 0;
        for (i = 0; i < N * N; i++) if (flag[i]) f++;
        count.text = "MINES " + (M - f);
    }
    function press(i, asFlag) {
        if (over || open[i]) return;
        if (asFlag) { flag[i] = !flag[i]; draw(); return; }
        if (flag[i]) return;
        if (!started) { started = true; layMines(i); t0 = getTimer(); }
        if (mine[i]) {
            over = true; open[i] = true; draw();
            message("Minesweeper", "BOOM! You stepped on a mine.\nPress New game to try again.");
            return;
        }
        var stack = [i];
        while (stack.length) {
            var k = stack.pop();
            if (open[k] || flag[k]) continue;
            open[k] = true; opened++;
            if (near[k] == 0) {
                var x = k % N, y = Math.floor(k / N);
                for (var dy = -1; dy <= 1; dy++) for (var dx = -1; dx <= 1; dx++) {
                    var nx = x + dx, ny = y + dy;
                    if (nx >= 0 && ny >= 0 && nx < N && ny < N && !open[ny * N + nx]) stack.push(ny * N + nx);
                }
            }
        }
        draw();
        if (opened == N * N - M) {
            over = true;
            message("Minesweeper", "You found all the mines in " + Math.floor((getTimer() - t0) / 1000) + " seconds. Well done!");
        }
    }
    g.onEnterFrame = function () { if (started && !over) timeT.text = "TIME " + Math.floor((getTimer() - t0) / 1000); };
    reset();
}

// ---- Solitaire (Klondike, one card at a time): click a card, then where it goes
// the four suits, drawn (s: 0 spades, 1 hearts, 2 diamonds, 3 clubs) around cx, cy
function suit(mc, cx, cy, r, s, col) {
    mc.beginFill(col, 100);
    if (s == 2) { mc.moveTo(cx, cy - r); mc.lineTo(cx + r * 0.75, cy); mc.lineTo(cx, cy + r); mc.lineTo(cx - r * 0.75, cy); mc.lineTo(cx, cy - r); mc.endFill(); return; }
    if (s == 1) {                                  // heart: two circles and a point
        mc.endFill();
        circle(mc, cx - r * 0.45, cy - r * 0.3, r * 0.5, col);
        circle(mc, cx + r * 0.45, cy - r * 0.3, r * 0.5, col);
        mc.beginFill(col, 100);
        mc.moveTo(cx - r * 0.93, cy - r * 0.15); mc.lineTo(cx + r * 0.93, cy - r * 0.15); mc.lineTo(cx, cy + r); mc.lineTo(cx - r * 0.93, cy - r * 0.15);
        mc.endFill();
        return;
    }
    mc.endFill();
    if (s == 0) {                                  // spade: an upside-down heart and a stem
        circle(mc, cx - r * 0.45, cy + r * 0.2, r * 0.5, col);
        circle(mc, cx + r * 0.45, cy + r * 0.2, r * 0.5, col);
        mc.beginFill(col, 100);
        mc.moveTo(cx - r * 0.93, cy + r * 0.05); mc.lineTo(cx + r * 0.93, cy + r * 0.05); mc.lineTo(cx, cy - r); mc.lineTo(cx - r * 0.93, cy + r * 0.05);
        mc.endFill();
    } else {                                       // club: three circles and a stem
        circle(mc, cx, cy - r * 0.45, r * 0.42, col);
        circle(mc, cx - r * 0.48, cy + r * 0.12, r * 0.42, col);
        circle(mc, cx + r * 0.48, cy + r * 0.12, r * 0.42, col);
    }
    mc.beginFill(col, 100);
    mc.moveTo(cx - r * 0.12, cy); mc.lineTo(cx + r * 0.12, cy); mc.lineTo(cx + r * 0.35, cy + r); mc.lineTo(cx - r * 0.35, cy + r); mc.lineTo(cx - r * 0.12, cy);
    mc.endFill();
}
function openSolitaire() {
    var w = makeWindow("solitaire", "Solitaire", 860, 570, false, null);
    if (!w) return;
    var g = w.body, CW = 84, CHh = 116;
    g.W = 860; g.H = 570;
    var RANKS = ["A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"];
    var stock, waste, found, tab, sel = null, moves = 0;
    box(g, 0, 0, 860, 570, 0x0A7A3A, 0, 0, 0);
    var info = gameLabel(g, "info", 2, 200, 12, 260, 22, "", 16, WHITE);
    button(g, "nw", 3, 200, 60, 120, 36, "New game", function () { deal(); }, 0, 16);
    var table = g.createEmptyMovieClip("t", 10);
    function red(c) { return c.s == 1 || c.s == 2; }
    function deal() {
        var deck = [];
        for (var s = 0; s < 4; s++) for (var r = 0; r < 13; r++) deck.push({s: s, r: r, up: false});
        for (var i = deck.length - 1; i > 0; i--) { var j = Math.floor(Math.random() * (i + 1)), t = deck[i]; deck[i] = deck[j]; deck[j] = t; }
        tab = [];
        for (i = 0; i < 7; i++) { tab[i] = []; for (j = 0; j <= i; j++) { var c = deck.pop(); c.up = j == i; tab[i].push(c); } }
        stock = deck; waste = []; found = [[], [], [], []]; sel = null; moves = 0;
        draw();
    }
    function card(mc, name, depth, x, y, c, hl) {
        var k = mc.createEmptyMovieClip(name, depth);
        k._x = x; k._y = y;
        if (!c) { box(k, 0, 0, CW, CHh, 0x0A6A32, 0x80C090, 2, 0); return k; }
        if (!c.up) {
            box(k, 0, 0, CW, CHh, WHITE, INK, 2, 0);
            var bk = k.createEmptyMovieClip("bk", 1);
            box(bk, 5, 5, CW - 10, CHh - 10, C1 == 0xB4FFFF ? 0x1E7BFF : C1, 0, 0, 0);
            return k;
        }
        box(k, 0, 0, CW, CHh, hl ? 0xFFF4A0 : WHITE, hl ? 0xFF8C00 : INK, hl ? 4 : 2, 0);
        var col = red(c) ? 0xD00000 : INK;
        label(k, "a", 1, 5, 3, 60, 22, RANKS[c.r], 16, col, false);
        var sm = k.createEmptyMovieClip("s", 3);         // own clip: overlapping fills cancel clicks
        suit(sm, 12, 32, 7, c.s, col);
        suit(sm, CW / 2, CHh / 2 + 6, 20, c.s, col);
        return k;
    }
    function draw() {
        table.removeMovieClip();
        table = g.createEmptyMovieClip("t", 10);
        var d = 1, i, j;
        // stock and waste
        var st = card(table, "st", d++, 10, 10, stock.length ? {up: false} : null, false);
        st.onRelease = function () { clickStock(); };
        if (waste.length) {
            var wc = card(table, "wa", d++, 104, 10, waste[waste.length - 1], sel && sel.p == "w");
            wc.onRelease = function () { clickPile("w", 0, waste.length - 1); };
        } else card(table, "wa", d++, 104, 10, null, false);
        // the four foundations
        for (i = 0; i < 4; i++) {
            var f = found[i], fc = card(table, "f" + i, d++, 470 + i * 96, 10, f.length ? f[f.length - 1] : null, sel && sel.p == "f" && sel.i == i);
            if (!f.length) { var fs = fc.createEmptyMovieClip("s", 5); suit(fs, CW / 2, CHh / 2, 20, i, 0x60A070); }
            fc.i = i;
            fc.onRelease = function () { clickPile("f", this.i, found[this.i].length - 1); };
        }
        // the seven columns
        for (i = 0; i < 7; i++) {
            var col = tab[i], x = 10 + i * 120, y = 140;
            if (!col.length) { var e = card(table, "e" + i, d++, x, y, null, false); e.i = i; e.onRelease = function () { clickPile("t", this.i, -1); }; }
            for (j = 0; j < col.length; j++) {
                var hl = sel && sel.p == "t" && sel.i == i && j >= sel.k;
                var cc = card(table, "c" + i + "_" + j, d++, x, y, col[j], hl);
                cc.i = i; cc.k = j;
                cc.onRelease = function () { clickPile("t", this.i, this.k); };
                y += col[j].up ? 26 : 12;
            }
        }
        var done = found[0].length + found[1].length + found[2].length + found[3].length;
        info.text = "MOVES " + moves + "    ON THE PILES " + done + " / 52";
        if (done == 52) message("Solitaire", "You won in " + moves + " moves! Press New game to play again.");
    }
    function clickStock() {
        sel = null;
        if (stock.length) { var c = stock.pop(); c.up = true; waste.push(c); }
        else { while (waste.length) { var c2 = waste.pop(); c2.up = false; stock.push(c2); } }
        moves++;
        draw();
    }
    function cardsOf(s) {
        if (s.p == "w") return [waste[waste.length - 1]];
        if (s.p == "f") return [found[s.i][found[s.i].length - 1]];
        return tab[s.i].slice(s.k);
    }
    function removeFrom(s) {
        if (s.p == "w") waste.pop();
        else if (s.p == "f") found[s.i].pop();
        else { tab[s.i].splice(s.k); var col = tab[s.i]; if (col.length && !col[col.length - 1].up) col[col.length - 1].up = true; }
    }
    function canFound(c, i) { var f = found[i]; return c.s == i && (f.length ? f[f.length - 1].r == c.r - 1 : c.r == 0); }
    function canTab(c, i) {
        var col = tab[i];
        if (!col.length) return c.r == 12;
        var t = col[col.length - 1];
        return t.up && t.r == c.r + 1 && red(t) != red(c);
    }
    function tryMove(s, p, i) {
        var cs = cardsOf(s);
        if (p == "f" && cs.length == 1 && canFound(cs[0], i)) { removeFrom(s); found[i].push(cs[0]); return true; }
        if (p == "t" && !(s.p == "t" && s.i == i) && canTab(cs[0], i)) { removeFrom(s); for (var k = 0; k < cs.length; k++) tab[i].push(cs[k]); return true; }
        return false;
    }
    var lastT = 0, lastKey = "";
    function clickPile(p, i, k) {
        var key = p + i + "_" + k, dbl = key == lastKey && getTimer() - lastT < 450;
        lastKey = key; lastT = getTimer();
        if (p == "t" && k >= 0 && !tab[i][k].up) {           // a face-down card: turn the top one
            if (k == tab[i].length - 1) { tab[i][k].up = true; sel = null; draw(); }
            return;
        }
        if (dbl && k >= 0) {                                 // double click: to its pile
            var s0 = {p: p, i: i, k: k};
            if (p != "f" && (p == "w" || k == tab[i].length - 1)) {
                var c = cardsOf(s0)[0];
                if (canFound(c, c.s)) { removeFrom(s0); found[c.s].push(c); sel = null; moves++; draw(); return; }
            }
        }
        if (sel) {
            var ok = tryMove(sel, p, i);
            sel = null;
            if (ok) { moves++; draw(); return; }
        }
        if (k < 0 || (p == "f" && !found[i].length)) { draw(); return; }
        sel = {p: p, i: i, k: k};
        draw();
    }
    deal();
}

// ================================================================ LOGIN
// at startup (and Log Off): your picture and name; a password if you set one
var loginOn = false, loginField = null, loginFirst = true;
var AVATARS = ["bird", "ball", "pyramid", "sun", "cat", "rocket"];
function showLogin() {
    closeMenu();
    closeCalendar();
    if (!loginFirst) while (winOrder.length) closeWindow(winOrder[winOrder.length - 1]);   // Log Off closes the apps
    loginOn = true;
    var L = _root.createEmptyMovieClip("login", 78);
    box(L, 0, 0, W, H, 0x000000, 0, 0, 0);
    L.onPress = function () {};
    L.useHandCursor = false;
    var pw = 460, ph = cfg.upass == "" ? 330 : 380;
    var p = L.createEmptyMovieClip("p", 2);
    p._x = (W - pw) / 2; p._y = (H - ph) / 2;
    var g = p.createEmptyMovieClip("g", 1);
    titleBar(g, 0, 0, pw, 30);
    box(g, 0, 32, pw, ph - 32, WHITE, INK, 2, 0);
    label(p, "t", 2, 8, 6, pw - 16, 22, "Welcome to ByroTT 3.0", 16, onC1(), false);
    picture(p, "av", 3, (pw - 128) / 2, 50, "avatar_" + cfg.upic + ".png");
    label(p, "n", 4, 0, 188, pw, 40, cfg.uname, 32, INK, false, "center");
    if (cfg.upass != "") {
        label(p, "pl", 5, 60, 240, 120, 22, "Password:", 16, INK, false);
        loginField = input(p, "pf", 6, 150, 236, 250, 28, 16);
        loginField.password = true;
        Selection.setFocus(loginField);
    } else loginField = null;
    p.err = label(p, "err", 7, 0, ph - 108, pw, 22, "", 16, 0xC00000, false, "center");
    button(p, "go", 8, (pw - 160) / 2, ph - 72, 160, 44, "Log In", function () { loginTry(); }, 1, 16);
}
function loginKey(code) { if (code == 13) loginTry(); }
function loginTry() {
    var L = _root.login;
    if (cfg.upass != "" && hashPass(loginField.text) != cfg.upass) {
        L.p.err.text = "Wrong Password";
        loginField.text = "";
        Selection.setFocus(loginField);
        return;
    }
    Selection.setFocus(null);
    L.removeMovieClip();
    loginOn = false;
    idle = getTimer();
    if (loginFirst) { loginFirst = false; afterLogin(); }
}
// Settings > User Account: name, picture, password
function userDialog() {
    closeMenu();
    var d = dialogLayer.createEmptyMovieClip("dim", 1);
    box(d, 0, 0, W, H, 0x000000, 0, 0, 0);
    d._alpha = 50;
    d.onPress = function () {};
    d.useHandCursor = false;
    var p = dialogLayer.createEmptyMovieClip("panel", 2);
    var pw = 640, ph = 400;
    p._x = (W - pw) / 2; p._y = (H - ph) / 2;
    var g = p.createEmptyMovieClip("g", 1);
    titleBar(g, 0, 0, pw, 28);
    box(g, 0, 30, pw, ph - 30, WHITE, INK, 2, 0);
    label(p, "t", 2, 8, 5, pw - 20, 22, "User Account", 16, onC1(), false);
    label(p, "nl", 3, 24, 52, 100, 22, "Name:", 16, INK, false);
    var nf = input(p, "nf", 4, 100, 48, 300, 28, 16);
    nf.text = cfg.uname;
    nf.maxChars = 20;
    label(p, "pl", 5, 24, 98, 300, 22, "Picture:", 16, INK, false);
    var pick = p.createEmptyMovieClip("pick", 6);
    function drawPics() {
        for (var i = 0; i < AVATARS.length; i++) {
            var a = pick["a" + i];
            if (!a) {
                a = pick.createEmptyMovieClip("a" + i, i + 1);
                a._x = 24 + i * 98; a._y = 124;
                a.fr = a.createEmptyMovieClip("fr", 1);
                var pc = picture(a, "pc", 2, 4, 4, "avatar_" + AVATARS[i] + ".png");
                pc._xscale = pc._yscale = 65;
                a.k = AVATARS[i];
                a.onRelease = function () { cfg.upic = this.k; drawPics(); };
            }
            a.fr.clear();
            box(a.fr, 0, 0, 92, 92, cfg.upic == AVATARS[i] ? C1 : WHITE, INK, cfg.upic == AVATARS[i] ? 3 : 1, 0);
        }
    }
    drawPics();
    var pst = label(p, "ps", 7, 24, 246, 400, 22, "", 16, INK, false);
    function pwText() { pst.text = "Login password: " + (cfg.upass == "" ? "none" : "set"); }
    pwText();
    button(p, "pw", 8, 24, 274, 200, 38, cfg.upass == "" ? "Set password..." : "Change password...", function () {
        var name = nf.text;
        askPassword("Login Password", "New login password\n(leave it empty for no password):", function (a) {
            if (a == "") { cfg.upass = ""; cfg.uname = name == "" ? "User" : name; saveCfg(); message("User Account", "No password at login now."); return; }
            askPassword("Login Password", "Type it again:", function (b2) {
                cfg.uname = name == "" ? "User" : name;
                if (a != b2) { message("User Account", "The two passwords are not the same. Nothing changed."); return; }
                cfg.upass = hashPass(a);
                saveCfg();
                message("User Account", "Password set. ByroTT asks for it at login.");
            });
        });
    }, 0, 16);
    button(p, "ok", 9, pw - 130, ph - 56, 110, 40, "OK", function () {
        cfg.uname = nf.text == "" ? "User" : nf.text;
        saveCfg();
        closeDialog();
        settingsPage("");
    }, 1, 16);
    dialogKeys = function (code) { if (code == 27) closeDialog(); };
}

// ================================================================ SCREENSAVER
// after some minutes with no mouse or keys: your bird logo bounces on black
var saverOn = false, saverMx = 0, saverMy = 0;
function saverStart() {
    saverOn = true;
    closeMenu();
    var S = _root.createEmptyMovieClip("saver", 90);
    box(S, 0, 0, W, H, 0x000000, 0, 0, 0);
    var lg = logo(S, "lg", 2, 100, 100, 240, C1 == 0xB4FFFF ? 0x1EC8DC : C1);
    var vx = 3, vy = 2;
    S.onEnterFrame = function () {
        lg._x += vx; lg._y += vy;
        if (lg._x < 0 || lg._x > W - 240) { vx = -vx; logoTint(lg, THEMES[THEME_NAMES[Math.floor(Math.random() * 5)]][1] | 0x404040); }
        if (lg._y < 0 || lg._y > H - 240) { vy = -vy; logoTint(lg, THEMES[THEME_NAMES[Math.floor(Math.random() * 5)]][0]); }
    };
    S.onPress = function () { saverOff(); };
    cursorClip._visible = false;
    saverMx = _root._xmouse; saverMy = _root._ymouse;
}
function saverOff() {
    if (!saverOn) return;
    saverOn = false;
    _root.saver.removeMovieClip();
    cursorClip._visible = true;
    idle = getTimer();
}
setInterval(function () {
    var m = Number(cfg.saver);
    if (saverOn) { if (Math.abs(_root._xmouse - saverMx) + Math.abs(_root._ymouse - saverMy) > 12) saverOff(); return; }
    if (m > 0 && !loginOn && idle > 0 && getTimer() - idle > m * 60000) saverStart();
}, 500);

// ================================================================ keyboard
var keyL = {};
keyL.onKeyDown = function () {
    var code = Key.getCode();
    if (dialogKeys) { dialogKeys(code); return; }
    if (menuOpen && code == 27) { closeMenu(); return; }
    var f = Selection.getFocus();
    if (saverOn) { saverOff(); return; }
    if (loginOn) { loginKey(code); return; }
    if (Key.isDown(17) && code == 84) { launch("term"); return; }   // Ctrl+T
    if (Key.isDown(17) && code == 87 && curApp) { closeWindow(curApp); return; }   // Ctrl+W
    if (Key.isDown(17) && code == 83) {                                             // Ctrl+S
        if (isFront("notepad")) { noteSave(false); return; }
        if (isFront("paint")) { paintSave(false, null); return; }
    }
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
        if (code == 33 || code == 34) webScrollBy((code == 34 ? 1 : -1) * (WEB_VIEW - 60));
        return;
    }
    if (gameKeys(code)) return;
    if (webPage && isFront("web")) {          // not typing: the keys scroll the page
        if (code == 40) webScrollBy(48);
        if (code == 38) webScrollBy(-48);
        if (code == 34 || code == 32) webScrollBy(WEB_VIEW - 60);
        if (code == 33) webScrollBy(-(WEB_VIEW - 60));
        if (code == 36) { webScroll = 0; webRender(); }
        if (code == 35) { webScroll = webTotal(); webRender(); }
    }
};
Key.addListener(keyL);

// ================================================================ start
var launchParts = [];
function startDesktop() {
    loadCfg();
    setWallpaper(cfg.wallpaper);
    deskLoad();
    buildIcons();
    paintTaskbar();
    buildClock();
    applyVolume();
    applyCursor();
    tick();
    var arg = FlashOS.launchArg ? FlashOS.launchArg() : "";
    launchParts = arg.split("|");
    // back from a game: no login again
    if (arg == "") showLogin(); else afterLogin();
}
function afterLogin() {
    var parts = launchParts;
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
