#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m4a_engine.h"
#include "hw_audio/hw_audio.h"
#include "m4a/m4a_driver.h"
#include "test_assert.h"

static void configure_square_2_engine(M4AEngine* engine, ToneData* voices)
{
    m4a_engine_set_voicegroup(engine, voices);
    m4a_engine_program_change(engine, 0, 0);
    m4a_engine_cc(engine, 0, 7, 127);
    m4a_engine_cc(engine, 0, 10, 64);
    m4a_engine_note_on(engine, 0, 60, 127);
}

static void configure_square_2_driver(M4ADriver* driver, ToneData* voices)
{
    m4a_driver_set_voicegroup(driver, voices);
    m4a_program_change(driver, 0, 0);
    m4a_cc(driver, 0, 7, 127);
    m4a_cc(driver, 0, 10, 64);
    m4a_note_on(driver, 0, 60, 127);
}

static void
render_pending_writes(M4ADriver* driver, HwAudio* hw, float* left, float* right, const int* partitions, size_t count)
{
    int offset = 0;
    for (size_t i = 0; i < count; i++)
    {
        m4a_advance(driver, partitions[i]);
        hw_audio_render_events(hw, m4a_get_pending_writes(driver), left + offset, right + offset, partitions[i]);
        m4a_consume_writes(driver);
        offset += partitions[i];
    }
}

static bool has_nonzero_stereo(const float* left, const float* right, size_t frames)
{
    for (size_t i = 0; i < frames; i++)
    {
        if (left[i] != 0.0f || right[i] != 0.0f)
            return true;
    }
    return false;
}

void test_porydaw_engine_run_all(void)
{
    printf("Testing porydaw engine shares the exact frontend facade...\n");
    enum
    {
        SAMPLE_RATE = 48000,
        FRAME_COUNT = 8192,
    };
    const int partitions[] = {2048, 2048, 2048, 2048};
    ToneData voices[128];
    memset(voices, 0, sizeof(voices));
    voices[0].type = VOICE_SQUARE_2;
    voices[0].key = 60;
    voices[0].attack = 0;
    voices[0].decay = 0;
    voices[0].sustain = 16;
    voices[0].release = 16;
    voices[0].wavePointer = (uint32_t*)(uintptr_t)3;
    float* engine_first_l = calloc(FRAME_COUNT, sizeof(*engine_first_l));
    float* engine_first_r = calloc(FRAME_COUNT, sizeof(*engine_first_r));
    float* facade_first_l = calloc(FRAME_COUNT, sizeof(*facade_first_l));
    float* facade_first_r = calloc(FRAME_COUNT, sizeof(*facade_first_r));
    float* engine_second_l = calloc(FRAME_COUNT, sizeof(*engine_second_l));
    float* engine_second_r = calloc(FRAME_COUNT, sizeof(*engine_second_r));
    float* facade_second_l = calloc(FRAME_COUNT, sizeof(*facade_second_l));
    float* facade_second_r = calloc(FRAME_COUNT, sizeof(*facade_second_r));
    M4AEngine engine;
    M4ADriver* driver = NULL;
    HwAudio* hw = NULL;
    bool engine_initialized = false;
    ASSERT(engine_first_l && engine_first_r && facade_first_l && facade_first_r && engine_second_l && engine_second_r &&
               facade_second_l && facade_second_r,
           "porydaw frontend output buffers allocate");
    if (engine_first_l && engine_first_r && facade_first_l && facade_first_r && engine_second_l && engine_second_r &&
        facade_second_l && facade_second_r)
    {
        engine_initialized = m4a_engine_init(&engine, SAMPLE_RATE);
        driver = m4a_driver_create(SAMPLE_RATE);
        hw = hw_audio_create(SAMPLE_RATE);
        ASSERT(engine_initialized && driver && hw, "porydaw shared frontend fixtures initialize");
        if (engine_initialized && driver && hw)
        {
            configure_square_2_engine(&engine, voices);
            configure_square_2_driver(driver, voices);
            /* One facade call is intentionally larger than the bounded
             * driver/render transaction; the facade must preserve the same
             * chronological frontend stream as the bounded reference calls. */
            m4a_engine_process(&engine, engine_first_l, engine_first_r, FRAME_COUNT);
            render_pending_writes(
                driver, hw, facade_first_l, facade_first_r, partitions, sizeof(partitions) / sizeof(partitions[0]));
            ASSERT(has_nonzero_stereo(engine_first_l, engine_first_r, FRAME_COUNT), "square-2 fixture is non-silent");
            ASSERT(memcmp(engine_first_l, facade_first_l, FRAME_COUNT * sizeof(*engine_first_l)) == 0,
                   "engine left output equals the shared frontend facade");
            ASSERT(memcmp(engine_first_r, facade_first_r, FRAME_COUNT * sizeof(*engine_first_r)) == 0,
                   "engine right output equals the shared frontend facade");
            const bool reset = m4a_engine_reset(&engine);
            ASSERT(reset, "porydaw engine resets");
            if (reset)
            {
                m4a_driver_destroy(driver);
                hw_audio_destroy(hw);
                driver = m4a_driver_create(SAMPLE_RATE);
                hw = hw_audio_create(SAMPLE_RATE);
                ASSERT(driver && hw, "canonical frontend fixtures reset");
                if (driver && hw)
                {
                    configure_square_2_engine(&engine, voices);
                    configure_square_2_driver(driver, voices);
                    for (size_t i = 0, offset = 0; i < sizeof(partitions) / sizeof(partitions[0]); i++)
                    {
                        m4a_engine_process(&engine, engine_second_l + offset, engine_second_r + offset, partitions[i]);
                        offset += partitions[i];
                    }
                    render_pending_writes(driver,
                                          hw,
                                          facade_second_l,
                                          facade_second_r,
                                          partitions,
                                          sizeof(partitions) / sizeof(partitions[0]));
                    ASSERT(memcmp(engine_second_l, facade_second_l, FRAME_COUNT * sizeof(*engine_second_l)) == 0,
                           "reset engine left output equals the shared frontend facade");
                    ASSERT(memcmp(engine_second_r, facade_second_r, FRAME_COUNT * sizeof(*engine_second_r)) == 0,
                           "reset engine right output equals the shared frontend facade");
                    ASSERT(memcmp(engine_first_l, engine_second_l, FRAME_COUNT * sizeof(*engine_first_l)) == 0,
                           "engine reset reproduces the first left stream");
                    ASSERT(memcmp(engine_first_r, engine_second_r, FRAME_COUNT * sizeof(*engine_first_r)) == 0,
                           "engine reset reproduces the first right stream");
                }
            }
        }
    }
    hw_audio_destroy(hw);
    m4a_driver_destroy(driver);
    if (engine_initialized)
        m4a_engine_destroy(&engine);
    free(facade_second_r);
    free(facade_second_l);
    free(engine_second_r);
    free(engine_second_l);
    free(facade_first_r);
    free(facade_first_l);
    free(engine_first_r);
    free(engine_first_l);
}
