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
     *   NR44 trigger resets the GBA noise LFSR and its clock origin to zero.
     *
     * Noise follows pinned mGBA GBA-mode feedback. Each clock derives
     * `bit0 ^ bit1 ^ 1`, shifts right, writes that feedback to bit 14
     * (and bit 6 in 7-bit mode), and exposes the feedback bit as output.
     * No sub-sample averaging is applied; the downstream current mGBA sinc
     * frontend consumes the DAC samples.
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
        uint16_t noise_lfsr;           /* GBA feedback register; NR44 seed is zero */
        uint32_t noise_timer_cycles;   /* unconsumed cycles from the latest NR44 clock origin */
        uint64_t noise_pending_cycles; /* lazy cycles retained until mGBA runs channel 4 */
        uint8_t noise_clock_shift;     /* NR43 bits 7-4 */
        uint8_t noise_divisor_code;    /* NR43 bits 2-0 */
        uint8_t noise_last_sample;     /* latest mGBA feedback bit */
        bool noise_width_7bit;         /* NR43 bit 3 */
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

        /* Shared 512 Hz PSG frame sequencer. Its event phase is fixed to
         * reset-time 32,768-cycle boundaries even while NR52 is disabled.
         * Re-enabling NR52 sets step 7 without rebasing that phase. */
        uint8_t frame_seq_step;
        double frame_seq_accum; /* debug view of frame_seq_cycle_remainder */
        uint16_t frame_seq_cycle_remainder;
        uint64_t frame_seq_ticks;
        uint64_t frame_seq_length_ticks;
        uint64_t frame_seq_sweep_ticks;
        uint64_t frame_seq_envelope_ticks;
    } HwPsgSynth;

    void hw_psg_init(HwPsgSynth* psg, float render_rate);
    void hw_psg_set_render_rate(HwPsgSynth* psg, float render_rate);

    /* Advance an exact GBA-cycle span and clock only the channels mGBA would
     * run for the current event. This preserves the shared absolute frame
     * cadence across trace and live rendering. */
    void hw_psg_advance_cycles(
        HwPsgSynth* psg, uint64_t cycles, bool clock_sq1, bool clock_sq2, bool clock_wave, bool clock_noise);

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

    /* Read current native mGBA GBA-mode PSG values [0, 15] without advancing. */
    void
    hw_psg_sample(const HwPsgSynth* psg, uint8_t* out_sq1, uint8_t* out_sq2, uint8_t* out_wave, uint8_t* out_noise);

    /* Render native per-channel values while advancing the compatibility
     * oscillator path. Buffers are overwritten; NULL still advances state. */
    void hw_psg_render(
        HwPsgSynth* psg, uint8_t* out_sq1, uint8_t* out_sq2, uint8_t* out_wave, uint8_t* out_noise, int frames);

#ifdef __cplusplus
}
#endif

#endif
