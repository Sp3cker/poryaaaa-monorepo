/*
 * ccomidi.legatobend - Max for Live MIDI portamento-note-glide transform.
 *
 * Primary wiring inside a future .amxd:
 *   [midiin] -> [ccomidi.legatobend] -> [midiout]
 */

extern "C" {
#include "ext.h"
#include "ext_obex.h"
}

#include "legatobend_core.hpp"
#include "legatobend_parser.hpp"

#include <cstdint>
#include <vector>

using namespace ccomidi_legatobend;

namespace {

constexpr auto kRampTickMs = 5L;

}  // namespace

typedef struct _legatobend {
    t_object ob;
    void* out_midi;
    t_clock* clock;
    ParserState parser;
    LegatoBendCore* core;
    double last_time_ms;
} t_legatobend;

static t_class* legatobend_class = nullptr;

static void legatobend_emit(t_legatobend* x, std::vector<std::uint8_t> const& bytes)
{
    for (auto byte : bytes) {
        outlet_int(x->out_midi, byte);
    }
}

static void legatobend_schedule_if_needed(t_legatobend* x)
{
    if (x->core->has_active_ramp()) {
        clock_delay(x->clock, kRampTickMs);
    }
}

static void legatobend_advance(t_legatobend* x)
{
    auto now = 0.0;
    clock_getftime(&now);
    if (x->last_time_ms <= 0.0) {
        x->last_time_ms = now;
        return;
    }
    auto out = std::vector<std::uint8_t>{};
    x->core->advance(now - x->last_time_ms, out);
    x->last_time_ms = now;
    legatobend_emit(x, out);
}

static void legatobend_tick(t_legatobend* x)
{
    legatobend_advance(x);
    legatobend_schedule_if_needed(x);
}

static void legatobend_int(t_legatobend* x, long value)
{
    auto byte = std::uint8_t(value & 0xFF);
    if (!x->core->enabled()) {
        outlet_int(x->out_midi, byte);
        return;
    }
    legatobend_advance(x);
    auto message = parse_byte(x->parser, byte);
    auto out = std::vector<std::uint8_t>{};
    x->core->process(message, out);
    legatobend_emit(x, out);
    legatobend_schedule_if_needed(x);
}

static void legatobend_bend_time(t_legatobend* x, long value)
{
    x->core->set_bend_time_ms(value);
    if (!x->core->enabled()) {
        x->core->reset();
        x->parser = ParserState{};
        clock_unset(x->clock);
    }
}

static void legatobend_bend_curve(t_legatobend* x, t_symbol* value)
{
    if (value == gensym("linear")) {
        x->core->set_bend_curve(BendCurve::Linear);
        return;
    }
    if (value == gensym("easing")) {
        x->core->set_bend_curve(BendCurve::Easing);
        return;
    }
    object_error((t_object*)x, "bend_curve expects linear or easing");
}

static void* legatobend_new(t_symbol* /*s*/, long /*argc*/, t_atom* /*argv*/)
{
    auto* x = (t_legatobend*)object_alloc(legatobend_class);
    if (!x) return nullptr;
    x->out_midi = intout(x);
    x->clock = clock_new(x, (method)legatobend_tick);
    x->parser = ParserState{};
    x->core = new LegatoBendCore();
    x->last_time_ms = 0.0;
    return x;
}

static void legatobend_free(t_legatobend* x)
{
    if (x->clock) clock_free(x->clock);
    delete x->core;
}

extern "C" void ext_main(void* /*r*/)
{
    auto* c = class_new("ccomidi.legatobend",
                        (method)legatobend_new, (method)legatobend_free,
                        (long)sizeof(t_legatobend), 0L, A_GIMME, 0);

    class_addmethod(c, (method)legatobend_int, "int", A_LONG, 0);
    class_addmethod(c, (method)legatobend_bend_time, "bend_time", A_LONG, 0);
    class_addmethod(c, (method)legatobend_bend_curve, "bend_curve", A_SYM, 0);

    class_register(CLASS_BOX, c);
    legatobend_class = c;
}
