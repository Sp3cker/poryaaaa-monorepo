#include "m4a_engine.h"

#include <string.h>

#include "hw_audio/hw_audio.h"
#include "m4a/m4a_driver.h"
#include "m4a_tables.h"

_Static_assert(M4A_ENGINE_MAX_PROCESS_FRAMES == M4A_RECOMMENDED_MAX_ADVANCE_FRAMES,
	       "M4A_ENGINE_MAX_PROCESS_FRAMES must match v2 driver queue limit");

uint32_t m4a_midi_key_to_freq(WaveData *wav, uint8_t key, uint8_t fineAdjust)
{
	uint32_t val1, val2;
	uint32_t fineAdjustShifted = (uint32_t)fineAdjust << 24;

	if (key > 178) {
		key = 178;
		fineAdjustShifted = 255u << 24;
	}

	val1 = gScaleTable[key];
	val1 = gFreqTable[val1 & 0xF] >> (val1 >> 4);

	val2 = gScaleTable[key + 1];
	val2 = gFreqTable[val2 & 0xF] >> (val2 >> 4);

	return umul3232H32(wav->freq, val1 + umul3232H32(val2 - val1, fineAdjustShifted));
}

uint32_t m4a_midi_key_to_cgb_freq(uint8_t chanNum, uint8_t key, uint8_t fineAdjust)
{
	if (chanNum == 4) {
		if (key <= 20)
			key = 0;
		else {
			key -= 21;
			if (key > 59)
				key = 59;
		}
		return gNoiseTable[key];
	} else {
		int32_t val1, val2;

		if (key <= 35) {
			fineAdjust = 0;
			key = 0;
		} else {
			key -= 36;
			if (key > 130) {
				key = 130;
				fineAdjust = 255;
			}
		}

		val1 = gCgbScaleTable[key];
		val1 = gCgbFreqTable[val1 & 0xF] >> (val1 >> 4);

		val2 = gCgbScaleTable[key + 1];
		val2 = gCgbFreqTable[val2 & 0xF] >> (val2 >> 4);

		return (uint32_t)(val1 + ((fineAdjust * (val2 - val1)) >> 8) + 2048);
	}
}

static void m4a_engine_xcmd_adapter(void *ctx, int trackIndex, uint8_t selector, uint32_t value)
{
	M4AEngine *engine = (M4AEngine *)ctx;

	if (engine->xcmd_fn)
		engine->xcmd_fn(engine->xcmd_ctx, trackIndex, selector, value);
}

bool m4a_engine_init(M4AEngine *engine, float sampleRate)
{
	memset(engine, 0, sizeof(*engine));

	engine->sampleRate = sampleRate;
	engine->volume = MAX_SONG_VOLUME;
	engine->reverbAmount = 0;

	engine->driver = m4a_driver_create(sampleRate);
	if (!engine->driver) {
		memset(engine, 0, sizeof(*engine));
		return false;
	}

	engine->hw = hw_audio_create(sampleRate);
	if (!engine->hw) {
		m4a_engine_destroy(engine);
		return false;
	}

	m4a_set_master_volume(engine->driver, 15);
	m4a_set_song_volume(engine->driver, engine->volume);
	m4a_set_reverb_amount(engine->driver, engine->reverbAmount);
	m4a_set_max_pcm_channels(engine->driver, MAX_PCM_CHANNELS);
	m4a_set_analog_filter(engine->driver, false);

	return true;
}

void m4a_engine_destroy(M4AEngine *engine)
{
	if (!engine)
		return;

	if (engine->hw)
		hw_audio_destroy(engine->hw);
	if (engine->driver)
		m4a_driver_destroy(engine->driver);

	memset(engine, 0, sizeof(*engine));
}

bool m4a_engine_reset(M4AEngine *engine)
{
	float sampleRate = engine->sampleRate;

	m4a_engine_destroy(engine);
	return m4a_engine_init(engine, sampleRate);
}

void m4a_engine_set_xcmd_callback(M4AEngine *engine, M4AEngineXcmdFn xcmd_fn, void *xcmd_ctx)
{
	engine->xcmd_fn = xcmd_fn;
	engine->xcmd_ctx = xcmd_ctx;
	m4a_driver_set_xcmd_callback(engine->driver, m4a_engine_xcmd_adapter, engine);
}

void m4a_engine_set_tempo_bpm(M4AEngine *engine, double bpm)
{
	m4a_set_tempo_bpm(engine->driver, bpm);
}

void m4a_engine_set_voicegroup(M4AEngine *engine, ToneData *voiceGroup)
{
	m4a_engine_all_sound_off(engine);
	engine->voiceGroup = voiceGroup;
	m4a_driver_set_voicegroup(engine->driver, voiceGroup);
	m4a_driver_refresh_voices(engine->driver);
}

void m4a_engine_refresh_voices(M4AEngine *engine)
{
	m4a_driver_refresh_voices(engine->driver);
}

void m4a_engine_note_on(M4AEngine *engine, int trackIndex, uint8_t key, uint8_t velocity)
{
	if (trackIndex < 0 || trackIndex >= MAX_TRACKS || !engine->voiceGroup)
		return;
	m4a_note_on(engine->driver, trackIndex, key, velocity);
}

void m4a_engine_note_off(M4AEngine *engine, int trackIndex, uint8_t key)
{
	if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
		return;
	m4a_note_off(engine->driver, trackIndex, key);
}

void m4a_engine_program_change(M4AEngine *engine, int trackIndex, uint8_t program)
{
	if (trackIndex < 0 || trackIndex >= MAX_TRACKS || !engine->voiceGroup)
		return;
	m4a_program_change(engine->driver, trackIndex, program);
}

void m4a_engine_cc(M4AEngine *engine, int trackIndex, uint8_t cc, uint8_t value)
{
	if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
		return;
	m4a_cc(engine->driver, trackIndex, cc, value);
}

void m4a_engine_pitch_bend(M4AEngine *engine, int trackIndex, int16_t bend)
{
	if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
		return;
	m4a_pitch_bend(engine->driver, trackIndex, bend);
}

void m4a_engine_all_notes_off(M4AEngine *engine, int trackIndex)
{
	if (trackIndex < 0 || trackIndex >= MAX_TRACKS)
		return;
	m4a_all_notes_off(engine->driver, trackIndex);
}

void m4a_engine_all_sound_off(M4AEngine *engine)
{
	m4a_all_sound_off(engine->driver);
}

void m4a_engine_set_volume(M4AEngine *engine, uint8_t volume)
{
	engine->volume = volume;
	m4a_set_song_volume(engine->driver, volume);
}

void m4a_engine_set_song_volume(M4AEngine *engine, uint8_t volume)
{
	m4a_engine_set_volume(engine, volume);
}

void m4a_engine_set_reverb_amount(M4AEngine *engine, uint8_t amount)
{
	engine->reverbAmount = amount;
	m4a_set_reverb_amount(engine->driver, amount);
}

void m4a_engine_set_max_pcm_channels(M4AEngine *engine, uint8_t max_channels)
{
	m4a_set_max_pcm_channels(engine->driver, max_channels);
}

void m4a_engine_set_analog_filter(M4AEngine *engine, bool enabled)
{
	m4a_set_analog_filter(engine->driver, enabled);
}

void m4a_engine_process(M4AEngine *engine, float *outL, float *outR, int numSamples)
{
	if (numSamples <= 0)
		return;

	m4a_advance(engine->driver, numSamples);
	hw_audio_render_events(engine->hw,
			       m4a_get_pending_writes(engine->driver),
			       m4a_get_pcm_ring(engine->driver),
			       outL, outR, numSamples);
	m4a_consume_writes(engine->driver);
}

M4ADriver *m4a_engine_driver(M4AEngine *engine)
{
	return engine->driver;
}

const M4ADriver *m4a_engine_driver_const(const M4AEngine *engine)
{
	return engine->driver;
}

HwAudio *m4a_engine_hw_audio(M4AEngine *engine)
{
	return engine->hw;
}

const HwAudio *m4a_engine_hw_audio_const(const M4AEngine *engine)
{
	return engine->hw;
}
