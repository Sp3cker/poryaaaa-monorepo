#include "test_assert.h"

#include "hw_audio/hw_mix.h"

static void test_hw_mix_sq1_matches_mgba_max_level(void)
{
    printf("Testing hw_mix: SQ1 max level matches mGBA GBA PSG scaling...\n");

    HwMixBus mix;
    hw_mix_init(&mix);
    ASSERT_EQ(mix.sampling_cycle, 1, "mix defaults to the ROM's 65536 Hz SOUNDBIAS resolution");
    mix.master_vol_left = 7;
    mix.master_vol_right = 7;
    mix.pan_mask_left = 0x01;
    mix.pan_mask_right = 0x01;
    mix.psg_volume_code = 2;
    mix.bias_level = 0x200;

    uint8_t sq1[1] = {15};
    int16_t L[1] = {0};
    int16_t R[1] = {0};

    hw_mix_render(&mix, sq1, NULL, NULL, NULL, NULL, NULL, L, R, 1);

    ASSERT_EQ(L[0], 11520, "SQ1 max level equals mGBA PCM16");
    ASSERT_EQ(R[0], 11520, "SQ1 max level matches on right channel");
}

static void test_hw_mix_sq2_wave_noise_match_mgba_max_level(void)
{
    printf("Testing hw_mix: SQ2, wave, and noise max levels match mGBA...\n");

    HwMixBus mix;
    hw_mix_init(&mix);
    mix.master_vol_left = 7;
    mix.master_vol_right = 7;
    mix.psg_volume_code = 2;
    mix.bias_level = 0x200;

    uint8_t max_sample[1] = {15};
    int16_t L[1] = {0};
    int16_t R[1] = {0};

    mix.pan_mask_left = 0x02;
    mix.pan_mask_right = 0x02;
    hw_mix_render(&mix, NULL, max_sample, NULL, NULL, NULL, NULL, L, R, 1);
    ASSERT_EQ(L[0], 11520, "SQ2 max level matches mGBA");
    ASSERT_EQ(R[0], 11520, "SQ2 max level matches on right channel");

    L[0] = 0;
    R[0] = 0;
    mix.pan_mask_left = 0x04;
    mix.pan_mask_right = 0x04;
    hw_mix_render(&mix, NULL, NULL, max_sample, NULL, NULL, NULL, L, R, 1);
    ASSERT_EQ(L[0], 11520, "programmable wave max level matches mGBA");
    ASSERT_EQ(R[0], 11520, "programmable wave max level matches on right channel");

    L[0] = 0;
    R[0] = 0;
    mix.pan_mask_left = 0x08;
    mix.pan_mask_right = 0x08;
    hw_mix_render(&mix, NULL, NULL, NULL, max_sample, NULL, NULL, L, R, 1);
    ASSERT_EQ(L[0], 11520, "noise max level matches mGBA");
    ASSERT_EQ(R[0], 11520, "noise max level matches on right channel");
}

static void test_hw_mix_sq1_nr50_zero_keeps_mgba_floor(void)
{
    printf("Testing hw_mix: SQ1 NR50 code 0 keeps mGBA 1/8 master floor...\n");

    HwMixBus mix;
    hw_mix_init(&mix);
    mix.master_vol_left = 0;
    mix.master_vol_right = 0;
    mix.pan_mask_left = 0x01;
    mix.pan_mask_right = 0x01;
    mix.psg_volume_code = 2;
    mix.bias_level = 0x200;

    uint8_t sq1[1] = {15};
    int16_t L[1] = {0};
    int16_t R[1] = {0};

    hw_mix_render(&mix, sq1, NULL, NULL, NULL, NULL, NULL, L, R, 1);

    ASSERT_EQ(L[0], 1440, "NR50 code 0 scales SQ1 by mGBA's 1/8 factor");
    ASSERT_EQ(R[0], 1440, "NR50 code 0 floor matches on right channel");
}

static void test_hw_mix_clips_summed_psg_after_mix_like_mgba(void)
{
    printf("Testing hw_mix: summed PSG clips after mix in mGBA DAC domain...\n");

    HwMixBus mix;
    hw_mix_init(&mix);
    mix.master_vol_left = 7;
    mix.master_vol_right = 7;
    mix.pan_mask_left = 0x0F;
    mix.pan_mask_right = 0x0F;
    mix.psg_volume_code = 2;
    mix.bias_level = 0x200;

    uint8_t sq1[1] = {15};
    uint8_t sq2[1] = {15};
    uint8_t wave[1] = {15};
    uint8_t noise[1] = {15};
    int16_t L[1] = {0};
    int16_t R[1] = {0};

    hw_mix_render(&mix, sq1, sq2, wave, noise, NULL, NULL, L, R, 1);

    ASSERT_EQ(L[0], 24528, "four max PSG voices clip to mGBA 10-bit DAC ceiling");
    ASSERT_EQ(R[0], 24528, "summed PSG clipping matches on right channel");
}

static void test_hw_mix_dma_a_max_matches_mgba_level(void)
{
    printf("Testing hw_mix: DMA A max level matches mGBA FIFO scaling...\n");

    HwMixBus mix;
    hw_mix_init(&mix);
    mix.pan_mask_left = 0x00;
    mix.pan_mask_right = 0x00;
    mix.dma_a_left = true;
    mix.dma_a_right = false;
    mix.dma_a_vol_code = 1;
    mix.dma_b_left = false;
    mix.dma_b_right = false;
    mix.bias_level = 0x200;

    int8_t dma_a[1] = {127};
    int16_t L[1] = {0};
    int16_t R[1] = {0};

    hw_mix_render(&mix, NULL, NULL, NULL, NULL, dma_a, NULL, L, R, 1);

    ASSERT_EQ(L[0], 24384, "DMA A +127 equals mGBA (127 << 2) through _applyBias");
    ASSERT_EQ(R[0], 0, "unrouted DMA A is silent on right channel");
}

void test_hw_mix_run_all(void)
{
    test_hw_mix_sq1_matches_mgba_max_level();
    test_hw_mix_sq2_wave_noise_match_mgba_max_level();
    test_hw_mix_sq1_nr50_zero_keeps_mgba_floor();
    test_hw_mix_clips_summed_psg_after_mix_like_mgba();
    test_hw_mix_dma_a_max_matches_mgba_level();
}
