// Copies the freshly built native server binary into the extension so the
// packaged VSIX is self-contained; the serverPath resolver falls back to this
// bundled copy when the repo target directory is absent.
const fs = require('fs');
const path = require('path');

const extensionRoot = path.resolve(__dirname, '..');
const serverSource = path.join(extensionRoot, '..', 'target', 'release', 'voicegroup-lsp');
const serverDirectory = path.join(extensionRoot, 'server');
const serverTarget = path.join(serverDirectory, 'voicegroup-lsp');

if (process.platform !== 'darwin') {
  throw new Error(`voicegroup-lsp-vscode packages a macOS server, got ${process.platform}.`);
}

if (!fs.existsSync(serverSource)) {
  throw new Error(
    `Server binary not found: ${serverSource}\n` +
      'Run cargo build --release in packages/voicegroup-lsp first.'
  );
}

fs.mkdirSync(serverDirectory, { recursive: true });
fs.copyFileSync(serverSource, serverTarget);
fs.chmodSync(serverTarget, 0o755);

console.log(`Bundled ${serverSource} -> ${serverTarget}`);
