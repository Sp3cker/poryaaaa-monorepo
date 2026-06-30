use lsp_types::{Diagnostic, DiagnosticSeverity, NumberOrString, Position, Range};
use voicegroup_core::{
    analyzer::{analyze_document, AnalysisContext},
    parser::{parse_document, Diagnostic as CoreDiagnostic, DiagnosticSeverity as CoreSeverity, SourceRange},
};

const DIAGNOSTIC_SOURCE: &str = "voicegroup-core";

/// Parses and analyzes one in-memory document through voicegroup-core, then maps findings to LSP.
pub fn diagnostics_for_text(text: &str) -> Vec<Diagnostic> {
    let document = parse_document(text);
    analyze_document(&document, &AnalysisContext::default())
        .iter()
        .map(to_lsp_diagnostic)
        .collect()
}

/// Converts a transport-agnostic core diagnostic into the LSP diagnostic shape.
pub fn to_lsp_diagnostic(diagnostic: &CoreDiagnostic) -> Diagnostic {
    Diagnostic {
        range: to_lsp_range(&diagnostic.range),
        severity: Some(to_lsp_severity(diagnostic.severity.clone())),
        code: Some(NumberOrString::String(diagnostic.code.clone())),
        code_description: None,
        source: Some(DIAGNOSTIC_SOURCE.to_string()),
        message: diagnostic.message.clone(),
        related_information: None,
        tags: None,
        data: None,
    }
}

/// Converts core's 1-based start-inclusive/end-exclusive range into LSP's 0-based range.
pub fn to_lsp_range(range: &SourceRange) -> Range {
    Range {
        start: to_lsp_position(range.start.line, range.start.column),
        end: to_lsp_position(range.end.line, range.end.column),
    }
}

/// Converts one core source position into an LSP position without underflowing malformed input.
fn to_lsp_position(line: usize, column: usize) -> Position {
    Position {
        line: line.saturating_sub(1) as u32,
        character: column.saturating_sub(1) as u32,
    }
}

/// Preserves core diagnostic severity while using the LSP enum values expected by editors.
fn to_lsp_severity(severity: CoreSeverity) -> DiagnosticSeverity {
    match severity {
        CoreSeverity::Error => DiagnosticSeverity::ERROR,
        CoreSeverity::Warning => DiagnosticSeverity::WARNING,
    }
}

#[cfg(test)]
mod tests {
    use super::{diagnostics_for_text, to_lsp_range};
    use lsp_types::{DiagnosticSeverity as LspDiagnosticSeverity, NumberOrString, Position, Range};
    use voicegroup_core::parser::{Diagnostic, DiagnosticSeverity, SourcePosition, SourceRange};

    #[test]
    fn converts_core_one_based_range_to_lsp_zero_based_range() {
        let core_range = SourceRange {
            start: SourcePosition { line: 3, column: 5 },
            end: SourcePosition { line: 3, column: 12 },
        };

        assert_eq!(
            to_lsp_range(&core_range),
            Range {
                start: Position { line: 2, character: 4 },
                end: Position { line: 2, character: 11 },
            }
        );
    }

    #[test]
    fn diagnostics_for_text_reports_malformed_voicegroup_line_from_core() {
        let diagnostics = diagnostics_for_text("voice_group drums, nope\n");

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].severity, Some(LspDiagnosticSeverity::ERROR));
        assert_eq!(
            diagnostics[0].code,
            Some(NumberOrString::String("invalid-voice-group".to_string()))
        );
        assert_eq!(diagnostics[0].source.as_deref(), Some("voicegroup-core"));
        assert_eq!(
            diagnostics[0].range,
            Range {
                start: Position { line: 0, character: 0 },
                end: Position { line: 0, character: 23 },
            }
        );
    }

    #[test]
    fn diagnostic_mapper_preserves_message_code_and_severity() {
        let diagnostic = Diagnostic {
            range: SourceRange {
                start: SourcePosition { line: 1, column: 1 },
                end: SourcePosition { line: 1, column: 4 },
            },
            severity: DiagnosticSeverity::Warning,
            code: "sample-code".to_string(),
            message: "sample warning".to_string(),
        };

        let mapped = super::to_lsp_diagnostic(&diagnostic);

        assert_eq!(mapped.message, "sample warning");
        assert_eq!(mapped.severity, Some(LspDiagnosticSeverity::WARNING));
        assert_eq!(
            mapped.code,
            Some(NumberOrString::String("sample-code".to_string()))
        );
    }
}
