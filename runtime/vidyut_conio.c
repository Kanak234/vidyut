/* vidyut_conio.c - keyboard input and the conio.h surface.
 * Part of VIDYUT. MIT licence.
 */
#include "vidyut_internal.h"
#include "conio.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if defined(_WIN32)
  #include <windows.h>
  /* Provided by the CRT even though our conio.h shadows Microsoft's. */
  int _getch(void);
  int _kbhit(void);
#else
  #include <termios.h>
  #include <unistd.h>
  #include <sys/select.h>
  static struct termios g_saved;
  static int g_raw = 0;
#endif

static int g_pushback = -1;
static int g_attr_fg  = LIGHTGRAY;
static int g_attr_bg  = BLACK;

/* ------------------------------------------------------------------ */
/* raw terminal                                                       */
/* ------------------------------------------------------------------ */

void vg_input_init(void)
{
#if !defined(_WIN32)
    struct termios raw;
    if (g_raw || !isatty(STDIN_FILENO)) return;
    if (tcgetattr(STDIN_FILENO, &g_saved) != 0) return;
    raw = g_saved;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) g_raw = 1;
#endif
}

void vg_input_restore(void)
{
#if !defined(_WIN32)
    if (!g_raw) return;
    tcsetattr(STDIN_FILENO, TCSANOW, &g_saved);
    g_raw = 0;
#endif
}

void vg_push_back(int c) { g_pushback = c; }

int vg_key_pending(void)
{
    if (g_pushback != -1) return 1;
#if defined(_WIN32)
    return _kbhit() ? 1 : 0;
#else
    {
        fd_set fds;
        struct timeval tv;
        if (!isatty(STDIN_FILENO)) {
            /* Redirected input: a byte is pending until end of file. */
            return feof(stdin) ? 0 : 1;
        }
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        tv.tv_sec = 0; tv.tv_usec = 0;
        return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
    }
#endif
}

int vg_read_key(int echo)
{
    int c;
    if (g_pushback != -1) { c = g_pushback; g_pushback = -1; return c; }

    /* Show the picture before blocking, or the user waits at a blank panel. */
    vg.dirty = 1;
    vg_flush();
    vg_emit_status("waiting");

#if defined(_WIN32)
    c = _getch();
    if (echo && c >= 32 && c < 127) { fputc(c, stdout); fflush(stdout); }
#else
    vg_input_init();
    if (g_raw) {
        unsigned char ch;
        ssize_t n = read(STDIN_FILENO, &ch, 1);
        c = (n == 1) ? (int)ch : EOF;
    } else {
        c = fgetc(stdin);
    }
    if (echo && c >= 32 && c < 127) { fputc(c, stdout); fflush(stdout); }
#endif
    vg_emit_status("running");
    return c;
}

/* ------------------------------------------------------------------ */
/* conio                                                              */
/* ------------------------------------------------------------------ */

int getch(void)  { return vg_read_key(0); }
int getche(void) { return vg_read_key(1); }
int kbhit(void)  { return vg_key_pending(); }

int ungetch(int c) { vg_push_back(c); return c; }

int putch(int c) { fputc(c, stdout); fflush(stdout); return c; }

int cputs(const char *s)
{
    if (!s) return 0;
    fputs(s, stdout);
    fflush(stdout);
    return 0;
}

int cprintf(const char *format, ...)
{
    va_list ap;
    int n;
    va_start(ap, format);
    n = vprintf(format, ap);
    va_end(ap);
    fflush(stdout);
    return n;
}

int cscanf(const char *format, ...)
{
    va_list ap;
    int n;
    /* scanf needs a cooked line, so step out of raw mode for the read. */
    vg_input_restore();
    va_start(ap, format);
    n = vscanf(format, ap);
    va_end(ap);
    vg_input_init();
    return n;
}

char *cgets(char *str)
{
    /* Turbo C convention: str[0] is the buffer size on the way in,
     * str[1] is the length on the way out, text starts at str[2]. */
    int maxlen, len = 0, c;
    if (!str) return NULL;
    maxlen = (unsigned char)str[0];
    vg_input_restore();
    while (len < maxlen - 1) {
        c = fgetc(stdin);
        if (c == EOF || c == '\n' || c == '\r') break;
        str[2 + len++] = (char)c;
    }
    str[2 + len] = '\0';
    str[1] = (char)len;
    vg_input_init();
    return str + 2;
}

char *gets_conio(char *str)
{
    vg_input_restore();
    if (fgets(str, 4096, stdin)) {
        size_t n = strlen(str);
        if (n && str[n-1] == '\n') str[n-1] = '\0';
        vg_input_init();
        return str;
    }
    vg_input_init();
    return NULL;
}

/* The text-mode calls drive the real terminal through ANSI sequences,
 * which is what the integrated terminal understands. */
void clrscr(void)  { fputs("\033[2J\033[H", stdout); fflush(stdout); }
void clreol(void)  { fputs("\033[K", stdout); fflush(stdout); }

void gotoxy(int x, int y)
{
    if (x < 1) x = 1;
    if (y < 1) y = 1;
    printf("\033[%d;%dH", y, x);
    fflush(stdout);
}

int wherex(void) { return 1; }
int wherey(void) { return 1; }

/* Map the 16 DOS colours onto ANSI: 0-7 normal, 8-15 bright. */
static int ansi_code(int c, int background)
{
    static const int order[8] = {0, 4, 2, 6, 1, 5, 3, 7};
    int base = background ? 40 : 30;
    int idx  = order[c & 7];
    if (c & 8) base += 60;
    return base + idx;
}

void textcolor(int color)
{
    g_attr_fg = color & 15;
    printf("\033[%dm", ansi_code(g_attr_fg, 0));
    fflush(stdout);
}

void textbackground(int color)
{
    g_attr_bg = color & 15;
    printf("\033[%dm", ansi_code(g_attr_bg, 1));
    fflush(stdout);
}

void textattr(int attr)
{
    textcolor(attr & 15);
    textbackground((attr >> 4) & 7);
}

void textmode(int mode) { (void)mode; clrscr(); }
void highvideo(void)    { fputs("\033[1m", stdout); fflush(stdout); }
void lowvideo(void)     { fputs("\033[2m", stdout); fflush(stdout); }
void normvideo(void)    { fputs("\033[0m", stdout); fflush(stdout); }

void window(int l, int t, int r, int b) { (void)l;(void)t;(void)r;(void)b; }
void delline(void) { fputs("\033[M", stdout); fflush(stdout); }
void insline(void) { fputs("\033[L", stdout); fflush(stdout); }

void gettextinfo(struct text_info *r)
{
    if (!r) return;
    memset(r, 0, sizeof *r);
    r->winleft = 1; r->wintop = 1; r->winright = 80; r->winbottom = 25;
    r->screenwidth = 80; r->screenheight = 25;
    r->attribute = (unsigned char)((g_attr_bg << 4) | g_attr_fg);
    r->normattr  = (unsigned char)((BLACK << 4) | LIGHTGRAY);
    r->currmode  = C80;
    r->curx = 1; r->cury = 1;
}

/* delay pushes the current picture out first, which is what makes
 * animation loops readable in the panel. */
void delay(unsigned milliseconds)
{
    if (vg.active) { vg.dirty = 1; vg_flush(); }
    vg_sleep_ms(milliseconds);
}
