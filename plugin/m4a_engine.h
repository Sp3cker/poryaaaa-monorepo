#ifndef M4A_ENGINE_H
#define M4A_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "voicegroup/voicegroup_types.h"

#define MAX_PCM_CHANNELS 12
#define MAX_CGB_CHANNELS 4
#define MAX_TRACKS 16
#define VBLANK_RATE 59.7275f
#define MAX_SONG_VOLUME 127
#define M4A_ENGINE_MAX_PROCESS_FRAMES 2048

typedef struct M4ADriver M4ADriver;
typedef struct HwAudio HwAudio;

typedef void (*M4AEngineXcmdFn)(void *ctx, int trackIndex, uint8_t selector, uint32_t value);

typedef struct M4AEngine M4AEngine;

struct M4AEngine {
	ToneData *voiceGroup;
	M4ADriver *driver;
	HwAudio *hw;
	float sampleRate;
	uint8_t volume;
	uint8_t reverbAmount;
	M4AEngineXcmdFn xcmd_fn;
	void *xcmd_ctx;
};

/* Engine lifecycle */
bool m4a_engine_init(M4AEngine *engine, float sampleRate);
void m4a_engine_destroy(M4AEngine *engine);
bool m4a_engine_reset(M4AEngine *engine);
void m4a_engine_set_xcmd_callback(M4AEngine *engine, M4AEngineXcmdFn xcmd_fn, void *xcmd_ctx);

/* Set voicegroup (must be loaded by voicegroup_loader; NULL is invalid). */
void m4a_engine_set_voicegroup(M4AEngine *engine, ToneData *voiceGroup);

/* Re-copy voiceGroup[currentProgram] into each track's currentVoice.
 * Call after editing voicegroup entries to propagate changes to active tracks. */
void m4a_engine_refresh_voices(M4AEngine *engine);

/* MIDI event handling */
void m4a_engine_note_on(M4AEngine *engine, int trackIndex, uint8_t key, uint8_t velocity);
void m4a_engine_note_off(M4AEngine *engine, int trackIndex, uint8_t key);
void m4a_engine_program_change(M4AEngine *engine, int trackIndex, uint8_t program);
void m4a_engine_cc(M4AEngine *engine, int trackIndex, uint8_t cc, uint8_t value);
void m4a_engine_pitch_bend(M4AEngine *engine, int trackIndex, int16_t bend);
void m4a_engine_all_notes_off(M4AEngine *engine, int trackIndex);
void m4a_engine_all_sound_off(M4AEngine *engine);

void m4a_engine_set_volume(M4AEngine *engine, uint8_t volume);
void m4a_engine_set_song_volume(M4AEngine *engine, uint8_t volume);
void m4a_engine_set_reverb_amount(M4AEngine *engine, uint8_t amount);
void m4a_engine_set_max_pcm_channels(M4AEngine *engine, uint8_t max_channels);
void m4a_engine_set_analog_filter(M4AEngine *engine, bool enabled);

/* Set tempo from DAW BPM.  The GBA relationship is tempoI ~= BPM
 * (24 ticks per quarter note at ~59.7 Hz VBlank gives BPM ~= tempoI). */
void m4a_engine_set_tempo_bpm(M4AEngine *engine, double bpm);

/* Audio processing. Callers should normally chunk to M4A_ENGINE_MAX_PROCESS_FRAMES. */
void m4a_engine_process(M4AEngine *engine, float *outL, float *outR, int numSamples);

/* Test/debug escape hatches. Production callers should use m4a_engine_* APIs. */
M4ADriver *m4a_engine_driver(M4AEngine *engine);
const M4ADriver *m4a_engine_driver_const(const M4AEngine *engine);
HwAudio *m4a_engine_hw_audio(M4AEngine *engine);
const HwAudio *m4a_engine_hw_audio_const(const M4AEngine *engine);

/* Frequency helpers */
uint32_t m4a_midi_key_to_freq(WaveData *wav, uint8_t key, uint8_t fineAdjust);
uint32_t m4a_midi_key_to_cgb_freq(uint8_t chanNum, uint8_t key, uint8_t fineAdjust);

/* 32x32->high32 multiply (matches GBA umul3232H32) */
static inline uint32_t umul3232H32(uint32_t a, uint32_t b)
{
	return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 32);
}

#endif /* M4A_ENGINE_H */
