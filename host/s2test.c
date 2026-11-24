/* Host test for boot/stage2.c: scan disk images like the BIOS would and
 * write the resulting USB FlashFS archive to a file.
 * Usage: s2test OUT.bin DISK.img [DISK2.img ...]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u8;
typedef unsigned int u32;
extern u8 *host_mem;
extern FILE *host_disks[4];
extern int host_ndisks;
void host_s2_main(void);

int main(int argc, char **argv) {
    u32 memtop = 128u << 20;
    host_mem = calloc(memtop, 1);
    for (int i = 2; i < argc && host_ndisks < 4; i++) host_disks[host_ndisks++] = fopen(argv[i], "rb");
    u32 *bi = (u32 *)(host_mem + 0x6000);
    bi[3] = 8192;   /* built-in FlashFS size */
    bi[4] = memtop;
    host_s2_main();
    if (!bi[5]) { printf("no SWF files found\n"); return 1; }
    u8 *a = host_mem + bi[5];
    u32 n = a[4] | (a[5] << 8);
    printf("archive at 0x%08x, %u bytes, %u files:\n", bi[5], bi[6], n);
    for (u32 i = 0; i < n; i++) {
        u8 *e = a + 8 + i * 64;
        u32 off = e[56] | (e[57] << 8) | (e[58] << 16) | (e[59] << 24);
        u32 sz = e[60] | (e[61] << 8) | (e[62] << 16) | (e[63] << 24);
        printf("  %-30s %7u bytes  starts %.3s\n", (char *)e, sz, a + off);
    }
    FILE *o = fopen(argv[1], "wb");
    fwrite(a, 1, bi[6], o);
    fclose(o);
    return 0;
}
