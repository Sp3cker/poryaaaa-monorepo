"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const fs = require("fs");
const vscode = require("vscode");
const node_1 = require("vscode-languageclient/node");
const serverPath_1 = require("./serverPath");
let client;
async function activate(context) {
    const output = vscode.window.createOutputChannel('Voicegroup LSP');
    context.subscriptions.push(output);
    const configPath = vscode.workspace.getConfiguration('voicegroupLSP').get('serverPath') ?? '';
    const extensionPath = fs.realpathSync(context.extensionPath);
    const serverPath = (0, serverPath_1.resolveServerPath)(extensionPath, configPath);
    output.appendLine(`Starting voicegroup-lsp from ${serverPath.path}`);
    if (!serverPath.found) {
        const checkedPaths = serverPath.checkedPaths.join(', ');
        output.appendLine(`Server binary does not exist. Checked: ${checkedPaths}`);
        void vscode.window.showErrorMessage(`Voicegroup LSP server binary not found. Checked: ${checkedPaths}`);
    }
    // VS Code owns editor activation; Rust owns the language rules. Keeping the
    // client as a launcher avoids duplicating poryaaaa voicegroup knowledge in JS.
    const serverOptions = {
        command: serverPath.path,
        args: []
    };
    const clientOptions = {
        documentSelector: [
            { scheme: 'file', language: 'voicegroup-inc', pattern: '**/sound/voicegroups/**/*.inc' }
        ],
        outputChannel: output,
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/sound/{direct_sound_data,programmable_wave_data,keysplit_tables}.inc')
        }
    };
    client = new node_1.LanguageClient('voicegroupLSP', 'Voicegroup LSP', serverOptions, clientOptions);
    context.subscriptions.push(client);
    await client.start();
}
async function deactivate() {
    await client?.stop();
    client = undefined;
}
//# sourceMappingURL=extension.js.map