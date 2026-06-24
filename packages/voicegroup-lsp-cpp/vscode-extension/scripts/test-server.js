const assert = require('assert');
const childProcess = require('child_process');
const path = require('path');

const extensionRoot = path.resolve(__dirname, '..');
const serverPath = path.join(extensionRoot, 'server', 'voicegroup-lsp');
const server = childProcess.spawn(serverPath, [], { stdio: ['pipe', 'pipe', 'pipe'] });

let stdout = Buffer.alloc(0);
let stderr = '';
const messages = [];

server.stdout.on('data', chunk => {
  stdout = Buffer.concat([stdout, chunk]);
  readMessages();
});
server.stderr.on('data', chunk => {
  stderr += chunk.toString();
});

function send(message) {
  const body = Buffer.from(JSON.stringify(message));
  server.stdin.write(`Content-Length: ${body.length}\r\n\r\n`);
  server.stdin.write(body);
}

function readMessages() {
  while (true) {
    const headerEnd = stdout.indexOf('\r\n\r\n');
    if (headerEnd < 0) {
      return;
    }
    const header = stdout.subarray(0, headerEnd).toString();
    const match = header.match(/Content-Length:\s*(\d+)/i);
    assert(match, `missing Content-Length header: ${header}`);
    const length = Number(match[1]);
    const bodyStart = headerEnd + 4;
    const bodyEnd = bodyStart + length;
    if (stdout.length < bodyEnd) {
      return;
    }
    messages.push(JSON.parse(stdout.subarray(bodyStart, bodyEnd).toString()));
    stdout = stdout.subarray(bodyEnd);
  }
}

send({ jsonrpc: '2.0', id: 1, method: 'initialize', params: { rootUri: null } });
send({
  jsonrpc: '2.0',
  method: 'textDocument/didOpen',
  params: {
    textDocument: {
      uri: 'file:///sound/voicegroups/broken.inc',
      languageId: 'voicegroup-inc',
      version: 1,
      text: '\tvoice_bogus 60, 0\n\tvoice_directsound 200, 0, MissingSample, 255, 0, 255, 127'
    }
  }
});

setTimeout(() => {
  server.kill();
  assert.equal(stderr, '');
  assert(messages.some(message => message.id === 1 && message.result?.capabilities?.textDocumentSync === 1));

  const diagnostics = messages.find(message => message.method === 'textDocument/publishDiagnostics')?.params?.diagnostics;
  assert(Array.isArray(diagnostics), 'expected publishDiagnostics notification');
  assert(diagnostics.some(diagnostic => diagnostic.code === 'unknown-macro' && diagnostic.severity === 1));
  assert(diagnostics.some(diagnostic => diagnostic.code === 'invalid-range' && diagnostic.severity === 1));
}, 300);
