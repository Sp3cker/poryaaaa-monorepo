use crate::ffi;
use crate::params::MixerMode;
use crate::process::ProcessRuntime;
use std::ffi::{CStr, CString};
use std::ptr::NonNull;

struct DriverHandle {
    ptr: NonNull<ffi::M4ADriver>,
}

impl DriverHandle {
    fn new(sample_rate: f32) -> Result<Self, RuntimeError> {
        let ptr = unsafe { ffi::m4a_driver_create(sample_rate) };
        let ptr = NonNull::new(ptr).ok_or(RuntimeError::DriverCreateFailed { sample_rate })?;
        let mut driver = Self { ptr };
        unsafe {
            ffi::m4a_driver_set_portamento_enabled(driver.as_mut_ptr(), 1);
            ffi::m4a_driver_set_pwm_enabled(driver.as_mut_ptr(), 1);
            ffi::m4a_set_max_pcm_channels(driver.as_mut_ptr(), 12);
        }
        Ok(driver)
    }

    fn as_const_ptr(&self) -> *const ffi::M4ADriver {
        self.ptr.as_ptr()
    }

    fn as_mut_ptr(&mut self) -> *mut ffi::M4ADriver {
        self.ptr.as_ptr()
    }
}

impl Drop for DriverHandle {
    fn drop(&mut self) {
        unsafe {
            ffi::m4a_driver_destroy(self.as_mut_ptr());
        }
    }
}

struct HardwareHandle {
    ptr: NonNull<ffi::HwAudio>,
}

impl HardwareHandle {
    fn new(sample_rate: f32) -> Result<Self, RuntimeError> {
        let ptr = unsafe { ffi::hw_audio_create(sample_rate) };
        let ptr = NonNull::new(ptr).ok_or(RuntimeError::HardwareCreateFailed { sample_rate })?;
        Ok(Self { ptr })
    }

    fn as_mut_ptr(&mut self) -> *mut ffi::HwAudio {
        self.ptr.as_ptr()
    }
}

impl Drop for HardwareHandle {
    fn drop(&mut self) {
        unsafe {
            ffi::hw_audio_destroy(self.as_mut_ptr());
        }
    }
}

pub(crate) struct LoadedVoiceGroupHandle {
    ptr: NonNull<ffi::LoadedVoiceGroup>,
}

impl LoadedVoiceGroupHandle {
    fn load(project_root: &str, bank: &str) -> Result<Self, RuntimeError> {
        let project_root_c =
            CString::new(project_root).map_err(|_| RuntimeError::InvalidCString {
                field: "project_root",
            })?;
        let bank_c =
            CString::new(bank).map_err(|_| RuntimeError::InvalidCString { field: "bank" })?;
        let ptr = unsafe { ffi::voicegroup_load(project_root_c.as_ptr(), bank_c.as_ptr()) };
        let ptr = NonNull::new(ptr)
            .ok_or_else(|| RuntimeError::VoicegroupLoadFailed(last_voicegroup_error()))?;
        Ok(Self { ptr })
    }

    fn voices(&mut self) -> Option<NonNull<ffi::ToneData>> {
        NonNull::new(unsafe { ffi::voicegroup_loaded_voices(self.ptr.as_ptr()) })
    }
}

impl Drop for LoadedVoiceGroupHandle {
    fn drop(&mut self) {
        unsafe {
            ffi::voicegroup_free(self.ptr.as_ptr());
        }
    }
}

#[derive(Debug, PartialEq)]
pub enum RuntimeError {
    DriverCreateFailed { sample_rate: f32 },
    HardwareCreateFailed { sample_rate: f32 },
    InvalidCString { field: &'static str },
    VoicegroupLoadFailed(String),
    VoicegroupHasNoVoices,
    ResetFailed,
}

impl std::fmt::Display for RuntimeError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::DriverCreateFailed { sample_rate } => {
                write!(
                    f,
                    "Failed to create m4a driver at sample rate {}",
                    sample_rate
                )
            }
            Self::HardwareCreateFailed { sample_rate } => {
                write!(
                    f,
                    "Failed to create hardware renderer at sample rate {}",
                    sample_rate
                )
            }
            Self::InvalidCString { field } => {
                write!(f, "Field '{}' contains interior NUL byte", field)
            }
            Self::VoicegroupLoadFailed(msg) => write!(f, "Voicegroup load failed: {}", msg),
            Self::VoicegroupHasNoVoices => write!(f, "Loaded voicegroup has no voices"),
            Self::ResetFailed => write!(f, "Runtime reset failed"),
        }
    }
}

impl std::error::Error for RuntimeError {}

#[derive(Clone, Copy, PartialEq)]
pub struct RuntimeConfig {
    pub sample_rate: f32,
    pub volume: u8,
    pub reverb: u8,
}

pub struct M4aRuntime {
    driver: Option<DriverHandle>,
    hardware: Option<HardwareHandle>,
    voicegroup: Option<LoadedVoiceGroupHandle>,
    last_applied_rate: f32,
}

// SAFETY: M4aRuntime owns address-stable native driver and hardware allocations.
// It is used by a single host/plugin thread at a time through
// Arc<Mutex<Option<M4aRuntime>>>. It does not implement Sync.
unsafe impl Send for M4aRuntime {}

impl M4aRuntime {
    pub fn new(config: RuntimeConfig) -> Result<Self, RuntimeError> {
        let (driver, hardware) = Self::create_native(config.sample_rate)?;
        let mut runtime = Self {
            driver: Some(driver),
            hardware: Some(hardware),
            voicegroup: None,
            last_applied_rate: config.sample_rate,
        };
        runtime.set_volume(config.volume);
        runtime.set_reverb_amount(config.reverb);
        Ok(runtime)
    }

    pub fn load_voicegroup(&mut self, project_root: &str, bank: &str) -> Result<(), RuntimeError> {
        let mut loaded = LoadedVoiceGroupHandle::load(project_root, bank)?;
        let voices = loaded.voices().ok_or(RuntimeError::VoicegroupHasNoVoices)?;
        self.bind_voicegroup_ptr(Some(voices));
        self.voicegroup = Some(loaded);
        Ok(())
    }

    pub fn clear_voicegroup(&mut self) {
        self.bind_voicegroup_ptr(None);
        self.voicegroup = None;
    }

    pub fn reset(&mut self) -> Result<(), RuntimeError> {
        if self.driver.is_none() || self.hardware.is_none() {
            self.retire_after_failed_reset();
            return Err(RuntimeError::ResetFailed);
        }

        self.destroy_native();
        let (driver, hardware) = match Self::create_native(self.last_applied_rate) {
            Ok(handles) => handles,
            Err(_) => {
                self.retire_after_failed_reset();
                return Err(RuntimeError::ResetFailed);
            }
        };
        self.driver = Some(driver);
        self.hardware = Some(hardware);
        self.rebind_loaded_voicegroup();
        Ok(())
    }

    #[allow(dead_code)]
    pub fn reconfigure(&mut self, config: RuntimeConfig) -> Result<(), RuntimeError> {
        if self.driver.is_none()
            || self.hardware.is_none()
            || (config.sample_rate - self.last_applied_rate).abs() > 0.001
        {
            self.clear_voicegroup();
            self.destroy_native();
            let (driver, hardware) = Self::create_native(config.sample_rate)?;
            self.driver = Some(driver);
            self.hardware = Some(hardware);
            self.last_applied_rate = config.sample_rate;
        } else {
            self.reset()?;
        }
        self.set_volume(config.volume);
        self.set_reverb_amount(config.reverb);
        Ok(())
    }

    pub fn is_ready(&self) -> bool {
        self.voicegroup.is_some() && self.driver.is_some() && self.hardware.is_some()
    }

    pub fn set_volume(&mut self, volume: u8) {
        if let Some(driver) = self.driver.as_mut() {
            unsafe { ffi::m4a_set_song_volume(driver.as_mut_ptr(), volume) }
        }
    }

    pub fn set_reverb_amount(&mut self, amount: u8) {
        if let Some(driver) = self.driver.as_mut() {
            unsafe { ffi::m4a_set_reverb_amount(driver.as_mut_ptr(), amount) }
        }
    }
    /// Queues a validated mixer selection for the driver's next audio boundary.
    pub fn set_pcm_mixer_mode(&mut self, mode: MixerMode) -> bool {
        self.driver.as_mut().is_some_and(|driver| unsafe {
            ffi::m4a_driver_set_pcm_mixer_mode(driver.as_mut_ptr(), ffi::mixer_mode_to_c(mode))
        })
    }

    /// Reads the driver's active mixer mode for runtime and event-order tests.
    #[cfg(test)]
    pub(crate) fn pcm_mixer_mode(&self) -> Option<MixerMode> {
        self.driver.as_ref().and_then(|driver| {
            let mode = unsafe { ffi::m4a_driver_get_pcm_mixer_mode(driver.as_const_ptr()) };
            ffi::mixer_mode_from_c(mode)
        })
    }


    #[allow(dead_code)]
    pub fn all_notes_off(&mut self, track: i32) {
        if let Some(driver) = self.driver.as_mut() {
            unsafe { ffi::m4a_all_notes_off(driver.as_mut_ptr(), track) }
        }
    }

    #[allow(dead_code)]
    pub fn all_sound_off(&mut self) {
        if let Some(driver) = self.driver.as_mut() {
            unsafe { ffi::m4a_all_sound_off(driver.as_mut_ptr()) }
        }
    }

    fn create_native(sample_rate: f32) -> Result<(DriverHandle, HardwareHandle), RuntimeError> {
        let driver = DriverHandle::new(sample_rate)?;
        let hardware = HardwareHandle::new(sample_rate)?;
        Ok((driver, hardware))
    }

    // Hardware must release before its driver. This is also used after a failed
    // allocation transaction, so each successfully created native allocation has
    // one owning Rust handle and one corresponding destructor call.
    fn destroy_native(&mut self) {
        let hardware = self.hardware.take();
        let driver = self.driver.take();
        drop(hardware);
        drop(driver);
    }

    fn retire_after_failed_reset(&mut self) {
        self.clear_voicegroup();
        self.destroy_native();
    }

    fn rebind_loaded_voicegroup(&mut self) {
        let voices = self
            .voicegroup
            .as_mut()
            .and_then(LoadedVoiceGroupHandle::voices);
        self.bind_voicegroup_ptr(voices);
    }

    fn bind_voicegroup_ptr(&mut self, voices: Option<NonNull<ffi::ToneData>>) {
        if let Some(driver) = self.driver.as_mut() {
            unsafe {
                ffi::m4a_driver_set_voicegroup(
                    driver.as_mut_ptr(),
                    voices.map_or(std::ptr::null_mut(), NonNull::as_ptr),
                );
            }
        }
    }

    fn render_chunk(&mut self, left: &mut [f32], right: &mut [f32]) {
        let (Some(driver), Some(hardware)) = (self.driver.as_mut(), self.hardware.as_mut()) else {
            return;
        };
        let frames = i32::try_from(left.len()).expect("driver render chunk fits c_int");

        unsafe {
            ffi::m4a_advance(driver.as_mut_ptr(), frames);
            let writes = ffi::m4a_get_pending_writes(driver.as_const_ptr());
            ffi::hw_audio_render_events(
                hardware.as_mut_ptr(),
                writes,
                left.as_mut_ptr(),
                right.as_mut_ptr(),
                frames,
            );
            ffi::m4a_consume_writes(driver.as_mut_ptr());
        }
    }
}

impl ProcessRuntime for M4aRuntime {
    fn set_tempo_bpm(&mut self, bpm: f64) {
        if let Some(driver) = self.driver.as_mut() {
            unsafe { ffi::m4a_set_tempo_bpm(driver.as_mut_ptr(), bpm) }
        }
    }

    fn note_on(&mut self, track: i32, key: u8, velocity: u8) {
        if let Some(driver) = self.driver.as_mut() {
            unsafe { ffi::m4a_note_on(driver.as_mut_ptr(), track, key, velocity) }
        }
    }

    fn note_off(&mut self, track: i32, key: u8) {
        if let Some(driver) = self.driver.as_mut() {
            unsafe { ffi::m4a_note_off(driver.as_mut_ptr(), track, key) }
        }
    }

    fn program_change(&mut self, track: i32, program: u8) {
        if let Some(driver) = self.driver.as_mut() {
            unsafe { ffi::m4a_program_change(driver.as_mut_ptr(), track, program) }
        }
    }

    fn cc(&mut self, track: i32, cc: u8, value: u8) {
        if let Some(driver) = self.driver.as_mut() {
            unsafe { ffi::m4a_cc(driver.as_mut_ptr(), track, cc, value) }
        }
    }

    fn pitch_bend(&mut self, track: i32, bend: i16) {
        if let Some(driver) = self.driver.as_mut() {
            unsafe { ffi::m4a_pitch_bend(driver.as_mut_ptr(), track, bend) }
        }
    }

    fn process(&mut self, left: &mut [f32], right: &mut [f32]) {
        debug_assert_eq!(left.len(), right.len());
        let total_frames = left.len().min(right.len());
        let max_frames = ffi::recommended_max_advance_frames();
        let mut offset = 0;
        while offset < total_frames {
            let end = (offset + max_frames).min(total_frames);
            self.render_chunk(&mut left[offset..end], &mut right[offset..end]);
            offset = end;
        }
    }
}

impl Drop for M4aRuntime {
    fn drop(&mut self) {
        self.clear_voicegroup();
        self.destroy_native();
    }
}

fn last_voicegroup_error() -> String {
    let ptr = unsafe { ffi::voicegroup_loader_last_error() };
    if ptr.is_null() {
        return "voicegroup_load failed".to_owned();
    }
    let message = unsafe { CStr::from_ptr(ptr) }
        .to_string_lossy()
        .into_owned();
    if message.is_empty() {
        "voicegroup_load failed".to_owned()
    } else {
        message
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn runtime_reconfigure_and_reset() {
        let config = RuntimeConfig {
            sample_rate: 44100.0,
            volume: 100,
            reverb: 10,
        };
        let mut runtime = M4aRuntime::new(config).expect("create runtime");
        assert!(!runtime.is_ready());

        assert!(runtime.reset().is_ok());
        assert!(runtime.reconfigure(config).is_ok());

        let new_config = RuntimeConfig {
            sample_rate: 48000.0,
            volume: 120,
            reverb: 20,
        };
        assert!(runtime.reconfigure(new_config).is_ok());
    }

    #[test]
    fn mixer_mode_request_commits_at_render_boundary() {
        let config = RuntimeConfig {
            sample_rate: 44100.0,
            volume: 100,
            reverb: 10,
        };
        let mut runtime = M4aRuntime::new(config).expect("create runtime");

        assert_eq!(runtime.pcm_mixer_mode(), Some(MixerMode::Ipatix));
        assert!(runtime.set_pcm_mixer_mode(MixerMode::Sappy));
        assert_eq!(runtime.pcm_mixer_mode(), Some(MixerMode::Ipatix));

        let mut left = [0.0; 1024];
        let mut right = [0.0; 1024];
        runtime.process(&mut left, &mut right);
        assert_eq!(runtime.pcm_mixer_mode(), Some(MixerMode::Sappy));
    }

    #[test]
    fn failed_reset_retires_native_handles() {
        let config = RuntimeConfig {
            sample_rate: 44100.0,
            volume: 100,
            reverb: 10,
        };
        let mut runtime = M4aRuntime::new(config).expect("create runtime");
        runtime.driver = None;

        assert!(runtime.reset().is_err());
        assert!(runtime.driver.is_none());
        assert!(runtime.hardware.is_none());
    }
}
