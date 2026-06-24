import * as path from 'path';
import * as fs from 'fs';
import * as vscode from 'vscode';
import { LanguageClient, LanguageClientOptions, ServerOptions } from 'vscode-languageclient/node';

let client: LanguageClient | undefined;

export async function activate(context: vscode.ExtensionContext) {
  const output = vscode.window.createOutputChannel('Voicegroup LSP');
  context.subscriptions.push(output);

  const configPath = vscode.workspace.getConfiguration('voicegroupLSP').get<string>('serverPath') ?? '';
  const extensionPath = fs.realpathSync(context.extensionPath);
  const packagedServerPath = path.join(extensionPath, 'server', 'voicegroup-lsp');
  const repoServerPath = path.resolve(extensionPath, '..', '.build', 'cpp', 'voicegroup-lsp');
  let serverPath = configPath.length > 0 ? configPath : packagedServerPath;
  if (configPath.length === 0 &&
      !fs.existsSync(packagedServerPath) &&
      context.extensionMode === vscode.ExtensionMode.Development) {
    serverPath = repoServerPath;
  }

  output.appendLine(`Starting voicegroup-lsp from ${serverPath}`);
  if (!fs.existsSync(serverPath)) {
    output.appendLine(`Server binary does not exist: ${serverPath}`);
    void vscode.window.showErrorMessage(`Voicegroup LSP server binary not found: ${serverPath}`);
    return;
  }

  // VS Code owns editor activation; the native server owns the language rules.
  // Keeping the client as a launcher avoids duplicating poryaaaa voicegroup knowledge in JS.
  const serverOptions: ServerOptions = {
    command: serverPath,
    args: []
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [
      { scheme: 'file', language: 'voicegroup-inc', pattern: '**/sound/voicegroups/**/*.inc' }
    ],
    outputChannel: output,
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher('**/sound/{direct_sound_data,programmable_wave_data,keysplit_tables}.inc')
    }
  };

  client = new LanguageClient('voicegroupLSP', 'Voicegroup LSP', serverOptions, clientOptions);
  context.subscriptions.push(client);
  await client.start();
}

export async function deactivate() {
  await client?.stop();
  client = undefined;
}
