// ByroTT 2.0 Setup (Byr.swf)
// Copyright (c) 2026 FlashOS. All rights reserved.
// Font: GNU Unifont (SIL Open Font License 1.1), like the Scratch mock-up.
// A cyan panel on black, pixel text, keys: Space = next, Esc = back.

var W = 1024, H = 768;
var PIX = "Unifont";
var INK = 0x000000, WHITE = 0xFFFFFF, C1 = 0x1EC8DC;
var PX = 150, PY = 110, PW = 724, PH = 548;      // the panel
var MIN_MB = 2048;                               // 2 GB

function fmt(size, color, align) {
    var f = new TextFormat();
    f.font = PIX; f.size = size < 24 ? 16 : 32; f.color = color; f.align = align ? align : "left";
    return f;
}
function label(mc, name, depth, x, y, w, h, text, size, color, align) {
    mc.createTextField(name, depth, x, y, w, h);
    var t = mc[name];
    t.embedFonts = true; t.selectable = false; t.multiline = true; t.wordWrap = true;
    t.setNewTextFormat(fmt(size, color, align));
    t.text = text;
    return t;
}
function box(mc, x, y, w, h, fill) {
    mc.beginFill(fill, 100);
    mc.moveTo(x, y); mc.lineTo(x + w, y); mc.lineTo(x + w, y + h); mc.lineTo(x, y + h); mc.lineTo(x, y);
    mc.endFill();
}

// ---------------------------------------------------------------- the screen
var bg = _root.createEmptyMovieClip("bg", 1);
box(bg, 0, 0, W, H, 0x000000);
var panel = _root.createEmptyMovieClip("panel", 2);
// cyan, getting paler towards the bottom (drawn in bands: a 2000s look)
for (var i = 0; i < 32; i++) {
    var t = i / 31;
    var r = Math.round(0x6C + (0xD0 - 0x6C) * t), g = Math.round(0xDE + (0xF2 - 0xDE) * t), b = Math.round(0xE2 + (0xF4 - 0xE2) * t);
    box(panel, PX, PY + Math.floor(i * PH / 32), PW, Math.ceil(PH / 32) + 1, (r << 16) | (g << 8) | b);
}
var page = null, next = null, back = null, footL, footR;

function newPage() {
    if (page) page.removeMovieClip();
    page = _root.createEmptyMovieClip("page", 10);
    next = null; back = null; tiles = [];
    footR = label(page, "fr", 90, PX + 40, PY + PH - 50, PW - 60, 40, "", 32, INK, "right");
    footL = label(page, "fl", 91, PX + 20, PY + 14, 400, 24, "", 16, 0x30474A, "left");
    return page;
}
function heading(p, text) { return label(p, "h", 1, PX + 50, PY + 50, PW - 80, 150, text, 32, INK); }
function body(p, y, text) { return label(p, "b", 2, PX + 50, y, PW - 100, 300, text, 16, INK); }
function logo(p, x, y, size, col) {
    var bm = FlashOS.loadBitmap(size > 140 ? "logo_big.png" : (size > 44 ? "logo_mid.png" : "logo_small.png"));
    var mc = p.createEmptyMovieClip("lg", 3);
    mc._x = x; mc._y = y;
    if (bm) { mc.attachBitmap(bm, 1, "auto", true); mc._xscale = mc._yscale = size / bm.width * 100; }
    new Color(mc).setRGB(col);
    return mc;
}

// 1. welcome
function welcome() {
    var p = newPage();
    logo(p, PX + 50, PY + 46, 128, INK);
    label(p, "h", 1, PX + 200, PY + 70, PW - 240, 100, "Welcome to\nByroTT 2.0", 32, INK);
    body(p, PY + 210, "This Setup will install ByroTT on your hard disk.\n\n" +
         "ByroTT is a desktop with a terminal for SWF games, files, the internet, notepad and settings.\n\n" +
         "You can uninstall it at any time in Settings: FlashOS comes back.");
    footR.text = "Press Space to Go Next";
    footL.text = "Esc = Exit";
    next = drives;
    back = function () { FlashOS.exit(); };
}

// 2. which drive
var disks = [], sel = -1, tiles = [];
function gbText(mb) {
    var gb = Math.floor(mb / 1024 * 100) / 100, s = "" + gb, k = s.indexOf(".");
    if (k < 0) s += ",00"; else { s = s.substr(0, k) + "," + s.substr(k + 1); if (s.length - k < 3) s += "0"; }
    return s;
}
function drives() {
    var p = newPage();
    heading(p, "In What Drive do you want\nInstall Byrott\n(Minimun Space Required is 2GB)");
    disks = FlashOS.disks ? FlashOS.disks() : [];
    tiles = [];
    sel = -1;
    var letters = "ABCD";
    for (var i = 0; i < disks.length && i < 4; i++) {
        var tl = p.createEmptyMovieClip("d" + i, 20 + i);
        tl._x = PX + 50 + i * 160; tl._y = PY + 210;
        tl.idx = i;
        tl.ok = disks[i].mb >= MIN_MB;
        tl.fr = tl.createEmptyMovieClip("fr", 1);
        label(tl, "t", 2, 10, 56, 134, 120, letters.charAt(i) + "\n(" + gbText(disks[i].mb) + "\nGB FREE)", 32, tl.ok ? WHITE : 0x8A8A8A);
        tl.paint = function () {
            this.fr.clear();
            if (sel == this.idx) box(this.fr, -5, -5, 154, 226, WHITE);
            box(this.fr, 0, 0, 144, 216, this.ok ? 0x000000 : 0x2A2A2A);
        };
        tl.onRelease = function () { if (this.ok) { sel = this.idx; paintTiles(); } };
        tiles.push(tl);
        if (sel < 0 && tl.ok) sel = i;
    }
    paintTiles();
    if (disks.length == 0)
        body(p, PY + 230, "No hard disk was found.\n\nVirtualBox: Settings, Storage: add a hard disk on an IDE controller.");
    else if (sel < 0)
        body(p, PY + 440, "No drive has 2 GB. Please use a bigger hard disk.");
    footR.text = "Press Space to Go Next";
    footL.text = "Esc = Back   Arrows = Drive";
    next = function () { if (sel >= 0) ready(); };
    back = welcome;
}
function paintTiles() { for (var i = 0; i < tiles.length; i++) tiles[i].paint(); }
function moveSel(d) {
    var n = sel;
    for (var k = 0; k < tiles.length; k++) {
        n += d;
        if (n < 0 || n >= tiles.length) return;
        if (tiles[n].ok) { sel = n; paintTiles(); return; }
    }
}

// 3. ready
function ready() {
    var p = newPage();
    var letters = "ABCD";
    heading(p, "Ready to Install ByroTT\non Drive " + letters.charAt(sel));
    body(p, PY + 170,
         "- The computer will start ByroTT from now on, even with the CD inside.\n\n" +
         "- A copy of FlashOS stays inside ByroTT. Uninstall ByroTT in Settings and FlashOS comes back.\n\n" +
         "- Your saved files are kept.\n\n" +
         "The system will ask you to confirm: press Y.");
    footR.text = "Press Space to Install";
    footL.text = "Esc = Back";
    next = install;
    back = drives;
}
function install() {
    var p = newPage();
    heading(p, "INSTALLING PLEASE DO NOT\nSHUT DOWN IT CAN POTENTIALLY\nBRICK YOUR COMPUTER!");
    next = null; back = null;
    // let this page show first: the install itself stops everything else
    setTimeout(function () {
        var r = FlashOS.install ? FlashOS.install(sel) : "this FlashOS cannot install other systems";
        if (r == "ok") done();
        else if (r == "cancelled") ready();
        else failed(r);
    }, 200);
}

// 4. done
function done() {
    var p = newPage();
    logo(p, PX + 50, PY + 46, 128, INK);
    label(p, "h", 1, PX + 200, PY + 70, PW - 240, 100, "Done Restart", 32, INK);
    body(p, PY + 230, "ByroTT is installed. The computer will restart and start ByroTT.\n\nYou can leave the CD in the drive.");
    footR.text = "Press Space to Restart";
    footL.text = "";
    next = function () { FlashOS.restart(); };
    back = null;
}
function failed(msg) {
    var p = newPage();
    heading(p, "ByroTT was not\nInstalled");
    body(p, PY + 170, "Reason: " + msg + ".\n\nStart the computer from the ByroTT CD and type Byr.swf to install.");
    footR.text = "Press Space to Exit";
    footL.text = "Esc = Back";
    next = function () { FlashOS.exit(); };
    back = ready;
}

var keys = {};
keys.onKeyDown = function () {
    var c = Key.getCode();
    if (c == 32 && next) next();
    else if (c == 27 && back) back();
    else if (c == 37 && tiles.length) moveSel(-1);
    else if (c == 39 && tiles.length) moveSel(1);
};
Key.addListener(keys);
welcome();
