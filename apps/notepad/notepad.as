// Flash Notepad 1.0 for FlashOS
// Text editor, drawing, picture viewer, video player and calculator.
// Copyright (c) 2026 FlashOS. All rights reserved.
// Font: Comic Neue by Craig Rozynski, SIL Open Font License 1.1.

// ---------------------------------------------------------------- theme
var CYAN = 0x1EC8DC;
var CYAN_DARK = 0x0A9DB0;
var CYAN_LIGHT = 0xDDF8FB;
var CYAN_PALE = 0xF2FDFE;
var INK = 0x0B3C49;
var GREY = 0x8AA3A8;
var WHITE = 0xFFFFFF;
var FONT = "Comic Neue";
var FONT_B = "Comic Neue Bold";
var W = 1024;
var H = 768;
var TOP = 92;      // content starts here
var BOTTOM = 734;  // status bar starts here

var depthN = 100;
function nd() { depthN++; return depthN; }

// ---------------------------------------------------------------- drawing helpers
function fmt(size, color, bold, align) {
    var f = new TextFormat();
    f.font = bold ? FONT_B : FONT;
    f.size = size;
    f.color = color;
    f.align = align ? align : "left";
    return f;
}

function label(mc, name, depth, x, y, w, h, text, size, color, bold, align) {
    mc.createTextField(name, depth, x, y, w, h);
    var t = mc[name];
    t.embedFonts = true;
    t.selectable = false;
    t.setNewTextFormat(fmt(size, color, bold, align));
    t.text = text;
    return t;
}

function rrect(mc, x, y, w, h, r) {
    if (r <= 0) {
        mc.moveTo(x, y); mc.lineTo(x + w, y); mc.lineTo(x + w, y + h); mc.lineTo(x, y + h); mc.lineTo(x, y);
        return;
    }
    mc.moveTo(x + r, y);
    mc.lineTo(x + w - r, y);
    mc.curveTo(x + w, y, x + w, y + r);
    mc.lineTo(x + w, y + h - r);
    mc.curveTo(x + w, y + h, x + w - r, y + h);
    mc.lineTo(x + r, y + h);
    mc.curveTo(x, y + h, x, y + h - r);
    mc.lineTo(x, y + r);
    mc.curveTo(x, y, x + r, y);
}

function box(mc, x, y, w, h, fill, line, lw, r) {
    if (lw > 0) mc.lineStyle(lw, line, 100); else mc.lineStyle();
    if (fill >= 0) mc.beginFill(fill, 100);
    rrect(mc, x, y, w, h, r);
    if (fill >= 0) mc.endFill();
    mc.lineStyle();
}

// ---------------------------------------------------------------- buttons
// style: 0 normal, 1 on cyan bar, 2 tool (toggle), 3 menu item
function button(parent, name, depth, x, y, w, h, text, onClick, style, size) {
    var b = parent.createEmptyMovieClip(name, depth);
    b._x = x;
    b._y = y;
    b.w = w;
    b.h = h;
    b.style = style;
    b.on = false;
    b.enabledFlag = true;
    b.bg = b.createEmptyMovieClip("bg", 1);
    b.icon = b.createEmptyMovieClip("icon", 2);
    if (!size) size = 17;
    var col = style == 1 ? WHITE : INK;
    b.lab = label(b, "lab", 3, 0, (h - size * 1.3) / 2, w, size * 1.4, text, size, col, true, style == 3 ? "left" : "center");
    if (style == 3) b.lab._x = 14;
    b.paint = function (state) {
        var g = this.bg;
        g.clear();
        var s = this.style;
        if (s == 1) {
            if (state > 0) box(g, 0, 0, this.w, this.h, state == 2 ? CYAN_DARK : 0x48D7E8, 0, 0, 8);
            else box(g, 0, 0, this.w, this.h, CYAN, 0, 0, 8);
        } else if (s == 3) {
            if (state > 0 && this.enabledFlag) box(g, 2, 0, this.w - 4, this.h, CYAN_LIGHT, 0, 0, 6);
            else box(g, 0, 0, this.w, this.h, WHITE, 0, 0, 0);
        } else {
            var fill = this.on ? CYAN : (state == 2 ? CYAN : (state == 1 ? CYAN_LIGHT : WHITE));
            box(g, 0, 0, this.w, this.h, fill, CYAN, 3, 10);
        }
    };
    b.setOn = function (v) { this.on = v; this.paint(0); };
    b.setEnabled = function (v) {
        this.enabledFlag = v;
        this.lab.textColor = v ? (this.style == 1 ? WHITE : INK) : GREY;
        this.paint(0);
    };
    b.paint(0);
    b.onRollOver = function () { this.paint(1); };
    b.onRollOut = function () { this.paint(this.on && this.style == 1 ? 2 : 0); };
    b.onDragOut = function () { this.paint(this.on && this.style == 1 ? 2 : 0); };
    b.onPress = function () { if (this.enabledFlag) this.paint(2); };
    b.onRelease = function () {
        this.paint(1);
        if (this.enabledFlag && onClick) onClick(this);
    };
    return b;
}

function arrowButton(parent, name, depth, x, y, w, h, up, onClick) {
    var b = button(parent, name, depth, x, y, w, h, "", onClick, 0, 14);
    var g = b.icon;
    g.beginFill(INK, 100);
    var cx = w / 2, cy = h / 2;
    if (up) { g.moveTo(cx - 9, cy + 5); g.lineTo(cx + 9, cy + 5); g.lineTo(cx, cy - 6); g.lineTo(cx - 9, cy + 5); }
    else { g.moveTo(cx - 9, cy - 5); g.lineTo(cx + 9, cy - 5); g.lineTo(cx, cy + 6); g.lineTo(cx - 9, cy - 5); }
    g.endFill();
    return b;
}

// ---------------------------------------------------------------- window frame
var frame = _root.createEmptyMovieClip("frame", 1);
box(frame, 0, 0, W, H, CYAN, 0, 0, 0);                  // cyan borders
box(frame, 8, 46, W - 16, BOTTOM - 46, CYAN_PALE, 0, 0, 14);
box(frame, 8, 46, W - 16, 42, CYAN_LIGHT, 0, 0, 14);    // tab strip
box(frame, 8, 74, W - 16, 14, CYAN_LIGHT, 0, 0, 0);

// app icon: a little notepad with a pencil
var ic = frame.createEmptyMovieClip("ic", 5);
ic._x = 14; ic._y = 7;
box(ic, 0, 2, 26, 30, WHITE, INK, 2, 4);
ic.lineStyle(2, CYAN_DARK, 100);
ic.moveTo(5, 11); ic.lineTo(20, 11); ic.moveTo(5, 17); ic.lineTo(20, 17); ic.moveTo(5, 23); ic.lineTo(15, 23);
ic.lineStyle(4, 0xFFB000, 100); ic.moveTo(30, 4); ic.lineTo(18, 26);
ic.lineStyle();

var titleTxt = label(frame, "title", 6, 560, 9, 452, 30, "Flash Notepad", 20, WHITE, true, "right");
var statusTxt = label(frame, "status", 7, 18, BOTTOM + 5, 700, 26, "", 16, WHITE, true, "left");
var storeTxt = label(frame, "store", 8, 640, BOTTOM + 5, 370, 26, "", 16, WHITE, false, "right");

function status(s) { statusTxt.text = s; }

// ---------------------------------------------------------------- layers
var pages = _root.createEmptyMovieClip("pages", 10);
var popup = _root.createEmptyMovieClip("popup", 50);   // menus
var modal = _root.createEmptyMovieClip("modal", 60);   // dialogs

// ---------------------------------------------------------------- menus
var menuOpen = null;
var menuBar = [];

function closeMenu() {
    if (menuOpen) { menuOpen.removeMovieClip(); menuOpen = null; }
    var catcher = popup.catcher;
    if (catcher) catcher.removeMovieClip();
    for (var i = 0; i < menuBar.length; i++) menuBar[i].setOn(false);
}

function openMenu(btn, items) {
    var wasOpen = menuOpen && menuOpen.owner == btn;
    closeMenu();
    if (wasOpen) return;
    var c = popup.createEmptyMovieClip("catcher", 1);
    box(c, 0, 0, W, H, 0xFFFFFF, 0, 0, 0);
    c._alpha = 0;
    c.onPress = function () {
        var x = _root._xmouse, y = _root._ymouse;
        closeMenu();
        if (y > 44) return;
        for (var j = 0; j < menuBar.length; j++) {
            var mb = menuBar[j];
            if (mb != btn && x >= mb._x && x < mb._x + mb.w) openMenu(mb, mb.getItems());
        }
    };
    c.useHandCursor = false;
    var m = popup.createEmptyMovieClip("menu", 2);
    m.owner = btn;
    menuOpen = m;
    var rowH = 34, w = 280, y = 8, i;
    var h = 16;
    for (i = 0; i < items.length; i++) h += items[i][0] == "-" ? 12 : rowH;
    m._x = btn._x;
    m._y = btn._y + btn.h + 4;
    var g = m.createEmptyMovieClip("g", 1);
    box(g, 4, 4, w, h, 0x000000, 0, 0, 12);
    g._alpha = 18;
    var p = m.createEmptyMovieClip("p", 2);
    box(p, 0, 0, w, h, WHITE, CYAN, 4, 12);
    for (i = 0; i < items.length; i++) {
        var it = items[i];
        if (it[0] == "-") {
            var sep = m.createEmptyMovieClip("s" + i, 10 + i);
            box(sep, 14, y + 5, w - 28, 2, CYAN_LIGHT, 0, 0, 0);
            y += 12;
            continue;
        }
        var b = button(m, "i" + i, 10 + i, 0, y, w, rowH, it[0], it[2], 3, 17);
        if (it[1] != "") label(b, "key", 4, w - 120, 6, 108, 24, it[1], 14, GREY, false, "right");
        b.action = it[2];
        b.onRelease = function () {
            var a = this.action;
            closeMenu();
            if (this.enabledFlag && a) a();
        };
        if (it.length > 3 && !it[3]) b.setEnabled(false);
        y += rowH;
    }
    btn.setOn(true);
}

function menuButton(name, x, w, getItems) {
    var b = button(frame, name, nd(), x, 6, w, 34, name, function (me) { openMenu(me, getItems()); }, 1, 19);
    b.setOn = function (v) { this.on = v; this.paint(v ? 2 : 0); };
    b.getItems = getItems;
    menuBar.push(b);
    return b;
}

// ---------------------------------------------------------------- dialogs
function dimmer() {
    var d = modal.createEmptyMovieClip("dim", 1);
    box(d, 0, 0, W, H, 0x06303A, 0, 0, 0);
    d._alpha = 45;
    d.onPress = function () {};
    d.useHandCursor = false;
    return d;
}

function panel(w, h, title) {
    var p = modal.createEmptyMovieClip("panel", 2);
    p._x = Math.round((W - w) / 2);
    p._y = Math.round((H - h) / 2) - 20;
    var s = p.createEmptyMovieClip("shadow", 1);
    box(s, 6, 8, w, h, 0x000000, 0, 0, 18);
    s._alpha = 25;
    var g = p.createEmptyMovieClip("g", 2);
    box(g, 0, 0, w, h, CYAN, 0, 0, 18);
    box(g, 8, 44, w - 16, h - 52, WHITE, 0, 0, 12);
    label(p, "title", 3, 18, 8, w - 36, 30, title, 20, WHITE, true, "left");
    return p;
}

var dialogKeys = null;   // function(code) while a dialog is open

function closeDialog() {
    modal.panel.removeMovieClip();
    modal.dim.removeMovieClip();
    dialogKeys = null;
    refocus();
}

// message with buttons: labels array, callback(index)
function ask(title, msg, labels, done) {
    closeMenu();
    dimmer();
    var p = panel(520, 230, title);
    var t = label(p, "msg", 4, 26, 58, 468, 100, msg, 18, INK, false, "left");
    t.multiline = true;
    t.wordWrap = true;
    t.text = msg;
    var bw = 130, gap = 14, x = 520 - 22 - labels.length * bw - (labels.length - 1) * gap;
    for (var i = 0; i < labels.length; i++) {
        var b = button(p, "b" + i, 10 + i, x + i * (bw + gap), 166, bw, 42, labels[i], null, 0, 18);
        b.idx = i;
        b.onRelease = function () { var k = this.idx; closeDialog(); if (done) done(k); };
        if (i == 0) b.setOn(true);
    }
    dialogKeys = function (code) {
        if (code == 13) { closeDialog(); if (done) done(0); }
        if (code == 27) { closeDialog(); if (done) done(labels.length - 1); }
    };
}
function alertBox(title, msg) { ask(title, msg, ["OK"], null); }

function where(w) {
    if (w == "disk") return "hard disk";
    if (w == "memory") return "memory";
    if (w == "cd") return "CD";
    if (w == "usb") return "USB";
    if (w == "net") return "internet";
    return "FlashOS";
}

function kb(n) {
    if (n < 1024) return n + " B";
    if (n < 1048576) return Math.round(n / 1024) + " KB";
    return Math.round(n / 104857.6) / 10 + " MB";
}

// file dialog: mode "open" or "save"; exts like "txt"; done(name)
function fileDialog(mode, title, exts, defName, done) {
    closeMenu();
    dimmer();
    var p = panel(620, 520, title);
    var files = FlashOS.files(exts);
    var rows = 9, rowH = 34, first = 0, sel = -1, lastClick = 0;
    var list = p.createEmptyMovieClip("list", 4);
    list._x = 24; list._y = 60;
    var lg = list.createEmptyMovieClip("g", 1);
    box(lg, 0, 0, 520, rows * rowH + 8, CYAN_PALE, CYAN, 3, 10);
    var nameF;
    var drawList = function () {
        for (var r = 0; r < rows; r++) {
            var i = first + r;
            var row = list["r" + r];
            if (!row) {
                row = list.createEmptyMovieClip("r" + r, 10 + r);
                row._x = 4; row._y = 4 + r * rowH;
                row.bg = row.createEmptyMovieClip("bg", 1);
                row.n = label(row, "n", 2, 10, 4, 330, 28, "", 17, INK, false, "left");
                row.s = label(row, "s", 3, 340, 6, 168, 26, "", 14, GREY, false, "right");
                row.r = r;
                row.onRelease = function () {
                    var k = first + this.r;
                    if (k >= files.length) return;
                    var again = (k == sel && getTimer() - lastClick < 500);
                    sel = k;
                    lastClick = getTimer();
                    nameF.text = files[k].name;
                    drawList();
                    if (again) okNow();
                };
            }
            row.bg.clear();
            if (i < files.length) {
                if (i == sel) box(row.bg, 0, 0, 512, rowH, CYAN, 0, 0, 8);
                row.n.text = files[i].name;
                row.s.text = kb(files[i].size) + " - " + where(files[i].where);
                row.n.textColor = i == sel ? WHITE : INK;
                row.s.textColor = i == sel ? WHITE : GREY;
                row._visible = true;
            } else {
                row._visible = false;
            }
        }
        if (files.length == 0) {
            emptyMsg._visible = true;
        }
    };
    var emptyMsg = label(list, "empty", 50, 20, 120, 480, 30, "No files here yet.", 18, GREY, false, "center");
    emptyMsg._visible = false;
    var up = arrowButton(p, "up", 20, 552, 60, 44, 44, true, function () { if (first > 0) { first -= rows; if (first < 0) first = 0; drawList(); } });
    var dn = arrowButton(p, "dn", 21, 552, 60 + rows * rowH - 36, 44, 44, false, function () { if (first + rows < files.length) { first += rows; drawList(); } });
    label(p, "nl", 22, 24, 386, 120, 28, "File name:", 18, INK, true, "left");
    p.createTextField("name", 23, 140, 380, 456, 38);
    nameF = p.name;
    nameF.embedFonts = true;
    nameF.type = mode == "save" ? "input" : "dynamic";
    nameF.border = true;
    nameF.borderColor = CYAN;
    nameF.background = true;
    nameF.setNewTextFormat(fmt(19, INK, false));
    nameF.maxChars = 40;
    nameF.text = defName;
    var info = FlashOS.storage() == "disk" ? "Your files are saved on the hard disk." : "FlashOS is not installed: files you save stay in memory until restart.";
    label(p, "info", 24, 24, 426, 572, 26, info, 14, GREY, false, "left");
    var okNow = function () {
        var n = nameF.text;
        if (n == "") return;
        closeDialog();
        done(n);
    };
    button(p, "ok", 25, 330, 462, 130, 42, mode == "save" ? "Save" : "Open", okNow, 0, 18).setOn(true);
    button(p, "cancel", 26, 470, 462, 126, 42, "Cancel", function () { closeDialog(); }, 0, 18);
    drawList();
    if (mode == "save") Selection.setFocus(nameF);
    dialogKeys = function (code) {
        if (code == 13) okNow();
        if (code == 27) closeDialog();
        if (code == 33) up.onRelease();
        if (code == 34) dn.onRelease();
        if ((code == 38 || code == 40) && files.length > 0) {
            sel += code == 40 ? 1 : -1;
            if (sel < 0) sel = 0;
            if (sel >= files.length) sel = files.length - 1;
            if (sel < first) first = sel;
            if (sel >= first + rows) first = sel - rows + 1;
            nameF.text = files[sel].name;
            drawList();
        }
    };
}

function addExt(n, ext) {
    var low = n.toLowerCase();
    if (low.lastIndexOf("." + ext) == low.length - ext.length - 1 && low.length > ext.length + 1) return n;
    return n + "." + ext;
}

// ---------------------------------------------------------------- pages and tabs
var page = "";
var tabs = {};
var pageClips = {};

function refocus() {
    if (page == "text" && !dialogKeys) Selection.setFocus(textF);
}

function showPage(name) {
    closeMenu();
    page = name;
    for (var k in pageClips) {
        pageClips[k]._visible = (k == name);
        tabs[k].setOn(k == name);
    }
    if (name == "text") { refocus(); textStatus(); }
    else Selection.setFocus(null);
    if (name == "draw") status("Draw with the mouse. Pick a tool and a colour on the left.");
    if (name == "images") imagesShow();
    if (name == "video") videoShow();
    if (name == "calc") status("Calculator: click the keys or type numbers, + - * / and Enter.");
    updateTitle();
}

function tabIcon(b, kind) {
    var g = b.icon;
    g._x = 14; g._y = 8;
    if (kind == "text") {
        box(g, 0, 0, 18, 22, WHITE, INK, 2, 3);
        g.lineStyle(2, CYAN_DARK, 100); g.moveTo(4, 7); g.lineTo(14, 7); g.moveTo(4, 12); g.lineTo(14, 12); g.moveTo(4, 17); g.lineTo(11, 17);
    } else if (kind == "draw") {
        box(g, 0, 2, 22, 18, WHITE, INK, 2, 9);
        box(g, 4, 6, 5, 5, 0xFF4040, 0, 0, 2); box(g, 11, 5, 5, 5, 0x30B030, 0, 0, 2); box(g, 6, 12, 5, 5, 0x3070FF, 0, 0, 2);
    } else if (kind == "images") {
        box(g, 0, 1, 24, 20, 0x9BE3FF, INK, 2, 3);
        g.lineStyle(); g.beginFill(0x2E9E4F, 100); g.moveTo(2, 19); g.lineTo(9, 9); g.lineTo(15, 19); g.endFill();
        box(g, 15, 4, 5, 5, 0xFFD000, 0, 0, 2);
    } else if (kind == "video") {
        box(g, 0, 2, 24, 18, INK, INK, 2, 4);
        g.lineStyle(); g.beginFill(WHITE, 100); g.moveTo(9, 6); g.lineTo(17, 11); g.lineTo(9, 16); g.endFill();
    } else {
        box(g, 2, 0, 18, 22, WHITE, INK, 2, 3);
        box(g, 5, 3, 12, 5, CYAN_LIGHT, 0, 0, 1);
        box(g, 5, 11, 4, 3, INK, 0, 0, 0); box(g, 11, 11, 4, 3, INK, 0, 0, 0); box(g, 5, 16, 4, 3, INK, 0, 0, 0); box(g, 11, 16, 4, 3, 0xFF8000, 0, 0, 0);
    }
    b.lab._x = 22;
    b.lab._width = b.w - 22;
}

function makeTab(name, title, x, w) {
    var b = button(frame, "tab_" + name, nd(), x, 50, w, 38, title, function () { showPage(name); }, 2, 18);
    tabIcon(b, name);
    tabs[name] = b;
    var c = pages.createEmptyMovieClip(name, nd());
    c._visible = false;
    pageClips[name] = c;
    return c;
}

var textPage = makeTab("text", "Text", 16, 120);
var drawPage = makeTab("draw", "Draw", 144, 120);
var imgPage = makeTab("images", "Pictures", 272, 150);
var vidPage = makeTab("video", "Video", 430, 120);
var calcPage = makeTab("calc", "Calculator", 558, 170);

// ================================================================ TEXT
var docName = "";
var dirty = false;
var textSize = 20;
var wrapOn = true;

var tg = textPage.createEmptyMovieClip("g", 1);
box(tg, 16, TOP + 6, W - 32, BOTTOM - TOP - 14, WHITE, CYAN, 3, 12);
textPage.createTextField("doc", 2, 26, TOP + 14, W - 84, BOTTOM - TOP - 30);
var textF = textPage.doc;
textF.embedFonts = true;
textF.type = "input";
textF.multiline = true;
textF.wordWrap = true;
textF.setNewTextFormat(fmt(textSize, INK, false));
textF.text = "";
textF.onChanged = function () {
    if (!dirty) { dirty = true; updateTitle(); }
    textStatus();
};

// scroll bar
var sb = textPage.createEmptyMovieClip("sb", 3);
sb._x = W - 52; sb._y = TOP + 14;
var sbH = BOTTOM - TOP - 30;
var sbTrack = sb.createEmptyMovieClip("track", 1);
box(sbTrack, 0, 0, 24, sbH, CYAN_PALE, CYAN_LIGHT, 2, 12);
var sbThumb = sb.createEmptyMovieClip("thumb", 2);
var thumbH = 60, sbDrag = false, sbLastMax = -1, sbLastPos = -1;
function drawThumb() {
    sbThumb.clear();
    box(sbThumb, 2, 0, 20, thumbH, CYAN, 0, 0, 10);
}
drawThumb();
sbTrack.onPress = function () {
    var y = sb._ymouse;
    var page = textF.bottomScroll - textF.scroll + 1;
    if (y < sbThumb._y) textF.scroll = textF.scroll - page; else textF.scroll = textF.scroll + page;
};
sbTrack.useHandCursor = false;
sbThumb.onPress = function () { sbDrag = true; this.dy = sb._ymouse - this._y; };
sbThumb.onRelease = function () { sbDrag = false; };
sbThumb.onReleaseOutside = function () { sbDrag = false; };
sbThumb.onMouseMove = function () {
    if (!sbDrag) return;
    var m = textF.maxscroll;
    var room = sbH - thumbH;
    var y = sb._ymouse - this.dy;
    if (y < 0) y = 0;
    if (y > room) y = room;
    textF.scroll = 1 + Math.round(y / room * (m - 1));
};
textPage.onEnterFrame = function () {
    if (page != "text") return;
    var m = textF.maxscroll, s = textF.scroll;
    if (m == sbLastMax && s == sbLastPos) return;
    sbLastMax = m; sbLastPos = s;
    sb._visible = m > 1;
    if (m > 1) {
        var vis = textF.bottomScroll - s + 1;
        var total = vis + m - 1;
        thumbH = Math.max(40, Math.round(sbH * vis / total));
        drawThumb();
        if (!sbDrag) sbThumb._y = Math.round((s - 1) / (m - 1) * (sbH - thumbH));
    }
};

function textStatus() {
    if (page != "text") return;
    var n = textF.length;
    status((docName == "" ? "New document" : docName) + "   -   " + n + (n == 1 ? " character" : " characters") + (dirty ? "   -   not saved" : ""));
}

function updateTitle() {
    var t = "Flash Notepad";
    if (page == "text") t = (dirty ? "* " : "") + (docName == "" ? "untitled.txt" : docName) + " - Flash Notepad";
    if (page == "draw") t = (drawDirty ? "* " : "") + (drawName == "" ? "untitled.bmp" : drawName) + " - Flash Notepad";
    titleTxt.text = t;
}

// unsaved changes? then(action) runs once it is safe to continue
function checkSaved(then) {
    if (!dirty) { then(); return; }
    ask("Save changes?", "Do you want to save the changes to " + (docName == "" ? "this document" : docName) + "?",
        ["Save", "Don't save", "Cancel"], function (k) {
            if (k == 0) saveText(false, then);
            if (k == 1) then();
        });
}

function newText() {
    checkSaved(function () {
        textF.text = "";
        docName = "";
        dirty = false;
        updateTitle();
        textStatus();
        refocus();
    });
}

function loadText(n) {
    var s = FlashOS.read(n);
    if (s == null) { alertBox("Open", "Sorry, " + n + " could not be read."); return; }
    textF.text = s;
    textF.scroll = 1;
    Selection.setFocus(textF);
    Selection.setSelection(0, 0);
    docName = n;
    dirty = false;
    updateTitle();
    textStatus();
}

function openText() {
    checkSaved(function () {
        fileDialog("open", "Open a text file", "txt,text,log,ini,md,csv,as,htm,html,xml", "", loadText);
    });
}

function saveText(askName, then) {
    var go = function (n) {
        n = addExt(n, "txt");
        if (FlashOS.write(n, textF.text)) {
            docName = n;
            dirty = false;
            updateTitle();
            textStatus();
            status("Saved " + n + " (" + kb(textF.length) + ") to the " + (FlashOS.storage() == "disk" ? "hard disk." : "memory."));
            if (then) then();
        } else {
            alertBox("Save", "Sorry, " + n + " could not be saved. The disk may be full.");
        }
    };
    if (askName || docName == "") fileDialog("save", "Save the text as", "txt", docName == "" ? "note.txt" : docName, go);
    else go(docName);
}

function findText() {
    closeMenu();
    dimmer();
    var p = panel(520, 200, "Find");
    label(p, "l", 4, 24, 64, 100, 28, "Find:", 18, INK, true, "left");
    p.createTextField("q", 5, 100, 58, 396, 38);
    var q = p.q;
    q.embedFonts = true; q.type = "input"; q.border = true; q.borderColor = CYAN; q.background = true;
    q.setNewTextFormat(fmt(19, INK, false));
    q.text = lastFind;
    Selection.setFocus(q);
    Selection.setSelection(0, lastFind.length);
    var go = function () {
        lastFind = q.text;
        closeDialog();
        findNext();
    };
    button(p, "ok", 6, 230, 136, 130, 42, "Find next", go, 0, 18).setOn(true);
    button(p, "c", 7, 370, 136, 126, 42, "Cancel", function () { closeDialog(); }, 0, 18);
    dialogKeys = function (code) { if (code == 13) go(); if (code == 27) closeDialog(); };
}
var lastFind = "";
function findNext() {
    if (lastFind == "") return;
    var all = textF.text.toLowerCase();
    Selection.setFocus(textF);
    var from = Selection.getEndIndex();
    var i = all.indexOf(lastFind.toLowerCase(), from);
    if (i < 0) i = all.indexOf(lastFind.toLowerCase(), 0);
    if (i < 0) { status("\"" + lastFind + "\" was not found."); return; }
    Selection.setSelection(i, i + lastFind.length);
    status("Found \"" + lastFind + "\".");
}

function two(n) { return n < 10 ? "0" + n : "" + n; }
function insertDate() {
    var d = new Date();
    var s = two(d.getHours()) + ":" + two(d.getMinutes()) + " " + two(d.getDate()) + "/" + two(d.getMonth() + 1) + "/" + d.getFullYear();
    Selection.setFocus(textF);
    textF.replaceSel(s);
}

function setTextSize(s) {
    if (s < 10) s = 10;
    if (s > 60) s = 60;
    textSize = s;
    textF.setTextFormat(fmt(textSize, INK, false));
    textF.setNewTextFormat(fmt(textSize, INK, false));
    status("Text size " + s);
    sbLastMax = -1;
}

function toggleWrap() {
    wrapOn = !wrapOn;
    textF.wordWrap = wrapOn;
    status(wrapOn ? "Word wrap is on." : "Word wrap is off.");
    sbLastMax = -1;
}

// ================================================================ DRAW
var CW = 846, CH = 610;           // canvas size
var CX = 160, CY = TOP + 12;
var drawName = "";
var drawDirty = false;
var tool = "pen";
var brush = 6;
var colour = 0x0B3C49;
var canvas = new flash.display.BitmapData(CW, CH, false, 0xFFFFFF);
var undoBmp = new flash.display.BitmapData(CW, CH, false, 0xFFFFFF);
var fullRect = new flash.geom.Rectangle(0, 0, CW, CH);
var origin = new flash.geom.Point(0, 0);

var dg = drawPage.createEmptyMovieClip("g", 1);
box(dg, 16, TOP + 6, 136, BOTTOM - TOP - 14, WHITE, CYAN, 3, 12);
box(dg, CX - 4, CY - 4, CW + 8, CH + 8, CYAN, 0, 0, 6);

var cv = drawPage.createEmptyMovieClip("cv", 2);
cv._x = CX; cv._y = CY;
cv.attachBitmap(canvas, 1);
var preview = cv.createEmptyMovieClip("preview", 5);
cv.useHandCursor = false;

var toolBtns = {};
function pickTool(t) {
    tool = t;
    for (var k in toolBtns) toolBtns[k].setOn(k == t);
    var names = {pen: "Pencil", eraser: "Eraser", fill: "Fill bucket", line: "Line", rect: "Rectangle", oval: "Circle"};
    status(names[t] + " - colour and size on the left.");
}
function toolButton(t, x, y, kind) {
    var b = button(drawPage, "t_" + t, nd(), x, y, 56, 44, "", function () { pickTool(t); }, 2, 14);
    var g = b.icon;
    g._x = 12; g._y = 8;
    if (kind == 0) { g.lineStyle(4, INK, 100); g.moveTo(4, 24); g.curveTo(10, 4, 18, 12); g.curveTo(26, 20, 30, 4); }
    if (kind == 1) { box(g, 4, 6, 24, 16, 0xFFB0C8, INK, 2, 4); g.lineStyle(2, INK, 100); g.moveTo(14, 6); g.lineTo(14, 22); }
    if (kind == 2) {
        g.lineStyle(2, INK, 100); g.beginFill(CYAN_LIGHT, 100);
        g.moveTo(6, 10); g.lineTo(18, 2); g.lineTo(28, 14); g.lineTo(16, 24); g.lineTo(6, 10); g.endFill();
        g.lineStyle(); g.beginFill(0x3070FF, 100); g.moveTo(26, 16); g.curveTo(32, 26, 26, 28); g.curveTo(22, 26, 26, 16); g.endFill();
    }
    if (kind == 3) { g.lineStyle(4, INK, 100); g.moveTo(4, 24); g.lineTo(28, 4); }
    if (kind == 4) { box(g, 4, 4, 24, 20, -1, INK, 3, 0); }
    if (kind == 5) {
        g.lineStyle(3, INK, 100);
        g.moveTo(28, 14); g.curveTo(28, 26, 16, 26); g.curveTo(4, 26, 4, 14); g.curveTo(4, 2, 16, 2); g.curveTo(28, 2, 28, 14);
    }
    toolBtns[t] = b;
}
toolButton("pen", 24, TOP + 16, 0);
toolButton("eraser", 88, TOP + 16, 1);
toolButton("fill", 24, TOP + 66, 2);
toolButton("line", 88, TOP + 66, 3);
toolButton("rect", 24, TOP + 116, 4);
toolButton("oval", 88, TOP + 116, 5);

label(drawPage, "szl", nd(), 24, TOP + 170, 120, 24, "Size", 16, INK, true, "left");
var sizeBtns = [];
var sizes = [2, 6, 12, 24];
for (var si = 0; si < 4; si++) {
    var sbn = button(drawPage, "sz" + si, nd(), 24 + (si % 2) * 64, TOP + 196 + Math.floor(si / 2) * 50, 56, 44, "", null, 2, 14);
    sbn.sz = sizes[si];
    var r = Math.min(sizes[si], 20) / 2 + 1;
    box(sbn.icon, 28 - r - 14 + 14 - 14, 22 - r - 8, r * 2, r * 2, INK, 0, 0, r);
    sbn.icon._x = 14; sbn.icon._y = 8;
    sbn.onRelease = function () {
        brush = this.sz;
        for (var j = 0; j < sizeBtns.length; j++) sizeBtns[j].setOn(sizeBtns[j] == this);
        status("Brush size " + brush);
    };
    sizeBtns.push(sbn);
}
sizeBtns[1].setOn(true);

label(drawPage, "cl", nd(), 24, TOP + 302, 120, 24, "Colour", 16, INK, true, "left");
var swatch = drawPage.createEmptyMovieClip("swatch", nd());
swatch._x = 24; swatch._y = TOP + 330;
function drawSwatch() {
    swatch.clear();
    box(swatch, 0, 0, 120, 30, colour, INK, 2, 8);
}
drawSwatch();
var palette = [0x000000, 0x0B3C49, 0x808080, 0xFFFFFF, 0xE53935, 0xFF7F00, 0xFFD000, 0x7BC043,
               0x1B8A3A, 0x1EC8DC, 0x1E6FE0, 0x2A2A9E, 0x9C27B0, 0xFF6EB4, 0x8B5A2B, 0xF5D7B0];
for (var pi = 0; pi < palette.length; pi++) {
    var pc = drawPage.createEmptyMovieClip("pc" + pi, nd());
    pc._x = 24 + (pi % 4) * 30;
    pc._y = TOP + 370 + Math.floor(pi / 4) * 30;
    box(pc, 0, 0, 26, 26, palette[pi], CYAN_DARK, 2, 6);
    pc.col = palette[pi];
    pc.onRelease = function () {
        colour = this.col;
        drawSwatch();
        if (tool == "eraser") pickTool("pen");
    };
}
button(drawPage, "clear", nd(), 24, TOP + 500, 120, 40, "Clear", function () { clearCanvas(); }, 0, 17);
button(drawPage, "undo", nd(), 24, TOP + 548, 120, 40, "Undo", function () { undoDraw(); }, 0, 17);

var drawing = false, lx = 0, ly = 0, sx0 = 0, sy0 = 0;
function snapshot() { undoBmp.copyPixels(canvas, fullRect, origin); }
function undoDraw() {
    var tmp = canvas.clone();
    canvas.copyPixels(undoBmp, fullRect, origin);
    undoBmp.copyPixels(tmp, fullRect, origin);
    tmp.dispose();
    markDraw();
}
function clearCanvas() {
    snapshot();
    canvas.fillRect(fullRect, 0xFFFFFF);
    markDraw();
}
function markDraw() {
    if (!drawDirty) { drawDirty = true; updateTitle(); }
}
function mx() { var x = Math.round(cv._xmouse); return x < -50 ? -50 : (x > CW + 50 ? CW + 50 : x); }
function my() { var y = Math.round(cv._ymouse); return y < -50 ? -50 : (y > CH + 50 ? CH + 50 : y); }

function ovalPoints(target, x0, y0, x1, y1, commit) {
    var cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, rx = Math.abs(x1 - x0) / 2, ry = Math.abs(y1 - y0) / 2;
    var px = cx + rx, py = cy, n = 72;
    if (!commit) target.moveTo(px, py);
    for (var i = 1; i <= n; i++) {
        var a = i / n * Math.PI * 2;
        var qx = cx + Math.cos(a) * rx, qy = cy + Math.sin(a) * ry;
        if (commit) canvas.drawLine(Math.round(px), Math.round(py), Math.round(qx), Math.round(qy), colour, brush);
        else target.lineTo(qx, qy);
        px = qx; py = qy;
    }
}

cv.onPress = function () {
    var x = mx(), y = my();
    snapshot();
    markDraw();
    if (tool == "fill") { canvas.floodFill(x, y, colour); return; }
    drawing = true;
    lx = sx0 = x; ly = sy0 = y;
    if (tool == "pen" || tool == "eraser") canvas.drawLine(x, y, x, y, tool == "eraser" ? 0xFFFFFF : colour, tool == "eraser" ? brush * 2 : brush);
};
cv.onMouseMove = function () {
    if (!drawing) return;
    var x = mx(), y = my();
    if (tool == "pen" || tool == "eraser") {
        canvas.drawLine(lx, ly, x, y, tool == "eraser" ? 0xFFFFFF : colour, tool == "eraser" ? brush * 2 : brush);
        lx = x; ly = y;
        return;
    }
    preview.clear();
    preview.lineStyle(Math.max(1, brush), colour, 100);
    if (tool == "line") { preview.moveTo(sx0, sy0); preview.lineTo(x, y); }
    if (tool == "rect") rrect(preview, Math.min(sx0, x), Math.min(sy0, y), Math.abs(x - sx0), Math.abs(y - sy0), 0);
    if (tool == "oval") ovalPoints(preview, sx0, sy0, x, y, false);
};
function endStroke() {
    if (!drawing) return;
    drawing = false;
    preview.clear();
    var x = mx(), y = my();
    if (tool == "line") canvas.drawLine(sx0, sy0, x, y, colour, brush);
    if (tool == "rect") {
        canvas.drawLine(sx0, sy0, x, sy0, colour, brush); canvas.drawLine(x, sy0, x, y, colour, brush);
        canvas.drawLine(x, y, sx0, y, colour, brush); canvas.drawLine(sx0, y, sx0, sy0, colour, brush);
    }
    if (tool == "oval") ovalPoints(null, sx0, sy0, x, y, true);
}
cv.onRelease = endStroke;
cv.onReleaseOutside = endStroke;

function checkDrawSaved(then) {
    if (!drawDirty) { then(); return; }
    ask("Save changes?", "Do you want to save your drawing" + (drawName == "" ? "" : " " + drawName) + "?",
        ["Save", "Don't save", "Cancel"], function (k) {
            if (k == 0) saveDrawing(false, then);
            if (k == 1) then();
        });
}
function newDrawing() {
    checkDrawSaved(function () {
        canvas.fillRect(fullRect, 0xFFFFFF);
        snapshot();
        drawName = "";
        drawDirty = false;
        updateTitle();
    });
}
function saveDrawing(askName, then) {
    var go = function (n) {
        n = addExt(n, "bmp");
        if (FlashOS.saveBitmap(n, canvas)) {
            drawName = n;
            drawDirty = false;
            updateTitle();
            status("Saved the drawing as " + n + ". You can see it in Pictures too.");
            imgList = null;
            if (then) then();
        } else alertBox("Save", "Sorry, the drawing could not be saved. The disk may be full.");
    };
    if (askName || drawName == "") fileDialog("save", "Save the drawing as", "bmp", drawName == "" ? "drawing.bmp" : drawName, go);
    else go(drawName);
}
function openDrawing() {
    checkDrawSaved(function () {
        fileDialog("open", "Open a picture to draw on", "bmp,png,jpg,jpeg,gif", "", function (n) {
            var b = FlashOS.loadBitmap(n);
            if (!b) { alertBox("Open", "Sorry, " + n + " is not a picture FlashOS can open."); return; }
            canvas.fillRect(fullRect, 0xFFFFFF);
            canvas.copyPixels(b, new flash.geom.Rectangle(0, 0, Math.min(b.width, CW), Math.min(b.height, CH)), origin);
            snapshot();
            drawName = n.toLowerCase().lastIndexOf(".bmp") == n.length - 4 ? n : "";
            drawDirty = drawName == "";
            updateTitle();
            status("Opened " + n + ".");
        });
    });
}

// ---------------------------------------------------------------- file list panel (pictures, videos)
function listPanel(parent, exts, onPick) {
    var lp = parent.createEmptyMovieClip("list", nd());
    lp._x = 16; lp._y = TOP + 6;
    var g = lp.createEmptyMovieClip("g", 1);
    box(g, 0, 0, 260, BOTTOM - TOP - 14, WHITE, CYAN, 3, 12);
    lp.rows = 14;
    lp.first = 0;
    lp.sel = -1;
    lp.files = [];
    lp.exts = exts;
    lp.empty = label(lp, "empty", 3, 14, 40, 232, 200, "", 16, GREY, false, "left");
    lp.empty.multiline = true;
    lp.empty.wordWrap = true;
    lp.redraw = function () {
        for (var r = 0; r < this.rows; r++) {
            var i = this.first + r;
            var row = this["r" + r];
            if (!row) {
                row = this.createEmptyMovieClip("r" + r, 10 + r);
                row._x = 6; row._y = 8 + r * 38;
                row.bg = row.createEmptyMovieClip("bg", 1);
                row.n = label(row, "n", 2, 8, 0, 232, 24, "", 16, INK, false, "left");
                row.s = label(row, "s", 3, 8, 18, 232, 20, "", 13, GREY, false, "left");
                row.r = r;
                row.lp = this;
                row.onRelease = function () { var L = this.lp; var k = L.first + this.r; if (k < L.files.length) { L.sel = k; L.redraw(); onPick(L.files[k].name, k); } };
            }
            row.bg.clear();
            if (i < this.files.length) {
                var on = i == this.sel;
                if (on) box(row.bg, 0, 0, 248, 37, CYAN, 0, 0, 8);
                row.n.text = this.files[i].name;
                row.s.text = kb(this.files[i].size) + " - " + where(this.files[i].where);
                row.n.textColor = on ? WHITE : INK;
                row.s.textColor = on ? WHITE : GREY;
                row._visible = true;
            } else row._visible = false;
        }
        this.empty._visible = this.files.length == 0;
        this.up._visible = this.dn._visible = this.files.length > this.rows;
    };
    lp.reload = function () {
        this.files = FlashOS.files(this.exts);
        if (this.sel >= this.files.length) this.sel = -1;
        if (this.first >= this.files.length) this.first = 0;
        this.redraw();
    };
    lp.up = arrowButton(lp, "up", 40, 140, BOTTOM - TOP - 66, 52, 40, true, function () { if (lp.first > 0) { lp.first -= lp.rows; if (lp.first < 0) lp.first = 0; lp.redraw(); } });
    lp.dn = arrowButton(lp, "dn", 41, 198, BOTTOM - TOP - 66, 52, 40, false, function () { if (lp.first + lp.rows < lp.files.length) { lp.first += lp.rows; lp.redraw(); } });
    lp.select = function (k) {
        if (k < 0 || k >= this.files.length) return;
        this.sel = k;
        if (k < this.first || k >= this.first + this.rows) this.first = Math.floor(k / this.rows) * this.rows;
        this.redraw();
        onPick(this.files[k].name, k);
    };
    return lp;
}

// ================================================================ PICTURES
var imgList = null;
var imgCache = {};
var imgZoomFit = true;
var imgCur = null, imgName = "";
var ig = imgPage.createEmptyMovieClip("g", 1);
var VX = 288, VY = TOP + 6, VW = W - 16 - VX, VH = BOTTOM - TOP - 66;
box(ig, VX, VY, VW, VH, 0x18333A, CYAN, 3, 12);
var view = imgPage.createEmptyMovieClip("view", 2);
var imgInfo = label(imgPage, "info", 4, VX + 20, VY + VH / 2 - 40, VW - 40, 80, "", 18, WHITE, false, "center");
imgInfo.multiline = true;
imgInfo.wordWrap = true;
var imgPick = listPanel(imgPage, "jpg,jpeg,png,gif,bmp", function (n, k) { viewImage(n); });
imgPick.empty.text = "No pictures found.\n\nPut .jpg .png .gif or .bmp files on a CD or USB stick, or save a drawing.";
var ib = 0;
button(imgPage, "prev", nd(), VX, BOTTOM - 52, 120, 42, "< Back", function () { imgPick.select(imgPick.sel - 1); }, 0, 17);
button(imgPage, "next", nd(), VX + 128, BOTTOM - 52, 120, 42, "Next >", function () { imgPick.select(imgPick.sel + 1); }, 0, 17);
var fitBtn = button(imgPage, "fit", nd(), VX + 256, BOTTOM - 52, 150, 42, "Fit to window", function () { imgZoomFit = true; layoutImage(); }, 2, 17);
var realBtn = button(imgPage, "real", nd(), VX + 414, BOTTOM - 52, 130, 42, "Real size", function () { imgZoomFit = false; layoutImage(); }, 2, 17);

function layoutImage() {
    fitBtn.setOn(imgZoomFit);
    realBtn.setOn(!imgZoomFit);
    if (!imgCur) return;
    var s = Math.min((VW - 24) / imgCur.width, (VH - 24) / imgCur.height);
    if (!imgZoomFit && s > 1) s = 1;  // real size, but never bigger than the window
    view._xscale = view._yscale = s * 100;
    view._x = Math.round(VX + (VW - imgCur.width * s) / 2);
    view._y = Math.round(VY + (VH - imgCur.height * s) / 2);
    status(imgName + "   -   " + imgCur.width + " x " + imgCur.height + " pixels   -   " + Math.round(s * 100) + "%");
}

function viewImage(n) {
    var b = imgCache[n];
    if (!b) {
        status("Opening " + n + "...");
        b = FlashOS.loadBitmap(n);
        if (b) imgCache[n] = b;
    }
    if (!b) { imgCur = null; view._visible = false; imgInfo.text = "Sorry, FlashOS cannot show " + n + "."; status(""); return; }
    imgCur = b;
    imgName = n;
    imgInfo.text = "";
    view._visible = true;
    view.attachBitmap(b, 1, "auto", true);
    layoutImage();
}

function imagesShow() {
    imgPick.reload();
    if (!imgCur) {
        imgInfo.text = imgPick.files.length ? "Pick a picture on the left." : "";
        layoutImage();
        status(imgPick.files.length + (imgPick.files.length == 1 ? " picture" : " pictures"));
    } else layoutImage();
}

// ================================================================ VIDEO
var vid = null, vidName = "";
var vg = vidPage.createEmptyMovieClip("g", 1);
box(vg, VX, VY, VW, VH, 0x000000, CYAN, 3, 12);
var screen = vidPage.createEmptyMovieClip("screen", 2);
var vidInfo = label(vidPage, "info", 4, VX + 20, VY + VH / 2 - 60, VW - 40, 120, "", 18, WHITE, false, "center");
vidInfo.multiline = true;
vidInfo.wordWrap = true;
var vidPick = listPanel(vidPage, "flv", function (n, k) { openVideo(n); });
vidPick.empty.text = "No videos found.\n\nFlash Notepad plays .flv videos (Sorenson Spark with MP3 sound) from a CD or USB stick.";
var playBtn = button(vidPage, "play", nd(), VX, BOTTOM - 52, 110, 42, "Play", function () { togglePlay(); }, 0, 17);
button(vidPage, "stop", nd(), VX + 118, BOTTOM - 52, 100, 42, "Stop", function () { if (vid) { vid.stop(); vidUI(); } }, 0, 17);
var bar = vidPage.createEmptyMovieClip("bar", nd());
bar._x = VX + 232; bar._y = BOTTOM - 42;
var BW = VW - 232 - 150;
box(bar, 0, 0, BW, 22, CYAN_LIGHT, CYAN, 2, 11);
var barFill = bar.createEmptyMovieClip("fill", 2);
bar.onPress = function () {
    if (!vid || vid.duration <= 0) return;
    var f = this._xmouse / BW;
    vid.seek(Math.max(0, Math.min(1, f)) * vid.duration);
    vidUI();
};
var timeTxt = label(vidPage, "time", nd(), W - 16 - 144, BOTTOM - 48, 140, 30, "0:00 / 0:00", 17, INK, true, "right");
var lastSec = -1, lastFillW = -1;

function mmss(t) { t = Math.floor(t); return Math.floor(t / 60) + ":" + two(t % 60); }
function vidUI() {
    if (!vid) return;
    playBtn.lab.text = vid.playing ? "Pause" : "Play";
    var sec = Math.floor(vid.time);
    if (sec != lastSec) { lastSec = sec; timeTxt.text = mmss(vid.time) + " / " + mmss(vid.duration); }
    var fw = vid.duration > 0 ? Math.round((BW - 4) * Math.min(1, vid.time / vid.duration)) : 0;
    if (fw != lastFillW) { lastFillW = fw; barFill.clear(); if (fw > 0) box(barFill, 2, 2, Math.max(fw, 18), 18, CYAN, 0, 0, 9); }
}
vidPage.onEnterFrame = function () { if (page == "video" && vid) vidUI(); };

function openVideo(n) {
    if (vid) vid.close();
    vid = null;
    screen._visible = false;
    if (!FlashOS.openVideo) { vidInfo.text = "This version of FlashOS cannot play videos yet."; return; }
    status("Opening " + n + "...");
    vid = FlashOS.openVideo(n);
    if (!vid) { vidInfo.text = "Sorry, FlashOS cannot play " + n + ".\nIt needs an .flv video made with the Sorenson Spark codec."; status(""); return; }
    vidName = n;
    vidInfo.text = "";
    screen.attachBitmap(vid.bitmap, 1, "auto", true);
    var s = Math.min((VW - 16) / vid.width, (VH - 16) / vid.height);
    screen._xscale = screen._yscale = s * 100;
    screen._x = Math.round(VX + (VW - vid.width * s) / 2);
    screen._y = Math.round(VY + (VH - vid.height * s) / 2);
    screen._visible = true;
    lastSec = -1; lastFillW = -1;
    vid.play();
    vidUI();
    status(n + "   -   " + vid.width + " x " + vid.height + "   -   " + mmss(vid.duration));
}
function togglePlay() {
    if (!vid) { vidPick.select(vidPick.sel < 0 ? 0 : vidPick.sel); return; }
    if (vid.playing) vid.pause(); else vid.play();
    vidUI();
}
function videoShow() {
    vidPick.reload();
    if (!vid) { vidInfo.text = vidPick.files.length ? "Pick a video on the left." : ""; status(vidPick.files.length + (vidPick.files.length == 1 ? " video" : " videos")); }
}
function leaveVideo() { if (vid && vid.playing) { vid.pause(); vidUI(); } }

// ================================================================ CALCULATOR
var cg = calcPage.createEmptyMovieClip("g", 1);
var KX = 262, KY = TOP + 24;
box(cg, KX, KY, 500, 600, WHITE, CYAN, 4, 22);
box(cg, KX + 24, KY + 22, 452, 96, CYAN_PALE, CYAN_LIGHT, 3, 14);
var calcExpr = label(calcPage, "expr", 3, KX + 40, KY + 28, 420, 26, "", 16, GREY, false, "right");
var calcShow = label(calcPage, "show", 4, KX + 40, KY + 52, 420, 60, "0", 42, INK, true, "right");
var cur = "0", acc = 0, pend = "", fresh = true;

function calcNum(s) {
    var v = parseFloat(s);
    return isNaN(v) ? 0 : v;
}
function calcFmt(v) {
    if (!isFinite(v)) return "Error";
    var s = "" + (Math.round(v * 1e10) / 1e10);
    return s.length > 16 ? "" + v : s;
}
function opSign(o) { return o == "*" ? "x" : (o == "/" ? ":" : o); }
function calcApply() {
    var b = calcNum(cur);
    if (pend == "+") acc = acc + b;
    else if (pend == "-") acc = acc - b;
    else if (pend == "*") acc = acc * b;
    else if (pend == "/") acc = b == 0 ? NaN : acc / b;
    else acc = b;
}
function calcKey(k) {
    if (k >= "0" && k <= "9" || k == ".") {
        if (fresh) { cur = k == "." ? "0." : k; fresh = false; }
        else if (!(k == "." && cur.indexOf(".") >= 0) && cur.length < 15) cur = cur == "0" && k != "." ? k : cur + k;
    } else if (k == "+" || k == "-" || k == "*" || k == "/") {
        if (!fresh || pend == "") calcApply();
        pend = k;
        cur = calcFmt(acc);
        fresh = true;
        calcExpr.text = cur + " " + opSign(k);
    } else if (k == "=") {
        if (pend != "") { calcExpr.text = calcFmt(acc) + " " + opSign(pend) + " " + cur + " ="; calcApply(); pend = ""; cur = calcFmt(acc); fresh = true; }
    } else if (k == "C") { cur = "0"; acc = 0; pend = ""; fresh = true; calcExpr.text = ""; }
    else if (k == "<") { if (!fresh) { cur = cur.length > 1 ? cur.substr(0, cur.length - 1) : "0"; } }
    else if (k == "+-") { cur = calcFmt(-calcNum(cur)); }
    else if (k == "%") { cur = calcFmt(calcNum(cur) / 100); }
    calcShow.text = cur;
}
var keys = [["C", "<", "%", "/"], ["7", "8", "9", "*"], ["4", "5", "6", "-"], ["1", "2", "3", "+"], ["+-", "0", ".", "="]];
for (var ky = 0; ky < 5; ky++) {
    for (var kx = 0; kx < 4; kx++) {
        var k = keys[ky][kx];
        var shown = k == "*" ? "x" : (k == "/" ? ":" : (k == "<" ? "Del" : k));
        var kbn = button(calcPage, "k" + ky + "_" + kx, nd(), KX + 24 + kx * 116, KY + 140 + ky * 88, 104, 76, shown, null, kx == 3 ? 2 : 0, 30);
        kbn.k = k;
        kbn.onRelease = function () { this.paint(1); calcKey(this.k); };
        if (k == "=") kbn.setOn(true);
    }
}

// ================================================================ MENUS
function fileMenu() {
    if (page == "draw") return [["New drawing", "Ctrl+N", newDrawing], ["Open picture...", "Ctrl+O", openDrawing],
        ["Save", "Ctrl+S", function () { saveDrawing(false, null); }], ["Save as...", "", function () { saveDrawing(true, null); }],
        ["-"], ["Delete a file...", "", deleteFile], ["-"], ["Exit", "Ctrl+Q", quitApp]];
    if (page == "images") return [["Open picture...", "Ctrl+O", function () { fileDialog("open", "Open a picture", "jpg,jpeg,png,gif,bmp", "", function (n) { imgPick.reload(); for (var i = 0; i < imgPick.files.length; i++) if (imgPick.files[i].name == n) imgPick.select(i); }); }],
        ["Draw on this picture", "", function () { if (imgName != "") { showPage("draw"); checkDrawSaved(function () { var b = FlashOS.loadBitmap(imgName); if (b) { canvas.fillRect(fullRect, 0xFFFFFF); canvas.copyPixels(b, new flash.geom.Rectangle(0, 0, Math.min(b.width, CW), Math.min(b.height, CH)), origin); snapshot(); drawName = ""; drawDirty = true; updateTitle(); } }); } }, imgName != ""],
        ["-"], ["Delete a file...", "", deleteFile], ["-"], ["Exit", "Ctrl+Q", quitApp]];
    if (page == "video") return [["Open video...", "Ctrl+O", function () { fileDialog("open", "Open a video", "flv", "", function (n) { vidPick.reload(); for (var i = 0; i < vidPick.files.length; i++) if (vidPick.files[i].name == n) vidPick.select(i); }); }],
        ["-"], ["Exit", "Ctrl+Q", quitApp]];
    if (page == "calc") return [["Copy result", "", function () { System.setClipboard(cur); status("Copied " + cur + ". Paste it in Text with Ctrl+V."); }], ["-"], ["Exit", "Ctrl+Q", quitApp]];
    return [["New", "Ctrl+N", newText], ["Open...", "Ctrl+O", openText], ["Save", "Ctrl+S", function () { saveText(false, null); }],
        ["Save as...", "", function () { saveText(true, null); }], ["-"], ["Delete a file...", "", deleteFile], ["-"], ["Exit", "Ctrl+Q", quitApp]];
}

function editMenu() {
    if (page == "draw") return [["Undo", "Ctrl+Z", undoDraw], ["Clear the drawing", "", clearCanvas], ["-"],
        ["Pencil", "", function () { pickTool("pen"); }], ["Eraser", "", function () { pickTool("eraser"); }], ["Fill bucket", "", function () { pickTool("fill"); }]];
    if (page == "images") return [["Fit to window", "", function () { imgZoomFit = true; layoutImage(); }], ["Real size", "", function () { imgZoomFit = false; layoutImage(); }],
        ["-"], ["Next picture", "Right", function () { imgPick.select(imgPick.sel + 1); }], ["Previous picture", "Left", function () { imgPick.select(imgPick.sel - 1); }]];
    if (page == "video") return [["Play / Pause", "Space", togglePlay], ["Stop", "", function () { if (vid) { vid.stop(); vidUI(); } }]];
    if (page == "calc") return [["Clear", "Esc", function () { calcKey("C"); }], ["Paste number", "Ctrl+V", function () { var v = calcNum(FlashOS.getClipboard()); cur = calcFmt(v); fresh = false; calcShow.text = cur; }]];
    var hasSel = Selection.getBeginIndex() != Selection.getEndIndex();
    return [["Undo", "Ctrl+Z", function () { textF.undo(); refocus(); }], ["-"],
        ["Cut", "Ctrl+X", function () { textF.cut(); refocus(); }, hasSel], ["Copy", "Ctrl+C", function () { textF.copy(); refocus(); }, hasSel],
        ["Paste", "Ctrl+V", function () { refocus(); textF.paste(); }], ["Delete", "Del", function () { refocus(); textF.replaceSel(""); }, hasSel],
        ["Select all", "Ctrl+A", function () { refocus(); textF.selectAll(); }], ["-"],
        ["Find...", "Ctrl+F", findText], ["Find next", "F3", function () { refocus(); findNext(); }],
        ["Time and date", "F5", insertDate], ["-"],
        ["Bigger text", "Ctrl+Plus", function () { setTextSize(textSize + 2); refocus(); }], ["Smaller text", "Ctrl+Minus", function () { setTextSize(textSize - 2); refocus(); }],
        [wrapOn ? "Word wrap: on" : "Word wrap: off", "", function () { toggleWrap(); refocus(); }]];
}

function helpMenu() {
    return [["About Flash Notepad", "", function () {
        alertBox("About Flash Notepad", "Flash Notepad 1.0 for FlashOS.\nWrite, draw, look at pictures, watch videos and do sums.\nFont: Comic Neue (SIL Open Font License).");
    }], ["Keyboard shortcuts", "", function () {
        alertBox("Keyboard shortcuts", "Ctrl+N new   Ctrl+O open   Ctrl+S save\nCtrl+Z undo   Ctrl+X/C/V cut, copy, paste\nCtrl+F find   F5 time and date   Ctrl+1...5 tabs\nF9/F10/F11 sound   F12 leave");
    }]];
}

menuButton("File", 52, 70, fileMenu);
menuButton("Edit", 126, 70, editMenu);
menuButton("Help", 200, 74, helpMenu);

function deleteFile() {
    fileDialog("open", "Delete a file from the hard disk", "", "", function (n) {
        ask("Delete " + n + "?", "Do you really want to delete " + n + "? This cannot be undone.", ["Delete", "Cancel"], function (k) {
            if (k != 0) return;
            if (FlashOS.remove(n)) {
                status(n + " was deleted.");
                imgList = null;
                if (imgCache[n]) imgCache[n] = null;
            } else alertBox("Delete", "Only files on the hard disk can be deleted. Files on a CD or USB stick stay as they are.");
        });
    });
}

function quitApp() {
    checkSaved(function () { checkDrawSaved(function () { if (vid) vid.close(); FlashOS.exit(); }); });
}

// ================================================================ keyboard
var keyL = {};
keyL.onKeyDown = function () {
    var code = Key.getCode();
    if (dialogKeys) { dialogKeys(code); return; }
    if (menuOpen) { if (code == 27) closeMenu(); return; }
    var ctrl = Key.isDown(Key.CONTROL);
    if (ctrl && code >= 49 && code <= 53) { showPage(["text", "draw", "images", "video", "calc"][code - 49]); return; }
    if (ctrl) {
        if (code == 78) { if (page == "draw") newDrawing(); else if (page == "text") newText(); return; }
        if (code == 79) { var it = fileMenu()[page == "text" || page == "draw" ? 1 : 0]; it[2](); return; }
        if (code == 83) { if (page == "draw") saveDrawing(false, null); else if (page == "text") saveText(false, null); return; }
        if (code == 70 && page == "text") { findText(); return; }
        if (code == 90 && page == "draw") { undoDraw(); return; }
        if ((code == 187 || code == 107) && page == "text") { setTextSize(textSize + 2); return; }
        if ((code == 189 || code == 109) && page == "text") { setTextSize(textSize - 2); return; }
        if (code == 86 && page == "calc") { editMenu()[1][2](); return; }
        return;
    }
    if (page == "text") {
        if (code == 114) findNext();
        if (code == 116) insertDate();
        return;
    }
    if (page == "images") {
        if (code == 39 || code == 34 || code == 32) imgPick.select(imgPick.sel + 1);
        if (code == 37 || code == 33) imgPick.select(imgPick.sel - 1);
        return;
    }
    if (page == "video") { if (code == 32) togglePlay(); return; }
    if (page == "calc") {
        var a = Key.getAscii();
        var ch = String.fromCharCode(a);
        if (code == 13 || ch == "=") calcKey("=");
        else if (code == 8) calcKey("<");
        else if (code == 27) calcKey("C");
        else if (ch == "," ) calcKey(".");
        else if (ch == "x" || ch == "X") calcKey("*");
        else if ("0123456789.+-*/%".indexOf(ch) >= 0 && a > 0) calcKey(ch);
    }
};
Key.addListener(keyL);

// ================================================================ start
var oldShow = showPage;
showPage = function (name) {
    if (page == "video" && name != "video") leaveVideo();
    oldShow(name);
};
for (var tk in tabs) {
    tabs[tk].nm = tk;
    tabs[tk].onRelease = function () { this.paint(1); showPage(this.nm); };
}
storeTxt.text = FlashOS.storage() == "disk" ? "Files are saved on the hard disk" : "FlashOS not installed: saving to memory";
pickTool("pen");
showPage("text");
textStatus();
