#include <stdint.h>
/* vidyut_draw.c - the rasteriser.
 * Part of VIDYUT. MIT licence.
 */
#include "vidyut_internal.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

static void fill_poly_interior(int n, const int *pts);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* patterns                                                           */
/* ------------------------------------------------------------------ */

static const unsigned char kFillPat[12][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* EMPTY      */
    {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, /* SOLID      */
    {0x00,0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF}, /* LINE       */
    {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80}, /* LTSLASH    */
    {0xE0,0xC1,0x83,0x07,0x0E,0x1C,0x38,0x70}, /* SLASH      */
    {0xF0,0x78,0x3C,0x1E,0x0F,0x87,0xC3,0xE1}, /* BKSLASH    */
    {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01}, /* LTBKSLASH  */
    {0xFF,0x88,0x88,0x88,0xFF,0x88,0x88,0x88}, /* HATCH      */
    {0x81,0x42,0x24,0x18,0x18,0x24,0x42,0x81}, /* XHATCH     */
    {0xCC,0x33,0xCC,0x33,0xCC,0x33,0xCC,0x33}, /* INTERLEAVE */
    {0x80,0x00,0x08,0x00,0x80,0x00,0x08,0x00}, /* WIDE_DOT   */
    {0x88,0x00,0x22,0x00,0x88,0x00,0x22,0x00}  /* CLOSE_DOT  */
};

static unsigned line_pattern(void)
{
    switch (vg.line.linestyle) {
        case SOLID_LINE:   return 0xFFFFu;
        case DOTTED_LINE:  return 0xCCCCu;
        case CENTER_LINE:  return 0xFC78u;
        case DASHED_LINE:  return 0xF8F8u;
        case USERBIT_LINE: return vg.line.upattern;
        default:           return 0xFFFFu;
    }
}

/* ------------------------------------------------------------------ */
/* pixel access                                                       */
/* ------------------------------------------------------------------ */

void vg_plot_abs(int x, int y, int color)
{
    unsigned char *p;
    if (!vg.active) return;
    if (x < 0 || y < 0 || x >= vg.w || y >= vg.h) return;
    p = &vg.fb[(size_t)y * vg.w + x];
    switch (vg.writemode) {
        case XOR_PUT: *p = (unsigned char)(*p ^ (color & 15)); break;
        case OR_PUT:  *p = (unsigned char)(*p | (color & 15)); break;
        case AND_PUT: *p = (unsigned char)(*p & (color & 15)); break;
        case NOT_PUT: *p = (unsigned char)(~color & 15);       break;
        default:      *p = (unsigned char)(color & 15);        break;
    }
    vg.dirty = 1;
}

/* Viewport-relative plot, the coordinate space user code draws in. */
void vg_plot(int x, int y, int color)
{
    int ax = x + vg.vp.left;
    int ay = y + vg.vp.top;
    if (vg.vp.clip) {
        if (ax < vg.vp.left || ax > vg.vp.right ||
            ay < vg.vp.top  || ay > vg.vp.bottom) return;
    }
    vg_plot_abs(ax, ay, color);
}

int vg_peek(int x, int y)
{
    if (!vg.active) return 0;
    if (x < 0 || y < 0 || x >= vg.w || y >= vg.h) return 0;
    return vg.fb[(size_t)y * vg.w + x];
}

void putpixel(int x, int y, int color)
{
    int saved = vg.writemode;
    vg_ensure_init();
    vg.writemode = COPY_PUT;      /* putpixel always writes straight through */
    vg_plot(x, y, color);
    vg.writemode = saved;
    vg_tick();
}

unsigned getpixel(int x, int y)
{
    vg_ensure_init();
    return (unsigned)vg_peek(x + vg.vp.left, y + vg.vp.top);
}

/* One horizontal run of the current fill pattern. Coordinates are
 * viewport-relative; the pattern is anchored to the screen so adjacent
 * shapes line up the way Turbo C's do. */
void vg_fill_span(int x0, int x1, int y, int patx, int paty)
{
    const unsigned char *pat;
    int x, col, bg;
    (void)patx; (void)paty;

    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    col = vg.fill.color;
    bg  = vg.bkcolor;

    if (vg.fill.pattern == SOLID_FILL) {
        for (x = x0; x <= x1; x++) vg_plot(x, y, col);
        return;
    }
    if (vg.fill.pattern == EMPTY_FILL) {
        for (x = x0; x <= x1; x++) vg_plot(x, y, bg);
        return;
    }
    pat = (vg.fill.pattern == USER_FILL)
            ? vg.userfill
            : kFillPat[vg.fill.pattern % 12];

    for (x = x0; x <= x1; x++) {
        int ax = x + vg.vp.left, ay = y + vg.vp.top;
        int bit = (pat[ay & 7] >> (7 - (ax & 7))) & 1;
        vg_plot(x, y, bit ? col : bg);
    }
}

/* ------------------------------------------------------------------ */
/* lines                                                              */
/* ------------------------------------------------------------------ */

/* Thickness in BGI is 1 or 3 pixels, drawn perpendicular to the run. */
static void thick_plot(int x, int y, int color, int steep)
{
    if (vg.line.thickness <= NORM_WIDTH) {
        vg_plot(x, y, color);
        return;
    }
    if (steep) {
        vg_plot(x - 1, y, color);
        vg_plot(x,     y, color);
        vg_plot(x + 1, y, color);
    } else {
        vg_plot(x, y - 1, color);
        vg_plot(x, y,     color);
        vg_plot(x, y + 1, color);
    }
}

static void bresenham_ex(int x1, int y1, int x2, int y2, int color,
                         int styled, int skip_first)
{
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy, e2;
    int steep = dy > dx;
    unsigned pat = styled ? line_pattern() : 0xFFFFu;
    int bit = 0;

    for (;;) {
        if (!skip_first && (!styled || ((pat >> (15 - (bit & 15))) & 1)))
            thick_plot(x1, y1, color, steep);
        skip_first = 0;
        bit++;
        if (x1 == x2 && y1 == y2) break;
        e2 = err * 2;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 <  dx) { err += dx; y1 += sy; }
    }
}

static void bresenham(int x1, int y1, int x2, int y2, int color, int styled)
{
    bresenham_ex(x1, y1, x2, y2, color, styled, 0);
}

/* Draws every pixel except the starting one. Chaining these keeps XOR mode
 * correct, because no pixel is written twice at a shared joint. */
static void bresenham_open(int x1, int y1, int x2, int y2, int color)
{
    bresenham_ex(x1, y1, x2, y2, color, 0, 1);
}

void line(int x1, int y1, int x2, int y2)
{
    vg_ensure_init();
    bresenham(x1, y1, x2, y2, vg.color, 1);
    vg_tick();
}

void lineto(int x, int y)
{
    vg_ensure_init();
    bresenham(vg.cpx, vg.cpy, x, y, vg.color, 1);
    vg.cpx = x; vg.cpy = y;
    vg_tick();
}

void linerel(int dx, int dy) { lineto(vg.cpx + dx, vg.cpy + dy); }

void rectangle(int left, int top, int right, int bottom)
{
    vg_ensure_init();
    bresenham(left,  top,    right, top,    vg.color, 1);
    bresenham(right, top,    right, bottom, vg.color, 1);
    bresenham(right, bottom, left,  bottom, vg.color, 1);
    bresenham(left,  bottom, left,  top,    vg.color, 1);
    vg_tick();
}

void bar(int left, int top, int right, int bottom)
{
    int y;
    vg_ensure_init();
    if (top > bottom) { int t = top; top = bottom; bottom = t; }
    for (y = top; y <= bottom; y++) vg_fill_span(left, right, y, 0, 0);
    vg_tick();
}

void bar3d(int left, int top, int right, int bottom, int depth, int topflag)
{
    vg_ensure_init();
    bar(left, top, right, bottom);
    bresenham(left,  top,    right, top,    vg.color, 1);
    bresenham(right, top,    right, bottom, vg.color, 1);
    bresenham(right, bottom, left,  bottom, vg.color, 1);
    bresenham(left,  bottom, left,  top,    vg.color, 1);
    if (depth > 0) {
        int dy = depth / 2;
        bresenham(right, bottom, right + depth, bottom - dy, vg.color, 1);
        bresenham(right, top,    right + depth, top - dy,    vg.color, 1);
        bresenham(right + depth, top - dy, right + depth, bottom - dy, vg.color, 1);
        if (topflag) {
            bresenham(left, top, left + depth, top - dy, vg.color, 1);
            bresenham(left + depth, top - dy, right + depth, top - dy, vg.color, 1);
        }
    }
    vg_tick();
}

/* ------------------------------------------------------------------ */
/* circles, arcs, ellipses                                            */
/* ------------------------------------------------------------------ */

void circle(int cx, int cy, int radius)
{
    int x = 0, y = radius, d = 3 - 2 * radius;
    vg_ensure_init();
    if (radius < 0) return;
    while (x <= y) {
        vg_plot(cx + x, cy + y, vg.color); vg_plot(cx - x, cy + y, vg.color);
        vg_plot(cx + x, cy - y, vg.color); vg_plot(cx - x, cy - y, vg.color);
        vg_plot(cx + y, cy + x, vg.color); vg_plot(cx - y, cy + x, vg.color);
        vg_plot(cx + y, cy - x, vg.color); vg_plot(cx - y, cy - x, vg.color);
        if (d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
    vg_tick();
}

/* Angles are degrees counter-clockwise from east, as in Turbo C. */
/* draw==0 only records the endpoints, which is what sector needs after it
 * has already filled the wedge. */
static void arc_generic(int cx, int cy, int st, int en, int rx, int ry, int draw)
{
    double a, step, r, a0, a1;
    int px = 0, py = 0, first = 1;
    int sx = cx, sy = cy, ex = cx, ey = cy;

    if (rx <= 0 || ry <= 0) return;
    while (en < st) en += 360;

    /* One step per pixel of the longer axis keeps the outline gap-free. */
    r = (rx > ry) ? rx : ry;
    step = 1.0 / (r > 1.0 ? r : 1.0);
    a0 = st * M_PI / 180.0;
    a1 = en * M_PI / 180.0;

    for (a = a0; a <= a1 + 1e-9; a += step) {
        int x = cx + (int)(rx * cos(a) + (cos(a) >= 0 ? 0.5 : -0.5));
        int y = cy - (int)(ry * sin(a) + (sin(a) >= 0 ? 0.5 : -0.5));
        if (first) {
            sx = x; sy = y;
            /* Plot the first point once; every later point is covered by
             * the segment drawn to it, so XOR mode never hits a pixel twice. */
            if (draw) vg_plot(x, y, vg.color);
            first = 0;
        } else if (draw && (x != px || y != py)) {
            bresenham_open(px, py, x, y, vg.color);
        }
        px = x; py = y; ex = x; ey = y;
    }
    vg.arcco.x = cx; vg.arcco.y = cy;
    vg.arcco.xstart = sx; vg.arcco.ystart = sy;
    vg.arcco.xend = ex;   vg.arcco.yend = ey;
}

void arc(int x, int y, int stangle, int endangle, int radius)
{
    vg_ensure_init();
    arc_generic(x, y, stangle, endangle, radius, radius, 0);
    vg_tick();
}

void ellipse(int x, int y, int stangle, int endangle, int xr, int yr)
{
    vg_ensure_init();
    arc_generic(x, y, stangle, endangle, xr, yr, 0);
    vg_tick();
}

void getarccoords(struct arccoordstype *a) { if (a) *a = vg.arcco; }

void fillellipse(int cx, int cy, int xr, int yr)
{
    int y;
    vg_ensure_init();
    if (xr <= 0 || yr <= 0) return;
    for (y = -yr; y <= yr; y++) {
        double t = 1.0 - (double)(y * y) / (double)(yr * yr);
        int dx;
        if (t < 0) continue;
        dx = (int)(xr * sqrt(t) + 0.5);
        vg_fill_span(cx - dx, cx + dx, cy + y, 0, 0);
    }
    arc_generic(cx, cy, 0, 360, xr, yr, 0);
    vg_tick();
}

void sector(int cx, int cy, int st, int en, int xr, int yr)
{
    double a, step, r;
    int lastx = 0, lasty = 0, first = 1;

    vg_ensure_init();
    if (xr <= 0 || yr <= 0) return;
    while (en < st) en += 360;
    r = (xr > yr) ? xr : yr;
    step = 1.0 / (r > 1.0 ? r : 1.0);

    /* Fill by sweeping triangles from the centre out to the rim. The
     * triangles are filled only — outlining each one would leave spokes
     * across the slice. */
    for (a = st * M_PI / 180.0; a <= en * M_PI / 180.0 + 1e-9; a += step) {
        int x = cx + (int)(xr * cos(a) + (cos(a) >= 0 ? 0.5 : -0.5));
        int y = cy - (int)(yr * sin(a) + (sin(a) >= 0 ? 0.5 : -0.5));
        if (!first) {
            int pts[6];
            pts[0] = cx;    pts[1] = cy;
            pts[2] = lastx; pts[3] = lasty;
            pts[4] = x;     pts[5] = y;
            fill_poly_interior(3, pts);
        }
        lastx = x; lasty = y; first = 0;
    }
    /* Now the boundary: the rim, then the two straight edges. */
    arc_generic(cx, cy, st, en, xr, yr, 1);
    bresenham(cx, cy, vg.arcco.xstart, vg.arcco.ystart, vg.color, 0);
    bresenham(cx, cy, vg.arcco.xend,   vg.arcco.yend,   vg.color, 0);
    vg_tick();
}

void pieslice(int x, int y, int st, int en, int radius)
{
    sector(x, y, st, en, radius, radius);
}

/* ------------------------------------------------------------------ */
/* polygons                                                           */
/* ------------------------------------------------------------------ */

void drawpoly(int n, const int *pts)
{
    int i;
    vg_ensure_init();
    if (n < 2 || !pts) return;
    for (i = 0; i < n - 1; i++)
        bresenham(pts[i*2], pts[i*2+1], pts[i*2+2], pts[i*2+3], vg.color, 1);
    vg_tick();
}

static void fill_poly_interior(int n, const int *pts)
{
    int ymin, ymax, y, i, count;
    int *xs;

    if (n < 3 || !pts) return;

    ymin = ymax = pts[1];
    for (i = 1; i < n; i++) {
        if (pts[i*2+1] < ymin) ymin = pts[i*2+1];
        if (pts[i*2+1] > ymax) ymax = pts[i*2+1];
    }
    xs = (int *)malloc(sizeof(int) * (size_t)(n + 1));
    if (!xs) { vg.result = grNoScanMem; return; }

    for (y = ymin; y <= ymax; y++) {
        count = 0;
        for (i = 0; i < n; i++) {
            int j  = (i + 1) % n;
            int y1 = pts[i*2+1], y2 = pts[j*2+1];
            int x1 = pts[i*2],   x2 = pts[j*2];
            if (y1 == y2) continue;
            if ((y >= y1 && y < y2) || (y >= y2 && y < y1))
                xs[count++] = x1 + (y - y1) * (x2 - x1) / (y2 - y1);
        }
        for (i = 1; i < count; i++) {
            int k = xs[i], j = i - 1;
            while (j >= 0 && xs[j] > k) { xs[j+1] = xs[j]; j--; }
            xs[j+1] = k;
        }
        for (i = 0; i + 1 < count; i += 2)
            vg_fill_span(xs[i], xs[i+1], y, 0, 0);
    }
    free(xs);
}

void fillpoly(int n, const int *pts)
{
    int i;
    vg_ensure_init();
    fill_poly_interior(n, pts);
    /* Turbo C outlines a filled polygon in the current colour. */
    for (i = 0; i < n; i++) {
        int j = (i + 1) % n;
        bresenham(pts[i*2], pts[i*2+1], pts[j*2], pts[j*2+1], vg.color, 1);
    }
    vg_tick();
}

/* ------------------------------------------------------------------ */
/* flood fill                                                         */
/* ------------------------------------------------------------------ */

typedef struct { int x, y; } VPt;

void floodfill(int sx, int sy, int border)
{
    VPt *stack;
    long cap, sp = 0;
    int seed, left, right, x, ay;
    int vw, vh;
    uint8_t *visited;

    vg_ensure_init();
    vw = vg.vp.right - vg.vp.left + 1;
    vh = vg.vp.bottom - vg.vp.top + 1;
    if (sx < 0 || sy < 0 || sx >= vw || sy >= vh) return;

    border &= 15;
    seed = vg_peek(sx + vg.vp.left, sy + vg.vp.top);
    if (seed == border) return;

    visited = (uint8_t *)calloc((size_t)((vw * vh + 7) / 8), 1);
    if (!visited) { vg.result = grNoFloodMem; return; }

#define V_GET(vx, vy) (visited[((vy) * vw + (vx)) >> 3] & (1 << (((vy) * vw + (vx)) & 7)))
#define V_SET(vx, vy) (visited[((vy) * vw + (vx)) >> 3] |= (1 << (((vy) * vw + (vx)) & 7)))

    cap   = (long)vw * vh / 4 + 64;
    stack = (VPt *)malloc(sizeof(VPt) * (size_t)cap);
    if (!stack) { free(visited); vg.result = grNoFloodMem; return; }

    stack[sp].x = sx; stack[sp].y = sy; sp++;
    V_SET(sx, sy);

    while (sp > 0) {
        VPt p = stack[--sp];
        ay = p.y + vg.vp.top;
        if (p.y < 0 || p.y >= vh) continue;
        if (vg_peek(p.x + vg.vp.left, ay) == border) continue;

        left = p.x;
        while (left > 0) {
            int c = vg_peek(left - 1 + vg.vp.left, ay);
            if (c == border || V_GET(left - 1, p.y)) break;
            left--;
            V_SET(left, p.y);
        }
        right = p.x;
        while (right < vw - 1) {
            int c = vg_peek(right + 1 + vg.vp.left, ay);
            if (c == border || V_GET(right + 1, p.y)) break;
            right++;
            V_SET(right, p.y);
        }
        vg_fill_span(left, right, p.y, 0, 0);

        /* Push the runs above and below; one seed per contiguous run. */
        for (x = left; x <= right; x++) {
            int dy;
            for (dy = -1; dy <= 1; dy += 2) {
                int ny = p.y + dy;
                int c;
                if (ny < 0 || ny >= vh) continue;
                if (V_GET(x, ny)) continue;
                c = vg_peek(x + vg.vp.left, ny + vg.vp.top);
                if (c == border) continue;
                if (x > left && !V_GET(x - 1, ny)) {
                    int cp = vg_peek(x - 1 + vg.vp.left, ny + vg.vp.top);
                    if (cp != border) continue;  /* same run */
                }
                V_SET(x, ny);
                if (sp >= cap) {
                    cap *= 2;
                    { VPt *g = (VPt *)realloc(stack, sizeof(VPt) * (size_t)cap);
                      if (!g) { free(stack); free(visited); vg.result = grNoFloodMem; return; }
                      stack = g; }
                }
                stack[sp].x = x; stack[sp].y = ny; sp++;
            }
        }
    }
#undef V_GET
#undef V_SET
    free(stack);
    free(visited);
    vg_tick();
}

/* ------------------------------------------------------------------ */
/* settings                                                           */
/* ------------------------------------------------------------------ */

void setlinestyle(int style, unsigned upattern, int thickness)
{
    vg.line.linestyle = style;
    vg.line.upattern  = upattern;
    vg.line.thickness = thickness;
}

void getlinesettings(struct linesettingstype *l) { if (l) *l = vg.line; }

void setfillstyle(int pattern, int color)
{
    if (pattern < 0 || pattern > USER_FILL) { vg.result = grError; return; }
    vg.fill.pattern = pattern;
    vg.fill.color   = color & 15;
}

void setfillpattern(const char *upattern, int color)
{
    int i;
    if (upattern) for (i = 0; i < 8; i++) vg.userfill[i] = (unsigned char)upattern[i];
    vg.fill.pattern = USER_FILL;
    vg.fill.color   = color & 15;
}

void getfillsettings(struct fillsettingstype *f) { if (f) *f = vg.fill; }

void getfillpattern(char *pattern)
{
    int i;
    if (pattern) for (i = 0; i < 8; i++) pattern[i] = (char)vg.userfill[i];
}

/* ------------------------------------------------------------------ */
/* viewport                                                           */
/* ------------------------------------------------------------------ */

void setviewport(int left, int top, int right, int bottom, int clip)
{
    vg_ensure_init();
    if (left < 0 || top < 0 || right >= vg.w || bottom >= vg.h ||
        left > right || top > bottom) {
        vg.result = grError;
        return;
    }
    vg.vp.left = left; vg.vp.top = top;
    vg.vp.right = right; vg.vp.bottom = bottom;
    vg.vp.clip = clip;
    vg.cpx = vg.cpy = 0;
}

void getviewsettings(struct viewporttype *v) { if (v) *v = vg.vp; }

void clearviewport(void)
{
    int y, x;
    vg_ensure_init();
    for (y = vg.vp.top; y <= vg.vp.bottom; y++)
        for (x = vg.vp.left; x <= vg.vp.right; x++)
            vg.fb[(size_t)y * vg.w + x] = (unsigned char)vg.bkcolor;
    vg.cpx = vg.cpy = 0;
    vg.dirty = 1;
    vg_flush();
}

void cleardevice(void)
{
    vg_ensure_init();
    memset(vg.fb, vg.bkcolor, (size_t)vg.w * vg.h);
    vg.cpx = vg.cpy = 0;
    vg.dirty = 1;
    vg_flush();
}

/* ------------------------------------------------------------------ */
/* images                                                             */
/* ------------------------------------------------------------------ */

/* Layout matches Turbo C: two words of size, then one byte per pixel. */
unsigned imagesize(int left, int top, int right, int bottom)
{
    long w = right - left + 1, h = bottom - top + 1;
    if (w <= 0 || h <= 0) return 0;
    return (unsigned)(4 + w * h);
}

void getimage(int left, int top, int right, int bottom, void *bitmap)
{
    unsigned short *hdr = (unsigned short *)bitmap;
    unsigned char  *px;
    int w = right - left + 1, h = bottom - top + 1, x, y;

    vg_ensure_init();
    if (!bitmap || w <= 0 || h <= 0) return;
    hdr[0] = (unsigned short)(w - 1);
    hdr[1] = (unsigned short)(h - 1);
    px = (unsigned char *)bitmap + 4;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            px[y * w + x] = (unsigned char)
                vg_peek(left + x + vg.vp.left, top + y + vg.vp.top);
}

void putimage(int left, int top, const void *bitmap, int op)
{
    const unsigned short *hdr = (const unsigned short *)bitmap;
    const unsigned char  *px;
    int w, h, x, y, saved;

    vg_ensure_init();
    if (!bitmap) return;
    w = hdr[0] + 1; h = hdr[1] + 1;
    px = (const unsigned char *)bitmap + 4;

    saved = vg.writemode;
    vg.writemode = op;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            vg_plot(left + x, top + y, px[y * w + x]);
    vg.writemode = saved;
    vg_tick();
}
