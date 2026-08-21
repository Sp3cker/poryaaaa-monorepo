#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cmake -S "${repo_root}/packages/poryaaaa" -B "${repo_root}/packages/poryaaaa/build"
cmake --build "${repo_root}/packages/poryaaaa/build" --target \
  poryaaaa_unit_tests \
  poryaaaa_pcm_mixer_oracle_tests \
  poryaaaa_pcm_mixer_no_alloc_tests \
  poryaaaa_m4a_state_compat_tests
"${repo_root}/packages/poryaaaa/build/poryaaaa_unit_tests"
ctest --test-dir "${repo_root}/packages/poryaaaa/build" --output-on-failure \
  -R '^poryaaaa_(pcm_mixer_(oracle|no_alloc)|m4a_state_compat)$'
cargo test --manifest-path "${repo_root}/packages/poryaaaa/plugin/Cargo.toml"
