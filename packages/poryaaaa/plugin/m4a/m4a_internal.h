#ifndef M4A_INTERNAL_H
#define M4A_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "m4a_driver.h"
#include "m4a_register_file.h"
#include "m4a_pcm_ring.h"
#include "voicegroup/voicegroup_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define M4A_MAX_TRACKS 16
#define M4A_MAX_CGB_CHANNELS 4  /* sq1, sq2, wave, noise */
#define M4A_MAX_PCM_CHANNELS 15 /* DirectSound polyphony cap */
/* Layer 1.5 event-queue capacity.  A 2048-frame render at the maximum
 * supported PCM rate emits one TIMER per byte plus FIFO refill writes;
 * leave room for dense CGB writes in that same render span. */
#define M4A_EVENT_QUEUE_CAP 32768

/* PCM_DMA constants live in m4a_pcm_ring.h (public — chip-side reads
 * them too).  M4A_PCM_SAMPLES_PER_VBLANK / M4A_PCM_RATE_HZ /
 * M4A_PCM_DMA_BUF_SIZE come in via that header, included above. */

/* PCM block geometry retains the m4a driver's canonical 59.7275-Hz rate. */
#define M4A_PCM_VBLANK_RATE_NUMERATOR 23891u
#define M4A_PCM_VBLANK_RATE_DENOMINATOR 400u
/* Pokemon Emerald runs m4aSoundVSync at line 150, ten scanlines before
 * SoundMain's line-160 VBlank callback. */
#define M4A_GBA_SCANLINE_CYCLES 1232u
#define M4A_VCOUNT_TO_VBLANK_CYCLES (10u * M4A_GBA_SCANLINE_CYCLES)

/* Channel `status` flag bits — match pokeemerald m4a_internal.h CHN_*.
 * We mirror the real m4a constants so disasm reads match. */
#define M4A_CHN_ON 0x80
#define M4A_CHN_STOP 0x40
#define M4A_CHN_LOOP 0x10
#define M4A_CHN_START 0x20
#define M4A_CHN_IEC 0x04 /* In Echo/pseudo-echo */
/* Envelope phase ordinals.  Encoded so `status--` walks the envelope
 * forward (ATTACK→DECAY→SUSTAIN→RELEASE), matching v1 m4a_engine.h and
 * pokeemerald m4a_internal.h.  m4a_cgb.c / m4a_pcm.c rely on this
 * ordering when transitioning between phases. */
#define M4A_CHN_ENV_MASK 0x03
#define M4A_CHN_ENV_RELEASE 0x00
#define M4A_CHN_ENV_SUSTAIN 0x01
#define M4A_CHN_ENV_DECAY 0x02
#define M4A_CHN_ENV_ATTACK 0x03

/* CgbSound `modify` bits — pokeemerald m4a_internal.h MO_*. */
#define M4A_MO_PIT 0x1  /* re-emit NRx3 + NRx4 freq write */
#define M4A_MO_VOL 0x2  /* re-emit NRx2 + NRx4-with-trigger */
#define M4A_MO_DUTY 0x4 /* re-emit square NRx1 duty/length write */

    /* Driver-internal track state.  Shape mirrors v1's M4ATrack so the disasm
     * comparison path stays clean.  Field set is the minimum needed for Layer 1
     * (CGB envelope/pitch); PCM-specific fields land in step 2. */
    typedef struct
    {
        uint8_t flags;
        uint8_t volume;    /* CC7 vol scaled by song master volume */
        uint8_t rawVolume; /* CC7 raw before song-master scaling */
        uint8_t volX;      /* xCmd external volume multiplier (0..64) */
        int8_t pan;        /* CC10 -64..+63 */
        int8_t panX;
        int8_t bend;       /* pitch bend signed -64..+63 */
        uint8_t bendRange; /* 1..12 semitones (default 2) */

        uint8_t lfoSpeed;
        uint8_t lfoSpeedC;
        uint8_t lfoDelay;
        uint8_t lfoDelayC;
        uint8_t mod;  /* mod depth */
        uint8_t modT; /* 0=vibrato 1=tremolo 2=autopan */
        int8_t modM;  /* current mod output */

        int8_t keyShift;
        int8_t keyShiftX;
        int8_t tune;
        uint8_t pitX;

        /* Computed by m4a_trk_vol_pit_set */
        int8_t keyM;
        uint8_t pitM;
        uint8_t volMR;
        uint8_t volML;

        uint8_t pseudoEchoVolume;
        uint8_t pseudoEchoLength;

        /* Legacy facade-only opt-in effect state. */
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

        /* xCmd (XCMD-via-MIDI-CC) state.  Two-CC protocol mirrors v1
         * (plugin/m4a_engine.c:779-796): CC 0x1E sets `extendedCommand`,
         * CC 0x1D/0x1F appends payload bytes.  When `extendedCommandCount`
         * reaches `xcmd_data_length(extendedCommand)` we apply.  Selector is
         * sticky after apply — only the byte count resets.  See xcmd.md. */
        uint8_t extendedCommand;      /* last 0x1E selector, 0 = idle */
        uint8_t extendedCommandCount; /* bytes accumulated so far */
        uint8_t extendedCommandBytes[4];
        uint32_t extendedValue; /* xCmd 0x0D payload (notify + storage) */

        uint8_t currentProgram; /* last program_change */
        ToneData currentVoice;  /* resolved voice for currentProgram */
    } M4ADriverTrack;

    /* Driver-internal CGB channel state (one per square 1, square 2, wave,
     * noise — total 4).  Equivalent to v1's M4ACGBChannel, narrowed to driver
     * concerns: hardware *runtime* state (phase accum, LFSR, declick) lives in
     * hw_audio.  This struct is what m4a software ticks. */
    typedef struct
    {
        uint8_t status;    /* M4A_CHN_* flags */
        uint8_t type;      /* 1=sq1, 2=sq2, 3=wave, 4=noise */
        uint8_t voiceType; /* Original ToneData.type, including FIX/alt bit */
        uint8_t rightVolume;
        uint8_t leftVolume;
        uint8_t attack;
        uint8_t decay;
        uint8_t sustain;
        uint8_t release;
        uint8_t key;
        uint8_t envelopeVolume; /* current 0..15 */
        uint8_t envelopeGoal;   /* target on note-on */
        uint8_t sustainGoal;    /* sustain level scaled by envelopeGoal */
        uint8_t envelopeCounter;
        uint8_t envelopeStepTimeAndDir; /* NRx2 low nibble retained across phase writes */
        uint8_t pseudoEchoVolume;
        uint8_t pseudoEchoLength;
        uint8_t midiKey;
        uint8_t velocity;
        uint8_t priority;
        int8_t rhythmPan;
        uint8_t gateTime;
        uint8_t length;    /* NRx1 length-counter init value */
        uint8_t sweep;     /* NR10 sweep byte (sq1 only) */
        uint8_t dutyCycle; /* sq1/sq2 duty 0..3 */
        uint8_t pan;       /* NR51-style L/R nibble */
        uint8_t panMask;
        uint8_t modify; /* M4A_MO_* bits — what needs writing this tick */

        uint16_t frequency;          /* sq1/sq2/wave: 11-bit GB freq.
                                       noise: NR43 byte (clock_shift<<4|divisor_code) | period_bit */
        uint32_t* wavePointer;       /* programmable wave (32 nibbles, 16 bytes) */
        uint32_t* loadedWavePointer; /* wave RAM payload last loaded by CgbSound */

        int trackIndex;

        /* Retained for the legacy audition stop path.  Wave RAM reloads are
         * governed solely by loadedWavePointer, which intentionally survives
         * channel disable like the ROM's currentPointer cache. */
        bool waveRamPending;

        /* Set by m4a_drv_cgb_start and consumed by emit_vol_write.  Wave
         * uses it to limit NR34 trigger to fresh notes; square and noise
         * follow the ROM's trigger-on-volume-write behavior. */
        bool freshStart;
    } M4ADriverCgbChan;

    /* Driver-internal PCM channel state.  Mirrors v1's M4APCMChannel.  All
     * voice synthesis happens here in software (real m4a's SoundMainRAM);
     * the hardware FIFOs only ever see the post-mix int8 stream. */
    typedef struct
    {
        uint8_t status; /* M4A_CHN_* flags */
        uint8_t type;   /* voice type (incl. VOICE_TYPE_FIX bit) */
        uint8_t rightVolume;
        uint8_t leftVolume;
        uint8_t attack;
        uint8_t decay;
        uint8_t sustain;
        uint8_t release;
        uint8_t key;
        uint8_t envelopeVolume;      /* current 0..255 */
        uint8_t envelopeVolumeRight; /* env × right × master, computed per tick */
        uint8_t envelopeVolumeLeft;
        uint8_t pseudoEchoVolume;
        uint8_t pseudoEchoLength;
        uint8_t midiKey;
        uint8_t velocity;
        uint8_t priority;
        int8_t rhythmPan;
        uint8_t gateTime;

        /* Sample-read state */
        WaveData* wav;
        int8_t* currentPointer; /* forward: current sample; reverse: one past next sample */
        int8_t sampleStored;    /* preceding source byte for reverse interpolation */
        int32_t count;          /* remaining samples in current segment */
        uint32_t fw;            /* 23-bit fractional position accumulator */
        uint32_t frequency;     /* per-PCM-tick step, Q9.23 */

        bool isLoop;
        int32_t loopLen;
        int8_t* loopStart;

        int trackIndex;
        /* Zero-length WaveData descriptors select Golden Sun's software
         * oscillators.  `fw` is then the 32-bit oscillator phase and `count`
         * is the pulse LFO accumulator or pseudo-saw filter state. */
        uint8_t synthType;
        uint32_t synthPulseDuty;

    } M4ADriverPcmChan;

    struct M4ADriver
    {
        /* Lifecycle / config */
        float host_rate;
        float pcm_mix_rate; /* requested rate; zero follows host_rate */
        uint32_t pcm_rate_hz;
        uint32_t pcm_max_samples_per_vblank;
        uint32_t pcm_dma_buf_size;
        /* Remainder of rate * 400 / 23891, reset with each PCM epoch. */
        uint32_t pcm_vblank_remainder;
        ToneData* voicegroup;
        M4ADriverXcmdFn xcmd_fn;
        void* xcmd_ctx;

        /* Plugin-level config (mirrored from host/GUI; not yet audible — wired
         * for §12a's "no shadow state" goal). */
        uint8_t song_volume;
        uint8_t master_volume;
        uint8_t reverb_amount;
        bool analog_filter;
        uint8_t max_pcm_channels;
        double tempo_bpm;
        bool compat_respect_base_midi_key;
        bool compat_portamento_enabled;
        bool compat_pwm_enabled;
        bool compat_pwm_active;
        bool compat_shadow_note;
        bool compat_zero_pcm_is_silent;
        bool compat_skip_pwm_tick;

        /* m4a tempo accumulator (vblank-clocked).  Fires LFO ticks when tempoC
         * crosses 150.  Mirrors v1's tempoD/tempoU/tempoI/tempoC. */
        uint16_t tempoD;
        uint16_t tempoU;
        uint16_t tempoI;
        uint16_t tempoC;

        /* CgbSound c15 counter (0..14 cycle for double-step on c15==0). */
        uint8_t c15;

        /* Host-facing advance converts frames to GBA cycles with an integer
         * remainder.  The VBlank clock is the exact 280896-cycle hardware
         * cadence, independent of host-buffer partitioning. */
        uint32_t host_rate_hz;
        uint64_t host_cycle_remainder;
        uint64_t current_cycle;
        uint64_t next_vcount_cycle;
        uint64_t next_vblank_cycle;
        /* DirectSound's DMA/timer scheduler.  The mixer writes the circular
         * software source ring at VBlank; DMA refills each hardware FIFO in
         * four-word bursts before the selected timer consumes one byte. */
        uint64_t next_pcm_timer_cycle;
        uint8_t pcm_dma_counter;
        uint8_t pcm_dma_period;
        uint64_t pcm_fifo_a_source_cursor;
        uint64_t pcm_fifo_b_source_cursor;
        uint8_t pcm_fifo_a_read;
        uint8_t pcm_fifo_a_write;
        uint8_t pcm_fifo_b_read;
        uint8_t pcm_fifo_b_write;
        uint8_t pcm_fifo_a_internal_remaining;
        uint8_t pcm_fifo_b_internal_remaining;
        /* Ordered driver→chip events.  The range is the absolute interval
         * since the last consume; each same-cycle emitter increments
         * event_next_order. */
        uint64_t event_range_begin_cycle;
        uint64_t event_cycle;
        uint32_t event_next_order;
        size_t event_count;
        /* Diagnostic: incremented every time m4a_internal_emit_event finds
         * the queue full and has to drop a write.  Production code should
         * keep this at 0 by chunking m4a_advance calls into windows that
         * fit in M4A_EVENT_QUEUE_CAP.  Tests assert it doesn't grow. */
        uint32_t events_dropped;
        M4ARegWrite events[M4A_EVENT_QUEUE_CAP];
        /* Stable batch view returned by m4a_get_pending_writes; rebuilt
         * whenever events[] changes.  Keeps the const-correct shape from
         * the public API without exposing the raw array directly. */
        M4ARegWriteBatch event_batch;

        /* Track + channel state */
        M4ADriverTrack tracks[M4A_MAX_TRACKS];
        M4ADriverCgbChan cgb[M4A_MAX_CGB_CHANNELS];
        M4ADriverPcmChan pcmChans[M4A_MAX_PCM_CHANNELS];

        /* SoundMainRAM accumulates a stereo sample as one 00LL00RR packed
         * word.  The ARM MLA deliberately lets the right product carry into
         * the left lane; the later downsampler separates both FIFO bytes. */
        uint32_t pcmMixPacked[M4A_PCM_MAX_SAMPLES_PER_VBLANK];
        int16_t pcmMixL[M4A_PCM_MAX_SAMPLES_PER_VBLANK];
        int16_t pcmMixR[M4A_PCM_MAX_SAMPLES_PER_VBLANK];

        /* SoundMainRAM carries the discarded low seven mixer bits between
         * adjacent output samples independently for the two packed lanes. */
        uint8_t pcm_noise_shape_left;
        uint8_t pcm_noise_shape_right;

        /* Reverb delay-line state.  Vanilla Sappy SoundMainRAM_Reverb is a
         * 4-tap design: read current L+R and (current+frameSize) L+R, sum,
         * scale by amount>>9, write back.  The active buffer retains the
         * canonical DMA buffer's duration as the PCM rate changes; the
         * other tap remains one active vblank ahead.
         *
         * Type is int8 because real m4a's reverb buffer IS the int8 FIFO
         * buffer (gPcmDmaBuffer) — wet samples are clamped to int8 range
         * before writeback so future tap reads see the same values that
         * would have been DMA'd.  v1 uses int8 too (m4a_reverb.c).  An
         * int16 buffer here would diverge on heavy mixes where pcmMix
         * temporarily exceeds [-128, 127] before the final clamp-to-int8
         * stage; the delay line would feed those out-of-range values back
         * into subsequent reverb sums, drifting from real-hardware behavior. */
        int8_t reverbBufL[M4A_PCM_MAX_DMA_BUF_SIZE];
        int8_t reverbBufR[M4A_PCM_MAX_DMA_BUF_SIZE];
        uint16_t reverbPos;

        /* Public contract output (driver→chip).  CgbSound writes regs each
         * tick; SoundMainRAM writes pcm.ring_a/ring_b each vblank. */
        M4ARegisterFile regs;
        M4APcmRing pcm;
    };

    /* Recomputes the integer host-rate divisor used by m4a_advance. */
    void m4a_internal_recompute_host_timing(M4ADriver* drv);

    /* Appends one register-write event at the current absolute event cycle.
     * Drops silently if the queue is full (caller's responsibility to size it
     * for the worst case — see M4A_EVENT_QUEUE_CAP). */
    void m4a_internal_emit_event(M4ADriver* drv, M4ARegId reg, uint32_t value);

    /* Run one LFO tempo tick across all tracks.  Called from m4a_main.c's
     * tempoC-overflow loop.  Mirrors v1's m4a_lfo_tick (m4a_engine.c:870):
     * each track with mod != 0 and lfoSpeed != 0 advances lfoSpeedC by
     * lfoSpeed, derives a triangle-wave sample, and folds it into modM.
     * When modM changes, the track's derived state is recomputed and
     * active CGB / PCM channels on this track are refreshed. */
    void m4a_internal_lfo_tick(M4ADriver* drv);

    /* Legacy facade-only control seams. */
    void m4a_internal_compat_effects_tick(M4ADriver* drv);
    void m4a_internal_reset_portamento(M4ADriver* drv);
    void m4a_internal_disable_pwm(M4ADriver* drv);
    void m4a_internal_compat_tick(M4ADriver* drv);

    /* Recompute every active PCM channel's source step after its mix rate
     * changes.  The next SoundMainRAM event consumes the corrected step. */
    void m4a_internal_refresh_pcm_pitches(M4ADriver* drv);
    /* Clears the software source and schedules canonical FIFO pointer resets
     * at the current hardware cycle. */
    void m4a_internal_reset_pcm_output(M4ADriver* drv);

#ifdef __cplusplus
}
#endif

#endif
