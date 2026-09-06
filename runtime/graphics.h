/* graphics.h - Borland Graphics Interface compatible header
 * Part of VIDYUT. Drop-in replacement for Turbo C's <graphics.h>.
 * MIT licence.
 */
#ifndef VIDYUT_GRAPHICS_H
#define VIDYUT_GRAPHICS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- colours (EGA/VGA 16 colour palette) ---- */
enum COLORS {
    BLACK = 0, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHTGRAY,
    DARKGRAY, LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, LIGHTRED, LIGHTMAGENTA,
    YELLOW, WHITE
};

/* ---- drivers ---- */
enum graphics_drivers {
    DETECT = 0, CGA, MCGA, EGA, EGA64, EGAMONO, IBM8514,
    HERCMONO, ATT400, VGA, PC3270
};

enum graphics_modes {
    VGALO = 0, VGAMED = 1, VGAHI = 2,
    EGALO = 0, EGAHI = 1,
    CGAC0 = 0, CGAC1 = 1, CGAC2 = 2, CGAC3 = 3, CGAHI = 4
};

/* ---- line styles ---- */
enum line_styles { SOLID_LINE = 0, DOTTED_LINE, CENTER_LINE, DASHED_LINE, USERBIT_LINE };
enum line_widths { NORM_WIDTH = 1, THICK_WIDTH = 3 };

/* ---- fill styles ---- */
enum fill_patterns {
    EMPTY_FILL = 0, SOLID_FILL, LINE_FILL, LTSLASH_FILL, SLASH_FILL,
    BKSLASH_FILL, LTBKSLASH_FILL, HATCH_FILL, XHATCH_FILL, INTERLEAVE_FILL,
    WIDE_DOT_FILL, CLOSE_DOT_FILL, USER_FILL
};

/* ---- text ---- */
enum font_names {
    DEFAULT_FONT = 0, TRIPLEX_FONT, SMALL_FONT, SANS_SERIF_FONT,
    GOTHIC_FONT, SCRIPT_FONT, SIMPLEX_FONT, TRIPLEX_SCR_FONT,
    COMPLEX_FONT, EUROPEAN_FONT, BOLD_FONT
};
enum text_directions { HORIZ_DIR = 0, VERT_DIR = 1 };
enum text_just {
    LEFT_TEXT = 0, CENTER_TEXT = 1, RIGHT_TEXT = 2,
    BOTTOM_TEXT = 0, TOP_TEXT = 2
};
#define USER_CHAR_SIZE 0

/* ---- write modes ---- */
enum put_type {
    COPY_PUT = 0, XOR_PUT, OR_PUT, AND_PUT, NOT_PUT
};
#define NORMAL_PUT COPY_PUT

/* ---- error codes ---- */
enum graphics_errors {
    grOk = 0, grNoInitGraph = -1, grNotDetected = -2, grFileNotFound = -3,
    grInvalidDriver = -4, grNoLoadMem = -5, grNoScanMem = -6,
    grNoFloodMem = -7, grFontNotFound = -8, grNoFontMem = -9,
    grInvalidMode = -10, grError = -11, grIOerror = -12,
    grInvalidFont = -13, grInvalidFontNum = -14
};

#define MAXCOLORS 15

struct palettetype  { unsigned char size; signed char colors[16]; };
struct linesettingstype { int linestyle; unsigned upattern; int thickness; };
struct fillsettingstype { int pattern; int color; };
struct textsettingstype { int font; int direction; int charsize; int horiz; int vert; };
struct viewporttype { int left, top, right, bottom; int clip; };
struct arccoordstype { int x, y, xstart, ystart, xend, yend; };
struct fillpatterntype { unsigned char pattern[8]; };

/* ---- lifecycle ---- */
void initgraph(int *graphdriver, int *graphmode, const char *pathtodriver);
void closegraph(void);
void detectgraph(int *graphdriver, int *graphmode);
int  graphresult(void);
char *grapherrormsg(int errorcode);
void restorecrtmode(void);
void setgraphmode(int mode);
int  getgraphmode(void);
const char *getdrivername(void);
const char *getmodename(int mode);
void graphdefaults(void);

/* ---- screen ---- */
int  getmaxx(void);
int  getmaxy(void);
int  getmaxcolor(void);
int  getmaxmode(void);
void cleardevice(void);
void setactivepage(int page);
void setvisualpage(int page);

/* ---- colour ---- */
void setcolor(int color);
int  getcolor(void);
void setbkcolor(int color);
int  getbkcolor(void);
void setpalette(int colornum, int color);
void setallpalette(struct palettetype *palette);
void getpalette(struct palettetype *palette);
int  getpalettesize(void);
void setrgbpalette(int colornum, int r, int g, int b);

/* ---- pixels & position ---- */
void putpixel(int x, int y, int color);
unsigned getpixel(int x, int y);
void moveto(int x, int y);
void moverel(int dx, int dy);
int  getx(void);
int  gety(void);

/* ---- primitives ---- */
void line(int x1, int y1, int x2, int y2);
void lineto(int x, int y);
void linerel(int dx, int dy);
void rectangle(int left, int top, int right, int bottom);
void circle(int x, int y, int radius);
void arc(int x, int y, int stangle, int endangle, int radius);
void ellipse(int x, int y, int stangle, int endangle, int xradius, int yradius);
void fillellipse(int x, int y, int xradius, int yradius);
void sector(int x, int y, int stangle, int endangle, int xradius, int yradius);
void pieslice(int x, int y, int stangle, int endangle, int radius);
void bar(int left, int top, int right, int bottom);
void bar3d(int left, int top, int right, int bottom, int depth, int topflag);
void drawpoly(int numpoints, const int *polypoints);
void fillpoly(int numpoints, const int *polypoints);
void floodfill(int x, int y, int border);
void getarccoords(struct arccoordstype *arccoords);

/* ---- styles ---- */
void setlinestyle(int linestyle, unsigned upattern, int thickness);
void getlinesettings(struct linesettingstype *lineinfo);
void setfillstyle(int pattern, int color);
void setfillpattern(const char *upattern, int color);
void getfillsettings(struct fillsettingstype *fillinfo);
void getfillpattern(char *pattern);
void setwritemode(int mode);

/* ---- text ---- */
void outtext(const char *textstring);
void outtextxy(int x, int y, const char *textstring);
void settextstyle(int font, int direction, int charsize);
void settextjustify(int horiz, int vert);
void setusercharsize(int multx, int divx, int multy, int divy);
void gettextsettings(struct textsettingstype *texttypeinfo);
int  textheight(const char *textstring);
int  textwidth(const char *textstring);

/* ---- viewport ---- */
void setviewport(int left, int top, int right, int bottom, int clip);
void getviewsettings(struct viewporttype *viewport);
void clearviewport(void);

/* ---- image ---- */
unsigned imagesize(int left, int top, int right, int bottom);
void getimage(int left, int top, int right, int bottom, void *bitmap);
void putimage(int left, int top, const void *bitmap, int op);

/* ---- misc ---- */
void delay(unsigned milliseconds);
int  registerbgidriver(void *driver);
int  registerbgifont(void *font);
int  installuserdriver(const char *name, int (*detect)(void));
int  installuserfont(const char *name);

/* VIDYUT extension: force the picture to be pushed to the panel now. */
void vidyut_flush(void);

#ifdef __cplusplus
}
#endif
#endif /* VIDYUT_GRAPHICS_H */
