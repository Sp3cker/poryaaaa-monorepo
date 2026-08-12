#include "m4a_engine.h"

#include <stdlib.h>
#include <string.h>

#include "hw_audio/hw_audio.h"
#include "m4a/m4a_driver.h"
#include "m4a/m4a_internal.h"
#include "m4a_tables.h"

_Static_assert(M4A_ENGINE_MAX_PROCESS_FRAMES == M4A_RECOMMENDED_MAX_ADVANCE_FRAMES,
               "M4A_ENGINE_MAX_PROCESS_FRAMES must match v2 driver queue limit");
_Static_assert(MAX_PCM_CHANNELS == M4A_MAX_PCM_CHANNELS, "legacy and v2 PCM pools must agree");

extern void m4a_drv_cgb_disable(M4ADriver* drv, M4ADriverCgbChan* ch, int idx);

uint32_t m4a_midi_key_to_freq(WaveData* wav, uint8_t key, uint8_t fineAdjust)
{
    uint32_t val1;
    uint32_t val2;
    uint32_t fineAdjustShifted = (uint32_t)fineAdjust << 24;

    if (key > 178)
    {
        key = 178;
        fineAdjustShifted = 255u << 24;
    }

    val1 = gScaleTable[key];
    val1 = gFreqTable[val1 & 0xFu] >> (val1 >> 4);
    val2 = gScaleTable[key + 1];
    val2 = gFreqTable[val2 & 0xFu] >> (val2 >> 4);
    return umul3232H32(wav->freq, val1 + umul3232H32(val2 - val1, fineAdjustShifted));
}

uint32_t m4a_midi_key_to_cgb_freq(uint8_t chanNum, uint8_t key, uint8_t fineAdjust)
{
    if (chanNum == 4)
    {
        if (key <= 20)
            key = 0;
        else
        {
            key -= 21;
            if (key > 59)
                key = 59;
        }
        return gNoiseTable[key];
    }

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

    int32_t val1 = gCgbScaleTable[key];
    val1 = gCgbFreqTable[val1 & 0xFu] >> (val1 >> 4);
    int32_t val2 = gCgbScaleTable[key + 1];
    val2 = gCgbFreqTable[val2 & 0xFu] >> (val2 >> 4);
    return (uint32_t)(val1 + ((fineAdjust * (val2 - val1)) >> 8) + 2048);
}

static void m4a_engine_xcmd_adapter(void* ctx, int trackIndex, uint8_t selector, uint32_t value)
{
    M4AEngine* engine = (M4AEngine*)ctx;
    if (engine->xcmd_fn)
        engine->xcmd_fn(engine->xcmd_ctx, trackIndex, selector, value);
}

static ToneData* compat_resolve_voice(ToneData* voice, uint8_t key)
{
    if (!voice)
        return NULL;

    if (voice->type & VOICE_KEYSPLIT_ALL)
    {
        ToneData* subGroup = (ToneData*)voice->subGroup;
        if (!subGroup)
            return NULL;
        ToneData* resolved = &subGroup[key];
        return (resolved->type & (VOICE_KEYSPLIT | VOICE_KEYSPLIT_ALL)) ? NULL : resolved;
    }

    if (voice->type & VOICE_KEYSPLIT)
    {
        ToneData* subGroup = (ToneData*)voice->subGroup;
        if (!subGroup || !voice->keySplitTable)
            return NULL;
        ToneData* resolved = &subGroup[voice->keySplitTable[key]];
        return (resolved->type & (VOICE_KEYSPLIT | VOICE_KEYSPLIT_ALL)) ? NULL : resolved;
    }

    return voice;
}

static void compat_track_to_driver(const M4ATrack* source, M4ADriverTrack* destination)
{
    destination->flags = source->flags;
    destination->volume = source->volume;
    destination->rawVolume = source->rawVolume;
    destination->volX = source->volX;
    destination->pan = source->pan;
    destination->panX = source->panX;
    destination->bend = source->bend;
    destination->bendRange = source->bendRange;
    destination->lfoSpeed = source->lfoSpeed;
    destination->lfoSpeedC = source->lfoSpeedC;
    destination->lfoDelay = source->lfoDelay;
    destination->lfoDelayC = source->lfoDelayC;
    destination->mod = source->mod;
    destination->modT = source->modT;
    destination->modM = source->modM;
    destination->keyShift = source->keyShift;
    destination->keyShiftX = source->keyShiftX;
    destination->tune = source->tune;
    destination->pitX = source->pitX;
    destination->keyM = source->keyM;
    destination->pitM = source->pitM;
    destination->volMR = source->volMR;
    destination->volML = source->volML;
    destination->pseudoEchoVolume = source->pseudoEchoVolume;
    destination->pseudoEchoLength = source->pseudoEchoLength;
    destination->portamentoDuration = source->portamentoDuration;
    destination->portamentoPrevKey = source->portamentoPrevKey;
    destination->portamentoTargetKey = source->portamentoTargetKey;
    destination->portamentoGliding = source->portamentoGliding;
    destination->portamentoElapsed = source->portamentoElapsed;
    destination->pwmPattern = source->pwmPattern;
    destination->pwmSpeed = source->pwmSpeed;
    destination->pwmSpeedCounter = source->pwmSpeedCounter;
    destination->pwmStep = source->pwmStep;
    destination->priority = source->priority;
    destination->currentProgram = source->currentProgram;
    destination->currentVoice = source->currentVoice;
}

static void compat_track_from_driver(M4ATrack* destination, const M4ADriverTrack* source)
{
    destination->flags = source->flags;
    destination->volume = source->volume;
    destination->rawVolume = source->rawVolume;
    destination->volX = source->volX;
    destination->pan = source->pan;
    destination->panX = source->panX;
    destination->bend = source->bend;
    destination->bendRange = source->bendRange;
    destination->lfoSpeed = source->lfoSpeed;
    destination->lfoSpeedC = source->lfoSpeedC;
    destination->lfoDelay = source->lfoDelay;
    destination->lfoDelayC = source->lfoDelayC;
    destination->mod = source->mod;
    destination->modT = source->modT;
    destination->modM = source->modM;
    destination->keyShift = source->keyShift;
    destination->keyShiftX = source->keyShiftX;
    destination->tune = source->tune;
    destination->pitX = source->pitX;
    destination->keyM = source->keyM;
    destination->pitM = source->pitM;
    destination->volMR = source->volMR;
    destination->volML = source->volML;
    destination->pseudoEchoVolume = source->pseudoEchoVolume;
    destination->pseudoEchoLength = source->pseudoEchoLength;
    destination->portamentoDuration = source->portamentoDuration;
    destination->portamentoPrevKey = source->portamentoPrevKey;
    destination->portamentoTargetKey = source->portamentoTargetKey;
    destination->portamentoGliding = source->portamentoGliding;
    destination->portamentoElapsed = source->portamentoElapsed;
    destination->pwmPattern = source->pwmPattern;
    destination->pwmSpeed = source->pwmSpeed;
    destination->pwmSpeedCounter = source->pwmSpeedCounter;
    destination->pwmStep = source->pwmStep;
    destination->priority = source->priority;
    destination->currentProgram = source->currentProgram;
    destination->currentVoice = source->currentVoice;
}

static void compat_tracks_to_driver(const M4AEngine* engine, M4ADriver* driver)
{
    if (!driver)
        return;

    for (int i = 0; i < MAX_TRACKS; i++)
        compat_track_to_driver(&engine->tracks[i], &driver->tracks[i]);
}

static uint8_t compat_clamp_pcm_count(uint8_t count)
{
    return count > MAX_PCM_CHANNELS ? MAX_PCM_CHANNELS : count;
}

static void compat_apply_driver_config(M4AEngine* engine, M4ADriver* driver, uint8_t maxPcmChannels)
{
    if (!driver)
        return;

    m4a_driver_set_voicegroup(driver, engine->voiceGroup);
    compat_tracks_to_driver(engine, driver);
    m4a_set_master_volume(driver, engine->masterVolume);
    m4a_set_song_volume(driver, engine->songMasterVolume);
    m4a_set_reverb_amount(driver, engine->reverb.amount);
    m4a_set_analog_filter(driver, engine->analogFilter);
    m4a_set_max_pcm_channels(driver, maxPcmChannels);
    driver->compat_respect_base_midi_key = engine->respectBaseMidiKey;
    driver->compat_portamento_enabled = engine->portamentoEnabled;
    driver->compat_pwm_enabled = engine->pwmEnabled;
    driver->compat_pwm_active = engine->pwmActiveFlag;
    driver->compat_zero_pcm_is_silent = driver == engine->driver;
    driver->compat_skip_pwm_tick = driver == engine->shadowDriver;
}

/* Apply direct legacy writes before every ingress or render boundary. */
static void compat_apply_public_state(M4AEngine* engine)
{
    engine->maxPcmChannels = compat_clamp_pcm_count(engine->maxPcmChannels);
    engine->reverbAmount = engine->reverb.amount;
    engine->volume = engine->songMasterVolume;

    compat_apply_driver_config(engine, engine->driver, engine->maxPcmChannels);
    compat_apply_driver_config(engine, engine->shadowDriver, MAX_PCM_CHANNELS);
    compat_apply_driver_config(engine, engine->auditionDriver, MAX_PCM_CHANNELS);
}

static void compat_copy_pcm_channel(M4APCMChannel* destination, const M4ADriverPcmChan* source, bool audition)
{
    const bool active = (source->status & M4A_CHN_ON) != 0;

    memset(destination, 0, sizeof(*destination));
    destination->status = source->status;
    destination->type = source->type;
    destination->rightVolume = source->rightVolume;
    destination->leftVolume = source->leftVolume;
    destination->attack = source->attack;
    destination->decay = source->decay;
    destination->sustain = source->sustain;
    destination->release = source->release;
    destination->key = source->key;
    destination->envelopeVolume = source->envelopeVolume;
    destination->envelopeVolumeRight = source->envelopeVolumeRight;
    destination->envelopeVolumeLeft = source->envelopeVolumeLeft;
    destination->pseudoEchoVolume = source->pseudoEchoVolume;
    destination->pseudoEchoLength = source->pseudoEchoLength;
    destination->midiKey = source->midiKey;
    destination->velocity = source->velocity;
    destination->priority = source->priority;
    destination->rhythmPan = source->rhythmPan;
    destination->gateTime = source->gateTime;
    destination->trackIndex = active ? source->trackIndex : -1;
    destination->audition = active && audition;

    if (!active)
        return;

    destination->wav = source->wav;
    destination->currentPointer = source->currentPointer;
    destination->count = source->count;
    destination->fw = source->fw;
    destination->frequency = source->frequency;
    destination->isLoop = source->isLoop;
    destination->loopLen = source->loopLen;
    destination->loopStart = source->loopStart;
    destination->synthType = source->synthType;
    destination->synthPulseDuty = source->synthPulseDuty;
}

static void compat_copy_cgb_channel(M4ACGBChannel* destination, const M4ADriverCgbChan* source, bool audition)
{
    const bool active = (source->status & M4A_CHN_ON) != 0;

    memset(destination, 0, sizeof(*destination));
    destination->status = source->status;
    destination->type = source->type;
    destination->rightVolume = source->rightVolume;
    destination->leftVolume = source->leftVolume;
    destination->attack = source->attack;
    destination->decay = source->decay;
    destination->sustain = source->sustain;
    destination->release = source->release;
    destination->key = source->key;
    destination->envelopeVolume = source->envelopeVolume;
    destination->envelopeGoal = source->envelopeGoal;
    destination->envelopeCounter = source->envelopeCounter;
    destination->pseudoEchoVolume = source->pseudoEchoVolume;
    destination->pseudoEchoLength = source->pseudoEchoLength;
    destination->midiKey = source->midiKey;
    destination->velocity = source->velocity;
    destination->priority = source->priority;
    destination->rhythmPan = source->rhythmPan;
    destination->gateTime = source->gateTime;
    destination->sustainGoal = source->sustainGoal;
    destination->length = source->length;
    destination->sweep = source->sweep;
    destination->dutyCycle = source->dutyCycle;
    destination->pan = source->pan;
    destination->panMask = source->panMask;
    destination->modify = source->modify;
    destination->frequency = source->frequency;
    destination->trackIndex = active ? source->trackIndex : -1;
    destination->audition = active && audition;

    if (active)
        destination->wavePointer = source->wavePointer;
}

static void compat_sync_channels(M4AEngine* engine)
{
    if (engine->driver)
    {
        for (int i = 0; i < MAX_PCM_CHANNELS; i++)
        {
            if (!(engine->driver->pcmChans[i].status & M4A_CHN_ON))
            {
                engine->primaryPcmAudition[i] = false;
                engine->primaryPcmAuditionSlot[i] = -1;
            }
            compat_copy_pcm_channel(
                &engine->pcmChannels[i], &engine->driver->pcmChans[i], engine->primaryPcmAudition[i]);
        }
        for (int i = 0; i < MAX_CGB_CHANNELS; i++)
        {
            if (!(engine->driver->cgb[i].status & M4A_CHN_ON))
                engine->primaryCgbAudition[i] = false;
            compat_copy_cgb_channel(&engine->cgbChannels[i], &engine->driver->cgb[i], engine->primaryCgbAudition[i]);
        }
    }

    if (engine->shadowDriver)
    {
        for (int i = 0; i < MAX_PCM_CHANNELS; i++)
        {
            if (!(engine->shadowDriver->pcmChans[i].status & M4A_CHN_ON))
                engine->shadowPcmAudition[i] = false;
            compat_copy_pcm_channel(&engine->pcmChannels[MAX_PCM_CHANNELS + i],
                                    &engine->shadowDriver->pcmChans[i],
                                    engine->shadowPcmAudition[i]);
        }
        for (int i = 0; i < MAX_CGB_CHANNELS; i++)
        {
            if (!(engine->shadowDriver->cgb[i].status & M4A_CHN_ON))
                engine->shadowCgbAudition[i] = false;
            compat_copy_cgb_channel(&engine->cgbChannels[MAX_CGB_CHANNELS + i],
                                    &engine->shadowDriver->cgb[i],
                                    engine->shadowCgbAudition[i]);
        }
    }
}

static void compat_sync_public_state(M4AEngine* engine)
{
    if (!engine->driver)
        return;

    M4ADriver* driver = engine->driver;
    engine->sampleRate = driver->host_rate;
    engine->samplesPerTick =
        (float)((double)driver->host_rate * (double)M4A_VBLANK_CYCLES / (double)M4A_GBA_CYCLES_PER_SECOND);
    engine->tickAccumulator = (float)((double)(driver->current_cycle % M4A_VBLANK_CYCLES) * (double)driver->host_rate /
                                      (double)M4A_GBA_CYCLES_PER_SECOND);
    engine->masterVolume = driver->master_volume;
    engine->songMasterVolume = driver->song_volume;
    engine->volume = driver->song_volume;
    engine->reverb.amount = driver->reverb_amount;
    engine->reverbAmount = driver->reverb_amount;
    engine->maxPcmChannels = compat_clamp_pcm_count(driver->max_pcm_channels);
    engine->analogFilter = driver->analog_filter;
    engine->c15 = driver->c15;
    engine->tempoD = driver->tempoD;
    engine->tempoU = driver->tempoU;
    engine->tempoI = driver->tempoI;
    engine->tempoC = driver->tempoC;
    engine->pwmActiveFlag = driver->compat_pwm_active;

    for (int i = 0; i < MAX_TRACKS; i++)
        compat_track_from_driver(&engine->tracks[i], &driver->tracks[i]);

    compat_sync_channels(engine);
}

static void
compat_record_poly_event(M4AEngine* engine, uint8_t type, uint8_t trackIndex, uint8_t midiKey, uint8_t byTrack)
{
    uint8_t program = 0;
    if (trackIndex < MAX_TRACKS)
    {
        if (type == M4A_POLY_DROPPED)
            engine->polyDropCount[trackIndex]++;
        else if (type == M4A_POLY_STOLEN)
            engine->polyStealCount[trackIndex]++;
        else if (type == M4A_POLY_TAIL_CUT)
            engine->polyTailCutCount[trackIndex]++;
        program = engine->tracks[trackIndex].currentProgram;
    }

    M4APolyEvent* event = &engine->polyEvents[engine->polyEventTotal % M4A_POLY_EVENT_CAPACITY];
    event->type = type;
    event->trackIndex = trackIndex;
    event->midiKey = midiKey;
    event->byTrack = byTrack;
    event->program = program;
    event->tick = engine->polyEventClock;
    engine->polyEventTotal++;
}

/* The allocation rules are intentionally byte-for-byte equivalent to the v2
 * driver and the legacy facade.  They let the compatibility layer account for
 * a loss before m4a_note_on overwrites its victim. */
static M4ADriverPcmChan* compat_select_pcm_channel(M4ADriver* driver, uint8_t priority, int trackIndex)
{
    M4ADriverPcmChan* best = NULL;
    uint8_t bestPriority = priority;
    int bestTrackIndex = trackIndex;
    bool bestIsStopping = false;
    const uint8_t maxPcm = compat_clamp_pcm_count(driver->max_pcm_channels);

    for (int i = 0; i < maxPcm; i++)
    {
        M4ADriverPcmChan* channel = &driver->pcmChans[i];
        if (!(channel->status & M4A_CHN_ON))
            return channel;

        if (channel->status & M4A_CHN_STOP)
        {
            if (!bestIsStopping)
            {
                bestIsStopping = true;
                bestPriority = channel->priority;
                bestTrackIndex = channel->trackIndex;
                best = channel;
            }
            else if (channel->priority < bestPriority ||
                     (channel->priority == bestPriority && channel->trackIndex >= bestTrackIndex))
            {
                bestPriority = channel->priority;
                bestTrackIndex = channel->trackIndex;
                best = channel;
            }
            continue;
        }

        if (!bestIsStopping && (channel->priority < bestPriority ||
                                (channel->priority == bestPriority && channel->trackIndex >= bestTrackIndex)))
        {
            bestPriority = channel->priority;
            bestTrackIndex = channel->trackIndex;
            best = channel;
        }
    }

    if (best && (bestIsStopping || priority >= bestPriority))
        return best;
    return NULL;
}

static void compat_sync_driver_phase(M4ADriver* destination, const M4ADriver* source)
{
    if (!destination || !source)
        return;

    destination->host_rate_hz = source->host_rate_hz;
    destination->host_cycle_remainder = source->host_cycle_remainder;
    destination->current_cycle = source->current_cycle;
    destination->next_vblank_cycle = source->next_vblank_cycle;
    destination->event_range_begin_cycle = source->event_range_begin_cycle;
    destination->event_cycle = source->event_cycle;
    destination->event_next_order = source->event_next_order;
    destination->tempoD = source->tempoD;
    destination->tempoU = source->tempoU;
    destination->tempoI = source->tempoI;
    destination->tempoC = source->tempoC;
    destination->c15 = source->c15;
}

static void compat_reset_sidecar(M4ADriver* driver, HwAudio* hw)
{
    m4a_all_sound_off(driver);
    m4a_consume_writes(driver);
    hw_audio_reset(hw);
}

static void compat_forget_audition_pcm_slot(M4AEngine* engine, int slot)
{
    for (int i = 0; i < MAX_PCM_CHANNELS; i++)
    {
        if (engine->primaryPcmAuditionSlot[i] == slot)
            engine->primaryPcmAuditionSlot[i] = -1;
    }
}

static void compat_remove_audition_pcm(M4AEngine* engine, int primaryIndex, const M4ADriverPcmChan* victim)
{
    int slot = engine->primaryPcmAuditionSlot[primaryIndex];
    if (slot < 0 || slot >= MAX_PCM_CHANNELS || !(engine->auditionDriver->pcmChans[slot].status & M4A_CHN_ON))
    {
        slot = -1;
        for (int i = 0; i < MAX_PCM_CHANNELS; i++)
        {
            const M4ADriverPcmChan* candidate = &engine->auditionDriver->pcmChans[i];
            if ((candidate->status & M4A_CHN_ON) && candidate->trackIndex == victim->trackIndex &&
                candidate->midiKey == victim->midiKey && candidate->wav == victim->wav)
            {
                slot = i;
                break;
            }
        }
    }
    if (slot >= 0)
    {
        M4ADriverPcmChan* channel = &engine->auditionDriver->pcmChans[slot];
        channel->status = 0;
        channel->envelopeVolume = 0;
        channel->envelopeVolumeLeft = 0;
        channel->envelopeVolumeRight = 0;
        compat_forget_audition_pcm_slot(engine, slot);
    }
    engine->primaryPcmAuditionSlot[primaryIndex] = -1;
    m4a_internal_reset_pcm_output(engine->auditionDriver);
}

static void compat_remove_audition_cgb(M4AEngine* engine, int cgbIndex, const M4ADriverCgbChan* victim)
{
    M4ADriverCgbChan* channel = &engine->auditionDriver->cgb[cgbIndex];
    if (!(channel->status & M4A_CHN_ON) || channel->trackIndex != victim->trackIndex ||
        channel->midiKey != victim->midiKey)
        return;

    channel->status = 0;
    channel->modify = 0;
    channel->freshStart = false;
    channel->waveRamPending = false;
    m4a_drv_cgb_disable(engine->auditionDriver, channel, cgbIndex);
}

static void
compat_preserve_shadow_pcm(M4AEngine* engine, const M4ADriverPcmChan* victim, int primaryIndex, bool audition)
{
    M4ADriverPcmChan* shadow = compat_select_pcm_channel(engine->shadowDriver, victim->priority, victim->trackIndex);
    if (!shadow)
        return;

    const int index = (int)(shadow - engine->shadowDriver->pcmChans);
    *shadow = *victim;
    engine->shadowPcmAudition[index] = audition;
    if (audition)
        compat_remove_audition_pcm(engine, primaryIndex, victim);
}

static void compat_preserve_shadow_cgb(M4AEngine* engine, const M4ADriverCgbChan* victim, int cgbIndex, bool audition)
{
    M4ADriverCgbChan* shadow = &engine->shadowDriver->cgb[cgbIndex];
    *shadow = *victim;
    engine->shadowCgbAudition[cgbIndex] = audition;
    hw_audio_clone_psg_lane(engine->shadowHw, engine->hw, cgbIndex);
    if (audition)
        compat_remove_audition_cgb(engine, cgbIndex, victim);
}

static void compat_note_on_shadow(M4ADriver* driver, int trackIndex, uint8_t key, uint8_t velocity)
{
    const bool wasShadowNote = driver->compat_shadow_note;
    driver->compat_shadow_note = true;
    m4a_note_on(driver, trackIndex, key, velocity);
    driver->compat_shadow_note = wasShadowNote;
}

static void compat_start_shadow_pcm(M4AEngine* engine, int trackIndex, uint8_t key, uint8_t velocity, bool audition)
{
    M4ADriverPcmChan* slot =
        compat_select_pcm_channel(engine->shadowDriver, engine->shadowDriver->tracks[trackIndex].priority, trackIndex);
    if (!slot)
        return;

    const int index = (int)(slot - engine->shadowDriver->pcmChans);
    compat_note_on_shadow(engine->shadowDriver, trackIndex, key, velocity);
    if (slot->status & M4A_CHN_ON)
        engine->shadowPcmAudition[index] = audition;
}

static void
compat_start_shadow_cgb(M4AEngine* engine, int trackIndex, uint8_t key, uint8_t velocity, int cgbIndex, bool audition)
{
    compat_note_on_shadow(engine->shadowDriver, trackIndex, key, velocity);
    if (engine->shadowDriver->cgb[cgbIndex].status & M4A_CHN_ON)
        engine->shadowCgbAudition[cgbIndex] = audition;
}

static void
compat_start_audition_pcm(M4AEngine* engine, int primaryIndex, int trackIndex, uint8_t key, uint8_t velocity)
{
    M4ADriverPcmChan* slot = compat_select_pcm_channel(
        engine->auditionDriver, engine->auditionDriver->tracks[trackIndex].priority, trackIndex);
    if (!slot)
    {
        engine->primaryPcmAuditionSlot[primaryIndex] = -1;
        return;
    }

    const int slotIndex = (int)(slot - engine->auditionDriver->pcmChans);
    compat_forget_audition_pcm_slot(engine, slotIndex);
    m4a_note_on(engine->auditionDriver, trackIndex, key, velocity);
    if ((slot->status & M4A_CHN_ON) && slot->trackIndex == trackIndex && slot->midiKey == key)
        engine->primaryPcmAuditionSlot[primaryIndex] = (int8_t)slotIndex;
    else
        engine->primaryPcmAuditionSlot[primaryIndex] = -1;
}

static void compat_render_driver(M4ADriver* driver, HwAudio* hw, float* outL, float* outR, int frames)
{
    m4a_advance(driver, frames);
    hw_audio_render_events(hw, m4a_get_pending_writes(driver), outL, outR, frames);
    m4a_consume_writes(driver);
}

static void compat_apply_analog_filter(M4AEngine* engine, float* outL, float* outR, int frames)
{
    if (!engine->analogFilter)
        return;

    for (int i = 0; i < frames; i++)
    {
        engine->lowPassLeft = engine->lowPassLeft * 0.6f + outL[i] * 0.4f;
        engine->lowPassRight = engine->lowPassRight * 0.6f + outR[i] * 0.4f;
        outL[i] = engine->lowPassLeft;
        outR[i] = engine->lowPassRight;
    }
}

static void compat_capture_active_auditions(M4AEngine* engine)
{
    memset(engine->primaryPcmAuditionSlot, -1, sizeof(engine->primaryPcmAuditionSlot));
    for (int i = 0; i < MAX_PCM_CHANNELS; i++)
    {
        const M4ADriverPcmChan* source = &engine->driver->pcmChans[i];
        if (!(source->status & M4A_CHN_ON) || !engine->primaryPcmAudition[i])
            continue;

        M4ADriverPcmChan* destination =
            compat_select_pcm_channel(engine->auditionDriver, source->priority, source->trackIndex);
        if (!destination)
            continue;

        *destination = *source;
        engine->primaryPcmAuditionSlot[i] = (int8_t)(destination - engine->auditionDriver->pcmChans);
    }

    for (int i = 0; i < MAX_CGB_CHANNELS; i++)
    {
        const M4ADriverCgbChan* source = &engine->driver->cgb[i];
        if (!(source->status & M4A_CHN_ON) || !engine->primaryCgbAudition[i])
            continue;

        engine->auditionDriver->cgb[i] = *source;
        hw_audio_clone_psg_lane(engine->auditionHw, engine->hw, i);
    }
}

static void compat_clear_invert_sidecars(M4AEngine* engine)
{
    compat_reset_sidecar(engine->shadowDriver, engine->shadowHw);
    compat_reset_sidecar(engine->auditionDriver, engine->auditionHw);
    memset(engine->shadowPcmAudition, 0, sizeof(engine->shadowPcmAudition));
    memset(engine->shadowCgbAudition, 0, sizeof(engine->shadowCgbAudition));
    memset(engine->primaryPcmAuditionSlot, -1, sizeof(engine->primaryPcmAuditionSlot));
}

static void compat_prepare_invert_sidecars(M4AEngine* engine)
{
    compat_clear_invert_sidecars(engine);
    compat_sync_driver_phase(engine->shadowDriver, engine->driver);
    compat_sync_driver_phase(engine->auditionDriver, engine->driver);
    hw_audio_sync_psg_timing(engine->shadowHw, engine->hw);
    hw_audio_sync_psg_timing(engine->auditionHw, engine->hw);
}

bool m4a_engine_init(M4AEngine* engine, float sampleRate)
{
    if (!engine)
        return false;

    memset(engine, 0, sizeof(*engine));
    engine->sampleRate = sampleRate;
    engine->samplesPerTick = sampleRate / VBLANK_RATE;
    engine->pcmMixRate = 13379.0f;
    engine->masterVolume = 15;
    engine->songMasterVolume = MAX_SONG_VOLUME;
    engine->volume = MAX_SONG_VOLUME;
    engine->maxPcmChannels = 5;
    engine->c15 = 0;
    memset(engine->primaryPcmAuditionSlot, -1, sizeof(engine->primaryPcmAuditionSlot));
    engine->polyEventClock = M4A_POLY_TICK_NONE;
    engine->tempoD = 150;
    engine->tempoU = 0x100;
    engine->tempoI = 150;

    for (int i = 0; i < MAX_TRACKS; i++)
    {
        M4ATrack* track = &engine->tracks[i];
        track->bendRange = 2;
        track->volX = 64;
        track->rawVolume = 127;
        track->volume = 127;
        track->lfoSpeed = 22;
    }

    m4a_reverb_init(&engine->reverb, engine->pcmMixRate, 0);

    engine->driver = m4a_driver_create(sampleRate);
    engine->hw = hw_audio_create(sampleRate);
    engine->shadowDriver = m4a_driver_create(sampleRate);
    engine->shadowHw = hw_audio_create(sampleRate);
    engine->auditionDriver = m4a_driver_create(sampleRate);
    engine->auditionHw = hw_audio_create(sampleRate);
    if (!engine->driver || !engine->hw || !engine->shadowDriver || !engine->shadowHw || !engine->auditionDriver ||
        !engine->auditionHw)
    {
        m4a_engine_destroy(engine);
        return false;
    }
    engine->driver->c15 = 0;
    engine->shadowDriver->c15 = 0;
    engine->auditionDriver->c15 = 0;

    compat_apply_public_state(engine);
    compat_sync_public_state(engine);
    return true;
}

M4AEngine* m4a_engine_create(float sampleRate)
{
    M4AEngine* engine = (M4AEngine*)malloc(sizeof(*engine));
    if (!engine)
        return NULL;
    if (!m4a_engine_init(engine, sampleRate))
    {
        free(engine);
        return NULL;
    }
    return engine;
}

void m4a_engine_free(M4AEngine* engine)
{
    if (!engine)
        return;
    m4a_engine_destroy(engine);
    free(engine);
}

void m4a_engine_destroy(M4AEngine* engine)
{
    if (!engine)
        return;

    m4a_reverb_destroy(&engine->reverb);
    hw_audio_destroy(engine->auditionHw);
    m4a_driver_destroy(engine->auditionDriver);
    hw_audio_destroy(engine->shadowHw);
    m4a_driver_destroy(engine->shadowDriver);
    hw_audio_destroy(engine->hw);
    m4a_driver_destroy(engine->driver);
    memset(engine, 0, sizeof(*engine));
}

bool m4a_engine_reset(M4AEngine* engine)
{
    if (!engine)
        return false;

    const float sampleRate = engine->sampleRate;
    m4a_engine_destroy(engine);
    return m4a_engine_init(engine, sampleRate);
}

void m4a_engine_set_xcmd_callback(M4AEngine* engine, M4AEngineXcmdFn xcmd_fn, void* xcmd_ctx)
{
    if (!engine || !engine->driver)
        return;

    engine->xcmd_fn = xcmd_fn;
    engine->xcmd_ctx = xcmd_ctx;
    m4a_driver_set_xcmd_callback(engine->driver, m4a_engine_xcmd_adapter, engine);
    /* Sidecar command streams update their own tracks but must not notify the
     * host a second or third time. */
    m4a_driver_set_xcmd_callback(engine->shadowDriver, NULL, NULL);
    m4a_driver_set_xcmd_callback(engine->auditionDriver, NULL, NULL);
}

void m4a_engine_set_pcm_mix_rate(M4AEngine* engine, float rate)
{
    if (!engine)
        return;

    if (rate != 0.0f)
    {
        if (rate < 1000.0f)
            rate = 1000.0f;
        else if (rate > (float)M4A_PCM_MAX_RATE_HZ)
            rate = (float)M4A_PCM_MAX_RATE_HZ;
    }

    engine->pcmMixRate = rate;
    m4a_driver_set_pcm_mix_rate(engine->driver, rate);
    m4a_driver_set_pcm_mix_rate(engine->shadowDriver, rate);
    m4a_driver_set_pcm_mix_rate(engine->auditionDriver, rate);

    const uint8_t amount = engine->reverb.amount;
    const float activeRate =
        engine->driver ? (float)engine->driver->pcm_rate_hz : (rate > 0.0f ? rate : engine->sampleRate);
    m4a_reverb_destroy(&engine->reverb);
    m4a_reverb_init(&engine->reverb, activeRate, amount);
    engine->pcmResampleAccum = 0.0f;
    engine->pcmPrevL = 0;
    engine->pcmPrevR = 0;
    engine->pcmCurL = 0;
    engine->pcmCurR = 0;
    compat_apply_public_state(engine);
    compat_sync_public_state(engine);
}

void m4a_engine_set_voicegroup(M4AEngine* engine, ToneData* voiceGroup)
{
    if (!engine)
        return;

    engine->voiceGroup = voiceGroup;
    m4a_driver_set_voicegroup(engine->driver, voiceGroup);
    m4a_driver_set_voicegroup(engine->shadowDriver, voiceGroup);
    m4a_driver_set_voicegroup(engine->auditionDriver, voiceGroup);
    compat_sync_public_state(engine);
}

void m4a_engine_refresh_voices(M4AEngine* engine)
{
    if (!engine)
        return;

    compat_apply_public_state(engine);
    m4a_driver_refresh_voices(engine->driver);
    m4a_driver_refresh_voices(engine->shadowDriver);
    m4a_driver_refresh_voices(engine->auditionDriver);
    compat_sync_public_state(engine);
}

void m4a_engine_note_on(M4AEngine* engine, int trackIndex, uint8_t key, uint8_t velocity)
{
    if (!engine || !engine->driver || trackIndex < 0 || trackIndex >= MAX_TRACKS || key > 127)
        return;

    compat_apply_public_state(engine);
    ToneData* voice = compat_resolve_voice(&engine->tracks[trackIndex].currentVoice, key);
    if (!voice)
    {
        compat_sync_public_state(engine);
        return;
    }

    const uint8_t voiceType = voice->type & VOICE_TYPE_CGB_MASK;
    const bool audition = engine->auditionNote;
    const bool invert = engine->polyDebugInvert;

    if (voiceType >= 1 && voiceType <= MAX_CGB_CHANNELS)
    {
        const int cgbIndex = voiceType - 1;
        M4ADriverCgbChan* channel = &engine->driver->cgb[cgbIndex];
        M4ADriverCgbChan victim = *channel;
        bool dropped = false;
        bool lostVictim = false;
        bool victimAudition = engine->primaryCgbAudition[cgbIndex];

        if ((channel->status & M4A_CHN_ON) && !(channel->status & M4A_CHN_STOP) &&
            (channel->priority > engine->tracks[trackIndex].priority ||
             (channel->priority == engine->tracks[trackIndex].priority && channel->trackIndex < trackIndex)))
        {
            dropped = true;
            compat_record_poly_event(engine, M4A_POLY_DROPPED, (uint8_t)trackIndex, key, (uint8_t)trackIndex);
        }
        else if ((channel->status & M4A_CHN_ON) && channel->trackIndex != trackIndex)
        {
            lostVictim = true;
            compat_record_poly_event(engine,
                                     (channel->status & M4A_CHN_STOP) ? M4A_POLY_TAIL_CUT : M4A_POLY_STOLEN,
                                     (uint8_t)channel->trackIndex,
                                     channel->midiKey,
                                     (uint8_t)trackIndex);
        }

        m4a_note_on(engine->driver, trackIndex, key, velocity);
        if (!dropped)
            engine->primaryCgbAudition[cgbIndex] = audition;

        if (invert)
        {
            if (dropped)
                compat_start_shadow_cgb(engine, trackIndex, key, velocity, cgbIndex, audition);
            else if (lostVictim)
                compat_preserve_shadow_cgb(engine, &victim, cgbIndex, victimAudition);

            if (!dropped && audition)
                m4a_note_on(engine->auditionDriver, trackIndex, key, velocity);
        }
    }
    else
    {
        if (!voice->wav)
        {
            compat_sync_public_state(engine);
            return;
        }

        M4ADriverPcmChan* channel =
            compat_select_pcm_channel(engine->driver, engine->tracks[trackIndex].priority, trackIndex);
        M4ADriverPcmChan victim;
        int channelIndex = -1;
        bool dropped = channel == NULL;
        bool lostVictim = false;
        bool victimAudition = false;

        if (dropped)
        {
            compat_record_poly_event(engine, M4A_POLY_DROPPED, (uint8_t)trackIndex, key, (uint8_t)trackIndex);
        }
        else
        {
            channelIndex = (int)(channel - engine->driver->pcmChans);
            victim = *channel;
            if (channel->status & M4A_CHN_ON)
            {
                lostVictim = true;
                victimAudition = engine->primaryPcmAudition[channelIndex];
                compat_record_poly_event(engine,
                                         (channel->status & M4A_CHN_STOP) ? M4A_POLY_TAIL_CUT : M4A_POLY_STOLEN,
                                         (uint8_t)channel->trackIndex,
                                         channel->midiKey,
                                         (uint8_t)trackIndex);
            }
        }

        m4a_note_on(engine->driver, trackIndex, key, velocity);
        if (invert)
        {
            if (dropped)
                compat_start_shadow_pcm(engine, trackIndex, key, velocity, audition);
            else if (lostVictim)
                compat_preserve_shadow_pcm(engine, &victim, channelIndex, victimAudition);
        }
        if (!dropped)
        {
            engine->primaryPcmAudition[channelIndex] = audition;
            engine->primaryPcmAuditionSlot[channelIndex] = -1;
        }
        if (invert && !dropped && audition)
            compat_start_audition_pcm(engine, channelIndex, trackIndex, key, velocity);
    }

    compat_sync_public_state(engine);
}

void m4a_engine_note_off(M4AEngine* engine, int trackIndex, uint8_t key)
{
    if (!engine || trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    compat_apply_public_state(engine);
    m4a_note_off(engine->driver, trackIndex, key);
    m4a_note_off(engine->shadowDriver, trackIndex, key);
    m4a_note_off(engine->auditionDriver, trackIndex, key);
    compat_sync_public_state(engine);
}

void m4a_engine_program_change(M4AEngine* engine, int trackIndex, uint8_t program)
{
    if (!engine || !engine->voiceGroup || trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    compat_apply_public_state(engine);
    m4a_program_change(engine->driver, trackIndex, program);
    m4a_program_change(engine->shadowDriver, trackIndex, program);
    m4a_program_change(engine->auditionDriver, trackIndex, program);
    compat_sync_public_state(engine);
}

void m4a_engine_cc(M4AEngine* engine, int trackIndex, uint8_t cc, uint8_t value)
{
    if (!engine || trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    compat_apply_public_state(engine);
    m4a_cc(engine->driver, trackIndex, cc, value);
    m4a_cc(engine->shadowDriver, trackIndex, cc, value);
    m4a_cc(engine->auditionDriver, trackIndex, cc, value);
    compat_sync_public_state(engine);
}

void m4a_engine_pitch_bend(M4AEngine* engine, int trackIndex, int16_t bend)
{
    if (!engine || trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    compat_apply_public_state(engine);
    m4a_pitch_bend(engine->driver, trackIndex, bend);
    m4a_pitch_bend(engine->shadowDriver, trackIndex, bend);
    m4a_pitch_bend(engine->auditionDriver, trackIndex, bend);
    compat_sync_public_state(engine);
}

void m4a_engine_all_notes_off(M4AEngine* engine, int trackIndex)
{
    if (!engine || trackIndex < 0 || trackIndex >= MAX_TRACKS)
        return;

    compat_apply_public_state(engine);
    m4a_all_notes_off(engine->driver, trackIndex);
    m4a_all_notes_off(engine->shadowDriver, trackIndex);
    m4a_all_notes_off(engine->auditionDriver, trackIndex);
    compat_sync_public_state(engine);
}

void m4a_engine_all_sound_off(M4AEngine* engine)
{
    if (!engine)
        return;

    compat_apply_public_state(engine);
    m4a_all_sound_off(engine->driver);
    m4a_all_sound_off(engine->shadowDriver);
    m4a_all_sound_off(engine->auditionDriver);
    memset(engine->primaryPcmAudition, 0, sizeof(engine->primaryPcmAudition));
    memset(engine->primaryCgbAudition, 0, sizeof(engine->primaryCgbAudition));
    memset(engine->shadowPcmAudition, 0, sizeof(engine->shadowPcmAudition));
    memset(engine->shadowCgbAudition, 0, sizeof(engine->shadowCgbAudition));
    m4a_engine_reset_portamento(engine);
    compat_sync_public_state(engine);
}

void m4a_engine_reset_portamento(M4AEngine* engine)
{
    if (!engine)
        return;

    for (int i = 0; i < MAX_TRACKS; i++)
    {
        engine->tracks[i].portamentoPrevKey = 0;
        engine->tracks[i].portamentoTargetKey = 0;
        engine->tracks[i].portamentoGliding = false;
        engine->tracks[i].portamentoElapsed = 0;
    }
    m4a_internal_reset_portamento(engine->driver);
    m4a_internal_reset_portamento(engine->shadowDriver);
    m4a_internal_reset_portamento(engine->auditionDriver);
}

void m4a_engine_set_portamento_enabled(M4AEngine* engine, bool enabled)
{
    if (!engine)
        return;

    engine->portamentoEnabled = enabled;
    if (!enabled)
    {
        for (int i = 0; i < MAX_TRACKS; i++)
            engine->tracks[i].portamentoDuration = 0;
        m4a_engine_reset_portamento(engine);
    }
    compat_apply_public_state(engine);
    compat_sync_public_state(engine);
}

void m4a_engine_set_pwm_enabled(M4AEngine* engine, bool enabled)
{
    if (!engine)
        return;

    compat_apply_public_state(engine);
    engine->pwmEnabled = enabled;
    if (!enabled)
    {
        engine->pwmActiveFlag = false;
        for (int i = 0; i < MAX_TRACKS; i++)
        {
            engine->tracks[i].pwmPattern = 0;
            engine->tracks[i].pwmSpeed = 0;
            engine->tracks[i].pwmSpeedCounter = 0;
            engine->tracks[i].pwmStep = 0;
        }
        m4a_internal_disable_pwm(engine->driver);
        m4a_internal_disable_pwm(engine->shadowDriver);
        m4a_internal_disable_pwm(engine->auditionDriver);
    }
    compat_apply_public_state(engine);
    compat_sync_public_state(engine);
}

void m4a_engine_set_poly_debug_invert(M4AEngine* engine, bool enabled)
{
    if (!engine)
        return;

    compat_apply_public_state(engine);
    if (enabled && !engine->polyDebugInvert)
    {
        compat_prepare_invert_sidecars(engine);
        compat_capture_active_auditions(engine);
    }
    else if (!enabled && engine->polyDebugInvert)
    {
        compat_clear_invert_sidecars(engine);
    }
    engine->polyDebugInvert = enabled;
    compat_sync_public_state(engine);
}

void m4a_engine_reset_poly_stats(M4AEngine* engine)
{
    if (!engine)
        return;

    memset(engine->polyDropCount, 0, sizeof(engine->polyDropCount));
    memset(engine->polyStealCount, 0, sizeof(engine->polyStealCount));
    memset(engine->polyTailCutCount, 0, sizeof(engine->polyTailCutCount));
    memset(engine->polyEvents, 0, sizeof(engine->polyEvents));
    engine->polyEventTotal = 0;
}

void m4a_engine_set_volume(M4AEngine* engine, uint8_t volume)
{
    if (!engine)
        return;

    engine->volume = volume;
    engine->songMasterVolume = volume;
    compat_apply_public_state(engine);
    compat_sync_public_state(engine);
}

void m4a_engine_set_song_volume(M4AEngine* engine, uint8_t volume)
{
    m4a_engine_set_volume(engine, volume);
}

void m4a_engine_set_reverb_amount(M4AEngine* engine, uint8_t amount)
{
    if (!engine)
        return;

    m4a_reverb_set_amount(&engine->reverb, amount);
    compat_apply_public_state(engine);
    compat_sync_public_state(engine);
}

void m4a_engine_set_max_pcm_channels(M4AEngine* engine, uint8_t max_channels)
{
    if (!engine)
        return;

    engine->maxPcmChannels = compat_clamp_pcm_count(max_channels);
    compat_apply_public_state(engine);
    compat_sync_public_state(engine);
}

void m4a_engine_set_analog_filter(M4AEngine* engine, bool enabled)
{
    if (!engine)
        return;

    engine->analogFilter = enabled;
    compat_apply_public_state(engine);
    compat_sync_public_state(engine);
}

void m4a_engine_set_tempo_bpm(M4AEngine* engine, double bpm)
{
    if (!engine)
        return;
    compat_apply_public_state(engine);

    if (bpm < 1.0)
        bpm = 1.0;
    uint16_t tempoI = (uint16_t)(bpm + 0.5);
    engine->tempoI = tempoI;
    if (engine->driver)
        engine->driver->tempoI = tempoI;
    if (engine->shadowDriver)
        engine->shadowDriver->tempoI = tempoI;
    if (engine->auditionDriver)
        engine->auditionDriver->tempoI = tempoI;
    compat_sync_public_state(engine);
}

void m4a_engine_process(M4AEngine* engine, float* outL, float* outR, int numSamples)
{
    if (!engine || !engine->driver || !engine->hw || numSamples <= 0)
        return;

    compat_apply_public_state(engine);
    int offset = 0;
    while (offset < numSamples)
    {
        const int frames =
            (numSamples - offset > M4A_ENGINE_MAX_PROCESS_FRAMES) ? M4A_ENGINE_MAX_PROCESS_FRAMES : numSamples - offset;
        float* const left = outL + offset;
        float* const right = outR + offset;

        if (engine->polyDebugInvert)
        {
            /* Keep the real v2 stream advancing exactly as normal, but replace
             * its output with the independently clocked lost/audition streams. */
            compat_render_driver(engine->driver, engine->hw, engine->invertScratchL, engine->invertScratchR, frames);
            compat_render_driver(engine->shadowDriver, engine->shadowHw, left, right, frames);
            compat_render_driver(
                engine->auditionDriver, engine->auditionHw, engine->invertScratchL, engine->invertScratchR, frames);
            for (int i = 0; i < frames; i++)
            {
                left[i] += engine->invertScratchL[i];
                right[i] += engine->invertScratchR[i];
            }
        }
        else
        {
            compat_render_driver(engine->driver, engine->hw, left, right, frames);
        }
        compat_apply_analog_filter(engine, left, right, frames);

        offset += frames;
    }

    compat_sync_public_state(engine);
}

void m4a_engine_tick(M4AEngine* engine)
{
    if (!engine || !engine->driver)
        return;

    compat_apply_public_state(engine);
    m4a_internal_compat_tick(engine->driver);
    m4a_internal_compat_tick(engine->shadowDriver);
    m4a_internal_compat_tick(engine->auditionDriver);
    compat_sync_public_state(engine);
}

void m4a_track_vol_pit_set(M4ATrack* track)
{
    if (!track)
        return;

    int32_t x = ((uint32_t)track->volume * track->volX) >> 5;
    if (track->modT == 1)
        x = ((uint32_t)x * (track->modM + 128)) >> 7;

    int32_t y = 2 * track->pan + track->panX;
    if (track->modT == 2)
        y += track->modM;
    if (y < -128)
        y = -128;
    else if (y > 127)
        y = 127;

    track->volMR = (uint8_t)(((uint32_t)((y + 128) * x)) >> 8);
    track->volML = (uint8_t)(((uint32_t)((127 - y) * x)) >> 8);
    int32_t pitch = (track->tune + (int32_t)track->bend * track->bendRange) * 4 + ((int32_t)track->keyShift << 8) +
                    ((int32_t)track->keyShiftX << 8) + track->pitX;
    if (track->modT == 0)
        pitch += 16 * track->modM;
    track->keyM = (int8_t)(pitch >> 8);
    track->pitM = (uint8_t)pitch;
}

M4ADriver* m4a_engine_driver(M4AEngine* engine)
{
    return engine ? engine->driver : NULL;
}

const M4ADriver* m4a_engine_driver_const(const M4AEngine* engine)
{
    return engine ? engine->driver : NULL;
}

HwAudio* m4a_engine_hw_audio(M4AEngine* engine)
{
    return engine ? engine->hw : NULL;
}

const HwAudio* m4a_engine_hw_audio_const(const M4AEngine* engine)
{
    return engine ? engine->hw : NULL;
}
