/*
 * flashsh - the FlashOS shell.
 *
 * Runs on the Linux text console (tty1) as the only user program.
 * - Boot animation: the FlashOS logo is "loaded" top-to-bottom, scanline by
 *   scanline, like an old PC slowly drawing an image off a disk / modem.
 * - Afterwards it shows the red prompt and accepts commands.
 * - The only thing it will execute is a .swf file (handed to Ruffle via
 *   /usr/bin/flash-run). Everything else is refused.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define VERSION "1.0"
#define MAX_SWF 256

static const char *LOGO[] = {
    " ___ _         _    ___  ___ ",
    "| __| |__ _ __| |_ / _ \\/ __|",
    "| _|| / _` (_-< ' \\ (_) \\__ \\",
    "|_| |_\\__,_/__/_||_\\___/|___/",
};
#define LOGO_ROWS (int)(sizeof(LOGO) / sizeof(LOGO[0]))

static const char *WELCOME = "Welcome to FlashOS,Insert An Command,";

/* Directories searched for .swf files (colon separated, overridable). */
static const char *swf_dirs(void) {
    const char *e = getenv("FLASHOS_SWF_DIRS");
    return e ? e : "/swf:/media";
}
static const char *runner(void) {
    const char *e = getenv("FLASHOS_RUNNER");
    return e ? e : "/usr/bin/flash-run";
}

/* ---------- terminal helpers ---------- */
static void out(const char *s) { fputs(s, stdout); }
static void msleep(int ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}
static void goto_rc(int row, int col) { printf("\033[%d;%dH", row, col); }

static void set_colors(void) {
    const char *t = getenv("TERM");
    /* On the real Linux console, retune palette slot 1 to pure "FlashOS red". */
    if (!t || !strcmp(t, "linux")) out("\033]P1FF0000\033]P9FF2020");
    out("\033[0;31;40m");
}

/* ---------- boot animation ---------- */
static void progress(int row, int pct) {
    int width = 20, fill = pct * width / 100;
    goto_rc(row, 2);
    out("LOADING FLASHOS.IMG [");
    for (int i = 0; i < width; i++) out(i < fill ? "#" : ".");
    printf("] %3d%%", pct);
}

static void draw_header(int animate) {
    out("\033[2J\033[H");
    if (!animate) {
        for (int r = 0; r < LOGO_ROWS; r++) { goto_rc(r + 2, 2); out(LOGO[r]); }
        goto_rc(LOGO_ROWS + 2, 2);
        out(WELCOME);
        return;
    }

    out("\033[?25l"); /* hide cursor during the load */
    int status_row = LOGO_ROWS + 6;
    int total = 0, done = 0;
    for (int r = 0; r < LOGO_ROWS; r++) total += (int)strlen(LOGO[r]);

    progress(status_row, 0);
    msleep(500);

    /* Old-PC image load: each "scanline" (logo row) is painted left to
     * right behind a solid beam, top to bottom, with a short pause
     * between lines as if the next chunk were still coming off the disk. */
    for (int r = 0; r < LOGO_ROWS; r++) {
        int len = (int)strlen(LOGO[r]);
        /* pre-fill the line with a dim "not loaded yet" band */
        goto_rc(r + 2, 2);
        for (int c = 0; c < len; c++) out("\xe2\x96\x91"); /* ░ */
        msleep(90);
        for (int c = 0; c < len; c++) {
            goto_rc(r + 2, 2 + c);
            putchar(LOGO[r][c]);
            if (c + 1 < len) out("\xe2\x96\x88"); /* █ beam */
            done++;
            if (c % 3 == 0) progress(status_row, done * 100 / total);
            fflush(stdout);
            msleep(14);
        }
        progress(status_row, done * 100 / total);
        msleep(160);
    }
    progress(status_row, 100);
    msleep(350);

    /* flicker once like a CRT settling, then clear the status line */
    goto_rc(status_row, 1); out("\033[2K");
    for (int i = 0; i < 2; i++) {
        for (int r = 0; r < LOGO_ROWS; r++) { goto_rc(r + 2, 1); out("\033[2K"); }
        msleep(70);
        for (int r = 0; r < LOGO_ROWS; r++) { goto_rc(r + 2, 2); out(LOGO[r]); }
        msleep(90);
    }

    /* type the welcome line */
    goto_rc(LOGO_ROWS + 2, 2);
    for (const char *p = WELCOME; *p; p++) { putchar(*p); fflush(stdout); msleep(22); }
    out("\033[?25h");
}

/* ---------- SWF helpers ---------- */
static int ends_with_swf(const char *s) {
    size_t n = strlen(s);
    return n > 4 && !strcasecmp(s + n - 4, ".swf");
}

/* Real SWF files start with FWS (plain), CWS (zlib) or ZWS (LZMA). */
static int is_swf_file(const char *path) {
    struct stat st;
    if (stat(path, &st) || !S_ISREG(st.st_mode)) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char m[3] = {0};
    size_t n = fread(m, 1, 3, f);
    fclose(f);
    return n == 3 && (m[0] == 'F' || m[0] == 'C' || m[0] == 'Z') && m[1] == 'W' && m[2] == 'S';
}

static char *found[MAX_SWF];
static int nfound;

static void scan_dir(const char *dir, int depth) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && nfound < MAX_SWF) {
        if (e->d_name[0] == '.') continue;
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st)) continue;
        if (S_ISDIR(st.st_mode) && depth < 2) scan_dir(p, depth + 1);
        else if (S_ISREG(st.st_mode) && ends_with_swf(e->d_name)) found[nfound++] = strdup(p);
    }
    closedir(d);
}

static int cmp_str(const void *a, const void *b) {
    return strcasecmp(*(char *const *)a, *(char *const *)b);
}

static void rescan(void) {
    for (int i = 0; i < nfound; i++) free(found[i]);
    nfound = 0;
    char *dirs = strdup(swf_dirs()), *save = NULL;
    for (char *t = strtok_r(dirs, ":", &save); t; t = strtok_r(NULL, ":", &save)) scan_dir(t, 0);
    free(dirs);
    qsort(found, nfound, sizeof *found, cmp_str);
}

static const char *base(const char *p) { const char *s = strrchr(p, '/'); return s ? s + 1 : p; }

/* Resolve a user argument to a path: number from LS, absolute path, or name. */
static const char *resolve(const char *arg) {
    static char buf[1024];
    char *endp;
    long idx = strtol(arg, &endp, 10);
    if (*arg && !*endp) return (idx >= 1 && idx <= nfound) ? found[idx - 1] : NULL;
    if (arg[0] == '/') { snprintf(buf, sizeof buf, "%s", arg); return access(buf, R_OK) ? NULL : buf; }
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < nfound; i++) {
            const char *b = base(found[i]);
            if (!strcasecmp(b, arg)) return found[i];
            if (pass && !ends_with_swf(arg) && !strncasecmp(b, arg, strlen(arg)) &&
                strlen(b) == strlen(arg) + 4) return found[i];
        }
        rescan();
    }
    return NULL;
}

/* ---------- commands ---------- */
static void cmd_help(void) {
    out("\n FlashOS commands:\n"
        "  LS               list SWF files (in /swf and USB drives)\n"
        "  RUN <name|#>     play a SWF file (or just type its name)\n"
        "  USB              rescan USB drives for SWF files\n"
        "  CLS              clear the screen\n"
        "  ABOUT            about FlashOS\n"
        "  REBOOT / SHUTDOWN\n"
        " While a SWF is playing press F12 or Ctrl+Q to return here.\n");
}

static void cmd_ls(void) {
    rescan();
    if (!nfound) { out("\n No SWF files found. Copy .swf files to a USB drive and type USB.\n"); return; }
    out("\n");
    for (int i = 0; i < nfound; i++) {
        struct stat st; stat(found[i], &st);
        printf(" %2d. %-34s %7lld KB  %s\n", i + 1, base(found[i]),
               (long long)(st.st_size + 1023) / 1024, found[i]);
    }
}

static void cmd_run(const char *arg) {
    if (!*arg) { out("\n Usage: RUN <file.swf>\n"); return; }
    const char *path = resolve(arg);
    if (!path) { printf("\n File not found: %s   (type LS)\n", arg); return; }
    if (!ends_with_swf(path) || !is_swf_file(path)) {
        printf("\n ACCESS DENIED: %s is not a SWF file.\n FlashOS can only run .swf files.\n", base(path));
        return;
    }
    printf("\n Loading %s ...", base(path));
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        execl(runner(), runner(), path, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    if (pid > 0) waitpid(pid, &status, 0);
    set_colors();
    draw_header(0);
    if (pid < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        printf("\n\n The SWF player stopped with an error (see /tmp/ruffle.log).");
}

static void cmd_usb(void) {
    out("\n Scanning USB drives...");
    fflush(stdout);
    if (access("/usr/bin/flash-mount", X_OK) == 0) {
        int rc = system("/usr/bin/flash-mount >/dev/null 2>&1");
        (void)rc;
    }
    rescan();
    printf(" %d SWF file(s) available.\n", nfound);
}

static void cmd_about(void) {
    printf("\n FlashOS %s - a tiny Linux that only plays Flash.\n"
           " Kernel: Linux | Userland: BusyBox | Player: Ruffle (ruffle.rs)\n", VERSION);
}

static void power(const char *what) {
    printf("\n %s...\n", !strcmp(what, "reboot") ? "Rebooting" : "Shutting down");
    fflush(stdout);
    sync();
    execlp(what, what, (char *)NULL);
    out(" Not allowed here.\n");
}

static void handle(char *line) {
    while (isspace((unsigned char)*line)) line++;
    char *end = line + strlen(line);
    while (end > line && isspace((unsigned char)end[-1])) *--end = 0;
    if (!*line) return;

    char *arg = line;
    while (*arg && !isspace((unsigned char)*arg)) arg++;
    if (*arg) { *arg++ = 0; while (isspace((unsigned char)*arg)) arg++; }

    if (!strcasecmp(line, "help") || !strcmp(line, "?")) cmd_help();
    else if (!strcasecmp(line, "ls") || !strcasecmp(line, "dir")) cmd_ls();
    else if (!strcasecmp(line, "run") || !strcasecmp(line, "play") || !strcasecmp(line, "load")) cmd_run(arg);
    else if (!strcasecmp(line, "cls") || !strcasecmp(line, "clear")) { draw_header(0); }
    else if (!strcasecmp(line, "usb") || !strcasecmp(line, "mount")) cmd_usb();
    else if (!strcasecmp(line, "about") || !strcasecmp(line, "ver")) cmd_about();
    else if (!strcasecmp(line, "reboot")) power("reboot");
    else if (!strcasecmp(line, "shutdown") || !strcasecmp(line, "poweroff")) power("poweroff");
    else if (ends_with_swf(line)) cmd_run(line);
    else if (!*arg && resolve(line)) cmd_run(line);
    else printf("\n '%s' is not a SWF file or command.\n FlashOS can only run .swf files. Type HELP.\n", line);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    int animate = !(argc > 1 && !strcmp(argv[1], "--no-anim"));
    set_colors();
    draw_header(animate);
    rescan();

    char line[512];
    for (;;) {
        out("\n\n\n > ");
        if (!fgets(line, sizeof line, stdin)) {
            if (feof(stdin)) return 0; /* init respawns us */
            clearerr(stdin);
            continue;
        }
        handle(line);
    }
}
