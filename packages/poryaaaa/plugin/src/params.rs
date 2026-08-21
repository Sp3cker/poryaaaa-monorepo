use crate::midi_activity::MidiActivity;
use nice_plug::prelude::*;
use nice_plug_iced::WindowState;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, RwLock};

pub const PROGRAM_COUNT: usize = 16;
pub const DEFAULT_VOLUME: u8 = 127;
pub const DEFAULT_REVERB: u8 = 0;
pub(crate) const DEFAULT_EDITOR_WIDTH: u32 = 525;
pub(crate) const DEFAULT_EDITOR_HEIGHT: u32 = 420;
pub(crate) const PCM_MIXER_PARAM_ID: &str = "pcm_mixer";

#[derive(Debug, Clone, Copy, PartialEq, Eq, Enum)]
pub enum MixerMode {
    #[id = "ipatix"]
    Ipatix,
    #[id = "sappy"]
    Sappy,
}

// Proc-macro attributes require string literals, so the derive IDs above cannot
// consume constants. This typed table is the runtime mapping; its test compares
// every entry with the IDs emitted by the compiled `Enum` derive.
const MIXER_MODE_STABLE_IDS: [(MixerMode, &str); 2] = [
    (MixerMode::Ipatix, "ipatix"),
    (MixerMode::Sappy, "sappy"),
];

impl MixerMode {
    pub(crate) fn stable_id(self) -> &'static str {
        MIXER_MODE_STABLE_IDS
            .iter()
            .find_map(|&(mode, id)| (mode == self).then_some(id))
            .expect("every mixer mode has a stable ID")
    }

    pub(crate) fn from_stable_id(id: &str) -> Option<Self> {
        MIXER_MODE_STABLE_IDS
            .iter()
            .find_map(|&(mode, stable_id)| (stable_id == id).then_some(mode))
    }

    pub(crate) fn from_state_integer(value: i32) -> Option<Self> {
        match value {
            0 => Some(Self::Ipatix),
            1 => Some(Self::Sappy),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) struct AudioSettings {
    pub volume: u8,
    pub reverb: u8,
    pub mixer_mode: MixerMode,
}

/// Carries the latest voicegroup load result shared between the runtime and editor.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VoicegroupLoadStatus {
    pub text: String,
    pub is_error: bool,
}

#[derive(Params)]
pub struct PoryaaaaParams {
    #[persist = "window-state-v2"]
    pub window_state: Arc<WindowState>,
    #[persist = "project-root"]
    pub project_root: Arc<RwLock<String>>,
    #[persist = "voicegroup"]
    pub voicegroup: Arc<RwLock<String>>,
    pub runtime_voicegroup_status: Arc<RwLock<Option<VoicegroupLoadStatus>>>,
    pub(crate) host_restart_pending: Arc<AtomicBool>,
    pub(crate) midi_activity: Arc<MidiActivity>,
    #[id = "vol"]
    pub volume: IntParam,
    #[id = "rev"]
    pub reverb: IntParam,
    #[id = "pcm_mixer"]
    pub pcm_mixer: EnumParam<MixerMode>,
    #[id = "p00"]
    pub program_00: IntParam,
    #[id = "p01"]
    pub program_01: IntParam,
    #[id = "p02"]
    pub program_02: IntParam,
    #[id = "p03"]
    pub program_03: IntParam,
    #[id = "p04"]
    pub program_04: IntParam,
    #[id = "p05"]
    pub program_05: IntParam,
    #[id = "p06"]
    pub program_06: IntParam,
    #[id = "p07"]
    pub program_07: IntParam,
    #[id = "p08"]
    pub program_08: IntParam,
    #[id = "p09"]
    pub program_09: IntParam,
    #[id = "p10"]
    pub program_10: IntParam,
    #[id = "p11"]
    pub program_11: IntParam,
    #[id = "p12"]
    pub program_12: IntParam,
    #[id = "p13"]
    pub program_13: IntParam,
    #[id = "p14"]
    pub program_14: IntParam,
    #[id = "p15"]
    pub program_15: IntParam,
}

impl Default for PoryaaaaParams {
    fn default() -> Self {
        Self::with_audio_defaults(DEFAULT_VOLUME, DEFAULT_REVERB)
    }
}

impl PoryaaaaParams {
    /// Builds params with config-seeded audio defaults before host state restore can override them.
    pub(crate) fn with_audio_defaults(volume: u8, reverb: u8) -> Self {
        Self::with_audio_defaults_and_mixer(volume, reverb, MixerMode::Ipatix)
    }

    /// Builds params with a configuration-selected mixer before host state restore.
    pub(crate) fn with_audio_defaults_and_mixer(
        volume: u8,
        reverb: u8,
        mixer_mode: MixerMode,
    ) -> Self {
        Self {
            window_state: WindowState::from_logical_size(
                DEFAULT_EDITOR_WIDTH,
                DEFAULT_EDITOR_HEIGHT,
            ),
            project_root: Arc::new(RwLock::new(String::new())),
            voicegroup: Arc::new(RwLock::new(String::new())),
            runtime_voicegroup_status: Arc::new(RwLock::new(None)),
            host_restart_pending: Arc::new(AtomicBool::new(false)),
            midi_activity: Arc::new(MidiActivity::default()),
            volume: audio_control_param("Volume", volume),
            reverb: audio_control_param("Reverb", reverb),
            pcm_mixer: mixer_mode_param(mixer_mode),
            program_00: channel_program_param(0),
            program_01: channel_program_param(1),
            program_02: channel_program_param(2),
            program_03: channel_program_param(3),
            program_04: channel_program_param(4),
            program_05: channel_program_param(5),
            program_06: channel_program_param(6),
            program_07: channel_program_param(7),
            program_08: channel_program_param(8),
            program_09: channel_program_param(9),
            program_10: channel_program_param(10),
            program_11: channel_program_param(11),
            program_12: channel_program_param(12),
            program_13: channel_program_param(13),
            program_14: channel_program_param(14),
            program_15: channel_program_param(15),
        }
    }
    /// Reads the host-facing global audio settings as m4a byte values.
    pub(crate) fn audio_settings(&self) -> AudioSettings {
        AudioSettings {
            volume: self.volume.value().clamp(0, 127) as u8,
            reverb: self.reverb.value().clamp(0, 127) as u8,
            mixer_mode: self.pcm_mixer.value(),
        }
    }

    /// Reads the committed voicegroup selection used for the next runtime initialization.
    pub(crate) fn committed_voicegroup_selection(&self) -> Option<(String, String)> {
        let project_root = self.project_root.read().expect("project root read").clone();
        let bank = self.voicegroup.read().expect("voicegroup read").clone();
        (!project_root.is_empty() && !bank.is_empty()).then_some((project_root, bank))
    }

    /// Commits a voicegroup selection after validation succeeds.
    pub(crate) fn commit_voicegroup_selection(&self, project_root: &str, bank: &str) {
        *self.project_root.write().expect("project root write") = project_root.to_string();
        *self.voicegroup.write().expect("voicegroup write") = bank.to_string();
    }

    /// Mirrors voicegroup load status into editor-visible state.
    pub(crate) fn write_voicegroup_status(&self, status: Option<VoicegroupLoadStatus>) {
        *self
            .runtime_voicegroup_status
            .write()
            .expect("runtime voicegroup status write") = status;
    }

    /// Reads the latest editor-visible voicegroup load status.
    pub(crate) fn voicegroup_status(&self) -> Option<VoicegroupLoadStatus> {
        self.runtime_voicegroup_status
            .read()
            .expect("runtime voicegroup status read")
            .clone()
    }

    /// Requests a host deactivate/reactivate cycle after a successful Load transaction.
    pub(crate) fn request_host_restart(&self) {
        self.host_restart_pending.store(true, Ordering::Release);
    }

    /// Consumes a pending host restart request exactly once.
    pub(crate) fn take_host_restart_request(&self) -> bool {
        self.host_restart_pending.swap(false, Ordering::AcqRel)
    }

    /// Provides indexed access for MIDI channel-oriented process and GUI code.
    pub fn program(&self, channel: usize) -> Option<&IntParam> {
        match channel {
            0 => Some(&self.program_00),
            1 => Some(&self.program_01),
            2 => Some(&self.program_02),
            3 => Some(&self.program_03),
            4 => Some(&self.program_04),
            5 => Some(&self.program_05),
            6 => Some(&self.program_06),
            7 => Some(&self.program_07),
            8 => Some(&self.program_08),
            9 => Some(&self.program_09),
            10 => Some(&self.program_10),
            11 => Some(&self.program_11),
            12 => Some(&self.program_12),
            13 => Some(&self.program_13),
            14 => Some(&self.program_14),
            15 => Some(&self.program_15),
            _ => None,
        }
    }
}

/// Builds the stepped, global mixer selector used by both the host and editor.
fn mixer_mode_param(default: MixerMode) -> EnumParam<MixerMode> {
    EnumParam::new("PCM Mixer", default).non_automatable()
}

/// Builds one automatable global audio-control parameter with m4a's 0..127 range.
fn audio_control_param(name: &'static str, default: u8) -> IntParam {
    IntParam::new(
        name,
        default.min(127) as i32,
        IntRange::Linear { min: 0, max: 127 },
    )
}

/// Builds one automatable channel program parameter with m4a's 0..127 program range.
fn channel_program_param(channel: usize) -> IntParam {
    IntParam::new(
        format!("Channel {} Program", channel + 1),
        channel as i32,
        IntRange::Linear { min: 0, max: 127 },
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn mixer_parameter_uses_stable_ids_and_is_non_automatable() {
        let params = PoryaaaaParams::with_audio_defaults_and_mixer(
            DEFAULT_VOLUME,
            DEFAULT_REVERB,
            MixerMode::Sappy,
        );

        assert_eq!(params.pcm_mixer.value(), MixerMode::Sappy);
        assert_eq!(params.pcm_mixer.to_string(), "Sappy");
        assert!(params
            .pcm_mixer
            .flags()
            .contains(ParamFlags::NON_AUTOMATABLE));
    }

    #[test]
    fn mixer_runtime_mapping_matches_derived_stable_ids() {
        let derived_ids = <MixerMode as Enum>::ids().expect("mixer IDs are derived");
        assert_eq!(derived_ids.len(), MIXER_MODE_STABLE_IDS.len());
        for (index, (mode, stable_id)) in MIXER_MODE_STABLE_IDS.iter().copied().enumerate() {
            assert_eq!(derived_ids[index], stable_id);
            assert_eq!(mode.to_index(), index);
            assert_eq!(mode.stable_id(), stable_id);
            assert_eq!(MixerMode::from_stable_id(stable_id), Some(mode));
        }
    }

    #[test]
    fn fresh_params_seed_ipatix_without_configuration() {
        assert_eq!(PoryaaaaParams::default().pcm_mixer.value(), MixerMode::Ipatix);
    }
}
