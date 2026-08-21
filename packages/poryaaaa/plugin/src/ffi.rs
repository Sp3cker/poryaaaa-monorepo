use crate::params::MixerMode;
use std::ffi::{c_char, c_float, c_int, c_uchar};

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum M4APcmMixerMode {
    Ipatix = 0,
    Sappy = 1,
}

pub(crate) fn mixer_mode_to_c(mode: MixerMode) -> M4APcmMixerMode {
    match mode {
        MixerMode::Ipatix => M4APcmMixerMode::Ipatix,
        MixerMode::Sappy => M4APcmMixerMode::Sappy,
    }
}

#[cfg(test)]
pub(crate) fn mixer_mode_from_c(mode: M4APcmMixerMode) -> Option<MixerMode> {
    match mode {
        M4APcmMixerMode::Ipatix => Some(MixerMode::Ipatix),
        M4APcmMixerMode::Sappy => Some(MixerMode::Sappy),
    }
}
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
    pub(crate) fn m4a_driver_set_pcm_mixer_mode(
        driver: *mut M4ADriver,
        mode: M4APcmMixerMode,
    ) -> bool;
    #[cfg(test)]
    pub(crate) fn m4a_driver_get_pcm_mixer_mode(
        driver: *const M4ADriver,
    ) -> M4APcmMixerMode;
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn mixer_mode_mapping_matches_c_enum_values() {
        assert_eq!(M4APcmMixerMode::Ipatix as i32, 0);
        assert_eq!(M4APcmMixerMode::Sappy as i32, 1);
        assert_eq!(
            mixer_mode_to_c(MixerMode::Ipatix),
            M4APcmMixerMode::Ipatix
        );
        assert_eq!(mixer_mode_to_c(MixerMode::Sappy), M4APcmMixerMode::Sappy);
        assert_eq!(
            mixer_mode_from_c(M4APcmMixerMode::Ipatix),
            Some(MixerMode::Ipatix)
        );
        assert_eq!(
            mixer_mode_from_c(M4APcmMixerMode::Sappy),
            Some(MixerMode::Sappy)
        );
    }
}
