#ifndef M4A_REGISTER_FILE_H
#define M4A_REGISTER_FILE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GBA audio register snapshot — driver writes, chip reads.
 *
 * Provisional v2-scaffold contract; replaced at Layer 1.5 by an
 * ordered M4ARegWrite event stream.  See HW_AUDIO_SCAFFOLD_PLAN.md §6a/§6c. */
typedef struct {
    bool     psg_master_enabled;

    /* PSG square 1 (NR10–NR14) */
    uint8_t  sq1_sweep_pace;
    int8_t   sq1_sweep_dir;
    uint8_t  sq1_sweep_step;
    uint8_t  sq1_duty;
    uint8_t  sq1_env_volume;
    uint16_t sq1_freq;
    uint8_t  sq1_length;
    bool     sq1_length_enable;
    bool     sq1_enabled;

    /* PSG square 2 (NR21–NR24) — no sweep */
    uint8_t  sq2_duty;
    uint8_t  sq2_env_volume;
    uint16_t sq2_freq;
    uint8_t  sq2_length;
    bool     sq2_length_enable;
    bool     sq2_enabled;

    /* Wave (NR30–NR34 + wave RAM) */
    bool     wave_dac_on;
    uint8_t  wave_length;
    bool     wave_length_enable;
    uint8_t  wave_volume_shift;
    uint16_t wave_freq;
    bool     wave_enabled;
    uint8_t  wave_ram[16];

    /* Noise (NR41–NR44) */
    uint8_t  noise_env_volume;
    uint8_t  noise_length;
    bool     noise_length_enable;
    uint8_t  noise_clock_shift;
    uint8_t  noise_divisor_code;
    bool     noise_width_7bit;
    bool     noise_enabled;

    /* SOUNDCNT_L (NR50/NR51) — PSG master vol + per-PSG-channel pan */
    uint8_t  master_vol_left;
    uint8_t  master_vol_right;
    uint8_t  pan_mask_left;
    uint8_t  pan_mask_right;

    /* SOUNDCNT_H — PSG/DMA bus volumes + DMA L/R routing */
    uint8_t  psg_volume_code;
    uint8_t  dma_a_volume_code;
    uint8_t  dma_b_volume_code;
    bool     dma_a_enable_left, dma_a_enable_right;
    bool     dma_b_enable_left, dma_b_enable_right;

    /* SOUNDBIAS */
    uint16_t bias_level;
    uint8_t  bias_sampling_cycle;

    /* Edge-trigger latches; chip clears after consuming. */
    bool     trigger_sq1, trigger_sq2, trigger_wave, trigger_noise;
} M4ARegisterFile;

#ifdef __cplusplus
}
#endif

#endif
