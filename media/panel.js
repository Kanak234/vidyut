'use strict';

(function () {
  const vscode = acquireVsCodeApi();

  const canvas  = document.getElementById('canvas');
  const screen  = document.getElementById('screen');
  const hint    = document.getElementById('hint');
  const consoleEl = document.getElementById('console');
  const stateEl = document.getElementById('state');
  const coordsEl = document.getElementById('coords');
  const zoomBtn = document.getElementById('zoomBtn');
  const gridBtn = document.getElementById('gridBtn');
  const saveBtn = document.getElementById('saveBtn');
  const stopBtn = document.getElementById('stopBtn');

  const ctx = canvas.getContext('2d', { alpha: false, willReadFrequently: false });

  const COLOR_NAMES = [
    'BLACK', 'BLUE', 'GREEN', 'CYAN', 'RED', 'MAGENTA', 'BROWN', 'LIGHTGRAY',
    'DARKGRAY', 'LIGHTBLUE', 'LIGHTGREEN', 'LIGHTCYAN', 'LIGHTRED',
    'LIGHTMAGENTA', 'YELLOW', 'WHITE'
  ];

  let width = 640;
  let height = 480;
  let palette = new Uint8Array(48);
  let indices = new Uint8Array(width * height);   // one palette index per pixel
  let image = null;
  let zoomMode = 'fit';
  let gridOn = false;
  let smoothing = false;
  let running = false;

  /* ---------------------------------------------------------------- */
  /* painting                                                         */
  /* ---------------------------------------------------------------- */

  function resize(w, h) {
    width = w;
    height = h;
    canvas.width = w;
    canvas.height = h;
    indices = new Uint8Array(w * h);
    image = ctx.createImageData(w, h);
    const d = image.data;
    for (let i = 3; i < d.length; i += 4) d[i] = 255;   // opaque
    applyZoom();
  }

  function paint() {
    if (!image) return;
    const d = image.data;
    for (let i = 0, p = 0; i < indices.length; i++, p += 4) {
      const c = (indices[i] & 15) * 3;
      d[p]     = palette[c];
      d[p + 1] = palette[c + 1];
      d[p + 2] = palette[c + 2];
    }
    ctx.putImageData(image, 0, 0);
    hint.hidden = true;
  }

  /* Frames arrive run-length encoded as hex: four digits of count, two of
     palette index. Decoding here keeps the wire small on big screens. */
  function decodeFrame(hex) {
    let pos = 0;
    const n = hex.length;
    for (let i = 0; i + 6 <= n; i += 6) {
      const run = parseInt(hex.substr(i, 4), 16);
      const val = parseInt(hex.substr(i + 4, 2), 16);
      if (!(run >= 0) || !(val >= 0)) break;
      const end = Math.min(pos + run, indices.length);
      indices.fill(val, pos, end);
      pos = end;
      if (pos >= indices.length) break;
    }
    paint();
  }

  /* ---------------------------------------------------------------- */
  /* zoom and grid                                                    */
  /* ---------------------------------------------------------------- */

  function applyZoom() {
    if (zoomMode === 'fit') {
      canvas.style.width = '';
      canvas.style.height = '';
      canvas.style.maxWidth = '100%';
      canvas.style.maxHeight = '100%';
    } else {
      const f = parseInt(zoomMode, 10) || 1;
      canvas.style.maxWidth = 'none';
      canvas.style.maxHeight = 'none';
      canvas.style.width = (width * f) + 'px';
      canvas.style.height = (height * f) + 'px';
    }
    canvas.classList.toggle('smooth', smoothing);
    zoomBtn.innerHTML = '<b>F5</b> Zoom: ' + (zoomMode === 'fit' ? 'fit' : zoomMode + '\u00d7');
    updateGridCell();
  }

  function updateGridCell() {
    const shown = canvas.getBoundingClientRect().width || width;
    const scale = shown / width;
    document.documentElement.style.setProperty('--cell', (scale) + 'px');
    /* Below about 3 device pixels per source pixel the grid is just noise. */
    screen.classList.toggle('grid', gridOn && scale >= 3);
  }

  function cycleZoom() {
    const order = ['fit', '1', '2', '3'];
    zoomMode = order[(order.indexOf(zoomMode) + 1) % order.length];
    applyZoom();
  }

  function toggleGrid() {
    gridOn = !gridOn;
    gridBtn.innerHTML = '<b>F6</b> Grid: ' + (gridOn ? 'on' : 'off');
    updateGridCell();
  }

  /* ---------------------------------------------------------------- */
  /* status and console                                               */
  /* ---------------------------------------------------------------- */

  function setState(name, label) {
    stateEl.dataset.state = name;
    stateEl.textContent = label;
  }

  function write(text, cls) {
    const atBottom =
      consoleEl.scrollTop + consoleEl.clientHeight >= consoleEl.scrollHeight - 4;
    const span = document.createElement('span');
    if (cls) span.className = cls;
    span.textContent = text;
    consoleEl.appendChild(span);
    /* Keep the console from growing without bound on chatty programs. */
    while (consoleEl.childNodes.length > 800) {
      consoleEl.removeChild(consoleEl.firstChild);
    }
    if (atBottom) consoleEl.scrollTop = consoleEl.scrollHeight;
  }

  /* ---------------------------------------------------------------- */
  /* pixel inspector                                                  */
  /* ---------------------------------------------------------------- */

  canvas.addEventListener('mousemove', (e) => {
    const r = canvas.getBoundingClientRect();
    const x = Math.floor((e.clientX - r.left) / r.width * width);
    const y = Math.floor((e.clientY - r.top) / r.height * height);
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    const c = indices[y * width + x] & 15;
    coordsEl.textContent = `x ${x}  y ${y}  ${c} ${COLOR_NAMES[c]}`;
  });

  canvas.addEventListener('mouseleave', () => { coordsEl.textContent = ''; });

  /* ---------------------------------------------------------------- */
  /* keyboard into the running program                                */
  /* ---------------------------------------------------------------- */

  function isShortcut(e) {
    if (e.key === 'F5' || e.key === 'F6' || e.key === 'F9') return true;
    if (e.altKey && (e.key === 'x' || e.key === 'X')) return true;
    return false;
  }

  canvas.addEventListener('keydown', (e) => {
    if (isShortcut(e)) return;             // handled by the document listener
    if (e.ctrlKey || e.metaKey || e.altKey) return;
    if (!running) return;

    let text = null;
    if (e.key === 'Enter') text = '\n';
    else if (e.key === 'Tab') text = '\t';
    else if (e.key === 'Backspace') text = '\b';
    else if (e.key === 'Escape') text = '\x1b';
    else if (e.key.length === 1) text = e.key;

    if (text !== null) {
      vscode.postMessage({ type: 'key', text });
      e.preventDefault();
    }
  });

  document.addEventListener('keydown', (e) => {
    if (e.key === 'F5') { cycleZoom(); e.preventDefault(); }
    else if (e.key === 'F6') { toggleGrid(); e.preventDefault(); }
    else if (e.key === 'F9') { requestSave(); e.preventDefault(); }
    else if (e.altKey && (e.key === 'x' || e.key === 'X')) {
      vscode.postMessage({ type: 'stop' });
      e.preventDefault();
    }
  });

  function requestSave() {
    try {
      vscode.postMessage({ type: 'savePng', data: canvas.toDataURL('image/png') });
    } catch (err) {
      vscode.postMessage({ type: 'error', text: String(err && err.message) });
    }
  }

  zoomBtn.addEventListener('click', cycleZoom);
  gridBtn.addEventListener('click', toggleGrid);
  saveBtn.addEventListener('click', requestSave);
  stopBtn.addEventListener('click', () => vscode.postMessage({ type: 'stop' }));

  window.addEventListener('resize', updateGridCell);

  /* ---------------------------------------------------------------- */
  /* messages from the extension                                      */
  /* ---------------------------------------------------------------- */

  window.addEventListener('message', (ev) => {
    const m = ev.data;
    switch (m.type) {
      case 'init':
        resize(m.w, m.h);
        indices.fill(0);
        paint();
        break;

      case 'pal':
        palette = new Uint8Array(m.rgb);
        paint();
        break;

      case 'frame':
        decodeFrame(m.rle);
        break;

      case 'status':
        if (m.s === 'waiting') {
          setState('waiting', 'Press a key');
          canvas.focus({ preventScroll: true });
        } else if (m.s === 'running') {
          setState('running', 'Running');
        }
        break;

      case 'begin':
        running = true;
        consoleEl.textContent = '';
        hint.hidden = false;
        hint.textContent = 'Compiling ' + m.file + '\u2026';
        setState('running', 'Running');
        break;

      case 'output':
        write(m.text, m.err ? 'err' : null);
        break;

      case 'note':
        write(m.text, 'note');
        break;

      case 'exit':
        running = false;
        if (m.code === 0) setState('done', 'Finished');
        else setState('error', 'Exit ' + m.code);
        break;

      case 'failed':
        running = false;
        setState('error', 'Failed');
        hint.hidden = false;
        hint.textContent = m.hint || 'The program did not build.';
        break;

      case 'settings':
        if (typeof m.zoom === 'string') zoomMode = m.zoom;
        if (typeof m.smoothing === 'boolean') smoothing = m.smoothing;
        applyZoom();
        break;

      case 'requestPng':
        requestSave();
        break;
    }
  });

  resize(width, height);
  vscode.postMessage({ type: 'ready' });
})();
