/* FlashKernel IDE/ATA disk driver (PIO), used by INSTALL.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * Works with IDE controllers and SATA controllers in IDE/legacy mode,
 * which covers QEMU, VirtualBox (IDE controller) and many older PCs.
 */
#include "kernel.h"

#ifndef HOSTED
static inline void outb(u16 p, u8 v) { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p)); }
static inline u8 inb(u16 p) { u8 v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline u16 inw(u16 p) { u16 v; __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(p)); }

static void delay400(int ctrl) { for (int i = 0; i < 4; i++) (void)inb((u16)ctrl); }

static int wait_ready(int base, int need_drq) {
    for (u32 i = 0; i < 2000000; i++) {
        u8 st = inb((u16)(base + 7));
        if (st == 0xFF) return 0;
        if (st & 0x80) continue;           /* BSY */
        if (st & 0x21) return 0;           /* ERR / DF */
        if (!need_drq || (st & 0x08)) return 1;
    }
    return 0;
}

static int identify(int base, int ctrl, int slave, ata_disk *d) {
    outb((u16)(base + 6), (u8)(0xA0 | (slave << 4)));
    delay400(ctrl);
    if (inb((u16)(base + 7)) == 0xFF) return 0;
    outb((u16)(base + 2), 0);
    outb((u16)(base + 3), 0);
    outb((u16)(base + 4), 0);
    outb((u16)(base + 5), 0);
    outb((u16)(base + 7), 0xEC);
    delay400(ctrl);
    u8 st = inb((u16)(base + 7));
    if (!st) return 0;
    for (u32 i = 0; i < 1000000 && (st & 0x80); i++) st = inb((u16)(base + 7));
    if (inb((u16)(base + 4)) || inb((u16)(base + 5))) return 0; /* ATAPI (CD drive) or SATA */
    if (!wait_ready(base, 1)) return 0;
    u16 id[256];
    for (int i = 0; i < 256; i++) id[i] = inw((u16)base);
    d->base = base;
    d->ctrl = ctrl;
    d->slave = slave;
    d->sectors = id[60] | ((u32)id[61] << 16);
    for (int i = 0; i < 20; i++) { d->model[i * 2] = (char)(id[27 + i] >> 8); d->model[i * 2 + 1] = (char)id[27 + i]; }
    d->model[40] = 0;
    for (int i = 39; i >= 0 && d->model[i] == ' '; i--) d->model[i] = 0;
    return d->sectors > 0;
}

int ata_scan(ata_disk *out, int max) {
    static const int ports[2][2] = {{0x1F0, 0x3F6}, {0x170, 0x376}};
    int n = 0;
    for (int c = 0; c < 2; c++)
        for (int s = 0; s < 2 && n < max; s++)
            if (identify(ports[c][0], ports[c][1], s, &out[n])) n++;
    return n;
}

static int rw(ata_disk *d, u32 lba, u8 *buf, u32 count, int write) {
    for (u32 k = 0; k < count; k++, lba++) {
        if (!wait_ready(d->base, 0)) return 0;
        outb((u16)(d->base + 6), (u8)(0xE0 | (d->slave << 4) | ((lba >> 24) & 0x0F)));
        outb((u16)(d->base + 2), 1);
        outb((u16)(d->base + 3), (u8)lba);
        outb((u16)(d->base + 4), (u8)(lba >> 8));
        outb((u16)(d->base + 5), (u8)(lba >> 16));
        outb((u16)(d->base + 7), write ? 0x30 : 0x20);
        delay400(d->ctrl);
        if (!wait_ready(d->base, 1)) return 0;
        u16 *w = (u16 *)(buf + k * 512);
        for (int i = 0; i < 256; i++) {
            if (write) outw((u16)d->base, w[i]);
            else w[i] = inw((u16)d->base);
        }
    }
    if (write) {
        outb((u16)(d->base + 7), 0xE7); /* flush cache */
        delay400(d->ctrl);
        if (!wait_ready(d->base, 0)) return 0;
    }
    return 1;
}
int ata_write(ata_disk *d, u32 lba, const u8 *buf, u32 count) { return rw(d, lba, (u8 *)buf, count, 1); }
int ata_read(ata_disk *d, u32 lba, u8 *buf, u32 count) { return rw(d, lba, buf, count, 0); }
/* ---------------- ATAPI (CD/DVD drives) ---------------- */
static int atapi_identify(int base, int ctrl, int slave, ata_disk *d) {
    outb((u16)(base + 6), (u8)(0xA0 | (slave << 4)));
    delay400(ctrl);
    u8 st = inb((u16)(base + 7));
    if (st == 0xFF) return 0;
    /* a reset leaves the ATAPI signature in the LBA mid/high registers */
    outb((u16)(base + 7), 0x08); /* DEVICE RESET (ATAPI only) */
    delay400(ctrl);
    for (u32 i = 0; i < 1000000 && (inb((u16)(base + 7)) & 0x80); i++) {}
    if (inb((u16)(base + 4)) != 0x14 || inb((u16)(base + 5)) != 0xEB) return 0;
    outb((u16)(base + 7), 0xA1); /* IDENTIFY PACKET DEVICE */
    delay400(ctrl);
    if (!wait_ready(base, 1)) return 0;
    u16 id[256];
    for (int i = 0; i < 256; i++) id[i] = inw((u16)base);
    d->base = base;
    d->ctrl = ctrl;
    d->slave = slave;
    d->sectors = 0;
    for (int i = 0; i < 20; i++) { d->model[i * 2] = (char)(id[27 + i] >> 8); d->model[i * 2 + 1] = (char)id[27 + i]; }
    d->model[40] = 0;
    for (int i = 39; i >= 0 && d->model[i] == ' '; i--) d->model[i] = 0;
    return 1;
}

int atapi_scan(ata_disk *out, int max) {
    static const int ports[2][2] = {{0x1F0, 0x3F6}, {0x170, 0x376}};
    int n = 0;
    for (int c = 0; c < 2; c++)
        for (int s = 0; s < 2 && n < max; s++)
            if (atapi_identify(ports[c][0], ports[c][1], s, &out[n])) n++;
    return n;
}

static int atapi_packet(ata_disk *d, const u8 *pkt, u8 *buf, u32 want) {
    int base = d->base;
    if (!wait_ready(base, 0) && (inb((u16)(base + 7)) & 0x80)) return 0;
    outb((u16)(base + 6), (u8)(0xA0 | (d->slave << 4)));
    delay400(d->ctrl);
    outb((u16)(base + 1), 0);          /* PIO */
    outb((u16)(base + 4), (u8)(want & 0xFF));
    outb((u16)(base + 5), (u8)(want >> 8));
    outb((u16)(base + 7), 0xA0);       /* PACKET */
    delay400(d->ctrl);
    if (!wait_ready(base, 1)) return 0;
    for (int i = 0; i < 6; i++) outw((u16)base, (u16)(pkt[i * 2] | (pkt[i * 2 + 1] << 8)));
    u32 got = 0;
    while (got < want) {
        delay400(d->ctrl);
        if (!wait_ready(base, 1)) return 0;
        u32 n = inb((u16)(base + 4)) | (inb((u16)(base + 5)) << 8);
        if (!n || got + n > want) return 0;
        u16 *w = (u16 *)(buf + got);
        for (u32 i = 0; i < n / 2; i++) w[i] = inw((u16)base);
        got += n;
    }
    for (u32 i = 0; i < 100000 && (inb((u16)(base + 7)) & 0x80); i++) {}
    return 1;
}

/* read count 2048-byte sectors */
int atapi_read(ata_disk *d, u32 lba, u8 *buf, u32 count) {
    for (u32 k = 0; k < count; k++, lba++) {
        u8 pkt[12] = {0x28, 0, (u8)(lba >> 24), (u8)(lba >> 16), (u8)(lba >> 8), (u8)lba, 0, 0, 1, 0, 0, 0};
        int ok = 0;
        for (int tries = 0; tries < 4 && !ok; tries++) { /* first read after a disc change fails once */
            ok = atapi_packet(d, pkt, buf + k * 2048, 2048);
            if (!ok) {
                u8 sense[12] = {0x03, 0, 0, 0, 18, 0, 0, 0, 0, 0, 0, 0};
                u8 sb[18];
                atapi_packet(d, sense, sb, 18);
            }
        }
        if (!ok) return 0;
    }
    return 1;
}
#else
int atapi_scan(ata_disk *out, int max) { (void)out; (void)max; return 0; }
int atapi_read(ata_disk *d, u32 lba, u8 *buf, u32 count) { (void)d; (void)lba; (void)buf; (void)count; return 0; }
int ata_scan(ata_disk *out, int max) { (void)out; (void)max; return 0; }
int ata_write(ata_disk *d, u32 lba, const u8 *buf, u32 count) { (void)d; (void)lba; (void)buf; (void)count; return 0; }
int ata_read(ata_disk *d, u32 lba, u8 *buf, u32 count) { (void)d; (void)lba; (void)buf; (void)count; return 0; }
#endif
