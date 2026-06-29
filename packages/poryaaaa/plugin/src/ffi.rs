use std::ffi::{c_char, c_float, c_int, c_uchar};

pub(crate) enum M4AEngine {}
pub(crate) enum LoadedVoiceGroup {}
pub(crate) enum ToneData {}

unsafe extern "C" {
    pub(crate) fn m4a_engine_create(sample_rate: c_float) -> *mut M4AEngine;
    pub(crate) fn m4a_engine_free(engine: *mut M4AEngine);
    pub(crate) fn m4a_engine_reset(engine: *mut M4AEngine) -> bool;
    pub(crate) fn m4a_engine_set_voicegroup(engine: *mut M4AEngine, voice_group: *mut ToneData);
    pub(crate) fn m4a_engine_set_volume(engine: *mut M4AEngine, volume: c_uchar);
    pub(crate) fn m4a_engine_set_reverb_amount(engine: *mut M4AEngine, amount: c_uchar);
    pub(crate) fn m4a_engine_set_tempo_bpm(engine: *mut M4AEngine, bpm: f64);
    pub(crate) fn m4a_engine_note_on(
        engine: *mut M4AEngine,
        track_index: c_int,
        key: c_uchar,
        velocity: c_uchar,
    );
    pub(crate) fn m4a_engine_note_off(engine: *mut M4AEngine, track_index: c_int, key: c_uchar);
    pub(crate) fn m4a_engine_program_change(
        engine: *mut M4AEngine,
        track_index: c_int,
        program: c_uchar,
    );
    pub(crate) fn m4a_engine_cc(
        engine: *mut M4AEngine,
        track_index: c_int,
        cc: c_uchar,
        value: c_uchar,
    );
    pub(crate) fn m4a_engine_pitch_bend(engine: *mut M4AEngine, track_index: c_int, bend: i16);
    pub(crate) fn m4a_engine_all_notes_off(engine: *mut M4AEngine, track_index: c_int);
    pub(crate) fn m4a_engine_all_sound_off(engine: *mut M4AEngine);
    pub(crate) fn m4a_engine_process(
        engine: *mut M4AEngine,
        out_l: *mut c_float,
        out_r: *mut c_float,
        num_samples: c_int,
    );

    pub(crate) fn voicegroup_load(
        project_root: *const c_char,
        voicegroup_name: *const c_char,
    ) -> *mut LoadedVoiceGroup;
    pub(crate) fn voicegroup_free(vg: *mut LoadedVoiceGroup);
    pub(crate) fn voicegroup_loaded_voices(vg: *mut LoadedVoiceGroup) -> *mut ToneData;
    pub(crate) fn voicegroup_loader_last_error() -> *const c_char;
}
