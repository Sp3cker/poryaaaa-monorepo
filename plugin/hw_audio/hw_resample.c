#include "hw_resample.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Renormalize the input/output position bookkeeping when input_idx
 * grows past this threshold.  Keeps both fields well within double
 * precision so cumulative drift doesn't accumulate over long renders. */
#define HW_RESAMPLE_RENORM_THRESHOLD ((int64_t)1 << 30)

static void rs_build_kernel(HwResample *rs, double input_rate, double output_rate) {
    if (input_rate <= 0.0 || output_rate <= 0.0) {
        rs->step = 1.0;
        memset(rs->kernel, 0, sizeof(rs->kernel));
        return;
    }
    rs->step = input_rate / output_rate;

    /* Cut frequency in normalized input-rate units (1.0 = input
     * Nyquist).  Set to min(input,output)/input — for downsampling
     * (input>output) this is output/input <1 and band-limits at the
     * output Nyquist; for upsampling (input<output) it is 1 and
     * passes everything up to input Nyquist. */
    double cut = (output_rate < input_rate ? output_rate : input_rate) / input_rate;
    if (cut > 1.0) cut = 1.0;

    const int half = HW_RESAMPLE_TAPS / 2;

    for (int k = 0; k < HW_RESAMPLE_PHASES; k++) {
        double frac = (double)k / (double)HW_RESAMPLE_PHASES;   /* 0..1 */
        double row[HW_RESAMPLE_TAPS];
        double sum = 0.0;
        for (int t = 0; t < HW_RESAMPLE_TAPS; t++) {
            /* x = distance in input-sample units from output to this tap.
             * Tap t corresponds to input position (anchor - half + 1 + t)
             * for output at (anchor + frac).  x = (t - half + 1) - frac. */
            double x = (double)(t - half + 1) - frac;
            double s;
            if (x > -1e-12 && x < 1e-12) {
                s = cut;
            } else {
                s = sin(M_PI * cut * x) / (M_PI * x);
            }
            /* Hann window across taps. */
            double w = 0.5 - 0.5 * cos(2.0 * M_PI * (double)t
                                       / (double)(HW_RESAMPLE_TAPS - 1));
            row[t] = s * w;
            sum   += row[t];
        }
        /* Normalize so each sub-kernel has DC gain = 1.  Without this,
         * truncation + windowing leaves a small frac-dependent error
         * that shows up as tiny DC offset variation on constant inputs. */
        double inv_sum = (sum > 1e-12 || sum < -1e-12) ? 1.0 / sum : 0.0;
        for (int t = 0; t < HW_RESAMPLE_TAPS; t++) {
            rs->kernel[k][t] = (float)(row[t] * inv_sum);
        }
    }
}

void hw_resample_init(HwResample *rs, double input_rate, double output_rate) {
    memset(rs, 0, sizeof(*rs));
    rs_build_kernel(rs, input_rate, output_rate);
    /* Initial output position: TAPS/2 - 1 input units in.  After
     * pushing TAPS inputs we have a fully-populated ring covering
     * input positions 0..TAPS-1 and the first output (at position
     * TAPS/2 - 1) uses the entire ring. */
    rs->output_pos = (double)(HW_RESAMPLE_TAPS / 2 - 1);
}

void hw_resample_set_rates(HwResample *rs, double input_rate, double output_rate) {
    rs_build_kernel(rs, input_rate, output_rate);
}

int hw_resample_process(HwResample *rs,
                        const float *in_l, const float *in_r, int in_n,
                        float *out_l, float *out_r, int max_out) {
    if (in_n <= 0) return 0;
    /* max_out == 0 is a valid use: caller wants to push inputs into
     * the ring for future drains without producing any outputs now.
     * Don't early-return — fall through to the push loop with
     * produced ≥ max_out so the inner produce-while never fires. */

    const int half = HW_RESAMPLE_TAPS / 2;
    int produced = 0;

    for (int i = 0; i < in_n; i++) {
        /* Push input into ring (newest at ring_head, then advance). */
        rs->ring_l[rs->ring_head] = in_l ? in_l[i] : 0.0f;
        rs->ring_r[rs->ring_head] = in_r ? in_r[i] : 0.0f;
        rs->ring_head = (rs->ring_head + 1) % HW_RESAMPLE_TAPS;
        if (rs->ring_count < HW_RESAMPLE_TAPS) rs->ring_count++;
        rs->input_idx++;

        /* Drain outputs while we have enough lookahead.  Need input at
         * (anchor + half) to be available — i.e. input_idx ≥ anchor + half + 1.
         * Anchor = floor(output_pos). */
        while (produced < max_out) {
            int64_t anchor = (int64_t)floor(rs->output_pos);
            if (anchor + half + 1 > rs->input_idx) break;

            double frac = rs->output_pos - (double)anchor;
            int phase_idx = (int)(frac * (double)HW_RESAMPLE_PHASES);
            if (phase_idx >= HW_RESAMPLE_PHASES) phase_idx = HW_RESAMPLE_PHASES - 1;
            if (phase_idx < 0) phase_idx = 0;

            const float *kk = rs->kernel[phase_idx];
            float sum_l = 0.0f, sum_r = 0.0f;

            /* Convolve: tap t corresponds to input at position
             * anchor - half + 1 + t.  Map to ring: position of input I
             * is (ring_head + I - input_idx) mod TAPS, normalized
             * positive.  Skip taps whose input is missing (only
             * happens at session start before ring is filled). */
            for (int t = 0; t < HW_RESAMPLE_TAPS; t++) {
                int64_t I = anchor - (int64_t)half + 1 + (int64_t)t;
                if (I < 0 || I >= rs->input_idx) continue;
                if (rs->input_idx - I > (int64_t)HW_RESAMPLE_TAPS) continue;
                int64_t off = (int64_t)rs->ring_head + I - rs->input_idx;
                int ring_idx = (int)(((off % HW_RESAMPLE_TAPS) + HW_RESAMPLE_TAPS)
                                     % HW_RESAMPLE_TAPS);
                sum_l += rs->ring_l[ring_idx] * kk[t];
                sum_r += rs->ring_r[ring_idx] * kk[t];
            }

            if (out_l) out_l[produced] = sum_l;
            if (out_r) out_r[produced] = sum_r;
            produced++;
            rs->output_pos += rs->step;
        }

        /* Renormalize input_idx and output_pos when input_idx grows
         * large enough to threaten double precision (every ~2^30
         * inputs ≈ several hours at 131 kHz). */
        if (rs->input_idx > HW_RESAMPLE_RENORM_THRESHOLD) {
            int64_t shift = rs->input_idx - (int64_t)HW_RESAMPLE_TAPS;
            rs->input_idx -= shift;
            rs->output_pos -= (double)shift;
        }
    }

    return produced;
}
