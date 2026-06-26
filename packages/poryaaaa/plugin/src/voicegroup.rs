use crate::PluginConfig;
use std::io;
use voicegroup_core::parser::Diagnostic;
use voicegroup_core::project_index::ProjectIndex;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VoicegroupProbeResult {
    pub status: VoicegroupProbeStatus,
    pub diagnostics: Vec<Diagnostic>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum VoicegroupProbeStatus {
    NotConfigured,
    Missing,
    Loaded {
        bank_name: String,
        source_relative_path: String,
        occupied_slots: usize,
    },
}

/// Loads voicegroup-core's typed project index and selected bank status for GUI diagnostics.
pub fn probe_voicegroup_bank(config: &PluginConfig) -> io::Result<VoicegroupProbeResult> {
    if config.project_root.is_empty() || config.voicegroup.is_empty() {
        return Ok(VoicegroupProbeResult {
            status: VoicegroupProbeStatus::NotConfigured,
            diagnostics: Vec::new(),
        });
    }

    let index = ProjectIndex::load(&config.project_root)?;
    let load_result = index.load_program_bank(&config.voicegroup);
    let status = match &load_result.bank {
        Some(bank) => VoicegroupProbeStatus::Loaded {
            bank_name: bank.name.clone(),
            source_relative_path: bank.source_relative_path.clone(),
            occupied_slots: bank
                .programs
                .iter()
                .filter(|program| program.is_some())
                .count(),
        },
        None => VoicegroupProbeStatus::Missing,
    };

    Ok(VoicegroupProbeResult {
        status,
        diagnostics: load_result.diagnostics,
    })
}
