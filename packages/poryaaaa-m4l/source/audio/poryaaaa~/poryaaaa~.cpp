/*
 * poryaaaa~ — GameBoy Advance m4a sound engine MSP external for Max for Live.
 *
 * Wraps the standalone m4a engine from /Users/.../poryaaaa/plugin/m4a_engine.*
 * (the same DSP core the CLAP plugin uses) into a Max audio object. The .amxd
 * device patch provides UI via live.dial and friends — no GUI lives inside the
 * external itself.
 *
 * MIDI capture: this external now keeps only a thin byte buffer (MidiBuffer).
 * Each event arriving on the right-side `int` inlet (from [midiin]) gets
 * stamped with the latest `beats` value the patcher fed us from plugsync~'s
 * "Ticks (1 PPQ)" outlet. On `dump <path>`, the buffer is written as a
 * fixed-layout binary file and a `dumped <path> <count>` message is emitted
 * on the status outlet. Everything musical (ticks, tracks, loop, validation,
 * SMF format) lives in the [v8 ccomidi_recorder.js] script.
 */

extern "C" {
#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#include "m4a_engine.h"
#include "voicegroup/voicegroup_loader.h"
#if defined(M4A_DRIVER_V2)
#include "m4a/m4a_driver.h"
#endif
#if defined(HW_AUDIO_V2)
#include "hw_audio/hw_audio.h"
#endif
}

#include "recorder/export_capture.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace ccomidi;

typedef struct _porya {
    t_pxobject ob;                    /* MUST be first */
    M4AEngine engine;
#if defined(M4A_DRIVER_V2)
    M4ADriver *m4a_v2;
#endif
#if defined(HW_AUDIO_V2)
    HwAudio *hw_v2;
#endif
    LoadedVoiceGroup *loadedVg;

    float *scratchL;
    float *scratchR;
    long scratchFrames;
    double samplerate;

    /* Rightmost outlet: emits recorder replies such as
     * "dumped <path> <count>" and "dumpfailed <path> <reason>". */
    void *statusOutlet;

    /* Runtime-selected by the Node-owned voicegroup loader path. */
    t_symbol *vgRoot;
    t_symbol *vgName;
    long progSlot[16];
    long songVolume;
    long reverbAmount;
    long analogFilter;
    long maxPcmChannels;

    /* raw MIDI byte parser state — driven by [midiin] → int inlet */
    uint8_t midiStatus;
    uint8_t midiData1;
    uint8_t midiBytesNeeded;
    uint8_t midiBytesGot;
    uint8_t midiInSysex;

    /* ---- recorder ------------------------------------------------------ */
    ExportCapture *capture;
    /* Latched from the patcher's [plugsync~] outlet 6 ("Ticks (1 PPQ)") via
     * the `beats` message. Each captured MIDI event is stamped with the
     * most-recent latched value — that's the only timing fact the C++ side
     * keeps. JS quantizes that beat value to the SMF PPQ grid at Save time. */

} t_porya;

static t_class *porya_class = NULL;

/* ---- forward decls ---- */
static void *porya_new(t_symbol *s, long argc, t_atom *argv);
static void porya_free(t_porya *x);
static void porya_assist(t_porya *x, void *b, long m, long a, char *s);

static void porya_dsp64(t_porya *x, t_object *dsp64, short *count,
                        double samplerate, long maxvectorsize, long flags);
static void porya_perform64(t_porya *x, t_object *dsp64,
                            double **ins, long numins,
                            double **outs, long numouts,
                            long sampleframes, long flags, void *userparam);

static void porya_int(t_porya *x, long byte);
static void porya_midievent(t_porya *x, t_symbol *s, long argc, t_atom *argv);
static void porya_dispatch_event(t_porya *x, uint8_t status, uint8_t d1, uint8_t d2);
static void porya_program(t_porya *x, long track, long program);
static void porya_voicegroup(t_porya *x, t_symbol *root, t_symbol *name);
static void porya_voicegroup_do(t_porya *x, t_symbol *s, short ac, t_atom *av);
static void porya_tempo(t_porya *x, double bpm);
static void porya_panic(t_porya *x);
static void porya_loadbang(t_porya *x);

/* ---- recorder forward decls ---- */
static void porya_record(t_porya *x, long onoff);
static void porya_beats(t_porya *x, double beats);
static void porya_dump(t_porya *x, t_symbol *path);
static void porya_dumpfailed(t_porya *x, t_symbol *path, const char *reason);
static void porya_clear(t_porya *x);
static void porya_anything(t_porya *x, t_symbol *s, long argc, t_atom *argv);

/* attribute getters/setters */
static t_max_err prog_get(t_porya *x, t_object *attr, long *ac, t_atom **av);
static t_max_err prog_set(t_porya *x, t_object *attr, long ac, t_atom *av);
static t_max_err songvol_set(t_porya *x, t_object *attr, long ac, t_atom *av);
static t_max_err reverb_set(t_porya *x, t_object *attr, long ac, t_atom *av);
static t_max_err analog_set(t_porya *x, t_object *attr, long ac, t_atom *av);
static t_max_err maxpcm_set(t_porya *x, t_object *attr, long ac, t_atom *av);
static long clamp_long(long v, long lo, long hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

extern "C" void ext_main(void *r)
{
    post("poryaaaa~: loaded build 2026-05-13 beats-selector-fix diagnostic");

    t_class *c = class_new("poryaaaa~",
                           (method)porya_new,
                           (method)porya_free,
                           (long)sizeof(t_porya),
                           0L, A_GIMME, 0);

    class_addmethod(c, (method)porya_dsp64,     "dsp64",     A_CANT, 0);
    class_addmethod(c, (method)porya_assist,    "assist",    A_CANT, 0);
    class_addmethod(c, (method)porya_loadbang,  "loadbang",  0);

    class_addmethod(c, (method)porya_int,       "int",       A_LONG, 0);
    class_addmethod(c, (method)porya_midievent, "midievent", A_GIMME, 0);

    class_addmethod(c, (method)porya_program,    "program",    A_LONG, A_LONG, 0);
    class_addmethod(c, (method)porya_voicegroup, "voicegroup", A_SYM, A_SYM, 0);
    class_addmethod(c, (method)porya_tempo,      "tempo",      A_FLOAT, 0);
    class_addmethod(c, (method)porya_panic,      "panic",      0);

    /* recorder */
    class_addmethod(c, (method)porya_record, "record", A_LONG,  0);
    class_addmethod(c, (method)porya_beats,  "beats",  A_FLOAT, 0);
    class_addmethod(c, (method)porya_dump,   "dump",   A_SYM,   0);
    class_addmethod(c, (method)porya_clear,  "clear",  0);
    class_addmethod(c, (method)porya_anything, "anything", A_GIMME, 0);

    /* 16 program-slot attrs: prog0..prog15 — automatable from Live */
    {
        char name[8];
        for (int i = 0; i < 16; i++) {
            snprintf(name, sizeof name, "prog%d", i);
            CLASS_ATTR_LONG(c, name, 0, t_porya, progSlot);
            CLASS_ATTR_ACCESSORS(c, name, prog_get, prog_set);
            CLASS_ATTR_FILTER_CLIP(c, name, 0, 127);
            CLASS_ATTR_SAVE(c, name, 0);
        }
    }

    CLASS_ATTR_LONG(c, "songvol", 0, t_porya, songVolume);
    CLASS_ATTR_ACCESSORS(c, "songvol", NULL, songvol_set);
    CLASS_ATTR_FILTER_CLIP(c, "songvol", 0, 127);
    CLASS_ATTR_SAVE(c, "songvol", 0);

    CLASS_ATTR_LONG(c, "reverb", 0, t_porya, reverbAmount);
    CLASS_ATTR_ACCESSORS(c, "reverb", NULL, reverb_set);
    CLASS_ATTR_FILTER_CLIP(c, "reverb", 0, 127);
    CLASS_ATTR_SAVE(c, "reverb", 0);

    CLASS_ATTR_LONG(c, "analogfilter", 0, t_porya, analogFilter);
    CLASS_ATTR_ACCESSORS(c, "analogfilter", NULL, analog_set);
    CLASS_ATTR_FILTER_CLIP(c, "analogfilter", 0, 1);
    CLASS_ATTR_SAVE(c, "analogfilter", 0);

    CLASS_ATTR_LONG(c, "maxpcm", 0, t_porya, maxPcmChannels);
    CLASS_ATTR_ACCESSORS(c, "maxpcm", NULL, maxpcm_set);
    CLASS_ATTR_FILTER_CLIP(c, "maxpcm", 1, MAX_PCM_CHANNELS);
    CLASS_ATTR_SAVE(c, "maxpcm", 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    porya_class = c;
}

/* ---- lifecycle ---- */

static void *porya_new(t_symbol *s, long argc, t_atom *argv)
{
    t_porya *x = (t_porya *)object_alloc(porya_class);
    if (!x) return NULL;

    dsp_setup((t_pxobject *)x, 0);     /* synth: zero audio inlets */
    /* Max creates outlets right-to-left: the first outlet_new call ends up
     * at the rightmost position. We want:
     *   outlet 0 = left signal
     *   outlet 1 = right signal
     *   outlet 2 = status messages
     * so create in order status, right-signal, left-signal. */
    x->statusOutlet = outlet_new(x, NULL);  /* outlet 2: any message */
    outlet_new(x, "signal");                /* outlet 1: right */
    outlet_new(x, "signal");                /* outlet 0: left  */

    /* defaults */
    x->loadedVg = NULL;
    x->scratchL = x->scratchR = NULL;
    x->scratchFrames = 0;
    x->samplerate = sys_getsr();
    if (x->samplerate <= 0.0) x->samplerate = 48000.0;

    x->vgRoot = gensym("");
    x->vgName = gensym("");
    for (int i = 0; i < 16; i++) x->progSlot[i] = 0;
    x->songVolume = MAX_SONG_VOLUME;
    x->reverbAmount = 0;
    x->analogFilter = 0;
    x->maxPcmChannels = 5;
    x->midiStatus = 0;
    x->midiData1 = 0;
    x->midiBytesNeeded = 0;
    x->midiBytesGot = 0;
    x->midiInSysex = 0;

    /* recorder */
    x->capture = new ExportCapture();

    m4a_engine_init(&x->engine, (float)x->samplerate);
#if defined(M4A_DRIVER_V2)
    x->m4a_v2 = m4a_driver_create((float)x->samplerate);
    m4a_set_song_volume(x->m4a_v2, (uint8_t)x->songVolume);
    m4a_set_reverb_amount(x->m4a_v2, (uint8_t)x->reverbAmount);
    m4a_set_analog_filter(x->m4a_v2, x->analogFilter ? true : false);
    m4a_set_max_pcm_channels(x->m4a_v2, (uint8_t)x->maxPcmChannels);
#endif
#if defined(HW_AUDIO_V2)
    x->hw_v2 = hw_audio_create((float)x->samplerate);
#endif

    /* attributes (including ATTR_SAVE restored values) are set by attr_args_process */
    attr_args_process(x, (short)argc, argv);

    return x;
}

static void porya_free(t_porya *x)
{
    dsp_free((t_pxobject *)x);
    m4a_engine_destroy(&x->engine);
#if defined(M4A_DRIVER_V2)
    if (x->m4a_v2) {
        m4a_driver_destroy(x->m4a_v2);
        x->m4a_v2 = NULL;
    }
#endif
#if defined(HW_AUDIO_V2)
    if (x->hw_v2) {
        hw_audio_destroy(x->hw_v2);
        x->hw_v2 = NULL;
    }
#endif
    if (x->loadedVg) {
        voicegroup_free(x->loadedVg);
        x->loadedVg = NULL;
    }
    if (x->scratchL) sysmem_freeptr(x->scratchL);
    if (x->scratchR) sysmem_freeptr(x->scratchR);
    delete x->capture;
    x->capture = nullptr;
}

static void porya_assist(t_porya *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        snprintf(s, 256, "raw MIDI bytes (int from [midiin]); messages: midievent program voicegroup tempo panic record beats dump");
    } else {
        switch (a) {
            case 0:  snprintf(s, 256, "(signal) audio out L"); break;
            case 1:  snprintf(s, 256, "(signal) audio out R"); break;
            default: snprintf(s, 256, "recorder replies: dumped <path> <count>, dumpfailed <path> <reason>"); break;
        }
    }
}

static void porya_loadbang(t_porya *x)
{
    /* Voicegroup restore is owned by the Node server. Do not auto-load here. */
}

/* ---- DSP ---- */

static void porya_dsp64(t_porya *x, t_object *dsp64, short *count,
                        double samplerate, long maxvectorsize, long flags)
{
    if (samplerate != x->samplerate) {
        m4a_engine_destroy(&x->engine);
        m4a_engine_init(&x->engine, (float)samplerate);
#if defined(M4A_DRIVER_V2)
        if (x->m4a_v2) m4a_driver_destroy(x->m4a_v2);
        x->m4a_v2 = m4a_driver_create((float)samplerate);
#endif
#if defined(HW_AUDIO_V2)
        if (x->hw_v2) hw_audio_destroy(x->hw_v2);
        x->hw_v2 = hw_audio_create((float)samplerate);
#endif
        x->samplerate = samplerate;
        /* re-apply persisted state — engine got memset */
        m4a_engine_set_song_volume(&x->engine, (uint8_t)x->songVolume);
        m4a_engine_set_reverb_amount(&x->engine, (uint8_t)x->reverbAmount);
        m4a_engine_set_analog_filter(&x->engine, x->analogFilter ? true : false);
        m4a_engine_set_max_pcm_channels(&x->engine, (uint8_t)x->maxPcmChannels);
#if defined(M4A_DRIVER_V2)
        m4a_set_song_volume(x->m4a_v2, (uint8_t)x->songVolume);
        m4a_set_reverb_amount(x->m4a_v2, (uint8_t)x->reverbAmount);
        m4a_set_analog_filter(x->m4a_v2, x->analogFilter ? true : false);
        m4a_set_max_pcm_channels(x->m4a_v2, (uint8_t)x->maxPcmChannels);
#endif
        for (int i = 0; i < 16; i++) {
            m4a_engine_program_change(&x->engine, i, (uint8_t)x->progSlot[i]);
#if defined(M4A_DRIVER_V2)
            m4a_program_change(x->m4a_v2, i, (uint8_t)x->progSlot[i]);
#endif
        }
        if (x->loadedVg) {
            m4a_engine_set_voicegroup(&x->engine, x->loadedVg->voices);
#if defined(M4A_DRIVER_V2)
            m4a_driver_set_voicegroup(x->m4a_v2, x->loadedVg->voices);
#endif
        }
    }

    if (maxvectorsize > x->scratchFrames) {
        if (x->scratchL) sysmem_freeptr(x->scratchL);
        if (x->scratchR) sysmem_freeptr(x->scratchR);
        x->scratchL = (float *)sysmem_newptr(sizeof(float) * (size_t)maxvectorsize);
        x->scratchR = (float *)sysmem_newptr(sizeof(float) * (size_t)maxvectorsize);
        x->scratchFrames = maxvectorsize;
    }

    object_method(dsp64, gensym("dsp_add64"), x, porya_perform64, 0, NULL);
}

static void porya_perform64(t_porya *x, t_object *dsp64,
                            double **ins, long numins,
                            double **outs, long numouts,
                            long sampleframes, long flags, void *userparam)
{
    float *sL = x->scratchL;
    float *sR = x->scratchR;
    if (!sL || !sR || sampleframes > x->scratchFrames) {
        /* defensive: zero outputs if scratch isn't ready */
        if (numouts > 0) memset(outs[0], 0, sizeof(double) * (size_t)sampleframes);
        if (numouts > 1) memset(outs[1], 0, sizeof(double) * (size_t)sampleframes);
        return;
    }

#if defined(M4A_DRIVER_V2) && defined(HW_AUDIO_V2)
    uint32_t toGo = (uint32_t)sampleframes;
    uint32_t off = 0;
    while (toGo > 0) {
        uint32_t chunk = toGo;
        if (chunk > (uint32_t)M4A_RECOMMENDED_MAX_ADVANCE_FRAMES)
            chunk = (uint32_t)M4A_RECOMMENDED_MAX_ADVANCE_FRAMES;
        m4a_advance(x->m4a_v2, (int)chunk);
        hw_audio_render_events(x->hw_v2,
                               m4a_get_pending_writes(x->m4a_v2),
                               m4a_get_pcm_ring(x->m4a_v2),
                               sL + off, sR + off, (int)chunk);
        m4a_consume_writes(x->m4a_v2);
        off += chunk;
        toGo -= chunk;
    }
#else
    m4a_engine_process(&x->engine, sL, sR, (int)sampleframes);
#endif

    double *outL = outs[0];
    double *outR = numouts > 1 ? outs[1] : outs[0];
    for (long i = 0; i < sampleframes; i++) {
        outL[i] = (double)sL[i];
        outR[i] = (double)sR[i];
    }
}

/* ---- MIDI input ----
 * Raw byte stream from [midiin] enters via porya_int — a small running-status
 * parser assembles channel-voice messages and forwards them to the engine.
 * porya_midievent stays around for sending pre-assembled 3-byte events from
 * a message box (testing/automation), and shares the same dispatcher.
 */

static void porya_dispatch_event(t_porya *x, uint8_t status, uint8_t d1, uint8_t d2)
{
    int type = (status >> 4) & 0x0F;
    int ch = status & 0x0F;
    if (ch >= MAX_TRACKS) return;
    d1 &= 0x7F;
    d2 &= 0x7F;

    switch (type) {
        case 0x8:
            m4a_engine_note_off(&x->engine, ch, d1);
#if defined(M4A_DRIVER_V2)
            m4a_note_off(x->m4a_v2, ch, d1);
#endif
            break;
        case 0x9:
            if (d2 == 0) {
                m4a_engine_note_off(&x->engine, ch, d1);
#if defined(M4A_DRIVER_V2)
                m4a_note_off(x->m4a_v2, ch, d1);
#endif
            } else {
                m4a_engine_note_on(&x->engine, ch, d1, d2);
#if defined(M4A_DRIVER_V2)
                m4a_note_on(x->m4a_v2, ch, d1, d2);
#endif
            }
            break;
        case 0xB:
            /* CCs incl. XCMD on 29/30/31 — engine routes them */
            m4a_engine_cc(&x->engine, ch, d1, d2);
#if defined(M4A_DRIVER_V2)
            m4a_cc(x->m4a_v2, ch, d1, d2);
#endif
            break;
        case 0xC:
            m4a_engine_program_change(&x->engine, ch, d1);
#if defined(M4A_DRIVER_V2)
            m4a_program_change(x->m4a_v2, ch, d1);
#endif
            x->progSlot[ch] = d1;
            break;
        case 0xE: {
            int16_t signed14 = (int16_t)(((d2 << 7) | d1) - 8192);
            m4a_engine_pitch_bend(&x->engine, ch, signed14);
#if defined(M4A_DRIVER_V2)
            m4a_pitch_bend(x->m4a_v2, ch, signed14);
#endif
            break;
        }
        default:
            /* poly AT (0xA), channel AT (0xD) — engine has no handler */
            break;
    }

    /* Recorder capture: stamp the parsed event with the latest beat-float we
     * received from plugsync~. JS converts to SMF ticks at Save time. */
    x->capture->capture_event(status, d1, d2);
}

static void porya_int(t_porya *x, long n)
{
    uint8_t b = (uint8_t)(n & 0xFF);

    /* realtime (clock/start/stop/sense) — no GBA m4a meaning, swallow */
    if (b >= 0xF8) return;

    if (x->midiInSysex) {
        if (b == 0xF7) { x->midiInSysex = 0; return; }
        if (b < 0x80)  return;
        /* a non-realtime status byte aborts sysex per spec; fall through */
        x->midiInSysex = 0;
    }

    if (b & 0x80) {
        if (b == 0xF0) {
            x->midiInSysex = 1;
            x->midiStatus = 0;
            x->midiBytesGot = 0;
            return;
        }
        if (b >= 0xF1) {
            /* MTC quarter-frame, song select, song pos — ignore, drop running status */
            x->midiStatus = 0;
            x->midiBytesGot = 0;
            return;
        }
        x->midiStatus = b;
        x->midiBytesNeeded = (((b & 0xF0) == 0xC0) || ((b & 0xF0) == 0xD0)) ? 1 : 2;
        x->midiBytesGot = 0;
        return;
    }

    /* data byte */
    if (!x->midiStatus) return;
    if (x->midiBytesGot == 0) {
        x->midiData1 = b;
        if (x->midiBytesNeeded == 1) {
            porya_dispatch_event(x, x->midiStatus, b, 0);
            /* running status: keep midiStatus, leave midiBytesGot at 0 */
        } else {
            x->midiBytesGot = 1;
        }
    } else {
        porya_dispatch_event(x, x->midiStatus, x->midiData1, b);
        x->midiBytesGot = 0;
    }
}

static void porya_midievent(t_porya *x, t_symbol *s, long argc, t_atom *argv)
{
    if (argc < 1) return;
    uint8_t status = (uint8_t)(atom_getlong(argv) & 0xFF);
    uint8_t d1 = argc > 1 ? (uint8_t)(atom_getlong(argv + 1) & 0xFF) : 0;
    uint8_t d2 = argc > 2 ? (uint8_t)(atom_getlong(argv + 2) & 0xFF) : 0;
    porya_dispatch_event(x, status, d1, d2);
}

static void porya_program(t_porya *x, long track, long program)
{
    int t = (int)clamp_long(track, 0, MAX_TRACKS - 1);
    int p = (int)clamp_long(program, 0, 127);
    m4a_engine_program_change(&x->engine, t, (uint8_t)p);
#if defined(M4A_DRIVER_V2)
    m4a_program_change(x->m4a_v2, t, (uint8_t)p);
#endif
    x->progSlot[t] = p;
}

static void porya_tempo(t_porya *x, double bpm)
{
    if (bpm < 1.0) bpm = 1.0;
    x->capture->set_tempo(bpm);
    m4a_engine_set_tempo_bpm(&x->engine, bpm);
#if defined(M4A_DRIVER_V2)
    m4a_set_tempo_bpm(x->m4a_v2, bpm);
#endif
}

static void porya_panic(t_porya *x)
{
    m4a_engine_all_sound_off(&x->engine);
#if defined(M4A_DRIVER_V2)
    m4a_all_sound_off(x->m4a_v2);
#endif
}

/* ---- recorder messages -------------------------------------------------- */

/* `record 1` starts export-detection capture. Normal playback only fills the
 * rolling prebuffer; the final buffer starts after offline-export timing is
 * detected from plugsync~ beat speed. `record 0` is a user reset: it clears
 * pending/exported MIDI so the next arm starts from a clean state. */
static void porya_record(t_porya *x, long armed)
{
    if (armed != 0) {
        x->capture->record_on();
        return;
    }

    x->capture->record_off();
}

/* Latched beat-float from [plugsync~] outlet 6 ("Ticks (1 PPQ)"). Arrives on
 * the scheduler thread, same thread as porya_int — but a dispatch race is
 * still possible if plugsync~ and [midiin] poll at different times, so the
 * capture helper keeps the last received value. */
static void porya_beats(t_porya *x, double beats)
{
    if (x->capture->beats(beats)) {
        object_post((t_object *)x,
                    "poryaaaa~: export capture detected; prebuffer=%zu events",
                    x->capture->size());
    }
}

/* Dump the buffer to `path` as a fixed-layout binary file, then emit
 * `dumped <path> <count>` on the status outlet so the JS save flow can
 * proceed. Path comes from JS (which generated it under /tmp), so we
 * don't expand ~ or mkdir anything here. */
static void porya_dump(t_porya *x, t_symbol *path)
{
    if (x->capture->state() == CaptureState::PendingExport) {
        object_error((t_object *)x,
                     "poryaaaa~: dump requested before export detected");
        porya_dumpfailed(x, path, "export_not_detected");
        return;
    }
    if (x->capture->state() == CaptureState::Exporting) {
        x->capture->finish_export();
    }
    if (!path || !path->s_name || !path->s_name[0]) {
        object_error((t_object *)x, "poryaaaa~: dump requires a path");
        porya_dumpfailed(x, path, "bad_path");
        return;
    }
    std::size_t count = x->capture->size();
    if (count == 0) {
        object_error((t_object *)x, "poryaaaa~: dump requested with empty buffer");
        porya_dumpfailed(x, path, "nothing_recorded");
        return;
    }
    bool ok = x->capture->dump_to_file(path->s_name);
    if (!ok) {
        object_error((t_object *)x, "poryaaaa~: dump failed to write %s",
                     path->s_name);
        porya_dumpfailed(x, path, "write_failed");
        return;
    }
    t_atom av[2];
    atom_setsym(&av[0], path);
    atom_setlong(&av[1], (long)count);
    outlet_anything(x->statusOutlet, gensym("dumped"), 2, av);
}

static void porya_dumpfailed(t_porya *x, t_symbol *path, const char *reason)
{
    t_atom av[2];
    atom_setsym(&av[0], path ? path : gensym(""));
    atom_setsym(&av[1], gensym(reason ? reason : "dump_failed"));
    outlet_anything(x->statusOutlet, gensym("dumpfailed"), 2, av);
}

/* Wipe the buffer. Called by JS after a successful Save so the next take
 * starts fresh without requiring the user to cycle the Rec toggle. */
static void porya_clear(t_porya *x)
{
    std::size_t before = x->capture->size();
    x->capture->clear();
    object_post((t_object *)x,
                "poryaaaa~: clear — buffer was %zu events, now %zu",
                before, x->capture->size());
}

static void porya_anything(t_porya *x, t_symbol *s, long argc, t_atom *argv)
{
    if (!s) return;

    object_post((t_object *)x,
                "poryaaaa~: anything fallback selector=%s argc=%ld",
                s->s_name, argc);

    if (s == gensym("beats")) {
        if (argc >= 1) porya_beats(x, atom_getfloat(argv));
        return;
    }
    if (s == gensym("record")) {
        if (argc >= 1) porya_record(x, atom_getlong(argv));
        return;
    }
    if (s == gensym("dump")) {
        if (argc >= 1) porya_dump(x, atom_getsym(argv));
        return;
    }
    if (s == gensym("clear")) {
        porya_clear(x);
        return;
    }
    if (s == gensym("tempo")) {
        if (argc >= 1) porya_tempo(x, atom_getfloat(argv));
        return;
    }
    if (s == gensym("program")) {
        if (argc >= 2) porya_program(x, atom_getlong(argv), atom_getlong(argv + 1));
        return;
    }
    if (s == gensym("voicegroup")) {
        if (argc >= 2) porya_voicegroup(x, atom_getsym(argv), atom_getsym(argv + 1));
        return;
    }

    object_error((t_object *)x, "poryaaaa~: unknown selector %s", s->s_name);
}

/* ---- voicegroup load (deferred to low-priority thread) ---- */

static void porya_voicegroup(t_porya *x, t_symbol *root, t_symbol *name)
{
    t_atom av[2];
    atom_setsym(&av[0], root);
    atom_setsym(&av[1], name);
    defer_low((t_object *)x, (method)porya_voicegroup_do, NULL, 2, av);
}

static void porya_voicegroup_do(t_porya *x, t_symbol *s, short ac, t_atom *av)
{
    if (ac < 2) return;
    t_symbol *root = atom_getsym(av + 0);
    t_symbol *name = atom_getsym(av + 1);
    if (!root || !name || !root->s_name[0] || !name->s_name[0]) return;

    LoadedVoiceGroup *vg = voicegroup_load(root->s_name, name->s_name, NULL);
    if (!vg) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "voicegroup load failed: root=%s name=%s",
                 root->s_name, name->s_name);
        object_error((t_object *)x, "%s", msg);
        return;
    }

    LoadedVoiceGroup *old = x->loadedVg;
    m4a_engine_all_sound_off(&x->engine);
#if defined(M4A_DRIVER_V2)
    m4a_all_sound_off(x->m4a_v2);
#endif
    m4a_engine_set_voicegroup(&x->engine, vg->voices);
#if defined(M4A_DRIVER_V2)
    m4a_driver_set_voicegroup(x->m4a_v2, vg->voices);
#endif
    x->loadedVg = vg;
    if (old) voicegroup_free(old);

    x->vgRoot = root;
    x->vgName = name;

    /* re-apply persisted program slots so each track has the right instrument
     * for the new voicegroup */
    for (int i = 0; i < 16; i++) {
        m4a_engine_program_change(&x->engine, i, (uint8_t)x->progSlot[i]);
#if defined(M4A_DRIVER_V2)
        m4a_program_change(x->m4a_v2, i, (uint8_t)x->progSlot[i]);
#endif
    }

    object_post((t_object *)x, "voicegroup loaded: root=%s name=%s",
                root->s_name, name->s_name);
}

/* ---- attribute getters/setters ---- */

static int prog_index_from_attr(t_object *attr)
{
    t_symbol *name = (t_symbol *)object_method(attr, gensym("getname"));
    if (!name || strncmp(name->s_name, "prog", 4) != 0) return -1;
    int idx = atoi(name->s_name + 4);
    if (idx < 0 || idx >= 16) return -1;
    return idx;
}

static t_max_err prog_get(t_porya *x, t_object *attr, long *ac, t_atom **av)
{
    char alloc;
    if (atom_alloc(ac, av, &alloc) != MAX_ERR_NONE) return MAX_ERR_GENERIC;
    int idx = prog_index_from_attr(attr);
    long v = (idx >= 0) ? x->progSlot[idx] : 0;
    atom_setlong(*av, v);
    return MAX_ERR_NONE;
}

static t_max_err prog_set(t_porya *x, t_object *attr, long ac, t_atom *av)
{
    if (ac < 1 || !av) return MAX_ERR_NONE;
    int idx = prog_index_from_attr(attr);
    if (idx < 0) return MAX_ERR_NONE;
    long v = clamp_long(atom_getlong(av), 0, 127);
    x->progSlot[idx] = v;
    m4a_engine_program_change(&x->engine, idx, (uint8_t)v);
#if defined(M4A_DRIVER_V2)
    m4a_program_change(x->m4a_v2, idx, (uint8_t)v);
#endif
    return MAX_ERR_NONE;
}

static t_max_err songvol_set(t_porya *x, t_object *attr, long ac, t_atom *av)
{
    if (ac < 1 || !av) return MAX_ERR_NONE;
    long v = clamp_long(atom_getlong(av), 0, 127);
    x->songVolume = v;
    m4a_engine_set_song_volume(&x->engine, (uint8_t)v);
#if defined(M4A_DRIVER_V2)
    m4a_set_song_volume(x->m4a_v2, (uint8_t)v);
#endif
    return MAX_ERR_NONE;
}

static t_max_err reverb_set(t_porya *x, t_object *attr, long ac, t_atom *av)
{
    if (ac < 1 || !av) return MAX_ERR_NONE;
    long v = clamp_long(atom_getlong(av), 0, 127);
    x->reverbAmount = v;
    m4a_engine_set_reverb_amount(&x->engine, (uint8_t)v);
#if defined(M4A_DRIVER_V2)
    m4a_set_reverb_amount(x->m4a_v2, (uint8_t)v);
#endif
    return MAX_ERR_NONE;
}

static t_max_err analog_set(t_porya *x, t_object *attr, long ac, t_atom *av)
{
    if (ac < 1 || !av) return MAX_ERR_NONE;
    long v = clamp_long(atom_getlong(av), 0, 1);
    x->analogFilter = v;
    m4a_engine_set_analog_filter(&x->engine, v ? true : false);
#if defined(M4A_DRIVER_V2)
    m4a_set_analog_filter(x->m4a_v2, v ? true : false);
#endif
    return MAX_ERR_NONE;
}

static t_max_err maxpcm_set(t_porya *x, t_object *attr, long ac, t_atom *av)
{
    if (ac < 1 || !av) return MAX_ERR_NONE;
    long v = clamp_long(atom_getlong(av), 1, MAX_PCM_CHANNELS);
    x->maxPcmChannels = v;
    m4a_engine_set_max_pcm_channels(&x->engine, (uint8_t)v);
#if defined(M4A_DRIVER_V2)
    m4a_set_max_pcm_channels(x->m4a_v2, (uint8_t)v);
#endif
    return MAX_ERR_NONE;
}
