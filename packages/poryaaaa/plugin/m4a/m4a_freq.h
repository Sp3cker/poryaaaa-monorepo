#ifndef M4A_FREQ_H
#define M4A_FREQ_H

#include <stdint.h>
#include "voicegroup/voicegroup_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MidiKeyToFreq — pokeemerald m4a.c equivalent.  Returns a 32.0
 * sample-step word for PCM playback at m4a's PCM_DMA_BUF rate. */
uint32_t m4a_midi_key_to_freq(WaveData *wav, uint8_t key, uint8_t fineAdjust);

/* MidiKeyToCgbFreq — pokeemerald m4a.c equivalent.
 *
 *   chanNum 1 / 2 → square 1/2:           returns 11-bit GB freq word (0..2047).
 *   chanNum 3     → wave (programmable):  returns 11-bit GB freq word.
 *   chanNum 4     → noise:                returns NR43 byte (clock_shift<<4 | divisor_code),
 *                                          *without* the period/width bit (NR43 bit 3) which
 *                                          the caller ORs in from the voice's wavePointer LSB. */
uint32_t m4a_midi_key_to_cgb_freq(uint8_t chanNum, uint8_t key, uint8_t fineAdjust);

#ifdef __cplusplus
}
#endif

#endif
