#include "m4a_engine_recorder.h"
#include "recorder/recorder_core.h"
#include "recorder/smf_writer.h"

/* ---- Lifecycle ---- */

void m4a_engine_recorder_init(M4AEngine *engine)
{
    if (!engine) return;
    engine->recorder = new ccomidi::RecorderCore();
}

void m4a_engine_recorder_destroy(M4AEngine *engine)
{
    if (!engine || !engine->recorder) return;
    delete reinterpret_cast<ccomidi::RecorderCore *>(engine->recorder);
    engine->recorder = nullptr;
}

/* ---- Hot-path ---- */

void m4a_engine_recorder_push(M4AEngine *engine, uint32_t sample_in_block,
                               uint8_t status, uint8_t d1, uint8_t d2)
{
    if (!engine || !engine->recorder) return;
    reinterpret_cast<ccomidi::RecorderCore *>(engine->recorder)
        ->push_event_in_block(sample_in_block, status, d1, d2);
}

void m4a_engine_recorder_set_tempo(M4AEngine *engine, uint32_t sample_in_block,
                                    double bpm)
{
    if (!engine || !engine->recorder) return;
    reinterpret_cast<ccomidi::RecorderCore *>(engine->recorder)
        ->set_tempo_in_block(sample_in_block, bpm);
}

void m4a_engine_recorder_advance(M4AEngine *engine, uint32_t frames)
{
    if (!engine || !engine->recorder) return;
    reinterpret_cast<ccomidi::RecorderCore *>(engine->recorder)
        ->advance_block(frames);
}

/* ---- Cold-path ---- */

void m4a_engine_recorder_reset(M4AEngine *engine)
{
    if (!engine || !engine->recorder) return;
    reinterpret_cast<ccomidi::RecorderCore *>(engine->recorder)->reset();
}

void m4a_engine_recorder_set_sample_rate(M4AEngine *engine, double sr)
{
    if (!engine || !engine->recorder) return;
    reinterpret_cast<ccomidi::RecorderCore *>(engine->recorder)
        ->set_sample_rate(sr);
}

uint64_t m4a_engine_recorder_event_count(const M4AEngine *engine)
{
    if (!engine || !engine->recorder) return 0;
    return (uint64_t)reinterpret_cast<const ccomidi::RecorderCore *>(engine->recorder)
        ->midi_event_count();
}

double m4a_engine_recorder_duration_seconds(const M4AEngine *engine)
{
    if (!engine || !engine->recorder) return 0.0;
    return reinterpret_cast<const ccomidi::RecorderCore *>(engine->recorder)
        ->duration_seconds();
}

void m4a_engine_recorder_update_loop(M4AEngine *engine, bool active,
                                      double start_sec, double end_sec,
                                      double pos_sec)
{
    if (!engine || !engine->recorder) return;
    reinterpret_cast<ccomidi::RecorderCore *>(engine->recorder)
        ->update_loop_from_transport(active, start_sec, end_sec, pos_sec);
}

bool m4a_engine_recorder_save_smf(const M4AEngine *engine, const char *path,
                                   uint16_t ppq, double fallback_bpm)
{
    if (!engine || !engine->recorder || !path) return false;
    ccomidi::SmfWriteOptions opts;
    opts.ppq = ppq;
    opts.fallbackBpm = fallback_bpm;
    auto snap = reinterpret_cast<const ccomidi::RecorderCore *>(engine->recorder)
        ->snapshot();
    return ccomidi::write_smf1(std::string(path), snap, opts);
}
