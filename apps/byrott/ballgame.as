// Ball Game v1.0 for ByroTT: TRY DODGE THE BALL!
// Move the white bar with the arrow keys (or the mouse) and do not let the
// balls touch it. Every 10 seconds another ball joins in.
// Copyright (c) 2026 FlashOS. All rights reserved.
// Font: GNU Unifont (SIL Open Font License 1.1).

var FONT = "Unifont";
var W = 800, H = 560;
var BAR_W = 130, BAR_H = 22, BAR_Y = H - 60;
var R = 30;                                      // ball radius

function fmt(size, color, align) {
    var f = new TextFormat();
    f.font = FONT; f.size = size; f.color = color; f.align = align ? align : "left";
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
function box(mc, x, y, w, h, c) {
    mc.beginFill(c, 100);
    mc.moveTo(x, y); mc.lineTo(x + w, y); mc.lineTo(x + w, y + h); mc.lineTo(x, y + h); mc.lineTo(x, y);
    mc.endFill();
}
function disc(mc, r, fill, line) {
    mc.lineStyle(5, line, 100);
    mc.beginFill(fill, 100);
    mc.moveTo(r, 0);
    for (var i = 1; i <= 28; i++) { var a = i / 28 * Math.PI * 2; mc.lineTo(Math.cos(a) * r, Math.sin(a) * r); }
    mc.endFill();
    mc.lineStyle();
}

var bg = _root.createEmptyMovieClip("bg", 1);
box(bg, 0, 0, W, H, 0x990000);
label(_root, "title", 2, 40, 22, 600, 40, "TRY DODGE THE BALL!", 32, 0xFFFFFF);
var scoreT = label(_root, "score", 3, W - 320, 30, 290, 24, "", 16, 0xFFFFFF, "right");

var bar = _root.createEmptyMovieClip("bar", 10);
bar.lineStyle(4, 0x000000, 100);
bar.beginFill(0xFFFFFF, 100);
bar.moveTo(0, 0); bar.lineTo(BAR_W, 0); bar.lineTo(BAR_W, BAR_H); bar.lineTo(0, BAR_H); bar.lineTo(0, 0);
bar.endFill();
bar._y = BAR_Y;
var bx = (W - BAR_W) / 2;

var msg = _root.createEmptyMovieClip("msg", 30);
var balls = [], nextId = 0, t0, best = 0, playing = false, lastMx = -1, useMouse = false, waitUntil = 0;

function addBall() {
    var b = _root.createEmptyMovieClip("ball" + nextId, 20 + nextId);
    nextId++;
    disc(b, R, 0xFFFFFF, 0x000000);
    b._x = R + Math.random() * (W - 2 * R);
    b._y = 90;
    var speed = 5 + balls.length * 0.8;
    var a = (0.25 + Math.random() * 0.5) * Math.PI;      // downwards, a bit sideways
    b.vx = Math.cos(a) * speed * (Math.random() < 0.5 ? -1 : 1);
    b.vy = Math.sin(a) * speed;
    balls.push(b);
}
function showMessage(title, sub) {
    msg.clear();
    if (msg.t) { msg.t.removeTextField(); msg.s.removeTextField(); }
    if (title == "") return;
    msg.lineStyle(4, 0x000000, 100);
    box(msg, 160, 180, W - 320, 170, 0xFFFFFF);
    msg.lineStyle();
    label(msg, "t", 1, 160, 206, W - 320, 40, title, 32, 0x000000, "center");
    label(msg, "s", 2, 170, 264, W - 340, 80, sub, 16, 0x000000, "center");
}
function start() {
    for (var i = 0; i < balls.length; i++) balls[i].removeMovieClip();
    balls = [];
    addBall();
    t0 = getTimer();
    playing = true;
    showMessage("", "");
}
function seconds() { return Math.floor((getTimer() - t0) / 1000); }

_root.onEnterFrame = function () {
    if (_root._xmouse != lastMx) { if (lastMx >= 0) useMouse = true; lastMx = _root._xmouse; }
    if (Key.isDown(Key.LEFT)) { bx -= 11; useMouse = false; }
    if (Key.isDown(Key.RIGHT)) { bx += 11; useMouse = false; }
    if (useMouse) bx = _root._xmouse - BAR_W / 2;
    if (bx < 0) bx = 0;
    if (bx > W - BAR_W) bx = W - BAR_W;
    bar._x = bx;
    if (!playing) return;
    var s = seconds();
    scoreT.text = "TIME " + s + "   BEST " + best;
    if (balls.length < 1 + Math.floor(s / 10) && balls.length < 6) addBall();
    for (var i = 0; i < balls.length; i++) {
        var b = balls[i];
        b._x += b.vx;
        b._y += b.vy;
        if (b._x < R) { b._x = R; b.vx = -b.vx; }
        if (b._x > W - R) { b._x = W - R; b.vx = -b.vx; }
        if (b._y < 80 + R) { b._y = 80 + R; b.vy = -b.vy; }
        if (b._y > H - R) { b._y = H - R; b.vy = -b.vy; }
        // does the ball touch the bar?
        var nx = Math.max(bx, Math.min(b._x, bx + BAR_W));
        var ny = Math.max(BAR_Y, Math.min(b._y, BAR_Y + BAR_H));
        var dx = b._x - nx, dy = b._y - ny;
        if (dx * dx + dy * dy < (R - 2) * (R - 2)) {
            playing = false;
            if (s > best) best = s;
            scoreT.text = "TIME " + s + "   BEST " + best;
            showMessage("THE BALL HIT YOU!", "YOU LASTED " + s + " SECONDS\n\nPRESS SPACE OR CLICK TO PLAY AGAIN");
            return;
        }
    }
};

var keys = {};
keys.onKeyDown = function () { if (Key.getCode() == 32 && !playing) start(); };
Key.addListener(keys);
_root.onMouseDown = function () { if (!playing) start(); };

scoreT.text = "TIME 0   BEST 0";
showMessage("BALL GAME v1.0", "ARROWS OR MOUSE MOVE THE WHITE BAR.\nDO NOT LET THE BALLS TOUCH IT!\n\nPRESS SPACE OR CLICK TO START");
