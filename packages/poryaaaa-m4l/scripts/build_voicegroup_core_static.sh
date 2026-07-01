#!/usr/bin/env bash
# Build the voicegroup-core C ABI artifacts consumed by the poryaaaa~ external.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
M4L_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
CORE_ROOT="$(cd "${M4L_ROOT}/../voicegroup-core" && pwd)"

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing required tool: $1" >&2
    exit 1
  fi
}

require_rust_target() {
  local target="$1"
  if ! rustup target list --installed | grep -Fxq "$target"; then
    echo "missing Rust target: $target" >&2
    echo "install it with: rustup target add $target" >&2
    exit 1
  fi
}

require_tool cargo
require_tool cbindgen

mkdir -p "${CORE_ROOT}/include"

if [[ "$(uname -s)" == "Darwin" ]]; then
  require_tool rustup
  require_tool lipo
  require_rust_target aarch64-apple-darwin
  require_rust_target x86_64-apple-darwin

  cargo build --manifest-path "${CORE_ROOT}/Cargo.toml" --release --target aarch64-apple-darwin
  cargo build --manifest-path "${CORE_ROOT}/Cargo.toml" --release --target x86_64-apple-darwin

  mkdir -p "${CORE_ROOT}/target/universal-apple-darwin/release"
  lipo -create \
    "${CORE_ROOT}/target/aarch64-apple-darwin/release/libvoicegroup_core.a" \
    "${CORE_ROOT}/target/x86_64-apple-darwin/release/libvoicegroup_core.a" \
    -output "${CORE_ROOT}/target/universal-apple-darwin/release/libvoicegroup_core.a"
else
  cargo build --manifest-path "${CORE_ROOT}/Cargo.toml" --release
fi

cbindgen \
  --quiet \
  --config "${CORE_ROOT}/cbindgen.toml" \
  --crate voicegroup-core \
  --output "${CORE_ROOT}/include/voicegroup_core.h" \
  "${CORE_ROOT}"
