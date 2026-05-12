#ifndef M4A_ENGINE_RECORDER_H
#define M4A_ENGINE_RECORDER_H

#include <stdint.h>
#include <stdbool.h>
#include "m4a_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Lifecycle: called from m4a_engine_init / m4a_engine_destroy. */
void m4a_engine_recorder_init(M4AEngine *engine);
void m4a_engine_recorder_destroy(M4AEngine *engine);

/* Hot-path (audio thread) — gated by caller on arm flag. */
void m4a_engine_recorder_push(M4AEngine *engine, uint32_t sample_in_block,
                              uint8_t status, uint8_t d1, uint8_t d2);
void m4a_engine_recorder_set_tempo(M4AEngine *engine, uint32_t sample_in_block,
                                   double bpm);
void m4a_engine_recorder_advance(M4AEngine *engine, uint32_t frames);

/* Cold-path (message thread). */
void     m4a_engine_recorder_reset(M4AEngine *engine);
void     m4a_engine_recorder_set_sample_rate(M4AEngine *engine, double sr);
uint64_t m4a_engine_recorder_event_count(const M4AEngine *engine);
double   m4a_engine_recorder_duration_seconds(const M4AEngine *engine);
void     m4a_engine_recorder_update_loop(M4AEngine *engine, bool active,
                                         double start_sec, double end_sec,
                                         double pos_sec);

/* Returns true on success. ppq default 480, fallback_bpm default 120. */
bool m4a_engine_recorder_save_smf(const M4AEngine *engine, const char *path,
                                  uint16_t ppq, double fallback_bpm);

#ifdef __cplusplus
}  /* extern "C" */

/* C++-only accessor for GUI/tests that want direct RecorderCore methods. */
namespace ccomidi { class RecorderCore; }
inline ccomidi::RecorderCore *m4a_engine_recorder(M4AEngine *engine) {
    return reinterpret_cast<ccomidi::RecorderCore *>(engine->recorder);
}
#endif

#endif /* M4A_ENGINE_RECORDER_H */
