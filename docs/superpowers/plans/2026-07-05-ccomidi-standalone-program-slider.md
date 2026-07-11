# CComidi Projects Json Fallback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `ccomidi.clap` usable when poryaaaa has not written `projects.json`, without removing the existing `projects.json` voice dropdown path.

**Architecture:** Keep the current voicegroup bridge, Program dropdown, Reload Voices button, and Drumset tab behavior for sessions where `projects.json` is available and contains slots. Add a fallback Program Change slider only for the no-voice-state path that currently displays a `Voicegroup:` error. The core MIDI sender, CLAP param IDs, saved-state layout, CC/xcmd rows, and drum-pad queue code stay unchanged.

**Tech Stack:** C++20, CLAP C interface, ImGui/Pugl GUI, CMake Release builds, root `.clang-format` via `xcrun clang-format`.

## Resolved Clarifications

- This is fallback code, not a heavy deletion/refactor.
- Final scope confirmation: implement **Fallback only**, not **Always standalone**. When `projects.json` has slots, the existing dropdown and Drumset behavior remain.
- `projects.json` support must continue to work.
- The fallback Program Change control should be a horizontal ImGui slider, not a custom rotary knob.
- Do not use Ableton as the CLAP smoke-test host; Ableton does not support CLAP here.

## Global Constraints

- Follow `/Users/spencer/dev/cProjects/poryaaaa-monorepo/AGENTS.md` and `packages/ccomidi/AGENTS.md`.
- Build only the Release configuration for `packages/ccomidi`.
- Keep existing CC/xcmd command rows.
- Keep the existing `Emit Program Change` checkbox.
- Keep the existing voice dropdown when `projects.json` provides voice slots.
- Keep the existing Reload Voices button so ccomidi can pick up `projects.json` later in the session.
- Keep the existing Drumset tab behavior for valid voicegroup slots that include drumset metadata.
- In fallback mode only, show a `SliderInt` for Program Change values `0..127` instead of a missing-`projects.json` error.
- Do not delete `voicegroup_bridge.cpp/.h`, `drum_pad_grid.cpp/.h`, or `test_voicegroup_bridge.cpp`.
- Do not change CLAP param IDs or saved-state layout.
- Format only files touched by this plan.

---

## File Structure

- Modify `packages/ccomidi/src/gui/ccomidi_editor.cpp`
  - Replace the no-voices error UI with fallback Program Change slider UI.
  - Keep the existing Program dropdown path for loaded voice slots.
  - Keep the existing Drumset tab logic.
- No planned changes to `packages/ccomidi/src/plugin/ccomidi_plugin.cpp`.
- No planned changes to `packages/ccomidi/src/plugin/ccomidi_plugin_shared.h`.
- No planned changes to `packages/ccomidi/src/plugin/voicegroup_bridge.cpp/.h`.
- No planned changes to `packages/ccomidi/CMakeLists.txt`.
- No planned changes to `packages/ccomidi/src/core/sender_core.cpp/.h`.

---

### Task 1: Add Fallback Program Slider UI

**Files:**
- Modify: `packages/ccomidi/src/gui/ccomidi_editor.cpp`

**Interfaces:**
- Consumes: `UiSnapshot::program`, `UiSnapshot::programEnabled`, `kParamProgram`, `kParamProgramEnabled`, `apply_ui_param_change`, and existing `plugin->voiceLoad` state.
- Produces: no-voice fallback UI that edits the same Program CLAP parameter used by the dropdown.

- [ ] **Step 1: Locate the current Program row no-voices branch**

In `draw_frame`, find the existing Program row table. It currently branches like this:

```cpp
if (hasVoices)
{
    char preview[288];
    if (currentVoice)
        std::snprintf(preview, sizeof(preview), "%03d  %s", currentVoice->program, currentVoice->name.c_str());
    else
        std::snprintf(preview, sizeof(preview), "%03d  (empty slot)", program);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (boldFont)
        ImGui::PushFont(boldFont, kBodyFontSize);
    const bool comboOpen = ImGui::BeginCombo("##program", preview);
    if (boldFont)
        ImGui::PopFont();
    if (comboOpen)
    {
        for (const VoiceSlot& slot : plugin->voiceLoad.slots)
        {
            char label[288];
            std::snprintf(label, sizeof(label), "%03d  %s", slot.program, slot.name.c_str());
            const bool selected = slot.program == program;
            if (ImGui::Selectable(label, selected))
                apply_ui_param_change(plugin, kParamProgram, static_cast<double>(slot.program));
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}
else
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Voicegroup: %s", plugin->voiceLoad.error.c_str());
    if (!plugin->voiceLoad.statePath.empty())
        ImGui::TextDisabled("state: %s", plugin->voiceLoad.statePath.c_str());
}
```

- [ ] **Step 2: Keep the `hasVoices` dropdown path unchanged**

Do not alter the `if (hasVoices)` branch except for formatting caused by nearby edits. This preserves `projects.json` support.

- [ ] **Step 3: Replace only the `else` branch with fallback slider UI**

Replace the `else` branch with:

```cpp
else
{
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("Program Change", &program, 0, 127, "%d", ImGuiSliderFlags_AlwaysClamp))
        apply_ui_param_change(plugin, kParamProgram, static_cast<double>(program));

    if (!plugin->voiceLoad.error.empty())
        ImGui::TextDisabled("Voice list unavailable: %s", plugin->voiceLoad.error.c_str());
}
```

Expected behavior:

- When `projects.json` is missing or invalid, the user can still set Program Change `0..127`.
- The error becomes secondary disabled text, not the primary control.
- The same `kParamProgram` automation/state path is used as the dropdown.

- [ ] **Step 4: Keep the Program checkbox behavior unchanged**

Do not change this code:

```cpp
if (ImGui::Checkbox("##emit_program", &programEnabled))
    apply_ui_param_change(plugin, kParamProgramEnabled, programEnabled ? 1.0 : 0.0);
```

The fallback slider remains inside the existing `ImGui::BeginDisabled(!programEnabled)` block, so disabling Program Change disables both dropdown and fallback slider.

- [ ] **Step 5: Keep Reload Voices button unchanged**

Do not remove this button:

```cpp
if (ImGui::Button(reloadVoicesLabel))
{
    std::lock_guard<std::mutex> lock(plugin->stateMutex);
    plugin->voiceLoad = voicegroup_bridge_load_state();
}
```

This lets ccomidi switch from fallback mode to dropdown mode if poryaaaa writes `projects.json` after ccomidi is already open.

- [ ] **Step 6: Keep Drumset tab behavior unchanged**

Do not remove this logic:

```cpp
const bool selectedVoiceIsDrumset = currentVoice && !currentVoice->drumset.empty();
```

and do not remove the existing Drumset branch. In fallback mode `currentVoice` is null, so the Drumset tab is naturally unavailable.

- [ ] **Step 7: Format the touched source file**

Run:

```bash
xcrun clang-format -i packages/ccomidi/src/gui/ccomidi_editor.cpp
```

Expected: command exits 0.

---

### Task 2: Build And Run Existing Package Tests

**Files:**
- Test/build only.

**Interfaces:**
- Consumes: unchanged core/plugin/build graph.
- Produces: evidence that the fallback UI change did not break ccomidi package builds or existing core behavior.

- [ ] **Step 1: Build ccomidi Release plugin**

Run:

```bash
cmake --build packages/ccomidi/build --config Release --target ccomidi
```

Expected: build exits 0.

- [ ] **Step 2: Build Release test executables**

Run:

```bash
cmake --build packages/ccomidi/build --config Release --target ccomidi_core_tests ccomidi_voicegroup_bridge_tests ccomidi_legatobend_core_tests
```

Expected: build exits 0.

- [ ] **Step 3: Run sender core tests**

Run:

```bash
packages/ccomidi/build/ccomidi_core_tests
```

Expected output is the executable's summary and exit code 0:

```text
Passed <passed>/<run> tests
```

- [ ] **Step 4: Run voicegroup bridge tests**

Run:

```bash
packages/ccomidi/build/ccomidi_voicegroup_bridge_tests
```

Expected output includes this summary and exit code 0:

```text
[ccomidi_voicegroup_bridge]
```

- [ ] **Step 5: Run legatobend core tests**

Run:

```bash
packages/ccomidi/build/ccomidi_legatobend_core_tests
```

Expected output includes this summary and exit code 0:

```text
[ccomidi_legatobend_core]
```

---

### Task 3: Manual Fallback Smoke Test

**Files:**
- Manual verification of installed `ccomidi.clap` in a CLAP-capable host.

**Interfaces:**
- Consumes: built CLAP bundle from Task 2.
- Produces: user-visible confirmation that ccomidi is usable without requiring `projects.json`.

- [ ] **Step 1: Ensure fallback condition exists**

Temporarily move the poryaaaa project state file out of the way if it exists:

```text
~/Library/Application Support/poryaaaa/projects.json
```

Keep a backup so it can be restored after the smoke test.

- [ ] **Step 2: Open `ccomidi.clap` in a concrete CLAP-capable host**

Do not use Ableton for this smoke test because Ableton does not support CLAP here. Preferred host: Bitwig Studio. Acceptable fallback if Bitwig is unavailable: another installed CLAP-capable host that can show or route outgoing MIDI events, such as REAPER with CLAP support plus a MIDI/event monitor plugin.

Expected:

```text
The ccomidi editor opens without requiring poryaaaa to be loaded first.
```

- [ ] **Step 3: Inspect fallback Program controls**

Expected UI while `projects.json` is unavailable:

```text
Emit Program Change checkbox is visible.
Program Change slider is visible and clamps to 0..127.
No Program dropdown is visible in fallback mode.
No Drumset tab is visible in fallback mode.
Voice list unavailable text, if shown, is secondary disabled text.
```

- [ ] **Step 4: Smoke Program Change emission in fallback mode**

Set Program Change to a known value, keep `Emit Program Change` enabled, start playback, and monitor MIDI output in the chosen CLAP host. In Bitwig, put a MIDI/event monitor after `ccomidi` on the same track or route the track's MIDI output to a visible monitor target.

Expected:

```text
A Program Change event emits on playback start with the selected Program Change number.
```

- [ ] **Step 5: Create deterministic `projects.json` state**

After fallback-mode verification, write a known-valid state file at:

```text
~/Library/Application Support/poryaaaa/projects.json
```

Use this body so the dropdown and Drumset tab can be verified even on a machine where poryaaaa has not generated state:

```json
{
  "root": "/tmp/ccomidi-smoke",
  "bank": "smoke",
  "slots": [
    {"program": 0, "name": "Smoke Piano"},
    {
      "program": 1,
      "name": "Smoke Drums",
      "typeCode": 128,
      "drumset": [
        {"note": 36, "name": "DirectSoundWaveData_Kick"},
        {"note": 38, "name": "DirectSoundWaveData_Snare"}
      ]
    }
  ]
}
```

- [ ] **Step 6: Confirm existing `projects.json` mode still works**

Click Reload Voices or reopen the plugin, then select `001  Smoke Drums` from the existing Program dropdown.

Expected:

```text
The existing Program dropdown returns when voice slots are available.
The fallback Program Change slider is not shown while voice slots are available.
The Drumset tab appears for the Smoke Drums slot.
```

- [ ] **Step 7: Restore the user's original `projects.json` state**

If Step 1 moved an existing state file aside, restore that original file after the smoke test.

---

## Implementation Notes

- The implementation should be tiny: one focused edit in `ccomidi_editor.cpp` plus formatting and verification.
- Do not remove or weaken `voicegroup_bridge`; the requirement is fallback, not deletion.
- Do not replace `projects.json` with another config file or environment variable.
- Do not bump `kStateVersion`; no CLAP parameter IDs or saved-state binary layout change.
- If a build failure points at unrelated stale generated files or package-local build output, report the exact failure instead of expanding scope.
