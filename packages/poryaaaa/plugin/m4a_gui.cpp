/*
 * m4a_gui.cpp - Dear ImGui + Pugl GUI for the M4A plugin.
 *
 * Provides a simple settings panel where the user can change the project
 * root, voicegroup, reverb, and volume levels in real time from the DAW.
 *
 * Thread-safety: all functions must be called from the main thread.
 */

#include "imgui.h"
#include "imfilebrowser.h"
#include "imgui_pugl_shell.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <filesystem>
#include <span>
#include <string>

#if defined(__clang__)
#define GUI_LOG_DISABLE_FORMAT_NONLITERAL \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wformat-nonliteral\"")
#define GUI_LOG_RESTORE_FORMAT_NONLITERAL \
    _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define GUI_LOG_DISABLE_FORMAT_NONLITERAL \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wformat-nonliteral\"")
#define GUI_LOG_RESTORE_FORMAT_NONLITERAL \
    _Pragma("GCC diagnostic pop")
#else
#define GUI_LOG_DISABLE_FORMAT_NONLITERAL
#define GUI_LOG_RESTORE_FORMAT_NONLITERAL
#endif

/* Text size scale (Tailwind-style). Use integer values so line heights land on
 * whole pixels — fractional sizes make row spacing visibly uneven.
 * Usage: ImGui::PushFont(nullptr, text::Lg) keeps the font, changes the size. */
namespace text {
[[maybe_unused]] constexpr float Sm   = 13.0f;
constexpr float Base = 16.0f;
[[maybe_unused]] constexpr float Lg   = 20.0f;
[[maybe_unused]] constexpr float Xl   = 24.0f;
} // namespace text

static bool copy_path_utf8(char *dst, size_t dstSize, const std::filesystem::path &path)
{
    if (!dst || dstSize == 0)
        return false;

    auto utf8 = path.u8string();
#if defined(__cpp_char8_t)
    const char *text = reinterpret_cast<const char *>(utf8.c_str());
#else
    const char *text = utf8.c_str();
#endif
    if (strlen(text) >= dstSize)
        return false;

    snprintf(dst, dstSize, "%s", text);
    return true;
}

static bool open_project_root_browser(ImGui::FileBrowser &browser, const char *path)
{
    try {
        if (path && path[0])
            browser.SetDirectory(path);
        browser.Open();
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

static void display_project_root_browser(ImGui::FileBrowser &browser)
{
    try {
        browser.Display();
    } catch (const std::exception &) {
        browser.Close();
    }
}

static bool project_root_button(const char *label, const ImVec2 &size)
{
    bool pressed = ImGui::Button("##projectRootButton", size);
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImGui::PushClipRect(min, max, true);
    ImVec2 textSize = ImGui::CalcTextSize(label);
    ImVec2 textPos(min.x + ImGui::GetStyle().FramePadding.x,
                   min.y + (max.y - min.y - textSize.y) * 0.5f);
    ImGui::GetWindowDrawList()->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), label);
    ImGui::PopClipRect();
    return pressed;
}

/* ---- Debug logging ---- */
static const char *s_logPath = nullptr;

static void gui_log(const char *fmt, ...)
{
    if (!s_logPath) return;
    FILE *f = fopen(s_logPath, "a");
    if (!f) return;
    time_t t = time(nullptr);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&t));
    fprintf(f, "[%s] ", tbuf);
    va_list ap;
    va_start(ap, fmt);
    GUI_LOG_DISABLE_FORMAT_NONLITERAL;
    vfprintf(f, fmt, ap);
    GUI_LOG_RESTORE_FORMAT_NONLITERAL;
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

/* Our C interface */
#include "m4a_gui.h"
#include "m4a_engine.h"
#include "m4a_plugin.h"
#include "m4a_engine_recorder.h"
#include "m4a_gui_font_assets.h"

/* CLAP GUI extension (for notifying host when floating window closes) */
#include <clap/ext/gui.h>

/* ---- Constants ---- */

static const int GUI_W = 540;
static const int GUI_H = 500;
static const int VOICE_SLOT_COUNT = 128;

/* ---- GUI state ---- */

struct M4AGuiState {
    poryaaaa::gui::ImGuiPuglShell *shell;
    ImFont        *boldFont;
    const clap_host_t *host;

    /* Currently displayed settings */
    M4AGuiSettings settings;

    /* Editable text buffers (not applied until "Reload" is clicked) */
    char projectRootBuf[512];
    char voicegroupBuf[256];
    ImGui::FileBrowser projectRootBrowser{
        ImGuiFileBrowserFlags_SelectDirectory |
        ImGuiFileBrowserFlags_HideRegularFiles |
        ImGuiFileBrowserFlags_EditPathString |
        ImGuiFileBrowserFlags_CloseOnEsc |
        ImGuiFileBrowserFlags_SkipItemsCausingError,
        "."
    };

    /* Pending change flags (cleared by poll_changes) */
    bool settingsChanged;
    bool reloadRequested;
    bool extractRequested;
    char extractStatus[256];
    double midiActivityUntil[16];
    double xcmdActivityUntil;
    double validXcmdUntil;
    char latestXcmd[128];

    /* True after the user closes the floating window */
    bool wasClosed;

    M4AGuiTimerCallback internalTimerCallback;
    void *internalTimerUserData;

    /* Voice editor state */
    std::span<ToneData> liveVoices;
    std::span<bool> voiceOverrides;
    int selectedVoice;
    int pendingRestoreVoice;  /* -1 = none */
    bool voicesDirty;         /* set when any voice param is edited */

    /* Sample swapper state */
    std::span<const ProjectAssetEntry> directsoundAssets;
    std::span<const ProjectAssetEntry> progWaveAssets;
    const ProjectAssetOverride *assetOverrides;
    char sampleFilterText[256];

    bool pendingSampleSwap;
    int pendingSwapVoice;
    ProjectAssetKind pendingSwapKind;
    char pendingSwapFileName[256];

    /* Recorder tab: direct pointer to plugin data (M4APluginData*) for
     * armed flag and path. Stored as void* to avoid circular include in header. */
    void *plugin_data;
    /* Status message shown after a save attempt */
    char recorderStatus[256];

};

/* ---- Internal helpers ---- */

static void sync_buffers(M4AGuiState *gui)
{
    snprintf(gui->projectRootBuf, sizeof(gui->projectRootBuf),
             "%s", gui->settings.projectRoot);
    snprintf(gui->voicegroupBuf, sizeof(gui->voicegroupBuf),
             "%s", gui->settings.voicegroupName);
}

static std::span<const ProjectAssetEntry> project_asset_span(const ProjectAssetEntry *assets, int assetCount)
{
    if (!assets || assetCount <= 0)
        return {};
    return { assets, static_cast<size_t>(assetCount) };
}

/* ---- Voice type helpers ---- */

static const char *voice_type_name(uint8_t type)
{
    uint8_t base = type & ~VOICE_TYPE_FIX;
    switch (base) {
    case 0x00: return "DirectSound";
    case 0x01: return "Square 1";
    case 0x02: return "Square 2";
    case 0x03: return "Prog Wave";
    case 0x04: return "Noise";
    case VOICE_CRY:          return "Cry";
    case VOICE_CRY_REVERSE:  return "Cry (Reverse)";
    case VOICE_KEYSPLIT:     return "Keysplit";
    case VOICE_KEYSPLIT_ALL: return "Drum Kit";
    default: return "Unknown";
    }
}

/* Edit ADSR for DirectSound voices (0-255 range). Returns true if changed. */
static bool edit_directsound_adsr(ToneData *voice)
{
    bool changed = false;
    int a = voice->attack, d = voice->decay, s = voice->sustain, r = voice->release;
    if (ImGui::SliderInt("Attack##ds", &a, 0, 255))  { voice->attack  = (uint8_t)a; changed = true; }
    if (ImGui::SliderInt("Decay##ds",  &d, 0, 255))  { voice->decay   = (uint8_t)d; changed = true; }
    if (ImGui::SliderInt("Sustain##ds",&s, 0, 255))   { voice->sustain = (uint8_t)s; changed = true; }
    if (ImGui::SliderInt("Release##ds",&r, 0, 255))   { voice->release = (uint8_t)r; changed = true; }
    return changed;
}

/* Edit ADSR for CGB voices (limited range). Returns true if changed. */
static bool edit_cgb_adsr(ToneData *voice)
{
    bool changed = false;
    int a = voice->attack, d = voice->decay, s = voice->sustain, r = voice->release;
    if (ImGui::SliderInt("Attack##cgb", &a, 0, 7))   { voice->attack  = (uint8_t)a; changed = true; }
    if (ImGui::SliderInt("Decay##cgb",  &d, 0, 7))   { voice->decay   = (uint8_t)d; changed = true; }
    if (ImGui::SliderInt("Sustain##cgb",&s, 0, 15))   { voice->sustain = (uint8_t)s; changed = true; }
    if (ImGui::SliderInt("Release##cgb",&r, 0, 7))    { voice->release = (uint8_t)r; changed = true; }
    return changed;
}

/* ---- Tab rendering ---- */

static void render_general_tab(M4AGuiState *gui)
{
    double now = ImGui::GetTime();
    bool xcmdActive = now < gui->xcmdActivityUntil;
    bool validXcmdActive = now < gui->validXcmdUntil;
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    auto draw_led = [&](bool active, const ImVec4 &onColor, const ImVec4 &offColor, const char *label) {
        ImVec2 ledCenter = ImGui::GetCursorScreenPos();
        ledCenter.x += 7.0f;
        ledCenter.y += 9.0f;
        drawList->AddCircleFilled(ledCenter, 5.0f,
                                  ImGui::GetColorU32(active ? onColor : offColor));
        ImGui::Dummy(ImVec2(14.0f, 18.0f));
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
    };

    /* Per-channel MIDI activity: 16 compact LEDs in a row, one per MIDI
     * channel. Each LED pulses independently when its channel sees an event. */
    static const ImVec4 kChanOn  = ImVec4(0.18f, 0.95f, 0.35f, 1.0f);
    static const ImVec4 kChanOff = ImVec4(0.18f, 0.24f, 0.20f, 1.0f);
    ImGui::TextUnformatted("MIDI Activity");
    {
        const float ledSlot = 18.0f;
        ImVec2 rowOrigin = ImGui::GetCursorScreenPos();
        for (int ch = 0; ch < 16; ch++) {
            bool active = now < gui->midiActivityUntil[ch];
            ImVec2 c;
            c.x = rowOrigin.x + ch * ledSlot + 7.0f;
            c.y = rowOrigin.y + 9.0f;
            drawList->AddCircleFilled(c, 5.0f,
                                       ImGui::GetColorU32(active ? kChanOn : kChanOff));
        }
        ImGui::Dummy(ImVec2(ledSlot * 16.0f, 18.0f));

        /* Channel-number labels under each LED (1-based to match how DAWs
         * present MIDI channels). */
        ImVec2 labelOrigin = ImGui::GetCursorScreenPos();
        for (int ch = 0; ch < 16; ch++) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", ch + 1);
            ImVec2 sz = ImGui::CalcTextSize(buf);
            ImVec2 p;
            p.x = labelOrigin.x + ch * ledSlot + (14.0f - sz.x) * 0.5f;
            p.y = labelOrigin.y;
            drawList->AddText(p, ImGui::GetColorU32(ImGuiCol_TextDisabled), buf);
        }
        ImGui::Dummy(ImVec2(ledSlot * 16.0f, ImGui::GetTextLineHeight()));
    }
    ImGui::Spacing();

    draw_led(xcmdActive,
             ImVec4(0.95f, 0.75f, 0.18f, 1.0f),
             ImVec4(0.26f, 0.22f, 0.14f, 1.0f),
             "XCMD Traffic");
    draw_led(validXcmdActive,
             ImVec4(0.18f, 0.75f, 0.95f, 1.0f),
             ImVec4(0.15f, 0.22f, 0.28f, 1.0f),
             "Valid XCMD");
    ImGui::Text("XCMD Target: %s", gui->latestXcmd[0] ? gui->latestXcmd : "None");
    ImGui::Spacing();

    /* ---- Project Settings ---- */
    ImGui::SeparatorText("Project Settings");

    ImGui::AlignTextToFramePadding();
    if (gui->boldFont)
        ImGui::PushFont(gui->boldFont, 0.0f);
    ImGui::Text("Project Root:");
    if (gui->boldFont)
        ImGui::PopFont();
    ImGui::SameLine();
    const char *rootLabel = gui->projectRootBuf[0] ? gui->projectRootBuf : "Choose Project Root";
    if (project_root_button(rootLabel, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        open_project_root_browser(gui->projectRootBrowser, gui->projectRootBuf);
    display_project_root_browser(gui->projectRootBrowser);
    if (gui->projectRootBrowser.HasSelected()) {
        if (copy_path_utf8(gui->projectRootBuf, sizeof(gui->projectRootBuf),
                           gui->projectRootBrowser.GetSelected())) {
            snprintf(gui->settings.projectRoot, sizeof(gui->settings.projectRoot),
                     "%s", gui->projectRootBuf);
            snprintf(gui->settings.voicegroupName, sizeof(gui->settings.voicegroupName),
                     "%s", gui->voicegroupBuf);
            gui->settingsChanged = true;
            gui->reloadRequested = true;
        }
        gui->projectRootBrowser.ClearSelected();
    }

    ImGui::AlignTextToFramePadding();
    if (gui->boldFont)
        ImGui::PushFont(gui->boldFont, 0.0f);
    ImGui::Text("Voicegroup:  ");
    if (gui->boldFont)
        ImGui::PopFont();
    ImGui::SameLine();
    {
        float reloadW = 80.0f;
        float extractW = 80.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - reloadW - extractW - spacing * 2.0f);
    }
    ImGui::InputText("##vg", gui->voicegroupBuf, sizeof(gui->voicegroupBuf));
    ImGui::SameLine();
    if (ImGui::Button("Reload", ImVec2(80, 0))) {
        snprintf(gui->settings.projectRoot,    sizeof(gui->settings.projectRoot),
                 "%s", gui->projectRootBuf);
        snprintf(gui->settings.voicegroupName, sizeof(gui->settings.voicegroupName),
                 "%s", gui->voicegroupBuf);
        gui->settingsChanged = true;
        gui->reloadRequested = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!gui->settings.voicegroupLoaded);
    if (ImGui::Button("Extract", ImVec2(80, 0)))
        gui->extractRequested = true;
    ImGui::EndDisabled();

    /* Voicegroup load status */
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Status:      ");
    ImGui::SameLine();
    if (gui->settings.voicegroupLoaded)
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Voicegroup loaded");
    else
        ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.35f, 1.0f), "Voicegroup not loaded");
    if (gui->extractStatus[0]) {
        ImGui::Text("Extract:     ");
        ImGui::SameLine();
        ImGui::TextUnformatted(gui->extractStatus);
    }

    ImGui::Spacing();

    /* ---- Audio Settings ---- */
    ImGui::SeparatorText("Audio Settings");
    {
        int v = (int)gui->settings.volume;
        if (ImGui::SliderInt("Volume (0-127)", &v, 0, 127)) {
            gui->settings.volume = (uint8_t)v;
            gui->settingsChanged = true;
        }
    }
    {
        int v = (int)gui->settings.reverbAmount;
        if (ImGui::SliderInt("Reverb (0-127)", &v, 0, 127)) {
            gui->settings.reverbAmount = (uint8_t)v;
            gui->settingsChanged = true;
        }
    }

}

/* Portable case-insensitive substring search (strcasestr is not on Windows). */
static const char *ci_strstr(const char *haystack, const char *needle)
{
    if (!needle[0]) return haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { h++; n++; }
        if (!*n) return haystack;
    }
    return NULL;
}

/*
 * Render a filterable sample selector combo for a voice slot.
 */
static void render_sample_selector(M4AGuiState *gui, int voiceIndex,
                                   const char *label, ProjectAssetKind kind,
                                   std::span<const ProjectAssetEntry> assets)
{
    if (assets.empty()) return;

    ImGui::Spacing();
    ImGui::SeparatorText(label);

    /* Determine preview text: current override name, or placeholder */
    const char *preview = "(select sample...)";
    const char *currentName = NULL;
    if (gui->assetOverrides && gui->assetOverrides[voiceIndex].active) {
        currentName = gui->assetOverrides[voiceIndex].fileName;
        preview = currentName;
    }

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo("##sampleCombo", preview)) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##filter", "Filter...", gui->sampleFilterText, sizeof(gui->sampleFilterText));

        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere(-1);

        ImGui::Separator();

        for (const ProjectAssetEntry &asset : assets) {
            if (gui->sampleFilterText[0] != '\0' &&
                ci_strstr(asset.fileName, gui->sampleFilterText) == NULL)
                continue;

            bool isSelected = (currentName && strcmp(asset.fileName, currentName) == 0);
            if (ImGui::Selectable(asset.fileName, isSelected)) {
                gui->pendingSampleSwap = true;
                gui->pendingSwapVoice = voiceIndex;
                gui->pendingSwapKind = kind;
                strncpy(gui->pendingSwapFileName, asset.fileName,
                        sizeof(gui->pendingSwapFileName) - 1);
                gui->pendingSwapFileName[sizeof(gui->pendingSwapFileName) - 1] = '\0';
                ImGui::CloseCurrentPopup();
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
}

static void render_voices_tab(M4AGuiState *gui)
{
    if (gui->liveVoices.empty()) {
        ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.35f, 1.0f), "No voicegroup loaded");
        return;
    }

    /* Voice selector */
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
    ImGui::SliderInt("##voiceSlider", &gui->selectedVoice, 0, 127);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputInt("##voiceInput", &gui->selectedVoice, 1, 10);
    if (gui->selectedVoice < 0) gui->selectedVoice = 0;
    if (gui->selectedVoice > 127) gui->selectedVoice = 127;

    int idx = gui->selectedVoice;
    ToneData *voice = &gui->liveVoices[idx];
    uint8_t type = voice->type;
    bool hasVoiceOverrides = !gui->voiceOverrides.empty();

    /* Type label */
    ImGui::Text("Type: %s (0x%02X)", voice_type_name(type), type);
    if (type == VOICE_DIRECTSOUND_NO_RESAMPLE) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Fixed]");
    }

    /* Modified indicator */
    if (hasVoiceOverrides && gui->voiceOverrides[idx]) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "(modified)");
    }

    ImGui::Separator();

    bool changed = false;
    uint8_t baseType = type & ~VOICE_TYPE_FIX;

    /* Dispatch to per-type editor */
    if (baseType == 0x00) {
        /* DirectSound — key and panSweep are metadata only */
        ImGui::Text("Key: %d", voice->key);
        ImGui::Text("Pan/Sweep: %d (0x%02X)", voice->panSweep, voice->panSweep);
        changed |= edit_directsound_adsr(voice);

        /* Read-only sample info */
        if (voice->wav) {
            ImGui::Spacing();
            ImGui::SeparatorText("Sample Info");
            ImGui::Text("Size: %u samples", voice->wav->size);
            ImGui::Text("Frequency: %u Hz", voice->wav->freq);
            ImGui::Text("Loop: %s (start: %u)", (voice->wav->status & 0x4000) ? "Yes" : "No", voice->wav->loopStart);
        }

        render_sample_selector(gui, idx, "Sample", PROJECT_ASSET_DIRECTSOUND,
                               gui->directsoundAssets);
    } else if (baseType == 0x01) {
        /* Square 1 */
        int key = voice->key;
        if (ImGui::SliderInt("Key", &key, 0, 127)) { voice->key = (uint8_t)key; changed = true; }
        int sweep = voice->panSweep;
        if (ImGui::SliderInt("Sweep", &sweep, 0, 127)) { voice->panSweep = (uint8_t)sweep; changed = true; }
        int duty = (int)(uintptr_t)voice->wavePointer & 0x03;
        const char *dutyNames[] = { "12.5%", "25%", "50%", "75%" };
        if (ImGui::Combo("Duty Cycle", &duty, dutyNames, 4)) {
            voice->wavePointer = (uint32_t *)(uintptr_t)(duty & 0x03);
            changed = true;
        }
        changed |= edit_cgb_adsr(voice);
    } else if (baseType == 0x02) {
        /* Square 2 */
        int key = voice->key;
        if (ImGui::SliderInt("Key", &key, 0, 127)) { voice->key = (uint8_t)key; changed = true; }
        int duty = (int)(uintptr_t)voice->wavePointer & 0x03;
        const char *dutyNames[] = { "12.5%", "25%", "50%", "75%" };
        if (ImGui::Combo("Duty Cycle", &duty, dutyNames, 4)) {
            voice->wavePointer = (uint32_t *)(uintptr_t)(duty & 0x03);
            changed = true;
        }
        changed |= edit_cgb_adsr(voice);
    } else if (baseType == 0x03) {
        /* Programmable Wave */
        int key = voice->key;
        if (ImGui::SliderInt("Key", &key, 0, 127)) { voice->key = (uint8_t)key; changed = true; }
        changed |= edit_cgb_adsr(voice);

        render_sample_selector(gui, idx, "Wave", PROJECT_ASSET_PROG_WAVE,
                               gui->progWaveAssets);
    } else if (baseType == 0x04) {
        /* Noise */
        int key = voice->key;
        if (ImGui::SliderInt("Key", &key, 0, 127)) { voice->key = (uint8_t)key; changed = true; }
        int period = (int)(uintptr_t)voice->wavePointer & 0x01;
        const char *periodNames[] = { "Normal (15-bit)", "Metallic (7-bit)" };
        if (ImGui::Combo("Period", &period, periodNames, 2)) {
            voice->wavePointer = (uint32_t *)(uintptr_t)(period & 0x01);
            changed = true;
        }
        changed |= edit_cgb_adsr(voice);
    } else if (baseType == VOICE_CRY || baseType == VOICE_CRY_REVERSE) {
        /* Cry — read-only display */
        ImGui::Text("Key: %d", voice->key);
        ImGui::Text("Attack: %d  Decay: %d  Sustain: %d  Release: %d",
                     voice->attack, voice->decay, voice->sustain, voice->release);
        ImGui::TextDisabled("(Cry voices are read-only)");
    } else if (baseType == VOICE_KEYSPLIT) {
        ImGui::TextDisabled("(Keysplit voice — sub-voice editing not supported)");
    } else if (baseType == VOICE_KEYSPLIT_ALL) {
        ImGui::TextDisabled("(Drum Kit voice — sub-voice editing not supported)");
    } else {
        ImGui::TextDisabled("(Unknown voice type)");
    }

    if (changed) {
        if (hasVoiceOverrides)
            gui->voiceOverrides[idx] = true;
        gui->voicesDirty = true;
    }

    /* Restore button */
    if (hasVoiceOverrides && gui->voiceOverrides[idx]) {
        ImGui::Spacing();
        if (ImGui::Button("Restore Original")) {
            gui->pendingRestoreVoice = idx;
        }
    }
}

static void render_recorder_tab(M4AGuiState *gui)
{
    M4APluginData *data = static_cast<M4APluginData *>(gui->plugin_data);
    if (!data) {
        ImGui::TextUnformatted("Recorder UI wiring deferred — engine ptr plumbing");
        return;
    }

    /* Record toggle — only when this is on does the audio thread push MIDI
     * events into the recorder buffer. */
    bool armed = atomic_load(&data->recorderArmed);
    if (ImGui::Checkbox("Record", &armed))
        atomic_store(&data->recorderArmed, armed);

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        m4a_recorder_reset(data->recorder);
        atomic_store(&data->recorderSeenPC,  0u);
        atomic_store(&data->recorderSeenVol, 0u);
        atomic_store(&data->recorderSeenPan, 0u);
        gui->recorderStatus[0] = '\0';
    }

    /* Status counters */
    uint64_t evCount = m4a_recorder_event_count(data->recorder);
    ImGui::Text("Buffered: %llu events", (unsigned long long)evCount);

    /* Filename input */
    ImGui::InputText("Filename", data->recorderPath, sizeof(data->recorderPath));

    /* Save button */
    if (ImGui::Button("Save SMF")) {
        if (data->recorderTempoBpm <= 0.0) {
            snprintf(gui->recorderStatus, sizeof(gui->recorderStatus),
                     "Failed: host tempo required");
#if !defined(_WIN32)
        } else if (strchr(data->recorderPath, '\\')) {
            snprintf(gui->recorderStatus, sizeof(gui->recorderStatus),
                     "Failed: use / path separators");
#endif
        } else {
            bool ok = m4a_recorder_save_smf(data->recorder, data->recorderPath, 96,
                                            data->recorderTempoBpm);
            snprintf(gui->recorderStatus, sizeof(gui->recorderStatus),
                     ok ? "Saved: %s" : "Failed: %s", data->recorderPath);
        }
    }

    if (gui->recorderStatus[0])
        ImGui::TextUnformatted(gui->recorderStatus);

    /* Per-channel capture indicators: 12 rows, three LEDs each.
     * A LED lights green once the recorder has captured an event of that
     * type for that channel since the last Clear. */
    ImGui::Separator();
    uint32_t pcMask  = atomic_load(&data->recorderSeenPC);
    uint32_t volMask = atomic_load(&data->recorderSeenVol);
    uint32_t panMask = atomic_load(&data->recorderSeenPan);
    auto led = [](uint32_t mask, int ch) {
        bool on = (mask >> ch) & 1u;
        ImGui::TextColored(on ? ImVec4(0.18f, 0.95f, 0.35f, 1.0f)
                              : ImVec4(0.18f, 0.24f, 0.20f, 1.0f), "\xE2\x97\x8F");
    };
    if (ImGui::BeginTable("rec_indicators", 4, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Ch");
        ImGui::TableSetupColumn("PC");
        ImGui::TableSetupColumn("Vol");
        ImGui::TableSetupColumn("Pan");
        ImGui::TableHeadersRow();
        for (int ch = 0; ch < 12; ch++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%d", ch + 1);
            ImGui::TableSetColumnIndex(1); led(pcMask,  ch);
            ImGui::TableSetColumnIndex(2); led(volMask, ch);
            ImGui::TableSetColumnIndex(3); led(panMask, ch);
        }
        ImGui::EndTable();
    }
}

static void render_frame(M4AGuiState *gui, uint32_t width, uint32_t height)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));

    ImGuiWindowFlags wflags =
        ImGuiWindowFlags_NoTitleBar      |
        ImGuiWindowFlags_NoResize        |
        ImGuiWindowFlags_NoMove          |
        ImGuiWindowFlags_NoCollapse      |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##Main", nullptr, wflags);

    /* ---- Plugin title ---- */
    ImGui::TextColored(ImVec4(0.3f, 0.75f, 1.0f, 1.0f), "poryaaaa");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 160.0f);
    ImGui::TextDisabled("pokeemerald");
    ImGui::Separator();
    ImGui::Spacing();

    /* ---- Tabbed content ---- */
    if (ImGui::BeginTabBar("##Tabs")) {
        if (ImGui::BeginTabItem("General")) {
            render_general_tab(gui);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Voices")) {
            render_voices_tab(gui);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Recorder")) {
            render_recorder_tab(gui);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

static void shell_draw_frame(void *userData, uint32_t width, uint32_t height)
{
    M4AGuiState *gui = (M4AGuiState *)userData;
    if (gui)
        render_frame(gui, width, height);
}

static void shell_closed(void *userData, bool wasDestroyed)
{
    M4AGuiState *gui = (M4AGuiState *)userData;
    if (!gui)
        return;

    gui->wasClosed = true;
    m4a_gui_stop_internal_timer(gui);
    if (gui->host) {
        const clap_host_gui_t *hostGui =
            (const clap_host_gui_t *)gui->host->get_extension(gui->host, CLAP_EXT_GUI);
        if (hostGui)
            hostGui->closed(gui->host, wasDestroyed);
    }
}

static void shell_timer(void *userData)
{
    M4AGuiState *gui = (M4AGuiState *)userData;
    if (!gui)
        return;

    if (gui->internalTimerCallback)
        gui->internalTimerCallback(gui->internalTimerUserData);
    else {
        gui_log("More than 1 Pugl timer is running. That's odd...");
        m4a_gui_tick(gui);
    }
}

static void shell_setup_style(void *userData, ImFont *, ImFont *boldFont)
{
    M4AGuiState *gui = (M4AGuiState *)userData;
    if (!gui)
        return;

    gui->boldFont = boldFont;

    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowPadding    = ImVec2(12, 12);
    style.ItemSpacing      = ImVec2(8, 6);
    style.FramePadding     = ImVec2(6, 4);
    style.GrabMinSize      = 10.0f;
    style.WindowRounding   = 4.0f;
    style.FrameRounding    = 3.0f;
    style.GrabRounding     = 3.0f;
}

/* ---- Public C interface ---- */

extern "C" {

M4AGuiState *m4a_gui_create(const clap_host_t *host, const M4AGuiSettings *initial,
                             const char *log_path)
{
    s_logPath = log_path;
    // gui_log("m4a_gui_create: begin");

    M4AGuiState *gui = new M4AGuiState();
    gui->host         = host;
    gui->selectedVoice       = 0;
    gui->pendingRestoreVoice = -1;
    gui->projectRootBrowser.SetTitle("Choose Project Root");
    gui->projectRootBrowser.SetWindowSize(700, 450);

    if (initial) {
        gui->settings = *initial;
    } else {
        memset(&gui->settings, 0, sizeof(gui->settings));
        gui->settings.volume = 127;
    }
    sync_buffers(gui);

    poryaaaa::gui::ImGuiPuglShellConfig shellConfig;
    shellConfig.title = "poryaaaa";
    shellConfig.className = "poryaaaa";
    shellConfig.defaultWidth = (uint32_t)GUI_W;
    shellConfig.defaultHeight = (uint32_t)GUI_H;
    shellConfig.minWidth = 200;
    shellConfig.minHeight = 150;
    shellConfig.fontSize = text::Base;
    shellConfig.regularFontPath = m4a_gui_regular_font_path();
    shellConfig.boldFontPath = m4a_gui_bold_font_path();
    shellConfig.glslVersion = "#version 330 core";

    poryaaaa::gui::ImGuiPuglShellCallbacks shellCallbacks;
    shellCallbacks.userData = gui;
    shellCallbacks.drawFrame = shell_draw_frame;
    shellCallbacks.closed = shell_closed;
    shellCallbacks.timer = shell_timer;
    shellCallbacks.setupStyle = shell_setup_style;

    gui->shell = poryaaaa::gui::imgui_pugl_shell_create(shellConfig, shellCallbacks);
    if (!gui->shell) {
        gui_log("m4a_gui_create: shared Pugl shell creation failed");
        delete gui;
        return nullptr;
    }

    gui_log("m4a_gui_create: success");
    return gui;
}

void m4a_gui_destroy(M4AGuiState *gui)
{
    if (!gui)
        return;

    poryaaaa::gui::imgui_pugl_shell_destroy(gui->shell);
    gui->shell = nullptr;

    delete gui;
    gui_log("m4a_gui_destroy: done");
}

bool m4a_gui_set_parent(M4AGuiState *gui, uintptr_t native_parent)
{
    gui_log("m4a_gui_set_parent: parent=0x%zx", (size_t)native_parent);
    if (!gui || !gui->shell) return false;
    if (!poryaaaa::gui::imgui_pugl_shell_set_parent(gui->shell, native_parent)) {
        gui_log("m4a_gui_set_parent: shared Pugl shell parent/realize failed");
        return false;
    }
    gui_log("m4a_gui_set_parent: success");
    return true;
}

bool m4a_gui_show(M4AGuiState *gui)
{
    gui_log("m4a_gui_show called");
    return gui && poryaaaa::gui::imgui_pugl_shell_show(gui->shell);
}

bool m4a_gui_hide(M4AGuiState *gui)
{
    return gui && poryaaaa::gui::imgui_pugl_shell_hide(gui->shell);
}

void m4a_gui_get_size(M4AGuiState *gui, uint32_t *width, uint32_t *height)
{
    if (!gui) {
        *width  = (uint32_t)GUI_W;
        *height = (uint32_t)GUI_H;
        return;
    }
    poryaaaa::gui::imgui_pugl_shell_get_size(gui->shell, width, height);
}

bool m4a_gui_set_size(M4AGuiState *gui, uint32_t width, uint32_t height)
{
    return gui &&
           poryaaaa::gui::imgui_pugl_shell_set_size(gui->shell, width, height);
}

bool m4a_gui_can_resize(M4AGuiState *gui)
{
    return gui && poryaaaa::gui::imgui_pugl_shell_can_resize(gui->shell);
}

void m4a_gui_update_settings(M4AGuiState *gui, const M4AGuiSettings *settings)
{
    if (!gui || !settings) return;
    gui->settings = *settings;
    sync_buffers(gui);
}

void m4a_gui_pulse_midi_activity(M4AGuiState *gui, int channel)
{
    if (!gui || !gui->shell)
        return;
    if (channel < 0 || channel >= 16)
        return;
    ImGui::SetCurrentContext(poryaaaa::gui::imgui_pugl_shell_context(gui->shell));
    gui->midiActivityUntil[channel] = ImGui::GetTime() + 0.15;
}

void m4a_gui_pulse_xcmd_activity(M4AGuiState *gui)
{
    if (!gui || !gui->shell)
        return;
    ImGui::SetCurrentContext(poryaaaa::gui::imgui_pugl_shell_context(gui->shell));
    gui->xcmdActivityUntil = ImGui::GetTime() + 0.15;
}

void m4a_gui_pulse_valid_xcmd(M4AGuiState *gui)
{
    if (!gui || !gui->shell)
        return;
    ImGui::SetCurrentContext(poryaaaa::gui::imgui_pugl_shell_context(gui->shell));
    gui->validXcmdUntil = ImGui::GetTime() + 0.15;
}

void m4a_gui_set_latest_xcmd(M4AGuiState *gui, const char *text)
{
    if (!gui)
        return;

    snprintf(gui->latestXcmd, sizeof(gui->latestXcmd), "%s", text ? text : "");
}

bool m4a_gui_poll_changes(M4AGuiState *gui, M4AGuiSettings *out, bool *reload_voicegroup)
{
    if (!gui || !gui->settingsChanged)
        return false;

    *out               = gui->settings;
    *reload_voicegroup = gui->reloadRequested;
    gui->settingsChanged  = false;
    gui->reloadRequested  = false;
    return true;
}

bool m4a_gui_poll_extract_request(M4AGuiState *gui)
{
    if (!gui || !gui->extractRequested)
        return false;
    gui->extractRequested = false;
    return true;
}

void m4a_gui_set_extract_status(M4AGuiState *gui, const char *status)
{
    if (!gui)
        return;
    snprintf(gui->extractStatus, sizeof(gui->extractStatus), "%s", status ? status : "");
}

bool m4a_gui_was_closed(M4AGuiState *gui)
{
    return gui && gui->wasClosed;
}

void m4a_gui_tick(M4AGuiState *gui)
{
    if (!gui || !gui->shell)
        return;
    poryaaaa::gui::imgui_pugl_shell_tick(gui->shell);
}

void m4a_gui_set_internal_timer_callback(M4AGuiState *gui,
                                          M4AGuiTimerCallback callback,
                                          void *user_data)
{
    if (!gui)
        return;
    gui->internalTimerCallback = callback;
    gui->internalTimerUserData = user_data;
}

void m4a_gui_set_voice_data(M4AGuiState *gui,
                             ToneData *liveVoices,
                             const ToneData *originalVoices,
                             bool *overrides)
{
    if (!gui) return;
    (void)originalVoices;
    gui->liveVoices = liveVoices ? std::span<ToneData>(liveVoices, VOICE_SLOT_COUNT) : std::span<ToneData>();
    gui->voiceOverrides = overrides ? std::span<bool>(overrides, VOICE_SLOT_COUNT) : std::span<bool>();
    if (!liveVoices)
        gui->pendingRestoreVoice = -1;
}

bool m4a_gui_poll_voice_restore(M4AGuiState *gui, int *voiceIndex)
{
    if (!gui || gui->pendingRestoreVoice < 0)
        return false;
    *voiceIndex = gui->pendingRestoreVoice;
    gui->pendingRestoreVoice = -1;
    return true;
}

bool m4a_gui_poll_voices_dirty(M4AGuiState *gui)
{
    if (!gui || !gui->voicesDirty)
        return false;
    gui->voicesDirty = false;
    return true;
}

void m4a_gui_start_internal_timer(M4AGuiState *gui)
{
    if (!gui || !gui->shell)
        return;
    poryaaaa::gui::imgui_pugl_shell_start_timer(gui->shell);
}

void m4a_gui_stop_internal_timer(M4AGuiState *gui)
{
    if (!gui || !gui->shell)
        return;
    poryaaaa::gui::imgui_pugl_shell_stop_timer(gui->shell);
}

void m4a_gui_set_project_assets(M4AGuiState *gui,
                                const ProjectAssetEntry *directsoundAssets,
                                int directsoundCount,
                                const ProjectAssetEntry *progWaveAssets,
                                int progWaveCount,
                                const ProjectAssetOverride *overrides)
{
    if (!gui) return;
    gui->directsoundAssets = project_asset_span(directsoundAssets, directsoundCount);
    gui->progWaveAssets = project_asset_span(progWaveAssets, progWaveCount);
    gui->assetOverrides = overrides;
}

void m4a_gui_set_plugin_data(M4AGuiState *gui, void *plugin_data)
{
    if (!gui) return;
    gui->plugin_data = plugin_data;
}

bool m4a_gui_poll_sample_swap(M4AGuiState *gui, int *voiceIndex,
                              ProjectAssetKind *kind,
                              char *fileName, int fileNameSize)
{
    if (!gui || !gui->pendingSampleSwap)
        return false;
    *voiceIndex = gui->pendingSwapVoice;
    *kind = gui->pendingSwapKind;
    strncpy(fileName, gui->pendingSwapFileName, (size_t)fileNameSize - 1);
    fileName[fileNameSize - 1] = '\0';
    gui->pendingSampleSwap = false;
    return true;
}
} /* extern "C" */
