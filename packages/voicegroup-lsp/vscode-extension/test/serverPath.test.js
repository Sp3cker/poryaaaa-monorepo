const assert = require('node:assert/strict');
const test = require('node:test');
const path = require('node:path');

const { resolveServerPath } = require('../out/serverPath.js');

function resolver(existingPaths) {
  const existing = new Set(existingPaths);
  return candidate => existing.has(candidate);
}

test('uses configured serverPath without probing default candidates', () => {
  const configured = '/custom/bin/voicegroup-lsp';
  const result = resolveServerPath('/extension/root', configured, resolver([configured]));

  assert.deepEqual(result, {
    path: configured,
    checkedPaths: [configured],
    found: true
  });
});

test('prefers release binary before debug and bundled server when serverPath is empty', () => {
  const extensionPath = '/repo/packages/voicegroup-lsp/vscode-extension';
  const release = path.resolve(extensionPath, '..', 'target', 'release', 'voicegroup-lsp');
  const debug = path.resolve(extensionPath, '..', 'target', 'debug', 'voicegroup-lsp');
  const bundled = path.join(extensionPath, 'server', 'voicegroup-lsp');

  const result = resolveServerPath(extensionPath, '', resolver([release, debug]));

  assert.deepEqual(result, {
    path: release,
    checkedPaths: [release, debug, bundled],
    found: true
  });
});

test('uses debug binary when release is missing', () => {
  const extensionPath = '/repo/packages/voicegroup-lsp/vscode-extension';
  const release = path.resolve(extensionPath, '..', 'target', 'release', 'voicegroup-lsp');
  const debug = path.resolve(extensionPath, '..', 'target', 'debug', 'voicegroup-lsp');
  const bundled = path.join(extensionPath, 'server', 'voicegroup-lsp');

  const result = resolveServerPath(extensionPath, '', resolver([debug]));

  assert.deepEqual(result, {
    path: debug,
    checkedPaths: [release, debug, bundled],
    found: true
  });
});

test('uses bundled server when repo binaries are absent', () => {
  const extensionPath = '/repo/packages/voicegroup-lsp/vscode-extension';
  const release = path.resolve(extensionPath, '..', 'target', 'release', 'voicegroup-lsp');
  const debug = path.resolve(extensionPath, '..', 'target', 'debug', 'voicegroup-lsp');
  const bundled = path.join(extensionPath, 'server', 'voicegroup-lsp');

  const result = resolveServerPath(extensionPath, '', resolver([bundled]));

  assert.deepEqual(result, {
    path: bundled,
    checkedPaths: [release, debug, bundled],
    found: true
  });
});

test('reports all default candidates when no binary exists', () => {
  const extensionPath = '/repo/packages/voicegroup-lsp/vscode-extension';
  const release = path.resolve(extensionPath, '..', 'target', 'release', 'voicegroup-lsp');
  const debug = path.resolve(extensionPath, '..', 'target', 'debug', 'voicegroup-lsp');
  const bundled = path.join(extensionPath, 'server', 'voicegroup-lsp');

  const result = resolveServerPath(extensionPath, '', resolver([]));

  assert.deepEqual(result, {
    path: release,
    checkedPaths: [release, debug, bundled],
    found: false
  });
});
