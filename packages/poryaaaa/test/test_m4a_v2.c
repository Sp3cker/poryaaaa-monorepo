#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "m4a/m4a_driver.h"
#include "m4a/m4a_internal.h"
#include "test_assert.h"

/* Assert the literal driver-to-chip transaction captured for one logical
 * SoundMain action.  Keeping this table-driven prevents tests from deriving
 * an expected value from the encoder under test. */
static void assert_transaction(const M4ARegWriteBatch* batch, const M4ARegWrite* expected, size_t expected_count)
{
    ASSERT(batch != NULL, "lifecycle transaction batch is available");
    if (!batch)
        return;

    ASSERT_EQ(batch->count, expected_count, "lifecycle transaction count");
    for (size_t i = 0; i < batch->count && i < expected_count; i++)
    {
        ASSERT_EQ(batch->events[i].cycle, expected[i].cycle, "lifecycle transaction cycle");
        ASSERT_EQ(batch->events[i].order, expected[i].order, "lifecycle transaction order");
        ASSERT_EQ(batch->events[i].reg, expected[i].reg, "lifecycle transaction register");
        ASSERT_EQ(batch->events[i].value, expected[i].value, "lifecycle transaction value");
    }
}

/* Start one voice through the public MIDI ingress; individual scenarios own
 * the subsequent lifecycle action and observed bus transaction. */
static M4ADriver* start_voice(ToneData* voices)
{
    M4ADriver* driver = m4a_driver_create(44100.0f);
    ASSERT(driver != NULL, "lifecycle driver allocates");
    if (!driver)
        return NULL;

    m4a_driver_set_voicegroup(driver, voices);
    m4a_program_change(driver, 0, 0);
    m4a_cc(driver, 0, 7, 127);
    m4a_cc(driver, 0, 10, 64);
    m4a_note_on(driver, 0, 60, 127);
    return driver;
}

/* The fixed vs_wild Sq1 slot has no evolving sweep, but its NR10 write is a
 * distinct required transaction and must never be conflated with Sq2. */
static void test_m4a_v2_sq1_lifecycle_transactions(void)
{
    printf("Testing v2 Sq1 lifecycle transaction contract...\n");

    ToneData voices[128];
    memset(voices, 0, sizeof(voices));
    voices[0].type = VOICE_SQUARE_1;
    voices[0].key = 60;
    voices[0].wavePointer = (uint32_t*)(uintptr_t)2;
    voices[0].attack = 0;
    voices[0].decay = 0;
    voices[0].sustain = 15;
    voices[0].release = 0;

    M4ADriver* driver = start_voice(voices);
    if (!driver)
        return;

    const M4ARegWrite start_expected[] = {
        {0, M4A_REG_NR10, 0x08u, 0},
        {0, M4A_REG_NR11, 0x80u, 1},
        {0, M4A_REG_NR13, 0x0Bu, 2},
        {0, M4A_REG_NR14, 0x06u, 3},
        {0, M4A_REG_NR51, 0x11u, 4},
        {0, M4A_REG_NR12, 0xF8u, 5},
        {0, M4A_REG_NR14, 0x86u, 6},
    };
    m4a_internal_compat_tick(driver);
    assert_transaction(
        m4a_get_pending_writes(driver), start_expected, sizeof(start_expected) / sizeof(start_expected[0]));
    m4a_consume_writes(driver);
    m4a_internal_compat_tick(driver);
    ASSERT_EQ(m4a_get_pending_writes(driver)->count, 0, "Sq1 steady envelope emits no transaction");
    m4a_consume_writes(driver);

    const M4ARegWrite pitch_expected[] = {
        {0, M4A_REG_NR13, 0x41u, 0},
        {0, M4A_REG_NR14, 0x06u, 1},
    };
    m4a_pitch_bend(driver, 0, 8192);
    m4a_internal_compat_tick(driver);
    assert_transaction(
        m4a_get_pending_writes(driver), pitch_expected, sizeof(pitch_expected) / sizeof(pitch_expected[0]));
    m4a_consume_writes(driver);

    const M4ARegWrite volume_pan_expected[] = {
        {0, M4A_REG_NR51, 0x11u, 0},
        {0, M4A_REG_NR12, 0xF8u, 1},
        {0, M4A_REG_NR14, 0x86u, 2},
    };
    m4a_cc(driver, 0, 10, 0);
    m4a_internal_compat_tick(driver);
    assert_transaction(m4a_get_pending_writes(driver),
                       volume_pan_expected,
                       sizeof(volume_pan_expected) / sizeof(volume_pan_expected[0]));
    m4a_consume_writes(driver);

    const M4ARegWrite retrigger_expected[] = {
        {0, M4A_REG_NR10, 0x08u, 0},
        {0, M4A_REG_NR11, 0x80u, 1},
        {0, M4A_REG_NR13, 0x41u, 2},
        {0, M4A_REG_NR14, 0x06u, 3},
        {0, M4A_REG_NR51, 0x10u, 4},
        {0, M4A_REG_NR12, 0xF8u, 5},
        {0, M4A_REG_NR14, 0x86u, 6},
    };
    m4a_note_on(driver, 0, 60, 127);
    m4a_internal_compat_tick(driver);
    assert_transaction(
        m4a_get_pending_writes(driver), retrigger_expected, sizeof(retrigger_expected) / sizeof(retrigger_expected[0]));
    m4a_consume_writes(driver);

    const M4ARegWrite release_expected[] = {
        {0, M4A_REG_NR12, 0x08u, 0},
        {0, M4A_REG_NR14, 0x80u, 1},
    };
    m4a_note_off(driver, 0, 60);
    m4a_internal_compat_tick(driver);
    assert_transaction(
        m4a_get_pending_writes(driver), release_expected, sizeof(release_expected) / sizeof(release_expected[0]));

    m4a_driver_destroy(driver);
}

/* The fixed vs_wild Sq2 slot has a real hardware-envelope transition; its
 * transaction contract deliberately contains no NR10 sweep write. */
static void test_m4a_v2_sq2_lifecycle_transactions(void)
{
    printf("Testing v2 Sq2 lifecycle transaction contract...\n");

    ToneData voices[128];
    memset(voices, 0, sizeof(voices));
    voices[0].type = VOICE_SQUARE_2;
    voices[0].key = 60;
    voices[0].wavePointer = (uint32_t*)(uintptr_t)2;
    voices[0].attack = 0;
    voices[0].decay = 1;
    voices[0].sustain = 1;
    voices[0].release = 1;

    M4ADriver* driver = start_voice(voices);
    if (!driver)
        return;

    const M4ARegWrite start_expected[] = {
        {0, M4A_REG_NR21, 0x80u, 0},
        {0, M4A_REG_NR23, 0x0Bu, 1},
        {0, M4A_REG_NR24, 0x06u, 2},
        {0, M4A_REG_NR51, 0x22u, 3},
        {0, M4A_REG_NR22, 0xF1u, 4},
        {0, M4A_REG_NR24, 0x86u, 5},
    };
    m4a_internal_compat_tick(driver);
    assert_transaction(
        m4a_get_pending_writes(driver), start_expected, sizeof(start_expected) / sizeof(start_expected[0]));
    m4a_consume_writes(driver);

    const M4ARegWrite pitch_expected[] = {
        {0, M4A_REG_NR23, 0x41u, 0},
        {0, M4A_REG_NR24, 0x06u, 1},
    };
    m4a_pitch_bend(driver, 0, 8192);
    m4a_internal_compat_tick(driver);
    assert_transaction(
        m4a_get_pending_writes(driver), pitch_expected, sizeof(pitch_expected) / sizeof(pitch_expected[0]));
    m4a_driver_destroy(driver);

    driver = start_voice(voices);
    if (!driver)
        return;
    m4a_internal_compat_tick(driver);
    m4a_consume_writes(driver);
    for (int i = 0; i < 13; i++)
    {
        m4a_internal_compat_tick(driver);
        ASSERT_EQ(m4a_get_pending_writes(driver)->count, 0, "Sq2 decay emits nothing before sustain transition");
        m4a_consume_writes(driver);
    }
    const M4ARegWrite envelope_expected[] = {
        {0, M4A_REG_NR51, 0x22u, 0},
        {0, M4A_REG_NR22, 0x18u, 1},
        {0, M4A_REG_NR24, 0x86u, 2},
    };
    m4a_internal_compat_tick(driver);
    assert_transaction(
        m4a_get_pending_writes(driver), envelope_expected, sizeof(envelope_expected) / sizeof(envelope_expected[0]));
    m4a_driver_destroy(driver);

    driver = start_voice(voices);
    if (!driver)
        return;
    m4a_internal_compat_tick(driver);
    m4a_consume_writes(driver);
    const M4ARegWrite volume_pan_expected[] = {
        {0, M4A_REG_NR51, 0x20u, 0},
        {0, M4A_REG_NR22, 0xE1u, 1},
        {0, M4A_REG_NR24, 0x86u, 2},
    };
    m4a_cc(driver, 0, 10, 0);
    m4a_internal_compat_tick(driver);
    assert_transaction(m4a_get_pending_writes(driver),
                       volume_pan_expected,
                       sizeof(volume_pan_expected) / sizeof(volume_pan_expected[0]));
    m4a_driver_destroy(driver);

    driver = start_voice(voices);
    if (!driver)
        return;
    m4a_internal_compat_tick(driver);
    m4a_consume_writes(driver);
    m4a_note_on(driver, 0, 60, 127);
    m4a_internal_compat_tick(driver);
    assert_transaction(
        m4a_get_pending_writes(driver), start_expected, sizeof(start_expected) / sizeof(start_expected[0]));
    m4a_consume_writes(driver);

    const M4ARegWrite release_expected[] = {
        {0, M4A_REG_NR51, 0x22u, 0},
        {0, M4A_REG_NR22, 0xF1u, 1},
        {0, M4A_REG_NR24, 0x86u, 2},
    };
    m4a_note_off(driver, 0, 60);
    m4a_internal_compat_tick(driver);
    assert_transaction(
        m4a_get_pending_writes(driver), release_expected, sizeof(release_expected) / sizeof(release_expected[0]));
    m4a_consume_writes(driver);
    for (int i = 0; i < 14; i++)
    {
        m4a_internal_compat_tick(driver);
        ASSERT_EQ(m4a_get_pending_writes(driver)->count, 0, "Sq2 release emits nothing before disable");
        m4a_consume_writes(driver);
    }
    const M4ARegWrite disable_expected[] = {
        {0, M4A_REG_NR22, 0x08u, 0},
        {0, M4A_REG_NR24, 0x80u, 1},
    };
    m4a_internal_compat_tick(driver);
    assert_transaction(
        m4a_get_pending_writes(driver), disable_expected, sizeof(disable_expected) / sizeof(disable_expected[0]));

    m4a_driver_destroy(driver);
}

typedef struct
{
    uint32_t fifo_a[4];
    uint32_t fifo_b[4];
} DirectSoundRefillExpected;

/* An empty DirectSound FIFO is refilled at the exact MP2K timer period, but
 * mGBA does not consume the newly transferred word until the next overflow. */
static void assert_first_directsound_refill(M4ADriver* driver, const DirectSoundRefillExpected* expected)
{
    const M4ARegWrite refill_expected[] = {
        {1254, M4A_REG_FIFO_A, expected->fifo_a[0], 0},
        {1254, M4A_REG_FIFO_A, expected->fifo_a[1], 1},
        {1254, M4A_REG_FIFO_A, expected->fifo_a[2], 2},
        {1254, M4A_REG_FIFO_A, expected->fifo_a[3], 3},
        {1254, M4A_REG_FIFO_B, expected->fifo_b[0], 4},
        {1254, M4A_REG_FIFO_B, expected->fifo_b[1], 5},
        {1254, M4A_REG_FIFO_B, expected->fifo_b[2], 6},
        {1254, M4A_REG_FIFO_B, expected->fifo_b[3], 7},
    };

    m4a_advance(driver, 4);
    assert_transaction(m4a_get_pending_writes(driver), refill_expected, sizeof(refill_expected) / sizeof(refill_expected[0]));
    m4a_consume_writes(driver);

    const M4ARegWrite consume_expected[] = {
        {2508, M4A_REG_TIMER_0, 0, 0},
    };
    m4a_advance(driver, 4);
    assert_transaction(
        m4a_get_pending_writes(driver), consume_expected, sizeof(consume_expected) / sizeof(consume_expected[0]));
}

/* DirectSound has no PSG configuration writes: its observable lifecycle is
 * the exact sampled FIFO stream, paired routing, and timer ordering. */
static void test_m4a_v2_directsound_lifecycle_transactions(void)
{
    printf("Testing v2 DirectSound lifecycle transaction contract...\n");

    int8_t data[512];
    memset(data, 127, sizeof(data));
    WaveData wave = {.freq = 0x00800000u, .size = sizeof(data), .data = data};
    ToneData voices[128];
    memset(voices, 0, sizeof(voices));
    voices[0].type = VOICE_DIRECTSOUND;
    voices[0].key = 60;
    voices[0].wav = &wave;
    voices[0].attack = 255;
    voices[0].decay = 255;
    voices[0].sustain = 255;
    voices[0].release = 0;
    const DirectSoundRefillExpected start_expected = {
        .fifo_a = {0x32311E00u, 0x31323132u, 0x32313232u, 0x31323231u},
        .fifo_b = {0x32311E00u, 0x31323132u, 0x32313232u, 0x31323231u},
    };
    const DirectSoundRefillExpected pitch_expected = {
        .fifo_a = {0x32322100u, 0x31323231u, 0x32323132u, 0x32313231u},
        .fifo_b = {0x32322100u, 0x31323231u, 0x32323132u, 0x32313231u},
    };
    const DirectSoundRefillExpected attack_expected = {
        .fifo_a = {0x0C0C0700u, 0x0C0C0C0Bu, 0x0C0C0C0Cu, 0x0B0C0C0Cu},
        .fifo_b = {0x0C0C0700u, 0x0C0C0C0Bu, 0x0C0C0C0Cu, 0x0B0C0C0Cu},
    };
    const DirectSoundRefillExpected pan_expected = {
        .fifo_a = {0, 0, 0, 0},
        .fifo_b = {0x63633C00u, 0x63636364u, 0x63636364u, 0x63636463u},
    };
    const DirectSoundRefillExpected release_expected = {0};

    M4ADriver* driver = start_voice(voices);
    if (!driver)
        return;
    m4a_internal_compat_tick(driver);
    /* All 16 FIFO bytes lock the ROM mixer's interpolation and discarded-bit
     * carry, rather than assuming one startup word repeats across the burst. */
    assert_first_directsound_refill(driver, &start_expected);
    m4a_driver_destroy(driver);

    driver = start_voice(voices);
    if (!driver)
        return;
    m4a_pitch_bend(driver, 0, 8192);
    m4a_internal_compat_tick(driver);
    /* Pitch changes the second interpolated source byte without changing
     * FIFO width, order, timer identity, or stereo routing. */
    assert_first_directsound_refill(driver, &pitch_expected);
    m4a_driver_destroy(driver);

    voices[0].attack = 64;
    driver = start_voice(voices);
    if (!driver)
        return;
    m4a_internal_compat_tick(driver);
    assert_first_directsound_refill(driver, &attack_expected);
    m4a_driver_destroy(driver);

    voices[0].attack = 255;
    driver = start_voice(voices);
    if (!driver)
        return;
    m4a_cc(driver, 0, 10, 0);
    m4a_internal_compat_tick(driver);
    assert_first_directsound_refill(driver, &pan_expected);
    m4a_driver_destroy(driver);

    driver = start_voice(voices);
    if (!driver)
        return;
    m4a_note_off(driver, 0, 60);
    m4a_note_on(driver, 0, 60, 127);
    m4a_internal_compat_tick(driver);
    assert_first_directsound_refill(driver, &start_expected);
    m4a_driver_destroy(driver);

    driver = start_voice(voices);
    if (!driver)
        return;
    m4a_note_off(driver, 0, 60);
    m4a_internal_compat_tick(driver);
    assert_first_directsound_refill(driver, &release_expected);
    m4a_driver_destroy(driver);
}

/* Keep lifecycle regressions separately callable from the monolithic engine
 * suite so their full-ROM bus contracts remain easy to locate. */
void test_m4a_v2_run_all(void)
{
    test_m4a_v2_sq1_lifecycle_transactions();
    test_m4a_v2_sq2_lifecycle_transactions();
    test_m4a_v2_directsound_lifecycle_transactions();
}
