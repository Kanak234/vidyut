#!/usr/bin/env node
/* Regression tests for the VIDYUT runtime.
 * Each case compiles a small C program, runs it, decodes the frames it
 * emits, and checks pixels. Run with: npm test
 */
'use strict';

const cp = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const ROOT = path.join(__dirname, '..');
const RT = path.join(ROOT, 'runtime');
const TMP = fs.mkdtempSync(path.join(os.tmpdir(), 'vidyut-test-'));

let passed = 0;
let failed = 0;

function check(name, cond, detail) {
  if (cond) {
    passed++;
    console.log(`  ok   ${name}`);
  } else {
    failed++;
    console.log(`  FAIL ${name}${detail ? ' - ' + detail : ''}`);
  }
}

function compilerCmd() {
  for (const c of ['gcc', 'clang', 'cc']) {
    const r = cp.spawnSync(c, ['--version'], { encoding: 'utf8' });
    if (r.status === 0) return c;
  }
  console.error('No C compiler found; cannot run the tests.');
  process.exit(2);
}

const CC = compilerCmd();
const runtimeSources = fs.readdirSync(RT).filter((f) => f.endsWith('.c'))
  .map((f) => path.join(RT, f));

/** Compile and run a program, returning its decoded frames and stdout. */
function build(name, source, stdin) {
  const src = path.join(TMP, name + '.c');
  const exe = path.join(TMP, name);
  fs.writeFileSync(src, source);

  const c = cp.spawnSync(CC, ['-std=c99', '-I', RT, src, ...runtimeSources,
                              '-o', exe, '-lm'], { encoding: 'utf8' });
  if (c.status !== 0) {
    return { compileError: c.stderr || c.stdout };
  }

  const r = cp.spawnSync(exe, [], {
    encoding: 'utf8',
    input: stdin === undefined ? '' : stdin,
    timeout: 20000,
    maxBuffer: 64 * 1024 * 1024
  });

  const frames = [];
  let out = '';
  let w = 0, h = 0, pal = [];

  for (const line of (r.stdout || '').split('\n')) {
    if (!line.startsWith('@vidyut ')) {
      if (line) out += line + '\n';
      continue;
    }
    let m;
    try { m = JSON.parse(line.slice(8)); } catch (_) { continue; }
    if (m.t === 'init') { w = m.w; h = m.h; }
    else if (m.t === 'pal') { pal = m.rgb; }
    else if (m.t === 'frame') frames.push(decode(m.rle, w, h));
  }
  return { frames, out, w, h, pal, exit: r.status, stderr: r.stderr };
}

function decode(hex, w, h) {
  const fb = new Uint8Array(w * h);
  let pos = 0;
  for (let i = 0; i + 6 <= hex.length; i += 6) {
    const run = parseInt(hex.substr(i, 4), 16);
    const val = parseInt(hex.substr(i + 4, 2), 16);
    fb.fill(val, pos, Math.min(pos + run, fb.length));
    pos += run;
  }
  return { fb, w, h, covered: pos, at(x, y) { return this.fb[y * this.w + x]; } };
}

function last(res) { return res.frames[res.frames.length - 1]; }

/* ------------------------------------------------------------------ */

console.log('VIDYUT runtime tests\n');

console.log('screen and protocol');
{
  const r = build('screen', `
#include <stdio.h>
#include <graphics.h>
int main(void){int gd=DETECT,gm;initgraph(&gd,&gm,"");
printf("%d %d\\n",getmaxx(),getmaxy());closegraph();return 0;}
`);
  check('compiles', !r.compileError, r.compileError);
  if (!r.compileError) {
    check('reports a 640x480 screen', r.w === 640 && r.h === 480, `${r.w}x${r.h}`);
    check('getmaxx/getmaxy are one less', r.out.trim() === '639 479', r.out.trim());
    check('every frame covers all pixels',
          r.frames.every((f) => f.covered === f.w * f.h));
    check('palette has 16 colours', r.pal.length === 48, String(r.pal.length));
  }
}

console.log('\npixels and colour');
{
  const r = build('pixels', `
#include <graphics.h>
#include <stdio.h>
int main(void){int gd=DETECT,gm;initgraph(&gd,&gm,"");
putpixel(10,10,LIGHTRED); putpixel(639,479,YELLOW);
printf("A%u\\n", getpixel(10,10));
printf("B%u\\n", getpixel(11,10));
putpixel(-5,-5,WHITE);           /* must not crash or wrap */
printf("C%u\\n", getpixel(639,479));
closegraph();return 0;}
`);
  check('compiles', !r.compileError, r.compileError);
  if (!r.compileError) {
    const f = last(r);
    check('putpixel writes the pixel', f.at(10, 10) === 12, String(f.at(10, 10)));
    check('neighbour is untouched', f.at(11, 10) === 0);
    check('bottom-right corner is reachable', f.at(639, 479) === 14);
    check('getpixel round-trips', r.out.includes('A12') && r.out.includes('C14'), r.out.trim());
    check('off-screen writes are ignored', r.exit === 0);
  }
}

console.log('\nlines and shapes');
{
  const r = build('shapes', `
#include <graphics.h>
int main(void){int gd=DETECT,gm;initgraph(&gd,&gm,"");
setcolor(WHITE); line(100,100,200,100);
setcolor(LIGHTGREEN); rectangle(300,100,400,200);
setcolor(YELLOW); circle(150,300,50);
closegraph();return 0;}
`);
  check('compiles', !r.compileError, r.compileError);
  if (!r.compileError) {
    const f = last(r);
    check('horizontal line is drawn end to end',
          f.at(100, 100) === 15 && f.at(150, 100) === 15 && f.at(200, 100) === 15);
    check('line has no stray pixels above', f.at(150, 99) === 0);
    check('rectangle draws all four corners',
          f.at(300, 100) === 10 && f.at(400, 100) === 10 &&
          f.at(300, 200) === 10 && f.at(400, 200) === 10);
    check('rectangle interior stays empty', f.at(350, 150) === 0);
    check('circle touches its four extremes',
          f.at(150, 250) === 14 && f.at(150, 350) === 14 &&
          f.at(100, 300) === 14 && f.at(200, 300) === 14);
  }
}

console.log('\nfills');
{
  const r = build('fills', `
#include <graphics.h>
int main(void){int gd=DETECT,gm;initgraph(&gd,&gm,"");
setcolor(WHITE); rectangle(50,50,250,250);
setfillstyle(SOLID_FILL,LIGHTBLUE); floodfill(150,150,WHITE);
setfillstyle(SOLID_FILL,LIGHTGREEN); bar(300,50,500,250);
closegraph();return 0;}
`);
  check('compiles', !r.compileError, r.compileError);
  if (!r.compileError) {
    const f = last(r);
    check('flood fill reaches the middle', f.at(150, 150) === 9, String(f.at(150, 150)));
    check('flood fill reaches the corners', f.at(52, 52) === 9 && f.at(248, 248) === 9);
    check('flood fill does not leak outside', f.at(20, 20) === 0 && f.at(280, 150) === 0);
    check('boundary is preserved', f.at(50, 150) === 15);
    check('bar fills solidly', f.at(400, 150) === 10 && f.at(301, 51) === 10);
  }
}

console.log('\npattern fill terminates');
{
  /* A patterned fill leaves some pixels at the seed colour; a fill that
     tracked membership by colour alone would loop here forever. */
  const r = build('pattern', `
#include <graphics.h>
int main(void){int gd=DETECT,gm;initgraph(&gd,&gm,"");
setcolor(WHITE); rectangle(20,20,600,400);
setfillstyle(HATCH_FILL,LIGHTCYAN); floodfill(300,200,WHITE);
closegraph();return 0;}
`);
  check('compiles', !r.compileError, r.compileError);
  if (!r.compileError) {
    check('finishes without hanging', r.exit === 0, 'exit ' + r.exit);
    const f = last(r);
    let cyan = 0;
    for (let i = 0; i < f.fb.length; i++) if (f.fb[i] === 11) cyan++;
    check('pattern actually painted', cyan > 1000, cyan + ' pixels');
  }
}

console.log('\nXOR write mode');
{
  const r = build('xormode', `
#include <graphics.h>
int main(void){int gd=DETECT,gm;initgraph(&gd,&gm,"");
setwritemode(XOR_PUT); setcolor(WHITE);
line(50,50,250,150);
line(50,50,250,150);   /* drawing twice must restore the screen */
circle(400,200,60);
circle(400,200,60);
closegraph();return 0;}
`);
  check('compiles', !r.compileError, r.compileError);
  if (!r.compileError) {
    const f = last(r);
    let lit = 0;
    for (let i = 0; i < f.fb.length; i++) if (f.fb[i] !== 0) lit++;
    check('XOR line drawn twice erases itself', lit === 0, lit + ' pixels left');
  }
}

console.log('\ntext');
{
  const r = build('text', `
#include <graphics.h>
#include <stdio.h>
int main(void){int gd=DETECT,gm;initgraph(&gd,&gm,"");
setcolor(WHITE); outtextxy(10,10,"Hi");
printf("W%d H%d\\n", textwidth("Hi"), textheight("Hi"));
settextstyle(DEFAULT_FONT,HORIZ_DIR,2);
printf("W%d H%d\\n", textwidth("Hi"), textheight("Hi"));
settextstyle(DEFAULT_FONT,VERT_DIR,1);
outtextxy(300,300,"Up");
closegraph();return 0;}
`);
  check('compiles', !r.compileError, r.compileError);
  if (!r.compileError) {
    const f = last(r);
    let lit = 0;
    for (let y = 10; y < 18; y++) for (let x = 10; x < 26; x++) if (f.at(x, y)) lit++;
    check('text puts pixels in its own cell', lit > 10, lit + ' pixels');
    check('metrics at size 1', r.out.includes('W16 H8'), r.out.trim());
    check('metrics double at size 2', r.out.includes('W32 H16'), r.out.trim());

    let up = 0;
    for (let y = 280; y <= 300; y++) for (let x = 295; x < 315; x++) if (f.at(x, y)) up++;
    check('vertical text stays on screen', up > 10, up + ' pixels');
  }
}

console.log('\nviewport clipping');
{
  const r = build('viewport', `
#include <graphics.h>
int main(void){int gd=DETECT,gm;initgraph(&gd,&gm,"");
setviewport(100,100,300,300,1);
setcolor(LIGHTRED);
line(-500,50,500,50);      /* must be clipped to the viewport */
closegraph();return 0;}
`);
  check('compiles', !r.compileError, r.compileError);
  if (!r.compileError) {
    const f = last(r);
    check('drawing is relative to the viewport origin', f.at(150, 150) === 12);
    check('clipped on the left', f.at(50, 150) === 0);
    check('clipped on the right', f.at(400, 150) === 0);
  }
}

console.log('\nimage capture');
{
  const r = build('image', `
#include <graphics.h>
#include <stdlib.h>
int main(void){int gd=DETECT,gm;void *buf;initgraph(&gd,&gm,"");
setfillstyle(SOLID_FILL,YELLOW); bar(10,10,40,40);
buf = malloc(imagesize(10,10,40,40));
getimage(10,10,40,40,buf);
putimage(200,200,buf,COPY_PUT);
free(buf); closegraph(); return 0;}
`);
  check('compiles', !r.compileError, r.compileError);
  if (!r.compileError) {
    const f = last(r);
    check('captured block is replayed elsewhere', f.at(215, 215) === 14, String(f.at(215, 215)));
    check('original block is still there', f.at(25, 25) === 14);
  }
}

console.log('\nkeyboard');
{
  const r = build('keys', `
#include <graphics.h>
#include <conio.h>
#include <stdio.h>
int main(void){int gd=DETECT,gm;int a,b;initgraph(&gd,&gm,"");
a=getch(); b=getch();
printf("got %c%c\\n",a,b);
closegraph();return 0;}
`, 'XY');
  check('compiles', !r.compileError, r.compileError);
  if (!r.compileError) {
    check('getch reads keys one at a time', r.out.includes('got XY'), r.out.trim());
  }
}

console.log('\ndrawing without initgraph');
{
  const r = build('noinit', `
#include <graphics.h>
int main(void){ setcolor(WHITE); line(0,0,100,100); return 0; }
`);
  check('compiles', !r.compileError, r.compileError);
  if (!r.compileError) {
    check('still produces a picture', r.frames.length > 0 && r.exit === 0);
    if (r.frames.length) check('the line is there', last(r).at(50, 50) === 15);
  }
}

console.log('\nexample programs compile');
{
  const dir = path.join(ROOT, 'examples');
  for (const f of fs.readdirSync(dir).filter((x) => /\.(c|cpp)$/.test(x)).sort()) {
    const isCpp = f.endsWith('.cpp');
    const cc = isCpp ? 'g++' : CC;
    const r = cp.spawnSync(cc, ['-I', RT, path.join(dir, f), ...runtimeSources,
                                '-o', path.join(TMP, f + '.out'), '-lm'],
                           { encoding: 'utf8' });
    check(f, r.status === 0, (r.stderr || '').split('\n')[0]);
  }
}

/* ------------------------------------------------------------------ */

fs.rmSync(TMP, { recursive: true, force: true });
console.log(`\n${passed} passed, ${failed} failed`);
process.exit(failed ? 1 : 0);
