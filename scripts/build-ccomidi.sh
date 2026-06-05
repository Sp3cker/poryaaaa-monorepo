#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cmake -S "${repo_root}/packages/ccomidi" -B "${repo_root}/packages/ccomidi/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "${repo_root}/packages/ccomidi/build" --target ccomidi
