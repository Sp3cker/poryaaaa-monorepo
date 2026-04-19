#ifndef VOICEGROUP_TYPES_H
#define VOICEGROUP_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Voice types (matching GBA ToneData.type) */
#define VOICE_DIRECTSOUND           0x00
#define VOICE_SQUARE_1              0x01
#define VOICE_SQUARE_2              0x02
#define VOICE_PROGRAMMABLE_WAVE     0x03
#define VOICE_NOISE                 0x04
#define VOICE_DIRECTSOUND_NO_RESAMPLE 0x08
#define VOICE_SQUARE_1_ALT          0x09
#define VOICE_SQUARE_2_ALT          0x0A
#define VOICE_PROGRAMMABLE_WAVE_ALT 0x0B
#define VOICE_NOISE_ALT             0x0C
#define VOICE_DIRECTSOUND_ALT       0x10
#define VOICE_CRY                   0x20
#define VOICE_CRY_REVERSE           0x30
#define VOICE_KEYSPLIT              0x40
#define VOICE_KEYSPLIT_ALL          0x80

#define VOICE_TYPE_CGB_MASK         0x07
#define VOICE_TYPE_FIX              0x08

/* WaveData header (matches GBA binary format) */
typedef struct {
    uint16_t type;
    uint16_t status;
    uint32_t freq;
    uint32_t loopStart;
    uint32_t size;
    int8_t *data;
} WaveData;

/* ToneData (voice/instrument definition) */
typedef struct {
    uint8_t type;
    uint8_t key;
    uint8_t length;
    uint8_t panSweep;
    union {
        WaveData *wav;
        uint32_t *wavePointer;  /* for programmable wave */
        void *subGroup;         /* for keysplit: points to ToneData array */
    };
    union {
        uint8_t *keySplitTable; /* for keysplit type */
    };
    uint8_t attack;
    uint8_t decay;
    uint8_t sustain;
    uint8_t release;
} ToneData;

#endif /* VOICEGROUP_TYPES_H */
