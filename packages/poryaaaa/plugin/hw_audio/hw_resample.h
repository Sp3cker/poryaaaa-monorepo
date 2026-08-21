#ifndef HW_RESAMPLE_H
#define HW_RESAMPLE_H

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif

    enum
    {
        HW_RESAMPLE_GBA_CLOCK_HZ = 16777216u,
        HW_RESAMPLE_FRAME_CLOCKS = 0x800u,
        HW_RESAMPLE_BLIP_STORAGE_SAMPLES = 0x4000u,
        HW_RESAMPLE_HALF_WIDTH = 8u,
        HW_RESAMPLE_END_FRAME_EXTRA = 2u,
        HW_RESAMPLE_BLIP_BUFFER_EXTRA = 18u,
        HW_RESAMPLE_MAX_AUDIO_BUFFERS = 0x2000u,
        HW_RESAMPLE_AA_TAPS = 49u,
    };

    typedef struct
    {
        uint64_t factor;
        uint64_t offset;
        uint32_t available;
        int32_t integrator;
        int32_t samples[HW_RESAMPLE_BLIP_STORAGE_SAMPLES + HW_RESAMPLE_BLIP_BUFFER_EXTRA];
    } HwResampleBlip;

    typedef struct
    {
        bool enabled;
        uint32_t input_rate_hz;
        uint32_t output_rate_hz;
        uint32_t newest;
        double coefficients[HW_RESAMPLE_AA_TAPS];
        double history_l[HW_RESAMPLE_AA_TAPS];
        double history_r[HW_RESAMPLE_AA_TAPS];
    } HwResampleAntialias;

    typedef struct
    {
        HwResampleBlip left;
        HwResampleBlip right;
        uint64_t clock;
        int16_t last_l;
        int16_t last_r;
        uint32_t audio_buffers;
        uint32_t host_rate_hz;
        float fps_target;
        HwResampleAntialias aa; /* Optional; excluded from the default path. */
    } HwResample;

    void hw_resample_init(HwResample*, uint32_t host_rate_hz, float fps_target, uint32_t audio_buffers);
    void hw_resample_set_antialias_input_rate(HwResample*, uint32_t dac_rate_hz);
    void hw_resample_reset(HwResample*);
    void hw_resample_set_output_rate(HwResample*, uint32_t host_rate_hz);
    void hw_resample_set_audio_buffers(HwResample*, uint32_t audio_buffers);
    void hw_resample_submit(HwResample*, int16_t left, int16_t right, uint32_t dac_period_cycles);
    uint32_t hw_resample_read_pcm16(HwResample*, int16_t* left, int16_t* right, uint32_t max_frames);
    void hw_resample_set_antialias(HwResample*, bool enabled);

#ifdef __cplusplus
}
#endif

#endif
