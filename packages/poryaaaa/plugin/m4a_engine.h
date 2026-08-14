#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "m4a_reverb.h"
#include "m4a/m4a_internal.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_PCM_CHANNELS 15
#define MAX_CGB_CHANNELS 4
#define TOTAL_PCM_CHANNELS (MAX_PCM_CHANNELS * 2)
#define TOTAL_CGB_CHANNELS (MAX_CGB_CHANNELS * 2)
#define MAX_TRACKS 16
#define VBLANK_RATE 59.7275f
#define MAX_SONG_VOLUME 127
#define M4A_ENGINE_MAX_PROCESS_FRAMES 2048

#define CHN_START 0x80
#define CHN_STOP 0x40
#define CHN_LOOP 0x10
#define CHN_IEC 0x04
#define CHN_ENV_MASK 0x03
#define CHN_ENV_ATTACK 0x03
#define CHN_ENV_DECAY 0x02
#define CHN_ENV_SUSTAIN 0x01
#define CHN_ENV_RELEASE 0x00
#define CHN_ON (CHN_START | CHN_STOP | CHN_IEC | CHN_ENV_MASK)

#define M4A_SYNTH_NONE 0
#define M4A_SYNTH_PULSE 1
#define M4A_SYNTH_SAW 2
#define M4A_SYNTH_TRIANGLE 3

#define MAX_PWM_PATTERN_STEPS 7

    typedef M4APulseWidthModPattern PulseWidthModPattern;

    typedef struct
    {
        uint8_t flags;
        uint8_t volume;
        uint8_t rawVolume;
        uint8_t volX;
        int8_t pan;
        int8_t panX;
        int8_t bend;
        uint8_t bendRange;
        uint8_t lfoSpeed;
        uint8_t lfoSpeedC;
        uint8_t lfoDelay;
        uint8_t lfoDelayC;
        uint8_t mod;
        uint8_t modT;
        int8_t modM;
        int8_t keyShift;
        int8_t keyShiftX;
        int8_t tune;
        uint8_t pitX;
        int8_t keyM;
        uint8_t pitM;
        uint8_t volMR;
        uint8_t volML;
        uint8_t pseudoEchoVolume;
        uint8_t pseudoEchoLength;
        uint8_t portamentoDuration;
        uint8_t portamentoPrevKey;
        uint8_t portamentoTargetKey;
        bool portamentoGliding;
        uint32_t portamentoElapsed;
        uint8_t pwmPattern;
        uint8_t pwmSpeed;
        uint8_t pwmSpeedCounter;
        uint8_t pwmStep;
        uint8_t priority;
        uint8_t currentProgram;
        ToneData currentVoice;
    } M4ATrack;

    typedef struct
    {
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
        WaveData* wav;
        int8_t* currentPointer;
        int32_t count;
        uint32_t fw;
        uint32_t frequency;
        int trackIndex;
        bool audition;
        bool isLoop;
        int32_t loopLen;
        int8_t* loopStart;
        uint8_t synthType;
        uint32_t synthPulseDuty;
    } M4APCMChannel;

    typedef struct
    {
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
        uint16_t sweepShadowFreq;
        uint8_t sweepStep;
        bool sweepEnabled;
        bool sweepMuted;
        float sweepClockAccum;
        uint32_t frequency;
        uint32_t phase;
        uint32_t* wavePointer;
        uint32_t phaseInc;
        uint32_t phaseIncFreq;
        int32_t waveSum;
        uint32_t* waveSumPointer;
        uint16_t lfsr;
        int trackIndex;
        bool audition;
        int32_t declickSample;
        int32_t declickSamplesRemaining;
    } M4ACGBChannel;

#define M4A_POLY_DROPPED 0
#define M4A_POLY_STOLEN 1
#define M4A_POLY_TAIL_CUT 2
#define M4A_POLY_TICK_NONE 0xFFFFFFFFu
#define M4A_POLY_EVENT_CAPACITY 64

    typedef struct
    {
        uint8_t type;
        uint8_t trackIndex;
        uint8_t midiKey;
        uint8_t byTrack;
        uint8_t program;
        uint32_t tick;
    } M4APolyEvent;

    typedef struct M4ADriver M4ADriver;
    typedef struct HwAudio HwAudio;
    typedef void (*M4AEngineXcmdFn)(void* ctx, int trackIndex, uint8_t selector, uint32_t value);

    typedef struct M4AEngine M4AEngine;

    struct M4AEngine
    {
        M4ATrack tracks[MAX_TRACKS];
        M4APCMChannel pcmChannels[TOTAL_PCM_CHANNELS];
        M4ACGBChannel cgbChannels[TOTAL_CGB_CHANNELS];
        M4AReverb reverb;
        float sampleRate;
        float samplesPerTick;
        float tickAccumulator;
        float pcmMixRate;
        uint8_t masterVolume;
        uint8_t songMasterVolume;
        uint8_t maxPcmChannels;
        uint8_t c15;
        bool respectBaseMidiKey;
        bool portamentoEnabled;
        bool pwmEnabled;
        bool pwmActiveFlag;
        bool polyDebugInvert;
        uint32_t polyEventClock;
        bool auditionNote;
        uint32_t polyDropCount[MAX_TRACKS];
        uint32_t polyStealCount[MAX_TRACKS];
        uint32_t polyTailCutCount[MAX_TRACKS];
        uint32_t polyEventTotal;
        M4APolyEvent polyEvents[M4A_POLY_EVENT_CAPACITY];
        bool analogFilter;
        uint16_t tempoD;
        uint16_t tempoU;
        uint16_t tempoI;
        uint16_t tempoC;
        ToneData* voiceGroup;

        M4ADriver* driver;
        HwAudio* hw;
        uint8_t volume;
        uint8_t reverbAmount;
        M4AEngineXcmdFn xcmd_fn;
        void* xcmd_ctx;

        M4ADriver* shadowDriver;
        HwAudio* shadowHw;
        M4ADriver* auditionDriver;
        HwAudio* auditionHw;
        bool primaryPcmAudition[MAX_PCM_CHANNELS];
        bool primaryCgbAudition[MAX_CGB_CHANNELS];
        int8_t primaryPcmAuditionSlot[MAX_PCM_CHANNELS];
        bool shadowPcmAudition[MAX_PCM_CHANNELS];
        bool shadowCgbAudition[MAX_CGB_CHANNELS];
        float invertScratchL[M4A_ENGINE_MAX_PROCESS_FRAMES];
        float invertScratchR[M4A_ENGINE_MAX_PROCESS_FRAMES];
    };

    bool m4a_engine_init(M4AEngine* engine, float sampleRate);
    M4AEngine* m4a_engine_create(float sampleRate);
    void m4a_engine_free(M4AEngine* engine);
    void m4a_engine_destroy(M4AEngine* engine);
    bool m4a_engine_reset(M4AEngine* engine);
    void m4a_engine_set_xcmd_callback(M4AEngine* engine, M4AEngineXcmdFn xcmd_fn, void* xcmd_ctx);

    void m4a_engine_set_pcm_mix_rate(M4AEngine* engine, float rate);
    void m4a_engine_set_voicegroup(M4AEngine* engine, ToneData* voiceGroup);
    void m4a_engine_refresh_voices(M4AEngine* engine);
    void m4a_engine_note_on(M4AEngine* engine, int trackIndex, uint8_t key, uint8_t velocity);
    void m4a_engine_note_off(M4AEngine* engine, int trackIndex, uint8_t key);
    void m4a_engine_program_change(M4AEngine* engine, int trackIndex, uint8_t program);
    void m4a_engine_cc(M4AEngine* engine, int trackIndex, uint8_t cc, uint8_t value);
    void m4a_engine_pitch_bend(M4AEngine* engine, int trackIndex, int16_t bend);
    void m4a_engine_all_notes_off(M4AEngine* engine, int trackIndex);
    void m4a_engine_all_sound_off(M4AEngine* engine);
    void m4a_engine_reset_portamento(M4AEngine* engine);
    void m4a_engine_set_portamento_enabled(M4AEngine* engine, bool enabled);
    void m4a_engine_set_pwm_enabled(M4AEngine* engine, bool enabled);
    void m4a_engine_set_poly_debug_invert(M4AEngine* engine, bool enabled);
    void m4a_engine_reset_poly_stats(M4AEngine* engine);
    void m4a_engine_set_volume(M4AEngine* engine, uint8_t volume);
    void m4a_engine_set_song_volume(M4AEngine* engine, uint8_t volume);
    void m4a_engine_set_reverb_amount(M4AEngine* engine, uint8_t amount);
    void m4a_engine_set_max_pcm_channels(M4AEngine* engine, uint8_t max_channels);
    void m4a_engine_set_analog_filter(M4AEngine* engine, bool enabled);
    void m4a_engine_set_tempo_bpm(M4AEngine* engine, double bpm);
    void m4a_engine_process(M4AEngine* engine, float* outL, float* outR, int numSamples);
    void m4a_engine_tick(M4AEngine* engine);
    void m4a_track_vol_pit_set(M4ATrack* track);
    M4ADriver* m4a_engine_driver(M4AEngine* engine);
    const M4ADriver* m4a_engine_driver_const(const M4AEngine* engine);
    HwAudio* m4a_engine_hw_audio(M4AEngine* engine);
    const HwAudio* m4a_engine_hw_audio_const(const M4AEngine* engine);
    uint32_t m4a_midi_key_to_freq(WaveData* wav, uint8_t key, uint8_t fineAdjust);
    uint32_t m4a_midi_key_to_cgb_freq(uint8_t chanNum, uint8_t key, uint8_t fineAdjust);

    static inline uint32_t umul3232H32(uint32_t a, uint32_t b)
    {
        return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 32);
    }

#ifdef __cplusplus
}
#endif
