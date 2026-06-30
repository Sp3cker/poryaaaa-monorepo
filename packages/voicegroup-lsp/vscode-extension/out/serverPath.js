"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.resolveServerPath = resolveServerPath;
const fs = require("fs");
const path = require("path");
// Resolves the Rust server binary path so the VS Code activation stays launcher-only.
function resolveServerPath(extensionPath, configuredPath, exists = fs.existsSync) {
    if (configuredPath.length > 0) {
        return {
            path: configuredPath,
            checkedPaths: [configuredPath],
            found: exists(configuredPath)
        };
    }
    const checkedPaths = [
        path.resolve(extensionPath, '..', 'target', 'release', 'voicegroup-lsp'),
        path.resolve(extensionPath, '..', 'target', 'debug', 'voicegroup-lsp')
    ];
    const existingPath = checkedPaths.find(candidate => exists(candidate));
    return {
        path: existingPath ?? checkedPaths[0],
        checkedPaths,
        found: existingPath !== undefined
    };
}
//# sourceMappingURL=serverPath.js.map