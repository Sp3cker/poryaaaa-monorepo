const childProcess = require('child_process');
const fs = require('fs');
const path = require('path');

const extensionRoot = path.resolve(__dirname, '..');
const packageRoot = path.resolve(extensionRoot, '..');
const serverSource = path.join(packageRoot, 'Sources', 'voicegroup-lsp-cpp', 'main.cpp');
const buildDirectory = path.join(packageRoot, '.build', 'cpp');
const builtServer = path.join(buildDirectory, 'voicegroup-lsp');
const serverDirectory = path.join(extensionRoot, 'server');
const serverTarget = path.join(serverDirectory, 'voicegroup-lsp');

if (process.platform !== 'darwin' || process.arch !== 'arm64') {
  throw new Error(`voicegroup-lsp-vscode packages a darwin-arm64 server, got ${process.platform}-${process.arch}.`);
}

fs.mkdirSync(buildDirectory, { recursive: true });

childProcess.execFileSync('c++', [
  '-std=c++17',
  '-O2',
  '-Wall',
  '-Wextra',
  '-pedantic',
  serverSource,
  '-o',
  builtServer
], {
  stdio: 'inherit'
});

fs.mkdirSync(serverDirectory, { recursive: true });
fs.copyFileSync(builtServer, serverTarget);
fs.chmodSync(serverTarget, 0o755);
