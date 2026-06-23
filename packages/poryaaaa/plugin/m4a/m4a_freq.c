#include "m4a_freq.h"
#include "m4a_tables.h"

static inline uint32_t umul3232H32(uint32_t a, uint32_t b)
{
    return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 32);
}

uint32_t m4a_midi_key_to_freq(WaveData* wav, uint8_t key, uint8_t fineAdjust)
{
    uint32_t fineAdjustShifted = (uint32_t)fineAdjust << 24;

    if (key > 178)
    {
        key = 178;
        fineAdjustShifted = 255u << 24;
    }

    uint32_t val1 = gScaleTable[key];
    val1 = gFreqTable[val1 & 0xF] >> (val1 >> 4);

    uint32_t val2 = gScaleTable[key + 1];
    val2 = gFreqTable[val2 & 0xF] >> (val2 >> 4);

    return umul3232H32(wav->freq, val1 + umul3232H32(val2 - val1, fineAdjustShifted));
}

uint32_t m4a_midi_key_to_cgb_freq(uint8_t chanNum, uint8_t key, uint8_t fineAdjust)
{
    if (chanNum == 4)
    {
        if (key <= 20)
        {
            key = 0;
        }
        else
        {
            key -= 21;
            if (key > 59)
                key = 59;
        }
        return gNoiseTable[key];
    }

    int32_t val1, val2;

    if (key <= 35)
    {
        fineAdjust = 0;
        key = 0;
    }
    else
    {
        key -= 36;
        if (key > 130)
        {
            key = 130;
            fineAdjust = 255;
        }
    }

    val1 = gCgbScaleTable[key];
    val1 = gCgbFreqTable[val1 & 0xF] >> (val1 >> 4);

    val2 = gCgbScaleTable[key + 1];
    val2 = gCgbFreqTable[val2 & 0xF] >> (val2 >> 4);

    return (uint32_t)(val1 + ((fineAdjust * (val2 - val1)) >> 8) + 2048);
}
