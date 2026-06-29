use crate::{midi_activity::MidiActivity, voicegroup::VoicegroupLoadStatus};
use nice_plug::prelude::*;
use nice_plug_egui::EguiState;
use std::sync::{Arc, RwLock};

pub const PROGRAM_COUNT: usize = 16;
pub(crate) const DEFAULT_EDITOR_WIDTH: u32 = 525;
pub(crate) const DEFAULT_EDITOR_HEIGHT: u32 = 325;

#[derive(Params)]
pub struct PoryaaaaParams {
    #[persist = "editor-state"]
    pub editor_state: Arc<EguiState>,
    #[persist = "project-root"]
    pub project_root: Arc<RwLock<String>>,
    #[persist = "voicegroup"]
    pub voicegroup: Arc<RwLock<String>>,
    pub runtime_voicegroup_status: Arc<RwLock<Option<VoicegroupLoadStatus>>>,
    pub(crate) midi_activity: Arc<MidiActivity>,
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
        Self {
            editor_state: EguiState::from_size(DEFAULT_EDITOR_WIDTH, DEFAULT_EDITOR_HEIGHT),
            project_root: Arc::new(RwLock::new(String::new())),
            voicegroup: Arc::new(RwLock::new(String::new())),
            runtime_voicegroup_status: Arc::new(RwLock::new(None)),
            midi_activity: Arc::new(MidiActivity::default()),
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
}

impl PoryaaaaParams {
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

/// Builds one automatable channel program parameter with m4a's 0..127 program range.
fn channel_program_param(channel: usize) -> IntParam {
    IntParam::new(
        format!("Channel {} Program", channel + 1),
        channel as i32,
        IntRange::Linear { min: 0, max: 127 },
    )
}
