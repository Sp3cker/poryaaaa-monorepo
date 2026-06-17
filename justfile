default:
    @just --list

# Build poryaaaa.clap and copy it to the user CLAP directory.
build target:
    #!/usr/bin/env bash
    set -euo pipefail
    case "{{target}}" in
      poryaaaa)
        cmake -S packages/poryaaaa -B packages/poryaaaa/build -DCMAKE_BUILD_TYPE=Release
        cmake --build packages/poryaaaa/build --config Release --target poryaaaa
        ;;
      ccomidi)
        cmake -S packages/ccomidi -B packages/ccomidi/build -DCMAKE_BUILD_TYPE=Release
        cmake --build packages/ccomidi/build --config Release --target ccomidi
        ;;
      voicegroup-bridge|swift-dylib)
        (cd packages/voicegroup-lsp && swift build -c release --product VoicegroupBridge)
        ;;
      textedit)
        # Build the voicegroup-lsp Swift package in release mode. This produces
        # (or updates) the voicegroup bridge dylib that the TextEdit VST3 loads
        # at runtime via dlopen for the embedded language service (document sync,
        # completions, hover, tab actions, etc.). Expected location:
        # packages/voicegroup-lsp/.build/release/libVoicegroupBridge.dylib
        (cd packages/voicegroup-lsp && swift build -c release --product VoicegroupBridge)

        # textedit is a standalone VST3 plugin. The packaged bundle (.vst3 directory
        # containing the executable, PkgInfo, moduleinfo.json, etc.) is produced by
        # the textedit_VST3 target (per packages/textedit/AGENTS.md and README).
        # We explicitly request the documented primary artefact target so that
        # `just build textedit` (and `just install textedit`) produce a usable VST3.
        cmake -S packages/{{target}} -B packages/{{target}}/build -DCMAKE_BUILD_TYPE=Release
        cmake --build packages/{{target}}/build --config Release --target textedit_VST3
        ;;
      m4l)
        cmake -S packages/poryaaaa -B packages/poryaaaa/build -DCMAKE_BUILD_TYPE=Release
        cmake --build packages/poryaaaa/build --config Release --target poryaaaa_napi
        cd packages/poryaaaa-m4l
        npm run build
        ;;
      *)
        echo "unknown build target: {{target}}" >&2
        echo "known targets: poryaaaa, ccomidi, voicegroup-bridge, swift-dylib, textedit, m4l" >&2
        exit 2
        ;;
    esac

# Alias for CLAP/package install workflows.
install target:
    #!/usr/bin/env bash
    set -euo pipefail
    case "{{target}}" in
      poryaaaa)
        cmake -S packages/poryaaaa -B packages/poryaaaa/build -DCMAKE_BUILD_TYPE=Release
        cmake --build packages/poryaaaa/build --config Release --target poryaaaa
        ;;
      ccomidi)
        cmake -S packages/ccomidi -B packages/ccomidi/build -DCMAKE_BUILD_TYPE=Release
        cmake --build packages/ccomidi/build --config Release --target ccomidi
        ;;
      textedit)
        # Build the voicegroup bridge dylib (runtime dep for TextEdit's language service)
        # in addition to the VST3 (which triggers the copy to user plugins dir).
        (cd packages/voicegroup-lsp && swift build -c release --product VoicegroupBridge)

        # Building the VST3 artefact target triggers COPY_PLUGIN_AFTER_BUILD
        # (see packages/textedit/CMakeLists.txt), installing to the user VST3 dir.
        cmake -S packages/textedit -B packages/textedit/build -DCMAKE_BUILD_TYPE=Release
        cmake --build packages/textedit/build --config Release --target textedit_VST3
        ;;
      m4l)
        cd packages/poryaaaa-m4l
        npm run install:max-package
        ;;
      *)
        echo "unknown install target: {{target}}" >&2
        echo "known targets: poryaaaa, ccomidi, textedit, m4l" >&2
        exit 2
        ;;
    esac

# Run the focused package test command.
test target:
    #!/usr/bin/env bash
    set -euo pipefail
    case "{{target}}" in
      poryaaaa)
        cmake -S packages/poryaaaa -B packages/poryaaaa/build -DCMAKE_BUILD_TYPE=Release
        cmake --build packages/poryaaaa/build --config Release --target poryaaaa_unit_tests
        packages/poryaaaa/build/poryaaaa_unit_tests
        ;;
      ccomidi)
        cmake -S packages/ccomidi -B packages/ccomidi/build -DCMAKE_BUILD_TYPE=Release
        cmake --build packages/ccomidi/build --config Release --target ccomidi_core_tests
        packages/ccomidi/build/ccomidi_core_tests
        ;;
      m4l|poryaaaa-m4l)
        cd packages/poryaaaa-m4l
        npm test
        npm run check
        ;;
      *)
        echo "unknown test target: {{target}}" >&2
        echo "known targets: poryaaaa, ccomidi, m4l" >&2
        exit 2
        ;;
    esac

# Print the user CLAP install directory.
clap-dir:
    @echo "$HOME/Library/Audio/Plug-Ins/CLAP"
