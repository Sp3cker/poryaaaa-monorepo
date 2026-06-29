use crate::ffi;
use crate::process::ProcessRuntime;
use std::ffi::{CStr, CString};
use std::ptr::NonNull;

pub(crate) struct EngineHandle {
    ptr: NonNull<ffi::M4AEngine>,
}

impl EngineHandle {
    fn new(sample_rate: f32) -> Result<Self, EngineError> {
        let ptr = unsafe { ffi::m4a_engine_create(sample_rate) };
        let ptr = NonNull::new(ptr).ok_or(EngineError::EngineCreateFailed { sample_rate })?;
        Ok(Self { ptr })
    }

    fn as_ptr(&mut self) -> *mut ffi::M4AEngine {
        self.ptr.as_ptr()
    }
}

impl Drop for EngineHandle {
    fn drop(&mut self) {
        unsafe {
            ffi::m4a_engine_free(self.ptr.as_ptr());
        }
    }
}

pub(crate) struct LoadedVoiceGroupHandle {
    ptr: NonNull<ffi::LoadedVoiceGroup>,
}

impl LoadedVoiceGroupHandle {
    fn load(project_root: &str, bank: &str) -> Result<Self, EngineError> {
        let project_root_c = CString::new(project_root)
            .map_err(|_| EngineError::InvalidCString { field: "project_root" })?;
        let bank_c = CString::new(bank)
            .map_err(|_| EngineError::InvalidCString { field: "bank" })?;
        let ptr = unsafe { ffi::voicegroup_load(project_root_c.as_ptr(), bank_c.as_ptr()) };
        let ptr = NonNull::new(ptr).ok_or_else(|| {
            EngineError::VoicegroupLoadFailed(last_voicegroup_error())
        })?;
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
pub enum EngineError {
    EngineCreateFailed { sample_rate: f32 },
    InvalidCString { field: &'static str },
    VoicegroupLoadFailed(String),
    VoicegroupHasNoVoices,
    ResetFailed,
}

impl std::fmt::Display for EngineError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::EngineCreateFailed { sample_rate } => {
                write!(f, "Failed to create engine at sample rate {}", sample_rate)
            }
            Self::InvalidCString { field } => write!(f, "Field '{}' contains interior NUL byte", field),
            Self::VoicegroupLoadFailed(msg) => write!(f, "Voicegroup load failed: {}", msg),
            Self::VoicegroupHasNoVoices => write!(f, "Loaded voicegroup has no voices"),
            Self::ResetFailed => write!(f, "Engine reset failed"),
        }
    }
}

impl std::error::Error for EngineError {}

#[derive(Clone, Copy, PartialEq)]
pub struct EngineConfig {
    pub sample_rate: f32,
    pub volume: u8,
    pub reverb: u8,
}

pub struct M4aEngine {
    pub(crate) engine: Option<EngineHandle>,
    pub(crate) voicegroup: Option<LoadedVoiceGroupHandle>,
    last_applied_rate: f32,
}

unsafe impl Send for M4aEngine {}

impl M4aEngine {
    pub fn new(config: EngineConfig) -> Result<Self, EngineError> {
        let engine = EngineHandle::new(config.sample_rate)?;
        let mut this = Self {
            engine: Some(engine),
            voicegroup: None,
            last_applied_rate: config.sample_rate,
        };
        this.set_volume(config.volume);
        this.set_reverb_amount(config.reverb);
        Ok(this)
    }

    pub fn load_voicegroup(&mut self, project_root: &str, bank: &str) -> Result<(), EngineError> {
        let mut loaded = LoadedVoiceGroupHandle::load(project_root, bank)?;
        let voices = loaded
            .voices()
            .ok_or(EngineError::VoicegroupHasNoVoices)?;
        self.bind_voicegroup_ptr(Some(voices));
        self.voicegroup = Some(loaded);
        Ok(())
    }

    pub fn clear_voicegroup(&mut self) {
        self.bind_voicegroup_ptr(None);
        self.voicegroup = None;
    }

    pub fn reset(&mut self) -> Result<(), EngineError> {
        let Some(engine) = self.engine.as_mut() else {
            return Err(EngineError::ResetFailed);
        };
        let ok = unsafe { ffi::m4a_engine_reset(engine.as_ptr()) };
        if ok {
            self.rebind_loaded_voicegroup();
            Ok(())
        } else {
            self.retire_after_failed_reset();
            Err(EngineError::ResetFailed)
        }
    }

    pub fn reconfigure(&mut self, config: EngineConfig) -> Result<(), EngineError> {
        if self.engine.is_none() || (config.sample_rate - self.last_applied_rate).abs() > 0.001 {
            self.clear_voicegroup();
            self.engine = None;
            let engine = EngineHandle::new(config.sample_rate)?;
            self.engine = Some(engine);
            self.last_applied_rate = config.sample_rate;
        } else {
            self.reset()?;
        }
        self.set_volume(config.volume);
        self.set_reverb_amount(config.reverb);
        Ok(())
    }

    pub fn is_ready(&self) -> bool {
        self.voicegroup.is_some() && self.engine.is_some()
    }

    pub fn set_volume(&mut self, volume: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_set_volume(engine.as_ptr(), volume) }
        }
    }

    pub fn set_reverb_amount(&mut self, amount: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_set_reverb_amount(engine.as_ptr(), amount) }
        }
    }

    pub fn all_notes_off(&mut self, track: i32) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_all_notes_off(engine.as_ptr(), track) }
        }
    }

    pub fn all_sound_off(&mut self) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_all_sound_off(engine.as_ptr()) }
        }
    }

    fn retire_after_failed_reset(&mut self) {
        self.clear_voicegroup();
        self.engine = None;
    }

    fn rebind_loaded_voicegroup(&mut self) {
        let voices = self
            .voicegroup
            .as_mut()
            .and_then(LoadedVoiceGroupHandle::voices);
        self.bind_voicegroup_ptr(voices);
    }

    fn bind_voicegroup_ptr(&mut self, voices: Option<NonNull<ffi::ToneData>>) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe {
                ffi::m4a_engine_set_voicegroup(
                    engine.as_ptr(),
                    voices.map_or(std::ptr::null_mut(), NonNull::as_ptr),
                );
            }
        }
    }
}

impl ProcessRuntime for M4aEngine {
    fn set_tempo_bpm(&mut self, bpm: f64) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_set_tempo_bpm(engine.as_ptr(), bpm) }
        }
    }

    fn note_on(&mut self, track: i32, key: u8, velocity: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_note_on(engine.as_ptr(), track, key, velocity) }
        }
    }

    fn note_off(&mut self, track: i32, key: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_note_off(engine.as_ptr(), track, key) }
        }
    }

    fn program_change(&mut self, track: i32, program: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_program_change(engine.as_ptr(), track, program) }
        }
    }

    fn cc(&mut self, track: i32, cc: u8, value: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_cc(engine.as_ptr(), track, cc, value) }
        }
    }

    fn pitch_bend(&mut self, track: i32, bend: i16) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_pitch_bend(engine.as_ptr(), track, bend) }
        }
    }

    fn process(&mut self, left: &mut [f32], right: &mut [f32]) {
        debug_assert_eq!(left.len(), right.len());
        let frames = left.len().min(i32::MAX as usize) as i32;
        if let Some(engine) = self.engine.as_mut() {
            unsafe {
                ffi::m4a_engine_process(
                    engine.as_ptr(),
                    left.as_mut_ptr(),
                    right.as_mut_ptr(),
                    frames,
                )
            }
        }
    }
}

impl Drop for M4aEngine {
    fn drop(&mut self) {
        self.clear_voicegroup();
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

/// A legacy wrapper for backwards compatibility until everything is migrated to M4aEngine.
#[allow(dead_code)]
pub(crate) struct CPluginRuntime {
    pub(crate) inner: M4aEngine,
}

#[allow(dead_code)]
impl CPluginRuntime {
    pub(crate) fn new(sample_rate: f32) -> Result<Self, String> {
        let config = EngineConfig {
            sample_rate,
            volume: 127,
            reverb: 0,
        };
        let inner = M4aEngine::new(config).map_err(|e| e.to_string())?;
        Ok(Self { inner })
    }

    pub(crate) fn reset(&mut self) -> bool {
        self.inner.reset().is_ok()
    }

    pub(crate) fn load_voicegroup(&mut self, project_root: &str, bank: &str) -> Result<(), String> {
        self.inner
            .load_voicegroup(project_root, bank)
            .map_err(|e| e.to_string())
    }

    pub(crate) fn clear_voicegroup(&mut self) {
        self.inner.clear_voicegroup();
    }

    pub(crate) fn has_loaded_voicegroup(&self) -> bool {
        self.inner.is_ready()
    }

    pub(crate) fn set_volume(&mut self, volume: u8) {
        self.inner.set_volume(volume);
    }

    pub(crate) fn set_reverb_amount(&mut self, amount: u8) {
        self.inner.set_reverb_amount(amount);
    }

    pub(crate) fn all_notes_off(&mut self, track: i32) {
        self.inner.all_notes_off(track);
    }

    pub(crate) fn all_sound_off(&mut self) {
        self.inner.all_sound_off();
    }

    pub(crate) fn retire_after_failed_reset(&mut self) {
        self.inner.retire_after_failed_reset();
    }
}

impl ProcessRuntime for CPluginRuntime {
    fn set_tempo_bpm(&mut self, bpm: f64) {
        self.inner.set_tempo_bpm(bpm);
    }

    fn note_on(&mut self, track: i32, key: u8, velocity: u8) {
        self.inner.note_on(track, key, velocity);
    }

    fn note_off(&mut self, track: i32, key: u8) {
        self.inner.note_off(track, key);
    }

    fn program_change(&mut self, track: i32, program: u8) {
        self.inner.program_change(track, program);
    }

    fn cc(&mut self, track: i32, cc: u8, value: u8) {
        self.inner.cc(track, cc, value);
    }

    fn pitch_bend(&mut self, track: i32, bend: i16) {
        self.inner.pitch_bend(track, bend);
    }

    fn process(&mut self, left: &mut [f32], right: &mut [f32]) {
        self.inner.process(left, right);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_engine_reconfigure_and_reset() {
        let config = EngineConfig {
            sample_rate: 44100.0,
            volume: 100,
            reverb: 10,
        };
        let mut engine = M4aEngine::new(config).expect("create engine");
        assert!(engine.is_ready() == false);

        // Reset should succeed when engine exists
        assert!(engine.reset().is_ok());

        // Reconfiguring to the same sample rate does not recreate engine
        assert!(engine.reconfigure(config).is_ok());

        // Reconfiguring to a different sample rate works
        let new_config = EngineConfig {
            sample_rate: 48000.0,
            volume: 120,
            reverb: 20,
        };
        assert!(engine.reconfigure(new_config).is_ok());
    }

    #[test]
    fn test_failed_reset_retires_engine() {
        let config = EngineConfig {
            sample_rate: 44100.0,
            volume: 100,
            reverb: 10,
        };
        let mut engine = M4aEngine::new(config).expect("create engine");
        engine.engine = None; // simulate failure or force retirement
        assert!(engine.reset().is_err());
    }
}
