#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "voicegroup/voicegroup_loader.h"
#include "m4a/m4a_driver.h"
#include "hw_audio/hw_audio.h"

/*
 * ⚠ Manual artifact / smoke utility — NOT a correctness or parity test.
 *
 * Loads a voicegroup, plays a canned sequence of notes, and writes the
 * resulting audio to a WAV file.  There are no automated assertions
 * here: the binary's job is to produce a listenable artifact for human
 * ears (or for diff against another build).  CI does NOT gate on its
 * output, and parity / spectral / level claims should NOT be derived
 * from this WAV — see HW_AUDIO_SCAFFOLD_PLAN.md §12 for the actual
 * parity gates (steps 9 + 10 + 11).
 *
 * Usage: poryaaaa_test <project_root> <voicegroup_name> [output.wav]
 *
 * Example:
 *   ./poryaaaa_test /path/to/pokeemerald petalburg output.wav
 */

#define SAMPLE_RATE 44100
#define DURATION_SECONDS 8
#define TOTAL_SAMPLES (SAMPLE_RATE * DURATION_SECONDS)

static void render_driver(M4ADriver* drv, HwAudio* hw, float* outL, float* outR, int frames)
{
    for (int offset = 0; offset < frames;)
    {
        int chunk = frames - offset;
        if (chunk > M4A_RECOMMENDED_MAX_ADVANCE_FRAMES)
        {
            chunk = M4A_RECOMMENDED_MAX_ADVANCE_FRAMES;
        }

        m4a_advance(drv, chunk);
        const M4ARegWriteBatch* writes = m4a_get_pending_writes(drv);
        hw_audio_render_events(hw, writes, outL + offset, outR + offset, chunk);
        m4a_consume_writes(drv);
        offset += chunk;
    }
}

/* WAV file writing */
static void write_u16_le(FILE* f, uint16_t val)
{
    uint8_t buf[2] = {val & 0xFF, (val >> 8) & 0xFF};
    fwrite(buf, 1, 2, f);
}

static void write_u32_le(FILE* f, uint32_t val)
{
    uint8_t buf[4] = {val & 0xFF, (val >> 8) & 0xFF, (val >> 16) & 0xFF, (val >> 24) & 0xFF};
    fwrite(buf, 1, 4, f);
}

static int write_wav(const char* path, const float* left, const float* right, int numSamples, int sampleRate)
{
    FILE* f = fopen(path, "wb");
    if (!f)
    {
        fprintf(stderr, "Cannot open %s for writing\n", path);
        return -1;
    }

    uint16_t numChannels = 2;
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
    uint16_t blockAlign = numChannels * bitsPerSample / 8;
    uint32_t dataSize = numSamples * numChannels * bitsPerSample / 8;

    /* RIFF header */
    fwrite("RIFF", 1, 4, f);
    write_u32_le(f, 36 + dataSize);
    fwrite("WAVE", 1, 4, f);

    /* fmt chunk */
    fwrite("fmt ", 1, 4, f);
    write_u32_le(f, 16); /* chunk size */
    write_u16_le(f, 1);  /* PCM format */
    write_u16_le(f, numChannels);
    write_u32_le(f, sampleRate);
    write_u32_le(f, byteRate);
    write_u16_le(f, blockAlign);
    write_u16_le(f, bitsPerSample);

    /* data chunk */
    fwrite("data", 1, 4, f);
    write_u32_le(f, dataSize);

    for (int i = 0; i < numSamples; i++)
    {
        /* Convert float to 16-bit signed */
        int32_t l = (int32_t)(left[i] * 32767.0f);
        int32_t r = (int32_t)(right[i] * 32767.0f);
        if (l > 32767)
            l = 32767;
        if (l < -32768)
            l = -32768;
        if (r > 32767)
            r = 32767;
        if (r < -32768)
            r = -32768;
        write_u16_le(f, (uint16_t)(int16_t)l);
        write_u16_le(f, (uint16_t)(int16_t)r);
    }

    fclose(f);
    return 0;
}

/*
 * Play a multi-program test using various instruments
 */
static void play_multi_program_test(M4ADriver* drv, HwAudio* hw, const char* outputPath)
{
    int totalSamples = SAMPLE_RATE * 12;
    float* outL = calloc(totalSamples, sizeof(float));
    float* outR = calloc(totalSamples, sizeof(float));
    int pos = 0;

    /* Test programs 0-7 with a short melody each */
    uint8_t programs[] = {0, 1, 35, 45, 56, 125, 126, 127};
    for (int i = 0; i < 8 && pos < totalSamples; i++)
    {
        uint8_t prog = programs[i];
        m4a_program_change(drv, 0, prog);
        m4a_cc(drv, 0, 7, 127);
        m4a_cc(drv, 0, 10, 64);

        /* Play 3 notes */
        uint8_t notesInstrument[] = {60, 64, 67};
        uint8_t notesPercussion[] = {38, 39, 40};
        for (int n = 0; n < 3 && pos < totalSamples; n++)
        {
            uint8_t* notes = prog == 0 ? notesPercussion : notesInstrument;
            m4a_note_on(drv, 0, notes[n], 100);

            int len = SAMPLE_RATE / 3;
            if (pos + len > totalSamples)
                len = totalSamples - pos;
            render_driver(drv, hw, outL + pos, outR + pos, len);
            pos += len;

            m4a_note_off(drv, 0, notes[n]);
        }

        /* Small gap */
        int gap = SAMPLE_RATE / 8;
        if (pos + gap > totalSamples)
            gap = totalSamples - pos;
        if (gap > 0)
        {
            render_driver(drv, hw, outL + pos, outR + pos, gap);
            pos += gap;
        }
    }

    /* Fill remaining */
    if (pos < totalSamples)
    {
        render_driver(drv, hw, outL + pos, outR + pos, totalSamples - pos);
    }

    printf("Writing %s...\n", outputPath);
    write_wav(outputPath, outL, outR, totalSamples, SAMPLE_RATE);

    free(outL);
    free(outR);
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <project_root> <voicegroup_name> [output.wav]\n", argv[0]);
        fprintf(stderr, "\nExample:\n");
        fprintf(stderr, "  %s /path/to/pokeemerald petalburg output.wav\n", argv[0]);
        return 1;
    }

    const char* projectRoot = argv[1];
    const char* vgName = argv[2];
    const char* outputPath = argc > 3 ? argv[3] : "output.wav";

    printf("Loading voicegroup '%s' from %s...\n", vgName, projectRoot);
    fflush(stdout);

    LoadedVoiceGroup* vg = voicegroup_load(projectRoot, vgName);
    if (!vg)
    {
        const char* err = voicegroup_loader_last_error();
        if (err && err[0] && strncmp(err, "Bad project root:", strlen("Bad project root:")) == 0)
            fprintf(stderr, "%s\n", err);
        else if (err && err[0])
            fprintf(stderr, "%s. Failed to load voicegroup '%s'\n", err, vgName);
        else
            fprintf(stderr, "Failed to load voicegroup '%s'\n", vgName);
        return 1;
    }

    printf("Voicegroup loaded successfully.\n");

    /* Count loaded voices */
    int dsCount = 0, sq1Count = 0, sq2Count = 0, pwCount = 0, noiseCount = 0;
    int ksCount = 0, ksaCount = 0;
    for (int i = 0; i < VOICEGROUP_SIZE; i++)
    {
        switch (vg->voices[i].type & 0xC7)
        {
        case VOICE_DIRECTSOUND:
            if (vg->voices[i].wav)
                dsCount++;
            break;
        case VOICE_SQUARE_1:
            sq1Count++;
            break;
        case VOICE_SQUARE_2:
            sq2Count++;
            break;
        case VOICE_PROGRAMMABLE_WAVE:
            pwCount++;
            break;
        case VOICE_NOISE:
            noiseCount++;
            break;
        case VOICE_KEYSPLIT:
            ksCount++;
            break;
        case VOICE_KEYSPLIT_ALL:
            ksaCount++;
            break;
        }
    }
    printf("Voices: %d DirectSound, %d Square1, %d Square2, %d ProgWave, %d Noise\n",
           dsCount,
           sq1Count,
           sq2Count,
           pwCount,
           noiseCount);
    printf("        %d Keysplit, %d KeysplitAll\n", ksCount, ksaCount);
    printf("Loaded %d samples, %d prog waves, %d sub-voicegroups\n",
           vg->waveDataCount,
           vg->progWaveCount,
           vg->subGroupCount);

    M4ADriver* drv = m4a_driver_create(SAMPLE_RATE);
    HwAudio* hw = hw_audio_create(SAMPLE_RATE);
    if (!drv || !hw)
    {
        fprintf(stderr, "Failed to initialize direct audio runtime\n");
        if (hw)
            hw_audio_destroy(hw);
        if (drv)
            m4a_driver_destroy(drv);
        voicegroup_free(vg);
        return 1;
    }
    /* The export utility has always rendered with this product configuration:
     * a 15-step m4a master and five DirectSound voices.  Set it on the
     * authoritative driver rather than depending on removed facade defaults. */
    m4a_set_master_volume(drv, 15);
    m4a_set_song_volume(drv, 127);
    m4a_set_reverb_amount(drv, 0);
    m4a_set_analog_filter(drv, false);
    m4a_set_max_pcm_channels(drv, 5);
    m4a_driver_set_voicegroup(drv, vg->voices);

    /* Run test */
    play_multi_program_test(drv, hw, outputPath);

    printf("Done! Output written to %s\n", outputPath);

    /* Cleanup */
    hw_audio_destroy(hw);
    m4a_driver_destroy(drv);
    voicegroup_free(vg);

    return 0;
}
