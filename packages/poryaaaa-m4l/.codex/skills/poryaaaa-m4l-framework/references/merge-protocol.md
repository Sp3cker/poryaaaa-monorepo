# Merge Protocol

Use this after multiple specialists or subagents contribute.

## Principles

1. One file has one writer at a time.
2. The lead specialist owns the interface contract.
3. Generated artifacts are regenerated after source changes, not edited by hand.
4. I/O counts, message selectors, and route tags must match across domains.
5. Verification runs after the merged state, not only per-domain.

## Ownership

| File type | Owner |
| --- | --- |
| V8 TypeScript | TypeScript V8 specialist |
| Node TypeScript | Node transport specialist |
| C++ external source | C++ externals specialist |
| Python AMXD generators | AMXD generator specialist |
| `devices/*.amxd` | AMXD generator specialist after regeneration |
| Max object claims | Max/M4L reference specialist |
| Test/build selection | Verification specialist |

## Sequence

1. Router chooses the lead domain and writes down the interface contract.
2. Secondary domains adapt to that contract.
3. If the contract changes, stop and update all affected domains before
   implementation continues.
4. Regenerate generated outputs after source changes.
5. Validate generated `.amxd` structure.
6. Run the critic protocol, then final verification.

## Common Conflicts

- Node emits a new route tag but patcher routing is unchanged: AMXD generator
  must update `[route ...]` and validate outlet counts.
- C++ status payload changes: TypeScript parser tests must update, and Node/V8
  consumers must agree on shape.
- UI widget changes need Live parameters: AMXD generator owns
  `saved_attribute_attributes`; Max/M4L reference verifies object semantics.
- Presentation layout conflicts: AMXD generator owns generated positions;
  Max/M4L reference verifies widget choice and syntax.

## Merge Checklist

- [ ] Each changed file has one responsible domain.
- [ ] Message contracts are documented in code or tests.
- [ ] Generated JS was produced from `code-src/`.
- [ ] Generated `.amxd` files were produced from `scripts/gen_*_amxd.py`.
- [ ] `scripts/amxd_inspect.py ... validate` passes for touched devices.
- [ ] Final verification command output is known.
