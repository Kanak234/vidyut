#include <graphics.h>
#include <conio.h>
#include <stdio.h>

int main(void)
{
    int gd = DETECT, gm;
    int tri[6]  = {60, 400, 160, 400, 110, 320};
    int star[10]= {300,300, 340,380, 260,330, 340,330, 260,380};
    int i;

    initgraph(&gd, &gm, "");

    /* text at several sizes and justifications */
    setcolor(YELLOW);
    settextstyle(DEFAULT_FONT, HORIZ_DIR, 3);
    outtextxy(20, 12, "VIDYUT graphics.h");
    settextstyle(DEFAULT_FONT, HORIZ_DIR, 1);
    setcolor(LIGHTCYAN);
    outtextxy(20, 48, "abcdefghijklmnopqrstuvwxyz 0123456789");
    outtextxy(20, 60, "ABCDEFGHIJKLMNOPQRSTUVWXYZ !@#$%^&*()");

    /* line styles */
    for (i = 0; i < 4; i++) {
        setlinestyle(i, 0, NORM_WIDTH);
        setcolor(LIGHTGREEN);
        line(20, 90 + i * 12, 300, 90 + i * 12);
    }
    setlinestyle(SOLID_LINE, 0, THICK_WIDTH);
    setcolor(LIGHTRED);
    line(20, 142, 300, 142);
    setlinestyle(SOLID_LINE, 0, NORM_WIDTH);

    /* circles and an outlined rectangle with flood fill */
    setcolor(WHITE);
    circle(400, 120, 60);
    setfillstyle(SOLID_FILL, LIGHTBLUE);
    floodfill(400, 120, WHITE);

    setcolor(LIGHTMAGENTA);
    rectangle(500, 60, 600, 180);
    setfillstyle(HATCH_FILL, LIGHTMAGENTA);
    floodfill(550, 120, LIGHTMAGENTA);

    /* fill patterns across a row of bars */
    for (i = 0; i < 12; i++) {
        setfillstyle(i, (i % 14) + 1);
        setcolor(DARKGRAY);
        bar(20 + i * 48, 180, 20 + i * 48 + 40, 240);
        rectangle(20 + i * 48, 180, 20 + i * 48 + 40, 240);
    }

    /* pie slices */
    setfillstyle(SOLID_FILL, LIGHTGREEN);
    setcolor(WHITE);
    pieslice(120, 300, 0, 120, 55);
    setfillstyle(SOLID_FILL, LIGHTRED);
    pieslice(120, 300, 120, 260, 55);
    setfillstyle(SOLID_FILL, YELLOW);
    pieslice(120, 300, 260, 360, 55);

    /* bar3d, polygons */
    setfillstyle(SOLID_FILL, CYAN);
    setcolor(WHITE);
    bar3d(240, 250, 300, 340, 20, 1);

    setfillstyle(SOLID_FILL, BROWN);
    setcolor(WHITE);
    fillpoly(3, tri);

    setcolor(LIGHTCYAN);
    setfillstyle(SOLID_FILL, BLUE);
    fillpoly(5, star);

    /* arcs and ellipse */
    setcolor(WHITE);
    arc(480, 300, 30, 300, 70);
    ellipse(480, 300, 0, 360, 90, 40);
    setfillstyle(SOLID_FILL, MAGENTA);
    fillellipse(590, 400, 40, 25);

    /* vertical text */
    setcolor(LIGHTGREEN);
    settextstyle(DEFAULT_FONT, VERT_DIR, 2);
    outtextxy(620, 460, "VERTICAL");

    /* centred text */
    settextstyle(DEFAULT_FONT, HORIZ_DIR, 2);
    settextjustify(CENTER_TEXT, TOP_TEXT);
    setcolor(WHITE);
    outtextxy(320, 440, "centred");
    settextjustify(LEFT_TEXT, TOP_TEXT);

    /* getpixel round trip */
    printf("getpixel(400,120) = %u  (expect %d)\n", getpixel(400, 120), LIGHTBLUE);
    printf("getmaxx=%d getmaxy=%d\n", getmaxx(), getmaxy());
    printf("textwidth(\"abc\")=%d textheight=%d\n", textwidth("abc"), textheight("abc"));

    closegraph();
    return 0;
}
