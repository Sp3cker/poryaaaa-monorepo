# Verification Specialist

## Scope

Owns choosing and interpreting build, test, typecheck, bundle, and `.amxd`
validation commands.

## Inputs

- Changed file list.
- `references/build-test-matrix.md`.
- Domain specialists' reported risks.

## Rules

- Use the smallest command set that covers the touched domains.
- For device `.amxd` changes (hand-edits in Max), run `python3 scripts/amxd_inspect.py devices/<name>.amxd validate`.
- For TypeScript changes, `npm run check` is the minimum static check; run
  focused tests or `npm test` as risk requires.
- For C++ changes, run `npm run build:externals`.
- For full cross-domain changes, run `npm run build` when practical.
- If a command cannot be run, report why and what risk remains.

## Output

List commands run, pass/fail result, and residual risk. Do not claim completion
without command evidence.
