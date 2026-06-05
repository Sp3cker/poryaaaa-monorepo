#include "m4a_engine_recorder.h"

#include <cstring>
#include <new>
#include <string>

#include "recorder/recorder_core.h"
#include "recorder/smf_writer.h"

struct M4ARecorder {
	ccomidi::RecorderCore core;
};

static bool recorder_save_path_is_valid(const char *path)
{
	if (!path || path[0] == '\0')
		return false;
#if defined(_WIN32)
	return true;
#else
	return std::strchr(path, '\\') == nullptr;
#endif
}

M4ARecorder *m4a_recorder_create(void)
{
	return new (std::nothrow) M4ARecorder();
}

void m4a_recorder_destroy(M4ARecorder *recorder)
{
	delete recorder;
}

void m4a_recorder_push_beats(M4ARecorder *recorder, double beats,
			     uint8_t status, uint8_t d1, uint8_t d2)
{
	if (!recorder)
		return;
	recorder->core.push_event_at_beats(beats, status, d1, d2);
}

void m4a_recorder_reset(M4ARecorder *recorder)
{
	if (!recorder)
		return;
	recorder->core.reset();
}

uint64_t m4a_recorder_event_count(const M4ARecorder *recorder)
{
	if (!recorder)
		return 0;
	return (uint64_t)recorder->core.midi_event_count();
}

bool m4a_recorder_save_smf(const M4ARecorder *recorder, const char *path,
			   uint16_t ppq, double tempo_bpm)
{
	if (!recorder || !recorder_save_path_is_valid(path))
		return false;

	ccomidi::SmfWriteOptions opts;
	opts.ppq = ppq;
	opts.tempoBpm = tempo_bpm;
	auto snap = recorder->core.snapshot();
	return ccomidi::write_smf1(std::string(path), snap, opts);
}

ccomidi::RecorderCore *m4a_recorder_core(M4ARecorder *recorder)
{
	if (!recorder)
		return nullptr;
	return &recorder->core;
}
