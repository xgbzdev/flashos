/* FlashOS stage 2: find .swf files on USB sticks and extra disks.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * Runs in real mode during boot (gcc -m16, unreal mode for addresses above
 * 1 MB). Every BIOS hard disk (USB sticks show up as BIOS disks) is checked
 * for a FAT16 or FAT32 volume - partitioned or not. All .swf files in the
 * root folder and in first-level folders are copied into memory as a second
 * FlashFS archive, which the kernel lists next to the built-in files.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define BOOTINFO 0x6000
#define MAX_FILES 128
#define BOUNCE_SEG 0x1000
#define CHUNK_SECS 64

#ifdef HOSTED_S2
#include <stdio.h>
#include <string.h>
u8 *host_mem;          /* fake physical memory */
FILE *host_disks[4];
int host_ndisks;
#define PHYS(a) (host_mem + (u32)(a))
static int disk_read(u8 drive, u32 lba, u16 count) {
    int d = drive - 0x80;
    if (d < 0 || d >= host_ndisks) return 0;
    fseek(host_disks[d], (long)lba * 512, SEEK_SET);
    size_t n = fread(PHYS(BOUNCE_SEG * 16), 1, (size_t)count * 512, host_disks[d]);
    return n == (size_t)count * 512;
}
static int disk_ok(u8 drive) { return drive - 0x80 < host_ndisks; }
static int disk_count(void) { return host_ndisks; }
#define s2_main host_s2_main
#else
#define PHYS(a) ((u8 *)(a))
u32 s2_bios13(u32 ax, u32 dx, u32 si, u32 bx);
void s2_unreal(void);
extern u16 s2_bx;
/* volatile: stops gcc turning these loops into movsb/stosb, which in
 * 16-bit code would use 16-bit SI/DI and wrap at 64 KB */
void *memset(void *d, int c, u32 n) {
    volatile u8 *dd = d;
    for (u32 i = 0; i < n; i++) dd[i] = (u8)c;
    return d;
}
void *memcpy(void *d, const void *s, u32 n) {
    volatile u8 *dd = d;
    const volatile u8 *ss = s;
    for (u32 i = 0; i < n; i++) dd[i] = ss[i];
    return d;
}
int memcmp(const void *a, const void *b, u32 n) {
    const u8 *x = a, *y = b;
    for (u32 i = 0; i < n; i++) if (x[i] != y[i]) return x[i] - y[i];
    return 0;
}
static struct {
    u8 size, zero;
    u16 count, off, seg;
    u32 lba_lo, lba_hi;
} __attribute__((packed)) dap;

static int disk_read(u8 drive, u32 lba, u16 count) {
    dap.size = 16;
    dap.zero = 0;
    dap.count = count;
    dap.off = 0;
    dap.seg = BOUNCE_SEG;
    dap.lba_lo = lba;
    dap.lba_hi = 0;
    u32 r = s2_bios13(0x4200, drive, (u32)&dap, 0);
    s2_unreal();
    return !(r & 0x10000);
}
static int disk_ok(u8 drive) {
    u32 r = s2_bios13(0x4100, drive, 0, 0x55AA);
    s2_unreal();
    return !(r & 0x10000) && s2_bx == 0xAA55;
}
static int disk_count(void) {
    u32 addr = 0x475; /* BIOS data area: number of hard disks */
    __asm__("" : "+r"(addr));
    return *(volatile u8 *)addr;
}
#endif

static u32 rd16(const u8 *p) { return p[0] | (p[1] << 8); }
static u32 rd32(const u8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24); }
static void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }

static u8 sec[512], fatsec[512];
static u32 fatsec_lba = 0xFFFFFFFF;

static int read_sec(u8 drive, u32 lba, u8 *dst) {
    if (!disk_read(drive, lba, 1)) return 0;
    memcpy(dst, PHYS(BOUNCE_SEG * 16), 512);
    return 1;
}

typedef struct {
    u8 drive;
    int fat32;
    u32 spc, fat_lba, root_lba, root_secs, data_lba, root_clus, nclus;
} fat_t;

static int mount(fat_t *f, u8 drive, u32 lba) {
    if (!read_sec(drive, lba, sec)) return 0;
    if (sec[510] != 0x55 || sec[511] != 0xAA) return 0;
    u32 bps = rd16(sec + 11), spc = sec[13], rsv = rd16(sec + 14), nfats = sec[16];
    u32 rootents = rd16(sec + 17), tot = rd16(sec + 19), fatsz = rd16(sec + 22);
    if (bps != 512 || !spc || (spc & (spc - 1)) || !rsv || nfats < 1 || nfats > 2) return 0;
    if (!tot) tot = rd32(sec + 32);
    if (!fatsz) fatsz = rd32(sec + 36);
    if (!fatsz || !tot) return 0;
    u32 root_secs = (rootents * 32 + 511) / 512;
    u32 data = rsv + nfats * fatsz + root_secs;
    if (data >= tot) return 0;
    u32 nclus = (tot - data) / spc;
    if (nclus < 4085) return 0; /* FAT12: not supported */
    f->drive = drive;
    f->fat32 = nclus >= 65525;
    f->spc = spc;
    f->fat_lba = lba + rsv;
    f->root_lba = lba + rsv + nfats * fatsz;
    f->root_secs = root_secs;
    f->data_lba = lba + data;
    f->root_clus = f->fat32 ? rd32(sec + 44) : 0;
    f->nclus = nclus;
    fatsec_lba = 0xFFFFFFFF;
    return 1;
}

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
/* name ends with "." + e (lower case), any case */
static int ext_is(const char *n, const char *e) {
    int l = 0, k = 0;
    while (n[l]) l++;
    while (e[k]) k++;
    if (l <= k + 1 || n[l - k - 1] != '.') return 0;
    for (int i = 0; i < k; i++) if (lower(n[l - k + i]) != e[i]) return 0;
    return 1;
}

static u32 next_clus(fat_t *f, u32 c) {
    u32 off = f->fat32 ? c * 4 : c * 2;
    u32 lba = f->fat_lba + off / 512;
    if (lba != fatsec_lba) {
        if (!read_sec(f->drive, lba, fatsec)) return 0x0FFFFFFF;
        fatsec_lba = lba;
    }
    off %= 512;
    if (f->fat32) {
        u32 v = rd32(fatsec + off) & 0x0FFFFFFF;
        return v >= 0x0FFFFFF7 ? 0x0FFFFFFF : v;
    }
    u32 v = rd16(fatsec + off);
    return v >= 0xFFF7 ? 0x0FFFFFFF : v;
}
static int clus_ok(fat_t *f, u32 c) { return c >= 2 && c < f->nclus + 2; }
static u32 clus_lba(fat_t *f, u32 c) { return f->data_lba + (c - 2) * f->spc; }

/* ---- output archive ---- */
static u8 *arch;
static u32 nfiles, data_pos, limit;

static void add_file(fat_t *f, const char *name, u32 clus, u32 size) {
    if (nfiles >= MAX_FILES || !size) return;
    u32 start = data_pos;
    if (start + size > limit) return;
    u8 *dst = arch + start;
    u32 left = size;
    while (left && clus_ok(f, clus)) {
        /* read a run of consecutive clusters at once */
        u32 run = 1, c = clus;
        while (run * f->spc < CHUNK_SECS && run * f->spc * 512 < left) {
            u32 n = next_clus(f, c);
            if (n != c + 1) break;
            c = n;
            run++;
        }
        u32 secs = run * f->spc;
        if (!disk_read(f->drive, clus_lba(f, clus), (u16)secs)) return;
        u32 bytes = secs * 512 < left ? secs * 512 : left;
        memcpy(dst, PHYS(BOUNCE_SEG * 16), bytes);
        dst += bytes;
        left -= bytes;
        clus = next_clus(f, c);
    }
    if (left) return;
    /* a .swf must really be a SWF */
    u8 *d = arch + start;
    if (ext_is(name, "swf") && !((d[0] == 'F' || d[0] == 'C' || d[0] == 'Z') && d[1] == 'W' && d[2] == 'S')) return;
    u8 *e = arch + 8 + nfiles * 64;
    for (int i = 0; i < 56; i++) e[i] = 0;
    for (int i = 0; i < 55 && name[i]; i++) e[i] = (u8)name[i];
    wr32(e + 56, start);
    wr32(e + 60, size);
    nfiles++;
    data_pos = (start + size + 15) & ~15u;
}

/* ---- directories ---- */
static char lfn[260];
static int lfn_ok;
static u32 subdirs[16];
static int nsub;

static int is_swf(const char *n) {
    static const char *const ext[] = {"swf", "txt", "jpg", "jpeg", "png", "gif", "bmp", "flv", 0};
    for (int i = 0; ext[i]; i++) if (ext_is(n, ext[i])) return 1;
    return 0;
}

/* process one 512-byte directory sector; returns 0 at end of directory */
static int dir_sector(fat_t *f, const u8 *s, int top) {
    for (int i = 0; i < 512; i += 32) {
        const u8 *d = s + i;
        if (d[0] == 0) return 0;
        if (d[0] == 0xE5) { lfn_ok = 0; continue; }
        if (d[11] == 0x0F) { /* long file name piece */
            int seq = d[0] & 0x1F;
            if (d[0] & 0x40) { for (int k = 0; k < 260; k++) lfn[k] = 0; lfn_ok = 1; }
            if (seq < 1 || seq > 20) { lfn_ok = 0; continue; }
            static const u8 pos[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
            for (int k = 0; k < 13; k++) {
                u32 ch = rd16(d + pos[k]);
                int idx = (seq - 1) * 13 + k;
                if (idx < 259 && ch != 0xFFFF && ch) lfn[idx] = ch < 128 ? (char)ch : '_';
            }
            continue;
        }
        char name[64];
        int n = 0;
        if (lfn_ok && lfn[0]) {
            for (; lfn[n] && n < 63; n++) name[n] = lfn[n];
        } else {
            for (int k = 0; k < 8 && d[k] != ' '; k++) name[n++] = lower((char)d[k]);
            if (d[8] != ' ') {
                name[n++] = '.';
                for (int k = 8; k < 11 && d[k] != ' '; k++) name[n++] = lower((char)d[k]);
            }
        }
        name[n] = 0;
        lfn_ok = 0;
        u8 attr = d[11];
        if (attr & 0x08) continue;                     /* volume label */
        u32 clus = (rd16(d + 20) << 16) | rd16(d + 26);
        if (!f->fat32) clus &= 0xFFFF;
        if (attr & 0x10) {
            if (top && name[0] != '.' && nsub < 16 && clus_ok(f, clus)) subdirs[nsub++] = clus;
            continue;
        }
        if (name[0] == '.' && name[1] == '_') continue; /* macOS "._" metadata files */
        if (attr & 0x02) continue;                      /* hidden */
        if (is_swf(name)) add_file(f, name, clus, rd32(d + 28));
    }
    return 1;
}

static u8 dirsec[512];
static void scan_chain(fat_t *f, u32 clus, int top) {
    int guard = 0;
    lfn_ok = 0;
    while (clus_ok(f, clus) && guard++ < 4096) {
        for (u32 s = 0; s < f->spc; s++) {
            if (!read_sec(f->drive, clus_lba(f, clus) + s, dirsec)) return;
            if (!dir_sector(f, dirsec, top)) return;
        }
        clus = next_clus(f, clus);
    }
}

static void scan_volume(fat_t *f) {
    nsub = 0;
    lfn_ok = 0;
    if (f->fat32) scan_chain(f, f->root_clus, 1);
    else
        for (u32 s = 0; s < f->root_secs; s++) {
            if (!read_sec(f->drive, f->root_lba + s, dirsec)) break;
            if (!dir_sector(f, dirsec, 1)) break;
        }
    int n = nsub;
    u32 dirs[16];
    for (int i = 0; i < n; i++) dirs[i] = subdirs[i];
    for (int i = 0; i < n; i++) scan_chain(f, dirs[i], 0);
}

static u8 *diag; /* bootinfo+28: disks, EDD mask, FAT mask, files */

static void scan_drive(u8 drive) {
    fat_t f;
    if (!read_sec(drive, 0, sec)) return;
    diag[4] |= (u8)(1 << (drive & 7));
    if ((drive & 7) < 4) {
        u8 *b = diag + 8 + (drive & 7) * 6;
        b[0] = sec[0]; b[1] = sec[1]; b[2] = sec[11]; b[3] = sec[12]; b[4] = sec[510]; b[5] = sec[511];
    }
    if (sec[510] != 0x55 || sec[511] != 0xAA) return;
    /* unpartitioned "superfloppy" stick? */
    u32 bps = rd16(sec + 11);
    if ((sec[0] == 0xEB || sec[0] == 0xE9) && bps == 512 && mount(&f, drive, 0)) { diag[2] |= (u8)(1 << (drive & 7)); scan_volume(&f); return; }
    u8 mbr[64];
    memcpy(mbr, sec + 446, 64);
    for (int p = 0; p < 4; p++) {
        u8 type = mbr[p * 16 + 4];
        u32 start = rd32(mbr + p * 16 + 8);
        if (type == 0x04 || type == 0x06 || type == 0x0E || type == 0x0B || type == 0x0C || type == 0x1B ||
            type == 0x1C || type == 0x1E)
            if (start && mount(&f, drive, start)) { diag[2] |= (u8)(1 << (drive & 7)); scan_volume(&f); }
    }
}

void s2_main(void) {
    u32 *bi = (u32 *)PHYS(BOOTINFO);
    u32 fs_bytes = bi[3], memtop = bi[4];
    u32 base = (0x02000000u + fs_bytes + 0xFFFFF) & ~0xFFFFFu;
    if (memtop < base + (64u << 20)) memtop = 0x04000000u; /* unknown: assume 64 MB */
    bi[5] = 0;
    bi[6] = 0;
    fatsec_lba = 0xFFFFFFFF;
    nsub = 0;
    lfn_ok = 0;
    diag = PHYS(BOOTINFO + 28);
    diag[0] = 0xEE; /* not scanned: too little memory */
    if (memtop < base + (40u << 20)) return;
    limit = memtop - base - (32u << 20); /* keep 32 MB for ActionScript */
    arch = PHYS(base);
    nfiles = 0;
    data_pos = 8 + MAX_FILES * 64;
    int n = disk_count();
    if (n > 8) n = 8;
    diag = PHYS(BOOTINFO + 28);
    for (int i = 0; i < 32; i++) diag[i] = 0;
    diag[0] = (u8)n;
    for (int i = 0; i < n; i++) {
        u8 d = (u8)(0x80 + i);
        if (disk_ok(d)) diag[1] |= (u8)(1 << i);
        scan_drive(d); /* try even without EDD: some BIOSes misreport it */
    }
    diag[3] = (u8)nfiles;
    if (!nfiles) return;
    arch[0] = 'F'; arch[1] = 'L'; arch[2] = 'F'; arch[3] = 'S';
    wr32(arch + 4, nfiles);
    bi[5] = base;
    bi[6] = data_pos;
}
