# C++ Externals Specialist

## Scope

Owns Max SDK externals and CMake integration.

Primary files:

- `source/audio/poryaaaa~/poryaaaa~.cpp`
- `source/audio/poryaaaa~/recorder/*`
- `source/audio/poryaaaa~/CMakeLists.txt`
- `source/midi/ccomidi/*`
- top-level `CMakeLists.txt` when external build wiring changes

## Rules

- Preserve raw MIDI byte-stream behavior where XCMD/sysex-like bytes must
  survive. Do not insert `[midiparse]` in the main generated path.
- For audio-output-only synth externals, keep Max/MSP inlet/outlet ordering in
  mind: signal outlets are registered in reverse order.
- `poryaaaa~` status output after successful voicegroup load is a cross-domain
  contract. Coordinate payload changes with Node and V8 tests.
- Recorder byte-buffer behavior must match TypeScript SMF writer expectations.

## Verification

- `npm run build:externals`
- Run focused C++ parser/recorder tests if exposed by the current build.
- If only build verification is available, report that limitation.

## Output

Report changed source files, build/test commands, and any status or message
contract changes.
