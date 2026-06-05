#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cmake -S "${repo_root}/packages/poryaaaa" -B "${repo_root}/packages/poryaaaa/build"
cmake --build "${repo_root}/packages/poryaaaa/build" --target poryaaaa_unit_tests
"${repo_root}/packages/poryaaaa/build/poryaaaa_unit_tests"
