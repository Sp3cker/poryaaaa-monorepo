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
void m4a_recorder_push(M4ARecorder *recorder, uint32_t sample_in_block,
		       uint8_t status, uint8_t d1, uint8_t d2);
void m4a_recorder_set_tempo(M4ARecorder *recorder, uint32_t sample_in_block,
			    double bpm);
void m4a_recorder_advance(M4ARecorder *recorder, uint32_t frames);

/* Cold-path (message thread). */
void     m4a_recorder_reset(M4ARecorder *recorder);
void     m4a_recorder_set_sample_rate(M4ARecorder *recorder, double sr);
uint64_t m4a_recorder_event_count(const M4ARecorder *recorder);
double   m4a_recorder_duration_seconds(const M4ARecorder *recorder);
void     m4a_recorder_update_loop(M4ARecorder *recorder, bool active,
				  double start_sec, double end_sec, double pos_sec);

/* Returns true on success. ppq default 480, fallback_bpm default 120. */
bool m4a_recorder_save_smf(const M4ARecorder *recorder, const char *path,
			   uint16_t ppq, double fallback_bpm);

#ifdef __cplusplus
}  /* extern "C" */

namespace ccomidi { class RecorderCore; }
ccomidi::RecorderCore *m4a_recorder_core(M4ARecorder *recorder);
#endif

#endif /* M4A_ENGINE_RECORDER_H */
