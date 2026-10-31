#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "m4a/m4a_internal.h"

static int failures;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                                         \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)

/* Run a real SoundMain and consume its unrelated hardware events. */
static void frame(M4ADriver* driver)
{
    m4a_advance(driver, (int)M4A_VBLANK_CYCLES);
    m4a_consume_writes(driver);
}

/* With FIFO refills beyond the frame, the ROM's paused and start paths have fixed costs. */
static void test_calibrated_player_paths(void)
{
    M4ADriver* paused = m4a_driver_create((float)M4A_GBA_CYCLES_PER_SECOND);
    M4ADriver* start = m4a_driver_create((float)M4A_GBA_CYCLES_PER_SECOND);
    M4ADriver* resume = m4a_driver_create((float)M4A_GBA_CYCLES_PER_SECOND);
    CHECK(paused && start && resume);
    if (!paused || !start || !resume)
        goto done;

    M4ADriver* drivers[] = {paused, start, resume};
    for (size_t i = 0; i < 3; ++i)
    {
        m4a_driver_cpu_set_enabled(drivers[i], true);
        drivers[i]->next_pcm_timer_cycle = M4A_VBLANK_CYCLES + 100000u;
    }
    m4a_driver_cpu_song_start(start, 1u, M4A_PLAYER_BGM, 10);
    m4a_driver_cpu_song_continue(resume, 1u, M4A_PLAYER_BGM, 10);
    for (size_t i = 0; i < 3; ++i)
        frame(drivers[i]);

    M4AFrameCost p = {0}, s = {0}, r = {0};
    uint64_t pc = 0, sc = 0, rc = 0;
    CHECK(m4a_driver_cpu_read(paused, &pc, &p, 1) == 1);
    CHECK(m4a_driver_cpu_read(start, &sc, &s, 1) == 1);
    CHECK(m4a_driver_cpu_read(resume, &rc, &r, 1) == 1);
    CHECK(p.valid && s.valid && r.valid);
    CHECK(p.sequencer == 822);
    CHECK(s.sequencer == 2179);
    CHECK(r.sequencer == 1748);
    CHECK(s.sequencerEstimated && s.cycle == M4A_VBLANK_CYCLES);
    CHECK(s.total == s.sequencer + s.cgb + s.mixer);

done:
    m4a_driver_destroy(paused);
    m4a_driver_destroy(start);
    m4a_driver_destroy(resume);
}

/* Refill stalls, tempo ticks, controller commands and loop GOTOs change actual path charges. */
static void test_timing_and_commands(void)
{
    M4ADriver* stall = m4a_driver_create((float)M4A_GBA_CYCLES_PER_SECOND);
    M4ADriver* normal = m4a_driver_create((float)M4A_GBA_CYCLES_PER_SECOND);
    M4ADriver* changed = m4a_driver_create((float)M4A_GBA_CYCLES_PER_SECOND);
    CHECK(stall && normal && changed);
    if (!stall || !normal || !changed)
        goto done;
    m4a_driver_cpu_set_enabled(stall, true);
    stall->next_pcm_timer_cycle = M4A_VBLANK_CYCLES + 600u;
    frame(stall);
    uint64_t sc = 0;
    M4AFrameCost s = {0};
    CHECK(m4a_driver_cpu_read(stall, &sc, &s, 1) == 1);
    CHECK(s.sequencer == 840);

    m4a_driver_cpu_set_enabled(normal, true);
    m4a_driver_cpu_set_enabled(changed, true);
    m4a_driver_cpu_song_start(normal, 1u, M4A_PLAYER_BGM, 10);
    m4a_driver_cpu_song_start(changed, 3u, M4A_PLAYER_BGM, 10);
    m4a_set_tempo_bpm(changed, 240.0);
    m4a_cc(changed, 0, 0x01, 40);
    m4a_cc(changed, 0, 0x07, 100);
    frame(normal);
    frame(changed);
    uint64_t nc = 0, cc = 0;
    M4AFrameCost n = {0}, c = {0};
    CHECK(m4a_driver_cpu_read(normal, &nc, &n, 1) == 1);
    CHECK(m4a_driver_cpu_read(changed, &cc, &c, 1) == 1);
    CHECK(c.sequencer > n.sequencer);

    m4a_driver_cpu_time_signature_phase(normal, 96, 20);
    m4a_driver_cpu_time_signature_phase(changed, 96, 20);
    m4a_driver_cpu_loop_start_meter(changed, 96, 20);
    m4a_driver_cpu_loop_point(changed, true);
    frame(normal);
    frame(changed);
    CHECK(m4a_driver_cpu_read(normal, &nc, &n, 1) == 1);
    CHECK(m4a_driver_cpu_read(changed, &cc, &c, 1) == 1);
    CHECK(c.sequencer > n.sequencer);
    m4a_driver_cpu_track_end(changed, 0);
    m4a_driver_cpu_track_end(changed, 1);
    frame(changed);
    CHECK(m4a_driver_cpu_read(changed, &cc, &c, 1) == 1);
    CHECK(c.valid);

done:
    m4a_driver_destroy(stall);
    m4a_driver_destroy(normal);
    m4a_driver_destroy(changed);
}

/* A loop GOTO emits one command and one wait on every declared track. */
static void test_loop_path_cost(void)
{
    M4ADriver* plain = m4a_driver_create((float)M4A_GBA_CYCLES_PER_SECOND);
    M4ADriver* loop = m4a_driver_create((float)M4A_GBA_CYCLES_PER_SECOND);
    CHECK(plain && loop);
    if (!plain || !loop)
        goto done;
    m4a_driver_cpu_set_enabled(plain, true);
    m4a_driver_cpu_set_enabled(loop, true);
    m4a_driver_cpu_song_start(plain, 1u, M4A_PLAYER_BGM, 10);
    m4a_driver_cpu_song_start(loop, 1u, M4A_PLAYER_BGM, 10);
    plain->next_pcm_timer_cycle = loop->next_pcm_timer_cycle = M4A_VBLANK_CYCLES + 100000u;
    frame(plain);
    frame(loop);
    plain->next_pcm_timer_cycle = loop->next_pcm_timer_cycle = 2u * M4A_VBLANK_CYCLES + 100000u;
    m4a_driver_cpu_time_signature_phase(plain, 96, 20);
    m4a_driver_cpu_time_signature_phase(loop, 96, 20);
    m4a_driver_cpu_loop_start_meter(loop, 96, 20);
    m4a_driver_cpu_loop_point(loop, true);
    frame(plain);
    frame(loop);
    uint64_t pc = 1, lc = 1;
    M4AFrameCost p = {0}, l = {0};
    CHECK(m4a_driver_cpu_read(plain, &pc, &p, 1) == 1);
    CHECK(m4a_driver_cpu_read(loop, &lc, &l, 1) == 1);
    CHECK(l.sequencer == p.sequencer + 300);

done:
    m4a_driver_destroy(plain);
    m4a_driver_destroy(loop);
}

/* Notes chased before enabling are restored to the estimated track chain in allocation order. */
static void test_chased_notes(void)
{
    static int8_t samples[64];
    static ToneData voices[128];
    WaveData wave = {.freq = 22050u << 10, .size = sizeof(samples), .data = samples, .status = 0xC000u};
    voices[0] = (ToneData){.type = VOICE_DIRECTSOUND, .wav = &wave, .attack = 0xFF,
                           .decay = 0xFE, .sustain = 0xC0, .release = 0xD0};
    M4ADriver* driver = m4a_driver_create((float)M4A_GBA_CYCLES_PER_SECOND);
    CHECK(driver);
    if (!driver)
        return;
    m4a_driver_set_voicegroup(driver, voices);
    m4a_program_change(driver, 0, 0);
    m4a_note_on(driver, 0, 60, 100);
    m4a_note_on(driver, 0, 64, 100);
    m4a_driver_cpu_set_enabled(driver, true);
    m4a_driver_cpu_song_continue(driver, 1u, M4A_PLAYER_BGM, 10);
    CHECK(driver->cpu_seq.owner[0] == 1);
    CHECK(driver->cpu_seq.owner[1] == 1);
    CHECK(driver->cpu_seq.chSeq[1] > driver->cpu_seq.chSeq[0]);
    frame(driver);
    uint64_t cursor = 0;
    M4AFrameCost cost = {0};
    CHECK(m4a_driver_cpu_read(driver, &cursor, &cost, 1) == 1);
    CHECK(cost.valid && cost.sequencer > 1748);
    m4a_driver_destroy(driver);
}

/* Every SoundMain is retained independently, including across disabled spans. */
static void test_history_and_enable(void)
{
    M4ADriver* driver = m4a_driver_create((float)M4A_GBA_CYCLES_PER_SECOND);
    CHECK(driver);
    if (!driver)
        return;
    frame(driver);
    uint64_t cursor = 0;
    M4AFrameCost out[M4A_CPU_COST_HISTORY] = {0};
    CHECK(m4a_driver_cpu_read(driver, &cursor, out, M4A_CPU_COST_HISTORY) == 0);
    m4a_driver_cpu_set_enabled(driver, true);
    for (int i = 0; i < 70; ++i)
        frame(driver);
    m4a_driver_cpu_set_enabled(driver, false);
    CHECK(m4a_driver_cpu_read(driver, &cursor, out, M4A_CPU_COST_HISTORY) == M4A_CPU_COST_HISTORY);
    CHECK(out[0].cycle == 8u * M4A_VBLANK_CYCLES);
    CHECK(out[M4A_CPU_COST_HISTORY - 1].cycle == 71u * M4A_VBLANK_CYCLES);
    frame(driver);
    m4a_driver_cpu_set_enabled(driver, true);
    frame(driver);
    CHECK(m4a_driver_cpu_read(driver, &cursor, out, 1) == 1);
    CHECK(out[0].cycle == 73u * M4A_VBLANK_CYCLES);
    CHECK(m4a_driver_cpu_read(driver, &cursor, out, 1) == 0);
    m4a_driver_destroy(driver);
}

/* Unsupported host-rate geometries are not presented as a GBA result. */
static void test_unsupported_geometry(void)
{
    M4ADriver* driver = m4a_driver_create(44100.0f);
    CHECK(driver);
    if (!driver)
        return;
    m4a_driver_cpu_set_enabled(driver, true);
    m4a_driver_set_pcm_mix_rate(driver, 0.0f);
    m4a_advance(driver, 2048);
    uint64_t cursor = 0;
    M4AFrameCost cost = {0};
    CHECK(m4a_driver_cpu_read(driver, &cursor, &cost, 1) == 1);
    CHECK(!cost.valid);
    m4a_driver_destroy(driver);
}

/* Instrumentation must leave PCM and ordered hardware output bit-identical. */
static void test_audio_unchanged_and_preview_invalid(void)
{
    static int8_t samples[64];
    static ToneData voices[128];
    WaveData wave = {.freq = 22050u << 10, .size = sizeof(samples), .data = samples, .status = 0xC000u};
    for (size_t i = 0; i < sizeof(samples); ++i)
        samples[i] = (int8_t)((i & 1u) ? 96 : -96);
    voices[0] = (ToneData){.type = VOICE_DIRECTSOUND, .wav = &wave, .attack = 0xFF,
                           .decay = 0xFE, .sustain = 0xC0, .release = 0xD0};
    M4ADriver* plain = m4a_driver_create((float)M4A_GBA_CYCLES_PER_SECOND);
    M4ADriver* metered = m4a_driver_create((float)M4A_GBA_CYCLES_PER_SECOND);
    CHECK(plain && metered);
    if (!plain || !metered)
        goto done;
    m4a_driver_set_voicegroup(plain, voices);
    m4a_driver_set_voicegroup(metered, voices);
    m4a_driver_cpu_set_enabled(metered, true);
    m4a_driver_cpu_song_start(metered, 1u, M4A_PLAYER_BGM, 10);
    m4a_program_change(plain, 0, 0);
    m4a_program_change(metered, 0, 0);
    m4a_note_on(plain, 0, 60, 100);
    m4a_note_on(metered, 0, 60, 100);
    for (int i = 0; i < 3; ++i)
    {
        m4a_advance(plain, (int)M4A_VBLANK_CYCLES);
        m4a_advance(metered, (int)M4A_VBLANK_CYCLES);
        const M4ARegWriteBatch* a = m4a_get_pending_writes(plain);
        const M4ARegWriteBatch* b = m4a_get_pending_writes(metered);
        CHECK(a->count == b->count);
        for (size_t j = 0; j < a->count && j < b->count; ++j)
        {
            CHECK(a->events[j].cycle == b->events[j].cycle);
            CHECK(a->events[j].reg == b->events[j].reg);
            CHECK(a->events[j].value == b->events[j].value);
            CHECK(a->events[j].order == b->events[j].order);
        }
        const M4APcmRing* x = m4a_get_pcm_ring(plain);
        const M4APcmRing* y = m4a_get_pcm_ring(metered);
        CHECK(x->write_cursor == y->write_cursor);
        CHECK(memcmp(x->ring_a, y->ring_a, sizeof(x->ring_a)) == 0);
        CHECK(memcmp(x->ring_b, y->ring_b, sizeof(x->ring_b)) == 0);
        m4a_consume_writes(plain);
        m4a_consume_writes(metered);
    }
    uint64_t cursor = 0;
    M4AFrameCost costs[4] = {0};
    CHECK(m4a_driver_cpu_read(metered, &cursor, costs, 4) == 3);
    CHECK(costs[0].valid && costs[0].sequencer > 2179);
    const uint8_t channel_status = metered->pcmChans[0].status;
    m4a_driver_cpu_note_off(metered, 0, 60);
    CHECK(metered->pcmChans[0].status == channel_status);
    CHECK(metered->cpu_seq.keyOn[0][60] == UINT32_MAX);
    metered->cpu_audition_event = true;
    m4a_note_on(metered, 0, 62, 100);
    metered->cpu_audition_event = false;
    frame(metered);
    CHECK(m4a_driver_cpu_read(metered, &cursor, costs, 1) == 1);
    CHECK(!costs[0].valid);
    m4a_all_sound_off(metered);
    frame(metered);
    CHECK(m4a_driver_cpu_read(metered, &cursor, costs, 1) == 1);
    CHECK(!costs[0].valid);
    frame(metered);
    CHECK(m4a_driver_cpu_read(metered, &cursor, costs, 1) == 1);
    CHECK(costs[0].valid);

done:
    m4a_driver_destroy(plain);
    m4a_driver_destroy(metered);
}

int main(void)
{
    test_calibrated_player_paths();
    test_timing_and_commands();
    test_loop_path_cost();
    test_chased_notes();
    test_history_and_enable();
    test_unsupported_geometry();
    test_audio_unchanged_and_preview_invalid();
    return failures ? 1 : 0;
}
