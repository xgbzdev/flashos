/* host test: decode every video frame of an FLV to raw yuv420p */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define HOSTED 1
#include "../kernel/flv.c"
void *amalloc(u32 n) { return calloc(1, n); }
u32 plat_ms(void) { return 0; }
void audio_stream_mp3(const u8 *d, u32 n) { (void)d; (void)n; }
void audio_stream_mp3_reset(void) {}
void audio_stream_push(const s16 *p, u32 f, int r) { (void)p; (void)f; (void)r; }
u32 audio_stream_queued(void) { return 0; }
s16 *audio_decode(int a, int b, int c, int d, u32 e, const u8 *f, u32 g, u32 *h, int *i) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i; return 0; }
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    static u8 buf[64 << 20];
    u32 n = (u32)fread(buf, 1, sizeof buf, f);
    fclose(f);
    flvvid *v = flv_open(buf, n);
    if (!v) { printf("open failed\n"); return 1; }
    FILE *o = fopen(argv[2], "wb");
    int frames = 0, bad = 0;
    v->dec.have_ref = 0;
    for (int i = 0; i < v->ntags; i++) {
        if (v->tags[i].type != 9) continue;
        const u8 *p = v->data + v->tags[i].off;
        int disp = 0;
        int r = h263_decode(&v->dec, p + 1, v->tags[i].size - 1, &disp);
        if (!r) { bad++; continue; }
        h263 *d = &v->dec;
        for (int y = 0; y < d->h; y++) fwrite(d->buf[r - 1][0] + y * d->ys, 1, d->w, o);
        for (int c = 1; c < 3; c++) for (int y = 0; y < d->h / 2; y++) fwrite(d->buf[r - 1][c] + y * d->cs, 1, d->w / 2, o);
        frames++;
    }
    fclose(o);
    printf("%dx%d frames %d errors %d duration %u ms\n", v->w, v->h, frames, bad, v->duration);
    return 0;
}
