/* FlashKernel main: boot animation and the FlashOS shell.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 */
#include "kernel.h"

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
             "  LS               list files\n"
             "  RUN <name|#>     play a SWF file (or just type its name)\n"
             "  CLS              clear the screen\n"
             "  ABOUT            about FlashOS\n"
             "  REBOOT / SHUTDOWN\n"
             " While a SWF plays press ESC or F12 to return here.\n");
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
        con_puts(ends_swf(f.name) && is_swf_data(&f) ? "SWF\n" : "---\n");
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
             " Memory free: ");
    put_num(heap_free_bytes() / 1024);
    con_puts(" KB   Files: ");
    put_num((u32)fs_count());
    con_putc('\n');
}

static void handle(char *line) {
    while (*line == ' ') line++;
    u32 n = kstrlen(line);
    while (n && line[n - 1] == ' ') line[--n] = 0;
    if (!*line) return;
    char *arg = line;
    while (*arg && *arg != ' ') arg++;
    if (*arg) { *arg++ = 0; while (*arg == ' ') arg++; }

    if (!kstricmp(line, "help") || !kstricmp(line, "?")) cmd_help();
    else if (!kstricmp(line, "ls") || !kstricmp(line, "dir")) cmd_ls();
    else if (!kstricmp(line, "run") || !kstricmp(line, "play") || !kstricmp(line, "load")) cmd_run(arg);
    else if (!kstricmp(line, "cls") || !kstricmp(line, "clear")) { redraw_header(); }
    else if (!kstricmp(line, "about") || !kstricmp(line, "ver")) cmd_about();
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

static void read_line(char *buf, int max) {
    int n = 0, on = 1;
    u32 t = plat_ms();
    con_cursor(1);
    for (;;) {
        int k = plat_key();
        if (k < 0) {
            if (plat_ms() - t >= 530) { on = !on; t = plat_ms(); con_cursor(on); }
            continue;
        }
        con_cursor(0);
        if (k == '\n') { buf[n] = 0; return; }
        if (k == '\b') { if (n) { n--; con_putc('\b'); } }
        else if (k >= 32 && k < 127 && n < max - 1) { buf[n++] = (char)k; con_putc((char)k); }
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
