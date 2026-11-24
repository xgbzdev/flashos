/* FlashKernel main: boot animation and the FlashOS shell.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 */
#include "kernel.h"
void render_init(void);

static const char *LOGO[] = {
    " ___ _         _    ___  ___ ",
    "| __| |__ _ __| |_ / _ \\/ __|",
    "| _|| / _` (_-< ' \\ (_) \\__ \\",
    "|_| |_\\__,_/__/_||_\\___/|___/",
};
static const char *WELCOME = "Welcome to FlashOS,Insert An Command,";

static void header_text(void) {
    for (int i = 0; i < 4; i++) { con_puts(LOGO[i]); con_putc('\n'); }
    con_puts(WELCOME);
}

/* The header is drawn off-screen, then revealed one pixel row at a time from
 * the top down, like an old PC slowly painting an image as it loads. */
static void boot_animation(void) {
    con_defer(1);
    con_clear();
    header_text();
    con_defer(0);

    plat_sleep(350);
    int bottom = con_row_y(5);
    for (int y = 0; y < bottom; y++) {
        video_present(0, y, scr_w, 1);
        plat_sleep(7);
        if (y % 48 == 47) plat_sleep(90); /* the next "disk block" arriving */
    }
    video_present(0, 0, scr_w, scr_h);
}

static int fresh_screen = 1;

static void redraw_header(void) {
    fresh_screen = 1;
    con_defer(1);
    con_clear();
    header_text();
    con_defer(0);
    video_present(0, 0, scr_w, scr_h);
}

/* ---------- helpers ---------- */
static int ends_swf(const char *s) {
    u32 n = kstrlen(s);
    return n > 4 && !kstricmp(s + n - 4, ".swf");
}
static int is_swf_data(const kfile *f) {
    return f->size > 8 && (f->data[0] == 'F' || f->data[0] == 'C' || f->data[0] == 'Z') &&
           f->data[1] == 'W' && f->data[2] == 'S';
}
static void put_num(u32 v) { char b[12]; kutoa(v, b); con_puts(b); }
static void pad(const char *s, int w) { con_puts(s); for (int n = (int)kstrlen(s); n < w; n++) con_putc(' '); }

static int resolve(const char *arg, kfile *f) {
    int all_digits = *arg != 0, n = 0;
    for (const char *p = arg; *p; p++) {
        if (*p < '0' || *p > '9') all_digits = 0;
        else n = n * 10 + (*p - '0');
    }
    if (all_digits) return fs_get(n - 1, f);
    if (fs_find(arg, f)) return 1;
    char with[64];
    kstrcpy(with, arg, 58);
    u32 l = kstrlen(with);
    kstrcpy(with + l, ".swf", 5);
    return fs_find(with, f);
}

/* ---------- commands ---------- */
static void cmd_help(void) {
    con_puts("\n FlashOS commands:\n"
             "  LS               list files (built-in, USB, downloads)\n"
             "  RUN <name|#>     play a SWF file (or just type its name)\n"
             "  CLS              clear the screen\n"
             "  ABOUT            about FlashOS\n"
             "  MEM              show installed RAM\n"
             "  INSTALL          install FlashOS on the hard disk\n"
             "  NET              connect to the network (cable) and show status\n"
             "  PING <host>      test the connection\n"
             "  GET <http://url> download a SWF file from the internet\n"
             "  REBOOT / SHUTDOWN\n"
             " Up/Down arrows repeat earlier commands.\n"
             " While a SWF plays press F12 or Ctrl+Q to return here.\n");
}

static void cmd_ls(void) {
    int n = fs_count();
    con_putc('\n');
    if (!n) { con_puts(" No files.\n"); return; }
    for (int i = 0; i < n; i++) {
        kfile f;
        fs_get(i, &f);
        char num[12];
        kutoa(i + 1, num);
        con_puts(i + 1 < 10 ? "  " : " ");
        con_puts(num);
        con_puts(". ");
        pad(f.name, 24);
        char kb[12];
        kutoa((f.size + 1023) / 1024, kb);
        for (int k = (int)kstrlen(kb); k < 6; k++) con_putc(' ');
        con_puts(kb);
        con_puts(" KB  ");
        con_puts(ends_swf(f.name) && is_swf_data(&f) ? "SWF" : "---");
        con_puts(f.usb == 1 ? "  USB\n" : f.usb == 2 ? "  NET\n" : "\n");
    }
}

static void cmd_run(const char *arg) {
    kfile f;
    if (!*arg) { con_puts("\n Usage: RUN <file.swf>\n"); return; }
    if (!resolve(arg, &f)) { con_puts("\n File not found: "); con_puts(arg); con_puts("   (type LS)\n"); return; }
    if (!ends_swf(f.name) || !is_swf_data(&f)) {
        con_puts("\n ACCESS DENIED: ");
        con_puts(f.name);
        con_puts(" is not a SWF file.\n FlashOS can only run .swf files.\n");
        return;
    }
    con_puts("\n Loading ");
    con_puts(f.name);
    con_puts(" ...");
    plat_sleep(250);
    const char *err = NULL;
    swf_play(f.data, f.size, &err);
    redraw_header();
    if (err) { con_puts("\n\n\n SWF error: "); con_puts(err); }
}

static void cmd_about(void) {
    con_puts("\n FlashOS " OS_VERSION " - the operating system that only plays Flash.\n"
             " Kernel: " KERNEL_NAME " " KERNEL_VERSION " (proprietary)\n"
             " Flash player: SWF 1-8 with ActionScript 1.0/2.0\n"
             " Files: ");
    put_num((u32)fs_count());
    con_putc('\n');
}

static void cmd_mem(void) {
    u32 top = plat_bootinfo()->memtop;
    u32 h2 = 0;
    plat_heap2(&h2);
    con_puts("\n Installed RAM:   ");
    if (top) {
        u32 mb = (top + 0x7FFFF) >> 20;
        put_num(mb);
        con_puts(" MB");
        if (mb >= 1024) {
            con_puts("  (");
            put_num(mb / 1024);
            con_putc('.');
            put_num((mb % 1024) * 10 / 1024);
            con_puts(" GB)");
        }
    } else {
        con_puts("unknown (BIOS did not report it)");
    }
    con_puts("\n Kernel + system: 32 MB\n SWF files:       ");
    put_num((plat_bootinfo()->fs_bytes + 1023) / 1024);
    con_puts(" KB built-in");
    if (plat_bootinfo()->usb_bytes) {
        con_puts(", ");
        put_num((plat_bootinfo()->usb_bytes + 1023) / 1024);
        con_puts(" KB from USB/disks");
    }
    const u8 *dg = (const u8 *)plat_bootinfo() + 28;
    con_puts("\n USB / disks:     ");
    if (dg[0] == 0xEE) con_puts("not scanned (needs more RAM)");
    else {
        put_num(dg[0]);
        con_puts(" BIOS disk(s), FAT on: ");
        int any = 0;
        for (int i = 0; i < 8; i++)
            if (dg[2] & (1 << i)) { if (any) con_puts(", "); con_puts("disk "); put_num((u32)i + 1); any = 1; }
        if (!any) con_puts("none");
    }
    con_puts("\n Free for Flash:  ");
    put_num(h2 >> 20);
    con_puts(" MB\n");
}

static void read_line(char *buf, int max);

/* INSTALL: copy FlashOS onto an IDE hard disk so it boots without the CD/USB.
 * The disk gets the same layout as the hybrid boot image (MBR + loader + stage 2
 * + kernel + built-in files), taken from the pristine copies the loader and
 * the kernel entry code kept in memory. */
#define SYS_COPY ((const u8 *)0x01F00000)
#define KRN_COPY ((const u8 *)0x01E00000)
#define S2_COPY ((const u8 *)0x01D00000)
static u32 le32(const u8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24); }

static int write_part(ata_disk *d, u32 lba, const u8 *src, u32 count, u32 *done, u32 total) {
    for (u32 i = 0; i < count; i += 16) {
        u32 n = count - i < 16 ? count - i : 16;
        if (!ata_write(d, lba + i, src + i * 512, n)) return 0;
        *done += n;
        u32 pct = *done * 100 / total;
        con_puts("\r Installing... ");
        put_num(pct);
        con_puts("%   ");
    }
    return 1;
}

static void cmd_install(void) {
#ifdef HOSTED
    con_puts("\n No IDE hard disk found.\n");
    return;
#else
    static ata_disk disks[4];
    int n = ata_scan(disks, 4);
    if (!n) {
        con_puts("\n No IDE hard disk found.\n"
                 " VirtualBox: Settings > Storage, put the disk on an IDE controller.\n"
                 " Real PC: set SATA mode to IDE / Legacy / Compatibility in the BIOS.\n");
        return;
    }
    const u8 *hd = SYS_COPY + 512;
    if (SYS_COPY[510] != 0x55 || SYS_COPY[511] != 0xAA || (hd[8] | (hd[9] << 8)) != 512) {
        con_puts("\n Installer data missing - boot FlashOS from the ISO to install.\n");
        return;
    }
    u32 k_lba = le32(hd + 10), k_cnt = le32(hd + 14), fs_lba = le32(hd + 18), fs_cnt = le32(hd + 22);
    u32 s2_lba = le32(hd + 30), s2_cnt = le32(hd + 34);
    u32 need = fs_lba + fs_cnt;

    con_puts("\n Hard disks:\n");
    for (int i = 0; i < n; i++) {
        con_puts("  ");
        put_num((u32)i + 1);
        con_puts(". ");
        pad(disks[i].model[0] ? disks[i].model : "ATA disk", 30);
        put_num(disks[i].sectors / 2048);
        con_puts(" MB\n");
    }
    int pick = 0;
    char ans[16];
    if (n > 1) {
        con_puts(" Install on disk number: ");
        read_line(ans, sizeof ans);
        pick = ans[0] - '1';
        if (pick < 0 || pick >= n || ans[1]) { con_puts("\n Cancelled.\n"); return; }
    }
    ata_disk *d = &disks[pick];
    if (d->sectors < need) { con_puts("\n That disk is too small.\n"); return; }
    con_puts("\n ALL DATA ON ");
    con_puts(d->model[0] ? d->model : "this disk");
    con_puts(" WILL BE ERASED.\n Type YES to install FlashOS: ");
    read_line(ans, sizeof ans);
    if (kstricmp(ans, "yes")) { con_puts("\n Cancelled.\n"); return; }
    con_putc('\n');
    u32 done = 0, total = 64 + k_cnt + s2_cnt + fs_cnt;
    int ok = write_part(d, 0, SYS_COPY, 64, &done, total) &&
             write_part(d, k_lba, KRN_COPY, k_cnt, &done, total) &&
             write_part(d, s2_lba, S2_COPY, s2_cnt, &done, total) &&
             write_part(d, fs_lba, plat_fs(), fs_cnt, &done, total);
    static u8 check[512];
    if (ok) ok = ata_read(d, 0, check, 1) && !memcmp(check, SYS_COPY, 512);
    if (!ok) { con_puts("\n Disk write failed. FlashOS was not installed.\n"); return; }
    con_puts("\n FlashOS is installed on the hard disk.\n"
             " Remove the CD / USB stick (VirtualBox: Settings > Storage, remove the\n"
             " ISO), then type REBOOT.\n");
#endif
}

static void hex8(u8 v) { const char *h = "0123456789ABCDEF"; con_putc(h[v >> 4]); con_putc(h[v & 15]); }
static void cmd_usbdiag(void) {
    const u8 *d = (const u8 *)plat_bootinfo() + 28;
    con_puts("\n disks="); put_num(d[0]);
    con_puts(" edd="); hex8(d[1]);
    con_puts(" read="); hex8(d[4]);
    con_puts(" fat="); hex8(d[2]);
    con_puts(" files="); put_num(d[3]);
    for (int i = 0; i < 4; i++) {
        con_puts("\n disk "); put_num((u32)i + 1); con_puts(": ");
        for (int k = 0; k < 6; k++) { hex8(d[8 + i * 6 + k]); con_putc(' '); }
    }
    con_putc('\n');
}

/* ---------- network ---------- */
static int net_up(void) {
    con_puts("\n Connecting...");
    int r = net_init();
    con_puts("\r              \r");
    if (r == 1) return 1;
    con_puts(r == -1 ? " No supported network card (Intel PRO/1000 or Realtek RTL8139).\n"
             : r == -2 ? " The network cable is unplugged.\n"
                       : " No answer from the network (DHCP). Check the cable and router.\n");
    return 0;
}

static void cmd_net(void) {
    int ok = net_up();
    static char b[512];
    if (ok || net_init() != -1) { con_puts(net_status(b)); con_putc('\n'); }
}

static void cmd_ping(const char *host) {
    if (!*host) { con_puts("\n Usage: PING <host or IP>\n"); return; }
    if (!net_up()) return;
    char err[48];
    for (int i = 0; i < 4; i++) {
        u32 ms = 0;
        con_puts(" ");
        con_puts(host);
        if (net_ping(host, &ms, err)) { con_puts(": reply in "); put_num(ms); con_puts(" ms\n"); }
        else { con_puts(": "); con_puts(err); con_putc('\n'); if (kstricmp(err, "no reply")) return; }
        plat_sleep(400);
    }
}

static void cmd_get(const char *url) {
    if (!*url) { con_puts("\n Usage: GET http://site/file.swf\n"); return; }
    if (!net_up()) return;
    con_puts(" Downloading... (Esc cancels)");
    u8 *data;
    u32 len;
    int status = 0;
    char err[48];
    int ok = net_get(url, &data, &len, &status, err);
    con_puts("\r                              \r");
    if (!ok) { con_puts(" Download failed: "); con_puts(err); con_putc('\n'); return; }
    if (status != 200) { con_puts(" Server answered "); put_num((u32)status); con_puts(" - nothing saved.\n"); return; }
    int swf = len > 8 && (data[0] == 'F' || data[0] == 'C' || data[0] == 'Z') && data[1] == 'W' && data[2] == 'S';
    if (!swf) {
        con_puts(" Received ");
        put_num(len);
        con_puts(" bytes, but it is not a SWF file - not saved:\n");
        int lines = 0, col = 0;
        for (u32 i = 0; i < len && i < 600 && lines < 6; i++) {
            char c = (char)data[i];
            if (c == '\n') { con_putc('\n'); lines++; col = 0; continue; }
            if (c < 32 || c > 126 || col > 70) continue;
            if (!col) con_puts("  ");
            con_putc(c);
            col++;
        }
        con_putc('\n');
        return;
    }
    /* file name = last part of the URL */
    const char *nm = url, *q;
    for (q = url; *q && *q != '?' && *q != '#'; q++) if (*q == '/') nm = q + 1;
    char name[56];
    u32 n = (u32)(q - nm);
    if (n > 50) n = 50;
    for (u32 i = 0; i < n; i++) name[i] = nm[i];
    name[n] = 0;
    if (!n) kstrcpy(name, "download", 56);
    if (!ends_swf(name)) kstrcpy(name + kstrlen(name), ".swf", 6);
    if (!fs_add_download(name, data, len)) { con_puts(" Not enough memory to keep it.\n"); return; }
    con_puts(" Saved ");
    con_puts(name);
    con_puts(" (");
    put_num((len + 1023) / 1024);
    con_puts(" KB). Type RUN ");
    con_puts(name);
    con_puts(" to play it.\n");
}

static void handle(char *line) {
    while (*line == ' ') line++;
    u32 n = kstrlen(line);
    while (n && line[n - 1] == ' ') line[--n] = 0;
    if (!*line) return;
    /* a whole line naming a file, spaces included ("My Game.swf") */
    {
        kfile f;
        if (fs_find(line, &f) || (!ends_swf(line) && resolve(line, &f) && kstrlen(line) > 2)) {
            int has_space = 0;
            for (char *p = line; *p; p++) if (*p == ' ') has_space = 1;
            if (has_space || ends_swf(line)) { cmd_run(line); return; }
        }
    }
    char *arg = line;
    while (*arg && *arg != ' ') arg++;
    if (*arg) { *arg++ = 0; while (*arg == ' ') arg++; }

    if (!kstricmp(line, "help") || !kstricmp(line, "?")) cmd_help();
    else if (!kstricmp(line, "ls") || !kstricmp(line, "dir")) cmd_ls();
    else if (!kstricmp(line, "run") || !kstricmp(line, "play") || !kstricmp(line, "load")) cmd_run(arg);
    else if (!kstricmp(line, "cls") || !kstricmp(line, "clear")) { redraw_header(); }
    else if (!kstricmp(line, "about") || !kstricmp(line, "ver")) cmd_about();
    else if (!kstricmp(line, "mem") || !kstricmp(line, "ram")) cmd_mem();
    else if (!kstricmp(line, "install")) cmd_install();
    else if (!kstricmp(line, "usbdiag")) cmd_usbdiag();
    else if (!kstricmp(line, "net")) cmd_net();
    else if (!kstricmp(line, "ping")) cmd_ping(arg);
    else if (!kstricmp(line, "get") || !kstricmp(line, "download")) cmd_get(arg);
    else if (!kstricmp(line, "reboot")) { con_puts("\n Rebooting...\n"); plat_sleep(400); plat_reboot(); }
    else if (!kstricmp(line, "shutdown") || !kstricmp(line, "poweroff")) {
        con_puts("\n Shutting down...\n");
        plat_sleep(400);
        plat_poweroff();
        con_defer(1); con_clear(); con_defer(0);
        con_puts("\n It is now safe to turn off your computer.");
        plat_halt();
    }
    else if (ends_swf(line)) cmd_run(line);
    else {
        kfile f;
        if (!*arg && resolve(line, &f)) { cmd_run(line); return; }
        con_puts("\n '");
        con_puts(line);
        con_puts("' is not a SWF file or command.\n FlashOS can only run .swf files. Type HELP.\n");
    }
}

#define HIST 32
static char hist[HIST][64];
static int nhist;

static void set_line(char *buf, int *n, const char *src, int max) {
    while (*n) { con_putc('\b'); (*n)--; }
    kstrcpy(buf, src, (u32)max);
    *n = (int)kstrlen(buf);
    con_puts(buf);
}

/* Line editor: Backspace, Enter, Up/Down = command history */
static void read_line(char *buf, int max) {
    int n = 0, on = 1, hi = nhist;
    u32 t = plat_ms();
    buf[0] = 0;
    con_cursor(1);
    for (;;) {
        int k = plat_key();
        if (k < 0) {
            if (plat_ms() - t >= 530) { on = !on; t = plat_ms(); con_cursor(on); }
            continue;
        }
        con_cursor(0);
        if (k == '\n') {
            buf[n] = 0;
            if (n && (!nhist || kstricmp(hist[(nhist - 1) % HIST], buf))) {
                kstrcpy(hist[nhist % HIST], buf, 64);
                nhist++;
            }
            return;
        }
        if (k == KEY_UP) {
            int oldest = nhist > HIST ? nhist - HIST : 0;
            if (hi > oldest) { hi--; set_line(buf, &n, hist[hi % HIST], max); }
        } else if (k == KEY_DOWN) {
            if (hi < nhist) { hi++; set_line(buf, &n, hi < nhist ? hist[hi % HIST] : "", max); }
        } else if (k == '\b') {
            if (n) { n--; buf[n] = 0; con_putc('\b'); }
        } else if (k >= 32 && k < 127 && n < max - 1) {
            buf[n++] = (char)k;
            buf[n] = 0;
            con_putc((char)k);
        }
        on = 1;
        t = plat_ms();
        con_cursor(1);
    }
}

void kmain(void) {
    u32 hsize;
    void *heap = plat_heap(&hsize);
    heap_init(heap, hsize);
    plat_init();
    video_init();
    render_init();
    con_init();
    boot_animation();

    char line[64];
    for (;;) {
        con_puts(fresh_screen ? "\n\n\n\n>" : "\n>");
        fresh_screen = 0;
        read_line(line, sizeof line);
        handle(line);
    }
}
