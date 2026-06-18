# AGENTS.md - poryaaaa-monorepo

Instructions for coding agents working in this monorepo.

## Read This First

- Follow the nearest package `AGENTS.md` before editing package code.
- Do not deduplicate third-party dependencies unless the task explicitly asks for dependency consolidation.
- Preserve package-local build directories and generated-output rules.
- When a task crosses packages, state the package boundary and verify every touched package.

## Quality
- Do not make code reusable if it's not going to be reused.
- Testability should not win over good architecture.
- Do not write 1-liner functions to get a boolean result. Inlining the check is simpler.
    - Example of bad:
    ```C++
    bool is_drumset_placeholder_voice(const std::string& name)
    {
        return name.find("Square") != std::string::npos;
    }
    ```
    This developer was executed in front of his loved ones over this extra 3 lines of code 
    He was resurected when corrected to:
    ```c++
    bool isDrumset = name.find("Square") != std::string::npos;
    ```
## Purpose

This repository contains the first-party poryaaaa projects:

- `packages/poryaaaa`: GBA m4a audio engine, CLAP instrument, and renderer
- `packages/ccomidi`: CLAP MIDI/control sender
- `packages/poryaaaa-m4l`: Max for Live package, externals, hand-maintained .amxd devices, and TypeScript controllers

First-party projects are normal directories, not git submodules.


## Package Guidance

- `packages/poryaaaa/AGENTS.md`: engine, CLAP plugin, renderer, voicegroup loader, recorder, and CMake rules.
- `packages/ccomidi/AGENTS.md`: CLAP MIDI sender, sender core, IPC, GUI, and Release-only build rule.
- `packages/poryaaaa-m4l/AGENTS.md`: Max for Live framework entrypoint and domain routing.

## Build And Test

Use package-local commands first. Root scripts under `scripts/` are thin wrappers
for convenience and must not hide package-specific failures.

Validation is not complete until the affected package checks pass or the skipped
checks are reported with the exact reason.

## Formatting

For C, C++, Objective-C, and Objective-C++ changes, use the root `.clang-format`
with Xcode's formatter:

```bash
xcrun clang-format -i path/to/file.cpp path/to/file.h
```

Format only files you intentionally touched unless the user explicitly asks for a
repo-wide formatting pass. Do not format vendored dependencies, generated files,
or package-local build outputs.

## Git Identity

Migration and repo-maintenance commits in this repository should use:

```bash
git config user.name "Sp3cker"
git config user.email "speker97@protonmail.com"
```
