#ifndef HW_RESAMPLE_H
#define HW_RESAMPLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Polyphase windowed-sinc resampler (§12 step 9).
 *
 * Resamples a stereo float stream from a fixed input (chip-internal)
 * rate to an arbitrary output (host) rate.  Used to bridge the
 * SOUNDBIAS-derived chip-internal cadence to the host audio output.
 *
 * Algorithm: sinc filter cut at min(input_rate, output_rate)/2,
 * Hann-windowed, decomposed into NPHASES fractional-delay kernels.
 * Each output sample picks the kernel for its fractional input
 * position and convolves with the most recent TAPS input samples.
 *
 * State persists across calls — the ring buffer of recent inputs and
 * the fractional output position survive between hw_audio render
 * calls so consecutive blocks knit together without phase resets.
 *
 * Latency ≈ TAPS/2 input samples = ~16/131072 s = ~120 µs.  At
 * session start, the first ~16/host_rate-step host samples are
 * effectively zero-padded; thereafter steady-state. */

#define HW_RESAMPLE_TAPS    32
#define HW_RESAMPLE_PHASES  64

typedef struct {
    /* Pre-computed polyphase kernel.  kernel[k][t] is the t-th tap of
     * the kernel for fractional output phase k/NPHASES. */
    float kernel[HW_RESAMPLE_PHASES][HW_RESAMPLE_TAPS];

    /* Ring buffer of last TAPS input samples (stereo). */
    float ring_l[HW_RESAMPLE_TAPS];
    float ring_r[HW_RESAMPLE_TAPS];
    int   ring_head;       /* next write index in ring (0..TAPS-1) */
    int   ring_count;      /* valid samples in ring (0..TAPS) */

    /* Phase tracking: input_idx counts inputs pushed since the last
     * renormalization; output_pos tracks the next output's position
     * in input-sample units (continuous, fractional).  Both are
     * renormalized periodically to bound float precision drift. */
    int64_t input_idx;
    double  output_pos;

    /* Rate ratio: input_rate / output_rate.  Each output advances
     * output_pos by step.  step ≥ 1 for downsampling (typical chip
     * → host case), < 1 for upsampling (rare; only if host > internal). */
    double  step;
} HwResample;

/* Initialize the resampler with the given input/output rates.
 * Builds the kernel and zeroes the ring; phase initialized so the
 * first output corresponds to ~TAPS/2 inputs of latency. */
void hw_resample_init(HwResample *rs, double input_rate, double output_rate);

/* Reconfigure rates.  Rebuilds the kernel; ring is preserved (so
 * mid-render rate changes don't pop), but consumers should generally
 * call this only at hw_audio_set_host_rate boundaries. */
void hw_resample_set_rates(HwResample *rs, double input_rate, double output_rate);

/* Feed `in_n` input samples; write up to `max_out` output samples.
 * Returns the actual number of outputs produced (≤ max_out, possibly
 * 0 during initial latency).  Caller must size out_l/out_r for at
 * least max_out floats; either may be NULL to discard one side. */
int  hw_resample_process(HwResample *rs,
                         const float *in_l, const float *in_r, int in_n,
                         float *out_l, float *out_r, int max_out);

#ifdef __cplusplus
}
#endif

#endif
