#include "m4a_engine.h"

#include <stdlib.h>
#include <string.h>

#include "hw_audio/hw_audio.h"
#include "m4a_tables.h"

_Static_assert(M4A_ENGINE_MAX_PROCESS_FRAMES == M4A_RECOMMENDED_MAX_ADVANCE_FRAMES,
               "compatibility render chunk must match the driver queue limit");
_Static_assert(MAX_PCM_CHANNELS == M4A_MAX_PCM_CHANNELS, "legacy and current PCM pools must agree");

extern void m4a_drv_cgb_disable(M4ADriver* drv, M4ADriverCgbChan* ch, int idx);

static void copy_track_to_driver(const M4ATrack* source, M4ADriverTrack* destination)
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

static void copy_track_from_driver(M4ATrack* destination, const M4ADriverTrack* source)
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

static uint8_t clamp_pcm_channels(uint8_t count)
{
    return count > MAX_PCM_CHANNELS ? MAX_PCM_CHANNELS : count;
}

static void apply_driver_config(M4AEngine* engine, M4ADriver* driver, uint8_t maxPcmChannels)
{
    if (!driver)
        return;
    m4a_driver_set_voicegroup(driver, engine->voiceGroup);
    for (int i = 0; i < MAX_TRACKS; ++i)
        copy_track_to_driver(&engine->tracks[i], &driver->tracks[i]);
    m4a_set_master_volume(driver, engine->masterVolume);
    m4a_set_song_volume(driver, engine->songMasterVolume);
    m4a_set_reverb_amount(driver, engine->reverb.amount);
    m4a_set_analog_filter(driver, engine->analogFilter);
    m4a_set_max_pcm_channels(driver, maxPcmChannels);
    m4a_driver_set_portamento_enabled(driver, engine->portamentoEnabled);
    m4a_driver_set_pwm_enabled(driver, engine->pwmEnabled);
}

static void apply_public_state(M4AEngine* engine)
{
    engine->maxPcmChannels = clamp_pcm_channels(engine->maxPcmChannels);
    engine->reverbAmount = engine->reverb.amount;
    engine->volume = engine->songMasterVolume;
    apply_driver_config(engine, engine->driver, engine->maxPcmChannels);
    apply_driver_config(engine, engine->shadowDriver, MAX_PCM_CHANNELS);
    apply_driver_config(engine, engine->auditionDriver, MAX_PCM_CHANNELS);
}

static void copy_pcm_channel(M4APCMChannel* destination, const M4ADriverPcmChan* source, bool audition)
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

static void copy_cgb_channel(M4ACGBChannel* destination, const M4ADriverCgbChan* source, bool audition)
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

static void sync_channels(M4AEngine* engine)
{
    if (engine->driver)
    {
        for (int i = 0; i < MAX_PCM_CHANNELS; ++i)
        {
            if (!(engine->driver->pcmChans[i].status & M4A_CHN_ON))
                engine->primaryPcmAudition[i] = false;
            copy_pcm_channel(&engine->pcmChannels[i], &engine->driver->pcmChans[i], engine->primaryPcmAudition[i]);
        }
        for (int i = 0; i < MAX_CGB_CHANNELS; ++i)
        {
            if (!(engine->driver->cgb[i].status & M4A_CHN_ON))
                engine->primaryCgbAudition[i] = false;
            copy_cgb_channel(&engine->cgbChannels[i], &engine->driver->cgb[i], engine->primaryCgbAudition[i]);
        }
    }
    if (engine->shadowDriver)
    {
        for (int i = 0; i < MAX_PCM_CHANNELS; ++i)
        {
            if (!(engine->shadowDriver->pcmChans[i].status & M4A_CHN_ON))
                engine->shadowPcmAudition[i] = false;
            copy_pcm_channel(&engine->pcmChannels[MAX_PCM_CHANNELS + i],
                             &engine->shadowDriver->pcmChans[i],
                             engine->shadowPcmAudition[i]);
        }
        for (int i = 0; i < MAX_CGB_CHANNELS; ++i)
            copy_cgb_channel(&engine->cgbChannels[MAX_CGB_CHANNELS + i],
                             &engine->shadowDriver->cgb[i],
                             engine->shadowCgbAudition[i]);
    }
}

static void sync_public_state(M4AEngine* engine)
{
    if (!engine->driver)
        return;
    const M4ADriver* driver = engine->driver;
    engine->sampleRate = driver->host_rate;
    engine->samplesPerTick = (float)((double)driver->host_rate * M4A_VBLANK_CYCLES / M4A_GBA_CYCLES_PER_SECOND);
    engine->tickAccumulator =
        (float)((double)(driver->current_cycle % M4A_VBLANK_CYCLES) * driver->host_rate / M4A_GBA_CYCLES_PER_SECOND);
    engine->masterVolume = driver->master_volume;
    engine->songMasterVolume = driver->song_volume;
    engine->volume = driver->song_volume;
    engine->reverb.amount = driver->reverb_amount;
    engine->reverbAmount = driver->reverb_amount;
    engine->maxPcmChannels = clamp_pcm_channels(driver->max_pcm_channels);
    engine->analogFilter = driver->analog_filter;
    engine->c15 = driver->c15;
    engine->tempoD = driver->tempoD;
    engine->tempoU = driver->tempoU;
    engine->tempoI = driver->tempoI;
    engine->tempoC = driver->tempoC;
    engine->pwmActiveFlag = driver->pwm_active;
    for (int i = 0; i < MAX_TRACKS; ++i)
        copy_track_from_driver(&engine->tracks[i], &driver->tracks[i]);
    sync_channels(engine);
}

static void record_poly_event(M4AEngine* engine, uint8_t type, uint8_t track, uint8_t key, uint8_t byTrack)
{
    const uint32_t index = engine->polyEventTotal % M4A_POLY_EVENT_CAPACITY;
    M4APolyEvent* event = &engine->polyEvents[index];
    if (track < MAX_TRACKS)
    {
        if (type == M4A_POLY_DROPPED)
            ++engine->polyDropCount[track];
        else if (type == M4A_POLY_STOLEN)
            ++engine->polyStealCount[track];
        else
            ++engine->polyTailCutCount[track];
        event->program = engine->tracks[track].currentProgram;
    }
    else
    {
        event->program = 0;
    }
    event->type = type;
    event->trackIndex = track;
    event->midiKey = key;
    event->byTrack = byTrack;
    event->tick = engine->polyEventClock;
    ++engine->polyEventTotal;
}

static ToneData* resolve_voice(ToneData* voice, uint8_t key)
{
    if (!voice)
        return NULL;
    if (voice->type & VOICE_KEYSPLIT_ALL)
    {
        ToneData* group = (ToneData*)voice->subGroup;
        if (!group)
            return NULL;
        voice = &group[key];
    }
    else if (voice->type & VOICE_KEYSPLIT)
    {
        ToneData* group = (ToneData*)voice->subGroup;
        if (!group || !voice->keySplitTable)
            return NULL;
        voice = &group[voice->keySplitTable[key]];
    }
    return (voice->type & (VOICE_KEYSPLIT | VOICE_KEYSPLIT_ALL)) ? NULL : voice;
}

static M4ADriverPcmChan* select_pcm_channel(M4ADriver* driver, uint8_t priority, int track)
{
    M4ADriverPcmChan* best = NULL;
    uint8_t bestPriority = priority;
    int bestTrack = track;
    bool bestStopping = false;
    for (int i = 0; i < clamp_pcm_channels(driver->max_pcm_channels); ++i)
    {
        M4ADriverPcmChan* channel = &driver->pcmChans[i];
        if (!(channel->status & M4A_CHN_ON))
            return channel;
        if (channel->status & M4A_CHN_STOP)
        {
            if (!bestStopping || channel->priority < bestPriority ||
                (channel->priority == bestPriority && channel->trackIndex >= bestTrack))
            {
                bestStopping = true;
                bestPriority = channel->priority;
                bestTrack = channel->trackIndex;
                best = channel;
            }
        }
        else if (!bestStopping && (channel->priority < bestPriority ||
                                   (channel->priority == bestPriority && channel->trackIndex >= bestTrack)))
        {
            bestPriority = channel->priority;
            bestTrack = channel->trackIndex;
            best = channel;
        }
    }
    return best && (bestStopping || priority >= bestPriority) ? best : NULL;
}

static void sync_driver_phase(M4ADriver* destination, const M4ADriver* source)
{
    destination->host_rate_hz = source->host_rate_hz;
    destination->host_cycle_remainder = source->host_cycle_remainder;
    destination->current_cycle = source->current_cycle;
    destination->next_vcount_cycle = source->next_vcount_cycle;
    destination->next_vblank_cycle = source->next_vblank_cycle;
    destination->next_pcm_timer_cycle = source->next_pcm_timer_cycle;
    destination->event_range_begin_cycle = source->event_range_begin_cycle;
    destination->event_cycle = source->event_cycle;
    destination->event_next_order = source->event_next_order;
    destination->tempoD = source->tempoD;
    destination->tempoU = source->tempoU;
    destination->tempoI = source->tempoI;
    destination->tempoC = source->tempoC;
    destination->c15 = source->c15;
}

static void reset_sidecar(M4ADriver* driver, HwAudio* hw)
{
    m4a_all_sound_off(driver);
    m4a_consume_writes(driver);
    hw_audio_reset(hw);
}

static void clear_invert_sidecars(M4AEngine* engine)
{
    reset_sidecar(engine->shadowDriver, engine->shadowHw);
    reset_sidecar(engine->auditionDriver, engine->auditionHw);
    memset(engine->primaryPcmAudition, 0, sizeof(engine->primaryPcmAudition));
    memset(engine->primaryCgbAudition, 0, sizeof(engine->primaryCgbAudition));
    memset(engine->shadowPcmAudition, 0, sizeof(engine->shadowPcmAudition));
    memset(engine->shadowCgbAudition, 0, sizeof(engine->shadowCgbAudition));
}

static void prepare_invert_sidecars(M4AEngine* engine)
{
    clear_invert_sidecars(engine);
    sync_driver_phase(engine->shadowDriver, engine->driver);
    sync_driver_phase(engine->auditionDriver, engine->driver);
    hw_audio_sync_psg_timing(engine->shadowHw, engine->hw);
    hw_audio_sync_psg_timing(engine->auditionHw, engine->hw);
}

static void render_driver(M4ADriver* driver, HwAudio* hw, float* left, float* right, int frames)
{
    m4a_advance(driver, frames);
    hw_audio_render_events(hw, m4a_get_pending_writes(driver), left, right, frames);
    m4a_consume_writes(driver);
}

static void xcmd_adapter(void* context, int track, uint8_t selector, uint32_t value)
{
    M4AEngine* engine = context;
    if (engine->xcmd_fn)
        engine->xcmd_fn(engine->xcmd_ctx, track, selector, value);
}

bool m4a_engine_init(M4AEngine* engine, float sampleRate)
{
    if (!engine)
        return false;
    memset(engine, 0, sizeof(*engine));
    engine->sampleRate = sampleRate;
    engine->samplesPerTick = sampleRate / VBLANK_RATE;
    engine->pcmMixRate = 13379.0f;
    engine->masterVolume = 12;
    engine->songMasterVolume = MAX_SONG_VOLUME;
    engine->volume = MAX_SONG_VOLUME;
    engine->maxPcmChannels = 12;
    engine->polyEventClock = M4A_POLY_TICK_NONE;
    for (int i = 0; i < MAX_TRACKS; ++i)
    {
        engine->tracks[i].bendRange = 2;
        engine->tracks[i].volX = 64;
        engine->tracks[i].rawVolume = 127;
        engine->tracks[i].volume = 127;
        engine->tracks[i].lfoSpeed = 22;
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
    apply_public_state(engine);
    sync_public_state(engine);
    return true;
}

M4AEngine* m4a_engine_create(float sampleRate)
{
    M4AEngine* engine = malloc(sizeof(*engine));
    if (!engine)
        return NULL;
    if (!m4a_engine_init(engine, sampleRate))
    {
        free(engine);
        return NULL;
    }
    return engine;
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

void m4a_engine_free(M4AEngine* engine)
{
    if (!engine)
        return;
    m4a_engine_destroy(engine);
    free(engine);
}

bool m4a_engine_reset(M4AEngine* engine)
{
    if (!engine)
        return false;
    const float sampleRate = engine->sampleRate;
    m4a_engine_destroy(engine);
    return m4a_engine_init(engine, sampleRate);
}

void m4a_engine_set_xcmd_callback(M4AEngine* engine, M4AEngineXcmdFn callback, void* context)
{
    if (!engine || !engine->driver)
        return;
    engine->xcmd_fn = callback;
    engine->xcmd_ctx = context;
    m4a_driver_set_xcmd_callback(engine->driver, xcmd_adapter, engine);
}

void m4a_engine_set_pcm_mix_rate(M4AEngine* engine, float rate)
{
    if (!engine)
        return;
    if (rate && rate < 1000.0f)
        rate = 1000.0f;
    else if (rate > (float)M4A_PCM_MAX_RATE_HZ)
        rate = (float)M4A_PCM_MAX_RATE_HZ;
    engine->pcmMixRate = rate;
    m4a_driver_set_pcm_mix_rate(engine->driver, rate);
    m4a_driver_set_pcm_mix_rate(engine->shadowDriver, rate);
    m4a_driver_set_pcm_mix_rate(engine->auditionDriver, rate);
    m4a_reverb_destroy(&engine->reverb);
    m4a_reverb_init(&engine->reverb, rate ? rate : engine->sampleRate, engine->reverbAmount);
    apply_public_state(engine);
    sync_public_state(engine);
}

void m4a_engine_set_voicegroup(M4AEngine* engine, ToneData* voiceGroup)
{
    if (!engine)
        return;
    engine->voiceGroup = voiceGroup;
    apply_public_state(engine);
    sync_public_state(engine);
}

void m4a_engine_refresh_voices(M4AEngine* engine)
{
    if (!engine)
        return;
    apply_public_state(engine);
    m4a_driver_refresh_voices(engine->driver);
    m4a_driver_refresh_voices(engine->shadowDriver);
    m4a_driver_refresh_voices(engine->auditionDriver);
    sync_public_state(engine);
}

void m4a_engine_note_on(M4AEngine* engine, int track, uint8_t key, uint8_t velocity)
{
    if (!engine || !engine->driver || track < 0 || track >= MAX_TRACKS || key > 127)
        return;
    apply_public_state(engine);
    ToneData* voice = resolve_voice(&engine->tracks[track].currentVoice, key);
    if (!voice)
    {
        sync_public_state(engine);
        return;
    }
    const bool audition = engine->auditionNote;
    const uint8_t voiceType = voice->type & VOICE_TYPE_CGB_MASK;
    if (voiceType >= 1 && voiceType <= MAX_CGB_CHANNELS)
    {
        const int index = voiceType - 1;
        M4ADriverCgbChan victim = engine->driver->cgb[index];
        const bool active = (victim.status & M4A_CHN_ON) != 0;
        const bool dropped = active && !(victim.status & M4A_CHN_STOP) &&
                             (victim.priority > engine->tracks[track].priority ||
                              (victim.priority == engine->tracks[track].priority && victim.trackIndex < track));
        if (dropped)
        {
            record_poly_event(engine, M4A_POLY_DROPPED, (uint8_t)track, key, (uint8_t)track);
            if (engine->polyDebugInvert)
            {
                m4a_note_on(engine->shadowDriver, track, key, velocity);
                engine->shadowCgbAudition[index] = audition;
            }
        }
        else
        {
            if (active && victim.trackIndex != track)
            {
                record_poly_event(engine,
                                  (victim.status & M4A_CHN_STOP) ? M4A_POLY_TAIL_CUT : M4A_POLY_STOLEN,
                                  (uint8_t)victim.trackIndex,
                                  victim.midiKey,
                                  (uint8_t)track);
                if (engine->polyDebugInvert)
                {
                    engine->shadowDriver->cgb[index] = victim;
                    engine->shadowCgbAudition[index] = engine->primaryCgbAudition[index];
                    hw_audio_clone_psg_lane(engine->shadowHw, engine->hw, index);
                }
            }
            m4a_note_on(engine->driver, track, key, velocity);
            engine->primaryCgbAudition[index] = audition;
            if (engine->polyDebugInvert && audition)
                m4a_note_on(engine->auditionDriver, track, key, velocity);
        }
    }
    else
    {
        if (!voice->wav)
        {
            sync_public_state(engine);
            return;
        }
        M4ADriverPcmChan* channel = select_pcm_channel(engine->driver, engine->tracks[track].priority, track);
        const bool dropped = channel == NULL;
        int index = -1;
        M4ADriverPcmChan victim;
        if (dropped)
        {
            record_poly_event(engine, M4A_POLY_DROPPED, (uint8_t)track, key, (uint8_t)track);
            if (engine->polyDebugInvert)
                m4a_note_on(engine->shadowDriver, track, key, velocity);
        }
        else
        {
            index = (int)(channel - engine->driver->pcmChans);
            victim = *channel;
            if (victim.status & M4A_CHN_ON)
            {
                record_poly_event(engine,
                                  (victim.status & M4A_CHN_STOP) ? M4A_POLY_TAIL_CUT : M4A_POLY_STOLEN,
                                  (uint8_t)victim.trackIndex,
                                  victim.midiKey,
                                  (uint8_t)track);
                if (engine->polyDebugInvert)
                {
                    M4ADriverPcmChan* shadow =
                        select_pcm_channel(engine->shadowDriver, victim.priority, victim.trackIndex);
                    if (shadow)
                    {
                        const int shadowIndex = (int)(shadow - engine->shadowDriver->pcmChans);
                        *shadow = victim;
                        engine->shadowPcmAudition[shadowIndex] = engine->primaryPcmAudition[index];
                    }
                }
            }
            m4a_note_on(engine->driver, track, key, velocity);
            engine->primaryPcmAudition[index] = audition;
            if (engine->polyDebugInvert && audition)
                m4a_note_on(engine->auditionDriver, track, key, velocity);
        }
    }
    sync_public_state(engine);
}

void m4a_engine_note_off(M4AEngine* engine, int track, uint8_t key)
{
    if (!engine || track < 0 || track >= MAX_TRACKS)
        return;
    apply_public_state(engine);
    m4a_note_off(engine->driver, track, key);
    m4a_note_off(engine->shadowDriver, track, key);
    m4a_note_off(engine->auditionDriver, track, key);
    sync_public_state(engine);
}

void m4a_engine_program_change(M4AEngine* engine, int track, uint8_t program)
{
    if (!engine || !engine->voiceGroup || track < 0 || track >= MAX_TRACKS)
        return;
    apply_public_state(engine);
    m4a_program_change(engine->driver, track, program);
    m4a_program_change(engine->shadowDriver, track, program);
    m4a_program_change(engine->auditionDriver, track, program);
    sync_public_state(engine);
}

void m4a_engine_cc(M4AEngine* engine, int track, uint8_t cc, uint8_t value)
{
    if (!engine || track < 0 || track >= MAX_TRACKS)
        return;
    apply_public_state(engine);
    m4a_cc(engine->driver, track, cc, value);
    m4a_cc(engine->shadowDriver, track, cc, value);
    m4a_cc(engine->auditionDriver, track, cc, value);
    sync_public_state(engine);
}

void m4a_engine_pitch_bend(M4AEngine* engine, int track, int16_t bend)
{
    if (!engine || track < 0 || track >= MAX_TRACKS)
        return;
    apply_public_state(engine);
    m4a_pitch_bend(engine->driver, track, bend);
    m4a_pitch_bend(engine->shadowDriver, track, bend);
    m4a_pitch_bend(engine->auditionDriver, track, bend);
    sync_public_state(engine);
}

void m4a_engine_all_notes_off(M4AEngine* engine, int track)
{
    if (!engine || track < 0 || track >= MAX_TRACKS)
        return;
    apply_public_state(engine);
    m4a_all_notes_off(engine->driver, track);
    m4a_all_notes_off(engine->shadowDriver, track);
    m4a_all_notes_off(engine->auditionDriver, track);
    sync_public_state(engine);
}

void m4a_engine_all_sound_off(M4AEngine* engine)
{
    if (!engine)
        return;
    apply_public_state(engine);
    m4a_all_sound_off(engine->driver);
    m4a_all_sound_off(engine->shadowDriver);
    m4a_all_sound_off(engine->auditionDriver);
    memset(engine->primaryPcmAudition, 0, sizeof(engine->primaryPcmAudition));
    memset(engine->primaryCgbAudition, 0, sizeof(engine->primaryCgbAudition));
    memset(engine->shadowPcmAudition, 0, sizeof(engine->shadowPcmAudition));
    memset(engine->shadowCgbAudition, 0, sizeof(engine->shadowCgbAudition));
    m4a_engine_reset_portamento(engine);
    sync_public_state(engine);
}

void m4a_engine_reset_portamento(M4AEngine* engine)
{
    if (!engine)
        return;
    for (int i = 0; i < MAX_TRACKS; ++i)
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
        m4a_engine_reset_portamento(engine);
    apply_public_state(engine);
    sync_public_state(engine);
}

void m4a_engine_set_pwm_enabled(M4AEngine* engine, bool enabled)
{
    if (!engine)
        return;
    engine->pwmEnabled = enabled;
    if (!enabled)
    {
        for (int i = 0; i < MAX_TRACKS; ++i)
        {
            engine->tracks[i].pwmPattern = 0;
            engine->tracks[i].pwmSpeed = 0;
            engine->tracks[i].pwmSpeedCounter = 0;
            engine->tracks[i].pwmStep = 0;
        }
    }
    apply_public_state(engine);
    sync_public_state(engine);
}

void m4a_engine_set_poly_debug_invert(M4AEngine* engine, bool enabled)
{
    if (!engine)
        return;
    apply_public_state(engine);
    if (enabled && !engine->polyDebugInvert)
        prepare_invert_sidecars(engine);
    else if (!enabled && engine->polyDebugInvert)
        clear_invert_sidecars(engine);
    engine->polyDebugInvert = enabled;
    sync_public_state(engine);
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
    apply_public_state(engine);
    sync_public_state(engine);
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
    apply_public_state(engine);
    sync_public_state(engine);
}

void m4a_engine_set_max_pcm_channels(M4AEngine* engine, uint8_t maxChannels)
{
    if (!engine)
        return;
    engine->maxPcmChannels = clamp_pcm_channels(maxChannels);
    apply_public_state(engine);
    sync_public_state(engine);
}

void m4a_engine_set_analog_filter(M4AEngine* engine, bool enabled)
{
    if (!engine)
        return;
    engine->analogFilter = enabled;
    apply_public_state(engine);
    sync_public_state(engine);
}

void m4a_engine_set_tempo_bpm(M4AEngine* engine, double bpm)
{
    if (!engine)
        return;
    apply_public_state(engine);
    m4a_set_tempo_bpm(engine->driver, bpm);
    m4a_set_tempo_bpm(engine->shadowDriver, bpm);
    m4a_set_tempo_bpm(engine->auditionDriver, bpm);
    sync_public_state(engine);
}

void m4a_engine_process(M4AEngine* engine, float* outL, float* outR, int samples)
{
    if (!engine || !engine->driver || !engine->hw || !outL || !outR || samples <= 0)
        return;
    apply_public_state(engine);
    for (int offset = 0; offset < samples;)
    {
        const int frames =
            samples - offset > M4A_ENGINE_MAX_PROCESS_FRAMES ? M4A_ENGINE_MAX_PROCESS_FRAMES : samples - offset;
        float* left = outL + offset;
        float* right = outR + offset;
        if (engine->polyDebugInvert)
        {
            render_driver(engine->driver, engine->hw, engine->invertScratchL, engine->invertScratchR, frames);
            render_driver(engine->shadowDriver, engine->shadowHw, left, right, frames);
            render_driver(
                engine->auditionDriver, engine->auditionHw, engine->invertScratchL, engine->invertScratchR, frames);
            for (int i = 0; i < frames; ++i)
            {
                left[i] += engine->invertScratchL[i];
                right[i] += engine->invertScratchR[i];
            }
        }
        else
        {
            render_driver(engine->driver, engine->hw, left, right, frames);
        }
        offset += frames;
    }
    sync_public_state(engine);
}

void m4a_engine_tick(M4AEngine* engine)
{
    if (engine)
        sync_public_state(engine);
}

void m4a_track_vol_pit_set(M4ATrack* track)
{
    if (!track)
        return;
    int32_t volume = ((uint32_t)track->volume * track->volX) >> 5;
    if (track->modT == 1)
        volume = ((uint32_t)volume * (track->modM + 128)) >> 7;
    int32_t pan = 2 * track->pan + track->panX;
    if (track->modT == 2)
        pan += track->modM;
    if (pan < -128)
        pan = -128;
    else if (pan > 127)
        pan = 127;
    track->volMR = (uint8_t)(((uint32_t)((pan + 128) * volume)) >> 8);
    track->volML = (uint8_t)(((uint32_t)((127 - pan) * volume)) >> 8);
    int32_t pitch = (track->tune + (int32_t)track->bend * track->bendRange) * 4 + ((int32_t)track->keyShift << 8) +
                    ((int32_t)track->keyShiftX << 8) + track->pitX;
    if (track->modT == 0)
        pitch += 16 * track->modM;
    track->keyM = (int8_t)(pitch >> 8);
    track->pitM = (uint8_t)pitch;
}

uint32_t m4a_midi_key_to_freq(WaveData* wav, uint8_t key, uint8_t fineAdjust)
{
    if (!wav)
        return 0;
    uint32_t shifted = (uint32_t)fineAdjust << 24;
    if (key > 178)
    {
        key = 178;
        shifted = 255u << 24;
    }
    uint32_t first = gScaleTable[key];
    first = gFreqTable[first & 0xFu] >> (first >> 4);
    uint32_t second = gScaleTable[key + 1];
    second = gFreqTable[second & 0xFu] >> (second >> 4);
    return umul3232H32(wav->freq, first + umul3232H32(second - first, shifted));
}

uint32_t m4a_midi_key_to_cgb_freq(uint8_t channel, uint8_t key, uint8_t fineAdjust)
{
    if (channel == 4)
    {
        if (key <= 20)
            key = 0;
        else if ((key -= 21) > 59)
            key = 59;
        return gNoiseTable[key];
    }
    if (key <= 35)
    {
        fineAdjust = 0;
        key = 0;
    }
    else if ((key -= 36) > 130)
    {
        key = 130;
        fineAdjust = 255;
    }
    int32_t first = gCgbScaleTable[key];
    first = gCgbFreqTable[first & 0xFu] >> (first >> 4);
    int32_t second = gCgbScaleTable[key + 1];
    second = gCgbFreqTable[second & 0xFu] >> (second >> 4);
    return (uint32_t)(first + ((fineAdjust * (second - first)) >> 8) + 2048);
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
