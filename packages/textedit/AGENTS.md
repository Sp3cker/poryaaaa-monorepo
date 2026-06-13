# AGENTS.md — textedit (standalone VST3 convenience text editor)

This package is a **standalone** VST3-only convenience plugin.

## Core Rules (stricter than the monorepo average because it is isolated)

- **Pure VST3. No CLAP.** Do not include any clap headers, do not link the monorepo's `third_party/clap`, do not produce .clap artifacts from this package.
- **No dependency on other products.** Never `#include` or link anything from `packages/poryaaaa`, `packages/ccomidi`, or `packages/poryaaaa-m4l`. This package must remain invisible to consumers of the audio/MIDI products ("they don't need to know about it").
- The existing CLAP plugins are perfect. Do not modify their code, CMake, or dependencies in any way. The "CLAP-only" policy that applied to them is superseded *only* for this new VST3 convenience plugin.
- Use JUCE for plugin hosting and editor UI. Do not route this package's editor through Pugl, ImGui, or bespoke AppKit/Metal glue while keyboard/input behavior is being stabilized.
- We will not use clap-wrapper. If a bespoke lower-level core is ever required, write it directly against the VST3 SDK.
- The plugin must not generate audio or write MIDI. It is a pure GUI convenience / utility editor.

## Where to work

- `CMakeLists.txt`
- `src/TextEditProcessor.*` (the JUCE `AudioProcessor`)
- `src/TextEditEditor.*` (the JUCE code editor UI)
- `README.md`, `AGENTS.md`, `Info.plist.in`

## Build & verification

- Always use the package-local `build/` directory.
- Primary target on mac: `textedit_VST3` (the .vst3 bundle).
- After changes: configure + build, confirm the `.vst3` bundle artifact exists.
- No changes are allowed outside `packages/textedit/`.

## Style

- Match normal JUCE plugin style: small `AudioProcessor`, small `AudioProcessorEditor`, no package-wide abstractions unless reused.
- Use `juce::CodeEditorComponent`/`juce::CodeDocument` for the editor surface so keyboard focus, clipboard, selection, and text input are handled by JUCE.
- Keep voicegroup language-service integration in focused bridge files. TextEdit uses the embedded service path unless a separate adapter is explicitly requested.
- Keep editor colors aligned with the dark Gruvbox palette from `/Users/sallegrezza/Downloads/gruvbox.lua` unless the user requests a different theme.

Goal: a working VST3 that hosts a code editor using JUCE, while obeying the isolation rules above.
