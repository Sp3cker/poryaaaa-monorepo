#include <stdio.h>

#include "m4a_plugin.h"

extern const clap_plugin_entry_t clap_entry;

int tests_run;
int tests_passed;

static const char* FRONTEND_CONFIG_PATH = "test/fixtures/state_compat/poryaaaa.cfg";
static const char* FRONTEND_PLUGIN_PATH = "test/fixtures/state_compat/frontend-test.clap";

static bool write_frontend_config(const char* mixerMode)
{
    FILE* file = fopen(FRONTEND_CONFIG_PATH, "wb");
    if (!file)
        return false;
    const bool written = fprintf(file, "pcm_mixer=%s\n", mixerMode) > 0;
    return fclose(file) == 0 && written;
}

struct M4AGuiState
{
    int unused;
};

M4AGuiState* m4a_gui_create(const clap_host_t* host, const M4AGuiSettings* initial, const char* log_path)
{
    (void)host;
    (void)initial;
    (void)log_path;
    return NULL;
}

void m4a_gui_destroy(M4AGuiState* gui)
{
    (void)gui;
}

bool m4a_gui_show(M4AGuiState* gui)
{
    (void)gui;
    return false;
}

bool m4a_gui_hide(M4AGuiState* gui)
{
    (void)gui;
    return false;
}

void m4a_gui_get_size(M4AGuiState* gui, uint32_t* width, uint32_t* height)
{
    (void)gui;
    if (width)
        *width = 0;
    if (height)
        *height = 0;
}

bool m4a_gui_set_size(M4AGuiState* gui, uint32_t width, uint32_t height)
{
    (void)gui;
    (void)width;
    (void)height;
    return false;
}

bool m4a_gui_can_resize(M4AGuiState* gui)
{
    (void)gui;
    return false;
}

bool m4a_gui_set_parent(M4AGuiState* gui, uintptr_t native_parent)
{
    (void)gui;
    (void)native_parent;
    return false;
}

void m4a_gui_tick(M4AGuiState* gui)
{
    (void)gui;
}

void m4a_gui_set_internal_timer_callback(M4AGuiState* gui, M4AGuiTimerCallback callback, void* user_data)
{
    (void)gui;
    (void)callback;
    (void)user_data;
}

void m4a_gui_start_internal_timer(M4AGuiState* gui)
{
    (void)gui;
}

void m4a_gui_stop_internal_timer(M4AGuiState* gui)
{
    (void)gui;
}

bool m4a_gui_was_closed(M4AGuiState* gui)
{
    (void)gui;
    return false;
}

void m4a_gui_update_settings(M4AGuiState* gui, const M4AGuiSettings* settings)
{
    (void)gui;
    (void)settings;
}

void m4a_gui_set_pcm_mixer_mode(M4AGuiState* gui, M4APcmMixerMode mode)
{
    (void)gui;
    (void)mode;
}

void m4a_gui_pulse_midi_activity(M4AGuiState* gui, int channel)
{
    (void)gui;
    (void)channel;
}

bool m4a_gui_poll_changes(M4AGuiState* gui, M4AGuiSettings* out, bool* reload_voicegroup)
{
    (void)gui;
    (void)out;
    (void)reload_voicegroup;
    return false;
}

void m4a_gui_set_voice_data(M4AGuiState* gui, ToneData* live_voices, const ToneData* original_voices, bool* overrides)
{
    (void)gui;
    (void)live_voices;
    (void)original_voices;
    (void)overrides;
}

void m4a_gui_set_plugin_data(M4AGuiState* gui, void* plugin_data)
{
    (void)gui;
    (void)plugin_data;
}

bool m4a_gui_poll_voice_restore(M4AGuiState* gui, int* voice_index)
{
    (void)gui;
    (void)voice_index;
    return false;
}

bool m4a_gui_poll_voices_dirty(M4AGuiState* gui)
{
    (void)gui;
    return false;
}

void m4a_gui_set_project_assets(M4AGuiState* gui,
                                const ProjectAssetEntry* directsound_assets,
                                int directsound_count,
                                const ProjectAssetEntry* prog_wave_assets,
                                int prog_wave_count,
                                const ProjectAssetOverride* overrides)
{
    (void)gui;
    (void)directsound_assets;
    (void)directsound_count;
    (void)prog_wave_assets;
    (void)prog_wave_count;
    (void)overrides;
}

bool m4a_gui_poll_sample_swap(
    M4AGuiState* gui, int* voice_index, ProjectAssetKind* kind, char* file_name, int file_name_size)
{
    (void)gui;
    (void)voice_index;
    (void)kind;
    (void)file_name;
    (void)file_name_size;
    return false;
}

void test_m4a_state_compat_run_all(const clap_plugin_t* plugin);

int main(void)
{
    if (!write_frontend_config("sappy"))
    {
        fprintf(stderr, "C plugin state test could not write its config fixture\n");
        return 1;
    }
    if (!clap_entry.init(FRONTEND_PLUGIN_PATH))
    {
        remove(FRONTEND_CONFIG_PATH);
        fprintf(stderr, "CLAP entry initialization failed\n");
        return 1;
    }

    const clap_plugin_factory_t* factory = (const clap_plugin_factory_t*)clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID);
    if (!factory)
    {
        remove(FRONTEND_CONFIG_PATH);
        fprintf(stderr, "CLAP plugin factory is unavailable\n");
        clap_entry.deinit();
        return 1;
    }
    const clap_plugin_t* plugin = factory->create_plugin(factory, NULL, "com.huderlem.poryaaaa");
    const bool pluginInitialized = plugin && plugin->init(plugin);
    remove(FRONTEND_CONFIG_PATH);
    if (!pluginInitialized)
    {
        fprintf(stderr, "C plugin state test could not create an initialized plugin\n");
        if (plugin)
            plugin->destroy(plugin);
        clap_entry.deinit();
        return 1;
    }

    test_m4a_state_compat_run_all(plugin);
    plugin->destroy(plugin);

    if (!write_frontend_config("Sappy"))
    {
        fprintf(stderr, "C plugin state test could not write its invalid config fixture\n");
        clap_entry.deinit();
        return 1;
    }
    const clap_plugin_t* invalidPlugin = factory->create_plugin(factory, NULL, "com.huderlem.poryaaaa");
    const bool invalidRejected = invalidPlugin && !invalidPlugin->init(invalidPlugin);
    remove(FRONTEND_CONFIG_PATH);
    if (invalidPlugin)
        invalidPlugin->destroy(invalidPlugin);
    if (!invalidRejected)
    {
        fprintf(stderr, "C plugin accepted an invalid mixer configuration\n");
        clap_entry.deinit();
        return 1;
    }
    clap_entry.deinit();

    printf("C state compatibility: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
