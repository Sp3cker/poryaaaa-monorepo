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
     * Frequency / clock model (per real GBA hardware):
     *   audio_hz_square = 131072 / (2048 - F)
     *   audio_hz_wave   =  65536 / (2048 - F)
     *   audio_hz_noise  = 524288 / divisor / 2^(shift+1)   [divisor 0→0.5, 1..7→1..7]
     *   square duty index advances every 16 * (2048 - F) GBA CPU cycles
     *   and is preserved across both MO_PIT (NRx3+NRx4-no-trigger) and
     *     NRx4-with-trigger writes; a trigger does not reset that timer.
     *   GBA wave clocks rotate the selected 32- or 64-nibble bank every
     *     8 * (2048 - F) cycles. NR34 leaves the prior sample latched and
     *     schedules the first clock 24 cycles after one full wave period.
     *   Wave RAM writes target the bank opposite NR30's playback bank.
     *   noise LFSR resets to 0x7F or 0x7FFF on NR44-with-trigger.
     *
     * Noise follows the linked mGBA 0.10.5 GBA-mode path.  Each clock emits
     * the old low bit, shifts right, and XORs that bit with 0x60 (7-bit) or
     * 0x6000 (15-bit).  No sub-sample averaging is applied; the downstream
     * current mGBA sinc frontend consumes the DAC samples.
     *
     * Synth runs at mGBA's SOUNDBIAS-selected DAC cadence, set by HwAudio
     * to `32768 << sampling_cycle`, not at the host rate. Reference
     * captures cover both square channels, all four duty patterns, normal
     * and alternate CGB voices, hardware envelopes, sweep, GBA wave-bank
     * timing, and noise. Isolated stereo captures also verify left-only,
     * right-only, and centered routing against mGBA.
     *
     * These captures establish native hardware-voice parity, not whole-engine
     * parity; parser timing and reverb remain separate boundaries. */

    typedef struct
    {
        /* Per-channel runtime state */
        uint64_t sq1_timer_cycles;
        uint64_t sq2_timer_cycles;
        uint8_t sq1_duty_index;
        uint8_t sq2_duty_index;
        uint32_t wave_cycles_until_update;
        uint64_t wave_pending_cycles;

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
        uint8_t wave_sample;

        bool sq1_dac_enabled;
        bool sq1_enabled;
        bool sq2_enabled;
        bool wave_enabled;
        bool wave_dac_on; /* NR30 bit 7 */
        bool wave_bank;   /* NR30 bit 6: playback bank */
        bool wave_size;   /* NR30 bit 5: both banks */
        uint16_t wave_length_counter;
        bool wave_length_enabled;

        uint8_t wave_ram[32];

        /* Noise (NR41..NR44) */
        uint16_t noise_lfsr;  /* shift register; width-specific NR44 reset */
        uint32_t noise_phase; /* fractional-clocks accumulator (2^32 = 1 clock) */
        uint32_t noise_timer_cycles;
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
         * `32768 << sampling_cycle`, not the host rate. The current mGBA sinc
         * frontend downstream converts DAC-rate samples to the host rate. */
        float render_rate;

        /* Shared 512 Hz PSG frame sequencer.  Mirrors mGBA GBA-mode
         * frame ownership: one chip-internal sequencer clocks length
         * (0/2/4/6), SQ1 sweep (2/6), and envelope (7). */
        uint8_t frame_seq_step;
        double frame_seq_accum;
        uint16_t frame_seq_cycle_remainder;
        uint64_t frame_seq_ticks;
        uint64_t frame_seq_length_ticks;
        uint64_t frame_seq_sweep_ticks;
        uint64_t frame_seq_envelope_ticks;
    } HwPsgSynth;

    void hw_psg_init(HwPsgSynth* psg, float render_rate);
    void hw_psg_set_render_rate(HwPsgSynth* psg, float render_rate);

    /* Accumulate exact trace cycles and clock only channels mGBA would
     * update for the current event. Normal host rendering does not use
     * this path. */
    void hw_psg_advance_cycles(HwPsgSynth* psg, uint64_t cycles, bool clock_sq1, bool clock_sq2, bool clock_wave);

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

    /* Read the current PSG DAC inputs without advancing any oscillator or
     * frame-sequencer state. Trace SAMPLE events use this exact observation. */
    void hw_psg_sample(const HwPsgSynth* psg, float* out_sq1, float* out_sq2, float* out_wave, float* out_noise);

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
     * raw mix and is preserved by the current mGBA frontend.
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
