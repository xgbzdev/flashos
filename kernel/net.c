/* FlashKernel wired networking.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * Drivers: Intel PRO/1000 (e1000 family: VirtualBox / QEMU default, many
 * Intel boards) and Realtek RTL8139. Protocols: Ethernet, ARP, IPv4, ICMP
 * (ping), UDP, DHCP, DNS, TCP (client) and HTTP/1.0 GET. Polled, no IRQs.
 * HTTPS is not supported (no TLS).
 */
#include "kernel.h"

#ifdef HOSTED
int net_init(void) { return 0; }
const char *net_status(char *buf) { (void)buf; return "no network in the simulator"; }
int net_ping(const char *host, u32 *ms, char *err) { (void)host; (void)ms; kstrcpy(err, "no network", 32); return 0; }
int net_get(const char *url, u8 **data, u32 *len, int *status, char *err) {
    (void)url; (void)data; (void)len; (void)status; kstrcpy(err, "no network", 32); return 0;
}
/* simulator: pages come from files sim_web_<n>.html, picked by the URL */
#include <stdio.h>
int net_get_buf(const char *url, u8 *buf, u32 cap, u8 **data, u32 *len, int *status, char *err) {
    char name[64];
    unsigned h = 0;
    for (const char *p = url; *p; p++) h = h * 31 + (u8)*p;
    FILE *f = fopen("sim_web.html", "rb");
    snprintf(name, sizeof name, "sim_web_%u.html", h % 4);
    const char *dot = url;                                /* pictures: sim_img.<ext> */
    for (const char *p = url; *p; p++) if (*p == '.') dot = p;
    if (!kstricmp(dot, ".png") || !kstricmp(dot, ".jpg") || !kstricmp(dot, ".gif"))
        snprintf(name, sizeof name, "sim_img%s", dot);
    FILE *g = fopen(name, "rb");
    if (g) { if (f) fclose(f); f = g; }
    if (!f) { kstrcpy(err, "no network in the simulator", 48); return 0; }
    *len = (u32)fread(buf, 1, cap - 1, f);
    fclose(f);
    *data = buf;
    *status = 200;
    return 1;
}
#else

static inline void outb(u16 p, u8 v) { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p)); }
static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(p)); }
static inline void outl(u16 p, u32 v) { __asm__ volatile("outl %0, %1" : : "a"(v), "Nd"(p)); }
static inline u8 inb(u16 p) { u8 v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline u16 inw(u16 p) { u16 v; __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline u32 inl(u16 p) { u32 v; __asm__ volatile("inl %1, %0" : "=a"(v) : "Nd"(p)); return v; }


/* ---------------------------------------------------------------- PCI */
static u32 pci_read(int bus, int dev, int fn, int off) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (off & 0xFC));
    return inl(0xCFC);
}
static void pci_write(int bus, int dev, int fn, int off, u32 v) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (off & 0xFC));
    outl(0xCFC, v);
}

/* ---------------------------------------------------------------- NIC layer */
static u8 mac[6];
static const char *card = "none";
static int (*nic_send)(const u8 *p, u32 len);
static int (*nic_recv)(u8 *p);            /* returns length or 0 */
static int (*nic_link)(void);

/* ---- Intel e1000 ---- */
typedef struct { u32 addr_lo, addr_hi; u16 len, csum; u8 status, errors; u16 special; } __attribute__((packed)) e1k_rx;
typedef struct { u32 addr_lo, addr_hi; u16 len; u8 cso, cmd, status, css; u16 special; } __attribute__((packed)) e1k_tx;
#define E_RX 32
#define E_TX 8
static volatile u8 *e1k;
static e1k_rx erx[E_RX] __attribute__((aligned(16)));
static e1k_tx etx[E_TX] __attribute__((aligned(16)));
static u8 erxbuf[E_RX][2048] __attribute__((aligned(16)));
static u8 etxbuf[E_TX][2048] __attribute__((aligned(16)));
static int erx_cur, etx_cur;

static u32 er(u32 r) { return *(volatile u32 *)(e1k + r); }
static void ew(u32 r, u32 v) { *(volatile u32 *)(e1k + r) = v; }

static int e1k_send(const u8 *p, u32 len) {
    if (len > 1514) return 0;
    e1k_tx *d = &etx[etx_cur];
    for (u32 i = 0; i < 200000 && d->cmd && !(d->status & 1); i++) {}
    memcpy(etxbuf[etx_cur], p, len);
    if (len < 60) { memset(etxbuf[etx_cur] + len, 0, 60 - len); len = 60; }
    d->addr_lo = (u32)etxbuf[etx_cur];
    d->addr_hi = 0;
    d->len = (u16)len;
    d->status = 0;
    d->cmd = 0x0B; /* EOP | IFCS | RS */
    etx_cur = (etx_cur + 1) % E_TX;
    ew(0x3818, (u32)etx_cur);
    return 1;
}
static int e1k_recv(u8 *p) {
    e1k_rx *d = &erx[erx_cur];
    if (!(d->status & 1)) return 0;
    int len = d->len;
    if (len > 1518 || !(d->status & 2)) len = 0;
    else memcpy(p, erxbuf[erx_cur], (u32)len);
    d->status = 0;
    int old = erx_cur;
    erx_cur = (erx_cur + 1) % E_RX;
    ew(0x2818, (u32)old);
    return len;
}
static int e1k_link(void) { return (er(0x0008) & 2) != 0; }

static int e1k_eeprom(int addr, u16 *out) {
    ew(0x0014, 1 | ((u32)addr << 8));
    for (int i = 0; i < 100000; i++) {
        u32 v = er(0x0014);
        if (v & 0x10) { *out = (u16)(v >> 16); return 1; }
    }
    ew(0x0014, 1 | ((u32)addr << 2)); /* 82574 style */
    for (int i = 0; i < 100000; i++) {
        u32 v = er(0x0014);
        if (v & 0x02) { *out = (u16)(v >> 16); return 1; }
    }
    return 0;
}

static int e1k_init(u32 bar) {
    e1k = (volatile u8 *)(bar & ~0xFu);
    ew(0x0000, er(0x0000) | 0x04000000);         /* reset */
    plat_sleep(20);
    ew(0x00D8, 0xFFFFFFFF);                       /* mask all interrupts */
    (void)er(0x00C0);
    ew(0x0000, (er(0x0000) | 0x40 | 0x20) & ~0x08u); /* SLU | ASDE, no LRST */
    u32 lo = er(0x5400), hi = er(0x5404);
    if (hi & 0x80000000u) {
        for (int i = 0; i < 4; i++) mac[i] = (u8)(lo >> (i * 8));
        mac[4] = (u8)hi;
        mac[5] = (u8)(hi >> 8);
    } else {
        u16 w;
        for (int i = 0; i < 3; i++) {
            if (!e1k_eeprom(i, &w)) return 0;
            mac[i * 2] = (u8)w;
            mac[i * 2 + 1] = (u8)(w >> 8);
        }
        ew(0x5400, mac[0] | (mac[1] << 8) | (mac[2] << 16) | ((u32)mac[3] << 24));
        ew(0x5404, mac[4] | (mac[5] << 8) | 0x80000000u);
    }
    for (int i = 0; i < 128; i++) ew(0x5200 + i * 4, 0);
    for (int i = 0; i < E_RX; i++) { erx[i].addr_lo = (u32)erxbuf[i]; erx[i].addr_hi = 0; erx[i].status = 0; }
    ew(0x2800, (u32)erx);
    ew(0x2804, 0);
    ew(0x2808, sizeof erx);
    ew(0x2810, 0);
    ew(0x2818, E_RX - 1);
    ew(0x0100, 0x02 | 0x8000 | 0x04000000);      /* EN | BAM | SECRC, 2 KB buffers */
    for (int i = 0; i < E_TX; i++) { etx[i].addr_lo = 0; etx[i].cmd = 0; etx[i].status = 1; }
    ew(0x3800, (u32)etx);
    ew(0x3804, 0);
    ew(0x3808, sizeof etx);
    ew(0x3810, 0);
    ew(0x3818, 0);
    ew(0x0400, 0x02 | 0x08 | (0x0F << 4) | (0x40 << 12)); /* EN | PSP | CT | COLD */
    ew(0x0410, 0x0060200A);
    erx_cur = etx_cur = 0;
    nic_send = e1k_send;
    nic_recv = e1k_recv;
    nic_link = e1k_link;
    return 1;
}

/* ---- Realtek RTL8139 ---- */
static u16 rtl;
static u8 rtl_rx[8192 + 16 + 1536] __attribute__((aligned(16)));
static u8 rtl_tx[4][1536] __attribute__((aligned(16)));
static int rtl_txi;
static u32 rtl_rxoff;

static u8 rtl_used[4];
static int rtl_send(const u8 *p, u32 len) {
    if (len > 1514) return 0;
    u16 tsd = (u16)(rtl + 0x10 + rtl_txi * 4);
    if (rtl_used[rtl_txi]) /* wait until the card is done with this buffer (OWN) */
        for (u32 i = 0; i < 200000 && !(inl(tsd) & 0x2000); i++) {}
    memcpy(rtl_tx[rtl_txi], p, len);
    if (len < 60) { memset(rtl_tx[rtl_txi] + len, 0, 60 - len); len = 60; }
    outl((u16)(rtl + 0x20 + rtl_txi * 4), (u32)rtl_tx[rtl_txi]);
    outl(tsd, len);
    rtl_used[rtl_txi] = 1;
    rtl_txi = (rtl_txi + 1) & 3;
    return 1;
}
static int rtl_recv(u8 *p) {
    if (inb((u16)(rtl + 0x37)) & 1) return 0; /* buffer empty */
    u8 *h = rtl_rx + rtl_rxoff;
    u16 st = (u16)(h[0] | (h[1] << 8)), len = (u16)(h[2] | (h[3] << 8));
    int out = 0;
    if ((st & 1) && len >= 4 && len <= 1518) {
        u32 n = len - 4;
        memcpy(p, rtl_rx + rtl_rxoff + 4, n); /* WRAP mode: packets are contiguous */
        out = (int)n;
    }
    rtl_rxoff = (rtl_rxoff + len + 4 + 3) & ~3u;
    rtl_rxoff %= 8192;
    outw((u16)(rtl + 0x38), (u16)(rtl_rxoff - 16));
    outw((u16)(rtl + 0x3E), 0xFFFF);
    return out;
}
static int rtl_link(void) { return !(inb((u16)(rtl + 0x58)) & 4); }

static int rtl_init(u32 bar) {
    rtl = (u16)(bar & ~3u);
    outb((u16)(rtl + 0x52), 0);
    outb((u16)(rtl + 0x37), 0x10);
    for (int i = 0; i < 100000 && (inb((u16)(rtl + 0x37)) & 0x10); i++) {}
    for (int i = 0; i < 6; i++) mac[i] = inb((u16)(rtl + i));
    outl((u16)(rtl + 0x30), (u32)rtl_rx);
    outw((u16)(rtl + 0x3C), 0);
    outl((u16)(rtl + 0x44), 0x0F | 0x80);         /* AB | AM | APM | AAP, WRAP */
    outb((u16)(rtl + 0x37), 0x0C);                /* RE | TE */
    rtl_rxoff = 0;
    rtl_txi = 0;
    memset(rtl_used, 0, sizeof rtl_used);
    nic_send = rtl_send;
    nic_recv = rtl_recv;
    nic_link = rtl_link;
    return 1;
}

static const u16 e1k_ids[] = {0x100E, 0x100F, 0x1004, 0x100C, 0x1015, 0x1016, 0x1017, 0x101E, 0x1026,
                              0x1076, 0x1079, 0x107C, 0x10D3, 0x10F5, 0x10EA, 0x1502, 0x153A, 0x15B8, 0};

static int probe(void) {
    for (int bus = 0; bus < 8; bus++)
        for (int dev = 0; dev < 32; dev++)
            for (int fn = 0; fn < 8; fn++) {
                u32 id = pci_read(bus, dev, fn, 0);
                if ((id & 0xFFFF) == 0xFFFF) { if (!fn) break; continue; }
                u16 ven = (u16)id, did = (u16)(id >> 16);
                u32 bar0 = pci_read(bus, dev, fn, 0x10);
                if (ven == 0x8086) {
                    for (int i = 0; e1k_ids[i]; i++)
                        if (did == e1k_ids[i]) {
                            pci_write(bus, dev, fn, 0x04, pci_read(bus, dev, fn, 0x04) | 0x06);
                            if (e1k_init(bar0)) { card = "Intel PRO/1000"; return 1; }
                        }
                }
                if (ven == 0x10EC && did == 0x8139) {
                    pci_write(bus, dev, fn, 0x04, pci_read(bus, dev, fn, 0x04) | 0x07);
                    if (rtl_init(bar0)) { card = "Realtek RTL8139"; return 1; }
                }
                if (!fn && !(pci_read(bus, dev, 0, 0x0C) & 0x00800000)) break;
            }
    return 0;
}

/* ---------------------------------------------------------------- IP layer */
static u32 my_ip, gw_ip, netmask, dns_ip;
static int inited, configured;
static u8 pkt[1600], out[1600];

typedef struct { u32 ip; u8 mac[6]; } arp_ent;
static arp_ent arp_tab[16];
static int arp_n;

static u16 csum(const void *data, u32 len, u32 sum) {
    const u8 *p = data;
    for (u32 i = 0; i + 1 < len; i += 2) sum += (p[i] << 8) | p[i + 1];
    if (len & 1) sum += p[len - 1] << 8;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (u16)~sum;
}
static void put16(u8 *p, u16 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static void put32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
static u16 get16(const u8 *p) { return (u16)((p[0] << 8) | p[1]); }
static u32 get32(const u8 *p) { return ((u32)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

static void eth_send(const u8 *dst, u16 type, u8 *frame, u32 len) {
    memcpy(frame, dst, 6);
    memcpy(frame + 6, mac, 6);
    put16(frame + 12, type);
    nic_send(frame, len);
}

static void arp_store(u32 ip, const u8 *m) {
    for (int i = 0; i < arp_n; i++) if (arp_tab[i].ip == ip) { memcpy(arp_tab[i].mac, m, 6); return; }
    int i = arp_n < 16 ? arp_n++ : 0;
    arp_tab[i].ip = ip;
    memcpy(arp_tab[i].mac, m, 6);
}

/* incoming packet handlers registered by the protocols below */
static void (*udp_handler)(u32 src, u16 sport, u16 dport, const u8 *data, u32 len);
static void (*tcp_handler)(u32 src, const u8 *seg, u32 len);
static u16 ping_id, ping_seq;
static int ping_got;

static void send_ip(u32 dst_ip, const u8 *dst_mac, u8 proto, u8 *frame, u32 plen) {
    u8 *ip = frame + 14;
    static u16 ident = 1;
    ip[0] = 0x45; ip[1] = 0;
    put16(ip + 2, (u16)(20 + plen));
    put16(ip + 4, ident++);
    put16(ip + 6, 0x4000);
    ip[8] = 64; ip[9] = proto;
    put16(ip + 10, 0);
    put32(ip + 12, my_ip);
    put32(ip + 16, dst_ip);
    put16(ip + 10, csum(ip, 20, 0));
    eth_send(dst_mac, 0x0800, frame, 14 + 20 + plen);
}

static void handle(u8 *f, int len) {
    if (len < 14) return;
    u16 type = get16(f + 12);
    if (type == 0x0806 && len >= 42) {
        u8 *a = f + 14;
        u16 op = get16(a + 6);
        u32 sip = get32(a + 14), tip = get32(a + 24);
        if (sip) arp_store(sip, a + 8);
        if (op == 1 && my_ip && tip == my_ip) {
            u8 *r = out + 14;
            put16(r, 1); put16(r + 2, 0x0800); r[4] = 6; r[5] = 4; put16(r + 6, 2);
            memcpy(r + 8, mac, 6); put32(r + 14, my_ip);
            memcpy(r + 18, a + 8, 6); put32(r + 24, sip);
            eth_send(a + 8, 0x0806, out, 42);
        }
        return;
    }
    if (type != 0x0800 || len < 34) return;
    u8 *ip = f + 14;
    int ihl = (ip[0] & 15) * 4;
    u32 tot = get16(ip + 2);
    if (tot > (u32)len - 14 || (int)tot < ihl) return;
    u32 src = get32(ip + 12), dst = get32(ip + 16);
    if (my_ip && dst != my_ip && dst != 0xFFFFFFFF) return;
    u8 *p = ip + ihl;
    u32 plen = tot - (u32)ihl;
    if (ip[9] == 1 && plen >= 8) {                     /* ICMP */
        if (p[0] == 8 && my_ip) {                      /* echo request -> reply */
            u8 *r = out + 34;
            memcpy(r, p, plen);
            r[0] = 0;
            put16(r + 2, 0);
            put16(r + 2, csum(r, plen, 0));
            send_ip(src, f + 6, 1, out, plen);
        } else if (p[0] == 0 && get16(p + 4) == ping_id && get16(p + 6) == ping_seq) {
            ping_got = 1;
        }
    } else if (ip[9] == 17 && plen >= 8) {             /* UDP */
        if (udp_handler) udp_handler(src, get16(p), get16(p + 2), p + 8, get16(p + 4) - 8u);
    } else if (ip[9] == 6 && plen >= 20) {             /* TCP */
        if (tcp_handler) tcp_handler(src, p, plen);
    }
}

static void net_poll(void) {
    int len;
    for (int n = 0; n < 64 && (len = nic_recv(pkt)) > 0; n++) handle(pkt, len);
}

static int arp_resolve(u32 ip, u8 *m) {
    static const u8 bcast[6] = {255, 255, 255, 255, 255, 255};
    if (ip == 0xFFFFFFFF) { memcpy(m, bcast, 6); return 1; }
    u32 hop = ((ip & netmask) == (my_ip & netmask)) ? ip : gw_ip;
    for (int tries = 0; tries < 4; tries++) {
        for (int i = 0; i < arp_n; i++) if (arp_tab[i].ip == hop) { memcpy(m, arp_tab[i].mac, 6); return 1; }
        u8 *r = out + 14;
        put16(r, 1); put16(r + 2, 0x0800); r[4] = 6; r[5] = 4; put16(r + 6, 1);
        memcpy(r + 8, mac, 6); put32(r + 14, my_ip);
        memset(r + 18, 0, 6); put32(r + 24, hop);
        eth_send(bcast, 0x0806, out, 42);
        u32 t = plat_ms();
        while (plat_ms() - t < 500) {
            net_poll();
            for (int i = 0; i < arp_n; i++) if (arp_tab[i].ip == hop) { memcpy(m, arp_tab[i].mac, 6); return 1; }
        }
    }
    return 0;
}

static void send_udp(u32 dst, u16 sport, u16 dport, const u8 *data, u32 len) {
    u8 m[6];
    if (!arp_resolve(dst, m)) return;
    u8 *u = out + 34;
    put16(u, sport); put16(u + 2, dport); put16(u + 4, (u16)(8 + len)); put16(u + 6, 0);
    memcpy(u + 8, data, len);
    send_ip(dst, m, 17, out, 8 + len);
}

/* ---------------------------------------------------------------- DHCP */
static u32 dhcp_xid, dhcp_offer, dhcp_server;
static int dhcp_state; /* 1 = got offer, 2 = got ack */

static void dhcp_udp(u32 src, u16 sport, u16 dport, const u8 *d, u32 len) {
    (void)src; (void)sport;
    if (dport != 68 || len < 240 || get32(d + 4) != dhcp_xid || d[0] != 2) return;
    u32 yi = get32(d + 16);
    int type = 0;
    u32 mask = 0, router = 0, dns = 0, server = 0;
    for (u32 i = 240; i + 1 < len && d[i] != 255;) {
        u8 o = d[i];
        if (!o) { i++; continue; }
        u8 l = d[i + 1];
        const u8 *v = d + i + 2;
        if (i + 2 + l > len) break;
        if (o == 53) type = v[0];
        if (o == 1 && l >= 4) mask = get32(v);
        if (o == 3 && l >= 4) router = get32(v);
        if (o == 6 && l >= 4) dns = get32(v);
        if (o == 54 && l >= 4) server = get32(v);
        i += 2 + l;
    }
    if (type == 2 && dhcp_state == 0) { dhcp_offer = yi; dhcp_server = server; dhcp_state = 1; }
    if (type == 5 && dhcp_state == 1) {
        my_ip = yi;
        netmask = mask ? mask : 0xFFFFFF00u;
        gw_ip = router;
        dns_ip = dns ? dns : router;
        dhcp_state = 2;
    }
}

static void dhcp_send(int type) {
    static u8 b[300];
    memset(b, 0, sizeof b);
    b[0] = 1; b[1] = 1; b[2] = 6;
    put32(b + 4, dhcp_xid);
    put16(b + 10, 0x8000);
    memcpy(b + 28, mac, 6);
    put32(b + 236, 0x63825363);
    int i = 240;
    b[i++] = 53; b[i++] = 1; b[i++] = (u8)type;
    if (type == 3) {
        b[i++] = 50; b[i++] = 4; put32(b + i, dhcp_offer); i += 4;
        b[i++] = 54; b[i++] = 4; put32(b + i, dhcp_server); i += 4;
    }
    b[i++] = 12; b[i++] = 7; memcpy(b + i, "flashos", 7); i += 7;
    b[i++] = 55; b[i++] = 3; b[i++] = 1; b[i++] = 3; b[i++] = 6;
    b[i++] = 255;
    u32 saved = my_ip;
    my_ip = 0;
    send_udp(0xFFFFFFFF, 68, 67, b, 300);
    my_ip = saved;
}

static int dhcp(void) {
    udp_handler = dhcp_udp;
    dhcp_xid = plat_ms() * 2654435761u ^ (mac[5] << 8 | mac[4]);
    for (int tries = 0; tries < 4 && dhcp_state < 2; tries++) {
        dhcp_state = 0;
        dhcp_send(1);
        u32 t = plat_ms();
        while (plat_ms() - t < 2000 && dhcp_state == 0) net_poll();
        if (dhcp_state != 1) continue;
        dhcp_send(3);
        t = plat_ms();
        while (plat_ms() - t < 2000 && dhcp_state == 1) net_poll();
    }
    udp_handler = NULL;
    return dhcp_state == 2;
}

/* ---------------------------------------------------------------- public: init / status */
static void ip_str(u32 ip, char *b) {
    for (int i = 0; i < 4; i++) {
        kutoa((ip >> (24 - i * 8)) & 255, b);
        b += kstrlen(b);
        if (i < 3) *b++ = '.';
    }
    *b = 0;
}

int net_init(void) {
    if (!inited) {
        inited = 1;
        if (!probe()) return -1;
        plat_sleep(100);
    }
    if (!nic_send) return -1;
    if (configured) return 1;
    u32 t = plat_ms();
    while (!nic_link() && plat_ms() - t < 3000) {}
    if (!nic_link()) return -2;
    if (!dhcp()) return -3;
    configured = 1;
    return 1;
}

const char *net_status(char *b) {
    char *p = b;
    const char *hex = "0123456789ABCDEF";
    kstrcpy(p, " Card: ", 64); p += kstrlen(p);
    kstrcpy(p, card, 64); p += kstrlen(p);
    if (!nic_send) return b;
    kstrcpy(p, "\n MAC:  ", 64); p += kstrlen(p);
    for (int i = 0; i < 6; i++) { *p++ = hex[mac[i] >> 4]; *p++ = hex[mac[i] & 15]; if (i < 5) *p++ = ':'; }
    kstrcpy(p, "\n Cable: ", 64); p += kstrlen(p);
    kstrcpy(p, nic_link() ? "connected" : "UNPLUGGED", 64); p += kstrlen(p);
    if (!configured) { kstrcpy(p, "\n IP:   not configured (no DHCP answer)", 64); return b; }
    kstrcpy(p, "\n IP:   ", 64); p += kstrlen(p); ip_str(my_ip, p); p += kstrlen(p);
    kstrcpy(p, "\n Gateway: ", 64); p += kstrlen(p); ip_str(gw_ip, p); p += kstrlen(p);
    kstrcpy(p, "\n DNS:  ", 64); p += kstrlen(p); ip_str(dns_ip, p); p += kstrlen(p);
    return b;
}

/* ---------------------------------------------------------------- DNS */
static u16 dns_id;
static u32 dns_answer;
static int dns_done;

static void dns_udp(u32 src, u16 sport, u16 dport, const u8 *d, u32 len) {
    (void)src; (void)dport;
    if (sport != 53 || len < 12 || get16(d) != dns_id) return;
    int qd = get16(d + 4), an = get16(d + 6);
    u32 i = 12;
    for (int q = 0; q < qd && i < len; q++) {
        while (i < len && d[i]) { if ((d[i] & 0xC0) == 0xC0) { i++; break; } i += d[i] + 1u; }
        i += 5;
    }
    for (int a = 0; a < an && i + 10 < len; a++) {
        if ((d[i] & 0xC0) == 0xC0) i += 2;
        else { while (i < len && d[i]) i += d[i] + 1u; i++; }
        u16 type = get16(d + i), rdl = get16(d + i + 8);
        i += 10;
        if (type == 1 && rdl == 4 && i + 4 <= len) { dns_answer = get32(d + i); dns_done = 1; return; }
        i += rdl;
    }
    dns_done = -1;
}

static int parse_ip(const char *s, u32 *ip) {
    u32 v = 0, part = 0;
    int dots = 0, digits = 0;
    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') { part = part * 10 + (*s - '0'); digits++; if (part > 255) return 0; }
        else if (*s == '.') { if (!digits) return 0; v = (v << 8) | part; part = 0; digits = 0; dots++; }
        else return 0;
    }
    if (dots != 3 || !digits) return 0;
    *ip = (v << 8) | part;
    return 1;
}

static int resolve(const char *host, u32 *ip) {
    if (parse_ip(host, ip)) return 1;
    u8 q[300];
    u32 n = 12;
    dns_id = (u16)(plat_ms() * 31 + 7);
    memset(q, 0, 12);
    put16(q, dns_id);
    put16(q + 2, 0x0100);
    put16(q + 4, 1);
    const char *p = host;
    while (*p && n < 280) {
        const char *e = p;
        while (*e && *e != '.') e++;
        u32 l = (u32)(e - p);
        if (!l || l > 63) return 0;
        q[n++] = (u8)l;
        memcpy(q + n, p, l);
        n += l;
        p = *e ? e + 1 : e;
    }
    q[n++] = 0;
    put16(q + n, 1); put16(q + n + 2, 1);
    n += 4;
    udp_handler = dns_udp;
    for (int tries = 0; tries < 3; tries++) {
        dns_done = 0;
        send_udp(dns_ip, 40000 + (u16)(dns_id & 0xFFF), 53, q, n);
        u32 t = plat_ms();
        while (plat_ms() - t < 2000 && !dns_done) net_poll();
        if (dns_done) break;
    }
    udp_handler = NULL;
    if (dns_done != 1) return 0;
    *ip = dns_answer;
    return 1;
}

/* ---------------------------------------------------------------- ping */
int net_ping(const char *host, u32 *ms, char *err) {
    int r = net_init();
    if (r < 0) { kstrcpy(err, r == -1 ? "no network card" : r == -2 ? "cable unplugged" : "no DHCP answer", 40); return 0; }
    u32 ip;
    if (!resolve(host, &ip)) { kstrcpy(err, "host not found", 40); return 0; }
    u8 m[6];
    if (!arp_resolve(ip, m)) { kstrcpy(err, "gateway not reachable", 40); return 0; }
    ping_id = (u16)(plat_ms() & 0xFFFF);
    ping_seq++;
    u8 *ic = out + 34;
    ic[0] = 8; ic[1] = 0;
    put16(ic + 2, 0);
    put16(ic + 4, ping_id);
    put16(ic + 6, ping_seq);
    for (int i = 0; i < 32; i++) ic[8 + i] = (u8)('a' + i % 26);
    put16(ic + 2, csum(ic, 40, 0));
    ping_got = 0;
    u32 t = plat_ms();
    send_ip(ip, m, 1, out, 40);
    while (plat_ms() - t < 3000 && !ping_got) net_poll();
    if (!ping_got) { kstrcpy(err, "no reply", 40); return 0; }
    *ms = plat_ms() - t;
    return 1;
}

/* ---------------------------------------------------------------- TCP + HTTP */
static struct {
    u32 ip;
    u16 lport, rport;
    u32 snd_una, snd_nxt, rcv_nxt;
    int state; /* 1 syn-sent, 2 established, 3 closed by peer, -1 reset */
    u8 *buf;
    u32 len, cap;
    u8 peer[6];
} tc;

static void tcp_send(u8 flags, const u8 *data, u32 len) {
    u8 *t = out + 34;
    put16(t, tc.lport);
    put16(t + 2, tc.rport);
    put32(t + 4, tc.snd_nxt);
    put32(t + 8, tc.rcv_nxt);
    int hl = 20;
    if (flags & 0x02) { /* SYN: MSS 1460 */
        t[20] = 2; t[21] = 4; put16(t + 22, 1460);
        hl = 24;
    }
    t[12] = (u8)((hl / 4) << 4);
    t[13] = flags;
    put16(t + 14, 65535);
    put16(t + 16, 0);
    put16(t + 18, 0);
    memcpy(t + hl, data, len);
    /* pseudo-header checksum */
    u8 ph[12];
    put32(ph, my_ip); put32(ph + 4, tc.ip); ph[8] = 0; ph[9] = 6; put16(ph + 10, (u16)(hl + len));
    u32 sum = 0;
    for (int i = 0; i < 12; i += 2) sum += (ph[i] << 8) | ph[i + 1];
    put16(t + 16, csum(t, hl + len, sum));
    send_ip(tc.ip, tc.peer, 6, out, hl + len);
}

static void tcp_in(u32 src, const u8 *s, u32 len) {
    if (src != tc.ip || get16(s) != tc.rport || get16(s + 2) != tc.lport) return;
    u32 seq = get32(s + 4), ack = get32(s + 8);
    u8 flags = s[13];
    u32 hl = (s[12] >> 4) * 4u;
    if (hl > len) return;
    const u8 *data = s + hl;
    u32 dl = len - hl;
    if (flags & 0x04) { tc.state = -1; return; }
    if (tc.state == 1) {
        if ((flags & 0x12) == 0x12 && ack == tc.snd_nxt + 1) { /* SYN consumes one number */
            tc.rcv_nxt = seq + 1;
            tc.snd_nxt = ack;
            tc.snd_una = ack;
            tc.state = 2;
            tcp_send(0x10, 0, 0);
        }
        return;
    }
    if (flags & 0x10) { if ((s32)(ack - tc.snd_una) > 0) tc.snd_una = ack; }
    if (dl && seq == tc.rcv_nxt) {
        if (tc.len + dl > tc.cap) dl = tc.cap - tc.len;
        memcpy(tc.buf + tc.len, data, dl);
        tc.len += dl;
        tc.rcv_nxt += len - hl;
    }
    if ((flags & 0x01) && seq + (len - hl) == tc.rcv_nxt) {
        tc.rcv_nxt++;
        tc.state = 3;
    }
    if (dl || (flags & 0x01)) tcp_send(0x10, 0, 0);
}

static int split_url(const char *url, char *host, u16 *port, const char **path) {
    if (!kstrnicmp(url, "https://", 8)) return -1;
    if (!kstrnicmp(url, "http://", 7)) url += 7;
    int n = 0;
    while (*url && *url != '/' && *url != ':' && n < 127) host[n++] = *url++;
    host[n] = 0;
    *port = 80;
    if (*url == ':') {
        u32 p = 0;
        url++;
        while (*url >= '0' && *url <= '9') p = p * 10 + (u32)(*url++ - '0');
        *port = (u16)p;
    }
    *path = *url ? url : "/";
    return n > 0;
}

static int http_once(const char *url, u8 *buf, u32 cap, u32 *outlen, char *err) {
    char host[128];
    u16 port;
    const char *path;
    int su = split_url(url, host, &port, &path);
    if (su < 0) { kstrcpy(err, "HTTPS is not supported (http:// only)", 48); return 0; }
    if (!su) { kstrcpy(err, "bad address", 48); return 0; }
    u32 ip;
    if (!resolve(host, &ip)) { kstrcpy(err, "host not found", 48); return 0; }
    memset(&tc, 0, sizeof tc);
    tc.ip = ip;
    tc.rport = port;
    tc.lport = (u16)(49152 + (plat_ms() & 0x3FFF));
    tc.snd_nxt = plat_ms() * 69069u + 12345u;
    tc.snd_una = tc.snd_nxt;
    tc.buf = buf;
    tc.cap = cap;
    if (!arp_resolve(ip, tc.peer)) { kstrcpy(err, "gateway not reachable", 48); return 0; }
    tcp_handler = tcp_in;
    tc.state = 1;
    for (int tries = 0; tries < 5 && tc.state == 1; tries++) {
        tcp_send(0x02, 0, 0);
        u32 t = plat_ms();
        while (plat_ms() - t < 1500 && tc.state == 1) net_poll();
    }
    if (tc.state != 2) { tcp_handler = NULL; kstrcpy(err, tc.state < 0 ? "connection refused" : "no answer from server", 48); return 0; }
    /* request */
    static char req[600];
    char *p = req;
    kstrcpy(p, "GET ", 600); p += kstrlen(p);
    kstrcpy(p, path, 400); p += kstrlen(p);
    kstrcpy(p, " HTTP/1.0\r\nHost: ", 600); p += kstrlen(p);
    kstrcpy(p, host, 128); p += kstrlen(p);
    kstrcpy(p, "\r\nUser-Agent: FlashOS/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n", 200); p += kstrlen(p);
    u32 rl = (u32)(p - req);
    u32 seq0 = tc.snd_nxt;
    u32 last = plat_ms(), sent_at = 0;
    while (tc.state == 2) {
        if ((s32)(tc.snd_una - (seq0 + rl)) < 0 && plat_ms() - sent_at > 1000) {
            tc.snd_nxt = seq0;
            tcp_send(0x18, (const u8 *)req, rl);
            tc.snd_nxt = seq0 + rl;
            sent_at = plat_ms();
        }
        u32 before = tc.len;
        net_poll();
        if (tc.len != before) last = plat_ms();
        if (plat_ms() - last > 15000) break;
        if (tc.len >= tc.cap) break;
        int k = plat_key();
        if (k == 27 || k == KEY_CTRL_Q) { kstrcpy(err, "cancelled", 48); tcp_send(0x14, 0, 0); tcp_handler = NULL; return 0; }
    }
    if (tc.state == 3) tcp_send(0x11, 0, 0); /* FIN|ACK */
    else if (tc.state == 2) tcp_send(0x14, 0, 0);
    tcp_handler = NULL;
    if (tc.state == -1 && !tc.len) { kstrcpy(err, "connection reset", 48); return 0; }
    *outlen = tc.len;
    return 1;
}

/* Downloads url. *data points into buf (body only). */
int net_get(const char *url, u8 **data, u32 *len, int *status, char *err) {
    u32 cap;
    u8 *buf = plat_heap2(&cap);
    return net_get_buf(url, buf, cap, data, len, status, err);
}

/* the same, into a caller's buffer */
int net_get_buf(const char *url, u8 *buf, u32 cap, u8 **data, u32 *len, int *status, char *err) {
    int r = net_init();
    if (r < 0) { kstrcpy(err, r == -1 ? "no supported network card" : r == -2 ? "network cable unplugged" : "no DHCP answer from the network", 48); return 0; }
    if (!buf || cap < (1u << 20)) { kstrcpy(err, "not enough memory", 48); return 0; }
    static char cur_url[512];
    kstrcpy(cur_url, url, sizeof cur_url);
    for (int redirect = 0; redirect < 4; redirect++) {
        u32 n = 0;
        if (!http_once(cur_url, buf, cap - 1, &n, err)) return 0;
        buf[n] = 0;
        /* status line */
        if (n < 12 || kstrnicmp((char *)buf, "HTTP/", 5)) { kstrcpy(err, "not an HTTP answer", 48); return 0; }
        int code = 0;
        const char *s = (const char *)buf;
        while (*s && *s != ' ') s++;
        while (*s == ' ') s++;
        while (*s >= '0' && *s <= '9') code = code * 10 + (*s++ - '0');
        *status = code;
        /* find header end and Location */
        u32 hend = 0;
        const char *loc = NULL;
        for (u32 i = 0; i + 3 < n; i++) {
            if (buf[i] == '\r' && buf[i + 1] == '\n') {
                if (!kstrnicmp((char *)buf + i + 2, "Location:", 9)) loc = (char *)buf + i + 11;
                if (buf[i + 2] == '\r' && buf[i + 3] == '\n') { hend = i + 4; break; }
            }
        }
        if (!hend) { kstrcpy(err, "bad HTTP answer", 48); return 0; }
        if ((code == 301 || code == 302 || code == 303 || code == 307 || code == 308) && loc) {
            while (*loc == ' ') loc++;
            int k = 0;
            char nu[512];
            while (loc[k] && loc[k] != '\r' && k < 500) { nu[k] = loc[k]; k++; }
            nu[k] = 0;
            if (nu[0] == '/') { /* relative */
                char host[128];
                u16 port;
                const char *path;
                split_url(cur_url, host, &port, &path);
                char *q = cur_url;
                kstrcpy(q, "http://", 512); q += kstrlen(q);
                kstrcpy(q, host, 128); q += kstrlen(q);
                if (port != 80) { *q++ = ':'; kutoa(port, q); q += kstrlen(q); }
                kstrcpy(q, nu, 400);
            } else {
                kstrcpy(cur_url, nu, sizeof cur_url);
            }
            if (!kstrnicmp(cur_url, "https://", 8)) { kstrcpy(err, "redirected to HTTPS - not supported", 48); return 0; }
            continue;
        }
        *data = buf + hend;
        *len = n - hend;
        return 1;
    }
    kstrcpy(err, "too many redirects", 48);
    return 0;
}
#endif
