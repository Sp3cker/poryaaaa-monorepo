/*
 * porya.reverb~ - MSP effect external carrying the Sappy/GBA-style
 * SoundMainRAM_Reverb algorithm as a standalone stereo processor.
 */

extern "C" {
#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
}

#include "gba_reverb.hpp"

#include <new>

using porya::GbaReverb;
using porya::DelayDepth;
using porya::ReverbRateMode;

typedef struct _porya_reverb {
    t_pxobject ob;
    GbaReverb *reverb;
    long amount;
    long rate;
    long depth;
    double samplerate;
    bool leftConnected;
    bool rightConnected;
} t_porya_reverb;

static t_class *porya_reverb_class = nullptr;

static void *porya_reverb_new(t_symbol *s, long argc, t_atom *argv);
static void porya_reverb_free(t_porya_reverb *x);
static void porya_reverb_assist(t_porya_reverb *x, void *b, long m, long a, char *s);
static void porya_reverb_dsp64(t_porya_reverb *x, t_object *dsp64, short *count,
                               double samplerate, long maxvectorsize, long flags);
static void porya_reverb_perform64(t_porya_reverb *x, t_object *dsp64,
                                   double **ins, long numins,
                                   double **outs, long numouts,
                                   long sampleframes, long flags, void *userparam);
static void porya_reverb_reset(t_porya_reverb *x);
static void porya_reverb_depth(t_porya_reverb *x, long depth);
static void porya_reverb_anything(t_porya_reverb *x, t_symbol *s, long argc, t_atom *argv);
static t_max_err porya_reverb_amount_set(t_porya_reverb *x, t_object *attr,
                                         long ac, t_atom *av);
static t_max_err porya_reverb_rate_set(t_porya_reverb *x, t_object *attr,
                                       long ac, t_atom *av);
static t_max_err porya_reverb_depth_set(t_porya_reverb *x, t_object *attr,
                                        long ac, t_atom *av);

extern "C" C74_EXPORT void ext_main(void *r)
{
    t_class *c = class_new("porya.reverb~",
                           reinterpret_cast<method>(porya_reverb_new),
                           reinterpret_cast<method>(porya_reverb_free),
                           static_cast<long>(sizeof(t_porya_reverb)),
                           nullptr, A_GIMME, 0);

    class_addmethod(c, reinterpret_cast<method>(porya_reverb_dsp64),
                    "dsp64", A_CANT, 0);
    class_addmethod(c, reinterpret_cast<method>(porya_reverb_assist),
                    "assist", A_CANT, 0);
    class_addmethod(c, reinterpret_cast<method>(porya_reverb_reset),
                    "reset", 0);
    class_addmethod(c, reinterpret_cast<method>(porya_reverb_depth),
                    "depth", A_LONG, 0);
    class_addmethod(c, reinterpret_cast<method>(porya_reverb_anything),
                    "anything", A_GIMME, 0);

    CLASS_ATTR_LONG(c, "amount", 0, t_porya_reverb, amount);
    CLASS_ATTR_ACCESSORS(c, "amount", nullptr, porya_reverb_amount_set);
    CLASS_ATTR_FILTER_CLIP(c, "amount", 0, 127);
    CLASS_ATTR_SAVE(c, "amount", 0);

    CLASS_ATTR_LONG(c, "rate", 0, t_porya_reverb, rate);
    CLASS_ATTR_ACCESSORS(c, "rate", nullptr, porya_reverb_rate_set);
    CLASS_ATTR_ENUMINDEX2(c, "rate", 0, "Original", "Host");
    CLASS_ATTR_FILTER_CLIP(c, "rate", 0, 1);
    CLASS_ATTR_SAVE(c, "rate", 0);

    CLASS_ATTR_LONG(c, "depth", 0, t_porya_reverb, depth);
    CLASS_ATTR_ACCESSORS(c, "depth", nullptr, porya_reverb_depth_set);
    CLASS_ATTR_ENUMINDEX3(c, "depth", 0, "Int8", "Int16", "Int32");
    CLASS_ATTR_FILTER_CLIP(c, "depth", 0, 2);
    CLASS_ATTR_SAVE(c, "depth", 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    porya_reverb_class = c;
}

static void *porya_reverb_new(t_symbol *s, long argc, t_atom *argv)
{
    t_porya_reverb *x = static_cast<t_porya_reverb *>(object_alloc(porya_reverb_class));
    if (!x) {
        return nullptr;
    }

    x->reverb = nullptr;
    dsp_setup(reinterpret_cast<t_pxobject *>(x), 2);
    outlet_new(x, "signal"); /* outlet 1: right */
    outlet_new(x, "signal"); /* outlet 0: left */

    x->reverb = new (std::nothrow) GbaReverb();
    if (!x->reverb) {
        freeobject(reinterpret_cast<t_object *>(x));
        return nullptr;
    }

    x->amount = 0;
    x->rate = static_cast<long>(ReverbRateMode::Original);
    x->depth = static_cast<long>(DelayDepth::Int8);
    x->samplerate = sys_getsr();
    if (x->samplerate <= 0.0) {
        x->samplerate = GbaReverb::kHostRateHz;
    }
    x->leftConnected = false;
    x->rightConnected = false;

    x->reverb->setHostSampleRate(x->samplerate);
    x->reverb->setAmount(x->amount);
    x->reverb->setRateMode(ReverbRateMode::Original);
    x->reverb->setDelayDepth(DelayDepth::Int8);

    attr_args_process(x, static_cast<short>(argc), argv);
    return x;
}

static void porya_reverb_free(t_porya_reverb *x)
{
    dsp_free(reinterpret_cast<t_pxobject *>(x));
    delete x->reverb;
    x->reverb = nullptr;
}

static void porya_reverb_assist(t_porya_reverb *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0:
                snprintf(s, 256, "(signal) left input; messages: reset, attributes amount/rate/depth");
                break;
            case 1:
                snprintf(s, 256, "(signal) right input");
                break;
            default:
                snprintf(s, 256, "input");
                break;
        }
    } else {
        snprintf(s, 256, a == 0 ? "(signal) left output" : "(signal) right output");
    }
}

static void porya_reverb_dsp64(t_porya_reverb *x, t_object *dsp64, short *count,
                               double samplerate, long maxvectorsize, long flags)
{
    if (x->reverb) {
        x->reverb->setHostSampleRate(samplerate);
    }
    x->samplerate = samplerate;
    x->leftConnected = count && count[0] != 0;
    x->rightConnected = count && count[1] != 0;

    object_method(dsp64, gensym("dsp_add64"), x, porya_reverb_perform64, 0, nullptr);
}

static void porya_reverb_perform64(t_porya_reverb *x, t_object *dsp64,
                                   double **ins, long numins,
                                   double **outs, long numouts,
                                   long sampleframes, long flags, void *userparam)
{
    if (!x->reverb || numins < 1 || numouts < 1) {
        return;
    }

    const double *fallback = ins[0];
    const double *inL = x->leftConnected ? ins[0] : (x->rightConnected && numins > 1 ? ins[1] : fallback);
    const double *inR = x->rightConnected && numins > 1 ? ins[1] : inL;
    double *outL = outs[0];
    double *outR = numouts > 1 ? outs[1] : outs[0];

    x->reverb->process(inL, inR, outL, outR, sampleframes);
}

static void porya_reverb_reset(t_porya_reverb *x)
{
    if (x->reverb) {
        x->reverb->reset();
    }
}

static void porya_reverb_depth(t_porya_reverb *x, long depth)
{
    t_atom av;
    atom_setlong(&av, depth);
    porya_reverb_depth_set(x, nullptr, 1, &av);
}

static void porya_reverb_anything(t_porya_reverb *x, t_symbol *s, long argc, t_atom *argv)
{
    if ((s == gensym("depth") || s == gensym("@depth")) && argc > 0) {
        porya_reverb_depth_set(x, nullptr, argc, argv);
    }
}

static t_max_err porya_reverb_amount_set(t_porya_reverb *x, t_object *attr,
                                         long ac, t_atom *av)
{
    if (ac <= 0 || !av) {
        return MAX_ERR_NONE;
    }

    long amount = atom_getlong(av);
    if (amount < 0) {
        amount = 0;
    } else if (amount > 127) {
        amount = 127;
    }
    x->amount = amount;
    if (x->reverb) {
        x->reverb->setAmount(amount);
    }
    return MAX_ERR_NONE;
}

static t_max_err porya_reverb_rate_set(t_porya_reverb *x, t_object *attr,
                                       long ac, t_atom *av)
{
    if (ac <= 0 || !av) {
        return MAX_ERR_NONE;
    }

    long rate = atom_getlong(av) == 1 ? 1 : 0;
    x->rate = rate;
    if (x->reverb) {
        x->reverb->setRateMode(rate == 1 ? ReverbRateMode::Host : ReverbRateMode::Original);
    }
    return MAX_ERR_NONE;
}

static t_max_err porya_reverb_depth_set(t_porya_reverb *x, t_object *attr,
                                        long ac, t_atom *av)
{
    if (ac <= 0 || !av) {
        return MAX_ERR_NONE;
    }

    long depth = atom_getlong(av);
    if (depth < 0 || depth > 2) {
        depth = 0;
    }
    x->depth = depth;
    if (x->reverb) {
        switch (depth) {
            case 1:
                x->reverb->setDelayDepth(DelayDepth::Int16);
                break;
            case 2:
                x->reverb->setDelayDepth(DelayDepth::Int32);
                break;
            case 0:
            default:
                x->reverb->setDelayDepth(DelayDepth::Int8);
                break;
        }
    }
    return MAX_ERR_NONE;
}
