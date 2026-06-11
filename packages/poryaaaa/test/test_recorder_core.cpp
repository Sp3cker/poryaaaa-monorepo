#include "m4a_engine_recorder.h"
#include "recorder/recorder_core.h"
#include "recorder/smf_writer.h"
#include "test_assert.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <vector>

namespace {

struct ParsedMidiEvent {
	int tick = 0;
	std::vector<unsigned char> bytes;
};

uint32_t read_be32(const std::vector<unsigned char> &bytes, size_t offset)
{
	return ((uint32_t)bytes[offset] << 24) | ((uint32_t)bytes[offset + 1] << 16) |
	       ((uint32_t)bytes[offset + 2] << 8) | (uint32_t)bytes[offset + 3];
}

uint16_t read_be16(const std::vector<unsigned char> &bytes, size_t offset)
{
	return (uint16_t)(((uint16_t)bytes[offset] << 8) | (uint16_t)bytes[offset + 1]);
}

bool read_vlq(const std::vector<unsigned char> &bytes, size_t end, size_t &pos,
	      int &value)
{
	value = 0;
	for (int i = 0; i < 4; i++) {
		if (pos >= end)
			return false;
		unsigned char byte = bytes[pos++];
		value = (value << 7) | (byte & 0x7F);
		if ((byte & 0x80) == 0)
			return true;
	}
	return false;
}

std::vector<unsigned char> read_file_bytes(const char *path)
{
	std::ifstream file(path, std::ios::binary);
	return std::vector<unsigned char>(std::istreambuf_iterator<char>(file),
					  std::istreambuf_iterator<char>());
}

std::vector<ParsedMidiEvent> parse_channel_events(const char *path)
{
	std::vector<unsigned char> bytes = read_file_bytes(path);
	std::vector<ParsedMidiEvent> events;
	if (bytes.size() < 14 || bytes[0] != 'M' || bytes[1] != 'T' ||
	    bytes[2] != 'h' || bytes[3] != 'd')
		return events;

	uint32_t headerLength = read_be32(bytes, 4);
	if (headerLength < 6 || bytes.size() < 8 + headerLength)
		return events;

	uint16_t trackCount = read_be16(bytes, 10);
	size_t pos = 8 + headerLength;
	for (uint16_t track = 0; track < trackCount; track++) {
		if (pos + 8 > bytes.size() || bytes[pos] != 'M' || bytes[pos + 1] != 'T' ||
		    bytes[pos + 2] != 'r' || bytes[pos + 3] != 'k')
			return events;
		uint32_t trackLength = read_be32(bytes, pos + 4);
		pos += 8;
		size_t trackEnd = pos + trackLength;
		if (trackEnd > bytes.size())
			return events;

		int tick = 0;
		unsigned char runningStatus = 0;
		while (pos < trackEnd) {
			int delta = 0;
			if (!read_vlq(bytes, trackEnd, pos, delta))
				return events;
			tick += delta;
			if (pos >= trackEnd)
				return events;

			unsigned char status = bytes[pos++];
			if (status < 0x80) {
				if (runningStatus == 0)
					return events;
				pos--;
				status = runningStatus;
			} else if (status < 0xF0) {
				runningStatus = status;
			}

			if (status == 0xFF) {
				if (pos >= trackEnd)
					return events;
				pos++;
				int length = 0;
				if (!read_vlq(bytes, trackEnd, pos, length))
					return events;
				pos += (size_t)length;
			} else if (status == 0xF0 || status == 0xF7) {
				int length = 0;
				if (!read_vlq(bytes, trackEnd, pos, length))
					return events;
				pos += (size_t)length;
			} else {
				int dataLength = ((status & 0xF0) == 0xC0 ||
						  (status & 0xF0) == 0xD0) ? 1 : 2;
				if (pos + (size_t)dataLength > trackEnd)
					return events;
				ParsedMidiEvent event;
				event.tick = tick;
				event.bytes.push_back(status);
				for (int i = 0; i < dataLength; i++)
					event.bytes.push_back(bytes[pos++]);
				events.push_back(event);
			}
		}
		pos = trackEnd;
	}
	return events;
}

bool has_event(const std::vector<ParsedMidiEvent> &events, int tick,
	       std::initializer_list<unsigned char> bytes)
{
	for (const ParsedMidiEvent &event : events) {
		if (event.tick == tick && event.bytes.size() == bytes.size() &&
		    std::equal(event.bytes.begin(), event.bytes.end(), bytes.begin()))
			return true;
	}
	return false;
}

int count_events_with_status_data1(const std::vector<ParsedMidiEvent> &events,
				   unsigned char status, unsigned char data1)
{
	int count = 0;
	for (const ParsedMidiEvent &event : events) {
		if (event.bytes.size() >= 2 && event.bytes[0] == status &&
		    event.bytes[1] == data1)
			count++;
	}
	return count;
}

double beat_for_tick(int tick)
{
	return 10.0 + (double)tick / 96.0;
}

double beat_for_fractional_tick(double tick)
{
	return 10.0 + tick / 96.0;
}

void test_recorder_core_records_beats()
{
	ccomidi::RecorderCore recorder;
	recorder.push_event_at_beats(4.25, 0x90, 60, 100);
	recorder.push_event_at_beats(4.75, 0x80, 60, 0);

	ccomidi::RecorderCore::Snapshot snapshot = recorder.snapshot();
	ASSERT(snapshot.midi.size() == 2, "recorder stores beat-stamped events");
	ASSERT_NEAR(snapshot.midi[0].beats, 4.25, 0.000001, "first event beats");
	ASSERT_EQ(snapshot.midi[0].status, 0x90, "first event status");
	ASSERT_EQ(snapshot.midi[0].data1, 60, "first event data1");
	ASSERT_EQ(snapshot.midi[0].data2, 100, "first event data2");
	ASSERT_NEAR(snapshot.midi[1].beats, 4.75, 0.000001, "second event beats");
}

void test_recorder_core_reset_clears_buffer()
{
	ccomidi::RecorderCore recorder;
	recorder.push_event_at_beats(1.0, 0x90, 64, 90);
	recorder.reset();

	ASSERT(recorder.midi_event_count() == 0, "reset clears recorder buffer");
}

void test_recorder_bridge_counts_beat_pushes()
{
	M4ARecorder *recorder = m4a_recorder_create();
	ASSERT(recorder != nullptr, "recorder bridge creates recorder");
	if (!recorder)
		return;

	m4a_recorder_push_beats(recorder, 2.0, 0xC0, 5, 0);
	ASSERT(m4a_recorder_event_count(recorder) == 1, "bridge push stores event");
	m4a_recorder_destroy(recorder);
}

void test_recorder_bridge_rejects_posix_backslashes()
{
#if !defined(_WIN32)
	M4ARecorder *recorder = m4a_recorder_create();
	ASSERT(recorder != nullptr, "recorder bridge creates recorder for path test");
	if (!recorder)
		return;

	bool ok = m4a_recorder_save_smf(recorder, "\\Users\\spencer\\testf1.mid",
					96, 120.0);
	ASSERT(!ok, "bridge rejects backslash path separators on POSIX");
	m4a_recorder_destroy(recorder);
#endif
}

void test_smf_writer_coarse_grid_and_latest_value()
{
	const char *path = "poryaaaa_smf_grid_test.mid";
	std::remove(path);

	ccomidi::RecorderCore::Snapshot snapshot;
	snapshot.midi.push_back({beat_for_tick(0), 0x90, 60, 100});
	snapshot.midi.push_back({beat_for_tick(5), 0xB0, 7, 10});
	snapshot.midi.push_back({beat_for_tick(6), 0xB0, 7, 20});
	snapshot.midi.push_back({beat_for_tick(7), 0xB0, 7, 30});
	snapshot.midi.push_back({beat_for_tick(8), 0x90, 62, 100});
	snapshot.midi.push_back({beat_for_tick(9), 0xC0, 3, 0});
	snapshot.midi.push_back({beat_for_tick(10), 0xC0, 4, 0});
	snapshot.midi.push_back({beat_for_tick(13), 0xE0, 1, 64});
	snapshot.midi.push_back({beat_for_tick(14), 0xE0, 2, 65});
	snapshot.midi.push_back({beat_for_tick(17), 0x80, 60, 0});
	snapshot.midi.push_back({beat_for_tick(18), 0x80, 62, 0});
	snapshot.midi.push_back({beat_for_tick(21), 0xB0, 0x1E, 0x03});
	snapshot.midi.push_back({beat_for_tick(22), 0xB0, 0x1D, 0x11});
	snapshot.midi.push_back({beat_for_tick(23), 0xB0, 0x1F, 0x22});

	ccomidi::SmfWriteOptions options;
	options.ppq = 96;
	options.tempoBpm = 120.0;
	ASSERT(ccomidi::write_smf1(path, snapshot, options), "writer creates SMF");

	std::vector<ParsedMidiEvent> events = parse_channel_events(path);
	ASSERT(has_event(events, 0, {0x90, 60, 100}), "note on stays at 96ppq tick");
	ASSERT(has_event(events, 4, {0xB0, 7, 30}), "latest CC in coarse cell wins");
	ASSERT(!has_event(events, 4, {0xB0, 7, 10}), "older CC value is dropped");
	ASSERT(!has_event(events, 4, {0xB0, 7, 20}), "middle CC value is dropped");
	ASSERT(has_event(events, 8, {0x90, 62, 100}), "next note stays on note grid");
	ASSERT(has_event(events, 8, {0xC0, 4}), "latest PC in coarse cell wins");
	ASSERT(!has_event(events, 8, {0xC0, 3}), "older PC is dropped");
	ASSERT(has_event(events, 12, {0xE0, 2, 65}), "latest pitch bend in coarse cell wins");
	ASSERT(!has_event(events, 12, {0xE0, 1, 64}), "older pitch bend is dropped");
	ASSERT(has_event(events, 17, {0x80, 60, 0}), "note off is not coarse-rounded");
	ASSERT(has_event(events, 18, {0x80, 62, 0}), "second note off is not coarse-rounded");
	ASSERT(has_event(events, 20, {0xB0, 0x1E, 0x03}), "XCMD selector is preserved");
	ASSERT(has_event(events, 20, {0xB0, 0x1D, 0x11}), "XCMD payload is preserved");
	ASSERT(has_event(events, 20, {0xB0, 0x1F, 0x22}), "XCMD alternate payload is preserved");
	ASSERT(count_events_with_status_data1(events, 0xB0, 0x1E) == 1,
	       "XCMD selector is not duplicated");
	std::remove(path);
}

void test_smf_writer_normalizes_note_ticks()
{
	const char *path = "poryaaaa_smf_note_grid_test.mid";
	std::remove(path);

	ccomidi::RecorderCore::Snapshot snapshot;
	snapshot.midi.push_back({beat_for_fractional_tick(0.49), 0x90, 60, 100});
	snapshot.midi.push_back({beat_for_fractional_tick(7.99), 0xB0, 7, 70});
	snapshot.midi.push_back({beat_for_fractional_tick(23.51), 0x90, 62, 100});
	snapshot.midi.push_back({beat_for_fractional_tick(47.51), 0x80, 60, 0});
	snapshot.midi.push_back({beat_for_fractional_tick(71.51), 0x80, 62, 0});

	ccomidi::SmfWriteOptions options;
	options.ppq = 96;
	options.tempoBpm = 120.0;
	ASSERT(ccomidi::write_smf1(path, snapshot, options), "writer creates jitter SMF");

	std::vector<ParsedMidiEvent> events = parse_channel_events(path);
	ASSERT(has_event(events, 0, {0x90, 60, 100}),
	       "first note rounds to export start");
	ASSERT(has_event(events, 4, {0xB0, 7, 70}),
	       "controller event still moves backward to coarse grid");
	ASSERT(has_event(events, 24, {0x90, 62, 100}),
	       "note on rounds to nearest PPQ tick");
	ASSERT(has_event(events, 48, {0x80, 60, 0}),
	       "note off rounds to nearest PPQ tick");
	ASSERT(has_event(events, 72, {0x80, 62, 0}),
	       "second note off rounds to nearest PPQ tick");
	std::remove(path);
}

// Test that explicit initial ccomidi params (non-default) are emitted at tick 0
// even when the snapshot has no pre-note events for them (or no events at all).
// This matches the M4L buildSmf(voicemap + initialCcs) contract and the
// requirement that any ccomidi parameter that isn't default must appear at
// tick-0 in the exported SMF for correct mid2agb / m4a track setup.
void test_smf_writer_emits_initial_ccomidi_params_at_tick0()
{
	const char *path = "poryaaaa_smf_initial_state_test.mid";
	std::remove(path);

	ccomidi::RecorderCore::Snapshot snapshot;
	// Only a note; no PC or special CCs in the captured events.
	snapshot.midi.push_back({beat_for_tick(10), 0x90, 60, 100});
	snapshot.midi.push_back({beat_for_tick(12), 0x80, 60, 0});

	ccomidi::SmfWriteOptions options;
	options.ppq = 96;
	options.tempoBpm = 120.0;
	// Provide initial for ch 0 (program 5) and a ccomidi param e.g. BENDR=4 (CC 0x14)
	// plus a normal VOL for ch 3.
	options.initialPrograms.push_back({0, 5});
	options.initialCcs.push_back({0, 0x14, 4});  // BENDR
	options.initialCcs.push_back({3, 0x07, 100});

	ASSERT(ccomidi::write_smf1(path, snapshot, options), "writer with initial state");

	std::vector<ParsedMidiEvent> events = parse_channel_events(path);

	// PC for ch0 at tick 0
	ASSERT(has_event(events, 0, {0xC0, 5}), "initial PC emitted at tick 0");
	// BENDR CC for ch0 at tick 0
	ASSERT(has_event(events, 0, {0xB0, 0x14, 4}), "initial BENDR (ccomidi param) at tick 0");
	// The note itself is later
	ASSERT(has_event(events, 10, {0x90, 60, 100}), "note after initial state");

	std::remove(path);
}

} // namespace

extern "C" void test_recorder_core_run_all(void)
{
	test_recorder_core_records_beats();
	test_recorder_core_reset_clears_buffer();
	test_recorder_bridge_counts_beat_pushes();
	test_recorder_bridge_rejects_posix_backslashes();
	test_smf_writer_coarse_grid_and_latest_value();
	test_smf_writer_normalizes_note_ticks();
	test_smf_writer_emits_initial_ccomidi_params_at_tick0();
}
