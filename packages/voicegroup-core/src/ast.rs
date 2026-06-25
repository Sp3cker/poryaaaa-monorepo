//! Source-preserving parsed document and diagnostic types shared by parser and analyzer.

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SourcePosition {
    // 1-based source line used by diagnostics and editor adapters.
    pub line: usize,
    // 1-based source column used to point at the exact token in a line.
    pub column: usize,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SourceRange {
    // Inclusive start position for the source construct or diagnostic.
    pub start: SourcePosition,
    // Exclusive end position for the source construct or diagnostic.
    pub end: SourcePosition,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DiagnosticSeverity {
    Error,
    Warning,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Diagnostic {
    // Source span the user-facing diagnostic should underline.
    pub range: SourceRange,
    // Error or warning level before adapters map it to their transport format.
    pub severity: DiagnosticSeverity,
    // Stable machine-readable identifier for tests, tooling, and future adapters.
    pub code: String,
    // Human-readable explanation shown by tools or loader reporting.
    pub message: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedArgument {
    // Trimmed argument text exactly as semantic analysis will validate it.
    pub text: String,
    // Source span for precise argument-level diagnostics.
    pub range: SourceRange,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedIdentifier {
    // Identifier text, such as a voicegroup name, label name, or macro name.
    pub text: String,
    // Source span for diagnostics and tooling features tied to the identifier.
    pub range: SourceRange,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedProgram {
    // Target program slot in the 128-entry voicegroup bank.
    pub slot: usize,
    // Parsed macro name with range so analyzer diagnostics can point at the macro token.
    pub macro_name: ParsedIdentifier,
    // Ordered comma-separated macro arguments preserved for catalog-driven analysis.
    pub arguments: Vec<ParsedArgument>,
    // Optional text after '@', used later as source metadata/display label rather than syntax.
    pub trailing_comment: Option<String>,
    // Full source span of the macro call for line-level diagnostics.
    pub range: SourceRange,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedVoiceGroup {
    // Declared voicegroup bank name that analysis and indexing use as the bank symbol.
    pub name: ParsedIdentifier,
    // First program slot populated by the following macro lines.
    pub start_slot: usize,
    // Source span of the voice_group declaration for declaration-level diagnostics.
    pub declaration_range: SourceRange,
    // Macro programs belonging to this normal voicegroup bank.
    pub programs: Vec<ParsedProgram>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedAssemblyLabel {
    // Assembly label symbol found in symbol-declaring source files.
    pub name: ParsedIdentifier,
    // Source span of the full label declaration, including the trailing '::'.
    pub range: SourceRange,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParsedDocument {
    // Normal voicegroup banks parsed from voice_group declarations.
    pub voice_groups: Vec<ParsedVoiceGroup>,
    // Assembly labels kept separate from normal banks for source indexing.
    pub assembly_labels: Vec<ParsedAssemblyLabel>,
    // Syntax diagnostics produced while parsing the in-memory source text.
    pub diagnostics: Vec<Diagnostic>,
}
