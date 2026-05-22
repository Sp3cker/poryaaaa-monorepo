#include "m4a_engine_recorder.h"

#include <new>
#include <string>

#include "recorder/recorder_core.h"
#include "recorder/smf_writer.h"

struct M4ARecorder {
	ccomidi::RecorderCore core;
};

M4ARecorder *m4a_recorder_create(void)
{
	return new (std::nothrow) M4ARecorder();
}

void m4a_recorder_destroy(M4ARecorder *recorder)
{
	delete recorder;
}

void m4a_recorder_push(M4ARecorder *recorder, uint32_t sample_in_block,
		       uint8_t status, uint8_t d1, uint8_t d2)
{
	if (!recorder)
		return;
	recorder->core.push_event_in_block(sample_in_block, status, d1, d2);
}

void m4a_recorder_set_tempo(M4ARecorder *recorder, uint32_t sample_in_block,
			    double bpm)
{
	if (!recorder)
		return;
	recorder->core.set_tempo_in_block(sample_in_block, bpm);
}

void m4a_recorder_advance(M4ARecorder *recorder, uint32_t frames)
{
	if (!recorder)
		return;
	recorder->core.advance_block(frames);
}

void m4a_recorder_reset(M4ARecorder *recorder)
{
	if (!recorder)
		return;
	recorder->core.reset();
}

void m4a_recorder_set_sample_rate(M4ARecorder *recorder, double sr)
{
	if (!recorder)
		return;
	recorder->core.set_sample_rate(sr);
}

uint64_t m4a_recorder_event_count(const M4ARecorder *recorder)
{
	if (!recorder)
		return 0;
	return (uint64_t)recorder->core.midi_event_count();
}

double m4a_recorder_duration_seconds(const M4ARecorder *recorder)
{
	if (!recorder)
		return 0.0;
	return recorder->core.duration_seconds();
}

void m4a_recorder_update_loop(M4ARecorder *recorder, bool active,
			      double start_sec, double end_sec, double pos_sec)
{
	if (!recorder)
		return;
	recorder->core.update_loop_from_transport(active, start_sec, end_sec, pos_sec);
}

bool m4a_recorder_save_smf(const M4ARecorder *recorder, const char *path,
			   uint16_t ppq, double fallback_bpm)
{
	if (!recorder || !path)
		return false;

	ccomidi::SmfWriteOptions opts;
	opts.ppq = ppq;
	opts.fallbackBpm = fallback_bpm;
	auto snap = recorder->core.snapshot();
	return ccomidi::write_smf1(std::string(path), snap, opts);
}

ccomidi::RecorderCore *m4a_recorder_core(M4ARecorder *recorder)
{
	if (!recorder)
		return nullptr;
	return &recorder->core;
}
