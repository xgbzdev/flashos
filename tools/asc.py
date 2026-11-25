#!/usr/bin/env python3
"""asc: a small ActionScript 1.0 compiler for FlashOS apps.
Copyright (c) 2026 FlashOS. All rights reserved.

Compiles a script into AVM1 bytecode and wraps it in a one-frame SWF,
optionally with an embedded TrueType font (as DefineFont3) so that text
fields can use it with  tf.embedFonts = true; fmt.font = "<font name>".

  asc.py app.as out.swf [--size 1024x768] [--fps 30] [--bg 000000]
                        [--font file.ttf "Font Name"] [--title Name]

Supported: var, function (declarations, expressions, closures), if/else,
while, do/while, for, for-in, switch, break, continue, return, all the
usual operators, object and array literals, new, typeof, instanceof, delete.
"""
import os
import re
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_demo_swf import Bits, rect, tag, sbits  # noqa: E402

# ---------------------------------------------------------------- lexer
PUNCT = sorted(""">>>= === !== >>> <<= >>= == != <= >= && || ++ -- += -= *= /= %= &= |= ^= << >>
{ } ( ) [ ] ; , . < > + - * / % & | ^ ! ~ ? : =""".split(), key=len, reverse=True)
KEYWORDS = {"var", "function", "if", "else", "while", "do", "for", "in", "return", "break", "continue",
            "new", "typeof", "instanceof", "delete", "this", "true", "false", "null", "undefined",
            "switch", "case", "default", "void"}


class Tok:
    def __init__(self, kind, val, line):
        self.kind, self.val, self.line = kind, val, line

    def __repr__(self):
        return f"{self.kind}:{self.val!r}@{self.line}"


def lex(src):
    toks, i, line, n = [], 0, 1, len(src)
    while i < n:
        c = src[i]
        if c == "\n":
            line += 1
            i += 1
            continue
        if c in " \t\r":
            i += 1
            continue
        if src.startswith("//", i):
            while i < n and src[i] != "\n":
                i += 1
            continue
        if src.startswith("/*", i):
            j = src.index("*/", i + 2)
            line += src.count("\n", i, j)
            i = j + 2
            continue
        if c.isdigit() or (c == "." and i + 1 < n and src[i + 1].isdigit()):
            m = re.compile(r"0[xX][0-9a-fA-F]+|(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?").match(src, i)
            t = m.group(0)
            toks.append(Tok("num", int(t, 16) if t[:2] in ("0x", "0X") else float(t), line))
            i = m.end()
            continue
        if c.isalpha() or c in "_$":
            m = re.compile(r"[A-Za-z_$][A-Za-z0-9_$]*").match(src, i)
            w = m.group(0)
            toks.append(Tok("kw" if w in KEYWORDS else "id", w, line))
            i = m.end()
            continue
        if c in "'\"":
            q, j, out = c, i + 1, []
            while src[j] != q:
                if src[j] == "\\":
                    e = src[j + 1]
                    j += 2
                    if e == "n": out.append("\n")
                    elif e == "t": out.append("\t")
                    elif e == "r": out.append("\r")
                    elif e == "x": out.append(chr(int(src[j:j + 2], 16))); j += 2
                    elif e == "u": out.append(chr(int(src[j:j + 4], 16))); j += 4
                    else: out.append(e)
                else:
                    if src[j] == "\n":
                        raise SyntaxError(f"line {line}: unterminated string")
                    out.append(src[j])
                    j += 1
            toks.append(Tok("str", "".join(out), line))
            i = j + 1
            continue
        for p in PUNCT:
            if src.startswith(p, i):
                toks.append(Tok("p", p, line))
                i += len(p)
                break
        else:
            raise SyntaxError(f"line {line}: unexpected {c!r}")
    toks.append(Tok("eof", None, line))
    return toks


# ---------------------------------------------------------------- parser
BINPREC = [("||",), ("&&",), ("|",), ("^",), ("&",), ("==", "!=", "===", "!=="),
           ("<", ">", "<=", ">=", "instanceof", "in"), ("<<", ">>", ">>>"), ("+", "-"), ("*", "/", "%")]
ASSIGN_OPS = {"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", ">>>="}


class Parser:
    def __init__(self, toks):
        self.t, self.i = toks, 0

    def peek(self, k=0):
        return self.t[self.i + k]

    def at(self, val, kind=None):
        tk = self.peek()
        return tk.val == val and (kind is None or tk.kind == kind) and tk.kind in ("p", "kw")

    def eat(self, val):
        if not self.at(val):
            tk = self.peek()
            raise SyntaxError(f"line {tk.line}: expected {val!r}, found {tk.val!r}")
        self.i += 1

    def opt(self, val):
        if self.at(val):
            self.i += 1
            return True
        return False

    def ident(self):
        tk = self.peek()
        if tk.kind != "id":
            raise SyntaxError(f"line {tk.line}: expected a name, found {tk.val!r}")
        self.i += 1
        return tk.val

    def program(self):
        out = []
        while self.peek().kind != "eof":
            out.append(self.stmt())
        return out

    def block(self):
        self.eat("{")
        out = []
        while not self.at("}"):
            out.append(self.stmt())
        self.eat("}")
        return ("block", out)

    def stmt(self):
        tk = self.peek()
        if tk.kind == "p" and tk.val == "{":
            return self.block()
        if tk.kind == "p" and tk.val == ";":
            self.i += 1
            return ("empty",)
        if tk.kind == "kw":
            k = tk.val
            if k == "var":
                self.i += 1
                d = self.vardecl()
                self.opt(";")
                return d
            if k == "function" and self.peek(1).kind == "id":
                self.i += 1
                name = self.ident()
                params, body = self.funcrest()
                return ("fdecl", name, params, body)
            if k == "if":
                self.i += 1
                self.eat("(")
                c = self.expr()
                self.eat(")")
                a = self.stmt()
                b = self.stmt() if self.opt("else") else None
                return ("if", c, a, b)
            if k == "while":
                self.i += 1
                self.eat("(")
                c = self.expr()
                self.eat(")")
                return ("while", c, self.stmt())
            if k == "do":
                self.i += 1
                body = self.stmt()
                self.eat("while")
                self.eat("(")
                c = self.expr()
                self.eat(")")
                self.opt(";")
                return ("do", body, c)
            if k == "for":
                return self.forstmt()
            if k == "return":
                self.i += 1
                e = None if (self.at(";") or self.at("}")) else self.expr()
                self.opt(";")
                return ("ret", e)
            if k in ("break", "continue"):
                self.i += 1
                self.opt(";")
                return (k,)
            if k == "switch":
                return self.switch()
        e = self.expr()
        self.opt(";")
        return ("expr", e)

    def vardecl(self):
        ds = []
        while True:
            name = self.ident()
            if self.opt(":"):
                self.ident()  # AS2 type annotation
            init = self.assign() if self.opt("=") else None
            ds.append((name, init))
            if not self.opt(","):
                break
        return ("var", ds)

    def forstmt(self):
        self.eat("for")
        self.eat("(")
        # for (var k in o) / for (k in o)
        if self.at("var") and self.peek(2).val == "in":
            self.i += 1
            k = self.ident()
            self.eat("in")
            o = self.expr()
            self.eat(")")
            return ("forin", k, True, o, self.stmt())
        if self.peek().kind == "id" and self.peek(1).val == "in":
            k = self.ident()
            self.eat("in")
            o = self.expr()
            self.eat(")")
            return ("forin", k, False, o, self.stmt())
        init = None
        if not self.at(";"):
            init = self.vardecl() if self.opt("var") else ("expr", self.expr())
        self.eat(";")
        c = None if self.at(";") else self.expr()
        self.eat(";")
        upd = None if self.at(")") else self.expr()
        self.eat(")")
        return ("for", init, c, upd, self.stmt())

    def switch(self):
        self.eat("switch")
        self.eat("(")
        e = self.expr()
        self.eat(")")
        self.eat("{")
        cases = []
        while not self.at("}"):
            if self.opt("default"):
                test = None
            else:
                self.eat("case")
                test = self.expr()
            self.eat(":")
            body = []
            while not (self.at("case") or self.at("default") or self.at("}")):
                body.append(self.stmt())
            cases.append((test, body))
        self.eat("}")
        return ("switch", e, cases)

    def funcrest(self):
        self.eat("(")
        params = []
        while not self.at(")"):
            params.append(self.ident())
            if self.opt(":"):
                self.ident()
            if not self.opt(","):
                break
        self.eat(")")
        if self.opt(":"):
            self.ident()
        return params, self.block()[1]

    # expressions
    def expr(self):
        e = self.assign()
        if self.at(","):
            es = [e]
            while self.opt(","):
                es.append(self.assign())
            return ("seq", es)
        return e

    def assign(self):
        left = self.cond()
        tk = self.peek()
        if tk.kind == "p" and tk.val in ASSIGN_OPS:
            if left[0] not in ("id", "mem", "idx"):
                raise SyntaxError(f"line {tk.line}: cannot assign to this")
            self.i += 1
            return ("assign", tk.val, left, self.assign())
        return left

    def cond(self):
        c = self.binary(0)
        if self.opt("?"):
            a = self.assign()
            self.eat(":")
            b = self.assign()
            return ("cond", c, a, b)
        return c

    def binary(self, lvl):
        if lvl == len(BINPREC):
            return self.unary()
        left = self.binary(lvl + 1)
        while True:
            tk = self.peek()
            if tk.kind in ("p", "kw") and tk.val in BINPREC[lvl]:
                self.i += 1
                right = self.binary(lvl + 1)
                left = ("log" if tk.val in ("&&", "||") else "bin", tk.val, left, right)
            else:
                return left

    def unary(self):
        tk = self.peek()
        if tk.kind == "p" and tk.val in ("!", "-", "+", "~"):
            self.i += 1
            e = self.unary()
            if tk.val == "-" and e[0] == "num":
                return ("num", -e[1])
            return ("un", tk.val, e)
        if tk.kind == "p" and tk.val in ("++", "--"):
            self.i += 1
            return ("pre", tk.val, self.unary())
        if tk.kind == "kw" and tk.val in ("typeof", "delete", "void"):
            self.i += 1
            return (tk.val, self.unary())
        e = self.postfix()
        tk = self.peek()
        if tk.kind == "p" and tk.val in ("++", "--"):
            self.i += 1
            return ("post", tk.val, e)
        return e

    def postfix(self):
        if self.at("new"):
            self.i += 1
            callee = self.primary()
            while True:
                if self.opt("."):
                    callee = ("mem", callee, self.name_after_dot())
                elif self.at("["):
                    self.i += 1
                    k = self.expr()
                    self.eat("]")
                    callee = ("idx", callee, k)
                else:
                    break
            args = self.args() if self.at("(") else []
            e = ("new", callee, args)
        else:
            e = self.primary()
        while True:
            if self.opt("."):
                e = ("mem", e, self.name_after_dot())
            elif self.at("["):
                self.i += 1
                k = self.expr()
                self.eat("]")
                e = ("idx", e, k)
            elif self.at("("):
                e = ("call", e, self.args())
            else:
                return e

    def name_after_dot(self):
        tk = self.peek()
        if tk.kind in ("id", "kw"):
            self.i += 1
            return tk.val
        raise SyntaxError(f"line {tk.line}: expected a name after '.'")

    def args(self):
        self.eat("(")
        out = []
        while not self.at(")"):
            out.append(self.assign())
            if not self.opt(","):
                break
        self.eat(")")
        return out

    def primary(self):
        tk = self.peek()
        self.i += 1
        if tk.kind == "num":
            return ("num", tk.val)
        if tk.kind == "str":
            return ("str", tk.val)
        if tk.kind == "id":
            return ("id", tk.val)
        if tk.kind == "kw":
            if tk.val in ("this", "true", "false", "null", "undefined"):
                return (tk.val,)
            if tk.val == "function":
                name = self.ident() if self.peek().kind == "id" else ""
                params, body = self.funcrest()
                return ("func", name, params, body)
        if tk.kind == "p":
            if tk.val == "(":
                e = self.expr()
                self.eat(")")
                return e
            if tk.val == "[":
                items = []
                while not self.at("]"):
                    items.append(self.assign())
                    if not self.opt(","):
                        break
                self.eat("]")
                return ("arr", items)
            if tk.val == "{":
                props = []
                while not self.at("}"):
                    k = self.peek()
                    self.i += 1
                    key = k.val if k.kind in ("id", "str", "kw") else str(int(k.val)) if k.kind == "num" else None
                    if key is None:
                        raise SyntaxError(f"line {k.line}: bad object key")
                    self.eat(":")
                    props.append((key, self.assign()))
                    if not self.opt(","):
                        break
                self.eat("}")
                return ("obj", props)
        raise SyntaxError(f"line {tk.line}: unexpected {tk.val!r}")


# ---------------------------------------------------------------- code generation
BIN = {"+": [0x47], "-": [0x0B], "*": [0x0C], "/": [0x0D], "%": [0x3F], "&": [0x60], "|": [0x61], "^": [0x62],
       "<<": [0x63], ">>": [0x64], ">>>": [0x65], "==": [0x49], "!=": [0x49, 0x12], "===": [0x66],
       "!==": [0x66, 0x12], "<": [0x48], ">": [0x67], "<=": [0x67, 0x12], ">=": [0x48, 0x12],
       "instanceof": [0x54]}


def latin1(s):
    try:
        return s.encode("latin-1")
    except UnicodeEncodeError:
        return s.encode("latin-1", "replace")


class Code:
    def __init__(self):
        self.b = bytearray()
        self.fix = []
        self.labels = {}
        self.n = 0

    def op(self, code, payload=b""):
        if code >= 0x80:
            self.b += bytes([code]) + struct.pack("<H", len(payload)) + payload
        else:
            self.b.append(code)

    def ops(self, codes):
        for c in codes:
            self.op(c)

    def push(self, *vals):
        p = b""
        for v in vals:
            if v is None:
                p += b"\x03"
            elif v is True or v is False:
                p += b"\x05" + bytes([1 if v else 0])
            elif isinstance(v, str):
                p += b"\x00" + latin1(v) + b"\x00"
            elif isinstance(v, tuple) and v[0] == "null":
                p += b"\x02"
            elif isinstance(v, tuple) and v[0] == "reg":
                p += b"\x04" + bytes([v[1]])
            else:
                f = float(v)
                if f == int(f) and -2 ** 31 <= f < 2 ** 31 and not (f == 0 and str(f).startswith("-")):
                    p += b"\x07" + struct.pack("<i", int(f))
                else:
                    d = struct.pack("<d", f)
                    p += b"\x06" + d[4:] + d[:4]
        self.op(0x96, p)

    def new_label(self):
        self.n += 1
        return self.n

    def place(self, lab):
        self.labels[lab] = len(self.b)

    def jump(self, lab, cond=False):
        self.b += bytes([0x9D if cond else 0x99]) + struct.pack("<H", 2)
        self.fix.append((len(self.b), lab))
        self.b += b"\x00\x00"

    def finish(self):
        for pos, lab in self.fix:
            off = self.labels[lab] - (pos + 2)
            self.b[pos:pos + 2] = struct.pack("<h", off)
        return bytes(self.b)


class Gen:
    def __init__(self):
        self.loops = []  # (break_label, continue_label)

    def compile(self, stmts, in_func=False):
        code = Code()
        saved = self.loops
        self.loops = []
        self.in_func = in_func
        prev_code, self.c = getattr(self, "c", None), code
        # hoist function declarations
        for s in stmts:
            if s[0] == "fdecl":
                self.function(s[1], s[2], s[3])
        for s in stmts:
            if s[0] != "fdecl":
                self.stmt(s)
        self.c = prev_code
        self.loops = saved
        return code.finish()

    def function(self, name, params, body):
        saved_in = getattr(self, "in_func", False)
        inner = self.compile(body, True)
        self.in_func = saved_in
        payload = latin1(name) + b"\x00" + struct.pack("<H", len(params))
        for p in params:
            payload += latin1(p) + b"\x00"
        payload += struct.pack("<H", len(inner))
        self.c.op(0x9B, payload)
        self.c.b += inner

    # ---- statements
    def stmt(self, s):
        c = self.c
        k = s[0]
        if k == "empty":
            return
        if k == "block":
            for x in s[1]:
                if x[0] == "fdecl":
                    self.function(x[1], x[2], x[3])
            for x in s[1]:
                if x[0] != "fdecl":
                    self.stmt(x)
        elif k == "fdecl":
            self.function(s[1], s[2], s[3])
        elif k == "var":
            for name, init in s[1]:
                if init is None:
                    if self.in_func:
                        c.push(name)
                        c.op(0x41)
                    continue
                c.push(name)
                self.ex(init)
                c.op(0x3C if self.in_func else 0x1D)
        elif k == "expr":
            self.ex_void(s[1])
        elif k == "if":
            els, end = c.new_label(), c.new_label()
            self.ex(s[1])
            c.op(0x12)
            c.jump(els, True)
            self.stmt(s[2])
            if s[3] is not None:
                c.jump(end)
            c.place(els)
            if s[3] is not None:
                self.stmt(s[3])
                c.place(end)
        elif k == "while":
            top, end = c.new_label(), c.new_label()
            c.place(top)
            self.ex(s[1])
            c.op(0x12)
            c.jump(end, True)
            self.loops.append((end, top))
            self.stmt(s[2])
            self.loops.pop()
            c.jump(top)
            c.place(end)
        elif k == "do":
            top, cont, end = c.new_label(), c.new_label(), c.new_label()
            c.place(top)
            self.loops.append((end, cont))
            self.stmt(s[1])
            self.loops.pop()
            c.place(cont)
            self.ex(s[2])
            c.jump(top, True)
            c.place(end)
        elif k == "for":
            _, init, cond, upd, body = s
            if init:
                self.stmt(init)
            top, cont, end = c.new_label(), c.new_label(), c.new_label()
            c.place(top)
            if cond:
                self.ex(cond)
                c.op(0x12)
                c.jump(end, True)
            self.loops.append((end, cont))
            self.stmt(body)
            self.loops.pop()
            c.place(cont)
            if upd:
                self.ex_void(upd)
            c.jump(top)
            c.place(end)
        elif k == "forin":
            _, var, isvar, obj, body = s
            top, end, done = c.new_label(), c.new_label(), c.new_label()
            self.ex(obj)
            c.op(0x55)  # Enumerate2: pushes null, then the names
            c.place(top)
            c.op(0x87, b"\x00")  # StoreRegister 0
            c.push(("null",))
            c.op(0x49)
            c.jump(done, True)
            c.push(var, ("reg", 0))
            c.op(0x3C if (isvar and self.in_func) else 0x1D)
            # break inside for-in must drain the remaining names
            self.loops.append((end, top))
            self.stmt(body)
            self.loops.pop()
            c.jump(top)
            c.place(end)  # break: pop names until null
            lp = c.new_label()
            c.place(lp)
            c.push(("null",))
            c.op(0x49)
            c.op(0x12)
            c.jump(lp, True)
            c.place(done)
        elif k == "switch":
            _, e, cases = s
            end = c.new_label()
            tmp = "__sw%d" % c.new_label()
            c.push(tmp)
            self.ex(e)
            c.op(0x3C if self.in_func else 0x1D)
            labels = [c.new_label() for _ in cases]
            default = None
            for i, (test, body) in enumerate(cases):
                if test is None:
                    default = labels[i]
                    continue
                c.push(tmp)
                c.op(0x1C)
                self.ex(test)
                c.op(0x66)
                c.jump(labels[i], True)
            c.jump(default if default else end)
            self.loops.append((end, self.loops[-1][1] if self.loops else end))
            for i, (test, body) in enumerate(cases):
                c.place(labels[i])
                for x in body:
                    self.stmt(x)
            self.loops.pop()
            c.place(end)
        elif k == "ret":
            if s[1] is None:
                c.push(None)
            else:
                self.ex(s[1])
            c.op(0x3E)
        elif k == "break":
            c.jump(self.loops[-1][0])
        elif k == "continue":
            c.jump(self.loops[-1][1])
        else:
            raise SyntaxError("unknown statement " + k)

    # ---- expressions
    def ex_void(self, e):
        k = e[0]
        if k == "assign":
            self.assign(e[1], e[2], e[3], False)
        elif k in ("pre", "post"):
            self.incdec(e[1], e[2], False, k == "post")
        elif k == "seq":
            for x in e[1]:
                self.ex_void(x)
        else:
            self.ex(e)
            self.c.op(0x17)

    def key(self, e):
        """push the member name for mem/idx targets"""
        if e[0] == "mem":
            self.c.push(e[2])
        else:
            self.ex(e[2])

    def assign(self, op, target, value, want):
        c = self.c

        def val():
            if op == "=":
                self.ex(value)
            else:
                self.get(target)
                self.ex(value)
                c.ops(BIN[op[:-1]])

        if target[0] == "id":
            if want:
                val()
                c.op(0x4C)
                c.push(target[1])
                c.op(0x4D)
                c.op(0x1D)
            else:
                c.push(target[1])
                val()
                c.op(0x1D)
        else:
            self.ex(target[1])
            self.key(target)
            val()
            if want:
                c.op(0x87, b"\x00")
                c.op(0x4F)
                c.push(("reg", 0))
            else:
                c.op(0x4F)

    def get(self, target):
        if target[0] == "id":
            self.ex(target)
        else:
            self.ex(target[1])
            self.key(target)
            self.c.op(0x4E)

    def incdec(self, op, target, want, post):
        c = self.c
        code = 0x50 if op == "++" else 0x51
        if target[0] not in ("id", "mem", "idx"):
            raise SyntaxError("cannot increment this")
        if want and post:
            self.get(target)
            c.op(0x4A)  # value before
        if target[0] == "id":
            if want and not post:
                self.get(target)
                c.op(code)
                c.op(0x4C)
                c.push(target[1])
                c.op(0x4D)
                c.op(0x1D)
            else:
                c.push(target[1])
                self.get(target)
                c.op(code)
                c.op(0x1D)
        else:
            self.ex(target[1])
            self.key(target)
            self.get(target)
            c.op(code)
            if want and not post:
                c.op(0x87, b"\x00")
                c.op(0x4F)
                c.push(("reg", 0))
            else:
                c.op(0x4F)

    def ex(self, e):
        c = self.c
        k = e[0]
        if k == "num":
            c.push(e[1])
        elif k == "str":
            c.push(e[1])
        elif k == "true":
            c.push(True)
        elif k == "false":
            c.push(False)
        elif k == "null":
            c.push(("null",))
        elif k == "undefined":
            c.push(None)
        elif k == "this":
            c.push("this")
            c.op(0x1C)
        elif k == "id":
            c.push(e[1])
            c.op(0x1C)
        elif k in ("mem", "idx"):
            self.get(e)
        elif k == "arr":
            for x in reversed(e[1]):
                self.ex(x)
            c.push(len(e[1]))
            c.op(0x42)
        elif k == "obj":
            for key, v in e[1]:
                c.push(key)
                self.ex(v)
            c.push(len(e[1]))
            c.op(0x43)
        elif k == "func":
            self.function(e[1] if False else "", e[2], e[3])
        elif k == "call":
            callee, args = e[1], e[2]
            for a in reversed(args):
                self.ex(a)
            c.push(len(args))
            if callee[0] == "id":
                c.push(callee[1])
                c.op(0x3D)
            elif callee[0] in ("mem", "idx"):
                self.ex(callee[1])
                self.key(callee)
                c.op(0x52)
            else:
                self.ex(callee)
                c.push(None)
                c.op(0x52)
        elif k == "new":
            callee, args = e[1], e[2]
            for a in reversed(args):
                self.ex(a)
            c.push(len(args))
            if callee[0] == "id":
                c.push(callee[1])
                c.op(0x40)
            elif callee[0] in ("mem", "idx"):
                self.ex(callee[1])
                self.key(callee)
                c.op(0x53)
            else:
                self.ex(callee)
                c.push(None)
                c.op(0x53)
        elif k == "un":
            op = e[1]
            if op == "!":
                self.ex(e[2])
                c.op(0x12)
            elif op == "-":
                c.push(0)
                self.ex(e[2])
                c.op(0x0B)
            elif op == "+":
                self.ex(e[2])
                c.op(0x4A)
            elif op == "~":
                self.ex(e[2])
                c.push(-1)
                c.op(0x62)
        elif k == "typeof":
            self.ex(e[1])
            c.op(0x44)
        elif k == "void":
            self.ex(e[1])
            c.op(0x17)
            c.push(None)
        elif k == "delete":
            t = e[1]
            if t[0] in ("mem", "idx"):
                self.ex(t[1])
                self.key(t)
                c.op(0x3A)
            else:
                c.push(t[1] if t[0] == "id" else "")
                c.op(0x3B)
        elif k == "bin":
            if e[1] == "in":
                raise SyntaxError("'in' operator is not supported")
            self.ex(e[2])
            self.ex(e[3])
            c.ops(BIN[e[1]])
        elif k == "log":
            end = c.new_label()
            self.ex(e[2])
            c.op(0x4C)
            if e[1] == "&&":
                c.op(0x12)
            c.jump(end, True)
            c.op(0x17)
            self.ex(e[3])
            c.place(end)
        elif k == "cond":
            els, end = c.new_label(), c.new_label()
            self.ex(e[1])
            c.op(0x12)
            c.jump(els, True)
            self.ex(e[2])
            c.jump(end)
            c.place(els)
            self.ex(e[3])
            c.place(end)
        elif k == "assign":
            self.assign(e[1], e[2], e[3], True)
        elif k in ("pre", "post"):
            self.incdec(e[1], e[2], True, k == "post")
        elif k == "seq":
            for x in e[1][:-1]:
                self.ex_void(x)
            self.ex(e[1][-1])
        else:
            raise SyntaxError("unknown expression " + k)


def compile_as(src):
    return Gen().compile(Parser(lex(src)).program()) + b"\x00"


# ---------------------------------------------------------------- fonts
def font_tag(font_id, ttf_path, name, chars):
    from fontTools.ttLib import TTFont
    from fontTools.pens.basePen import decomposeQuadraticSegment
    from fontTools.pens.recordingPen import RecordingPen

    f = TTFont(ttf_path)
    upm = f["head"].unitsPerEm
    sc = 20480.0 / upm
    cmap = f.getBestCmap()
    gs = f.getGlyphSet()
    hmtx = f["hmtx"]
    hhea = f["hhea"]
    codes, shapes, advs, bounds = [], [], [], []
    for ch in chars:
        gname = cmap.get(ch)
        if gname is None:
            continue
        pen = RecordingPen()
        gs[gname].draw(pen)
        contours, cur, start = [], None, None
        for op, args in pen.value:
            if op == "moveTo":
                start = args[0]
                cur = [("M", start)]
                contours.append(cur)
            elif op == "lineTo":
                cur.append(("L", args[0]))
            elif op == "qCurveTo":
                if args[-1] is None:  # closed contour of off-curve points only
                    args = args[:-1]
                    if cur is None:
                        cur = [("M", args[-1])]
                        contours.append(cur)
                for ctrl, pt in decomposeQuadraticSegment(args):
                    cur.append(("Q", ctrl, pt))
            elif op == "curveTo":  # cubic (CFF): approximate with two quadratics
                p0 = cur[-1][-1]
                c1, c2, p3 = args
                mid = ((c1[0] + c2[0]) / 2, (c1[1] + c2[1]) / 2)
                q1 = ((p0[0] + 3 * c1[0]) / 4, (p0[1] + 3 * c1[1]) / 4)
                q2 = ((3 * c2[0] + p3[0]) / 4, (3 * c2[1] + p3[1]) / 4)
                cur.append(("Q", q1, mid))
                cur.append(("Q", q2, p3))
            elif op in ("closePath", "endPath"):
                if cur and cur[-1][-1] != start:
                    cur.append(("L", start))
        b = Bits()
        b.u(1, 4)
        b.u(0, 4)
        first = True
        x0 = y0 = x1 = y1 = 0
        allp = []
        for cont in contours:
            px = py = None
            for seg in cont:
                if seg[0] == "M":
                    x, y = round(seg[1][0] * sc), round(-seg[1][1] * sc)
                    b.u(0, 1)
                    b.u(5 if first else 1, 5)
                    n = sbits(x, y)
                    b.u(n, 5); b.s(x, n); b.s(y, n)
                    if first:
                        b.u(1, 1)  # fill style 1 (NumFillBits = 1)
                    first = False
                    px, py = x, y
                    allp.append((x, y))
                elif seg[0] == "L":
                    x, y = round(seg[1][0] * sc), round(-seg[1][1] * sc)
                    dx, dy = x - px, y - py
                    if dx == 0 and dy == 0:
                        continue
                    n = max(sbits(dx, dy), 2)
                    b.u(1, 1); b.u(1, 1); b.u(n - 2, 4); b.u(1, 1); b.s(dx, n); b.s(dy, n)
                    px, py = x, y
                    allp.append((x, y))
                else:
                    cx, cy = round(seg[1][0] * sc), round(-seg[1][1] * sc)
                    ax, ay = round(seg[2][0] * sc), round(-seg[2][1] * sc)
                    v = (cx - px, cy - py, ax - cx, ay - cy)
                    n = max(sbits(*v), 2)
                    b.u(1, 1); b.u(0, 1); b.u(n - 2, 4)
                    for q in v:
                        b.s(q, n)
                    px, py = ax, ay
                    allp.append((x, y) if False else (ax, ay))
        b.u(0, 6)
        shapes.append(b.bytes())
        codes.append(ch)
        advs.append(round(hmtx[gname][0] * sc))
        if allp:
            xs, ys = [p[0] for p in allp], [p[1] for p in allp]
            bounds.append(rect(min(xs), max(xs), min(ys), max(ys)))
        else:
            bounds.append(rect(0, 0, 0, 0))
    n = len(codes)
    offs, pos = [], 4 * (n + 1)
    for s in shapes:
        offs.append(pos)
        pos += len(s)
    body = struct.pack("<H", font_id)
    body += bytes([0x80 | 0x08 | 0x04, 1])  # HasLayout, WideOffsets, WideCodes; language Latin
    nb = name.encode("latin-1")
    body += bytes([len(nb)]) + nb + struct.pack("<H", n)
    body += b"".join(struct.pack("<I", o) for o in offs) + struct.pack("<I", pos)
    body += b"".join(shapes)
    body += b"".join(struct.pack("<H", ch) for ch in codes)
    body += struct.pack("<HHh", round(hhea.ascent * sc), round(-hhea.descent * sc), round(hhea.lineGap * sc))
    body += b"".join(struct.pack("<h", a) for a in advs)
    body += b"".join(bounds)
    body += struct.pack("<H", 0)
    return tag(75, body)


# ---------------------------------------------------------------- SWF
def build_swf(code, w, h, fps, bg, fonts=(), title=None):
    tags = tag(69, struct.pack("<I", 0x08))  # FileAttributes: AS1/2
    tags += tag(9, bytes(bg))
    for i, (path, name) in enumerate(fonts):
        chars = list(range(32, 127)) + list(range(160, 256))
        tags += font_tag(1 + i, path, name, chars)
    if title:
        tags += tag(77, ("<rdf:RDF><dc:title>%s</dc:title></rdf:RDF>" % title).encode() + b"\x00")
    tags += tag(12, code)
    tags += tag(1, b"")
    tags += tag(0, b"")
    body = rect(0, w * 20, 0, h * 20) + struct.pack("<HH", int(fps * 256), 1) + tags
    return b"CWS" + bytes([8]) + struct.pack("<I", 8 + len(body)) + zlib.compress(body, 9)


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        print(__doc__)
        sys.exit(1)
    src, out = args[0], args[1]
    w, h, fps, bg, fonts, title = 1024, 768, 30.0, (0, 0, 0), [], None
    i = 2
    while i < len(args):
        a = args[i]
        if a == "--size":
            w, h = map(int, args[i + 1].split("x")); i += 2
        elif a == "--fps":
            fps = float(args[i + 1]); i += 2
        elif a == "--bg":
            v = int(args[i + 1], 16); bg = ((v >> 16) & 255, (v >> 8) & 255, v & 255); i += 2
        elif a == "--font":
            fonts.append((args[i + 1], args[i + 2])); i += 3
        elif a == "--title":
            title = args[i + 1]; i += 2
        else:
            raise SystemExit("unknown option " + a)
    with open(src, encoding="utf-8") as f:
        text = f.read()
    try:
        code = compile_as(text)
    except SyntaxError as e:
        raise SystemExit(f"{src}: {e}")
    data = build_swf(code, w, h, fps, bg, fonts, title)
    with open(out, "wb") as f:
        f.write(data)
    print(f"{out}: {len(data)} bytes ({len(code)} bytes of ActionScript)")


if __name__ == "__main__":
    main()
