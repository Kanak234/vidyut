/* vidyut_internal.h - shared state for the VIDYUT runtime.
 * Not intended for user code. MIT licence.
 */
#ifndef VIDYUT_INTERNAL_H
#define VIDYUT_INTERNAL_H

#include "graphics.h"
#include <stdio.h>

#define VIDYUT_DEF_W 640
#define VIDYUT_DEF_H 480
#define VIDYUT_AUTOFLUSH_MS 40

typedef struct {
    int  active;
    int  w, h;
    unsigned char *fb;

    unsigned char pal[16][3];
    unsigned char palmap[16];

    int color, bkcolor;
    int writemode;
    int cpx, cpy;
    int result;

    struct fillsettingstype fill;
    struct linesettingstype line;
    struct textsettingstype text;
    struct viewporttype     vp;
    struct arccoordstype    arcco;

    unsigned char userfill[8];
    int usr_multx, usr_divx, usr_multy, usr_divy;

    int  dirty;
    long last_flush_ms;

    FILE *out;
    int   out_is_stdout;
    int   input_ready;
} VState;

extern VState vg;

/* core */
void vg_ensure_init(void);
void vg_open_screen(int w, int h);
void vg_close_screen(void);
void vg_flush(void);
void vg_tick(void);
void vg_atexit(void);
void vg_emit_init(void);
void vg_emit_status(const char *state);
void vg_emit_raw(const char *body);
long vg_now_ms(void);
void vg_sleep_ms(unsigned ms);

/* draw */
void vg_plot(int x, int y, int color);           /* viewport-relative, clipped */
void vg_plot_abs(int x, int y, int color);       /* screen coords, clipped */
int  vg_peek(int x, int y);                      /* screen coords */
void vg_fill_span(int x0, int x1, int y, int patx, int paty);

/* input */
void vg_input_init(void);
void vg_input_restore(void);
int  vg_read_key(int echo);
int  vg_key_pending(void);
void vg_push_back(int c);

/* font */
const unsigned char *vg_font8x8(int ch);

#endif
