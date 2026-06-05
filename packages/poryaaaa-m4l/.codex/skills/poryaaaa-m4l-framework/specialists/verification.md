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
- For generated `.amxd` changes, validation is mandatory.
- For TypeScript changes, `npm run check` is the minimum static check; run
  focused tests or `npm test` as risk requires.
- For C++ changes, run `npm run build:externals`.
- For full cross-domain changes, run `npm run build` when practical.
- If a command cannot be run, report why and what risk remains.

## Output

List commands run, pass/fail result, and residual risk. Do not claim completion
without command evidence.
