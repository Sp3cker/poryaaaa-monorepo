use crate::{PluginConfig, PoryaaaaParams};
use std::io;
use voicegroup_core::parser::{Diagnostic, DiagnosticSeverity};
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

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VoicegroupLoadStatus {
    pub text: String,
    pub is_error: bool,
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

/// Loads the selected voicegroup through voicegroup-core and formats GUI status text.
pub fn load_voicegroup_from_params(params: &PoryaaaaParams) -> VoicegroupLoadStatus {
    let config = PluginConfig {
        project_root: params
            .project_root
            .read()
            .expect("project root read")
            .clone(),
        voicegroup: params.voicegroup.read().expect("voicegroup read").clone(),
        ..Default::default()
    };

    match probe_voicegroup_bank(&config) {
        Ok(result) => load_status_from_probe(result),
        Err(error) => VoicegroupLoadStatus {
            text: format!("Bad project root: {error}"),
            is_error: true,
        },
    }
}

fn load_status_from_probe(result: VoicegroupProbeResult) -> VoicegroupLoadStatus {
    if let Some(diagnostic) = result
        .diagnostics
        .iter()
        .find(|diagnostic| diagnostic.severity == DiagnosticSeverity::Error)
    {
        return VoicegroupLoadStatus {
            text: format_diagnostic(diagnostic),
            is_error: true,
        };
    }

    match result.status {
        VoicegroupProbeStatus::NotConfigured => VoicegroupLoadStatus {
            text: "Enter a project root and voicegroup before loading.".to_string(),
            is_error: true,
        },
        VoicegroupProbeStatus::Missing => {
            let text = result
                .diagnostics
                .first()
                .map(format_diagnostic)
                .unwrap_or_else(|| {
                    "Error missing-voicegroup: voicegroup bank was not found".to_string()
                });
            VoicegroupLoadStatus {
                text,
                is_error: true,
            }
        }
        VoicegroupProbeStatus::Loaded {
            bank_name,
            source_relative_path,
            occupied_slots,
        } => VoicegroupLoadStatus {
            text: format!(
                "Loaded {bank_name} from {source_relative_path} ({occupied_slots} occupied {})",
                if occupied_slots == 1 { "slot" } else { "slots" }
            ),
            is_error: false,
        },
    }
}

fn format_diagnostic(diagnostic: &Diagnostic) -> String {
    let severity = match diagnostic.severity {
        DiagnosticSeverity::Error => "Error",
        DiagnosticSeverity::Warning => "Warning",
    };
    format!("{severity} {}: {}", diagnostic.code, diagnostic.message)
}
