#include "hw_psg.h"

#include <string.h>

#if defined(__clang__) || defined(__GNUC__)
#define HW_PSG_FALLTHROUGH __attribute__((fallthrough))
#else
#define HW_PSG_FALLTHROUGH
#endif

/* GB square duty patterns.  Top 3 bits of the 32-bit phase index into
 * an 8-bit pattern; bit value (0/1) → ±amplitude.  Patterns chosen to
 * match real GB output (bit 0 = first emitted sample). */
static const uint8_t kDutyPatterns[4] = {
    0x01,  /* 12.5%: 0000_0001 */
    0x81,  /* 25%:   1000_0001 */
    0x87,  /* 50%:   1000_0111 */
    0x7E,  /* 75%:   0111_1110 */
};

#define HW_PSG_FRAME_SEQ_HZ 512.0

static void hw_psg_reset_frame_sequencer(HwPsgSynth *psg, uint8_t step) {
    psg->frame_seq_step = (uint8_t)(step & 7u);
    psg->frame_seq_accum = 0.0;
    psg->frame_seq_ticks = 0;
    psg->frame_seq_length_ticks = 0;
    psg->frame_seq_sweep_ticks = 0;
    psg->frame_seq_envelope_ticks = 0;
}

static void hw_psg_frame_length(HwPsgSynth *psg) {
    psg->frame_seq_length_ticks++;
}

static bool hw_psg_update_sq1_sweep(HwPsgSynth *psg, bool initial) {
    if (initial || psg->sq1_sweep_time != 8) {
        int freq = psg->sq1_sweep_shadow_freq;
        int delta = freq >> psg->sq1_sweep_shift;

        if (psg->sq1_sweep_decrease) {
            freq -= delta;
            if (!initial && freq >= 0) {
                psg->sq1_sweep_shadow_freq = (uint16_t)freq;
                psg->sq1_freq = (uint16_t)freq;
            }
        } else {
            freq += delta;
            if (freq >= 2048) {
                return false;
            }
            if (!initial && psg->sq1_sweep_shift) {
                psg->sq1_sweep_shadow_freq = (uint16_t)freq;
                psg->sq1_freq = (uint16_t)freq;
                if (!hw_psg_update_sq1_sweep(psg, true)) {
                    return false;
                }
            }
        }
        psg->sq1_sweep_occurred = true;
    }
    psg->sq1_sweep_timer = psg->sq1_sweep_time;
    return true;
}

static void hw_psg_frame_sweep(HwPsgSynth *psg) {
    psg->frame_seq_sweep_ticks++;
    if (!psg->sq1_sweep_enabled) return;
    if (psg->sq1_sweep_timer > 0) {
        psg->sq1_sweep_timer--;
    }
    if (psg->sq1_sweep_timer == 0) {
        if (!hw_psg_update_sq1_sweep(psg, false)) {
            psg->sq1_enabled = false;
        }
    }
}

static void hw_psg_frame_envelope(HwPsgSynth *psg) {
    psg->frame_seq_envelope_ticks++;
}

static void hw_psg_tick_frame_sequencer(HwPsgSynth *psg) {
    psg->frame_seq_step = (uint8_t)((psg->frame_seq_step + 1u) & 7u);
    psg->frame_seq_ticks++;

    switch (psg->frame_seq_step) {
	case 2:
	case 6:
		hw_psg_frame_sweep(psg);
		HW_PSG_FALLTHROUGH;
	case 0:
    case 4:
        hw_psg_frame_length(psg);
        break;
    case 7:
        hw_psg_frame_envelope(psg);
        break;
    default:
        break;
    }
}

static void hw_psg_advance_frame_sequencer(HwPsgSynth *psg) {
    if (psg->render_rate <= 0.0f) return;
    psg->frame_seq_accum += HW_PSG_FRAME_SEQ_HZ / (double)psg->render_rate;
    while (psg->frame_seq_accum >= 1.0) {
        psg->frame_seq_accum -= 1.0;
        hw_psg_tick_frame_sequencer(psg);
    }
}

static void hw_psg_clear_channel_state(HwPsgSynth *psg) {
    psg->sq1_phase = 0;
    psg->sq2_phase = 0;
    psg->wave_phase = 0;

    psg->sq1_freq = 0;
    psg->sq2_freq = 0;
    psg->wave_freq = 0;

    psg->sq1_sweep_shadow_freq = 0;
    psg->sq1_sweep_time = 8;
    psg->sq1_sweep_shift = 0;
    psg->sq1_sweep_timer = 0;
    psg->sq1_sweep_decrease = false;
    psg->sq1_sweep_enabled = false;
    psg->sq1_sweep_occurred = false;

    psg->sq1_duty = 0;
    psg->sq2_duty = 0;

    psg->sq1_env_vol = 0;
    psg->sq2_env_vol = 0;
    psg->wave_vol_code = 0;

    psg->sq1_dac_enabled = false;
    psg->sq1_enabled = false;
    psg->sq2_enabled = false;
    psg->wave_enabled = false;
    psg->wave_dac_on = false;

    psg->noise_lfsr = 0;
    psg->noise_phase = 0;
    psg->noise_clock_shift = 0;
    psg->noise_divisor_code = 0;
    psg->noise_last_sample = 0;
    psg->noise_width_7bit = false;
    psg->noise_env_vol = 0;
    psg->noise_enabled = false;
}

/* Convert the 11-bit GB freq word + audio-rate constant + render_rate
 * into a 32-bit phase increment per render-rate sample.  audio_freq_hz
 * = RATE_NUM / (2048 - F); phase_inc = audio_hz / render_rate × 2^32.
 *
 * Step 9 sets render_rate to a fixed chip-internal rate (131072 Hz)
 * well above any host Nyquist, so PSG synth no longer aliases against
 * host rate.  The polyphase resampler in hw_resample.c band-limits at
 * host_rate/2 when downsampling to host.  SOUNDBIAS sampling_cycle
 * variation (max(131072, quirk_rate) per plan §7b) is still pending
 * — see hw_psg.h header banner. */
static uint32_t phase_inc_from_freq(uint16_t freq_word, float rate_num, float render_rate) {
    int denom = 2048 - (int)(freq_word & 0x07FF);
    if (denom <= 0) denom = 1;
    if (render_rate <= 0.0f) return 0;
    double audio_hz = (double)rate_num / (double)denom;
    double inc = audio_hz / (double)render_rate * 4294967296.0;
    if (inc < 0.0) inc = 0.0;
    if (inc > 4294967295.0) inc = 4294967295.0;
    return (uint32_t)inc;
}

void hw_psg_init(HwPsgSynth *psg, float render_rate) {
    memset(psg, 0, sizeof(*psg));
    psg->render_rate = render_rate;
    psg->sq1_sweep_time = 8;
    hw_psg_reset_frame_sequencer(psg, 0);
    /* Match the driver's register-file defaults (m4a_driver_create) so
     * the chip starts in a "configured" state matching what real m4a
     * writes during init: NR52 master-enable on (NR50/NR51/SOUNDCNT_H
     * defaults are owned by HwMixBus, see hw_mix_init). */
    psg->master_enabled    = true;
    /* LFSR resets to 0 (matches mGBA gb_audio.c:374); reloaded on each
     * NR44 trigger.  noise_enabled stays false until trigger anyway. */
    psg->noise_lfsr        = 0;
}

void hw_psg_set_render_rate(HwPsgSynth *psg, float render_rate) {
    psg->render_rate = render_rate;
}

void hw_psg_get_frame_sequencer_debug(const HwPsgSynth *psg,
                                      HwPsgFrameSequencerDebug *out) {
    if (!out) return;
    out->frame_step = psg->frame_seq_step;
    out->frame_accum = psg->frame_seq_accum;
    out->frame_ticks = psg->frame_seq_ticks;
    out->length_ticks = psg->frame_seq_length_ticks;
    out->sweep_ticks = psg->frame_seq_sweep_ticks;
    out->envelope_ticks = psg->frame_seq_envelope_ticks;
}

/* Convert NR32 byte → linear gain in [0..1].  Real GB shifts the wave
 * sample right by code; we mirror that with a fixed factor table.
 *   0x00       → mute
 *   bits 6-5 = 01 (0x20) → 100% (no shift)
 *   bits 6-5 = 10 (0x40) →  50% (>>1)
 *   bits 6-5 = 11 (0x60) →  25% (>>2)
 *   bit 7 = 1 (0x80)      →  75% (specific GBA mode) */
static float wave_vol_factor(uint8_t code) {
    if (code == 0) return 0.0f;
    if (code & 0x80) return 0.75f;
    switch ((code >> 5) & 3) {
    case 1: return 1.00f;
    case 2: return 0.50f;
    case 3: return 0.25f;
    default: return 0.0f;
    }
}

void hw_psg_apply_event(HwPsgSynth *psg, const M4ARegWrite *ev) {
    uint32_t v = ev->value;
    switch (ev->reg) {

    /* ---- Square 1 (NR10..NR14) ---- */
    case M4A_REG_NR10:
    {
        bool old_decrease = psg->sq1_sweep_decrease;
        psg->sq1_sweep_shift = (uint8_t)(v & 0x07);
        psg->sq1_sweep_decrease = (v & 0x08) != 0;
        if (psg->sq1_sweep_occurred && old_decrease && !psg->sq1_sweep_decrease) {
            psg->sq1_enabled = false;
        }
        psg->sq1_sweep_occurred = false;
        psg->sq1_sweep_time = (uint8_t)((v >> 4) & 0x07);
        if (!psg->sq1_sweep_time) psg->sq1_sweep_time = 8;
        break;
    }
    case M4A_REG_NR11:
        psg->sq1_duty = (uint8_t)((v >> 6) & 0x03);
        break;
    case M4A_REG_NR12:
        psg->sq1_env_vol = (uint8_t)((v >> 4) & 0x0F);
        psg->sq1_dac_enabled = (v & 0xF8) != 0;
        if (!psg->sq1_dac_enabled) psg->sq1_enabled = false;  /* NRx2 == 0 → DAC off */
        break;
    case M4A_REG_NR13:
        psg->sq1_freq = (uint16_t)((psg->sq1_freq & 0x0700) | (v & 0xFF));
        break;
    case M4A_REG_NR14:
        psg->sq1_freq = (uint16_t)((psg->sq1_freq & 0x00FF) | ((v & 0x07) << 8));
        if (v & 0x80) {
            /* NRx4 trigger: re-arm (envelope already loaded by NR12; phase
             * is preserved per real GB hardware). */
            if (psg->sq1_dac_enabled) psg->sq1_enabled = true;
            psg->sq1_sweep_shadow_freq = psg->sq1_freq;
            psg->sq1_sweep_timer = psg->sq1_sweep_time;
            psg->sq1_sweep_enabled = (psg->sq1_sweep_timer != 8)
                || psg->sq1_sweep_shift;
            psg->sq1_sweep_occurred = false;
            if (psg->sq1_enabled && psg->sq1_sweep_shift) {
                if (!hw_psg_update_sq1_sweep(psg, true)) {
                    psg->sq1_enabled = false;
                }
            }
        }
        break;

    /* ---- Square 2 (NR21..NR24) ---- */
    case M4A_REG_NR21:
        psg->sq2_duty = (uint8_t)((v >> 6) & 0x03);
        break;
    case M4A_REG_NR22:
        psg->sq2_env_vol = (uint8_t)((v >> 4) & 0x0F);
        psg->sq2_enabled = (psg->sq2_env_vol != 0) || ((v & 0x08) != 0);
        if ((v & 0xF8) == 0) psg->sq2_enabled = false;
        break;
    case M4A_REG_NR23:
        psg->sq2_freq = (uint16_t)((psg->sq2_freq & 0x0700) | (v & 0xFF));
        break;
    case M4A_REG_NR24:
        psg->sq2_freq = (uint16_t)((psg->sq2_freq & 0x00FF) | ((v & 0x07) << 8));
        if (v & 0x80) {
            if (psg->sq2_env_vol != 0) psg->sq2_enabled = true;
        }
        break;

    /* ---- Wave (NR30..NR34) + wave RAM ---- */
    case M4A_REG_NR30:
        psg->wave_dac_on = (v & 0x80) != 0;
        if (!psg->wave_dac_on) psg->wave_enabled = false;
        break;
    case M4A_REG_NR31:
        /* Length-load — not implemented yet (length-counter is its own
         * subsystem, deferred). */
        (void)v;
        break;
    case M4A_REG_NR32:
        psg->wave_vol_code = (uint8_t)v;
        break;
    case M4A_REG_NR33:
        psg->wave_freq = (uint16_t)((psg->wave_freq & 0x0700) | (v & 0xFF));
        break;
    case M4A_REG_NR34:
        psg->wave_freq = (uint16_t)((psg->wave_freq & 0x00FF) | ((v & 0x07) << 8));
        if (v & 0x80) {
            /* Wave channel trigger: real GB resets wave position to 0. */
            psg->wave_phase = 0;
            if (psg->wave_dac_on) psg->wave_enabled = true;
        }
        break;
    case M4A_REG_WAVE_RAM_BYTE: {
        uint32_t addr = (v >> 8) & 0x0F;
        uint8_t  byte = (uint8_t)(v & 0xFF);
        psg->wave_ram[addr] = byte;
        break;
    }

    /* ---- Noise (NR41..NR44) ---- */
    case M4A_REG_NR41:
        /* Length-load (bits 5-0).  Length-counter not implemented yet
         * (deferred subsystem); m4a always writes pace=0 and never
         * relies on the chip's hardware length counter. */
        (void)v;
        break;
    case M4A_REG_NR42:
        psg->noise_env_vol = (uint8_t)((v >> 4) & 0x0F);
        /* DAC gating: NRx2 with top 5 bits all zero disables the channel
         * (env vol = 0 AND direction = 0).  Mirrors square channels. */
        if ((v & 0xF8) == 0) psg->noise_enabled = false;
        break;
    case M4A_REG_NR43:
        psg->noise_clock_shift  = (uint8_t)((v >> 4) & 0x0F);
        psg->noise_width_7bit   = (v & 0x08) != 0;
        psg->noise_divisor_code = (uint8_t)(v & 0x07);
        break;
    case M4A_REG_NR44:
        if (v & 0x80) {
            /* NR44 trigger: reset LFSR to 0 (mirrors mGBA gb_audio.c:374
             * GBAudioWriteNR44).  Note: 0 is the canonical reset, NOT
             * 0x7FFF — under the mGBA polynomial `(lfsr ^ (lfsr>>1) ^ 1)
             * & 1` the all-ones state is a fixed point.  v1 used a
             * different polynomial (no `^ 1`) so 0x7FFF worked there, but
             * for v2 we mirror mGBA's GBA-mode path verbatim. */
            psg->noise_lfsr        = 0;
            psg->noise_last_sample = 0;
            if (psg->noise_env_vol != 0) psg->noise_enabled = true;
        }
        break;

    /* NR52 master-enable gates the channel DACs at the synth stage
     * (real GB powers down the DAC paths when this bit is clear).
     * NR50, NR51, SOUNDCNT_H PSG vol bits, and SOUNDBIAS land on the
     * mix-bus stage (HwMixBus), not here — see hw_mix.h. */
    case M4A_REG_NR52:
        if ((v & 0x80) == 0) {
            if (psg->master_enabled) {
                psg->master_enabled = false;
                hw_psg_clear_channel_state(psg);
                hw_psg_reset_frame_sequencer(psg, 7);
            }
        } else if (!psg->master_enabled) {
            psg->master_enabled = true;
            hw_psg_reset_frame_sequencer(psg, 7);
        }
        break;
    default:
        break;
    }
}

void hw_psg_render(HwPsgSynth *psg,
                   float *out_sq1,
                   float *out_sq2,
                   float *out_wave,
                   float *out_noise,
                   int frames) {
    if (frames <= 0) return;
    if (!psg->master_enabled) {
        /* NR52 master-disable: powered-down DACs.  Zero all outputs. */
        if (out_sq1)   memset(out_sq1,   0, (size_t)frames * sizeof(float));
        if (out_sq2)   memset(out_sq2,   0, (size_t)frames * sizeof(float));
        if (out_wave)  memset(out_wave,  0, (size_t)frames * sizeof(float));
        if (out_noise) memset(out_noise, 0, (size_t)frames * sizeof(float));
        return;
    }

    uint32_t sq2_inc = (psg->sq2_enabled && psg->sq2_freq < 2048)
        ? phase_inc_from_freq(psg->sq2_freq, 131072.0f, psg->render_rate) : 0;
    uint32_t wave_inc = (psg->wave_enabled && psg->wave_dac_on && psg->wave_freq < 2048)
        ? phase_inc_from_freq(psg->wave_freq, 65536.0f, psg->render_rate) : 0;

    /* Noise timer: noise_freq_hz = 524288 / divisor / 2^(shift+1), where
     * divisor = (code == 0 ? 0.5 : code).  Convert to clocks-per-host-sample,
     * split into whole-clocks (advanced unconditionally) + fractional
     * (advanced when noise_phase overflows).  At render rate (131072 Hz)
     * noise_freq can still exceed Nyquist by ~4× — we step the LFSR
     * through every whole clock but only sample the latest LSB per
     * output frame (no averaging, per §12.6 gate).  The downstream
     * polyphase resampler then band-limits at host_rate/2. */
    int      noise_whole_clocks = 0;
    uint32_t noise_phase_inc    = 0;
    if (psg->noise_enabled && psg->render_rate > 0.0f) {
        float divisor = (psg->noise_divisor_code == 0)
            ? 0.5f : (float)psg->noise_divisor_code;
        float noise_hz = 524288.0f / divisor
                        / (float)(1u << (psg->noise_clock_shift + 1u));
        double clocks_per_sample = (double)noise_hz / (double)psg->render_rate;
        if (clocks_per_sample < 0.0) clocks_per_sample = 0.0;
        noise_whole_clocks = (int)clocks_per_sample;
        double frac = clocks_per_sample - (double)noise_whole_clocks;
        if (frac < 0.0) frac = 0.0;
        if (frac > 1.0) frac = 1.0;
        noise_phase_inc = (uint32_t)(frac * 4294967296.0);
    }

    float wave_factor = wave_vol_factor(psg->wave_vol_code);

    for (int i = 0; i < frames; i++) {
        uint32_t sq1_inc = (psg->sq1_enabled && psg->sq1_freq < 2048)
            ? phase_inc_from_freq(psg->sq1_freq, 131072.0f, psg->render_rate) : 0;

        /* Square 1 — mGBA GBA-mode unipolar.  In gb_audio.c
         * `GBAudioSamplePSG` the GBA path uses `dcOffset = 0` and each
         * `audio->chN.sample` is the unsigned current channel value
         * (square: env_vol when duty bit is high, 0 when low).  We
         * mirror that as a [0, env_vol/15] float here so the chip
         * output has the positive DC offset that real hardware leaks
         * through `_applyBias`.  bipolar synth was a poryaaaa
         * simplification that broke per-channel + full-mix DC parity
         * against mGBA captures (~3.4% full-scale on littleroot_test). */
        if (out_sq1) {
            float s = 0.0f;
            if (sq1_inc) {
                uint8_t pos = (uint8_t)(psg->sq1_phase >> 29);
                float bit = (kDutyPatterns[psg->sq1_duty] >> pos) & 1u
                    ? 1.0f : 0.0f;
                s = bit * (psg->sq1_env_vol / 15.0f);
            }
            out_sq1[i] = s;
        }
        if (sq1_inc) psg->sq1_phase += sq1_inc;

        /* Square 2 — same unipolar convention as Square 1. */
        if (out_sq2) {
            float s = 0.0f;
            if (sq2_inc) {
                uint8_t pos = (uint8_t)(psg->sq2_phase >> 29);
                float bit = (kDutyPatterns[psg->sq2_duty] >> pos) & 1u
                    ? 1.0f : 0.0f;
                s = bit * (psg->sq2_env_vol / 15.0f);
            }
            out_sq2[i] = s;
        }
        if (sq2_inc) psg->sq2_phase += sq2_inc;

        /* Wave — mGBA GBA-mode unipolar.  Wave RAM nibble is the
         * unsigned 4-bit value (0..15); applying the NR32 volume
         * shift (wave_factor: 0=mute, 1=100%, 2=50%, 3=25%, 4=75%)
         * scales it.  Output is unsigned [0, wave_factor]. */
        if (out_wave) {
            float s = 0.0f;
            if (wave_inc) {
                uint8_t pos = (uint8_t)(psg->wave_phase >> 27);  /* 0..31 */
                uint8_t byte = psg->wave_ram[pos >> 1];
                uint8_t nib = (pos & 1) ? (byte & 0x0F) : ((byte >> 4) & 0x0F);
                s = ((float)nib / 15.0f) * wave_factor;
            }
            out_wave[i] = s;
        }
        if (wave_inc) psg->wave_phase += wave_inc;

        /* Noise — mGBA GBA-mode "current LFSR sample shifted" model.
         * Mirrors gb_audio.c:631-637 verbatim: per clock,
         *   lsb  = (lfsr ^ (lfsr>>1) ^ 1) & 1
         *   lfsr >>= 1
         *   if (lsb) lfsr |= coeff   else lfsr &= ~coeff
         * with coeff = 0x4000 (15-bit) or 0x40 (7-bit).  In mGBA
         * GBA-mode `audio->ch4.sample = lsb * envelope.currentVolume`
         * (gb_audio.c:641) — unsigned 0..env_vol.  Mirror with
         * unipolar [0, env_vol/15] float here so noise DC matches
         * mGBA captures.  No sub-sample averaging — §12.6 gate
         * decision. */
        if (psg->noise_enabled) {
            int extra = 0;
            uint32_t prev_phase = psg->noise_phase;
            psg->noise_phase += noise_phase_inc;
            if (psg->noise_phase < prev_phase) extra = 1;
            int clocks = noise_whole_clocks + extra;
            uint16_t coeff = psg->noise_width_7bit ? 0x0040u : 0x4000u;
            for (int c = 0; c < clocks; c++) {
                uint16_t lsb = (uint16_t)((psg->noise_lfsr
                                       ^ (psg->noise_lfsr >> 1)
                                       ^ 1u) & 1u);
                psg->noise_lfsr >>= 1;
                if (lsb) psg->noise_lfsr = (uint16_t)(psg->noise_lfsr | coeff);
                else     psg->noise_lfsr = (uint16_t)(psg->noise_lfsr & (uint16_t)~coeff);
                psg->noise_last_sample = (uint8_t)lsb;
            }
            if (out_noise) {
                float bit = psg->noise_last_sample ? 1.0f : 0.0f;
                out_noise[i] = bit * ((float)psg->noise_env_vol / 15.0f);
            }
        } else if (out_noise) {
            out_noise[i] = 0.0f;
        }

        /* mGBA samples PSG output before running the frame event.  Keep
         * the tick at the end of the internal sample so future audible
         * length/sweep/envelope hooks affect the following sample, not
         * the boundary sample that preceded the frame event. */
        hw_psg_advance_frame_sequencer(psg);
    }
}
