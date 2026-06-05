#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m4a_engine.h"
#include "voicegroup/voicegroup_loader.h"

#if defined(M4A_DRIVER_V2)
#include "m4a/m4a_driver.h"
#endif
#if defined(HW_AUDIO_V2)
#include "hw_audio/hw_audio.h"
#endif

#define SAMPLE_RATE 44100.0f
#define DEFAULT_LOOPS 1000
#define RENDER_FRAMES 4096
#define CHUNK_FRAMES 512

static int failures = 0;

#define CHECK(cond, msg) \
	do { \
		if (!(cond)) { \
			fprintf(stderr, "FAIL: %s\n", msg); \
			failures++; \
		} \
	} while (0)

static WaveData *make_test_wave(void)
{
	enum { DATA_SIZE = 64 };
	WaveData *wd = (WaveData *)calloc(1, sizeof(WaveData) + DATA_SIZE + 1);
	if (!wd) {
		return NULL;
	}

	wd->freq = 22050u << 10;
	wd->loopStart = 0;
	wd->size = DATA_SIZE;
	wd->data = (int8_t *)((uint8_t *)wd + sizeof(WaveData));
	for (int i = 0; i < DATA_SIZE; i++) {
		wd->data[i] = (int8_t)((i & 1) ? 80 : -80);
	}
	wd->data[DATA_SIZE] = 0;
	return wd;
}

static void make_test_voicegroup(ToneData voices[VOICEGROUP_SIZE], WaveData *wd)
{
	memset(voices, 0, sizeof(ToneData) * VOICEGROUP_SIZE);
	voices[0].type = VOICE_DIRECTSOUND;
	voices[0].key = 60;
	voices[0].wav = wd;
	voices[0].attack = 0x10;
	voices[0].decay = 0x20;
	voices[0].sustain = 0x80;
	voices[0].release = 0x20;
}

static float peak_abs(const float *left, const float *right, int count)
{
	float peak = 0.0f;
	for (int i = 0; i < count; i++) {
		float l = fabsf(left[i]);
		float r = fabsf(right[i]);
		if (l > peak) {
			peak = l;
		}
		if (r > peak) {
			peak = r;
		}
	}
	return peak;
}

static bool is_playable_voice(const ToneData *voice)
{
	switch (voice->type) {
	case VOICE_DIRECTSOUND:
	case VOICE_DIRECTSOUND_NO_RESAMPLE:
	case VOICE_DIRECTSOUND_ALT:
	case VOICE_CRY:
	case VOICE_CRY_REVERSE:
		return voice->wav != NULL;
	case VOICE_SQUARE_1:
	case VOICE_SQUARE_2:
	case VOICE_NOISE:
	case VOICE_SQUARE_1_ALT:
	case VOICE_SQUARE_2_ALT:
	case VOICE_NOISE_ALT:
		return true;
	case VOICE_PROGRAMMABLE_WAVE:
	case VOICE_PROGRAMMABLE_WAVE_ALT:
		return voice->wavePointer != NULL;
	default:
		return false;
	}
}

static int first_playable_program(ToneData *voices)
{
	for (int i = 0; i < VOICEGROUP_SIZE; i++) {
		if (is_playable_voice(&voices[i])) {
			return i;
		}
	}
	return -1;
}

static bool run_engine_cycle(ToneData *voices, int program)
{
	M4AEngine engine;
	float left[CHUNK_FRAMES];
	float right[CHUNK_FRAMES];
	float peak = 0.0f;

	if (!m4a_engine_init(&engine, SAMPLE_RATE)) {
		return false;
	}

	m4a_engine_set_voicegroup(&engine, voices);
	m4a_engine_program_change(&engine, 0, (uint8_t)program);
	m4a_engine_cc(&engine, 0, 7, 127);
	m4a_engine_cc(&engine, 0, 10, 64);
	m4a_engine_note_on(&engine, 0, 60, 100);

	for (int done = 0; done < RENDER_FRAMES; done += CHUNK_FRAMES) {
		memset(left, 0, sizeof(left));
		memset(right, 0, sizeof(right));
		m4a_engine_process(&engine, left, right, CHUNK_FRAMES);
		float chunk_peak = peak_abs(left, right, CHUNK_FRAMES);
		if (chunk_peak > peak) {
			peak = chunk_peak;
		}
	}

	m4a_engine_note_off(&engine, 0, 60);
	m4a_engine_all_sound_off(&engine);
	m4a_engine_destroy(&engine);

	return peak > 0.0001f;
}

#if defined(M4A_DRIVER_V2) && defined(HW_AUDIO_V2)
static bool run_driver_hw_cycle(ToneData *voices, int program)
{
	M4ADriver *drv = m4a_driver_create(SAMPLE_RATE);
	HwAudio *hw = hw_audio_create(SAMPLE_RATE);
	float left[CHUNK_FRAMES];
	float right[CHUNK_FRAMES];
	float peak = 0.0f;

	if (!drv || !hw) {
		if (drv) {
			m4a_driver_destroy(drv);
		}
		if (hw) {
			hw_audio_destroy(hw);
		}
		return false;
	}

	m4a_driver_set_voicegroup(drv, voices);
	m4a_program_change(drv, 0, (uint8_t)program);
	m4a_cc(drv, 0, 7, 127);
	m4a_cc(drv, 0, 10, 64);
	m4a_note_on(drv, 0, 60, 100);

	for (int done = 0; done < RENDER_FRAMES; done += CHUNK_FRAMES) {
		memset(left, 0, sizeof(left));
		memset(right, 0, sizeof(right));
		m4a_advance(drv, CHUNK_FRAMES);
		hw_audio_render_events(hw, m4a_get_pending_writes(drv),
		                       m4a_get_pcm_ring(drv), left, right, CHUNK_FRAMES);
		m4a_consume_writes(drv);
		float chunk_peak = peak_abs(left, right, CHUNK_FRAMES);
		if (chunk_peak > peak) {
			peak = chunk_peak;
		}
	}

	m4a_note_off(drv, 0, 60);
	m4a_all_sound_off(drv);
	hw_audio_destroy(hw);
	m4a_driver_destroy(drv);

	return peak > 0.0001f;
}
#endif

static void run_synthetic_cycles(int loops)
{
	for (int i = 0; i < loops; i++) {
		WaveData *wd = make_test_wave();
		ToneData voices[VOICEGROUP_SIZE];
		CHECK(wd != NULL, "test wave allocation succeeds");
		if (!wd) {
			return;
		}
		make_test_voicegroup(voices, wd);

		CHECK(run_engine_cycle(voices, 0), "M4AEngine cycle produces audio");
#if defined(M4A_DRIVER_V2) && defined(HW_AUDIO_V2)
		CHECK(run_driver_hw_cycle(voices, 0), "M4ADriver/HwAudio cycle produces audio");
#endif

		free(wd);
	}
}

static void run_loaded_voicegroup_cycles(const char *project_root,
                                         const char *voicegroup_name,
                                         int loops)
{
	for (int i = 0; i < loops; i++) {
		LoadedVoiceGroup *vg = voicegroup_load(project_root, voicegroup_name, NULL);
		CHECK(vg != NULL, "voicegroup_load succeeds");
		if (!vg) {
			return;
		}

		int program = first_playable_program(vg->voices);
		CHECK(program >= 0, "loaded voicegroup contains a playable voice");
		if (program < 0) {
			voicegroup_free(vg);
			return;
		}

		CHECK(run_engine_cycle(vg->voices, program), "loaded voicegroup engine cycle produces audio");
#if defined(M4A_DRIVER_V2) && defined(HW_AUDIO_V2)
		CHECK(run_driver_hw_cycle(vg->voices, program), "loaded voicegroup driver/hw cycle produces audio");
#endif

		voicegroup_free(vg);
	}
}

int main(int argc, char **argv)
{
	int loops = DEFAULT_LOOPS;

	if (argc >= 2) {
		loops = atoi(argv[1]);
		if (loops <= 0) {
			fprintf(stderr, "usage: %s [loops] [project_root voicegroup_name]\n", argv[0]);
			return 2;
		}
	}

	printf("Running poryaaaa engine lifecycle harness: loops=%d\n", loops);
	run_synthetic_cycles(loops);

	if (argc >= 4) {
		printf("Running loaded voicegroup lifecycle harness: root=%s voicegroup=%s loops=%d\n",
		       argv[2], argv[3], loops);
		run_loaded_voicegroup_cycles(argv[2], argv[3], loops);
	}

	if (failures != 0) {
		fprintf(stderr, "%d lifecycle checks failed\n", failures);
		return 1;
	}

	printf("poryaaaa engine lifecycle harness passed\n");
	return 0;
}
