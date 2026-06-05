#!/usr/bin/env bash
# Screenshot every Max for Live help patcher under
# /Applications/Max.app/Contents/Resources/C74/help/m4l/ into
# docs/max-ref/screenshots/.
#
# Designed to NOT steal keyboard focus from whatever you're working in:
#
#   * `open -a Max -g <file>` opens each help file in Max without bringing
#     Max to the front (-g = "background").
#   * `screencapture -l <windowID>` captures the rendered window contents
#     directly via its CoreGraphics window ID, even when the window is
#     obscured or behind your other apps.
#   * The window is closed by Accessibility-pressing its red close button
#     (AXPress on the AXCloseButton subrole), scoped to that one Max window
#     by title — no synthesised Cmd+W keystrokes that could land on the
#     wrong app.
#
# We need a window-ID lookup. AppleScript can't reach CGWindowList, and
# JXA's bridge for the returned CFArray is broken enough that it's not worth
# the workaround. Instead we shell out to `/usr/bin/swift -` (system Swift,
# included with Xcode Command Line Tools) which calls
# CGWindowListCopyWindowInfo directly and matches the Max window whose
# title contains the help file's basename. Cold start is ~0.2s/call.
#
# Permissions (first run will prompt; grant to the shell that runs this):
#   * Screen Recording — for `screencapture` AND for Swift to read window
#                        titles via CGWindowListCopyWindowInfo.
#   * Accessibility    — for AppleScript System Events AXPress on the close
#                        button. (No keystroke synthesis.)
#
# Usage: scripts/screenshot_help_patches.sh [help_subdir]
#   help_subdir defaults to "m4l"; pass "max", "msp", "jitter" for others.

set -euo pipefail

HELP_ROOT="/Applications/Max.app/Contents/Resources/C74/help"
SUBDIR="${1:-m4l}"
HELP_DIR="$HELP_ROOT/$SUBDIR"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$REPO_ROOT/docs/max-ref/screenshots/$SUBDIR"

WAIT_OPEN="${WAIT_OPEN:-2.0}"   # seconds for Max to render the patcher
WAIT_CLOSE="${WAIT_CLOSE:-0.3}" # seconds after AXPress(close)

if [[ ! -d "$HELP_DIR" ]]; then
    echo "no such help dir: $HELP_DIR" >&2
    exit 1
fi
mkdir -p "$OUT_DIR"

# Swift helper text — passed on stdin to `swift -` per call.
read -r -d '' GET_WINDOW_ID_SWIFT <<'SWIFT' || true
import CoreGraphics
import Foundation

let needle = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : ""
guard !needle.isEmpty else { exit(0) }
guard let list = CGWindowListCopyWindowInfo([.optionOnScreenOnly], kCGNullWindowID) as? [[String: Any]] else {
    exit(0)
}
for w in list {
    guard let owner = w[kCGWindowOwnerName as String] as? String, owner == "Max" else { continue }
    let title = (w[kCGWindowName as String] as? String) ?? ""
    if title.contains(needle), let num = w[kCGWindowNumber as String] as? Int {
        print(num)
        exit(0)
    }
}
SWIFT

# Find a CGWindowID for a Max window whose title contains $1. Empty if none.
get_window_id() {
    local needle="$1"
    /usr/bin/swift - "$needle" 2>/dev/null <<<"$GET_WINDOW_ID_SWIFT" || true
}

# AXPress the close button of the Max window whose title contains $1.
close_window_by_title() {
    local needle="$1"
    /usr/bin/osascript - "$needle" >/dev/null 2>&1 <<'OSA' || true
on run argv
    set needle to item 1 of argv
    tell application "System Events"
        tell process "Max"
            repeat with w in windows
                try
                    if (name of w) contains needle then
                        set closeBtn to (first button of w whose subrole is "AXCloseButton")
                        perform action "AXPress" of closeBtn
                        return
                    end if
                end try
            end repeat
        end tell
    end tell
end run
OSA
}

shopt -s nullglob
files=("$HELP_DIR"/*.maxhelp)
shopt -u nullglob
if [[ ${#files[@]} -eq 0 ]]; then
    echo "no .maxhelp files in $HELP_DIR" >&2
    exit 1
fi

echo "sweeping ${#files[@]} files from $HELP_DIR"
echo "output:  $OUT_DIR"

# Make sure Max is running, but in the background.
open -a Max -g
sleep 1

ok=0; missed=0
for f in "${files[@]}"; do
    base="$(basename "$f" .maxhelp)"
    outpath="$OUT_DIR/$base.png"
    echo "  $base"

    open -a Max -g "$f"
    sleep "$WAIT_OPEN"

    wid=""
    # Max sometimes titles the window with or without the extension and may
    # take a moment to register the new window. Retry briefly.
    for _ in 1 2 3 4 5; do
        wid="$(get_window_id "$base" || true)"
        [[ -n "$wid" ]] && break
        sleep 0.4
    done

    if [[ -z "$wid" ]]; then
        echo "    (skip: no Max window matched \"$base\")" >&2
        ((missed++)) || true
        continue
    fi

    if screencapture -x -l "$wid" "$outpath" 2>/dev/null && [[ -s "$outpath" ]]; then
        ((ok++)) || true
    else
        echo "    (skip: screencapture failed for window $wid)" >&2
        ((missed++)) || true
    fi

    close_window_by_title "$base"
    sleep "$WAIT_CLOSE"
done

echo "done — wrote $ok, missed $missed"
