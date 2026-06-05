# AGENTS.md - poryaaaa-monorepo

Instructions for coding agents working in this monorepo.

## Purpose

This repository contains the first-party poryaaaa projects:

- `packages/poryaaaa`: GBA m4a audio engine, CLAP instrument, and renderer
- `packages/ccomidi`: CLAP MIDI/control sender
- `packages/poryaaaa-m4l`: Max for Live package, externals, generated devices, and TypeScript controllers

First-party projects are normal directories, not git submodules.

## Read This First

- Follow the nearest package `AGENTS.md` before editing package code.
- Keep changes surgical and scoped to the package or cross-package contract requested.
- Do not deduplicate third-party dependencies unless the task explicitly asks for dependency consolidation.
- Preserve package-local build directories and generated-output rules.
- When a task crosses packages, state the package boundary and verify every touched package.

## Package Guidance

- `packages/poryaaaa/AGENTS.md`: engine, CLAP plugin, renderer, voicegroup loader, recorder, and CMake rules.
- `packages/ccomidi/AGENTS.md`: CLAP MIDI sender, sender core, IPC, GUI, and Release-only build rule.
- `packages/poryaaaa-m4l/AGENTS.md`: Max for Live framework entrypoint and domain routing.

## Build And Test

Use package-local commands first. Root scripts under `scripts/` are thin wrappers
for convenience and must not hide package-specific failures.

Validation is not complete until the affected package checks pass or the skipped
checks are reported with the exact reason.

## Git Identity

Migration and repo-maintenance commits in this repository should use:

```bash
git config user.name "Sp3cker"
git config user.email "speker97@protonmail.com"
```
