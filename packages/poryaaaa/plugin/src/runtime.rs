use crate::ffi;
use crate::process::ProcessRuntime;
use std::ffi::{CStr, CString};
use std::ptr::NonNull;

pub(crate) struct EngineHandle {
    ptr: NonNull<ffi::M4AEngine>,
}

pub(crate) struct LoadedVoiceGroupHandle {
    ptr: NonNull<ffi::LoadedVoiceGroup>,
}

/// Owns the C engine and any loaded voicegroup whose voice array the engine borrows.
pub(crate) struct CPluginRuntime {
    engine: Option<EngineHandle>,
    voicegroup: Option<LoadedVoiceGroupHandle>,
}

// NicePlug moves the plugin between host threads, but only calls this runtime
// through `&mut self`; the C engine and voicegroup handles are never shared.
unsafe impl Send for CPluginRuntime {}

impl EngineHandle {
    fn new(sample_rate: f32) -> Result<Self, String> {
        let ptr = unsafe { ffi::m4a_engine_create(sample_rate) };
        let ptr = NonNull::new(ptr).ok_or_else(|| "m4a_engine_create failed".to_owned())?;
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

impl LoadedVoiceGroupHandle {
    fn load(project_root: &str, bank: &str) -> Result<Self, String> {
        let project_root = CString::new(project_root)
            .map_err(|_| "project root contains interior NUL".to_owned())?;
        let bank = CString::new(bank).map_err(|_| "voicegroup contains interior NUL".to_owned())?;
        let ptr = unsafe { ffi::voicegroup_load(project_root.as_ptr(), bank.as_ptr()) };
        let ptr = NonNull::new(ptr).ok_or_else(last_voicegroup_error)?;
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

#[allow(dead_code)]
impl CPluginRuntime {
    pub(crate) fn new(sample_rate: f32) -> Result<Self, String> {
        Ok(Self {
            engine: Some(EngineHandle::new(sample_rate)?),
            voicegroup: None,
        })
    }

    pub(crate) fn reset(&mut self) -> bool {
        let Some(engine) = self.engine.as_mut() else {
            return false;
        };
        let ok = unsafe { ffi::m4a_engine_reset(engine.as_ptr()) };
        if ok {
            self.rebind_loaded_voicegroup();
        } else {
            self.retire_after_failed_reset();
        }
        ok
    }

    /// Drops invalid C handles after reset destroys the old engine but cannot recreate it.
    fn retire_after_failed_reset(&mut self) {
        self.engine = None;
        self.voicegroup = None;
    }

    pub(crate) fn load_voicegroup(&mut self, project_root: &str, bank: &str) -> Result<(), String> {
        let mut loaded = LoadedVoiceGroupHandle::load(project_root, bank)?;
        let voices = loaded
            .voices()
            .ok_or_else(|| "loaded voicegroup has no voices pointer".to_owned())?;
        self.bind_voicegroup_ptr(Some(voices));
        self.voicegroup = Some(loaded);
        Ok(())
    }

    pub(crate) fn clear_voicegroup(&mut self) {
        self.bind_voicegroup_ptr(None);
        self.voicegroup = None;
    }

    pub(crate) fn has_loaded_voicegroup(&self) -> bool {
        self.voicegroup.is_some()
    }

    pub(crate) fn set_volume(&mut self, volume: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_set_volume(engine.as_ptr(), volume) }
        }
    }

    pub(crate) fn set_reverb_amount(&mut self, amount: u8) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_set_reverb_amount(engine.as_ptr(), amount) }
        }
    }

    pub(crate) fn all_notes_off(&mut self, track: i32) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_all_notes_off(engine.as_ptr(), track) }
        }
    }

    pub(crate) fn all_sound_off(&mut self) {
        if let Some(engine) = self.engine.as_mut() {
            unsafe { ffi::m4a_engine_all_sound_off(engine.as_ptr()) }
        }
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
impl ProcessRuntime for CPluginRuntime {
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

impl Drop for CPluginRuntime {
    fn drop(&mut self) {
        self.engine = None;
        self.voicegroup = None;
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
    fn failed_reset_retires_runtime_handles() {
        let mut runtime = CPluginRuntime::new(48_000.0).expect("runtime");

        runtime.retire_after_failed_reset();

        assert!(runtime.engine.is_none());
        assert!(!runtime.has_loaded_voicegroup());
        assert!(!runtime.reset());
    }
}
