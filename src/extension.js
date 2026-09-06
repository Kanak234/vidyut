'use strict';

const vscode = require('vscode');
const fs = require('fs');
const path = require('path');

const runner = require('./runner');
const { Panel } = require('./panel');

let panel = null;
let current = null;        // handle for the program that is running
let lastFile = null;
let output = null;

function log(line) {
  if (output) output.appendLine(line);
}

function setRunning(on) {
  vscode.commands.executeCommand('setContext', 'vidyut.running', on);
}

function pushSettings() {
  const c = vscode.workspace.getConfiguration('vidyut');
  panel.post({
    type: 'settings',
    zoom: c.get('zoom', 'fit'),
    smoothing: c.get('smoothing', false)
  });
}

/** The C or C++ file to run: the active editor, or the last one that ran. */
function targetFile() {
  const ed = vscode.window.activeTextEditor;
  if (ed && ['c', 'cpp'].includes(ed.document.languageId)) return ed.document;
  if (lastFile) {
    return vscode.workspace.textDocuments.find((d) => d.uri.fsPath === lastFile) || null;
  }
  return null;
}

async function stop() {
  if (current) {
    current.kill();
    current = null;
  }
  setRunning(false);
}

async function runCommand() {
  const doc = targetFile();
  if (!doc) {
    vscode.window.showWarningMessage(
      'Open a C or C++ file first, then run VIDYUT from that editor.');
    return;
  }
  if (doc.isDirty) await doc.save();

  await stop();

  const filePath = doc.uri.fsPath;
  lastFile = filePath;

  panel.reveal('VIDYUT \u2014 ' + path.basename(filePath));
  pushSettings();
  panel.post({ type: 'begin', file: path.basename(filePath) });

  let exe;
  try {
    exe = await runner.compile(context_, filePath, log);
  } catch (e) {
    if (e.kind === 'no-compiler') {
      panel.post({ type: 'failed', hint: 'No C compiler found.' });
      panel.post({ type: 'output', text: e.message + '\n', err: true });
      vscode.window.showErrorMessage(e.message, 'Check My Compiler')
        .then((pick) => {
          if (pick) vscode.commands.executeCommand('vidyut.checkCompiler');
        });
    } else {
      panel.post({ type: 'failed', hint: 'The program did not compile.' });
      panel.post({ type: 'output', text: (e.diagnostics || e.message) + '\n', err: true });
    }
    log(e.message);
    return;
  }

  setRunning(true);
  current = runner.launch(exe, path.dirname(filePath), {
    onMessage(m) {
      panel.post(m);
    },
    onOutput(text, isErr) {
      panel.post({ type: 'output', text, err: !!isErr });
    },
    onExit(code, signal) {
      current = null;
      setRunning(false);
      if (signal) {
        panel.post({ type: 'note', text: `\nStopped (${signal}).\n` });
        panel.post({ type: 'exit', code: 1 });
      } else {
        panel.post({ type: 'note', text: `\nProgram finished with exit code ${code}.\n` });
        panel.post({ type: 'exit', code });
      }
    }
  });

  panel.onKey = (text) => { if (current) current.send(text); };
  panel.onStop = () => { stop(); };
}

async function checkCompiler() {
  const chan = output;
  chan.show(true);
  chan.appendLine('Checking for a C compiler...');
  for (const cmd of ['gcc', 'g++', 'clang', 'cc']) {
    const r = await runner.run(cmd, ['--version'], {});
    if (r.code === 0) {
      chan.appendLine(`  ${cmd}: ${(r.out || r.err).split('\n')[0]}`);
    } else {
      chan.appendLine(`  ${cmd}: not found`);
    }
  }
  chan.appendLine('');
  chan.appendLine('If nothing was found: ' + runner.installHint());
}

async function openExample() {
  const dir = path.join(context_.extensionPath, 'examples');
  let files;
  try {
    files = fs.readdirSync(dir).filter((f) => /\.(c|cpp)$/.test(f)).sort();
  } catch (_) {
    vscode.window.showErrorMessage('The examples folder is missing from the extension.');
    return;
  }
  const pick = await vscode.window.showQuickPick(
    files.map((f) => ({
      label: f,
      description: EXAMPLE_BLURBS[f] || ''
    })),
    { placeHolder: 'Which example would you like to open?' }
  );
  if (!pick) return;

  const target = await vscode.window.showSaveDialog({
    defaultUri: vscode.Uri.file(path.join(
      vscode.workspace.workspaceFolders
        ? vscode.workspace.workspaceFolders[0].uri.fsPath
        : require('os').homedir(),
      pick.label)),
    saveLabel: 'Save example'
  });
  if (!target) return;

  fs.copyFileSync(path.join(dir, pick.label), target.fsPath);
  const doc = await vscode.workspace.openTextDocument(target);
  await vscode.window.showTextDocument(doc);
  vscode.window.showInformationMessage(
    'Press Ctrl+Alt+G to run it.', 'Run now').then((p) => {
      if (p) vscode.commands.executeCommand('vidyut.run');
    });
}

const EXAMPLE_BLURBS = {
  'bouncing_ball.c': 'Animation with delay and XOR erase',
  'moving_car.c': 'The classic lab exercise, drawn step by step',
  'analog_clock.c': 'Trigonometry, arcs and a live second hand',
  'bar_chart.c': 'Fill patterns, axes and labels',
  'dda_line.c': 'DDA line drawing, one pixel at a time',
  'bresenham_circle.c': 'Midpoint circle algorithm from scratch',
  'flood_fill_demo.c': 'Boundary fill inside a hand-drawn shape',
  'keyboard_paint.c': 'Move with the arrow keys and draw'
};

/**
 * Write a c_cpp_properties.json include path so IntelliSense stops
 * underlining graphics.h in red.
 */
async function setUpProject() {
  const folders = vscode.workspace.workspaceFolders;
  if (!folders || !folders.length) {
    vscode.window.showWarningMessage('Open a folder first, then run this command.');
    return;
  }
  const root = folders[0].uri.fsPath;
  const dir = path.join(root, '.vscode');
  const file = path.join(dir, 'c_cpp_properties.json');
  const rtDir = runner.runtimeDir(context_);

  let json = { configurations: [], version: 4 };
  if (fs.existsSync(file)) {
    try {
      json = JSON.parse(fs.readFileSync(file, 'utf8'));
    } catch (_) {
      const go = await vscode.window.showWarningMessage(
        'c_cpp_properties.json could not be read. Replace it?', 'Replace', 'Cancel');
      if (go !== 'Replace') return;
      json = { configurations: [], version: 4 };
    }
  }
  if (!Array.isArray(json.configurations)) json.configurations = [];
  if (!json.configurations.length) {
    json.configurations.push({ name: 'VIDYUT', includePath: ['${workspaceFolder}/**'] });
  }
  for (const cfg of json.configurations) {
    cfg.includePath = cfg.includePath || [];
    if (!cfg.includePath.includes(rtDir)) cfg.includePath.push(rtDir);
  }

  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(file, JSON.stringify(json, null, 2) + '\n');
  vscode.window.showInformationMessage(
    'This folder now knows where graphics.h lives. IntelliSense should be happy.');
}

let context_ = null;

function activate(context) {
  context_ = context;
  output = vscode.window.createOutputChannel('VIDYUT');
  panel = new Panel(context);
  setRunning(false);

  context.subscriptions.push(
    output,
    vscode.commands.registerCommand('vidyut.run', runCommand),
    vscode.commands.registerCommand('vidyut.stop', stop),
    vscode.commands.registerCommand('vidyut.openExample', openExample),
    vscode.commands.registerCommand('vidyut.setUpProject', setUpProject),
    vscode.commands.registerCommand('vidyut.checkCompiler', checkCompiler),
    vscode.commands.registerCommand('vidyut.savePng', () => panel.requestPng()),
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (e.affectsConfiguration('vidyut') && panel.visible) pushSettings();
    })
  );
}

function deactivate() {
  if (current) current.kill();
  if (panel) panel.dispose();
}

module.exports = { activate, deactivate };
