# Subagent Prompts

Use these as templates. Keep prompts self-contained and name the allowed write
scope. Tell workers they are not alone in the codebase and must not revert
unrelated changes.

## Explorer

```text
In /Users/sallegrezza/dev/cProjects/poryaaaa-m4l, inspect only this domain:
<domain and files>.

Question: <specific question>.

Do not edit files. Return:
- facts with file paths
- risks or unknowns
- suggested verification
```

## Worker

```text
In /Users/sallegrezza/dev/cProjects/poryaaaa-m4l, implement this bounded task:
<task>.

Ownership:
- You may edit: <files/directories>.
- Do not edit: <files/directories>.

You are not alone in the codebase. Do not revert unrelated edits. Work with any
changes you find.

Use the poryaaaa-m4l framework rules:
- edit TypeScript in code-src, not javascript bundles
- validate generated .amxd structure after generator changes
- use docs/max-ref before making Max object claims

Return:
- files changed
- commands run and results
- unresolved risks
```

## Reviewer

```text
In /Users/sallegrezza/dev/cProjects/poryaaaa-m4l, review this change set for:
<domain>.

Focus on bugs, regressions, missing verification, and violations of repo rules.
Do not edit files.

Return findings first, ordered by severity, with file paths and line numbers.
If no issues, say so and list residual verification risk.
```
