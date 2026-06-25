#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SourcePosition {
    pub line: usize,
    pub column: usize,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SourceRange {
    pub start: SourcePosition,
    pub end: SourcePosition,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DiagnosticSeverity {
    Error,
    Warning,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Diagnostic {
    pub range: SourceRange,
    pub severity: DiagnosticSeverity,
    pub code: String,
    pub message: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedArgument {
    pub text: String,
    pub range: SourceRange,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedIdentifier {
    pub text: String,
    pub range: SourceRange,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedProgram {
    pub slot: usize,
    pub macro_name: ParsedIdentifier,
    pub arguments: Vec<ParsedArgument>,
    pub trailing_comment: Option<String>,
    pub range: SourceRange,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedVoiceGroup {
    pub name: ParsedIdentifier,
    pub start_slot: usize,
    pub declaration_range: SourceRange,
    pub programs: Vec<ParsedProgram>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedAssemblyLabel {
    pub name: ParsedIdentifier,
    pub range: SourceRange,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedDocument {
    pub voice_groups: Vec<ParsedVoiceGroup>,
    pub assembly_labels: Vec<ParsedAssemblyLabel>,
    pub diagnostics: Vec<Diagnostic>,
}
