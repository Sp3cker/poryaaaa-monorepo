#include "m4a_internal.h"
#include "m4a_pcm_internal.h"

#include <string.h>

/* Advance the exact PCM-rate remainder used by SoundMainRAM's VBlank blocks. */
static uint32_t pcm_next_frame_size(M4ADriver* drv)
{
    const uint64_t numerator = (uint64_t)drv->pcm_rate_hz * M4A_PCM_VBLANK_RATE_DENOMINATOR;
    const uint64_t total = (uint64_t)drv->pcm_vblank_remainder + numerator;
    uint32_t samples = (uint32_t)(total / M4A_PCM_VBLANK_RATE_NUMERATOR);
    drv->pcm_vblank_remainder = (uint32_t)(total % M4A_PCM_VBLANK_RATE_NUMERATOR);
    if (samples > drv->pcm_max_samples_per_vblank)
        samples = drv->pcm_max_samples_per_vblank;
    return samples;
}

/* Return the configured circular source-ring size within adapter storage. */
static uint32_t pcm_active_dma_buf_size(const M4ADriver* drv)
{
    const uint32_t size = drv->pcm_dma_buf_size;
    return size != 0 && size <= M4A_PCM_MAX_DMA_BUF_SIZE ? size : M4A_PCM_DMA_BUF_SIZE;
}
/* Resolve common block geometry once before adapter dispatch. */
static M4APcmBlockGeometry pcm_block_geometry(const M4ADriver* drv, uint32_t frame_size)
{
    if (frame_size > M4A_PCM_MAX_SAMPLES_PER_VBLANK)
        frame_size = M4A_PCM_MAX_SAMPLES_PER_VBLANK;
    const uint32_t dma_buf_size = pcm_active_dma_buf_size(drv);
    uint32_t segment = 0;
    if (drv->pcm_dma_counter > 1u && drv->pcm_dma_counter <= drv->pcm_dma_period)
        segment = (uint32_t)drv->pcm_dma_period - ((uint32_t)drv->pcm_dma_counter - 1u);
    return (M4APcmBlockGeometry){
        .frame_size = frame_size,
        .dma_buf_size = dma_buf_size,
        .ring_base = (uint32_t)(((uint64_t)segment * drv->pcm_max_samples_per_vblank) % dma_buf_size),
    };
}

/* Keep the already-published absolute window while clearing its complement
 * with at most two contiguous writes.  Unsigned distance preserves windows
 * that cross uint64_t wrap as well as windows that cross the ring boundary. */
static void pcm_zero_unpublished_lane(int8_t* ring, uint32_t size, uint64_t fifo_source_cursor, uint64_t write_cursor)
{
    const uint64_t span = write_cursor - fifo_source_cursor;
    if (span == 0)
    {
        memset(ring, 0, size);
        return;
    }
    if (span >= size)
        return;

    const uint32_t start = (uint32_t)(fifo_source_cursor % size);
    const uint32_t end = start + (uint32_t)span;
    if (end <= size)
    {
        memset(ring, 0, start);
        memset(ring + end, 0, size - end);
        return;
    }

    const uint32_t wrapped_end = end - size;
    memset(ring + wrapped_end, 0, start - wrapped_end);
}

/* A mode commit cuts PCM voices and private adapter state, but deliberately
 * leaves all common hardware cadence, FIFO, event, and ring metadata intact. */
static bool pcm_commit_requested_mode(M4ADriver* drv)
{
    if (!drv || drv->requested_pcm_mode == drv->active_pcm_mode ||
        (drv->requested_pcm_mode != M4A_PCM_MIXER_IPATIX && drv->requested_pcm_mode != M4A_PCM_MIXER_SAPPY))
        return false;

    const uint32_t buf_size = pcm_active_dma_buf_size(drv);
    pcm_zero_unpublished_lane(drv->pcm.ring_a, buf_size, drv->pcm_fifo_a_source_cursor, drv->pcm.write_cursor);
    pcm_zero_unpublished_lane(drv->pcm.ring_b, buf_size, drv->pcm_fifo_b_source_cursor, drv->pcm.write_cursor);

    memset(drv->pcmChans, 0, sizeof(drv->pcmChans));
    memset(&drv->pcmMixerState, 0, sizeof(drv->pcmMixerState));

    drv->active_pcm_mode = drv->requested_pcm_mode;
    m4a_drv_pcm_reset(drv);
    return true;
}

/* Dispatch reset at the mode seam; each adapter owns only its private state. */
void m4a_drv_pcm_reset(M4ADriver* drv)
{
    if (!drv)
        return;
    switch (drv->active_pcm_mode)
    {
    case M4A_PCM_MIXER_SAPPY:
        m4a_pcm_sappy_reset(drv);
        break;
    case M4A_PCM_MIXER_IPATIX:
    default:
        m4a_pcm_ipatix_reset(drv);
        break;
    }
}

/* Dispatch channel initialization without exposing adapter cursor state. */
void m4a_drv_pcm_start(M4ADriver* drv, M4ADriverPcmChan* ch, WaveData* wav, uint8_t type, uint32_t start_offset)
{
    if (!drv || !ch)
        return;
    switch (drv->active_pcm_mode)
    {
    case M4A_PCM_MIXER_SAPPY:
        m4a_pcm_sappy_start(drv, ch, wav, type, start_offset);
        break;
    case M4A_PCM_MIXER_IPATIX:
    default:
        m4a_pcm_ipatix_start(drv, ch, wav, type, start_offset);
        break;
    }
}

/* Dispatch effective-pitch updates at the lifecycle seam. */
void m4a_drv_pcm_update_pitch(M4ADriver* drv, M4ADriverPcmChan* ch, uint32_t frequency)
{
    if (!drv || !ch)
        return;
    switch (drv->active_pcm_mode)
    {
    case M4A_PCM_MIXER_SAPPY:
        m4a_pcm_sappy_update_pitch(drv, ch, frequency);
        break;
    case M4A_PCM_MIXER_IPATIX:
    default:
        m4a_pcm_ipatix_update_pitch(drv, ch, frequency);
        break;
    }
}

/* Dispatch portamento inheritance without copying common records in track
 * code; the selected adapter copies only its own playback position. */
bool m4a_drv_pcm_inherit(M4ADriver* drv, M4ADriverPcmChan* destination, const M4ADriverPcmChan* source)
{
    if (!drv || !destination || !source)
        return false;
    switch (drv->active_pcm_mode)
    {
    case M4A_PCM_MIXER_SAPPY:
        return m4a_pcm_sappy_inherit(drv, destination, source);
    case M4A_PCM_MIXER_IPATIX:
    default:
        return m4a_pcm_ipatix_inherit(drv, destination, source);
    }
}

/* Clone one complete same-mode channel, including the active private union. */
bool m4a_drv_pcm_clone(M4ADriver* destination,
                       M4ADriverPcmChan* destination_channel,
                       const M4ADriver* source,
                       const M4ADriverPcmChan* source_channel)
{
    if (!destination || !destination_channel || !source || !source_channel ||
        destination->active_pcm_mode != source->active_pcm_mode)
        return false;
    *destination_channel = *source_channel;
    return true;
}

/* Copy the common logical channel into the normalized consumer view. */
void m4a_drv_pcm_snapshot(const M4ADriver* drv, const M4ADriverPcmChan* ch, M4APcmChannelSnapshot* snapshot)
{
    if (!snapshot)
        return;
    if (!drv || !ch)
    {
        memset(snapshot, 0, sizeof(*snapshot));
        return;
    }
    *snapshot = (M4APcmChannelSnapshot){
        .status = ch->status,
        .type = ch->type,
        .right_volume = ch->rightVolume,
        .left_volume = ch->leftVolume,
        .attack = ch->attack,
        .decay = ch->decay,
        .sustain = ch->sustain,
        .release = ch->release,
        .key = ch->key,
        .envelope_volume = ch->envelopeVolume,
        .envelope_volume_right = ch->envelopeVolumeRight,
        .envelope_volume_left = ch->envelopeVolumeLeft,
        .pseudo_echo_volume = ch->pseudoEchoVolume,
        .pseudo_echo_length = ch->pseudoEchoLength,
        .midi_key = ch->midiKey,
        .velocity = ch->velocity,
        .priority = ch->priority,
        .rhythm_pan = ch->rhythmPan,
        .gate_time = ch->gateTime,
        .wav = ch->wav,
        .frequency = ch->frequency,
        .track_index = ch->trackIndex,
    };
}

/* Compute common geometry once, then dispatch one source-owned render. */
M4APcmBlockOutput m4a_drv_pcm_render(M4ADriver* drv, uint32_t frame_size)
{
    M4APcmBlockOutput output = {0};
    if (!drv)
        return output;

    const M4APcmBlockGeometry geometry = pcm_block_geometry(drv, frame_size);
    switch (drv->active_pcm_mode)
    {
    case M4A_PCM_MIXER_SAPPY:
        return m4a_pcm_sappy_render(drv, &geometry);
    case M4A_PCM_MIXER_IPATIX:
    default:
        return m4a_pcm_ipatix_render(drv, &geometry);
    }
}

/* Publish adapter-produced FIFO bytes into the driver's circular source ring. */
void m4a_sound_main_ram(M4ADriver* drv)
{
    if (!drv)
        return;

    const bool committed = pcm_commit_requested_mode(drv);
    M4APcmBlockOutput output = m4a_drv_pcm_render(drv, pcm_next_frame_size(drv));
    if (committed)
        output.kind = M4A_PCM_BLOCK_SILENCE;

    const uint32_t frame_size = output.geometry.frame_size;
    const uint32_t buf_size = output.geometry.dma_buf_size;
    if (frame_size == 0 || buf_size == 0 || (output.kind == M4A_PCM_BLOCK_RENDERED && (!output.left || !output.right)))
        return;

    const bool silence = output.kind == M4A_PCM_BLOCK_SILENCE;
    for (uint32_t i = 0; i < frame_size; i++)
    {
        const uint32_t index = (output.geometry.ring_base + i) % buf_size;
        drv->pcm.ring_a[index] = silence ? 0 : output.right[i];
        drv->pcm.ring_b[index] = silence ? 0 : output.left[i];
    }

    drv->pcm.write_cursor += frame_size;
    drv->pcm.pcm_samples_per_vblank = frame_size;
    drv->pcm.pcm_dma_buf_size = buf_size;
}
