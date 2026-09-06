/* conio.h - Turbo C console I/O compatible header
 * Part of VIDYUT. MIT licence.
 */
#ifndef VIDYUT_CONIO_H
#define VIDYUT_CONIO_H

#ifdef __cplusplus
extern "C" {
#endif

enum text_modes {
    LASTMODE = -1, BW40 = 0, C40 = 1, BW80 = 2, C80 = 3, MONO = 7, C4350 = 64
};

#ifndef VIDYUT_GRAPHICS_H
enum COLORS {
    BLACK = 0, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHTGRAY,
    DARKGRAY, LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, LIGHTRED, LIGHTMAGENTA,
    YELLOW, WHITE
};
#endif
#define BLINK 128

struct text_info {
    unsigned char winleft, wintop, winright, winbottom;
    unsigned char attribute, normattr;
    unsigned char currmode;
    unsigned char screenheight, screenwidth;
    unsigned char curx, cury;
};

int  getch(void);
int  getche(void);
int  kbhit(void);
int  putch(int c);
int  ungetch(int c);
char *cgets(char *str);
int  cputs(const char *str);
int  cprintf(const char *format, ...);
int  cscanf(const char *format, ...);
char *gets_conio(char *str);

void clrscr(void);
void clreol(void);
void gotoxy(int x, int y);
int  wherex(void);
int  wherey(void);
void textcolor(int color);
void textbackground(int color);
void textattr(int attr);
void textmode(int mode);
void highvideo(void);
void lowvideo(void);
void normvideo(void);
void window(int left, int top, int right, int bottom);
void gettextinfo(struct text_info *r);
void delline(void);
void insline(void);

#ifndef VIDYUT_GRAPHICS_H
void delay(unsigned milliseconds);
#endif

#ifdef __cplusplus
}
#endif
#endif /* VIDYUT_CONIO_H */
