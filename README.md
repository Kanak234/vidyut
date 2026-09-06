# VIDYUT

**Write Turbo C graphics in VS Code. No DOSBox, no Turbo C install.**

Your college lab gives you `graphics.h` programs. Turbo C is from 1992, DOSBox
is fiddly, and neither runs properly on a modern laptop. VIDYUT lets you write
the same code in VS Code and watch it draw, in a panel next to your editor.

```c
#include <graphics.h>

int main(void)
{
    int gd = DETECT, gm;
    initgraph(&gd, &gm, "");

    setcolor(YELLOW);
    circle(320, 240, 100);
    setfillstyle(SOLID_FILL, LIGHTBLUE);
    floodfill(320, 240, YELLOW);

    getch();
    closegraph();
    return 0;
}
```

Press **Ctrl+Alt+G**. That is the whole workflow.

## What you need

A C compiler on your PATH — `gcc` on Windows through MinGW-w64, the Xcode
command line tools on macOS, `build-essential` on Linux. Run **VIDYUT: Check
My Compiler** from the command palette and it will tell you what it found.

Nothing else. No BGI driver files, no `EGAVGA.BGI`, no linker settings.

## How it works

`graphics.h` and `conio.h` ship with the extension and are put on your include
path automatically. Behind them is a complete BGI rasteriser: your program
draws into a real framebuffer, exactly as it would have on a VGA card, and the
finished picture is sent to the panel.

That matters for the calls that read the screen back. `getpixel` returns what
is actually there. `floodfill` walks real pixels and stops at real boundaries.
`XOR_PUT` erases what it drew. Programs that animate by drawing and undrawing
work the way they did on the original hardware.

## What is supported

**Shapes** — `line` `lineto` `linerel` `rectangle` `circle` `arc` `ellipse`
`fillellipse` `sector` `pieslice` `bar` `bar3d` `drawpoly` `fillpoly`
`floodfill` `putpixel` `getpixel`

**Style** — `setcolor` `setbkcolor` `setfillstyle` `setfillpattern`
`setlinestyle` `setwritemode` `setpalette` `setrgbpalette` and the twelve
standard fill patterns

**Text** — `outtext` `outtextxy` `settextstyle` `settextjustify`
`setusercharsize` `textwidth` `textheight`, horizontal and vertical

**Screen** — `initgraph` `closegraph` `cleardevice` `setviewport`
`clearviewport` `getimage` `putimage` `imagesize` `getmaxx` `getmaxy`

**conio** — `getch` `getche` `kbhit` `clrscr` `gotoxy` `textcolor`
`textbackground` `cprintf` `delay` and the rest

The sixteen EGA colour constants are the real ones, so `LIGHTCYAN` is the
colour you remember.

## Using the panel

| | |
| --- | --- |
| **Ctrl+Alt+G** | Run and draw |
| **Ctrl+Alt+X** | Stop the program |
| **F5** | Cycle zoom: fit, 1×, 2×, 3× |
| **F6** | Pixel grid, once zoomed in far enough |
| **F9** | Save the picture as a PNG |

Hover anywhere on the picture and the status bar shows the coordinate and the
colour index under the pointer — useful when a `floodfill` leaks and you need
to find the gap in your outline.

Your program's `printf` output appears under the picture. When the program
calls `getch`, click the picture and type: the keystroke goes straight to your
program. `scanf` works the same way.

## Examples

Run **VIDYUT: Open an Example** for eight working programs, including the ones
your lab manual probably asks for: a moving car, DDA line drawing, the midpoint
circle algorithm, boundary fill, a bar chart, and an analog clock.

## If IntelliSense underlines graphics.h

Run **VIDYUT: Set Up This Folder for graphics.h**. It adds the include path to
`.vscode/c_cpp_properties.json` and the red squiggles go away. Compiling always
worked; this only quiets the editor.

## Settings

| setting | default | what it does |
| --- | --- | --- |
| `vidyut.compilerC` | `gcc` | Command used for `.c` files |
| `vidyut.compilerCpp` | `g++` | Command used for `.cpp` files |
| `vidyut.extraCompilerArgs` | `[]` | Extra compiler flags, for example `-Wall` |
| `vidyut.zoom` | `fit` | How the picture is sized |
| `vidyut.smoothing` | `false` | Smooth when scaled up; off keeps pixels sharp |
| `vidyut.runTimeout` | `120` | Seconds before a runaway program is stopped |
| `vidyut.openPanelBeside` | `true` | Open the picture next to your code |

## Differences from real Turbo C

Worth knowing before you hand in an assignment:

- The stroked fonts (`TRIPLEX_FONT`, `GOTHIC_FONT` and the rest) are drawn with
  the 8×8 bitmap font scaled up. Sizes are right; letterforms are not.
- `setactivepage` and `setvisualpage` are accepted but do nothing. There is one
  page. Animate with `delay` instead, which pushes each frame to the panel.
- Text-mode `conio` calls drive the output area through ANSI codes rather than
  an 80×25 character buffer, so `wherex` and `wherey` always report 1.
- `int86`, `int86x` and direct BIOS access are not supported and never will be.

## Privacy

VIDYUT sends nothing anywhere. Your code is compiled and run on your machine,
and the panel has no network access under its content security policy.

## Licence

MIT © 2026 Kanak Prabhakar
