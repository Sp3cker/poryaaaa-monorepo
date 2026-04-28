#ifndef M4A_ENGINE_H
#define M4A_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#include "voicegroup/voicegroup_types.h"

#define MAX_PCM_CHANNELS 12
#define MAX_CGB_CHANNELS 4
#define MAX_TRACKS 16
#define VBLANK_RATE 59.7275f
#define MAX_SONG_VOLUME 127 // called "mxv" in pokeemerald

/* CGB internal synthesis rate.  mGBA's native PSG rate is 32768 Hz on GBA
 * (DMG_SM83_FREQUENCY 4.194304 MHz / SAMPLE_INTERVAL 32 cycles / timingFactor 4 =
 * 32768 Hz; gb_audio.c line 18-21).  Match it exactly so that aliasing,
 * envelope step timing, and noise LFSR coalescing reproduce mGBA's output. */
#define CGB_INTERNAL_RATE 32768.0f

typedef void (*M4AEngineXcmdFn)(void *ctx, int trackIndex, uint8_t selector, uint32_t value);

/* Channel status flags (matching GBA) */
#define CHN_START       0x80
#define CHN_STOP        0x40
#define CHN_LOOP        0x10
#define CHN_IEC         0x04
#define CHN_ENV_MASK    0x03
#define CHN_ENV_ATTACK  0x03
#define CHN_ENV_DECAY   0x02
#define CHN_ENV_SUSTAIN 0x01
#define CHN_ENV_RELEASE 0x00
#define CHN_ON          (CHN_START | CHN_STOP | CHN_IEC | CHN_ENV_MASK)

/* Track state (per MIDI channel) */
typedef struct {
    uint8_t flags;
    uint8_t volume;         /* track volume scaled by songMasterVolume (0-127) */
    uint8_t rawVolume;      /* raw CC 0x7 volume before songMasterVolume scaling */
    uint8_t volX;           /* external volume multiplier (0-64) */
    int8_t pan;             /* track pan (-64 to +63) */
    int8_t panX;            /* external pan adjustment */
    int8_t bend;            /* pitch bend (-64 to +63) */
    uint8_t bendRange;      /* bend range in semitones (default 2) */
    uint8_t lfoSpeed;
    uint8_t lfoSpeedC;
    uint8_t lfoDelay;
    uint8_t lfoDelayC;
    uint8_t mod;            /* modulation depth */
    uint8_t modT;           /* 0=vibrato, 1=tremolo, 2=autopan */
    int8_t modM;            /* current modulation output */
    int8_t keyShift;
    int8_t keyShiftX;
    int8_t tune;
    uint8_t pitX;
    int8_t keyM;            /* computed key after modifications */
    uint8_t pitM;           /* computed fine pitch */
    uint8_t volMR;          /* computed right volume */
    uint8_t volML;          /* computed left volume */
    uint8_t pseudoEchoVolume;
    uint8_t pseudoEchoLength;
    uint8_t priority;
    uint8_t currentProgram; /* last program_change index (0-127) */
    ToneData currentVoice;  /* current instrument */
    uint8_t extendedCommand;   /* last XCMD selector (0x1D/0x1F), dispatched by 0x1E */
    uint8_t extendedCommandBytes[4];
    uint8_t extendedCommandCount;
    uint8_t extendedWait;
    uint16_t extendedLoopCount;
    uint32_t extendedValue;
} M4ATrack;

/* PCM Sound Channel */
typedef struct {
    uint8_t status;
    uint8_t type;
    uint8_t rightVolume;
    uint8_t leftVolume;
    uint8_t attack;
    uint8_t decay;
    uint8_t sustain;
    uint8_t release;
    uint8_t key;
    uint8_t envelopeVolume;
    uint8_t envelopeVolumeRight;
    uint8_t envelopeVolumeLeft;
    uint8_t pseudoEchoVolume;
    uint8_t pseudoEchoLength;
    uint8_t midiKey;
    uint8_t velocity;
    uint8_t priority;
    int8_t rhythmPan;
    uint8_t gateTime;

    /* Sample playback */
    WaveData *wav;
    int8_t *currentPointer;
    int32_t count;          /* remaining samples */
    uint32_t fw;            /* fractional position (23-bit fraction) */
    uint32_t frequency;     /* playback frequency word */

    /* Owner */
    int trackIndex;
    bool isLoop;
    int32_t loopLen;        /* loop length in samples */
    int8_t *loopStart;      /* pointer to loop start in sample data */
} M4APCMChannel;

/* CGB Channel (square, noise, programmable wave) */
typedef struct {
    uint8_t status;
    uint8_t type;           /* 1=sq1, 2=sq2, 3=progwave, 4=noise */
    uint8_t rightVolume;
    uint8_t leftVolume;
    uint8_t attack;
    uint8_t decay;
    uint8_t sustain;
    uint8_t release;
    uint8_t key;
    uint8_t envelopeVolume;
    uint8_t envelopeGoal;
    uint8_t envelopeCounter;
    uint8_t pseudoEchoVolume;
    uint8_t pseudoEchoLength;
    uint8_t midiKey;
    uint8_t velocity;
    uint8_t priority;
    int8_t rhythmPan;
    uint8_t gateTime;
    uint8_t sustainGoal;
    uint8_t length;
    uint8_t sweep;
    uint8_t dutyCycle;
    uint8_t pan;
    uint8_t panMask;
    uint8_t modify;

    uint32_t frequency;
    uint32_t phase;         /* phase accumulator for synthesis */
    uint32_t *wavePointer;  /* programmable wave data */

    uint16_t lfsr;          /* noise LFSR state */
    /* Noise sub-sample averaging (mGBA's _coalesceNoiseChannel).  When the
     * LFSR clocks more than once per output sample, return the mean instead
     * of just the latest bit — band-limits the noise to the output Nyquist. */
    int32_t noiseAccum;
    int32_t noiseSamples;

    int trackIndex;

    /* Wave channel (type 3) declick: avoids a pop when the note ends by
     * smoothly fading the last sample to zero over DECLICK_SAMPLES frames. */
    int32_t declickSample;           /* last rendered sample, pre-pan, post-scale */
    int32_t declickSamplesRemaining; /* countdown; 0 = no declick active */
} M4ACGBChannel;

/* Forward declaration */
typedef struct M4AEngine M4AEngine;

#include "m4a_reverb.h"

/* Engine state */
struct M4AEngine {
    M4ATrack tracks[MAX_TRACKS];
    M4APCMChannel pcmChannels[MAX_PCM_CHANNELS];
    M4ACGBChannel cgbChannels[MAX_CGB_CHANNELS];
    M4AReverb reverb;

    float sampleRate;
    float samplesPerTick;
    float tickAccumulator;

    uint8_t masterVolume;   /* 0-15 */
    uint8_t songMasterVolume; /* 0-127 */
    uint8_t maxPcmChannels; /* active PCM channel count */
    uint8_t c15;            /* counter 0-14 for CGB envelope double-step */

    /* Output low-pass filter: 2nd-order Butterworth at 16384 Hz (GBA Nyquist).
     * Approximates the band-limit imposed by mGBA Qt's sinc resampler when it
     * upsamples GBA-native (~32768 Hz) to host rate.  When sampleRate <=
     * 32768 the filter is bypassed (host can't carry content above the
     * cutoff anyway).  Single biquad, Q = 1/sqrt(2), Direct Form II Transposed —
     * monotonic magnitude response, short impulse response, no audible ringing
     * on square-wave edges. */
    bool analogFilter;
    bool lpfBypass;
    float lpf_b0, lpf_b1, lpf_b2, lpf_a1, lpf_a2;
    float lpf_sL1, lpf_sL2;
    float lpf_sR1, lpf_sR2;

    /* CGB resampler state.  CGB channels render at CGB_INTERNAL_RATE; this
     * accumulator tracks the fractional position between two adjacent native
     * samples (cgbPrev*, cgbCurr*) for linear interpolation up to host rate.
     * accum advances by CGB_INTERNAL_RATE/sampleRate per host sample; when it
     * crosses 1.0 we render a new native CGB sample. */
    float cgbResampleAccum;
    int32_t cgbPrevL, cgbPrevR;
    int32_t cgbCurrL, cgbCurrR;

    /* PCM sample-and-hold upsampler state.  Real m4a renders PCM into the
     * GBA's FIFO at pcmStepHz (≈13379 Hz, "freq index 4"), and the hardware
     * DAC reads the FIFO at the SOUNDBIAS sample rate (32768 Hz default),
     * naturally applying a zero-order hold.  The hold's sinc(πf/pcmStepHz)
     * frequency response rolls high frequencies off above ~6.7 kHz — that
     * rolloff is part of the GBA's characteristic sound.  We replicate it by
     * advancing PCM channels at pcmStepHz and holding the last rendered
     * mix between updates. */
    float pcmStepHz;
    float pcmStepAccum;
    int32_t pcmHeldL, pcmHeldR;

    /* Tempo system (matches GBA MPlayMain tempo accumulator).
     * tempoD = base tempo (ply_tempo param * 2), default 150.
     * tempoU = user tempo multiplier (default 0x100 = 1.0x).
     * tempoI = (tempoD * tempoU) >> 8, the effective tempo increment.
     * tempoC = accumulator, incremented by tempoI each VBlank.
     * When tempoC >= 150, one "tempo tick" fires (LFO advances). */
    uint16_t tempoD;
    uint16_t tempoU;
    uint16_t tempoI;
    uint16_t tempoC;

    /* Loaded voice data */
    ToneData *voiceGroup;   /* array of 128 ToneData entries */
    M4AEngineXcmdFn xcmd_fn;
    void *xcmd_ctx;
};

/* Engine lifecycle */
void m4a_engine_init(M4AEngine *engine, float sampleRate);
void m4a_engine_destroy(M4AEngine *engine);
void m4a_engine_set_xcmd_callback(M4AEngine *engine, M4AEngineXcmdFn xcmd_fn, void *xcmd_ctx);

/* Set voicegroup (must be loaded by voicegroup_loader) */
void m4a_engine_set_voicegroup(M4AEngine *engine, ToneData *voiceGroup);

/* Re-copy voiceGroup[currentProgram] into each track's currentVoice.
 * Call after editing voicegroup entries to propagate changes to active tracks. */
void m4a_engine_refresh_voices(M4AEngine *engine);

/* MIDI event handling */
void m4a_engine_note_on(M4AEngine *engine, int trackIndex, uint8_t key, uint8_t velocity);
void m4a_engine_note_off(M4AEngine *engine, int trackIndex, uint8_t key);
void m4a_engine_program_change(M4AEngine *engine, int trackIndex, uint8_t program);
void m4a_engine_cc(M4AEngine *engine, int trackIndex, uint8_t cc, uint8_t value);
void m4a_engine_pitch_bend(M4AEngine *engine, int trackIndex, int16_t bend);
void m4a_engine_all_notes_off(M4AEngine *engine, int trackIndex);
void m4a_engine_all_sound_off(M4AEngine *engine);
void m4a_engine_lpf_reset(M4AEngine *engine);
void m4a_engine_set_song_volume(M4AEngine *engine, uint8_t volume);

/* Set tempo from DAW BPM.  The GBA relationship is tempoI ≈ BPM
 * (24 ticks per quarter note at ~59.7 Hz VBlank gives BPM ≈ tempoI). */
void m4a_engine_set_tempo_bpm(M4AEngine *engine, double bpm);

/* Audio processing */
void m4a_engine_process(M4AEngine *engine, float *outL, float *outR, int numSamples);

/* Internal: engine tick (~60Hz) */
void m4a_engine_tick(M4AEngine *engine);

/* Internal: track volume/pitch calculation (matches TrkVolPitSet) */
void m4a_track_vol_pit_set(M4ATrack *track);

/* Frequency helpers */
uint32_t m4a_midi_key_to_freq(WaveData *wav, uint8_t key, uint8_t fineAdjust);
uint32_t m4a_midi_key_to_cgb_freq(uint8_t chanNum, uint8_t key, uint8_t fineAdjust);

/* 32x32->high32 multiply (matches GBA umul3232H32) */
static inline uint32_t umul3232H32(uint32_t a, uint32_t b)
{
    return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 32);
}

#endif /* M4A_ENGINE_H */
