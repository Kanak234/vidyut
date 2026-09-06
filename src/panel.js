'use strict';

const vscode = require('vscode');
const fs = require('fs');
const path = require('path');

function nonce() {
  const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
  let s = '';
  for (let i = 0; i < 32; i++) s += chars[Math.floor(Math.random() * chars.length)];
  return s;
}

class Panel {
  constructor(context) {
    this.context = context;
    this.panel = null;
    this.onKey = null;
    this.onStop = null;
    this.onRerun = null;
    this.pending = [];
  }

  get visible() {
    return this.panel !== null;
  }

  reveal(title) {
    const beside = vscode.workspace.getConfiguration('vidyut').get('openPanelBeside', true);
    const column = beside ? vscode.ViewColumn.Beside : vscode.ViewColumn.Active;

    if (this.panel) {
      this.panel.title = title;
      this.panel.reveal(column, true);
      return;
    }

    this.panel = vscode.window.createWebviewPanel(
      'vidyut.screen',
      title,
      { viewColumn: column, preserveFocus: true },
      {
        enableScripts: true,
        retainContextWhenHidden: true,
        localResourceRoots: [vscode.Uri.file(path.join(this.context.extensionPath, 'media'))]
      }
    );

    this.panel.webview.html = this.html();

    this.panel.webview.onDidReceiveMessage((m) => {
      if (m.type === 'key' && this.onKey) this.onKey(m.text);
      else if (m.type === 'stop' && this.onStop) this.onStop();
      else if (m.type === 'rerun' && this.onRerun) this.onRerun();
      else if (m.type === 'ready') this.drain();
      else if (m.type === 'savePng') this.savePng(m.data);
      else if (m.type === 'error') {
        vscode.window.showErrorMessage('VIDYUT panel: ' + m.text);
      }
    }, null, this.context.subscriptions);

    this.panel.onDidDispose(() => {
      this.panel = null;
      if (this.onStop) this.onStop();
    }, null, this.context.subscriptions);
  }

  post(msg) {
    if (!this.panel) {
      this.pending.push(msg);
      return;
    }
    this.panel.webview.postMessage(msg);
  }

  drain() {
    const queued = this.pending;
    this.pending = [];
    for (const m of queued) this.post(m);
  }

  async savePng(dataUrl) {
    try {
      const b64 = String(dataUrl).split(',')[1];
      const uri = await vscode.window.showSaveDialog({
        filters: { 'PNG image': ['png'] },
        saveLabel: 'Save picture'
      });
      if (!uri) return;
      fs.writeFileSync(uri.fsPath, Buffer.from(b64, 'base64'));
      vscode.window.showInformationMessage(`Saved ${path.basename(uri.fsPath)}.`);
    } catch (e) {
      vscode.window.showErrorMessage('Could not save the picture: ' + e.message);
    }
  }

  requestPng() {
    this.post({ type: 'requestPng' });
  }

  dispose() {
    if (this.panel) this.panel.dispose();
    this.panel = null;
  }

  html() {
    const media = (f) =>
      this.panel.webview.asWebviewUri(
        vscode.Uri.file(path.join(this.context.extensionPath, 'media', f)));
    const n = nonce();
    const csp = [
      `default-src 'none'`,
      `img-src ${this.panel.webview.cspSource} data:`,
      `style-src ${this.panel.webview.cspSource}`,
      `script-src 'nonce-${n}'`,
      `font-src ${this.panel.webview.cspSource}`
    ].join('; ');

    return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta http-equiv="Content-Security-Policy" content="${csp}">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="stylesheet" href="${media('panel.css')}">
<title>VIDYUT</title>
</head>
<body>
  <div class="frame">
    <div class="titlebar">
      <span class="rule" aria-hidden="true"></span>
      <h1>VIDYUT &mdash; graphics.h</h1>
      <span class="rule" aria-hidden="true"></span>
    </div>

    <div class="screen" id="screen">
      <canvas id="canvas" width="640" height="480" tabindex="0"
              aria-label="Program graphics output"></canvas>
      <p class="hint" id="hint">Press Ctrl+Alt+G in a C file to draw.</p>
    </div>

    <div class="console" id="console" aria-live="polite" aria-label="Program output"></div>

    <div class="statusbar">
      <span class="state" id="state" data-state="idle">Idle</span>
      <span class="coords" id="coords"></span>
      <span class="spacer"></span>
      <button type="button" id="zoomBtn" class="fkey"><b>F5</b> Zoom: fit</button>
      <button type="button" id="gridBtn" class="fkey"><b>F6</b> Grid: off</button>
      <button type="button" id="saveBtn" class="fkey"><b>F9</b> Save PNG</button>
      <button type="button" id="stopBtn" class="fkey"><b>Alt+X</b> Stop</button>
    </div>
  </div>
<script nonce="${n}" src="${media('panel.js')}"></script>
</body>
</html>`;
  }
}

module.exports = { Panel };
