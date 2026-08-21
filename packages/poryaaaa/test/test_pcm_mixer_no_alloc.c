#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "m4a/m4a_driver.h"
#include "voicegroup/voicegroup_loader.h"

extern uint64_t poryaaaa_test_malloc_calls;
extern uint64_t poryaaaa_test_calloc_calls;
extern uint64_t poryaaaa_test_realloc_calls;
extern uint64_t poryaaaa_test_free_calls;

static int8_t fixture_samples[128];
static WaveData fixture_wave;
static ToneData fixture_voices[VOICEGROUP_SIZE];

typedef struct
{
    uint64_t malloc_calls;
    uint64_t calloc_calls;
    uint64_t realloc_calls;
    uint64_t free_calls;
} AllocationSnapshot;

static void setup_fixture(void)
{
    memset(fixture_samples, 0, sizeof(fixture_samples));
    for (size_t index = 0; index < sizeof(fixture_samples); ++index)
        fixture_samples[index] = (int8_t)((index & 1u) ? 96 : -96);

    fixture_wave = (WaveData){
        .type = 0,
        .status = 0xC000u,
        .freq = 22050u << 10u,
        .loopStart = 0,
        .size = (uint32_t)sizeof(fixture_samples),
        .data = fixture_samples,
    };
    memset(fixture_voices, 0, sizeof(fixture_voices));
    fixture_voices[0] = (ToneData){
        .type = VOICE_DIRECTSOUND,
        .key = 60,
        .length = 0,
        .panSweep = 0,
        .wav = &fixture_wave,
        .attack = 0x10,
        .decay = 0x20,
        .sustain = 0x80,
        .release = 0x20,
    };
}

static AllocationSnapshot allocation_snapshot(void)
{
    return (AllocationSnapshot){
        .malloc_calls = poryaaaa_test_malloc_calls,
        .calloc_calls = poryaaaa_test_calloc_calls,
        .realloc_calls = poryaaaa_test_realloc_calls,
        .free_calls = poryaaaa_test_free_calls,
    };
}

static bool render_and_consume(M4ADriver* driver, int frames)
{
    m4a_advance(driver, frames);
    const M4ARegWriteBatch* batch = m4a_get_pending_writes(driver);
    if (!batch)
        return false;
    m4a_consume_writes(driver);
    return true;
}

static bool unchanged(AllocationSnapshot before)
{
    return poryaaaa_test_malloc_calls == before.malloc_calls && poryaaaa_test_calloc_calls == before.calloc_calls &&
           poryaaaa_test_realloc_calls == before.realloc_calls && poryaaaa_test_free_calls == before.free_calls;
}

int main(void)
{
    setup_fixture();
    M4ADriver* driver = m4a_driver_create(44100.0f);
    if (!driver)
    {
        fprintf(stderr, "m4a_driver_create failed\n");
        return 1;
    }

    m4a_driver_set_voicegroup(driver, fixture_voices);
    m4a_driver_set_pcm_mix_rate(driver, 44100.0f);
    m4a_set_max_pcm_channels(driver, 1);
    m4a_set_master_volume(driver, 12);
    m4a_set_reverb_amount(driver, 64);
    m4a_program_change(driver, 0, 0);
    m4a_cc(driver, 0, 7, 127);
    m4a_cc(driver, 0, 10, 64);
    m4a_note_on(driver, 0, 60, 100);
    if (!m4a_driver_set_pcm_mixer_mode(driver, M4A_PCM_MIXER_IPATIX))
    {
        fprintf(stderr, "initial iPatix mode request failed\n");
        m4a_driver_destroy(driver);
        return 1;
    }

    AllocationSnapshot before = allocation_snapshot();

    /* First render, repeated active-mode requests, a real request, its
     * SoundMain/VBlank commit, and the following render are all measured. */
    bool ok = render_and_consume(driver, 2048);
    ok = ok && m4a_driver_set_pcm_mixer_mode(driver, M4A_PCM_MIXER_IPATIX);
    ok = ok && m4a_driver_set_pcm_mixer_mode(driver, M4A_PCM_MIXER_IPATIX);
    ok = ok && m4a_driver_set_pcm_mixer_mode(driver, M4A_PCM_MIXER_SAPPY);
    ok = ok && render_and_consume(driver, 2048);
    ok = ok && render_and_consume(driver, 2048);

    if (!ok || !unchanged(before))
    {
        fprintf(stderr,
                "PCM mixer allocated during measured interval (malloc=%llu calloc=%llu realloc=%llu free=%llu)\n",
                (unsigned long long)(poryaaaa_test_malloc_calls - before.malloc_calls),
                (unsigned long long)(poryaaaa_test_calloc_calls - before.calloc_calls),
                (unsigned long long)(poryaaaa_test_realloc_calls - before.realloc_calls),
                (unsigned long long)(poryaaaa_test_free_calls - before.free_calls));
        m4a_driver_destroy(driver);
        return 1;
    }

    m4a_driver_destroy(driver);
    return 0;
}
