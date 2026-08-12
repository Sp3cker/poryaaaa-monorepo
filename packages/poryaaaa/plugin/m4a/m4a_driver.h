#ifndef M4A_DRIVER_H
#define M4A_DRIVER_H

#include <stddef.h>

#include "m4a_register_file.h"
#include "m4a_pcm_ring.h"
#include "voicegroup/voicegroup_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Software m4a driver. Mirrors the real m4a routines from the
     * pokeemerald disassembly (Music 4 Advance / Sappy / MPlayDef).
     * m4a_advance fires SoundMain at VBlank cadence; the register file
     * and PCM ring are the authoritative driver-to-chip outputs. Song
     * sequencing remains external because the DAW or renderer owns it. */
    typedef struct M4ADriver M4ADriver;

    /* Mirror of M4AEngineXcmdFn so v2 can be wired into poryaaaa's xcmd path
     * without dragging in m4a_engine.h. */
    typedef void (*M4ADriverXcmdFn)(void* ctx, int trackIndex, uint8_t selector, uint32_t value);

    /* Canonical hardware time shared by every public event consumer. */
#define M4A_GBA_CYCLES_PER_SECOND 16777216u
#define M4A_VBLANK_CYCLES 280896u

    /* ---- Cycle-domain driver→chip event contract ----
     *
     * Driver events are ordered hardware observations, not host-buffer
     * annotations.  Each carries an absolute GBA cycle and an order that
     * restarts at zero whenever the cycle changes.  The chip advances to
     * that cycle before applying the write.  The register snapshot remains
     * available for UI/debug consumers only; timing-sensitive chip logic
     * MUST consume this stream. */
    typedef enum
    {
        M4A_REG_NR10,
        M4A_REG_NR11,
        M4A_REG_NR12,
        M4A_REG_NR13,
        M4A_REG_NR14,
        M4A_REG_NR21,
        M4A_REG_NR22,
        M4A_REG_NR23,
        M4A_REG_NR24,
        M4A_REG_NR30,
        M4A_REG_NR31,
        M4A_REG_NR32,
        M4A_REG_NR33,
        M4A_REG_NR34,
        M4A_REG_NR41,
        M4A_REG_NR42,
        M4A_REG_NR43,
        M4A_REG_NR44,
        M4A_REG_NR50,
        M4A_REG_NR51,
        M4A_REG_NR52,
        M4A_REG_SOUNDCNT_H,
        M4A_REG_SOUNDBIAS,
        /* Generic byte event retained for replaying external trace writes.
         * value = (addr_in_wave_ram << 8) | byte. */
        M4A_REG_WAVE_RAM_BYTE,
        /* ROM CgbSound's four fixed little-endian 32-bit Wave RAM stores.
         * value is the raw word payload for 0x04000090 + 4 * word_index. */
        M4A_REG_WAVE_RAM_WORD_0,
        M4A_REG_WAVE_RAM_WORD_1,
        M4A_REG_WAVE_RAM_WORD_2,
        M4A_REG_WAVE_RAM_WORD_3,
        /* Canonical DirectSound bus observations.  FIFO values are one
         * little-endian 32-bit word; TIMER event values identify timer 0/1. */
        M4A_REG_FIFO_A,
        M4A_REG_FIFO_B,
        M4A_REG_TIMER_0,
        M4A_REG_TIMER_1,
    } M4ARegId;

    typedef struct
    {
        uint64_t cycle; /* absolute GBA cycle */
        M4ARegId reg;
        uint32_t value; /* register payload, see plan §6c */
        uint32_t order; /* strictly increasing among events at `cycle` */
    } M4ARegWrite;

    typedef struct
    {
        const M4ARegWrite* events;
        size_t count;
        uint64_t begin_cycle; /* inclusive render interval */
        uint64_t end_cycle;   /* inclusive event boundary, exclusive output end */
    } M4ARegWriteBatch;

    /* Lifecycle */
    M4ADriver* m4a_driver_create(float host_sample_rate);
    void m4a_driver_destroy(M4ADriver* drv);
    void m4a_driver_set_host_rate(M4ADriver* drv, float hz);
    /* `rate == 0` follows the driver's host rate.  The active geometry is
     * bounded by the static PCM ring capacity. */
    void m4a_driver_set_pcm_mix_rate(M4ADriver* drv, float rate);
    void m4a_driver_set_xcmd_callback(M4ADriver* drv, M4ADriverXcmdFn fn, void* ctx);
    /* Voicegroup wiring (driver receives an already-populated ToneData*). */
    void m4a_driver_set_voicegroup(M4ADriver* drv, ToneData* vg);
    void m4a_driver_refresh_voices(M4ADriver* drv);

    /* MIDI ingress. */
    void m4a_note_on(M4ADriver* drv, int track, uint8_t key, uint8_t velocity);
    void m4a_note_off(M4ADriver* drv, int track, uint8_t key);
    void m4a_cc(M4ADriver* drv, int track, uint8_t cc, uint8_t value);
    void m4a_pitch_bend(M4ADriver* drv, int track, int16_t bend);
    void m4a_program_change(M4ADriver* drv, int track, uint8_t program);
    void m4a_all_notes_off(M4ADriver* drv, int track);
    void m4a_all_sound_off(M4ADriver* drv);

    /* Engine-level params. */
    void m4a_set_song_volume(M4ADriver* drv, uint8_t volume);
    void m4a_set_master_volume(M4ADriver* drv, uint8_t volume); /* 0..15 m4a master */
    void m4a_set_reverb_amount(M4ADriver* drv, uint8_t amount); /* 0..127 */
    void m4a_set_analog_filter(M4ADriver* drv, bool enabled);   /* chip-side LPF */
    void m4a_set_max_pcm_channels(M4ADriver* drv, uint8_t maxChannels);
    void m4a_set_tempo_bpm(M4ADriver* drv, double bpm);

    /* Advance the driver by `host_frames` while converting the host duration
     * to absolute GBA cycles with an integer remainder.  SoundMain fires at
     * the exact 280896-cycle VBlank cadence and emits ordered cycle events.
     * Bound `host_frames` to M4A_RECOMMENDED_MAX_ADVANCE_FRAMES per
     * render-event-consume cycle (chunk if larger).  m4a_get_events_dropped()
     * reports any overflow. */
    void m4a_advance(M4ADriver* drv, int host_frames);

    /* Returns the completed absolute GBA cycle of the host-facing advance. */
    uint64_t m4a_driver_current_cycle(const M4ADriver* drv);

    /* Rebase a fresh driver's absolute timeline before its first render. */
    bool m4a_driver_set_initial_cycle(M4ADriver* drv, uint64_t cycle);

    /* Read-only accessor for non-timing consumers (UI, params, debug). */
    const M4ARegisterFile* m4a_get_register_file(const M4ADriver* drv);
    const M4APcmRing* m4a_get_pcm_ring(const M4ADriver* drv);

    /* Mutable accessor for the chip's render path.  The chip *consumes*
     * edge-trigger latches (trigger_sq1/sq2/wave/noise) by clearing them
     * after applying the corresponding NRx4 write — see §6a "Edge-trigger
     * latches" in HW_AUDIO_SCAFFOLD_PLAN.md.  Other consumers should use
     * the const accessor above. */
    M4ARegisterFile* m4a_get_register_file_mut(M4ADriver* drv);

/* Layer 1.5 event-stream accessors.
 *
 * Capacity / chunking: the queue is bounded. m4a_get_events_dropped() returns a
 * monotonic counter incremented on overflow; tests assert it stays 0. */
#define M4A_RECOMMENDED_MAX_ADVANCE_FRAMES 2048

    const M4ARegWriteBatch* m4a_get_pending_writes(const M4ADriver* drv);
    void m4a_consume_writes(M4ADriver* drv);
    uint32_t m4a_get_events_dropped(const M4ADriver* drv);

#ifdef __cplusplus
}
#endif

#endif
