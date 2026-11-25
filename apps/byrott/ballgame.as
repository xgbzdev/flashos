// Ball Game v2.0 for ByroTT: TRY DODGE THE BALL!
// Move the white bar with the arrow keys (or the mouse) and do not let the
// balls touch it. Every 15 seconds the level goes up: the balls get faster
// and one more joins in. Catch the falling power-ups:
//   S  shield: the next ball that hits you bounces off
//   T  slow time: the balls move at half speed for 6 seconds
//   W  wide bar: the bar is wider for 8 seconds
// The best 5 scores are saved on the disk (ballgame.hi).
// Copyright (c) 2026 FlashOS. All rights reserved.
// Font: GNU Unifont (SIL Open Font License 1.1).

var FONT = "Unifont";
var W = 800, H = 560, TOP = 80;
var BAR_H = 22, BAR_Y = H - 60, BAR_W0 = 130;
var R = 30;                                      // ball radius
var HI_FILE = "ballgame.hi";

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

// ---------------------------------------------------------------- the screen
var bg = _root.createEmptyMovieClip("bg", 1);
box(bg, 0, 0, W, H, 0x990000);
label(_root, "title", 2, 30, 22, 420, 40, "TRY DODGE THE BALL!", 32, 0xFFFFFF);
var scoreT = label(_root, "score", 3, W - 360, 14, 340, 22, "", 16, 0xFFFFFF, "right");
var levelT = label(_root, "level", 4, W - 360, 36, 340, 22, "", 16, 0xFFD040, "right");
var powT = label(_root, "pow", 5, 30, H - 30, 740, 22, "", 16, 0xFFFFFF);

var bar = _root.createEmptyMovieClip("bar", 10);
var barW = BAR_W0, shield = false;
function drawBar() {
    bar.clear();
    bar.lineStyle(4, shield ? 0x40A0FF : 0x000000, 100);
    bar.beginFill(shield ? 0xC8E4FF : 0xFFFFFF, 100);
    bar.moveTo(0, 0); bar.lineTo(barW, 0); bar.lineTo(barW, BAR_H); bar.lineTo(0, BAR_H); bar.lineTo(0, 0);
    bar.endFill();
}
bar._y = BAR_Y;
drawBar();
var bx = (W - barW) / 2;

var msg = _root.createEmptyMovieClip("msg", 300);
var balls = [], pows = [], nextId = 0, t0, playing = false, lastMx = -1, useMouse = false;
var level = 1, points = 0, slowUntil = 0, wideUntil = 0, nextPow = 0, lastTick = 0;
var scores = loadScores();

// ---------------------------------------------------------------- high scores
function loadScores() {
    var out = [];
    var t = FlashOS.read ? FlashOS.read(HI_FILE) : null;
    if (t == null) return out;
    var ls = t.split("\n");
    for (var i = 0; i < ls.length; i++) {
        var k = ls[i].indexOf("\t");
        if (k > 0) out.push([ls[i].substr(0, k), Number(ls[i].substr(k + 1))]);
    }
    return out;
}
function saveScores() {
    var t = "";
    for (var i = 0; i < scores.length; i++) t += scores[i][0] + "\t" + scores[i][1] + "\n";
    if (FlashOS.write) FlashOS.write(HI_FILE, t);
}
function addScore(name, pts) {
    scores.push([name, pts]);
    scores.sort(function (a, b) { return b[1] - a[1]; });
    while (scores.length > 5) scores.pop();
    saveScores();
}
function qualifies(pts) { return pts > 0 && (scores.length < 5 || pts > scores[scores.length - 1][1]); }
function scoreTable() {
    var t = "BEST SCORES\n";
    if (scores.length == 0) return t + "(none yet)";
    for (var i = 0; i < scores.length; i++) {
        var n = scores[i][0];
        while (n.length < 12) n += " ";
        var p = "" + scores[i][1];
        while (p.length < 7) p = " " + p;
        t += (i + 1) + ". " + n + p + "\n";
    }
    return t;
}
function best() { return scores.length ? scores[0][1] : 0; }

// ---------------------------------------------------------------- balls and power-ups
function addBall() {
    var b = _root.createEmptyMovieClip("ball" + nextId, 20 + (nextId % 200));
    nextId++;
    disc(b, R, 0xFFFFFF, 0x000000);
    b._x = R + Math.random() * (W - 2 * R);
    b._y = TOP + R;
    var speed = 4.5 + level * 0.9;
    var a = (0.25 + Math.random() * 0.5) * Math.PI;
    b.vx = Math.cos(a) * speed * (Math.random() < 0.5 ? -1 : 1);
    b.vy = Math.sin(a) * speed;
    balls.push(b);
}
var POWS = [["S", 0x40A0FF], ["T", 0x22CC55], ["W", 0xFFD040]];
function addPow() {
    var k = Math.floor(Math.random() * POWS.length);
    var p = _root.createEmptyMovieClip("pow" + nextId, 230 + (nextId % 50));
    nextId++;
    p.lineStyle(3, 0x000000, 100);
    box(p, -18, -18, 36, 36, POWS[k][1]);
    p.lineStyle();
    label(p, "t", 1, -18, -12, 36, 22, POWS[k][0], 16, 0x000000, "center");
    p.kind = POWS[k][0];
    p._x = 40 + Math.random() * (W - 80);
    p._y = TOP;
    pows.push(p);
}
function clearAll() {
    for (var i = 0; i < balls.length; i++) balls[i].removeMovieClip();
    for (i = 0; i < pows.length; i++) pows[i].removeMovieClip();
    balls = []; pows = [];
}

// ---------------------------------------------------------------- messages
var nameField = null;
function showMessage(title, sub, h) {
    msg.clear();
    if (msg.t) { msg.t.removeTextField(); msg.s.removeTextField(); }
    if (title == "") return;
    if (!h) h = 300;
    var y = Math.round((H - h) / 2) + 20;
    msg.lineStyle(4, 0x000000, 100);
    box(msg, 150, y, W - 300, h, 0xFFFFFF);
    msg.lineStyle();
    label(msg, "t", 1, 150, y + 18, W - 300, 40, title, 32, 0x000000, "center");
    label(msg, "s", 2, 170, y + 70, W - 340, h - 80, sub, 16, 0x000000, "center");
}
function askName(pts) {
    showMessage("NEW BEST SCORE!", pts + " POINTS\n\nTYPE YOUR NAME AND PRESS ENTER:", 260);
    msg.createTextField("nm", 3, 260, 330, 280, 26);
    nameField = msg.nm;
    nameField.embedFonts = true;
    nameField.type = "input";
    nameField.border = true;
    nameField.borderColor = 0x000000;
    nameField.background = true;
    nameField.backgroundColor = 0xFFFFFF;
    nameField.maxChars = 12;
    nameField.setNewTextFormat(fmt(16, 0x000000, "center"));
    nameField.text = "";
    nameField.pts = pts;
    Selection.setFocus(nameField);
}
function startScreen() {
    showMessage("BALL GAME v2.0", "ARROWS OR MOUSE MOVE THE WHITE BAR.\nDO NOT LET THE BALLS TOUCH IT!\nCATCH  S = SHIELD   T = SLOW TIME   W = WIDE BAR\n\n" +
                scoreTable() + "\nPRESS SPACE OR CLICK TO START", 380);
}

// ---------------------------------------------------------------- game
function start() {
    clearAll();
    level = 1; points = 0; shield = false; barW = BAR_W0; slowUntil = 0; wideUntil = 0;
    drawBar();
    addBall();
    t0 = getTimer();
    lastTick = t0;
    nextPow = t0 + 6000;
    playing = true;
    powT.text = "";
    showMessage("", "");
}
function gameOver() {
    playing = false;
    if (qualifies(points)) askName(points);
    else showMessage("THE BALL HIT YOU!", "LEVEL " + level + "   " + points + " POINTS\n\n" + scoreTable() + "\nPRESS SPACE OR CLICK TO PLAY AGAIN", 330);
}
function hud() {
    scoreT.text = "POINTS " + points + "   BEST " + Math.max(best(), points);
    levelT.text = "LEVEL " + level;
}
function power(kind) {
    var now = getTimer();
    if (kind == "S") { shield = true; drawBar(); }
    if (kind == "T") slowUntil = now + 6000;
    if (kind == "W") { wideUntil = now + 8000; barW = 210; drawBar(); }
}

_root.onEnterFrame = function () {
    if (_root._xmouse != lastMx) { if (lastMx >= 0) useMouse = true; lastMx = _root._xmouse; }
    if (Key.isDown(Key.LEFT)) { bx -= 11; useMouse = false; }
    if (Key.isDown(Key.RIGHT)) { bx += 11; useMouse = false; }
    if (useMouse) bx = _root._xmouse - barW / 2;
    if (bx < 0) bx = 0;
    if (bx > W - barW) bx = W - barW;
    bar._x = bx;
    if (!playing) return;
    var now = getTimer();
    // points: one per tenth of a second, times the level
    while (now - lastTick >= 100) { lastTick += 100; points += level; }
    var newLevel = 1 + Math.floor((now - t0) / 15000);
    if (newLevel != level) { level = newLevel; if (balls.length < 6) addBall(); }
    if (now >= nextPow) { addPow(); nextPow = now + 7000 + Math.random() * 5000; }
    if (wideUntil && now > wideUntil) { wideUntil = 0; barW = BAR_W0; drawBar(); }
    var slow = now < slowUntil ? 0.5 : 1;
    var st = "";
    if (shield) st += "SHIELD   ";
    if (slow < 1) st += "SLOW TIME " + Math.ceil((slowUntil - now) / 1000) + "   ";
    if (wideUntil) st += "WIDE BAR " + Math.ceil((wideUntil - now) / 1000);
    powT.text = st;
    hud();
    // power-ups fall straight down
    for (var p = pows.length - 1; p >= 0; p--) {
        var q = pows[p];
        q._y += 3;
        if (q._y + 18 >= BAR_Y && q._y - 18 <= BAR_Y + BAR_H && q._x + 18 >= bx && q._x - 18 <= bx + barW) {
            power(q.kind);
            q.removeMovieClip(); pows.splice(p, 1);
        } else if (q._y > H + 20) { q.removeMovieClip(); pows.splice(p, 1); }
    }
    for (var i = 0; i < balls.length; i++) {
        var b = balls[i];
        b._x += b.vx * slow;
        b._y += b.vy * slow;
        if (b._x < R) { b._x = R; b.vx = -b.vx; }
        if (b._x > W - R) { b._x = W - R; b.vx = -b.vx; }
        if (b._y < TOP + R) { b._y = TOP + R; b.vy = -b.vy; }
        if (b._y > H - R) { b._y = H - R; b.vy = -b.vy; }
        var nx = Math.max(bx, Math.min(b._x, bx + barW));
        var ny = Math.max(BAR_Y, Math.min(b._y, BAR_Y + BAR_H));
        var dx = b._x - nx, dy = b._y - ny;
        if (dx * dx + dy * dy < (R - 2) * (R - 2)) {
            if (shield) {                       // the shield takes the hit
                shield = false; drawBar();
                b.vy = -Math.abs(b.vy);
                b._y = BAR_Y - R - 2;
                continue;
            }
            gameOver();
            return;
        }
    }
};

var keys = {};
keys.onKeyDown = function () {
    var c = Key.getCode();
    if (nameField) {
        if (c == 13) {
            var n = nameField.text;
            if (n == "") n = "PLAYER";
            var pts = nameField.pts;
            nameField = null;
            Selection.setFocus(null);
            msg.nm.removeTextField();
            addScore(n.toUpperCase(), pts);
            showMessage("SAVED!", "LEVEL " + level + "   " + pts + " POINTS\n\n" + scoreTable() + "\nPRESS SPACE OR CLICK TO PLAY AGAIN", 330);
        }
        return;
    }
    if (c == 32 && !playing) start();
};
Key.addListener(keys);
_root.onMouseDown = function () { if (!playing && !nameField) start(); };

level = 1;
hud();
startScreen();
