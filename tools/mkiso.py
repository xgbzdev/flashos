#!/usr/bin/env python3
"""Build the FlashOS bootable image.  Copyright (c) 2026 FlashOS. All rights reserved.

Writes an ISO 9660 image with an El Torito "no emulation" boot entry (CDs,
VMs) that is also a valid MBR disk (dd it to a USB stick):

  bytes 0..511       MBR (boot/mbr.asm) + partition table
  bytes 512..2559    stage loader, patched for 512-byte sectors (USB/HDD)
  sector 16..18      primary volume descriptor, boot record, terminator
  sector 19..22      path tables, root directory, boot catalog
  then               LOADER.BIN (2048-byte-sector copy), FLASHKRN.BIN, FLASHFS.BIN

Usage: mkiso.py MBR LOADER KERNEL FLASHFS STAGE2 OUT.iso
"""
import struct
import sys
import time

SECTOR = 2048


def both16(v):
    return struct.pack("<H", v) + struct.pack(">H", v)


def both32(v):
    return struct.pack("<I", v) + struct.pack(">I", v)


def pad(b, n=SECTOR):
    return b + b"\0" * (-len(b) % n)


def sectors(n, size=SECTOR):
    return (n + size - 1) // size


def dir_record(lba, size, flags, name):
    t = time.gmtime()
    date = bytes([t.tm_year - 1900, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, 0])
    body = (bytes([0]) + both32(lba) + both32(size) + date + bytes([flags, 0, 0]) +
            both16(1) + bytes([len(name)]) + name)
    rec = bytes([33 + len(name)]) + body
    return rec + (b"\0" if len(rec) % 2 else b"")


def patch_loader(loader, sector_size, k_lba, k_cnt, fs_lba, fs_cnt, fs_bytes, s2_lba, s2_cnt):
    b = bytearray(loader)
    struct.pack_into("<HIIIIIIII", b, 8, sector_size, k_lba, k_cnt, fs_lba, fs_cnt, fs_bytes, s2_lba, s2_cnt, 0)
    return bytes(b)


def main():
    args = sys.argv[1:]
    label = b"FLASHOS"
    extra = []   # (ISO name, data): extra files visible on the CD
    while "--label" in args:
        i = args.index("--label"); label = args[i + 1].encode(); del args[i:i + 2]
    while "--file" in args:
        i = args.index("--file")
        name, path = args[i + 1].split("=", 1)
        extra.append((name.upper().encode() + b";1", open(path, "rb").read()))
        del args[i:i + 2]
    mbr, loader, kernel, fs, stage2 = (open(p, "rb").read() for p in args[0:5])
    out = args[5]
    assert len(mbr) == 512 and len(loader) == 2048

    LBA_PVD, LBA_BOOT, LBA_TERM = 16, 17, 18
    LBA_LPATH, LBA_MPATH, LBA_ROOT, LBA_CAT, LBA_LOADER = 19, 20, 21, 22, 23
    lba_kernel = LBA_LOADER + 1
    lba_s2 = lba_kernel + sectors(len(kernel))
    lba_fs = lba_s2 + sectors(len(stage2))
    total = lba_fs + sectors(len(fs))
    extra_lba = []
    for name, data in extra:
        extra_lba.append(total)
        total += sectors(len(data))

    files = [  # ISO 9660 names, sorted
        (b"BOOT.CAT;1", LBA_CAT, SECTOR),
        (b"FLASHFS.BIN;1", lba_fs, len(fs)),
        (b"FLASHKRN.BIN;1", lba_kernel, len(kernel)),
        (b"LOADER.BIN;1", LBA_LOADER, SECTOR),
        (b"STAGE2.BIN;1", lba_s2, len(stage2)),
    ] + [(name, extra_lba[i], len(data)) for i, (name, data) in enumerate(extra)]
    files.sort()
    root = dir_record(LBA_ROOT, SECTOR, 2, b"\0") + dir_record(LBA_ROOT, SECTOR, 2, b"\1")
    for name, lba, size in files:
        root += dir_record(lba, size, 0, name)
    assert len(root) <= SECTOR

    ptable_l = bytes([1, 0]) + struct.pack("<I", LBA_ROOT) + struct.pack("<H", 1) + b"\0\0"
    ptable_m = bytes([1, 0]) + struct.pack(">I", LBA_ROOT) + struct.pack(">H", 1) + b"\0\0"

    now = time.strftime("%Y%m%d%H%M%S00").encode() + b"\0"
    blank_date = b"0" * 16 + b"\0"
    pvd = bytearray(SECTOR)
    pvd[0:8] = b"\x01CD001\x01\x00"
    pvd[8:40] = b"FLASHOS".ljust(32)
    pvd[40:72] = label.ljust(32)
    pvd[80:88] = both32(total)
    pvd[120:124] = both16(1)
    pvd[124:128] = both16(1)
    pvd[128:132] = both16(SECTOR)
    pvd[132:140] = both32(len(ptable_l))
    pvd[140:144] = struct.pack("<I", LBA_LPATH)
    pvd[148:152] = struct.pack(">I", LBA_MPATH)
    pvd[156:190] = dir_record(LBA_ROOT, SECTOR, 2, b"\0")
    pvd[190:318] = b"FLASHOS".ljust(128)
    pvd[318:446] = b"FLASHOS".ljust(128)
    pvd[446:574] = b"FLASHOS".ljust(128)
    pvd[574:702] = b"FLASHOS BUILD TOOLS".ljust(128)
    pvd[702:739] = b"".ljust(37)
    pvd[739:776] = b"".ljust(37)
    pvd[776:813] = b"".ljust(37)
    pvd[813:830] = now
    pvd[830:847] = now
    pvd[847:864] = blank_date
    pvd[864:881] = blank_date
    pvd[881] = 1

    boot = bytearray(SECTOR)
    boot[0:7] = b"\x00CD001\x01"
    boot[7:39] = b"EL TORITO SPECIFICATION".ljust(32, b"\0")
    boot[71:75] = struct.pack("<I", LBA_CAT)

    term = bytearray(SECTOR)
    term[0:7] = b"\xffCD001\x01"

    # boot catalog: validation entry + default entry (no emulation, 4 x 512 bytes)
    val = bytearray(32)
    val[0] = 1
    val[4:28] = b"FLASHOS".ljust(24, b"\0")
    val[30], val[31] = 0x55, 0xAA
    csum = (-sum(struct.unpack("<16H", bytes(val)))) & 0xFFFF
    struct.pack_into("<H", val, 28, csum)
    assert sum(struct.unpack("<16H", bytes(val))) & 0xFFFF == 0
    entry = struct.pack("<BBHBBHI", 0x88, 0, 0, 0, 0, 4, LBA_LOADER) + b"\0" * 20
    catalog = bytes(val) + entry

    loader_cd = patch_loader(loader, 2048, lba_kernel, sectors(len(kernel)),
                             lba_fs, sectors(len(fs)), len(fs), lba_s2, sectors(len(stage2)))
    loader_hd = patch_loader(loader, 512, lba_kernel * 4, sectors(len(kernel), 512),
                             lba_fs * 4, sectors(len(fs), 512), len(fs), lba_s2 * 4, sectors(len(stage2), 512))

    # MBR + partition table covering the whole image
    total512 = total * 4
    m = bytearray(mbr)
    struct.pack_into("<I", m, 440, int(time.time()) & 0xFFFFFFFF)
    part = struct.pack("<B3sB3sII", 0x80, b"\x00\x02\x00", 0x17, b"\xfe\xff\xff", 0, total512)
    m[446:462] = part

    system_area = bytearray(16 * SECTOR)
    system_area[0:512] = m
    system_area[512:2560] = loader_hd

    img = (bytes(system_area) + bytes(pvd) + bytes(boot) + bytes(term) +
           pad(ptable_l) + pad(ptable_m) + pad(root) + pad(catalog) + loader_cd +
           pad(kernel) + pad(stage2) + pad(fs) + b"".join(pad(d) for _, d in extra))
    assert len(img) == total * SECTOR, (len(img), total * SECTOR)
    with open(out, "wb") as f:
        f.write(img)
    print(f"ISO: {total} sectors ({len(img) // 1024} KB) -> {out}")


if __name__ == "__main__":
    main()
