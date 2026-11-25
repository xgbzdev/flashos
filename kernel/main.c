/* FlashKernel main: boot animation and the FlashOS shell.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 */
#include "kernel.h"
#include "audio.h"
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
#ifdef BYROTT
    video_present(0, 0, scr_w, scr_h);   /* ByroTT edition: no animation */
    return;
#endif

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
#ifdef BYROTT
/* started from the ByroTT CD, but ByroTT is already on the hard disk:
 * start ByroTT anyway (flags bit 1 marks a ByroTT install) */
static int disk_byrott;
static void find_disk_byrott(void) {
#ifndef HOSTED
    static ata_disk disks[4];
    static u8 sec[1024];
    int n = ata_scan(disks, 4);
    for (int i = 0; i < n; i++) {
        if (!ata_read(&disks[i], 0, sec, 2)) continue;
        const u8 *hd = sec + 512;
        if (sec[510] != 0x55 || sec[511] != 0xAA || (hd[8] | (hd[9] << 8)) != 512) continue;
        if ((hd[38] & 3) == 3) { disk_byrott = 1; return; }
    }
#endif
}
static int installed(void) { return (plat_bootinfo()->flags & 1) || disk_byrott; }
#else
static int installed(void) { return plat_bootinfo()->flags & 1; }
#endif

static void cmd_help(void) {
    con_puts("\n FlashOS commands:\n"
             "  LS               list files (built-in, USB, CD, NET)\n"
             "  RUN <name|#>     play a SWF file (or just type its name)\n"
             "  CLS              clear the screen\n"
             "  ABOUT            about FlashOS\n"
             "  MEM              show installed RAM\n"
             "  RESCAN           read game CDs / ISO images again\n"
             "  VOL [0-100|MUTE] sound volume (F9/F10/F11 while playing)\n"
             "  TIME             check that the clock runs at the right speed\n");
    if (!installed()) con_puts("  INSTALL          install FlashOS on the hard disk\n");
    con_puts(
             "  NET              connect to the wired network\n"
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
        if (!fs_get(i, &f)) continue;
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
        con_puts(f.usb == 1 ? "  USB\n" : f.usb == 2 ? "  NET\n" : f.usb == 3 ? "  CD\n" : "\n");
    }
}

#ifdef BYROTT
static void byr_after_play(void);
static void byrott_loop(void);
#endif
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
#ifdef BYROTT
    byr_after_play(); /* the setup may ask for a restart */
#endif
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

typedef void (*progress_fn)(u32 pct);
static void con_progress(u32 pct) {
    con_puts("\r Installing... ");
    put_num(pct);
    con_puts("%   ");
}
static int write_part(ata_disk *d, u32 lba, const u8 *src, u32 count, u32 *done, u32 total, progress_fn pr) {
    for (u32 i = 0; i < count; i += 16) {
        u32 n = count - i < 16 ? count - i : 16;
        if (!ata_write(d, lba + i, src + i * 512, n)) return 0;
        *done += n;
        pr(*done * 100 / total);
    }
    return 1;
}

/* the system this kernel booted from, copied onto disk d */
static int sys_install(ata_disk *d, progress_fn pr) {
    const u8 *hd = SYS_COPY + 512;
    u32 k_lba = le32(hd + 10), k_cnt = le32(hd + 14), fs_lba = le32(hd + 18), fs_cnt = le32(hd + 22);
    u32 s2_lba = le32(hd + 30), s2_cnt = le32(hd + 34);
    /* mark the hard-disk loader as "installed" (hides INSTALL there) */
    u8 *sys = kmalloc(64 * 512);
    if (!sys) return 0;
    memcpy(sys, SYS_COPY, 64 * 512);
    sys[512 + 38] |= 1;
#ifdef BYROTT
    sys[512 + 38] |= 2;            /* this is ByroTT (a ByroTT CD will start it) */
#endif
    u32 done = 0, total = 64 + k_cnt + s2_cnt + fs_cnt;
    int ok = write_part(d, 0, sys, 64, &done, total, pr) &&
             write_part(d, k_lba, KRN_COPY, k_cnt, &done, total, pr) &&
             write_part(d, s2_lba, S2_COPY, s2_cnt, &done, total, pr) &&
             write_part(d, fs_lba, plat_fs(), fs_cnt, &done, total, pr);
    static u8 check[512];
    if (ok) ok = ata_read(d, 0, check, 1) && !memcmp(check, sys, 512);
    return ok;
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
    u32 need = le32(hd + 18) + le32(hd + 22); /* end of the built-in files */

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
    if (!sys_install(d, con_progress)) { con_puts("\n Disk write failed. FlashOS was not installed.\n"); return; }
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

extern u32 diag_voices, diag_voice_frames, diag_pushed, diag_mixed, diag_pumps, diag_restarts, diag_blocks, diag_mp3frames;
static void cmd_snddiag(void) {
    con_puts("\n blocks="); put_num(diag_blocks);
    con_puts(" mp3frames="); put_num(diag_mp3frames);
    con_puts(" pushed="); put_num(diag_pushed);
    con_puts(" mixed="); put_num(diag_mixed);
    con_puts("\n pumps="); put_num(diag_pumps);
    con_puts(" restarts="); put_num(diag_restarts);
    con_puts(" rate="); put_num((u32)audio_rate());
    con_puts("\n voices="); put_num(diag_voices);
    con_puts(" voice_frames="); put_num(diag_voice_frames);
    con_puts(" master="); put_num((u32)audio_get_master());
    con_puts(audio_get_muted() ? " MUTED" : "");
    con_putc('\n');
}

/* TIME: count 10 seconds on screen to compare with a real clock */
static void cmd_time(void) {
    con_puts("\n Clock: ");
    con_puts(plat_clock_name());
    con_puts("\n Start a stopwatch now. Counting 10 seconds: ");
    u32 t0 = plat_ms();
    for (u32 s = 1; s <= 10; s++) {
        while (plat_ms() - t0 < s * 1000) {
            int k = plat_key();
            if (k == KEY_ESC || k == KEY_CTRL_Q) { con_puts(" stopped\n"); return; }
        }
        put_num(s);
        con_putc(' ');
    }
    con_puts("\n If your stopwatch also shows 10 seconds, FlashOS keeps time correctly.\n");
}

static void cmd_vol(const char *arg) {
    if (*arg) {
        int v = 0;
        for (const char *p = arg; *p >= '0' && *p <= '9'; p++) v = v * 10 + (*p - '0');
        if (!kstricmp(arg, "mute") || !kstricmp(arg, "off")) audio_set_muted(1);
        else { audio_set_muted(0); audio_set_master(v > 100 ? 256 : v * 256 / 100); }
    }
    con_puts("\n Volume: ");
    if (audio_get_muted()) con_puts("muted");
    else { put_num((u32)(audio_get_master() * 100 + 128) / 256); con_putc('%'); }
    con_puts(audio_init() ? "   (sound card: AC'97)" : "   (no AC'97 sound card found)");
    con_puts("\n While a SWF plays: F9 mute, F10 quieter, F11 louder.\n");
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
    else if (!kstricmp(line, "install") && !installed()) cmd_install();
    else if (!kstricmp(line, "usbdiag")) cmd_usbdiag();
    else if (!kstricmp(line, "net")) cmd_net();
    else if (!kstricmp(line, "vol") || !kstricmp(line, "volume")) cmd_vol(arg);
    else if (!kstricmp(line, "snddiag")) cmd_snddiag();
    else if (!kstricmp(line, "time") || !kstricmp(line, "clock")) cmd_time();
#ifdef BYROTT
    else if (!kstricmp(line, "byrott") && installed()) { byrott_loop(); redraw_header(); }
#endif
    else if (!kstricmp(line, "rescan")) {
        con_puts("\n Reading CD drives...");
        int n = cd_scan();
        con_puts("\r ");
        put_num((u32)n);
        con_puts(" file(s) found on CDs / ISO images. Type LS.\n");
    }
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

#ifdef BYROTT
/* ================================================================ ByroTT edition
 * This FlashOS starts ByroTT (a Flash desktop, byrott.swf) by itself once it
 * is installed. It shows the logo quickly, then ByroTT; there is no terminal.
 * ByroTT's setup (byr.swf) and ByroTT may install this system, put the
 * original FlashOS back (flashos.iso, kept in the built-in files), restart
 * and play another SWF in a window. */
extern int quit_requested;
extern int swf_window;            /* player.c: play inside a window */
extern int swf_locked;            /* player.c: F12 / Ctrl+Q do not close it */
extern const char *swf_window_title;
static char launch_arg[256];
static char run_req[64];
u32 byr_win_color = 0xFF1EC8DCu;   /* the window colour picked in ByroTT */
static int power_req;             /* 1 restart, 2 shut down, 3 FlashOS terminal */

const char *byr_launch_arg(void) { return launch_arg; }
int byr_installed(void) { return installed(); }
void byr_run(const char *name, const char *back, u32 color) {
    if (color) byr_win_color = 0xFF000000u | color;
    kstrcpy(run_req, name, sizeof run_req);
    kstrcpy(launch_arg, back, sizeof launch_arg);
    quit_requested = 1;
}
void byr_power(int what) { power_req = what; quit_requested = 1; }

static void byr_after_play(void) {
    if (power_req == 1) plat_reboot();
    if (power_req == 2) plat_poweroff();
}

/* ---- a box drawn by the system itself: a SWF cannot draw or answer it ---- */
static void text_at(int x, int y, const char *s, u32 col, int sc) {
    const u8 *font = plat_font();
    for (; *s; s++, x += 8 * sc) {
        const u8 *g = font + (u8)*s * 16;
        for (int r = 0; r < 16; r++)
            for (int b = 0; b < 8; b++)
                if (g[r] & (0x80 >> b)) video_fill(x + b * sc, y + r * sc, sc, sc, col);
    }
}
#define BX 152
#define BY 244
#define BW 720
#define BH 280
static void sys_box(const char *title, const char *l1, const char *l2, const char *l3) {
    video_fill(BX - 6, BY - 6, BW + 12, BH + 12, 0xFF000000u);
    video_fill(BX - 4, BY - 4, BW + 8, BH + 8, 0xFFFF2020u);
    video_fill(BX, BY, BW, BH, 0xFF000000u);
    text_at(BX + 24, BY + 20, "FlashOS", 0xFFFF2020u, 2);
    text_at(BX + 24, BY + 64, title, 0xFFFFFFFFu, 1);
    text_at(BX + 24, BY + 104, l1, 0xFFE0E0E0u, 1);
    text_at(BX + 24, BY + 128, l2, 0xFFE0E0E0u, 1);
    text_at(BX + 24, BY + 152, l3, 0xFFE0E0E0u, 1);
    video_present(BX - 6, BY - 6, BW + 12, BH + 12);
}
static int sys_confirm(const char *title, const char *l1, const char *l2, const char *l3) {
    sys_box(title, l1, l2, l3);
    text_at(BX + 24, BY + 220, "Press Y to allow, N to cancel.", 0xFFFFD040u, 1);
    video_present(BX, BY + 200, BW, 60);
    plat_flush_input();
    for (;;) {
        int code, down, ascii;
        while (plat_kevent(&code, &down, &ascii)) {
            if (!down) continue;
            if (code == 89) return 1;
            if (code == 78 || code == 27) return 0;
        }
    }
}
static void box_progress(u32 pct) {
    video_fill(BX + 24, BY + 214, BW - 48, 28, 0xFF401010u);
    video_fill(BX + 24, BY + 214, (BW - 48) * (int)pct / 100, 28, 0xFFFF2020u);
    video_present(BX + 24, BY + 214, BW - 48, 28);
}

/* the disk to use: the one FlashOS is on, else the first one */
static ata_disk *pick_disk(u32 need) {
    static ata_disk disks[4];
    static u8 s0[512];
    int n = ata_scan(disks, 4);
    for (int i = 0; i < n; i++)
        if (ata_read(&disks[i], 0, s0, 1) && !memcmp(s0, SYS_COPY, 440) && disks[i].sectors >= need) return &disks[i];
    for (int i = 0; i < n; i++) if (disks[i].sectors >= need) return &disks[i];
    return NULL;
}

/* the hard disks, for ByroTT Setup's drive list: size in MB, 0 = none */
static ata_disk setup_disks[4];
static int setup_ndisks = -1;
int byr_disk_count(void) {
#ifdef HOSTED
    return 0;
#else
    if (setup_ndisks < 0) setup_ndisks = ata_scan(setup_disks, 4);
    return setup_ndisks;
#endif
}
u32 byr_disk_mb(int i) { return i >= 0 && i < byr_disk_count() ? setup_disks[i].sectors / 2048 : 0; }
const char *byr_disk_model(int i) { return i >= 0 && i < byr_disk_count() ? setup_disks[i].model : ""; }

/* "ok", "cancelled" or an error; disk: index in the drive list, -1 = pick one */
const char *byr_install(int disk) {
#ifdef HOSTED
    (void)disk;
    return "no hard disk in the simulator";
#else
    const u8 *hd = SYS_COPY + 512;
    if (SYS_COPY[510] != 0x55 || SYS_COPY[511] != 0xAA || (hd[8] | (hd[9] << 8)) != 512)
        return "start the computer from the ByroTT CD to install";
    u32 need = le32(hd + 18) + le32(hd + 22);
    ata_disk *d;
    if (disk >= 0) {
        if (disk >= byr_disk_count()) return "that drive is not there";
        d = &setup_disks[disk];
        if (d->sectors < need) return "that drive is too small";
    } else d = pick_disk(need);
    if (!d) return "no IDE hard disk found";
    if (!sys_confirm("ByroTT Setup wants to install ByroTT on the hard disk.",
                     "The computer will start ByroTT from now on.",
                     "Your saved documents are kept.",
                     "You can uninstall it later in ByroTT's Settings.")) return "cancelled";
    sys_box("INSTALLING PLEASE DO NOT SHUT DOWN", "IT CAN POTENTIALLY BRICK YOUR COMPUTER!", "", "");
    return sys_install(d, box_progress) ? "ok" : "disk write failed";
#endif
}

const char *byr_uninstall(void) {
#ifdef HOSTED
    return "no hard disk in the simulator";
#else
    kfile f;
    if (!fs_find("flashos.iso", &f) || f.size < 4096) return "the FlashOS image is missing";
    const u8 *hd = f.data + 512;
    if ((hd[8] | (hd[9] << 8)) != 512) return "the FlashOS image is damaged";
    u32 need = le32(hd + 18) + le32(hd + 22);          /* sectors up to the end of its files */
    if (need * 512 > f.size + 2047) return "the FlashOS image is damaged";
    ata_disk *d = pick_disk(need);
    if (!d) return "no IDE hard disk found";
    if (!sys_confirm("ByroTT wants to uninstall itself.",
                     "The original FlashOS will be put back on the hard disk.",
                     "Your saved documents are kept.", "")) return "cancelled";
    sys_box("Deleting... PLEASE DO NOT SHUT DOWN!", "Putting FlashOS back.", "", "");
    static u8 sec[512 * 16];
    for (u32 i = 0; i < need; i += 16) {
        u32 n = need - i < 16 ? need - i : 16;
        memset(sec, 0, sizeof sec);
        u32 from = i * 512, bytes = n * 512;
        if (from + bytes > f.size) bytes = f.size > from ? f.size - from : 0;
        memcpy(sec, f.data + from, bytes);
        if (i == 0) sec[512 + 38] |= 1;                 /* installed: hides INSTALL */
        if (!ata_write(d, i, sec, n)) return "disk write failed";
        box_progress((i + n) * 100 / need);
    }
    return "ok";
#endif
}

/* SWFs saved on the hard disk are read into one reusable buffer */
static u8 *runbuf;
static u32 runcap;
static void *run_alloc(u32 n) {
    if (n > runcap) { runbuf = kmalloc(n); runcap = runbuf ? n : 0; }
    return runbuf;
}

static void play_windowed(const char *name) {
    kfile f;
    const u8 *data = NULL;
    u32 size = 0;
    if (installed() && !kstricmp(name, "byr.swf")) data = NULL;
    else if (fs_find(name, &f)) { data = f.data; size = f.size; }
    else data = docs_read(name, &size, run_alloc);
    if (!data || size < 8 || !((data[0] == 'F' || data[0] == 'C' || data[0] == 'Z') && data[1] == 'W' && data[2] == 'S')) {
        kstrcpy(launch_arg + kstrlen(launch_arg), "|not a SWF file", 64);
        return;
    }
    const char *err = NULL;
    swf_window = 1;
    swf_window_title = !kstricmp(name, "ballgame.swf") ? "Ball Game v2.0" : name;
    swf_play(data, size, &err);
    swf_window = 0;
    if (err) {
        u32 l = kstrlen(launch_arg);
        kstrcpy(launch_arg + l, "|", 2);
        kstrcpy(launch_arg + l + 1, err, sizeof launch_arg - l - 1);
    }
}

/* ByroTT, for ever; if it is closed (F12) it starts again */
static void byrott_loop(void) {
    launch_arg[0] = 0;
    for (;;) {
        kfile f;
        if (!fs_find("byrott.swf", &f)) return;
        run_req[0] = 0;
        power_req = 0;
        const char *err = NULL;
        swf_locked = 1;
        swf_play(f.data, f.size, &err);
        swf_locked = 0;
        byr_after_play();
        if (run_req[0]) { play_windowed(run_req); continue; }
        launch_arg[0] = 0;
    }
}

/* ByroTT is installed but its files are gone (the mock-up's error screen) */
static void byrott_missing(void) {
    for (;;) {
        video_fill(0, 0, scr_w, scr_h, 0xFF000000u);
        int x = scr_w / 2 - 300, y = scr_h / 2 - 80;
        text_at(x, y, "Byrott Is Not Found Or Installation", 0xFFFFFFFFu, 2);
        text_at(x, y + 34, "Gone Wrong", 0xFFFFFFFFu, 2);
        text_at(x, y + 68, "Please Re-Install it Again", 0xFFFFFFFFu, 2);
        text_at(x + 50, y + 102, "Press Space for quick uninstall", 0xFFFFFFFFu, 2);
        video_present(0, 0, scr_w, scr_h);
        plat_flush_input();
        int code = 0, down, ascii;
        for (;;) if (plat_kevent(&code, &down, &ascii) && down && (code == 32 || code == 27)) break;
        if (code == 27) return;                 /* Esc: the FlashOS terminal */
        const char *r = byr_uninstall();
        if (!kstricmp(r, "ok")) {
            text_at(x, y + 160, "Installing... PLEASE DO NOT SHUT DOWN!!", 0xFFFFFFFFu, 2);
            video_present(0, 0, scr_w, scr_h);
            plat_sleep(1500);
            plat_reboot();
        }
    }
}

/* the FlashOS logo, shown at once (no animation), with no welcome line */
static void fast_logo(void) {
    con_defer(1);
    con_clear();
    for (int i = 0; i < 4; i++) { con_puts(LOGO[i]); con_putc('\n'); }
    con_defer(0);
    video_present(0, 0, scr_w, scr_h);
    plat_sleep(600);
    plat_flush_input();
}
#endif

void kmain(void) {
    u32 hsize;
    void *heap = plat_heap(&hsize);
    heap_init(heap, hsize);
    plat_init();
    video_init();
    render_init();
    cd_scan(); /* game CDs / ISO images in the CD drives */
    con_init();
#ifdef BYROTT
    if (!(plat_bootinfo()->flags & 1)) find_disk_byrott();
    if (installed()) {
        fast_logo();
        byrott_loop();            /* only returns if byrott.swf is missing */
        byrott_missing();
        redraw_header();
    } else {
        boot_animation();
    }
#else
    boot_animation();
#endif

    char line[64];
    for (;;) {
        con_puts(fresh_screen ? "\n\n\n\n>" : "\n>");
        fresh_screen = 0;
        read_line(line, sizeof line);
        handle(line);
    }
}
