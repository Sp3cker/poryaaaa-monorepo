#!/usr/bin/env bash
# Install (symlink) this project into Max's user packages folder so that
#   - [v8 *.js] and [node.script *.js] resolve to bundled files in javascript/
#   - .amxd devices in devices/ pick up bundled patchers/externals
#
# Idempotent: re-running just rewrites the symlink. If an old real directory
# is already installed, pass --replace-existing to move it aside and install
# the symlink.
# Note: javascript/*.js files are build artifacts — run `npm run build` from
# the repo root after editing code-src/*.ts before reloading Max.

set -euo pipefail

REPLACE_EXISTING=0
ALL_EXISTING=0
for arg in "$@"; do
    case "$arg" in
        --replace-existing) REPLACE_EXISTING=1 ;;
        --all-existing) ALL_EXISTING=1 ;;
        *)
            echo "usage: $0 [--replace-existing] [--all-existing]" >&2
            exit 2
            ;;
    esac
done
if [[ "$ALL_EXISTING" -eq 1 && "$REPLACE_EXISTING" -ne 1 ]]; then
    echo "--all-existing requires --replace-existing" >&2
    exit 2
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGE_NAME="$(basename "$REPO_ROOT")"

install_link() {
    local pkg_dir="$1"
    mkdir -p "$pkg_dir"
    local link="$pkg_dir/$PACKAGE_NAME"

    if [[ -L "$link" ]]; then
        local existing
        existing="$(readlink "$link")"
        if [[ "$existing" == "$REPO_ROOT" ]]; then
            echo "Linked $link -> $REPO_ROOT"
            return
        fi
        rm "$link"
    elif [[ -e "$link" ]]; then
        if [[ "$REPLACE_EXISTING" -ne 1 ]]; then
            echo "Refusing to overwrite non-symlink at $link" >&2
            echo "Re-run with --replace-existing to move it aside and install the symlink." >&2
            exit 1
        fi
        local backup="$link.backup.$(date +%Y%m%d%H%M%S)"
        mv "$link" "$backup"
        echo "Moved existing package to $backup"
    fi

    ln -s "$REPO_ROOT" "$link"
    echo "Linked $link -> $REPO_ROOT"
}

# Detect installed Max versions (prefer Max 9 for the default, but optionally
# update every existing Max Documents package folder).
PKG_DIRS=()
if [[ "$ALL_EXISTING" -eq 1 ]]; then
    for v in 9 8; do
        candidate="$HOME/Documents/Max $v/Packages"
        if [[ -d "$candidate" || -d "$HOME/Documents/Max $v" ]]; then
            PKG_DIRS+=("$candidate")
        fi
    done
else
    for v in 9 8; do
        candidate="$HOME/Documents/Max $v/Packages"
        if [[ -d "$candidate" || -d "$HOME/Documents/Max $v" ]]; then
            PKG_DIRS+=("$candidate")
            break
        fi
    done
fi
if [[ "${#PKG_DIRS[@]}" -eq 0 ]]; then
    PKG_DIRS=("$HOME/Documents/Max 9/Packages")
fi

for pkg_dir in "${PKG_DIRS[@]}"; do
    install_link "$pkg_dir"
done
echo
echo "Restart Max (or 'Rescan File Search Path' in Options) to pick up the package."
