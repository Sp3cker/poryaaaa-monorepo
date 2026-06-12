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
      textedit)
        cmake -S packages/{{target}} -B packages/{{target}}/build -DCMAKE_BUILD_TYPE=Release
        cmake --build packages/{{target}}/build --config Release --target {{target}}
        ;;
      m4l)
        cd packages/poryaaaa-m4l
        npm run build
        ;;
      *)
        echo "unknown build target: {{target}}" >&2
        echo "known targets: poryaaaa, ccomidi, textedit, m4l" >&2
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
      m4l)
        cd packages/poryaaaa-m4l
        npm run install:max-package
        ;;
      *)
        echo "unknown install target: {{target}}" >&2
        echo "known targets: poryaaaa, ccomidi, textedit m4l" >&2
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
