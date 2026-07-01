//! Plugin-facing Load transaction for validated voicegroup selection.

use std::path::Path;

use crate::ast::{Diagnostic, DiagnosticSeverity};
use crate::diagnostic_format::diagnostic_message;
use crate::project_index::ProjectIndex;
use crate::projects_json;

/// Validates a draft root/bank and emits shared project state only after success.
pub fn load_for_plugin(
    project_root: &Path,
    bank_name: &str,
    projects_json_path: &Path,
) -> Result<(), String> {
    if !project_root.is_dir() {
        return Err(format!(
            "Bad project root: {} is not a directory",
            project_root.display()
        ));
    }

    let index =
        ProjectIndex::load(project_root).map_err(|error| format!("Bad project root: {error}"))?;

    let load_result = index.load_program_bank(bank_name);
    let bank = load_result
        .bank
        .as_ref()
        .ok_or_else(|| first_error_message(&load_result.diagnostics))?;

    if let Some(diagnostic) = first_error(&load_result.diagnostics) {
        return Err(diagnostic_message(diagnostic));
    }

    projects_json::emit(projects_json_path, project_root, bank_name, &index, bank)
}

fn first_error(diagnostics: &[Diagnostic]) -> Option<&Diagnostic> {
    diagnostics
        .iter()
        .find(|diagnostic| diagnostic.severity == DiagnosticSeverity::Error)
}

fn first_error_message(diagnostics: &[Diagnostic]) -> String {
    diagnostics
        .first()
        .map(diagnostic_message)
        .unwrap_or_else(|| "voicegroup bank could not be loaded".to_string())
}
