#ifndef M4A_PCM_INTERNAL_H
#define M4A_PCM_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "m4a_pcm_ring.h"
#include "voicegroup/voicegroup_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    struct M4ADriver;
    struct M4ADriverPcmChan;

    /* Common scheduling geometry is computed once by the dispatcher.  Adapters
     * receive it as immutable input and retain ownership of source arithmetic. */
    typedef struct
    {
        uint32_t frame_size;
        uint32_t dma_buf_size;
        uint32_t ring_base;
    } M4APcmBlockGeometry;

    typedef enum
    {
        M4A_PCM_BLOCK_RENDERED = 0,
        M4A_PCM_BLOCK_SILENCE = 1,
    } M4APcmBlockKind;

    /* Adapter buffers remain read-only outside their translation unit.  SILENCE
     * is an explicit publication result; orchestration never clears an adapter's
     * returned storage. */
    typedef struct
    {
        M4APcmBlockGeometry geometry;
        const int8_t* left;
        const int8_t* right;
        M4APcmBlockKind kind;
    } M4APcmBlockOutput;

#define M4A_IPATIX_BDPCM_BLOCK_BYTES 33u
#define M4A_IPATIX_BDPCM_BLOCK_SAMPLES 64u

    /* State derived from the pinned iPatix source and never part of the common
     * channel model or a public/debug snapshot. */
    typedef struct
    {
        int8_t* current_pointer;
        int8_t sample_stored;
        int32_t count;
        uint32_t fine_position;
        bool is_loop;
        int32_t loop_length;
        uint8_t synth_type;
        uint32_t synth_pulse_duty;
        bool compressed;
        int32_t source_position;
        int32_t loop_start_position;
        int32_t decoded_block;
        bool decoded_block_valid;
        int8_t decoded_samples[M4A_IPATIX_BDPCM_BLOCK_SAMPLES];
    } M4APcmIpatixChannelState;

    /* Global iPatix work and feedback are embedded in M4ADriver so render and
     * mode-reset paths remain allocation-free.  Historical DMA taps read the
     * common M4APcmRing directly; keeping a second byte history would hide ring
     * wrapping and publication order. */
    typedef struct
    {
        uint32_t packed_mix[M4A_PCM_MAX_SAMPLES_PER_VBLANK];
        int8_t output_left[M4A_PCM_MAX_SAMPLES_PER_VBLANK];
        int8_t output_right[M4A_PCM_MAX_SAMPLES_PER_VBLANK];
        uint8_t discarded_left;
        uint8_t discarded_right;
        bool initialized;
    } M4APcmIpatixGlobalState;

#define M4A_SAPPY_COMPRESSED_BLOCK_BYTES 33u
#define M4A_SAPPY_COMPRESSED_BLOCK_SAMPLES 64u

    /* Per-voice cursor and compressed-cache tags derived from vanilla
     * SoundChannel.  Logical indices replace GBA addresses without changing the
     * pinned cursor conventions. */
    typedef struct
    {
        int32_t source_position;
        int32_t count;
        uint32_t fractional_position;
        uint32_t start_offset;
        int32_t decoded_block;
        bool special_initialized;
        bool compressed;
    } M4APcmSappyChannelState;

    /* Vanilla mixes directly into final DMA bytes and shares one 64-sample
     * compressed decode buffer while rendering a channel. */
    typedef struct
    {
        int8_t output_left[M4A_PCM_MAX_SAMPLES_PER_VBLANK];
        int8_t output_right[M4A_PCM_MAX_SAMPLES_PER_VBLANK];
        int8_t decoding_buffer[M4A_SAPPY_COMPRESSED_BLOCK_SAMPLES];
        bool initialized;
    } M4APcmSappyGlobalState;

    /* Observable channel state copied to engine/debug consumers.  Cursor,
     * interpolation, decoder, synth, and feedback storage are intentionally absent. */
    typedef struct
    {
        uint8_t status;
        uint8_t type;
        uint8_t right_volume;
        uint8_t left_volume;
        uint8_t attack;
        uint8_t decay;
        uint8_t sustain;
        uint8_t release;
        uint8_t key;
        uint8_t envelope_volume;
        uint8_t envelope_volume_right;
        uint8_t envelope_volume_left;
        uint8_t pseudo_echo_volume;
        uint8_t pseudo_echo_length;
        uint8_t midi_key;
        uint8_t velocity;
        uint8_t priority;
        int8_t rhythm_pan;
        uint8_t gate_time;
        WaveData* wav;
        uint32_t frequency;
        int track_index;
    } M4APcmChannelSnapshot;

    /* Reset all iPatix channel/global state at a PCM epoch boundary. */
    void m4a_pcm_ipatix_reset(struct M4ADriver* drv);

    /* Render one scheduled block using dispatcher-owned common geometry. */
    M4APcmBlockOutput m4a_pcm_ipatix_render(struct M4ADriver* drv, const M4APcmBlockGeometry* geometry);

    /* Initialize one common channel and its iPatix-private playback state. */
    void m4a_pcm_ipatix_start(
        struct M4ADriver* drv, struct M4ADriverPcmChan* ch, WaveData* wav, uint8_t type, uint32_t start_offset);

    /* Apply a common pitch update at the adapter's lifecycle seam. */
    void m4a_pcm_ipatix_update_pitch(struct M4ADriver* drv, struct M4ADriverPcmChan* ch, uint32_t frequency);

    /* Transfer private playback position during portamento inheritance. */
    bool m4a_pcm_ipatix_inherit(struct M4ADriver* drv,
                                struct M4ADriverPcmChan* destination,
                                const struct M4ADriverPcmChan* source);

    /* Reset all vanilla Sappy channel/global state at a PCM epoch boundary. */
    void m4a_pcm_sappy_reset(struct M4ADriver* drv);

    /* Render one vanilla Sappy block using dispatcher-owned common geometry. */
    M4APcmBlockOutput m4a_pcm_sappy_render(struct M4ADriver* drv, const M4APcmBlockGeometry* geometry);

    /* Initialize one common channel and its Sappy-private playback state. */
    void m4a_pcm_sappy_start(
        struct M4ADriver* drv, struct M4ADriverPcmChan* ch, WaveData* wav, uint8_t type, uint32_t start_offset);

    /* Apply a common pitch update at the vanilla adapter lifecycle seam. */
    void m4a_pcm_sappy_update_pitch(struct M4ADriver* drv, struct M4ADriverPcmChan* ch, uint32_t frequency);

    /* Transfer vanilla playback state during portamento inheritance. */
    bool m4a_pcm_sappy_inherit(struct M4ADriver* drv,
                               struct M4ADriverPcmChan* destination,
                               const struct M4ADriverPcmChan* source);

    /* Reset the active PCM adapter at a driver PCM epoch boundary. */
    void m4a_drv_pcm_reset(struct M4ADriver* drv);

    /* Dispatcher seams used by track, engine, and SoundMain orchestration. */
    void m4a_drv_pcm_start(
        struct M4ADriver* drv, struct M4ADriverPcmChan* ch, WaveData* wav, uint8_t type, uint32_t start_offset);
    void m4a_drv_pcm_update_pitch(struct M4ADriver* drv, struct M4ADriverPcmChan* ch, uint32_t frequency);
    bool m4a_drv_pcm_inherit(struct M4ADriver* drv,
                             struct M4ADriverPcmChan* destination,
                             const struct M4ADriverPcmChan* source);
    bool m4a_drv_pcm_clone(struct M4ADriver* destination,
                           struct M4ADriverPcmChan* destination_channel,
                           const struct M4ADriver* source,
                           const struct M4ADriverPcmChan* source_channel);
    void m4a_drv_pcm_snapshot(const struct M4ADriver* drv,
                              const struct M4ADriverPcmChan* ch,
                              M4APcmChannelSnapshot* snapshot);
    M4APcmBlockOutput m4a_drv_pcm_render(struct M4ADriver* drv, uint32_t frame_size);

#ifdef __cplusplus
}
#endif

#endif
