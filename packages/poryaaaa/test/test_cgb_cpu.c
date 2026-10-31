#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "m4a/m4a_internal.h"
#include "test_assert.h"

extern void m4a_cgb_sound(M4ADriver* drv);
extern void m4a_drv_cgb_start(M4ADriverCgbChan* ch);
extern void m4a_chn_vol_set_cgb(M4ADriverCgbChan* ch, M4ADriverTrack* track);

int tests_run = 0;
int tests_passed = 0;

static uint32_t test_wave[4] = {0x01234567u, 0x89abcdefu, 0xfedcba98u, 0x76543210u};

/* Prepare one channel with a non-instantaneous attack and centered pan. */
static M4ADriverCgbChan* prepare_channel(M4ADriver* drv, int index)
{
    M4ADriverCgbChan* ch = &drv->cgb[index];
    ch->trackIndex = 0;
    ch->rightVolume = 64;
    ch->leftVolume = 64;
    ch->attack = 2;
    ch->decay = 3;
    ch->sustain = 8;
    ch->release = 2;
    ch->sweep = 0x08;
    ch->frequency = 0x345;
    ch->wavePointer = test_wave;
    m4a_chn_vol_set_cgb(ch, &drv->tracks[0]);
    return ch;
}

/* Cost one direct CgbSound pass with a known post-decrement c15 phase. */
static int64_t cost_pass(M4ADriver* drv, uint8_t c15)
{
    drv->cpu_cost_enabled = true;
    drv->cpu_cgb_cycles = 0;
    drv->c15 = c15;
    m4a_cgb_sound(drv);
    return drv->cpu_cgb_cycles;
}

static void test_inactive_and_entry(void)
{
    M4ADriver* drv = m4a_driver_create(44100.0f);
    ASSERT(drv != NULL, "inactive CGB driver allocates");
    if (!drv)
        return;

    int64_t normal = cost_pass(drv, 13);
    int64_t reload = cost_pass(drv, 14);
    ASSERT_EQ(normal, 229, "CgbSound entry plus four inactive channels");
    ASSERT_EQ(reload, 227, "c15 reload uses the measured shorter entry");
    m4a_driver_destroy(drv);
}

static void test_start_paths(void)
{
    const int expected[] = {766, 732, 857, 746};
    const int stopped[] = {425, 409, 431, 425};
    for (int i = 0; i < M4A_MAX_CGB_CHANNELS; ++i)
    {
        M4ADriver* drv = m4a_driver_create(44100.0f);
        ASSERT(drv != NULL, "CGB start driver allocates");
        if (!drv)
            continue;
        M4ADriverCgbChan* ch = prepare_channel(drv, i);
        m4a_drv_cgb_start(ch);
        int64_t actual = cost_pass(drv, 13);
        ASSERT_EQ(actual, expected[i], "each CGB start uses its calibrated register path");
        if (i == 2)
        {
            m4a_drv_cgb_start(ch);
            actual = cost_pass(drv, 13);
            ASSERT_EQ(actual, 786, "cached wave start skips the measured RAM upload");
        }
        m4a_drv_cgb_start(ch);
        ch->status |= M4A_CHN_STOP;
        actual = cost_pass(drv, 13);
        ASSERT_EQ(actual, stopped[i], "start and stop uses the channel-specific oscillator-off path");
        m4a_driver_destroy(drv);
    }

    M4ADriver* drv = m4a_driver_create(44100.0f);
    ASSERT(drv != NULL, "instantaneous CGB start driver allocates");
    if (!drv)
        return;
    M4ADriverCgbChan* ch = prepare_channel(drv, 1);
    ch->attack = 0;
    m4a_drv_cgb_start(ch);
    int64_t actual = cost_pass(drv, 13);
    ASSERT_EQ(actual, 772, "zero attack falls through to decay");
    ch->decay = 0;
    m4a_drv_cgb_start(ch);
    actual = cost_pass(drv, 13);
    ASSERT_EQ(actual, 808, "zero attack and decay fall through to sustain");
    ch->sustain = 0;
    ch->pseudoEchoVolume = 0;
    m4a_drv_cgb_start(ch);
    actual = cost_pass(drv, 13);
    ASSERT_EQ(actual, 810, "zero sustain without echo starts then stops the oscillator");
    ch->pseudoEchoVolume = 64;
    m4a_drv_cgb_start(ch);
    actual = cost_pass(drv, 13);
    ASSERT_EQ(actual, 904, "zero sustain with echo takes the measured echo path");
    m4a_driver_destroy(drv);
}

static void test_pan_and_frequency(void)
{
    const uint8_t right[] = {64, 80, 64, 32};
    const uint8_t left[] = {64, 32, 80, 80};
    const int expected[] = {732, 736, 738, 742};
    for (int i = 0; i < 4; ++i)
    {
        M4ADriver* drv = m4a_driver_create(44100.0f);
        ASSERT(drv != NULL, "CGB pan driver allocates");
        if (!drv)
            continue;
        M4ADriverCgbChan* ch = prepare_channel(drv, 1);
        ch->rightVolume = right[i];
        ch->leftVolume = left[i];
        m4a_chn_vol_set_cgb(ch, &drv->tracks[0]);
        m4a_drv_cgb_start(ch);
        int64_t actual = cost_pass(drv, 13);
        ASSERT_EQ(actual, expected[i], "CgbPan's four paths have distinct measured charges");
        m4a_driver_destroy(drv);
    }

    M4ADriver* drv = m4a_driver_create(44100.0f);
    ASSERT(drv != NULL, "fixed-frequency driver allocates");
    if (!drv)
        return;
    M4ADriverCgbChan* ch = prepare_channel(drv, 1);
    ch->voiceType = VOICE_TYPE_FIX;
    m4a_drv_cgb_start(ch);
    int64_t actual = cost_pass(drv, 13);
    ASSERT_EQ(actual, 772, "fixed-frequency pitch path adds its measured charge");
    m4a_driver_destroy(drv);

    drv = m4a_driver_create(44100.0f);
    ASSERT(drv != NULL, "fixed-frequency noise driver allocates");
    if (!drv)
        return;
    ch = prepare_channel(drv, 3);
    ch->voiceType = VOICE_TYPE_FIX;
    m4a_drv_cgb_start(ch);
    actual = cost_pass(drv, 13);
    ASSERT_EQ(actual, 746, "noise does not use the square and wave fixed-frequency path");
    m4a_driver_destroy(drv);
}

static void test_output_paths(void)
{
    const uint8_t modify[] = {0, M4A_MO_PIT, M4A_MO_VOL, M4A_MO_PIT | M4A_MO_VOL};
    const int expected[][4] = {
        {430, 486, 507, 563},
        {422, 478, 487, 543},
        {436, 492, 503, 559},
        {430, 482, 495, 547},
    };
    for (int channel = 0; channel < M4A_MAX_CGB_CHANNELS; ++channel)
    {
        for (int i = 0; i < 4; ++i)
        {
            M4ADriver* drv = m4a_driver_create(44100.0f);
            ASSERT(drv != NULL, "CGB output-path driver allocates");
            if (!drv)
                continue;
            M4ADriverCgbChan* ch = prepare_channel(drv, channel);
            drv->regs.sq1_sweep_dir = -1;
            ch->status = M4A_CHN_ENV_SUSTAIN;
            ch->envelopeCounter = 1;
            ch->modify = modify[i];
            int64_t actual = cost_pass(drv, 13);
            ASSERT_EQ(actual, expected[channel][i], "pitch and volume flags select each channel's register paths");
            m4a_driver_destroy(drv);
        }
    }

    M4ADriver* drv = m4a_driver_create(44100.0f);
    ASSERT(drv != NULL, "square sweep-path driver allocates");
    if (!drv)
        return;
    M4ADriverCgbChan* ch = prepare_channel(drv, 0);
    ch->sweep = 0;
    m4a_drv_cgb_start(ch);
    int64_t actual = cost_pass(drv, 13);
    ASSERT_EQ(actual, 768, "positive sweep selects the upstream second NR14 write path");
    m4a_driver_destroy(drv);
}

static void test_envelope_paths(void)
{
    struct EnvelopeCase
    {
        uint8_t status;
        uint8_t volume;
        uint8_t decay;
        uint8_t sustain;
        uint8_t echo;
        int expected;
    };
    const struct EnvelopeCase cases[] = {
        {M4A_CHN_ENV_ATTACK, 1, 3, 8, 0, 611},
        {M4A_CHN_ENV_DECAY, 8, 3, 8, 0, 617},
        {M4A_CHN_ENV_SUSTAIN, 4, 3, 8, 0, 585},
        {M4A_CHN_STOP | M4A_CHN_ENV_RELEASE, 6, 3, 8, 0, 601},
        {M4A_CHN_ENV_ATTACK, 7, 3, 8, 0, 714},
        {M4A_CHN_ENV_DECAY, 5, 3, 8, 0, 722},
        {M4A_CHN_ENV_ATTACK, 7, 0, 8, 0, 732},
        {M4A_CHN_ENV_DECAY, 1, 3, 0, 0, 678},
        {M4A_CHN_ENV_ATTACK, 7, 0, 0, 64, 732},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        M4ADriver* drv = m4a_driver_create(44100.0f);
        ASSERT(drv != NULL, "CGB envelope driver allocates");
        if (!drv)
            continue;
        M4ADriverCgbChan* ch = prepare_channel(drv, 1);
        ch->status = cases[i].status;
        ch->envelopeVolume = cases[i].volume;
        ch->envelopeCounter = 0;
        ch->decay = cases[i].decay;
        ch->sustain = cases[i].sustain;
        ch->pseudoEchoVolume = cases[i].echo;
        int64_t actual = cost_pass(drv, 13);
        ASSERT_EQ(actual, cases[i].expected, "envelope phases and transitions use measured paths");
        m4a_driver_destroy(drv);
    }

    M4ADriver* drv = m4a_driver_create(44100.0f);
    ASSERT(drv != NULL, "CGB double-step driver allocates");
    if (!drv)
        return;
    M4ADriverCgbChan* ch = prepare_channel(drv, 1);
    ch->status = M4A_CHN_ENV_SUSTAIN;
    ch->envelopeCounter = 1;
    int64_t actual = cost_pass(drv, 0);
    ASSERT_EQ(actual, 621, "c15 double-step charges both executed envelope passes");
    m4a_driver_destroy(drv);

    drv = m4a_driver_create(44100.0f);
    ASSERT(drv != NULL, "fresh-note double-step driver allocates");
    if (!drv)
        return;
    ch = prepare_channel(drv, 1);
    ch->attack = 1;
    m4a_drv_cgb_start(ch);
    actual = cost_pass(drv, 0);
    ASSERT_EQ(actual, 957, "fresh note charges its second envelope step only once");
    m4a_driver_destroy(drv);

    drv = m4a_driver_create(44100.0f);
    ASSERT(drv != NULL, "wave envelope driver allocates");
    if (!drv)
        return;
    ch = prepare_channel(drv, 2);
    ch->status = M4A_CHN_ENV_DECAY;
    ch->envelopeVolume = 5;
    ch->envelopeCounter = 0;
    actual = cost_pass(drv, 13);
    ASSERT_EQ(actual, 734, "wave decay-to-sustain uses its measured shorter phase path");
    m4a_driver_destroy(drv);
}

static void test_release_and_echo(void)
{
    struct ReleaseCase
    {
        uint8_t status;
        uint8_t release;
        uint8_t echo;
        uint8_t echo_length;
        int expected;
    };
    const struct ReleaseCase cases[] = {
        {M4A_CHN_STOP | M4A_CHN_ENV_SUSTAIN, 2, 0, 0, 521},
        {M4A_CHN_STOP | M4A_CHN_ENV_SUSTAIN, 0, 0, 0, 495},
        {M4A_CHN_STOP | M4A_CHN_ENV_SUSTAIN, 0, 64, 0, 533},
        {M4A_CHN_IEC, 2, 64, 1, 441},
        {M4A_CHN_IEC, 2, 64, 2, 396},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        M4ADriver* drv = m4a_driver_create(44100.0f);
        ASSERT(drv != NULL, "CGB release driver allocates");
        if (!drv)
            continue;
        M4ADriverCgbChan* ch = prepare_channel(drv, 1);
        ch->status = cases[i].status;
        ch->envelopeVolume = 5;
        ch->release = cases[i].release;
        ch->pseudoEchoVolume = cases[i].echo;
        ch->pseudoEchoLength = cases[i].echo_length;
        int64_t actual = cost_pass(drv, 13);
        ASSERT_EQ(actual, cases[i].expected, "release and pseudo-echo paths charge actual branches");
        m4a_driver_destroy(drv);
    }
}

/* Instrumentation must not change channel state, register state, or bus events. */
static void assert_same_output(const M4ADriver* enabled, const M4ADriver* disabled)
{
    ASSERT(memcmp(&enabled->regs, &disabled->regs, sizeof(enabled->regs)) == 0,
           "telemetry preserves the register file");
    ASSERT(memcmp(enabled->cgb, disabled->cgb, sizeof(enabled->cgb)) == 0,
           "telemetry preserves all CGB channel state");
    ASSERT_EQ(enabled->event_count, disabled->event_count, "telemetry preserves event count");
    for (size_t i = 0; i < enabled->event_count && i < disabled->event_count; ++i)
    {
        ASSERT_EQ(enabled->events[i].cycle, disabled->events[i].cycle, "telemetry preserves event cycle");
        ASSERT_EQ(enabled->events[i].order, disabled->events[i].order, "telemetry preserves event order");
        ASSERT_EQ(enabled->events[i].reg, disabled->events[i].reg, "telemetry preserves event register");
        ASSERT_EQ(enabled->events[i].value, disabled->events[i].value, "telemetry preserves event value");
    }
}

static void test_disabled_transparency(void)
{
    M4ADriver* enabled = m4a_driver_create(44100.0f);
    M4ADriver* disabled = m4a_driver_create(44100.0f);
    ASSERT(enabled != NULL && disabled != NULL, "telemetry comparison drivers allocate");
    if (!enabled || !disabled)
    {
        m4a_driver_destroy(enabled);
        m4a_driver_destroy(disabled);
        return;
    }

    for (int i = 0; i < M4A_MAX_CGB_CHANNELS; ++i)
    {
        m4a_drv_cgb_start(prepare_channel(enabled, i));
        m4a_drv_cgb_start(prepare_channel(disabled, i));
    }
    enabled->cpu_cost_enabled = true;
    disabled->cpu_cost_enabled = false;
    disabled->cpu_cgb_cycles = 123;
    enabled->c15 = disabled->c15 = 13;
    m4a_cgb_sound(enabled);
    m4a_cgb_sound(disabled);
    ASSERT(enabled->cpu_cgb_cycles > 0, "enabled telemetry accumulates cost");
    ASSERT_EQ(disabled->cpu_cgb_cycles, 123, "disabled telemetry does not write its accumulator");
    assert_same_output(enabled, disabled);

    enabled->cgb[0].status |= M4A_CHN_STOP;
    disabled->cgb[0].status |= M4A_CHN_STOP;
    enabled->cgb[0].release = disabled->cgb[0].release = 0;
    enabled->cgb[1].status |= M4A_CHN_STOP;
    disabled->cgb[1].status |= M4A_CHN_STOP;
    enabled->cgb[2].modify |= M4A_MO_PIT | M4A_MO_VOL;
    disabled->cgb[2].modify |= M4A_MO_PIT | M4A_MO_VOL;
    enabled->c15 = disabled->c15 = 0;
    m4a_cgb_sound(enabled);
    m4a_cgb_sound(disabled);
    ASSERT_EQ(disabled->cpu_cgb_cycles, 123, "disabled telemetry remains untouched across frames");
    assert_same_output(enabled, disabled);
    m4a_driver_destroy(enabled);
    m4a_driver_destroy(disabled);
}

int main(void)
{
    test_inactive_and_entry();
    test_start_paths();
    test_pan_and_frequency();
    test_output_paths();
    test_envelope_paths();
    test_release_and_echo();
    test_disabled_transparency();
    printf("CGB CPU tests: %d/%d passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
