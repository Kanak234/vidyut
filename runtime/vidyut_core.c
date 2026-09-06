/* vidyut_core.c - framebuffer, state and the wire protocol.
 * Part of VIDYUT. MIT licence.
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "vidyut_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <sys/time.h>
  #include <unistd.h>
#endif

VState vg;

/* Default EGA/VGA palette, index -> RGB. */
static const unsigned char kDefaultRGB[16][3] = {
    {  0,  0,  0}, {  0,  0,168}, {  0,168,  0}, {  0,168,168},
    {168,  0,  0}, {168,  0,168}, {168, 84,  0}, {168,168,168},
    { 84, 84, 84}, { 84, 84,252}, { 84,252, 84}, { 84,252,252},
    {252, 84, 84}, {252, 84,252}, {252,252, 84}, {252,252,252}
};

/* ------------------------------------------------------------------ */
/* clock                                                              */
/* ------------------------------------------------------------------ */

long vg_now_ms(void)
{
#if defined(_WIN32)
    return (long)GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long)(tv.tv_sec * 1000L + tv.tv_usec / 1000L);
#endif
}

void vg_sleep_ms(unsigned ms)
{
#if defined(_WIN32)
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec  = ms / 1000u;
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

/* ------------------------------------------------------------------ */
/* protocol                                                           */
/* ------------------------------------------------------------------ */

static void emit_open(void)
{
    const char *path;
    if (vg.out) return;
    path = getenv("VIDYUT_OUT");
    if (path && *path) {
        vg.out = fopen(path, "wb");
        vg.out_is_stdout = 0;
    }
    if (!vg.out) {
        vg.out = stdout;
        vg.out_is_stdout = 1;
    }
}

/* Every line the panel reads is prefixed so it can be told apart from the
 * program's own stdout when the two share a stream. */
static void emit_line(const char *body)
{
    emit_open();
    fputs("@vidyut ", vg.out);
    fputs(body, vg.out);
    fputc('\n', vg.out);
    fflush(vg.out);
}

void vg_emit_raw(const char *body) { emit_line(body); }

static void emit_palette(void)
{
    char buf[512];
    int n = 0, i;
    n += snprintf(buf + n, sizeof buf - n, "{\"t\":\"pal\",\"rgb\":[");
    for (i = 0; i < 16; i++) {
        n += snprintf(buf + n, sizeof buf - n, "%s%d,%d,%d",
                      i ? "," : "",
                      vg.pal[i][0], vg.pal[i][1], vg.pal[i][2]);
    }
    snprintf(buf + n, sizeof buf - n, "]}");
    emit_line(buf);
}

void vg_emit_init(void)
{
    char buf[128];
    snprintf(buf, sizeof buf,
             "{\"t\":\"init\",\"w\":%d,\"h\":%d}", vg.w, vg.h);
    emit_line(buf);
    emit_palette();
}

void vg_emit_status(const char *state)
{
    char buf[128];
    snprintf(buf, sizeof buf, "{\"t\":\"status\",\"s\":\"%s\"}", state);
    emit_line(buf);
}

/* Run-length encode the framebuffer as hex: 4 digits count, 2 digits index. */
void vg_flush(void)
{
    long total, i;
    size_t cap, len;
    char *hex;
    static const char *H = "0123456789abcdef";

    if (!vg.active || !vg.dirty) return;

    total = (long)vg.w * vg.h;
    cap   = (size_t)total * 6 + 64;   /* worst case: no run ever repeats */
    hex   = (char *)malloc(cap);
    if (!hex) return;

    len = 0;
    i   = 0;
    while (i < total) {
        unsigned char v = vg.fb[i];
        long run = 1;
        while (i + run < total && vg.fb[i + run] == v && run < 0xFFFF) run++;
        hex[len++] = H[(run >> 12) & 0xF];
        hex[len++] = H[(run >>  8) & 0xF];
        hex[len++] = H[(run >>  4) & 0xF];
        hex[len++] = H[ run        & 0xF];
        hex[len++] = H[(v >> 4) & 0xF];
        hex[len++] = H[ v       & 0xF];
        i += run;
    }
    hex[len] = '\0';

    emit_open();
    fputs("@vidyut {\"t\":\"frame\",\"rle\":\"", vg.out);
    fwrite(hex, 1, len, vg.out);
    fputs("\"}\n", vg.out);
    fflush(vg.out);

    free(hex);
    vg.dirty        = 0;
    vg.last_flush_ms = vg_now_ms();
}

/* Called after each drawing primitive: keeps long draw loops animating
 * without flooding the channel. */
void vg_tick(void)
{
    long now;
    if (!vg.active || !vg.dirty) return;
    now = vg_now_ms();
    if (now - vg.last_flush_ms >= VIDYUT_AUTOFLUSH_MS) vg_flush();
}

/* ------------------------------------------------------------------ */
/* lifecycle                                                          */
/* ------------------------------------------------------------------ */

static void reset_settings(void)
{
    vg.color      = WHITE;
    vg.bkcolor    = BLACK;
    vg.fill.pattern = SOLID_FILL;
    vg.fill.color   = WHITE;
    vg.line.linestyle = SOLID_LINE;
    vg.line.upattern  = 0xFFFF;
    vg.line.thickness = NORM_WIDTH;
    vg.writemode  = COPY_PUT;
    vg.cpx = vg.cpy = 0;
    vg.text.font      = DEFAULT_FONT;
    vg.text.direction = HORIZ_DIR;
    vg.text.charsize  = 1;
    vg.text.horiz     = LEFT_TEXT;
    vg.text.vert      = TOP_TEXT;
    vg.usr_multx = vg.usr_divx = vg.usr_multy = vg.usr_divy = 1;
    vg.vp.left = 0; vg.vp.top = 0;
    vg.vp.right = vg.w - 1; vg.vp.bottom = vg.h - 1;
    vg.vp.clip = 1;
    memset(vg.userfill, 0xFF, sizeof vg.userfill);
}

void vg_ensure_init(void)
{
    if (vg.active) return;
    /* A program that draws without calling initgraph still gets a screen,
     * which is friendlier than silently doing nothing. */
    vg_open_screen(VIDYUT_DEF_W, VIDYUT_DEF_H);
}

void vg_open_screen(int w, int h)
{
    int i;
    if (w <= 0) w = VIDYUT_DEF_W;
    if (h <= 0) h = VIDYUT_DEF_H;

    if (vg.fb) free(vg.fb);
    vg.w  = w;
    vg.h  = h;
    vg.fb = (unsigned char *)calloc((size_t)w * h, 1);
    if (!vg.fb) {
        vg.result = grNoInitGraph;
        return;
    }
    for (i = 0; i < 16; i++) {
        vg.pal[i][0] = kDefaultRGB[i][0];
        vg.pal[i][1] = kDefaultRGB[i][1];
        vg.pal[i][2] = kDefaultRGB[i][2];
        vg.palmap[i] = (unsigned char)i;
    }
    vg.active = 1;
    vg.result = grOk;
    reset_settings();
    vg.dirty = 1;
    vg.last_flush_ms = 0;
    vg_emit_init();
    vg_flush();
}

void vg_close_screen(void)
{
    if (!vg.active) return;
    vg_flush();
    vg.active = 0;
    free(vg.fb);
    vg.fb = NULL;
}

/* Registered with atexit so the last picture always reaches the panel,
 * even when the program never calls closegraph. */
void vg_atexit(void)
{
    if (vg.active) vg_flush();
    if (vg.out) {
        emit_line("{\"t\":\"end\"}");
        if (!vg.out_is_stdout) fclose(vg.out);
        vg.out = NULL;
    }
    vg_input_restore();
}

void graphdefaults(void)
{
    vg_ensure_init();
    reset_settings();
}

/* ------------------------------------------------------------------ */
/* palette                                                            */
/* ------------------------------------------------------------------ */

void setpalette(int colornum, int color)
{
    if (colornum < 0 || colornum > 15) { vg.result = grError; return; }
    vg.palmap[colornum] = (unsigned char)(color & 15);
    vg.pal[colornum][0] = kDefaultRGB[color & 15][0];
    vg.pal[colornum][1] = kDefaultRGB[color & 15][1];
    vg.pal[colornum][2] = kDefaultRGB[color & 15][2];
    emit_palette();
}

void setrgbpalette(int colornum, int r, int g, int b)
{
    if (colornum < 0 || colornum > 15) { vg.result = grError; return; }
    /* Turbo C takes 6-bit DAC values; scale to 8-bit. */
    vg.pal[colornum][0] = (unsigned char)((r & 63) * 255 / 63);
    vg.pal[colornum][1] = (unsigned char)((g & 63) * 255 / 63);
    vg.pal[colornum][2] = (unsigned char)((b & 63) * 255 / 63);
    emit_palette();
}

void setallpalette(struct palettetype *palette)
{
    int i;
    if (!palette) return;
    for (i = 0; i < palette->size && i < 16; i++)
        setpalette(i, palette->colors[i]);
}

void getpalette(struct palettetype *palette)
{
    int i;
    if (!palette) return;
    palette->size = 16;
    for (i = 0; i < 16; i++) palette->colors[i] = (signed char)vg.palmap[i];
}

int getpalettesize(void) { return 16; }

/* ------------------------------------------------------------------ */
/* trivial accessors                                                  */
/* ------------------------------------------------------------------ */

int  getmaxx(void)     { vg_ensure_init(); return vg.w - 1; }
int  getmaxy(void)     { vg_ensure_init(); return vg.h - 1; }
int  getmaxcolor(void) { return MAXCOLORS; }
int  getmaxmode(void)  { return 2; }
int  getcolor(void)    { return vg.color; }
int  getbkcolor(void)  { return vg.bkcolor; }
int  getx(void)        { return vg.cpx; }
int  gety(void)        { return vg.cpy; }

void setcolor(int color) { vg_ensure_init(); vg.color = color & 15; }

void setbkcolor(int color)
{
    vg_ensure_init();
    vg.bkcolor = color & 15;
    /* Turbo C repaints the whole background immediately. */
    memset(vg.fb, vg.bkcolor, (size_t)vg.w * vg.h);
    vg.dirty = 1;
    vg_flush();
}

void setwritemode(int mode) { vg.writemode = mode; }

void moveto(int x, int y)   { vg.cpx = x; vg.cpy = y; }
void moverel(int dx, int dy){ vg.cpx += dx; vg.cpy += dy; }

int  graphresult(void) { int r = vg.result; vg.result = grOk; return r; }

char *grapherrormsg(int errorcode)
{
    switch (errorcode) {
        case grOk:            return (char *)"No error";
        case grNoInitGraph:   return (char *)"Graphics not installed";
        case grNotDetected:   return (char *)"Graphics hardware not detected";
        case grFileNotFound:  return (char *)"Device driver file not found";
        case grInvalidDriver: return (char *)"Invalid device driver file";
        case grNoLoadMem:     return (char *)"Not enough memory to load driver";
        case grNoScanMem:     return (char *)"Out of memory in scan fill";
        case grNoFloodMem:    return (char *)"Out of memory in flood fill";
        case grFontNotFound:  return (char *)"Font file not found";
        case grNoFontMem:     return (char *)"Not enough memory to load font";
        case grInvalidMode:   return (char *)"Invalid graphics mode";
        case grIOerror:       return (char *)"Graphics I/O error";
        case grInvalidFont:   return (char *)"Invalid font file";
        case grInvalidFontNum:return (char *)"Invalid font number";
        default:              return (char *)"Graphics error";
    }
}

const char *getdrivername(void) { return "VIDYUT VGA"; }
const char *getmodename(int mode) { (void)mode; return "640x480 VGA"; }

void detectgraph(int *gd, int *gm)
{
    if (gd) *gd = VGA;
    if (gm) *gm = VGAHI;
}

void initgraph(int *gd, int *gm, const char *path)
{
    int w = VIDYUT_DEF_W, h = VIDYUT_DEF_H;
    (void)path;
    if (gd && *gd == DETECT) { *gd = VGA; if (gm) *gm = VGAHI; }
    if (gd && gm) {
        /* Honour the classic sizes so old code lays out as it expects. */
        if (*gd == CGA)                  { w = 320; h = 200; }
        else if (*gd == EGA && *gm == EGALO) { w = 640; h = 200; }
        else if (*gd == EGA)             { w = 640; h = 350; }
        else if (*gd == VGA && *gm == VGALO)  { w = 640; h = 200; }
        else if (*gd == VGA && *gm == VGAMED) { w = 640; h = 350; }
    }
    vg_open_screen(w, h);
    vg_input_init();
    vg_emit_status("running");
}

void closegraph(void)     { vg_close_screen(); }
void restorecrtmode(void) { vg_flush(); }
void setgraphmode(int m)  { (void)m; vg_ensure_init(); cleardevice(); }
int  getgraphmode(void)   { return VGAHI; }

void setactivepage(int p) { (void)p; }
void setvisualpage(int p) { (void)p; }

int registerbgidriver(void *d) { (void)d; return 0; }
int registerbgifont(void *f)   { (void)f; return 0; }
int installuserdriver(const char *n, int (*d)(void)) { (void)n; (void)d; return 0; }
int installuserfont(const char *n) { (void)n; return 0; }

void vidyut_flush(void) { vg.dirty = 1; vg_flush(); }
