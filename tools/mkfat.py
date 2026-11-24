#!/usr/bin/env python3
"""Make a FAT16/FAT32 disk image with files - for testing FlashOS USB loading.
Copyright (c) 2026 FlashOS. All rights reserved.

Usage: mkfat.py OUT.img {16|32} {mbr|raw} SIZE_MB FILE[:DIR/NAME] ...
"""
import struct
import sys


def lfn_entries(name, short):
    csum = 0
    for b in short:
        csum = (((csum & 1) << 7) + (csum >> 1) + b) & 0xFF
    chars = [ord(c) for c in name] + [0]
    while len(chars) % 13:
        chars.append(0xFFFF)
    parts = [chars[i:i + 13] for i in range(0, len(chars), 13)]
    out = []
    for i, p in enumerate(parts):
        seq = i + 1 | (0x40 if i == len(parts) - 1 else 0)
        e = bytearray(32)
        e[0] = seq
        e[11] = 0x0F
        e[13] = csum
        pos = [1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30]
        for k, ch in enumerate(p):
            struct.pack_into("<H", e, pos[k], ch)
        out.append(bytes(e))
    return list(reversed(out))


class Fat:
    def __init__(self, fat_bits, size_mb):
        self.bits = fat_bits
        self.total = size_mb * 2048
        self.spc = 1 if fat_bits == 16 else 1
        self.rsv = 1 if fat_bits == 16 else 32
        self.root_ents = 512 if fat_bits == 16 else 0
        ent = 2 if fat_bits == 16 else 4
        self.fatsz = (self.total * ent // 512 // self.spc) + 2
        self.root_secs = self.root_ents * 32 // 512
        self.data = self.rsv + 2 * self.fatsz + self.root_secs
        self.nclus = (self.total - self.data) // self.spc
        self.fat = [0] * (self.nclus + 2)
        self.fat[0] = 0xFFF8 if fat_bits == 16 else 0x0FFFFFF8
        self.fat[1] = 0xFFFF if fat_bits == 16 else 0x0FFFFFFF
        self.next = 2
        self.img = bytearray(self.total * 512)
        self.root = []
        if fat_bits == 32:
            self.root_clus = self.alloc(1)[0]

    def eoc(self):
        return 0xFFFF if self.bits == 16 else 0x0FFFFFFF

    def alloc(self, n):
        cl = list(range(self.next, self.next + n))
        self.next += n
        for a, b in zip(cl, cl[1:]):
            self.fat[a] = b
        self.fat[cl[-1]] = self.eoc()
        return cl

    def write_clusters(self, data):
        n = max(1, (len(data) + 511) // 512)
        cl = self.alloc(n)
        for i, c in enumerate(cl):
            off = (self.data + (c - 2) * self.spc) * 512
            chunk = data[i * 512:(i + 1) * 512]
            self.img[off:off + len(chunk)] = chunk
        return cl[0]

    def entry(self, name, short, attr, clus, size):
        e = bytearray(32)
        e[0:11] = short
        e[11] = attr
        struct.pack_into("<H", e, 20, clus >> 16)
        struct.pack_into("<H", e, 26, clus & 0xFFFF)
        struct.pack_into("<I", e, 28, size)
        return b"".join(lfn_entries(name, short)) + bytes(e)

    def build(self, files):
        dirs = {}
        counter = [0]

        def short_name(name, is_dir=False):
            counter[0] += 1
            base = "F%06d" % counter[0]
            ext = name.rsplit(".", 1)[1].upper()[:3] if "." in name and not is_dir else ""
            return (base.ljust(8) + ext.ljust(3)).encode()

        for path, data in files:
            if "/" in path:
                d, n = path.split("/", 1)
                dirs.setdefault(d, []).append((n, data))
            else:
                self.root.append((path, data))
        root_bytes = b""
        for d, items in dirs.items():
            body = b""
            for n, data in items:
                c = self.write_clusters(data)
                body += self.entry(n, short_name(n), 0x20, c, len(data))
            c = self.write_clusters(body + b"\0" * 32)
            root_bytes += self.entry(d, short_name(d, True), 0x10, c, 0)
        for n, data in self.root:
            c = self.write_clusters(data)
            attr = 0x22 if n.startswith("._") and False else 0x20
            root_bytes += self.entry(n, short_name(n), attr, c, len(data))
        root_bytes += self.entry("FLASHGAMES", b"FLASHGAMES ", 0x08, 0, 0)[-32:]
        if self.bits == 16:
            off = (self.rsv + 2 * self.fatsz) * 512
            self.img[off:off + len(root_bytes)] = root_bytes
        else:
            n = (len(root_bytes) + 32 + 511) // 512
            extra = self.alloc(n - 1) if n > 1 else []
            chain = [self.root_clus] + extra
            if extra:
                self.fat[self.root_clus] = extra[0]
            for i, c in enumerate(chain):
                off = (self.data + (c - 2) * self.spc) * 512
                chunk = root_bytes[i * 512:(i + 1) * 512]
                self.img[off:off + len(chunk)] = chunk
        # FATs
        fmt = "<H" if self.bits == 16 else "<I"
        fb = b"".join(struct.pack(fmt, v) for v in self.fat)
        for k in range(2):
            off = (self.rsv + k * self.fatsz) * 512
            self.img[off:off + len(fb)] = fb
        # boot sector
        bs = bytearray(512)
        bs[0:3] = b"\xEB\x58\x90"
        bs[3:11] = b"MSDOS5.0"
        struct.pack_into("<HBHBHHBH", bs, 11, 512, self.spc, self.rsv, 2, self.root_ents,
                         self.total if self.total < 65536 else 0, 0xF8, self.fatsz if self.bits == 16 else 0)
        struct.pack_into("<I", bs, 32, self.total if self.total >= 65536 else 0)
        if self.bits == 32:
            struct.pack_into("<IHHI", bs, 36, self.fatsz, 0, 0, self.root_clus)
        bs[510:512] = b"\x55\xAA"
        self.img[0:512] = bs
        return bytes(self.img)


def main():
    out, bits, layout, size = sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4])
    files = []
    for spec in sys.argv[5:]:
        src, _, dst = spec.partition(":")
        files.append((dst or src.split("/")[-1], open(src, "rb").read()))
    vol = Fat(bits, size).build(files)
    if layout == "mbr":
        start = 2048
        mbr = bytearray(start * 512)
        ptype = 0x0E if bits == 16 else 0x0C
        mbr[446:462] = struct.pack("<B3sB3sII", 0, b"\0\0\0", ptype, b"\0\0\0", start, len(vol) // 512)
        mbr[510:512] = b"\x55\xAA"
        vol = bytes(mbr) + vol
    open(out, "wb").write(vol)
    print(f"{out}: FAT{bits} {layout}, {len(files)} files")


if __name__ == "__main__":
    main()
