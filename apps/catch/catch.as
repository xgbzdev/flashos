// CATCH for ByroTT / FlashOS
// Move the basket and catch the falling balls. Gold balls are worth more,
// black bombs cost a life. Every 10 balls the level goes up and they fall faster.
// Copyright (c) 2026 FlashOS. All rights reserved.
// Font: Press Start 2P by CodeMan38, SIL Open Font License 1.1.

var FONT = "Press Start 2P";
var W = 800, H = 600, GROUND = 540;
var BW = 120, BH = 34;                   // the basket

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
function disc(mc, x, y, r, c) {
    mc.beginFill(c, 100);
    mc.moveTo(x + r, y);
    for (var i = 1; i <= 20; i++) { var a = i / 20 * Math.PI * 2; mc.lineTo(x + Math.cos(a) * r, y + Math.sin(a) * r); }
    mc.endFill();
}

// ---------------------------------------------------------------- scenery
var sky = _root.createEmptyMovieClip("sky", 1);
for (var i = 0; i < 20; i++) {
    var t = i / 19;
    box(sky, 0, i * 28, W, 29, (Math.round(0x20 + 0x50 * t) << 16) | (Math.round(0x40 + 0x80 * t) << 8) | Math.round(0x90 + 0x50 * t));
}
var hills = _root.createEmptyMovieClip("hills", 2);
hills.beginFill(0x2E8A3E, 100);
hills.moveTo(0, GROUND);
for (var x = 0; x <= W; x += 20) hills.lineTo(x, GROUND - 40 - Math.sin(x / 90) * 26 - Math.sin(x / 37) * 8);
hills.lineTo(W, GROUND); hills.lineTo(0, GROUND);
hills.endFill();
box(hills, 0, GROUND, W, H - GROUND, 0x6A4A26);
box(hills, 0, GROUND, W, 6, 0x3FAA4E);

var layer = _root.createEmptyMovieClip("balls", 5);
var basket = _root.createEmptyMovieClip("basket", 10);
basket.lineStyle(3, 0x3A2410, 100);
basket.beginFill(0xC8873A, 100);
basket.moveTo(-BW / 2, 0); basket.lineTo(BW / 2, 0); basket.lineTo(BW / 2 - 14, BH); basket.lineTo(-BW / 2 + 14, BH); basket.lineTo(-BW / 2, 0);
basket.endFill();
basket.lineStyle(2, 0x7A4E1E, 100);
for (var k = -2; k <= 2; k++) { basket.moveTo(k * 22, 4); basket.lineTo(k * 18, BH - 3); }
basket.moveTo(-BW / 2 + 6, 12); basket.lineTo(BW / 2 - 6, 12);
basket.moveTo(-BW / 2 + 10, 23); basket.lineTo(BW / 2 - 10, 23);
basket.lineStyle();
basket._y = GROUND - BH;

var hud = _root.createEmptyMovieClip("hud", 20);
box(hud, 0, 0, W, 44, 0x000000);
var scoreT = label(hud, "s", 1, 16, 14, 300, 24, "", 14, 0xFFFFFF);
var levelT = label(hud, "l", 2, 300, 14, 200, 24, "", 14, 0xFFD21E, "center");
var livesT = label(hud, "v", 3, W - 316, 14, 300, 24, "", 14, 0xFF6060, "right");
var msg = _root.createEmptyMovieClip("msg", 30);

// ---------------------------------------------------------------- game state
var balls = [], score, lives, caught, level, best = 0, playing = false, spawnT, nextId = 0, bx = W / 2;
var useMouse = false, lastMx = -1;

function hudText() {
    scoreT.text = "SCORE " + score;
    levelT.text = "LEVEL " + level;
    var h = "";
    for (var i = 0; i < lives; i++) h += "o";
    livesT.text = "LIVES " + h;
}
function message(title, sub) {
    msg.clear();
    if (msg.t) { msg.t.removeTextField(); msg.s.removeTextField(); }
    if (title == "") return;
    box(msg, 140, 190, W - 280, 200, 0x000000);
    msg._alpha = 100;
    label(msg, "t", 1, 140, 226, W - 280, 40, title, 28, 0xFFD21E, "center");
    label(msg, "s", 2, 160, 290, W - 320, 100, sub, 12, 0xFFFFFF, "center");
}
function start() {
    for (var i = 0; i < balls.length; i++) balls[i].removeMovieClip();
    balls = [];
    score = 0; lives = 3; caught = 0; level = 1;
    spawnT = 0;
    playing = true;
    message("", "");
    hudText();
}
function newBall() {
    var r = Math.random(), kind = r < 0.12 + level * 0.015 ? "bomb" : (r < 0.24 + level * 0.015 ? "gold" : "ball");
    var b = layer.createEmptyMovieClip("b" + nextId, nextId);
    nextId++;
    var col = [0xE0443C, 0x2F6FE0, 0x2FAE5B, 0xFF6EC7, 0x1EC8DC][Math.floor(Math.random() * 5)];
    if (kind == "bomb") {
        disc(b, 0, 0, 15, 0x161616);
        b.lineStyle(3, 0x8A6A3A, 100); b.moveTo(4, -14); b.curveTo(10, -24, 16, -20); b.lineStyle();
        disc(b, 17, -21, 4, 0xFFB020);
    } else {
        disc(b, 0, 0, 15, kind == "gold" ? 0xFFD21E : col);
        disc(b, -5, -5, 5, 0xFFFFFF);
        b.lineStyle(2, 0x000000, 40); b.moveTo(15, 0);
        for (var i = 1; i <= 20; i++) { var a = i / 20 * Math.PI * 2; b.lineTo(Math.cos(a) * 15, Math.sin(a) * 15); }
        b.lineStyle();
    }
    b.kind = kind;
    b._x = 30 + Math.random() * (W - 60);
    b._y = 30;
    b.vy = 2.2 + level * 0.55 + Math.random() * 1.2;
    b.vx = (Math.random() - 0.5) * (level > 3 ? 2 : 0.8);
    balls.push(b);
}
function pop(x, y, text, col) {
    var p = _root.createEmptyMovieClip("pop" + nextId, 1000 + (nextId % 50));
    nextId++;
    label(p, "t", 1, x - 60, y - 30, 120, 24, text, 12, col, "center");
    p.life = 24;
    p.onEnterFrame = function () { this._y -= 2; this._alpha -= 4; if (--this.life <= 0) this.removeMovieClip(); };
}

_root.onEnterFrame = function () {
    // the basket: arrows or the mouse
    if (_root._xmouse != lastMx) { if (lastMx >= 0) useMouse = true; lastMx = _root._xmouse; }
    var speed = 9 + level;
    if (Key.isDown(Key.LEFT)) { bx -= speed; useMouse = false; }
    if (Key.isDown(Key.RIGHT)) { bx += speed; useMouse = false; }
    if (useMouse) bx += (_root._xmouse - bx) * 0.5;
    if (bx < BW / 2) bx = BW / 2;
    if (bx > W - BW / 2) bx = W - BW / 2;
    basket._x = bx;
    if (!playing) return;
    if (--spawnT <= 0) {
        newBall();
        spawnT = Math.max(14, 52 - level * 5) + Math.floor(Math.random() * 16);
    }
    for (var i = balls.length - 1; i >= 0; i--) {
        var b = balls[i];
        b._y += b.vy;
        b._x += b.vx;
        if (b._x < 16 || b._x > W - 16) b.vx = -b.vx;
        var inBasket = b._y >= GROUND - BH - 4 && b._y <= GROUND - BH + 14 && Math.abs(b._x - bx) < BW / 2 - 6;
        if (inBasket) {
            if (b.kind == "bomb") { lives--; pop(b._x, b._y, "BOOM!", 0xFF6060); }
            else {
                var pts = b.kind == "gold" ? 50 : 10;
                score += pts * level;
                caught++;
                pop(b._x, b._y, "+" + pts * level, b.kind == "gold" ? 0xFFD21E : 0xFFFFFF);
                if (caught % 10 == 0) { level++; pop(W / 2, 240, "LEVEL " + level, 0x1EC8DC); }
            }
            b.removeMovieClip();
            balls.splice(i, 1);
        } else if (b._y > GROUND + 4) {
            if (b.kind != "bomb") { lives--; pop(b._x, GROUND - 10, "MISS", 0xFF6060); }
            b.removeMovieClip();
            balls.splice(i, 1);
        }
    }
    hudText();
    if (lives <= 0) {
        playing = false;
        if (score > best) best = score;
        message("GAME OVER", "SCORE " + score + "   BEST " + best + "\n\nPRESS SPACE TO PLAY AGAIN");
    }
};

var keys = {};
keys.onKeyDown = function () {
    if (Key.getCode() == 32 && !playing) start();
};
Key.addListener(keys);
_root.onMouseDown = function () { if (!playing) start(); };

score = 0; lives = 3; level = 1;
hudText();
message("CATCH!", "CATCH THE BALLS WITH THE BASKET.\nGOLD = 50 POINTS. AVOID THE BOMBS!\n\nARROWS OR MOUSE TO MOVE\nPRESS SPACE TO START");
