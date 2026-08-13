use std::ffi::{c_char, c_float, c_int, c_uchar};

pub(crate) enum M4ADriver {}
pub(crate) enum HwAudio {}
pub(crate) enum M4ARegWriteBatch {}
pub(crate) enum LoadedVoiceGroup {}
pub(crate) enum ToneData {}

pub(crate) fn recommended_max_advance_frames() -> usize {
    let frames = usize::try_from(unsafe { m4a_driver_recommended_max_advance_frames() })
        .expect("native driver frame limit must be positive");
    assert!(frames > 0, "native driver frame limit must be positive");
    frames
}

unsafe extern "C" {
    pub(crate) fn m4a_driver_create(sample_rate: c_float) -> *mut M4ADriver;
    pub(crate) fn m4a_driver_destroy(driver: *mut M4ADriver);
    pub(crate) fn m4a_driver_set_voicegroup(driver: *mut M4ADriver, voice_group: *mut ToneData);
    pub(crate) fn m4a_driver_set_portamento_enabled(driver: *mut M4ADriver, enabled: c_uchar);
    pub(crate) fn m4a_driver_set_pwm_enabled(driver: *mut M4ADriver, enabled: c_uchar);
    pub(crate) fn m4a_set_max_pcm_channels(driver: *mut M4ADriver, max_channels: c_uchar);
    pub(crate) fn m4a_set_song_volume(driver: *mut M4ADriver, volume: c_uchar);
    pub(crate) fn m4a_set_reverb_amount(driver: *mut M4ADriver, amount: c_uchar);
    pub(crate) fn m4a_set_tempo_bpm(driver: *mut M4ADriver, bpm: f64);
    pub(crate) fn m4a_note_on(
        driver: *mut M4ADriver,
        track_index: c_int,
        key: c_uchar,
        velocity: c_uchar,
    );
    pub(crate) fn m4a_note_off(driver: *mut M4ADriver, track_index: c_int, key: c_uchar);
    pub(crate) fn m4a_program_change(driver: *mut M4ADriver, track_index: c_int, program: c_uchar);
    pub(crate) fn m4a_cc(driver: *mut M4ADriver, track_index: c_int, cc: c_uchar, value: c_uchar);
    pub(crate) fn m4a_pitch_bend(driver: *mut M4ADriver, track_index: c_int, bend: i16);
    pub(crate) fn m4a_all_notes_off(driver: *mut M4ADriver, track_index: c_int);
    pub(crate) fn m4a_all_sound_off(driver: *mut M4ADriver);
    pub(crate) fn m4a_advance(driver: *mut M4ADriver, host_frames: c_int);
    pub(crate) fn m4a_get_pending_writes(driver: *const M4ADriver) -> *const M4ARegWriteBatch;
    pub(crate) fn m4a_consume_writes(driver: *mut M4ADriver);
    pub(crate) fn m4a_driver_recommended_max_advance_frames() -> c_int;

    pub(crate) fn hw_audio_create(sample_rate: c_float) -> *mut HwAudio;
    pub(crate) fn hw_audio_destroy(hardware: *mut HwAudio);
    pub(crate) fn hw_audio_render_events(
        hardware: *mut HwAudio,
        events: *const M4ARegWriteBatch,
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
