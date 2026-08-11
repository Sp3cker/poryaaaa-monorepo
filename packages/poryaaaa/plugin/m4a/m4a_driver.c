#include "m4a_driver.h"
#include "m4a_internal.h"

#include <stdlib.h>
#include <string.h>

static float normalize_pcm_mix_rate(float rate)
{
    if (rate == 0.0f)
        return 0.0f;
    if (!(rate > 0.0f) || rate < 1000.0f)
        return 1000.0f;
    if (rate > (float)M4A_PCM_MAX_RATE_HZ)
        return (float)M4A_PCM_MAX_RATE_HZ;
    return rate;
}

static uint32_t pcm_active_rate(const M4ADriver* drv, float requested_rate)
{
    float rate = requested_rate == 0.0f ? drv->host_rate : requested_rate;
    if (!(rate > 0.0f))
        rate = (float)M4A_PCM_RATE_HZ;
    if (rate > (float)M4A_PCM_MAX_RATE_HZ)
        rate = (float)M4A_PCM_MAX_RATE_HZ;
    if (rate < 1.0f)
        rate = 1.0f;
    return (uint32_t)(rate + 0.5f);
}

static uint32_t pcm_max_samples_per_vblank(uint32_t rate)
{
    const uint64_t numerator = (uint64_t)rate * M4A_PCM_VBLANK_RATE_DENOMINATOR;
    uint32_t samples = (uint32_t)((numerator + M4A_PCM_VBLANK_RATE_NUMERATOR - 1) / M4A_PCM_VBLANK_RATE_NUMERATOR);
    if (samples == 0)
        samples = 1;
    if (samples > M4A_PCM_MAX_SAMPLES_PER_VBLANK)
        samples = M4A_PCM_MAX_SAMPLES_PER_VBLANK;
    return samples;
}

static uint32_t pcm_dma_buf_size(uint32_t rate, uint32_t samples_per_vblank)
{
    uint64_t scaled = (uint64_t)rate * M4A_PCM_DMA_BUF_SIZE;
    uint32_t size = (uint32_t)(scaled / M4A_PCM_RATE_HZ);
    if (size < samples_per_vblank)
        size = samples_per_vblank;
    if (size > M4A_PCM_MAX_DMA_BUF_SIZE)
        size = M4A_PCM_MAX_DMA_BUF_SIZE;
    return size;
}

static uint16_t m4a_soundcnt_h_value(const M4ADriver* drv, uint16_t extra_bits)
{
    uint16_t value = (uint16_t)(drv->regs.psg_volume_code & 3u);
    value |= (uint16_t)(drv->regs.dma_a_volume_code & 1u) << 2u;
    value |= (uint16_t)(drv->regs.dma_b_volume_code & 1u) << 3u;
    if (drv->regs.dma_a_enable_right)
        value |= 1u << 8u;
    if (drv->regs.dma_a_enable_left)
        value |= 1u << 9u;
    if (drv->regs.dma_b_enable_right)
        value |= 1u << 12u;
    if (drv->regs.dma_b_enable_left)
        value |= 1u << 13u;
    return (uint16_t)(value | extra_bits);
}

static void m4a_reset_pcm_fifo_scheduler(M4ADriver* drv)
{
    const uint32_t rate = drv->pcm_rate_hz ? drv->pcm_rate_hz : M4A_PCM_RATE_HZ;
    drv->pcm_fifo_a_source_cursor = 0;
    drv->pcm_fifo_b_source_cursor = 0;
    drv->pcm_fifo_a_read = 0;
    drv->pcm_fifo_a_write = 0;
    drv->pcm_fifo_b_read = 0;
    drv->pcm_fifo_b_write = 0;
    drv->pcm_fifo_a_internal_remaining = 0;
    drv->pcm_fifo_b_internal_remaining = 0;
    drv->next_pcm_timer_cycle = drv->current_cycle + M4A_GBA_CYCLES_PER_SECOND / rate;
    drv->pcm_timer_cycle_remainder = M4A_GBA_CYCLES_PER_SECOND % rate;
}

static void m4a_reset_pcm_output_state(M4ADriver* drv)
{
    memset(drv->pcmMixL, 0, sizeof(drv->pcmMixL));
    memset(drv->pcmMixR, 0, sizeof(drv->pcmMixR));
    memset(drv->reverbBufL, 0, sizeof(drv->reverbBufL));
    memset(drv->reverbBufR, 0, sizeof(drv->reverbBufR));
    drv->reverbPos = 0;
    drv->pcm_vblank_remainder = 0;
    memset(&drv->pcm, 0, sizeof(drv->pcm));
    drv->pcm.pcm_rate_hz = drv->pcm_rate_hz;
    drv->pcm.pcm_dma_buf_size = drv->pcm_dma_buf_size;
    drv->pcm_prefill_pending = true;
}

static void m4a_driver_configure_pcm(M4ADriver* drv, float requested_rate, bool emit_fifo_reset)
{
    requested_rate = normalize_pcm_mix_rate(requested_rate);
    const uint32_t rate = pcm_active_rate(drv, requested_rate);
    const uint32_t max_samples = pcm_max_samples_per_vblank(rate);
    const uint32_t buffer_size = pcm_dma_buf_size(rate, max_samples);
    const bool geometry_changed = drv->pcm_rate_hz != rate || drv->pcm_max_samples_per_vblank != max_samples ||
                                  drv->pcm_dma_buf_size != buffer_size;

    drv->pcm_mix_rate = requested_rate;
    if (!geometry_changed)
        return;

    drv->pcm_rate_hz = rate;
    drv->pcm_max_samples_per_vblank = max_samples;
    drv->pcm_dma_buf_size = buffer_size;
    m4a_reset_pcm_output_state(drv);
    m4a_reset_pcm_fifo_scheduler(drv);

    m4a_internal_refresh_pcm_pitches(drv);
    if (emit_fifo_reset)
        m4a_internal_reset_pcm_output(drv);
}

/* Quantize the host-facing rate once so frame-to-cycle conversion has no
 * floating-point decisions in the render path. */
void m4a_internal_recompute_host_timing(M4ADriver* drv)
{
    if (!drv || !(drv->host_rate > 0.0f) || drv->host_rate >= (float)UINT32_MAX)
    {
        drv->host_rate_hz = 0;
        drv->host_cycle_remainder = 0;
        return;
    }

    uint32_t new_rate = (uint32_t)(drv->host_rate + 0.5f);
    if (new_rate == 0)
    {
        drv->host_rate_hz = 0;
        drv->host_cycle_remainder = 0;
        return;
    }

    if (drv->host_rate_hz)
        drv->host_cycle_remainder = drv->host_cycle_remainder * new_rate / drv->host_rate_hz;
    drv->host_rate_hz = new_rate;
}

M4ADriver* m4a_driver_create(float host_sample_rate)
{
    M4ADriver* drv = (M4ADriver*)calloc(1, sizeof(*drv));
    if (!drv)
        return NULL;
    drv->host_rate = host_sample_rate;
    m4a_internal_recompute_host_timing(drv);
    drv->next_vblank_cycle = M4A_VBLANK_CYCLES;
    drv->event_range_begin_cycle = 0;
    drv->event_cycle = 0;
    m4a_driver_configure_pcm(drv, (float)M4A_PCM_RATE_HZ, false);
    drv->song_volume = 127;
    drv->master_volume = 12;
    drv->tempo_bpm = 120.0;
    drv->c15 = 14;
    /* Raw v2 callers have the complete PCM pool unless they explicitly
     * configure a narrower one.  The legacy facade applies its own default. */
    drv->max_pcm_channels = M4A_MAX_PCM_CHANNELS;

    /* Register-file defaults match what real m4a writes during init —
     * NR50/NR51/SOUNDCNT_H/SOUNDBIAS  See
     * pokeemerald m4a.c m4a_init for the canonical writes. */
    drv->regs.psg_master_enabled = true;
    drv->regs.master_vol_left = 7;   /* NR50 high nibble: max */
    drv->regs.master_vol_right = 7;  /* NR50 low nibble:  max */
    drv->regs.psg_volume_code = 2;   /* SOUNDCNT_H bits 1-0: 100% */
    drv->regs.dma_a_volume_code = 1; /* SOUNDCNT_H bit 2: 100% */
    drv->regs.dma_b_volume_code = 1; /* SOUNDCNT_H bit 3: 100% */
    /* Pokemon Emerald's m4a_init writes SOUND_A_RIGHT_OUTPUT |
     * SOUND_B_LEFT_OUTPUT (m4a.c:352–354): DMA A → right, DMA B → left.
     * Other games may configure differently; the chip honours whatever
     * the register file says at render time. */
    drv->regs.dma_a_enable_right = true;
    drv->regs.dma_b_enable_left = true;
    drv->regs.bias_level = 0x200;
    drv->regs.bias_sampling_cycle = 1; /* m4a_init selects the 65536 Hz DAC rate */

    /* m4a tempo defaults: ply_tempo defaults to 75, and the doubling step
     * (D = ply_tempo*2) is canonical from MPlayMain. */
    drv->tempoD = 150;
    drv->tempoU = 0x100;
    drv->tempoI = drv->tempoD;
    drv->tempoC = 0;

    /* All four CGB channel `type` slots populated; chip indexing matches the
     * register-file layout (sq1, sq2, wave, noise).  panMask = 0xFF means
     * "no per-channel routing restriction" — m4a_chn_vol_set_cgb's
     * ch->pan &= ch->panMask must not zero pan when the cgb_pan() result
     * is 0xFF (centered) or 0x0F/0xF0 (hard-panned). */
    for (int i = 0; i < M4A_MAX_CGB_CHANNELS; i++)
    {
        drv->cgb[i].type = (uint8_t)(i + 1);
        drv->cgb[i].trackIndex = -1;
        drv->cgb[i].panMask = 0xFF;
    }

    /* MIDI tracks without CC7 start at full volume before song scaling. */
    for (int i = 0; i < M4A_MAX_TRACKS; i++)
    {
        drv->tracks[i].rawVolume = 127;
        drv->tracks[i].volume = 127;
        drv->tracks[i].volX = 64;
        drv->tracks[i].pan = 0;
        drv->tracks[i].bendRange = 2;
        drv->tracks[i].lfoSpeed = 22;
    }

    return drv;
}

void m4a_driver_destroy(M4ADriver* drv)
{
    free(drv);
}

void m4a_driver_set_host_rate(M4ADriver* drv, float hz)
{
    if (!drv)
        return;
    drv->host_rate = hz;
    m4a_internal_recompute_host_timing(drv);
    if (drv->pcm_mix_rate == 0.0f)
        m4a_driver_configure_pcm(drv, 0.0f, true);
}

void m4a_driver_set_pcm_mix_rate(M4ADriver* drv, float rate)
{
    if (!drv)
        return;
    m4a_driver_configure_pcm(drv, rate, true);
}

void m4a_driver_set_xcmd_callback(M4ADriver* drv, M4ADriverXcmdFn fn, void* ctx)
{
    if (!drv)
        return;
    drv->xcmd_fn = fn;
    drv->xcmd_ctx = ctx;
}

void m4a_driver_set_voicegroup(M4ADriver* drv, ToneData* vg)
{
    if (!drv)
        return;
    drv->voicegroup = vg;
}

void m4a_driver_refresh_voices(M4ADriver* drv)
{
    if (!drv || !drv->voicegroup)
        return;
    /* re-copy each track's voice from
     * the (possibly edited) voicegroup so currentVoice picks up GUI-side
     * tweaks for already-programmed tracks.  No channel state changes —
     * the next note_on uses the refreshed voice. */
    for (int i = 0; i < M4A_MAX_TRACKS; i++)
        drv->tracks[i].currentVoice = drv->voicegroup[drv->tracks[i].currentProgram];
}

/* m4a_set_song_volume lives in m4a_track.c: it has to recompute every
 * track's effective volume and refresh active CGB channel volumes, which
 * is most of the track-side surface area. */

void m4a_set_master_volume(M4ADriver* drv, uint8_t volume)
{
    if (!drv)
        return;
    drv->master_volume = volume;
}

void m4a_set_reverb_amount(M4ADriver* drv, uint8_t amount)
{
    if (!drv)
        return;
    drv->reverb_amount = amount;
}

void m4a_set_analog_filter(M4ADriver* drv, bool enabled)
{
    if (!drv)
        return;
    drv->analog_filter = enabled;
}

void m4a_set_max_pcm_channels(M4ADriver* drv, uint8_t maxChannels)
{
    if (!drv)
        return;
    drv->max_pcm_channels = maxChannels;
}

void m4a_set_tempo_bpm(M4ADriver* drv, double bpm)
{
    if (!drv)
        return;
    drv->tempo_bpm = bpm;
    /* MPlayMain: tempoI = (tempoD * tempoU) >> 8.  We map bpm → tempoU as
     * (bpm/120)*256, keeping tempoD at its 150 default. */
    if (bpm > 0.0)
    {
        double scale = bpm / 120.0;
        if (scale < 0.0)
            scale = 0.0;
        if (scale > 16.0)
            scale = 16.0;
        drv->tempoU = (uint16_t)(scale * 256.0 + 0.5);
        drv->tempoI = (uint16_t)(((uint32_t)drv->tempoD * drv->tempoU) >> 8);
    }
}

const M4ARegisterFile* m4a_get_register_file(const M4ADriver* drv)
{
    return drv ? &drv->regs : NULL;
}

M4ARegisterFile* m4a_get_register_file_mut(M4ADriver* drv)
{
    return drv ? &drv->regs : NULL;
}

const M4APcmRing* m4a_get_pcm_ring(const M4ADriver* drv)
{
    return drv ? &drv->pcm : NULL;
}

/* Append one event in the stable order established for the current cycle. */
void m4a_internal_emit_event(M4ADriver* drv, M4ARegId reg, uint32_t value)
{
    if (!drv)
        return;
    if (drv->event_count >= M4A_EVENT_QUEUE_CAP || drv->event_next_order == UINT32_MAX)
    {
        drv->events_dropped++;
        return;
    }

    M4ARegWrite* ev = &drv->events[drv->event_count++];
    ev->cycle = drv->event_cycle;
    ev->reg = reg;
    ev->value = value;
    ev->order = drv->event_next_order++;
}

/* Reset the software source and hardware FIFO queues at the current cycle. */
void m4a_internal_reset_pcm_output(M4ADriver* drv)
{
    if (!drv)
        return;

    m4a_reset_pcm_output_state(drv);
    m4a_reset_pcm_fifo_scheduler(drv);
    if (drv->event_count == 0 || drv->event_cycle != drv->current_cycle)
    {
        drv->event_cycle = drv->current_cycle;
        drv->event_next_order = 0;
    }
    m4a_internal_emit_event(drv, M4A_REG_SOUNDCNT_H, m4a_soundcnt_h_value(drv, 0x8800u));
}

const M4ARegWriteBatch* m4a_get_pending_writes(const M4ADriver* drv)
{
    if (!drv)
        return NULL;
    /* The batch struct lives inside M4ADriver; rebuild its view so it
     * always reflects the queue and absolute render interval. */
    M4ARegWriteBatch* batch = (M4ARegWriteBatch*)&drv->event_batch;
    batch->events = drv->events;
    batch->count = drv->event_count;
    batch->begin_cycle = drv->event_range_begin_cycle;
    batch->end_cycle = drv->current_cycle;
    return batch;
}

uint32_t m4a_get_events_dropped(const M4ADriver* drv)
{
    return drv ? drv->events_dropped : 0;
}

/* Exposes the driver-side endpoint consumed by the hardware renderer. */
uint64_t m4a_driver_current_cycle(const M4ADriver* drv)
{
    return drv ? drv->current_cycle : 0;
}

/* Rebase only an untouched queue so the first VBlank remains one full period away. */
bool m4a_driver_set_initial_cycle(M4ADriver* drv, uint64_t cycle)
{
    if (!drv || drv->event_count != 0 || cycle > UINT64_MAX - M4A_VBLANK_CYCLES)
        return false;
    const uint32_t pcm_rate = drv->pcm_rate_hz ? drv->pcm_rate_hz : M4A_PCM_RATE_HZ;
    const uint32_t pcm_period = M4A_GBA_CYCLES_PER_SECOND / pcm_rate;
    if (cycle > UINT64_MAX - pcm_period)
        return false;

    drv->current_cycle = cycle;
    drv->next_vblank_cycle = cycle + M4A_VBLANK_CYCLES;
    m4a_reset_pcm_fifo_scheduler(drv);
    drv->event_range_begin_cycle = cycle;
    drv->event_cycle = cycle;
    drv->event_next_order = 0;
    return true;
}

void m4a_consume_writes(M4ADriver* drv)
{
    if (!drv)
        return;
    drv->event_count = 0;
    drv->event_range_begin_cycle = drv->current_cycle;
    drv->event_cycle = drv->current_cycle;
    drv->event_next_order = 0;

    /* Snapshot trigger latches are conceptually a side-effect view of
     * the same NRx4-with-trigger event the queue carried.  Once the
     * chip has consumed the batch, the latches must reset — otherwise
     * any later snapshot consumer (UI, debug, hw_audio_render fallback)
     * would see ghost triggers.  Mirrors §6a "Edge-trigger latches.
     * Driver sets ... Chip clears after consuming." */
    drv->regs.trigger_sq1 = false;
    drv->regs.trigger_sq2 = false;
    drv->regs.trigger_wave = false;
    drv->regs.trigger_noise = false;
}
