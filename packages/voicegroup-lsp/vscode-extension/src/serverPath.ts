import * as fs from 'fs';
import * as path from 'path';

export interface ResolvedServerPath {
  path: string;
  checkedPaths: string[];
  found: boolean;
}

// Resolves the Rust server binary path so the VS Code activation stays launcher-only.
export function resolveServerPath(
  extensionPath: string,
  configuredPath: string,
  exists: (candidate: string) => boolean = fs.existsSync
): ResolvedServerPath {
  if (configuredPath.length > 0) {
    return {
      path: configuredPath,
      checkedPaths: [configuredPath],
      found: exists(configuredPath)
    };
  }

  // Prefer freshly built repo binaries during development; fall back to the
  // server binary bundled into the extension for packaged installs.
  const checkedPaths = [
    path.resolve(extensionPath, '..', 'target', 'release', 'voicegroup-lsp'),
    path.resolve(extensionPath, '..', 'target', 'debug', 'voicegroup-lsp'),
    path.join(extensionPath, 'server', 'voicegroup-lsp')
  ];
  const existingPath = checkedPaths.find(candidate => exists(candidate));

  return {
    path: existingPath ?? checkedPaths[0],
    checkedPaths,
    found: existingPath !== undefined
  };
}
