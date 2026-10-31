#ifndef M4A_SEQ_ESTIMATE_H
#define M4A_SEQ_ESTIMATE_H

#include "m4a_driver.h"

#define M4A_EST_CHANNELS (M4A_MAX_PCM_CHANNELS + 4)
#define M4A_EST_TICKS 8
#define MPT_FLG_VOLSET 0x01
#define MPT_FLG_VOLCHG 0x03
#define MPT_FLG_PITSET 0x04
#define MPT_FLG_PITCHG 0x0C
#define M4A_TRK_VOLCHG MPT_FLG_VOLCHG
#define M4A_TRK_PITCHG MPT_FLG_PITCHG
#define M4A_XCMD_IECV 8
#define M4A_XCMD_IECL 9

typedef struct {
    int32_t cycles;          /* MPlayMain's body since the last SoundMain (head / exit at publication) */
    int32_t held;            /* commands applied in frames without a tick: run at the next tick */
    uint32_t clock;          /* tempo ticks completed (the engine's tempoC / 150 accumulator) */
    uint8_t tickInFrame;     /* host-supplied tick boundaries since the last SoundMain */
    bool songDeclared;       /* the host declared the song's tracks */
    bool playing;            /* MusicPlayerInfo.status without PAUSE */
    bool allDone;            /* this frame's tick found no track (TICK_ALL_DONE, then PAUSE) */
    uint8_t trackCount;      /* the player's track count (MusicPlayerInfo.trackCount) */
    int16_t mpEntry, mpHead, mpPaused, mpExit;
    uint16_t declared;       /* the song's tracks (declared, or joined by a note) */
    uint16_t exist;          /* MPT_FLG_EXIST per engine track */
    uint16_t startPending;   /* MPT_FLG_START: the START init at the next tick */
    uint16_t ended;          /* FINE ran in this frame, on tick endTick[] */
    uint16_t firstNote;      /* a note has been seen since the start (mid2agb's implicit VOL) */
    uint16_t volSeen;        /* a VOL came before it */
    uint16_t barLen;         /* mid2agb's WholeNoteMark period in ticks (0: none) */
    uint32_t barStart;       /* the clock of the last time signature's mark */
    uint16_t loopBarLen;     /* the bar marks at the loop start ('['), restored by the GOTO */
    uint16_t loopBarPhase;
    bool loopSaved;
    uint32_t tempoClock;     /* the tick of the last TEMPO charged (UINT32_MAX: none) */
    /* per track: mid2agb's compression state and the W## bookkeeping */
    uint8_t lastOp[M4A_MAX_TRACKS];      /* s_lastOpName as the command byte (0: none) */
    uint8_t lastOpKey[M4A_MAX_TRACKS];   /* key + 1 of the note whose (predicted) command is lastOp, 0: none */
    uint8_t lastKey[M4A_MAX_TRACKS];     /* s_lastNote (0xFF: none) */
    uint8_t lastVel[M4A_MAX_TRACKS];     /* s_lastVelocity (0xFF: none) */
    uint8_t trackFlags[M4A_MAX_TRACKS];
    uint8_t extraChg[M4A_MAX_TRACKS];    /* VOLCHG / PITCHG from commands the engine never sees (KEYSH, the implicit VOL) */
    uint8_t cmdTicks[M4A_MAX_TRACKS];    /* the ticks (bit = tick index in the frame) the track had an event on */
    uint8_t endTick[M4A_MAX_TRACKS];     /* the tick index of this frame's FINE */
    bool keep[M4A_MAX_TRACKS];           /* s_keepLastOpName: a wait since the last note */
    uint32_t lastEvClock[M4A_MAX_TRACKS];
    uint32_t lastWClock[M4A_MAX_TRACKS];
    uint16_t lastDur[M4A_MAX_TRACKS];    /* the last finished note's length in ticks (0: none) */
    uint32_t keyOn[M4A_MAX_TRACKS][128]; /* the clock of each key's note-on (UINT32_MAX: none): its length at the note-off */
    /* the walk steps of channels a command took from a chain after the
     * owner's walk of that tick had met them, per tick of the frame */
    uint8_t ghostWalked[M4A_MAX_TRACKS][M4A_EST_TICKS];
    int16_t ghostCycles[M4A_MAX_TRACKS][M4A_EST_TICKS];
    /* per physical driver channel: the track chains */
    uint8_t owner[M4A_EST_CHANNELS]; /* track + 1 while chained */
    bool tie[M4A_EST_CHANNELS];      /* a TIE (predicted, or ended by EOT): gate 0 */
    uint32_t chSeq[M4A_EST_CHANNELS];
    uint32_t chStart[M4A_EST_CHANNELS];  /* the tick of its note (walked from the next one) */
    uint32_t chExpire[M4A_EST_CHANNELS]; /* the tick its N## gate expires (UINT32_MAX: not known) */
    uint32_t seqCounter;
} M4ASeqEstimate;

void m4a_est_init(M4ADriver* e);
void m4a_est_reset(M4ADriver* e);
void m4a_est_sound_off(M4ADriver* e);
void m4a_est_note_on(M4ADriver* e, int track, uint8_t key, uint8_t velocity, const ToneData* voice);
void m4a_est_note_off(M4ADriver* e, int track, uint8_t key);
void m4a_est_cc(M4ADriver* e, int track, uint8_t cc, uint8_t value);
void m4a_est_program(M4ADriver* e, int track);
void m4a_est_bend(M4ADriver* e, int track);
void m4a_est_xcmd(M4ADriver* e, int track, uint8_t command);
void m4a_est_track_end(M4ADriver* e, int track);
void m4a_est_tempo(M4ADriver* e);
void m4a_est_tick_boundary(M4ADriver* e);
uint16_t m4a_est_lfo_mask(const M4ADriver* e);
void m4a_est_charge(M4ADriver* e, int cycles);
void m4a_est_frame(M4ADriver* e, int nTicks);
int32_t m4a_est_publish(M4ADriver* e);

#endif
