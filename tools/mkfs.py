#!/usr/bin/env python3
"""Build a FlashFS archive.  Copyright (c) 2026 FlashOS. All rights reserved.

Usage: mkfs.py OUT.bin FILE [FILE ...]
Layout: "FLFS" | u32 count | count x {name[56], u32 offset, u32 size} | data (4-byte aligned)
"""
import os
import struct
import sys


def main():
    out, files = sys.argv[1], sorted(sys.argv[2:], key=lambda p: os.path.basename(p).lower())
    header = 8 + 64 * len(files)
    entries, blobs, off = [], [], header
    for path in files:
        name = os.path.basename(path).encode()[:55]
        data = open(path, "rb").read()
        entries.append(struct.pack("<56sII", name, off, len(data)))
        blobs.append(data + b"\0" * (-len(data) % 4))
        off += len(blobs[-1])
    with open(out, "wb") as f:
        f.write(b"FLFS" + struct.pack("<I", len(files)) + b"".join(entries) + b"".join(blobs))
    print(f"FlashFS: {len(files)} files, {off} bytes -> {out}")


if __name__ == "__main__":
    main()
