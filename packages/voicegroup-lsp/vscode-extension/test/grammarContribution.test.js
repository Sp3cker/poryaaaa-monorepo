const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const extensionRoot = path.resolve(__dirname, '..');

function readJsonFile(filePath) {
  return JSON.parse(fs.readFileSync(filePath, 'utf8'));
}

function readPackageJson(relativePath) {
  return readJsonFile(path.join(extensionRoot, relativePath));
}

function voicegroupGrammarContribution(packageJson) {
  return packageJson.contributes?.grammars?.find(entry => entry.language === 'voicegroup-inc');
}

function collectScopeNames(node, scopes = new Set()) {
  if (!node || typeof node !== 'object') {
    return scopes;
  }

  if (typeof node.name === 'string') {
    scopes.add(node.name);
  }

  for (const key of ['captures', 'beginCaptures', 'endCaptures', 'whileCaptures']) {
    const captures = node[key];
    if (captures && typeof captures === 'object') {
      for (const capture of Object.values(captures)) {
        collectScopeNames(capture, scopes);
      }
    }
  }

  for (const key of ['patterns', 'repository']) {
    const children = node[key];
    if (Array.isArray(children)) {
      for (const child of children) {
        collectScopeNames(child, scopes);
      }
    } else if (children && typeof children === 'object') {
      for (const child of Object.values(children)) {
        collectScopeNames(child, scopes);
      }
    }
  }

  return scopes;
}

test('contributes a TextMate grammar for voicegroup-inc documents', () => {
  const packageJson = readPackageJson('package.json');
  const grammar = voicegroupGrammarContribution(packageJson);

  assert.deepEqual(grammar, {
    language: 'voicegroup-inc',
    scopeName: 'source.voicegroup-inc',
    path: './syntaxes/voicegroup-inc.tmLanguage.json'
  });
});

test('voicegroup-inc TextMate grammar exposes theme-compatible token scopes', () => {
  const packageJson = readPackageJson('package.json');
  const contribution = voicegroupGrammarContribution(packageJson);
  assert.ok(contribution, 'package.json must contribute a grammar for voicegroup-inc');
  const grammar = readJsonFile(path.join(extensionRoot, contribution.path));

  assert.equal(grammar.scopeName, 'source.voicegroup-inc');
  assert.ok(Array.isArray(grammar.patterns) && grammar.patterns.length > 0, 'grammar must include top-level patterns');

  const scopes = [...collectScopeNames(grammar)].sort();
  const hasScope = prefix => scopes.some(scope => scope === prefix || scope.startsWith(`${prefix}.`));

  assert.ok(hasScope('comment.line'), `expected a line-comment scope; found: ${scopes.join(', ')}`);
  assert.ok(hasScope('entity.name.label'), `expected a label scope; found: ${scopes.join(', ')}`);
  assert.ok(scopes.some(scope => scope.startsWith('support.function.') && scope.includes('voice')), `expected voice macro names to use support.function.*voice* scope; found: ${scopes.join(', ')}`);
  assert.ok(hasScope('constant.numeric'), `expected a numeric literal scope; found: ${scopes.join(', ')}`);
  assert.ok(hasScope('variable.other.symbol'), `expected a symbol reference scope; found: ${scopes.join(', ')}`);
});
