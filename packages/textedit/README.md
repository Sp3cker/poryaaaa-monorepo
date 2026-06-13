# textedit - standalone JUCE VST3 code editor plugin

This is a completely standalone VST3 plugin whose only purpose is to be a code editor.

- Pure VST3 output from this package.
- JUCE owns plugin hosting, platform editor embedding, keyboard focus, clipboard, and native text input.
- The editor surface uses `juce::CodeEditorComponent` with a `juce::CodeDocument` and a voicegroup-focused tokeniser.
- The editor talks to an embedded voicegroup language service for document sync, diagnostics, completion, hover, and signature-help requests.
- Colors are based on the dark Gruvbox palette from `/Users/sallegrezza/Downloads/gruvbox.lua`.
- MIDI is not accepted or output.
- Audio is passed through when the host provides matching input/output buses.
- The package has zero dependencies on the other products in the monorepo.

## Building

```bash
cmake -S packages/textedit -B packages/textedit/build -DCMAKE_BUILD_TYPE=Release
cmake --build packages/textedit/build --config Release --target textedit_VST3
```

The build will fetch JUCE 8.0.6 by default. To use an existing checkout:

```bash
cmake -S packages/textedit -B packages/textedit/build -DCMAKE_BUILD_TYPE=Release -DJUCE_ROOT=/path/to/JUCE
```

On macOS, JUCE copies the built VST3 to the user plugin location after build.

The embedded voicegroup language service loads the Swift bridge dylib at runtime.
The default bridge path is:

```bash
packages/voicegroup-lsp/.build/release/libVoicegroupBridge.dylib
```

Override it with either:

```bash
cmake -S packages/textedit -B packages/textedit/build -DTEXTEDIT_VOICEGROUP_BRIDGE_PATH=/path/to/libVoicegroupBridge.dylib
TEXTEDIT_VOICEGROUP_BRIDGE_PATH=/path/to/libVoicegroupBridge.dylib
```

## Implementation Notes

- `src/TextEditProcessor.*` contains the JUCE `AudioProcessor` and state persistence.
- `src/TextEditEditor.*` contains the `AudioProcessorEditor`, `CodeDocument`, and `CodeEditorComponent`.
- `src/VoicegroupLanguageService.*` contains the editor-facing language-service boundary and embedded implementation.
- `src/VoicegroupLanguageBridge.*` contains the C ABI loader for the Swift voicegroup core bridge.
- `src/VoicegroupTokeniser.*` and `src/GruvboxTheme.*` contain syntax coloring and editor colors.
- Plugin state is the editor text serialized into the VST3 state block.

Complete when `textedit_VST3` builds, the bundle verifies, and a DAW can open the editor and type into the code view.
