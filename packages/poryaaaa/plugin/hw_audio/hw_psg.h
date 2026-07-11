#ifndef HW_PSG_H
#define HW_PSG_H

#include <stdbool.h>
#include <stdint.h>

#include "m4a/m4a_driver.h" /* M4ARegWrite */

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        uint8_t initial_volume;
        uint8_t current_volume;
        uint8_t step_time;
        uint8_t next_step;
        uint8_t dead;
        bool direction;
    } HwPsgEnvelope;

    /* PSG synth state — sq1, sq2, wave, noise.  Mirrors the relevant subset
     * of mGBA gb_audio.c hardware state: envelope volume + duty/wave-RAM/
     * LFSR + clock state + DAC + pan masks.
     *
     * Frequency / phase model (per real GB hardware):
     *   audio_hz_square = 131072 / (2048 - F)
     *   audio_hz_wave   =  65536 / (2048 - F)
     *   audio_hz_noise  = 524288 / divisor / 2^(shift+1)   [divisor 0→0.5, 1..7→1..7]
     *   square duty index advances every 16 * (2048 - F) GBA CPU cycles
     *   and is preserved across both MO_PIT (NRx3+NRx4-no-trigger) and
     *     NRx4-with-trigger writes; a trigger does not reset that timer.
     *   wave RAM position resets to 0 on NR34-with-trigger (real GB GBATEK).
     *   noise LFSR resets to 0x7F or 0x7FFF on NR44-with-trigger.
     *   The ROM retriggers square/noise on envelope writes but limits wave
     *     retriggers to fresh notes so NR34 does not reset the wave position.
     *
     * Noise follows the linked mGBA 0.10.5 GBA-mode path.  Each clock emits
     * the old low bit, shifts right, and XORs that bit with 0x60 (7-bit) or
     * 0x6000 (15-bit).  No sub-sample averaging is applied; the downstream
     * mGBA blip frontend consumes the DAC steps.
     *
     * Synth runs at mGBA's SOUNDBIAS-selected DAC cadence, set by HwAudio
     * to `32768 << sampling_cycle`, not at the host rate. Reference
     * captures cover both square channels, all four duty patterns, normal
     * and alternate CGB voices, hardware envelopes, sweep, and noise.
     * Isolated stereo captures also verify left-only, right-only, and
     * centered routing against mGBA.
     *
     * These captures establish PSG voice parity, not whole-engine parity;
     * parser timing, PCM, wave, and reverb remain separate boundaries. */

    typedef struct
    {
        /* Per-channel runtime state */
        uint32_t sq1_timer_cycles;
        uint32_t sq2_timer_cycles;
        uint8_t sq1_duty_index;
        uint8_t sq2_duty_index;
        uint32_t wave_phase;

        uint16_t sq1_freq; /* 11-bit freq word */
        uint16_t sq2_freq;
        uint16_t wave_freq;

        uint16_t sq1_sweep_shadow_freq;
        uint8_t sq1_sweep_time; /* NR10 pace; 0 is stored as 8 */
        uint8_t sq1_sweep_shift;
        uint8_t sq1_sweep_timer;
        bool sq1_sweep_decrease;
        bool sq1_sweep_enabled;
        bool sq1_sweep_occurred;

        uint8_t sq1_duty; /* 0..3 */
        uint8_t sq2_duty;

        uint16_t sq1_length_counter;
        uint16_t sq2_length_counter;
        bool sq1_length_enabled;
        bool sq2_length_enabled;

        HwPsgEnvelope sq1_envelope;
        HwPsgEnvelope sq2_envelope;
        uint8_t wave_vol_code; /* NR32 byte */

        bool sq1_dac_enabled;
        bool sq1_enabled;
        bool sq2_enabled;
        bool wave_enabled;
        bool wave_dac_on; /* NR30 bit 7 */
        uint16_t wave_length_counter;
        bool wave_length_enabled;

        uint8_t wave_ram[16];

        /* Noise (NR41..NR44) */
        uint16_t noise_lfsr;        /* shift register; width-specific NR44 reset */
        uint32_t noise_phase;       /* fractional-clocks accumulator (2^32 = 1 clock) */
        uint8_t noise_clock_shift;  /* NR43 bits 7-4 */
        uint8_t noise_divisor_code; /* NR43 bits 2-0 */
        uint8_t noise_last_sample;  /* last emitted old LFSR low bit */
        bool noise_width_7bit;      /* NR43 bit 3 */
        HwPsgEnvelope noise_envelope;
        bool noise_enabled;
        uint16_t noise_length_counter;
        bool noise_length_enabled;

        /* NR52 bit 7 — master enable.  When false all PSG channels silent.
         * Owned here (not by mix bus) because it gates the synth itself in
         * real GB hardware: when NR52 master is off the channels' DAC paths
         * are powered down, not just muted at the mix stage. */
        bool master_enabled;

        /* Synth render rate. Driven by HwAudio: this is
         * `32768 << sampling_cycle`, not the host rate. The mGBA blip
         * frontend downstream converts DAC-rate steps to the host rate. */
        float render_rate;

        /* Shared 512 Hz PSG frame sequencer.  Mirrors mGBA GBA-mode
         * frame ownership: one chip-internal sequencer clocks length
         * (0/2/4/6), SQ1 sweep (2/6), and envelope (7). */
        uint8_t frame_seq_step;
        double frame_seq_accum;
        uint64_t frame_seq_ticks;
        uint64_t frame_seq_length_ticks;
        uint64_t frame_seq_sweep_ticks;
        uint64_t frame_seq_envelope_ticks;
    } HwPsgSynth;

    void hw_psg_init(HwPsgSynth* psg, float render_rate);
    void hw_psg_set_render_rate(HwPsgSynth* psg, float render_rate);

    typedef struct
    {
        uint8_t frame_step;
        double frame_accum;
        uint64_t frame_ticks;
        uint64_t length_ticks;
        uint64_t sweep_ticks;
        uint64_t envelope_ticks;
    } HwPsgFrameSequencerDebug;

    void hw_psg_get_frame_sequencer_debug(const HwPsgSynth* psg, HwPsgFrameSequencerDebug* out);

    /* Apply one M4ARegWrite event to the synth state.  Decodes the raw
     * NRxx byte payload into the relevant channel-state fields per real
     * GB hardware register layout.  NR50/51 and SOUNDCNT_H PSG vol bits
     * land on HwMixBus, NOT on the synth — see hw_mix.h. */
    void hw_psg_apply_event(HwPsgSynth* psg, const M4ARegWrite* ev);

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
     * samples; the positive PSG DC passes through `_applyBias` into the
     * raw mix before HwAudio's mGBA-style frontend high-pass removes it.
     * Earlier poryaaaa revisions used dipolar ±env_vol/15 synthesis,
     * which changed the signal before the GBA mix and clip stages.
     *
     * NR52 master-disable zeros every channel's output here at the synth
     * stage, mirroring real GB's powered-down-DAC behaviour. */
    void hw_psg_render(HwPsgSynth* psg, float* out_sq1, float* out_sq2, float* out_wave, float* out_noise, int frames);

#ifdef __cplusplus
}
#endif

#endif
