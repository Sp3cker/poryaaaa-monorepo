#ifndef HW_PSG_H
#define HW_PSG_H

#include <stdbool.h>
#include <stdint.h>

#include "m4a/m4a_driver.h"   /* M4ARegWrite */

#ifdef __cplusplus
extern "C" {
#endif

/* PSG synth state — sq1, sq2, wave, noise.  Mirrors the relevant subset
 * of mGBA gb_audio.c hardware state: envelope volume + duty/wave-RAM/
 * LFSR + phase + DAC + pan masks.
 *
 * Frequency / phase model (per real GB hardware):
 *   audio_hz_square = 131072 / (2048 - F)
 *   audio_hz_wave   =  65536 / (2048 - F)
 *   audio_hz_noise  = 524288 / divisor / 2^(shift+1)   [divisor 0→0.5, 1..7→1..7]
 *   phase increment per host sample = audio_hz / host_rate × 2^32
 *   square phase is preserved across both MO_PIT (NRx3+NRx4-no-trigger) and
 *     NRx4-with-trigger writes — real GB doesn't reset the duty step counter.
 *   wave RAM position resets to 0 on NR34-with-trigger (real GB GBATEK).
 *   noise LFSR resets to 0 on NR44-with-trigger (mirrors mGBA gb_audio.c:374).
 *   Driver MUST gate the trigger bit to fresh-note events only — emitting
 *     trigger=1 on every envelope-update tick repeatedly resets wave RAM /
 *     LFSR position and corrupts sustained notes.  See m4a_cgb.c freshStart.
 *
 * Noise model — per §12.6 verification gate decision (closed 2026-04-29,
 * `noise_verification_gate.md`): mGBA's GBA-mode "current LFSR sample
 * shifted" path, mirroring `gb_audio.c:631-637` verbatim.  Each clock
 * computes `lsb = (lfsr ^ (lfsr>>1) ^ 1) & 1`, shifts the register
 * right by 1, and writes the new feedback bit to position 14 (15-bit)
 * or 6 (7-bit).  Output is the latest `lsb` (the feedback bit itself,
 * matching `audio->ch4.sample = lsb * envelope.currentVolume` at
 * gb_audio.c:641).  LFSR initial state on NR44 trigger is 0 (mGBA),
 * not 0x7FFF — the all-ones state is a fixed point under this
 * polynomial.  NO sub-sample averaging — the v1 `_coalesceNoiseChannel`
 * clone is explicitly dropped.  Step 9's polyphase resampler band-limits.
 *
 * ⚠ Self-consistency tested but mGBA / hardware parity NOT proven —
 * see plan §12.10b.  Don't make spectral / level / per-channel
 * comparisons against v1 or mGBA captures from this code yet.
 *
 * Synth runs at the chip-internal "render rate" — set by HwAudio to
 * `max(131072, 32768 << sampling_cycle)` per plan §7b (131072 Hz for
 * SOUNDBIAS sampling_cycle 0/1/2; 262144 Hz for sampling_cycle 3),
 * NOT at the actual host rate.  Step 9's polyphase resampler bridges
 * from render rate to host rate and band-limits at host_rate/2.
 *
 * Open gates that must close before parity claims are valid (plan §12
 * "blocking gates"):
 *   - step 10b — mGBA capture-comparison parity (self-consistency
 *     tests landed at §12.10a but don't compare to a reference)
 *   - steps 1-2 audit — driver-side `MPlayMain` / `ply_*` + LFO/MODT
 *     and PCM / reverb verification before whole-song A/B
 *
 * Until those land, this file is correct for AUDIBILITY + INTERNAL
 * COHERENCE — it proves the call graph + event stream + per-channel
 * synth shapes are right, but not that they match real-hardware /
 * mGBA spectra. */

typedef struct {
    /* Per-channel runtime state */
    uint32_t sq1_phase;
    uint32_t sq2_phase;
    uint32_t wave_phase;

    uint16_t sq1_freq;          /* 11-bit freq word */
    uint16_t sq2_freq;
    uint16_t wave_freq;

    uint16_t sq1_sweep_shadow_freq;
    uint8_t  sq1_sweep_time;       /* NR10 pace; 0 is stored as 8 */
    uint8_t  sq1_sweep_shift;
    uint8_t  sq1_sweep_timer;
    bool     sq1_sweep_decrease;
    bool     sq1_sweep_enabled;
    bool     sq1_sweep_occurred;

    uint8_t  sq1_duty;          /* 0..3 */
    uint8_t  sq2_duty;

    uint8_t  sq1_env_vol;       /* 0..15 */
    uint8_t  sq2_env_vol;
    uint8_t  wave_vol_code;     /* NR32 byte */

    bool     sq1_dac_enabled;
    bool     sq1_enabled;
    bool     sq2_enabled;
    bool     wave_enabled;
    bool     wave_dac_on;       /* NR30 bit 7 */

    uint8_t  wave_ram[16];

    /* Noise (NR41..NR44) */
    uint16_t noise_lfsr;            /* shift register; reload to 0 on NR44 trigger */
    uint32_t noise_phase;           /* fractional-clocks accumulator (2^32 = 1 clock) */
    uint8_t  noise_clock_shift;     /* NR43 bits 7-4 */
    uint8_t  noise_divisor_code;    /* NR43 bits 2-0 */
    uint8_t  noise_last_sample;     /* last LFSR feedback bit (held between output samples) */
    bool     noise_width_7bit;      /* NR43 bit 3 */
    uint8_t  noise_env_vol;         /* 0..15 */
    bool     noise_enabled;

    /* NR52 bit 7 — master enable.  When false all PSG channels silent.
     * Owned here (not by mix bus) because it gates the synth itself in
     * real GB hardware: when NR52 master is off the channels' DAC paths
     * are powered down, not just muted at the mix stage. */
    bool     master_enabled;

    /* Synth render rate.  Driven by HwAudio: this is the chip-
     * internal rate `max(131072, 32768 << sampling_cycle)` — 131072 Hz
     * for SOUNDBIAS sampling_cycle 0/1/2; 262144 Hz for sampling_cycle
     * 3 — NOT the host rate.  The polyphase resampler downstream
     * converts render-rate output to host rate. */
    float    render_rate;

    /* Shared 512 Hz PSG frame sequencer.  Mirrors mGBA GBA-mode
     * frame ownership: one chip-internal sequencer clocks length
     * (0/2/4/6), SQ1 sweep (2/6), and envelope (7).  Phase 1 exposes
     * debug counters only; later length/sweep/envelope implementations
     * should attach to these hook points. */
    uint8_t  frame_seq_step;
    double   frame_seq_accum;
    uint64_t frame_seq_ticks;
    uint64_t frame_seq_length_ticks;
    uint64_t frame_seq_sweep_ticks;
    uint64_t frame_seq_envelope_ticks;
} HwPsgSynth;

void hw_psg_init(HwPsgSynth *psg, float render_rate);
void hw_psg_set_render_rate(HwPsgSynth *psg, float render_rate);

typedef struct {
    uint8_t  frame_step;
    double   frame_accum;
    uint64_t frame_ticks;
    uint64_t length_ticks;
    uint64_t sweep_ticks;
    uint64_t envelope_ticks;
} HwPsgFrameSequencerDebug;

void hw_psg_get_frame_sequencer_debug(const HwPsgSynth *psg,
                                      HwPsgFrameSequencerDebug *out);

/* Apply one M4ARegWrite event to the synth state.  Decodes the raw
 * NRxx byte payload into the relevant channel-state fields per real
 * GB hardware register layout.  NR50/51 and SOUNDCNT_H PSG vol bits
 * land on HwMixBus, NOT on the synth — see hw_mix.h. */
void hw_psg_apply_event(HwPsgSynth *psg, const M4ARegWrite *ev);

/* Render `frames` host-rate per-channel mono samples into the four
 * provided buffers.  Each output is the channel's pre-mix UNIPOLAR
 * audio in [0, env_vol/15] (square / noise) or [0, wave_factor]
 * (wave) — no per-channel headroom budget; the mix bus owns final
 * gain.  Buffers are OVERWRITTEN, not summed — the channel's
 * contribution is exclusive.  Pass NULL for any buffer the caller
 * doesn't need (still advances phase + LFSR state).
 *
 * Unipolar synth mirrors mGBA GBA-mode `GBAudioSamplePSG`
 * (gb_audio.c:743) which uses `dcOffset = 0` and unsigned channel
 * samples; the positive PSG DC then leaks through `_applyBias` to
 * the signed full-mix output.  Earlier poryaaaa revisions used
 * dipolar ±env_vol/15 synth — that broke per-channel + full-mix DC
 * parity against mGBA captures (~3.4% full-scale on littleroot_test).
 *
 * NR52 master-disable zeros every channel's output here at the synth
 * stage, mirroring real GB's powered-down-DAC behaviour. */
void hw_psg_render(HwPsgSynth *psg,
                   float *out_sq1,
                   float *out_sq2,
                   float *out_wave,
                   float *out_noise,
                   int frames);

#ifdef __cplusplus
}
#endif

#endif
