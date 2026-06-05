#ifndef M4A_ENGINE_RECORDER_H
#define M4A_ENGINE_RECORDER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct M4ARecorder M4ARecorder;

M4ARecorder *m4a_recorder_create(void);
void m4a_recorder_destroy(M4ARecorder *recorder);

/* Hot-path (audio thread) -- gated by caller on arm flag. */
void m4a_recorder_push_beats(M4ARecorder *recorder, double beats,
			     uint8_t status, uint8_t d1, uint8_t d2);

/* Cold-path (message thread). */
void     m4a_recorder_reset(M4ARecorder *recorder);
uint64_t m4a_recorder_event_count(const M4ARecorder *recorder);

/* Returns true on success. tempo_bpm is required; pass 96 ppq for mid2agb. */
bool m4a_recorder_save_smf(const M4ARecorder *recorder, const char *path,
			   uint16_t ppq, double tempo_bpm);

#ifdef __cplusplus
}  /* extern "C" */

namespace ccomidi { class RecorderCore; }
ccomidi::RecorderCore *m4a_recorder_core(M4ARecorder *recorder);
#endif

#endif /* M4A_ENGINE_RECORDER_H */
