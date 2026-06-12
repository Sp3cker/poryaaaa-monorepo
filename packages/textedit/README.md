# textedit - standalone JUCE VST3 code editor plugin

This is a completely standalone VST3 plugin whose only purpose is to be a code editor.

- Pure VST3 output from this package.
- JUCE owns plugin hosting, platform editor embedding, keyboard focus, clipboard, and native text input.
- The editor surface uses `juce::CodeEditorComponent` with a `juce::CodeDocument` and a voicegroup-focused tokeniser.
- The plugin launches `voicegroup-lsp` over stdio for document sync, diagnostics, completion, hover, and signature-help requests.
- Colors are based on the dark Gruvbox palette from `/Users/sallegrezza/Downloads/gruvbox.lua`.
- No MIDI input or output.
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

The default LSP server path is:

```bash
/Users/sallegrezza/dev/swiftProjects/voicegroup-lsp/.build/release/voicegroup-lsp
```

Override it with:

```bash
cmake -S packages/textedit -B packages/textedit/build -DTEXTEDIT_VOICEGROUP_LSP_PATH=/path/to/voicegroup-lsp
```

On macOS, JUCE copies the built VST3 to the user plugin location after build.

## Implementation Notes

- `src/TextEditProcessor.*` contains the JUCE `AudioProcessor` and state persistence.
- `src/TextEditEditor.*` contains the `AudioProcessorEditor`, `CodeDocument`, and `CodeEditorComponent`.
- `src/VoicegroupLspClient.*` contains the stdio JSON-RPC/LSP bridge.
- `src/VoicegroupTokeniser.*` and `src/GruvboxTheme.*` contain syntax coloring and editor colors.
- Plugin state is the editor text serialized into the VST3 state block.

Complete when `textedit_VST3` builds, the bundle verifies, and a DAW can open the editor and type into the code view.
