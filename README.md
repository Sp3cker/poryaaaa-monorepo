# poryaaaa-monorepo

Monorepo for the first-party poryaaaa projects.

## Setup

This repo uses `just` for common build and test commands.

macOS:

```bash
brew install just
```

Windows:

```powershell
choco install just
```

## Build Commands

From the repo root:

```bash
just build poryaaaa
just build ccomidi
just build m4l
```

The CLAP builds install to the user CLAP directory through their package-local
CMake post-build rules:

```bash
just install poryaaaa
just install ccomidi
just clap-dir
```

Run focused package checks with:

```bash
just test poryaaaa
just test ccomidi
just test m4l
```

## Package Hierarchy

`packages/poryaaaa` is the GBA m4a audio engine, CLAP instrument, renderer,
voicegroup loader, and shared recorder code. It is the core audio/runtime
project.

`packages/ccomidi` is the CLAP MIDI/control sender. It emits program changes,
CCs, xCMD sequences, and related control data intended for poryaaaa playback
and export workflows.

`packages/poryaaaa-m4l` is the Max for Live layer. It contains Max externals,
hand-maintained `.amxd` devices (edited in Max + validated with
`amxd_inspect.py`), TypeScript controller code, recorder save/export
logic, and bridge code that connects Live workflows to `poryaaaa` and
`ccomidi`.

First-party packages are normal directories, not git submodules. Third-party
dependencies may still be package-local submodules when the package requires
that layout.

## Agent Guidance

Read root `AGENTS.md` first, then the nearest package `AGENTS.md` before
editing package code. Cross-package work should preserve package boundaries and
verify every affected package.
