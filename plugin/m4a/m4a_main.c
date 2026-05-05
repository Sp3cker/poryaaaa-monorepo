#include "m4a_internal.h"

/* Forward decls from m4a_cgb.c / m4a_pcm.c */
extern void m4a_cgb_sound(M4ADriver *drv);
extern void m4a_sound_main_ram(M4ADriver *drv);

/* SoundMain — pokeemerald m4a.c equivalent.  Fires once per vblank.
 * Tick c15 counter, drive tempo accumulator (which fires LFO ticks),
 * then run the CGB envelope/length tick + the PCM software mixer.
 *
 * MPlayMain's song-walk lives in the DAW (poryaaaa is a player, not a
 * sequencer), so we do not advance MIDI events here.  Everything else
 * the real m4a driver does inside MPlayMain — LFO advancement, the
 * per-track derived state — happens via the LFO tick + the
 * vol/pit/pan CC handlers in m4a_track.c. */
static void m4a_sound_main(M4ADriver *drv) {
    if (drv->c15 > 0)
        drv->c15--;
    else
        drv->c15 = 14;

    /* Tempo accumulator: tempoC += tempoI per vblank; one LFO tick
     * fires per 150 accumulated units (matches v1's tempoI/150
     * ticks-per-vblank rate, mirrors pokeemerald m4a.c). */
    drv->tempoC += drv->tempoI;
    while (drv->tempoC >= 150) {
        drv->tempoC -= 150;
        m4a_internal_lfo_tick(drv);
    }

    m4a_cgb_sound(drv);
    m4a_sound_main_ram(drv);
}

/* Advance the driver's internal vblank clock by `host_frames` at the
 * configured host rate.  Fires SoundMain N times where N = floor(elapsed
 * vblanks).  Multiple host-block calls between vblanks are no-ops on
 * register state but advance the host-side accumulator.
 *
 * Each vblank firing is tagged with an event_vblank_offset that the
 * register-write emitters in m4a_cgb.c use as the sample_offset for
 * any event they queue during that vblank.  The offset is
 * render-span-relative (since the last m4a_consume_writes call). */
void m4a_advance(M4ADriver *drv, int host_frames) {
    if (!drv) return;
    if (host_frames <= 0) return;
    if (drv->vblank_step <= 0.0) {
        drv->event_render_offset += (uint32_t)host_frames;
        return;
    }

    uint32_t base = drv->event_render_offset;
    drv->vblank_accum += (double)host_frames;
    while (drv->vblank_accum >= drv->vblank_step) {
        drv->vblank_accum -= drv->vblank_step;
        /* Vblank fires (host_frames - vblank_accum) frames into this
         * advance call; add to base for the render-span-relative
         * offset that gets stamped onto every event emitted by
         * m4a_sound_main below. */
        uint32_t into_call = (uint32_t)((double)host_frames - drv->vblank_accum + 0.5);
        if (into_call > (uint32_t)host_frames) into_call = (uint32_t)host_frames;
        drv->event_vblank_offset = base + into_call;
        m4a_sound_main(drv);
    }
    drv->event_render_offset = base + (uint32_t)host_frames;
}
