//! Crate-private wording for diagnostics surfaced as Rust error strings.

use crate::ast::Diagnostic;

/// Keeps Rust-facing diagnostic wording consistent outside the structured AST model.
pub(crate) fn diagnostic_message(diagnostic: &Diagnostic) -> String {
    if matches!(
        diagnostic.code.as_str(),
        "missing-voicegroup" | "voicegroup-read-failed"
    ) {
        format!("{}: {}", diagnostic.code, diagnostic.message)
    } else {
        format!(
            "line {}: {}: {}",
            diagnostic.range.start.line, diagnostic.code, diagnostic.message
        )
    }
}
