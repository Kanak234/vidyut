'use strict';

const vscode = require('vscode');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');

const RUNTIME_SOURCES = [
  'vidyut_core.c',
  'vidyut_draw.c',
  'vidyut_text.c',
  'vidyut_conio.c',
  'vidyut_boot.c'
];

/** Where compiled runtime objects and program binaries live. */
function workDir(context) {
  const dir = path.join(context.globalStorageUri.fsPath, 'build');
  fs.mkdirSync(dir, { recursive: true });
  return dir;
}

function runtimeDir(context) {
  return path.join(context.extensionPath, 'runtime');
}

function exeName(base) {
  return process.platform === 'win32' ? base + '.exe' : base;
}

function config() {
  return vscode.workspace.getConfiguration('vidyut');
}

function compilerFor(filePath) {
  const ext = path.extname(filePath).toLowerCase();
  const c = config();
  if (ext === '.cpp' || ext === '.cc' || ext === '.cxx' || ext === '.c++') {
    return { cmd: c.get('compilerCpp', 'g++'), isCpp: true };
  }
  return { cmd: c.get('compilerC', 'gcc'), isCpp: false };
}

function run(cmd, args, opts) {
  return new Promise((resolve) => {
    let out = '';
    let err = '';
    let child;
    try {
      child = cp.spawn(cmd, args, Object.assign({ shell: false }, opts));
    } catch (e) {
      resolve({ code: -1, out: '', err: String(e && e.message ? e.message : e) });
      return;
    }
    child.on('error', (e) => resolve({ code: -1, out, err: err + String(e.message) }));
    child.stdout.on('data', (d) => { out += d.toString(); });
    child.stderr.on('data', (d) => { err += d.toString(); });
    child.on('close', (code) => resolve({ code, out, err }));
  });
}

/** A short fingerprint so cached objects are rebuilt when the compiler changes. */
async function compilerFingerprint(cmd) {
  const r = await run(cmd, ['--version'], {});
  if (r.code !== 0) return null;
  return (r.out || r.err).split('\n')[0].trim();
}

function installHint() {
  if (process.platform === 'win32') {
    return 'Install MinGW-w64 (for example with "winget install -e --id MSYS2.MSYS2", ' +
           'then "pacman -S mingw-w64-ucrt-x86_64-gcc") and make sure gcc is on your PATH.';
  }
  if (process.platform === 'darwin') {
    return 'Run "xcode-select --install" to get the command line compilers.';
  }
  return 'Install the compilers with "sudo apt install build-essential" ' +
         'or the equivalent for your distribution.';
}

/**
 * Build the runtime once per compiler and cache the objects. Returns the
 * list of object files to link against.
 */
async function ensureRuntime(context, compiler, log) {
  const fp = await compilerFingerprint(compiler);
  if (fp === null) {
    const e = new Error(
      `Could not run "${compiler}". ${installHint()}`);
    e.kind = 'no-compiler';
    throw e;
  }

  const key = Buffer.from(fp).toString('hex').slice(0, 24);
  const objDir = path.join(workDir(context), 'rt-' + key);
  fs.mkdirSync(objDir, { recursive: true });

  const rtDir = runtimeDir(context);
  const objects = [];

  for (const src of RUNTIME_SOURCES) {
    const srcPath = path.join(rtDir, src);
    const objPath = path.join(objDir, src.replace(/\.c$/, '.o'));
    objects.push(objPath);

    let fresh = false;
    try {
      fresh = fs.statSync(objPath).mtimeMs >= fs.statSync(srcPath).mtimeMs;
    } catch (_) {
      fresh = false;
    }
    if (fresh) continue;

    log(`Building runtime: ${src}`);
    const r = await run(compiler, ['-c', '-O2', '-I', rtDir, srcPath, '-o', objPath], {});
    if (r.code !== 0) {
      const e = new Error(`Could not build the VIDYUT runtime.\n${r.err}`);
      e.kind = 'runtime-build';
      throw e;
    }
  }
  return objects;
}

/**
 * Compile the user's file. Resolves with the executable path, or rejects
 * with an error carrying the compiler's own diagnostics.
 */
async function compile(context, filePath, log) {
  const { cmd } = compilerFor(filePath);
  const objects = await ensureRuntime(context, cmd, log);
  const rtDir = runtimeDir(context);
  const out = path.join(workDir(context), exeName(
    path.basename(filePath).replace(/[^A-Za-z0-9_.-]/g, '_').replace(/\.[^.]+$/, '')));

  const args = [
    '-I', rtDir,
    ...config().get('extraCompilerArgs', []),
    filePath,
    ...objects,
    '-o', out
  ];
  if (process.platform !== 'win32') args.push('-lm');

  log(`$ ${cmd} ${args.join(' ')}`);
  const r = await run(cmd, args, { cwd: path.dirname(filePath) });
  if (r.code !== 0) {
    const e = new Error(r.err || r.out || 'Compilation failed.');
    e.kind = 'compile';
    e.diagnostics = r.err || r.out || '';
    throw e;
  }
  if (r.err.trim()) log(r.err.trim());
  return out;
}

/**
 * Run a compiled program, splitting the protocol lines out of its output.
 * Returns a handle with kill() and sendKey().
 */
function launch(exePath, cwd, handlers) {
  const child = cp.spawn(exePath, [], {
    cwd,
    env: Object.assign({}, process.env, { VIDYUT_PANEL: '1' }),
    stdio: ['pipe', 'pipe', 'pipe']
  });

  let stdoutTail = '';
  let stderrTail = '';

  const consumeStdout = (chunk) => {
    stdoutTail += chunk.toString();
    let nl;
    while ((nl = stdoutTail.indexOf('\n')) !== -1) {
      const line = stdoutTail.slice(0, nl).replace(/\r$/, '');
      stdoutTail = stdoutTail.slice(nl + 1);
      if (line.startsWith('@vidyut ')) {
        let msg = null;
        try {
          msg = JSON.parse(line.slice(8));
        } catch (_) {
          /* A malformed frame is dropped rather than killing the run. */
        }
        if (msg) handlers.onMessage(msg);
      } else {
        handlers.onOutput(line + '\n');
      }
    }
    /* A prompt without a trailing newline should still appear. */
    if (stdoutTail.length && !stdoutTail.startsWith('@vidyut')) {
      handlers.onOutput(stdoutTail);
      stdoutTail = '';
    }
  };

  child.stdout.on('data', consumeStdout);
  child.stderr.on('data', (d) => {
    stderrTail += d.toString();
    handlers.onOutput(d.toString(), true);
  });

  let timer = null;
  const timeout = vscode.workspace.getConfiguration('vidyut').get('runTimeout', 120);
  if (timeout > 0) {
    timer = setTimeout(() => {
      handlers.onOutput(
        `\nStopped after ${timeout} seconds. ` +
        `Raise vidyut.runTimeout if the program needs longer.\n`, true);
      try { child.kill(); } catch (_) { /* already gone */ }
    }, timeout * 1000);
  }

  child.on('close', (code, signal) => {
    if (timer) clearTimeout(timer);
    if (stdoutTail) handlers.onOutput(stdoutTail);
    handlers.onExit(code, signal, stderrTail);
  });

  child.on('error', (e) => {
    if (timer) clearTimeout(timer);
    handlers.onOutput(`Could not start the program: ${e.message}\n`, true);
    handlers.onExit(-1, null, stderrTail);
  });

  return {
    kill() {
      try { child.kill(); } catch (_) { /* already gone */ }
    },
    send(text) {
      try {
        if (child.stdin.writable) child.stdin.write(text);
      } catch (_) { /* the program stopped reading */ }
    },
    closeInput() {
      try { child.stdin.end(); } catch (_) { /* already closed */ }
    }
  };
}

module.exports = { compile, launch, workDir, runtimeDir, compilerFor, installHint, run };
