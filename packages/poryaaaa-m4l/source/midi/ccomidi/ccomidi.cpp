/*
 * ccomidi - Max for Live MIDI effect.
 *
 * Primary wiring inside the .amxd:
 *   [midiin] -> [ccomidi] -> [midiout]
 *
 * Raw MIDI bytes pass through unchanged after running-status parsing. The
 * named control API emits fixed CC/XCMD state for poryaaaa~. Legacy dynamic
 * row names (rN_vN/rN_type/rN_en) are no longer part of the public Max API.
 */

extern "C" {
#include "ext.h"
#include "ext_obex.h"
}

#include "ccomidi_parser.h"

#include <array>
#include <cstddef>
#include <cstdint>

using namespace ccomidi;

namespace {

constexpr std::uint8_t kStatusCc = 0xB0;
constexpr std::uint8_t kStatusProgram = 0xC0;

constexpr std::uint8_t kCcMod = 0x01;
constexpr std::uint8_t kCcVolume = 0x07;
constexpr std::uint8_t kCcPan = 0x0A;
constexpr std::uint8_t kCcBendRange = 0x14;
constexpr std::uint8_t kCcLfoSpeed = 0x15;
constexpr std::uint8_t kCcModType = 0x16;
constexpr std::uint8_t kCcTune = 0x18;
constexpr std::uint8_t kCcLfoDelay = 0x1A;
constexpr std::uint8_t kCcXcmdValue = 0x1D;
constexpr std::uint8_t kCcXcmdSelect = 0x1E;
constexpr std::uint8_t kCcPriority21 = 0x21;
constexpr std::uint8_t kCcPriority27 = 0x27;

constexpr std::uint8_t kXcmdIecv = 0x08;
constexpr std::uint8_t kXcmdIecl = 0x09;

enum OptionalControl : std::size_t {
    OptMod = 0,
    OptLfoSpeed,
    OptLfoDelay,
    OptBendRange,
    OptModType,
    OptTune,
    OptIecv,
    OptIecl,
    OptPriority,
    OptCount,
};

} // namespace

typedef struct _cco {
    t_object ob;
    void *out_midi;

    long programVal;
    long volVal;
    long panVal;
    long modVal;
    long lfoSpdVal;
    long lfoDlyVal;
    long bendRangeVal;
    long modTypeVal;
    long tuneVal;      /* raw MIDI value: 0..127, neutral 64 */
    long iecvVal;
    long ieclVal;
    long priorityVal;  /* 0, 21, or 27 */

    long restoreMode;
    long transportPlaying;
    std::array<long, OptCount> pendingClear;

    ParserState parserState;
} t_cco;

static t_class *cco_class = nullptr;

/* ---------- helpers ---------- */

static long clamp_long(long v, long lo, long hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int midi_data_byte_count(std::uint8_t status)
{
    std::uint8_t high = status & 0xF0;
    return (high == 0xC0 || high == 0xD0) ? 1 : 2;
}

static std::uint8_t max_channel_to_nibble(long channel)
{
    if (channel >= 1 && channel <= 16) return (std::uint8_t)((channel - 1) & 0x0F);
    if (channel >= 0 && channel <= 15) return (std::uint8_t)(channel & 0x0F);
    return 0;
}

static void emit_status_data(t_cco *x, std::uint8_t status, std::uint8_t d1, std::uint8_t d2)
{
    int n = midi_data_byte_count(status);
    outlet_int(x->out_midi, status);
    outlet_int(x->out_midi, d1);
    if (n == 2) outlet_int(x->out_midi, d2);
}

static void emit_channel_event(t_cco *x, std::uint8_t high_nibble,
                               std::uint8_t d1, std::uint8_t d2,
                               long channel = 0)
{
    std::uint8_t status = (high_nibble & 0xF0) | max_channel_to_nibble(channel);
    emit_status_data(x, status, d1, d2);
}

static void emit_cc(t_cco *x, std::uint8_t cc, long value)
{
    emit_channel_event(x, kStatusCc, cc, (std::uint8_t)clamp_long(value, 0, 127));
}

static void emit_xcmd(t_cco *x, std::uint8_t selector, long value)
{
    emit_cc(x, kCcXcmdSelect, selector);
    emit_cc(x, kCcXcmdValue, value);
}

static void emit_program_value(t_cco *x)
{
    emit_channel_event(x, kStatusProgram, (std::uint8_t)clamp_long(x->programVal, 0, 127), 0);
}

static long optional_value(t_cco *x, OptionalControl opt)
{
    switch (opt) {
        case OptMod:       return x->modVal;
        case OptLfoSpeed:  return x->lfoSpdVal;
        case OptLfoDelay:  return x->lfoDlyVal;
        case OptBendRange: return x->bendRangeVal;
        case OptModType:   return x->modTypeVal;
        case OptTune:      return x->tuneVal;
        case OptIecv:      return x->iecvVal;
        case OptIecl:      return x->ieclVal;
        case OptPriority:  return x->priorityVal;
        default:           return 0;
    }
}

static long optional_neutral(OptionalControl opt)
{
    return opt == OptTune ? 64 : 0;
}

static bool optional_active(t_cco *x, OptionalControl opt)
{
    return optional_value(x, opt) != optional_neutral(opt);
}

static void emit_priority(t_cco *x, bool include_neutral)
{
    if (x->priorityVal == 21) {
        emit_cc(x, kCcPriority21, 21);
    } else if (x->priorityVal == 27) {
        emit_cc(x, kCcPriority27, 27);
    } else if (include_neutral) {
        emit_cc(x, kCcPriority21, 0);
        emit_cc(x, kCcPriority27, 0);
    }
}

static void emit_optional(t_cco *x, OptionalControl opt, bool include_neutral)
{
    if (!include_neutral && !optional_active(x, opt)) return;

    switch (opt) {
        case OptMod:       emit_cc(x, kCcMod, x->modVal); break;
        case OptLfoSpeed:  emit_cc(x, kCcLfoSpeed, x->lfoSpdVal); break;
        case OptLfoDelay:  emit_cc(x, kCcLfoDelay, x->lfoDlyVal); break;
        case OptBendRange: emit_cc(x, kCcBendRange, x->bendRangeVal); break;
        case OptModType:   emit_cc(x, kCcModType, x->modTypeVal); break;
        case OptTune:      emit_cc(x, kCcTune, x->tuneVal); break;
        case OptIecv:      emit_xcmd(x, kXcmdIecv, x->iecvVal); break;
        case OptIecl:      emit_xcmd(x, kXcmdIecl, x->ieclVal); break;
        case OptPriority:  emit_priority(x, include_neutral); break;
        default:           break;
    }
}

static void emit_transport_snapshot(t_cco *x)
{
    emit_program_value(x);
    emit_cc(x, kCcVolume, x->volVal);
    emit_cc(x, kCcPan, x->panVal);

    for (std::size_t i = 0; i < OptCount; ++i) {
        const auto opt = (OptionalControl)i;
        if (optional_active(x, opt)) {
            emit_optional(x, opt, false);
        } else if (x->pendingClear[i]) {
            emit_optional(x, opt, true);
            x->pendingClear[i] = 0;
        }
    }

}

static void emit_full_snapshot(t_cco *x)
{
    emit_program_value(x);
    emit_cc(x, kCcVolume, x->volVal);
    emit_cc(x, kCcPan, x->panVal);

    for (std::size_t i = 0; i < OptCount; ++i) {
        emit_optional(x, (OptionalControl)i, true);
        x->pendingClear[i] = 0;
    }

}

static void set_optional_value(t_cco *x, OptionalControl opt, long old_value, long new_value)
{
    const bool was_active = old_value != optional_neutral(opt);
    const bool is_active = new_value != optional_neutral(opt);

    if (x->restoreMode) return;

    if (is_active) {
        emit_optional(x, opt, false);
        x->pendingClear[(std::size_t)opt] = 0;
    } else if (was_active) {
        if (x->transportPlaying) {
            emit_optional(x, opt, true);
            x->pendingClear[(std::size_t)opt] = 0;
        } else {
            x->pendingClear[(std::size_t)opt] = 1;
        }
    }
}

/* ---------- raw MIDI byte parser (running status) ---------- */

static void cco_int(t_cco *x, long b)
{
    ParserOutput out = parse_byte(x->parserState, (std::uint8_t)(b & 0xFF));
    for (std::uint8_t i = 0; i < out.length; ++i) {
        outlet_int(x->out_midi, out.bytes[i]);
    }
}

/* Parsed Program Change input is intentionally disabled for now. If an
 * explicit external PC input becomes necessary, re-register pgmchange. */
static void cco_pgm_in(t_cco *x, long program, long channel)
{
    long p = clamp_long(program, 0, 127);
    emit_channel_event(x, kStatusProgram, (std::uint8_t)p, 0, channel);
}

static void cco_bend(t_cco *x, long val14, long channel)
{
    if (x->restoreMode) return;
    long v = clamp_long(val14, 0, 16383);
    std::uint8_t lo = (std::uint8_t)(v & 0x7F);
    std::uint8_t hi = (std::uint8_t)((v >> 7) & 0x7F);
    emit_channel_event(x, 0xE0, lo, hi, channel);
}

/* ---------- control messages ---------- */

static void cco_sendall(t_cco *x) { emit_full_snapshot(x); }

static void cco_transport(t_cco *x, long playing)
{
    long was = x->transportPlaying;
    x->transportPlaying = playing ? 1 : 0;
    if (!was && x->transportPlaying) emit_transport_snapshot(x);
}

static void cco_panic(t_cco *x)
{
    for (long &pending : x->pendingClear) pending = 0;
}

static void cco_restore(t_cco *x, long restoring)
{
    x->restoreMode = restoring ? 1 : 0;
}

static void cco_program(t_cco *x, long value)
{
    x->programVal = clamp_long(value, 0, 127);
    if (!x->restoreMode) emit_program_value(x);
}

static void cco_vol(t_cco *x, long value)
{
    x->volVal = clamp_long(value, 0, 127);
    if (!x->restoreMode) emit_cc(x, kCcVolume, x->volVal);
}

static void cco_pan(t_cco *x, long value)
{
    x->panVal = clamp_long(value, 0, 127);
    if (!x->restoreMode) emit_cc(x, kCcPan, x->panVal);
}

static void cco_mod(t_cco *x, long value)
{
    long old = x->modVal;
    x->modVal = clamp_long(value, 0, 127);
    set_optional_value(x, OptMod, old, x->modVal);
}

static void cco_lfo_spd(t_cco *x, long value)
{
    long old = x->lfoSpdVal;
    x->lfoSpdVal = clamp_long(value, 0, 127);
    set_optional_value(x, OptLfoSpeed, old, x->lfoSpdVal);
}

static void cco_lfo_dly(t_cco *x, long value)
{
    long old = x->lfoDlyVal;
    x->lfoDlyVal = clamp_long(value, 0, 127);
    set_optional_value(x, OptLfoDelay, old, x->lfoDlyVal);
}

static void cco_bend_range(t_cco *x, long value)
{
    long old = x->bendRangeVal;
    x->bendRangeVal = clamp_long(value, 0, 32);
    set_optional_value(x, OptBendRange, old, x->bendRangeVal);
}

static void cco_mod_type(t_cco *x, long value)
{
    long old = x->modTypeVal;
    x->modTypeVal = clamp_long(value, 0, 2);
    set_optional_value(x, OptModType, old, x->modTypeVal);
}

static void cco_tune(t_cco *x, long value)
{
    long old = x->tuneVal;
    x->tuneVal = clamp_long(value, 0, 127);
    set_optional_value(x, OptTune, old, x->tuneVal);
}

static void cco_iecv(t_cco *x, long value)
{
    long old = x->iecvVal;
    x->iecvVal = clamp_long(value, 0, 32);
    set_optional_value(x, OptIecv, old, x->iecvVal);
}

static void cco_iecl(t_cco *x, long value)
{
    long old = x->ieclVal;
    x->ieclVal = clamp_long(value, 0, 32);
    set_optional_value(x, OptIecl, old, x->ieclVal);
}

static void cco_priority(t_cco *x, long value)
{
    long old = x->priorityVal;
    long next = (value == 21 || value == 27) ? value : 0;
    if (x->restoreMode && next == 0) return;
    x->priorityVal = next;
    if (!x->restoreMode && old != 0 && x->priorityVal != 0 && old != x->priorityVal) {
        if (old == 21) emit_cc(x, kCcPriority21, 0);
        else if (old == 27) emit_cc(x, kCcPriority27, 0);
    }
    set_optional_value(x, OptPriority, old, x->priorityVal);
}

/* ---------- lifecycle ---------- */

static void *cco_new(t_symbol * /*s*/, long /*argc*/, t_atom * /*argv*/)
{
    t_cco *x = (t_cco *)object_alloc(cco_class);
    if (!x) return nullptr;

    x->out_midi = intout(x);
    x->programVal = 0;
    x->volVal = 64;
    x->panVal = 64;
    x->modVal = 0;
    x->lfoSpdVal = 0;
    x->lfoDlyVal = 0;
    x->bendRangeVal = 0;
    x->modTypeVal = 0;
    x->tuneVal = 64;
    x->iecvVal = 0;
    x->ieclVal = 0;
    x->priorityVal = 0;
    x->restoreMode = 1;
    x->transportPlaying = 0;
    x->pendingClear = {};
    x->parserState = ParserState{};

    x->restoreMode = 0;
    return x;
}

static void cco_free(t_cco * /*x*/) {}

extern "C" void ext_main(void * /*r*/)
{
    t_class *c = class_new("ccomidi",
                           (method)cco_new, (method)cco_free,
                           (long)sizeof(t_cco), 0L, A_GIMME, 0);

    class_addmethod(c, (method)cco_int, "int", A_LONG, 0);
    // class_addmethod(c, (method)cco_pgm_in, "pgmchange", A_LONG, A_DEFLONG, 0);
    class_addmethod(c, (method)cco_bend, "bend", A_LONG, A_DEFLONG, 0);
    class_addmethod(c, (method)cco_sendall, "sendall", 0);
    class_addmethod(c, (method)cco_transport, "transport", A_LONG, 0);
    class_addmethod(c, (method)cco_panic, "panic", 0);
    class_addmethod(c, (method)cco_restore, "restore", A_LONG, 0);
    class_addmethod(c, (method)cco_program, "program", A_LONG, 0);
    class_addmethod(c, (method)cco_vol, "vol", A_LONG, 0);
    class_addmethod(c, (method)cco_pan, "pan", A_LONG, 0);
    class_addmethod(c, (method)cco_mod, "mod", A_LONG, 0);
    class_addmethod(c, (method)cco_lfo_spd, "lfo_spd", A_LONG, 0);
    class_addmethod(c, (method)cco_lfo_dly, "lfo_dly", A_LONG, 0);
    class_addmethod(c, (method)cco_bend_range, "bend_range", A_LONG, 0);
    class_addmethod(c, (method)cco_mod_type, "mod_type", A_LONG, 0);
    class_addmethod(c, (method)cco_tune, "tune", A_LONG, 0);
    class_addmethod(c, (method)cco_iecv, "iecv", A_LONG, 0);
    class_addmethod(c, (method)cco_iecl, "iecl", A_LONG, 0);
    class_addmethod(c, (method)cco_priority, "priority", A_LONG, 0);

    class_register(CLASS_BOX, c);
    cco_class = c;
}
