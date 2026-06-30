use napi::bindgen_prelude::Result;
use napi_derive::napi;
use voicegroup_core::ast::{Diagnostic, DiagnosticSeverity};
use voicegroup_core::project_index::ProjectIndex;
// Structs re-made for NAPI
#[napi(object)]
pub struct VoiceSlot {
    pub name: String,
    pub type_code: u32,
}

#[napi(object)]
pub struct VoicegroupDiagnostic {
    pub severity: String,
    pub code: String,
    pub message: String,
    pub start_line: u32,
    pub start_column: u32,
    pub end_line: u32,
    pub end_column: u32,
}

#[napi(object)]
pub struct VoicegroupParseResult {
    pub ok: bool,
    pub slots: Option<Vec<Option<VoiceSlot>>>,
    pub diagnostics: Vec<VoicegroupDiagnostic>,
    pub source_path: Option<String>,
}

/// Lists banks declared by sound/voice_groups.inc, matching voicegroup-core project indexing.
#[napi]
pub fn scan_voicegroup_banks(root: String) -> Result<Vec<String>> {
    let index =
        ProjectIndex::load(root).map_err(|error| napi::Error::from_reason(error.to_string()))?;
    Ok(index
        .voicegroup_names()
        .map(ToOwned::to_owned)
        .collect::<Vec<_>>())
}

/// Loads the selected bank through voicegroup-core and returns the small Node/M4L slot contract.
#[napi]
pub fn parse_voicegroup(root: String, bank: String) -> Result<VoicegroupParseResult> {
    let index =
        ProjectIndex::load(root).map_err(|error| napi::Error::from_reason(error.to_string()))?;
    let result = index.load_program_bank(&bank);
    let diagnostics = result
        .diagnostics
        .iter()
        .map(convert_diagnostic)
        .collect::<Vec<_>>();

    let Some(bank) = result.bank else {
        return Ok(VoicegroupParseResult {
            ok: false,
            slots: None,
            diagnostics,
            source_path: None,
        });
    };

    let slots = bank
        .programs
        .iter()
        .map(|program| {
            program.as_ref().map(|program| VoiceSlot {
                name: program.display_name.clone(),
                type_code: program.type_code as u32,
            })
        })
        .collect::<Vec<_>>();

    Ok(VoicegroupParseResult {
        ok: diagnostics
            .iter()
            .all(|diagnostic| diagnostic.severity != "error"),
        slots: Some(slots),
        diagnostics,
        source_path: Some(bank.source_relative_path),
    })
}

// Converts core diagnostics into stable plain objects for Node callers.
fn convert_diagnostic(diagnostic: &Diagnostic) -> VoicegroupDiagnostic {
    VoicegroupDiagnostic {
        severity: match diagnostic.severity {
            DiagnosticSeverity::Error => "error".to_string(),
            DiagnosticSeverity::Warning => "warning".to_string(),
        },
        code: diagnostic.code.clone(),
        message: diagnostic.message.clone(),
        start_line: diagnostic.range.start.line as u32,
        start_column: diagnostic.range.start.column as u32,
        end_line: diagnostic.range.end.line as u32,
        end_column: diagnostic.range.end.column as u32,
    }
}
