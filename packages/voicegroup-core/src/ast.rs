//! Source-preserving parsed document and diagnostic types shared by parser and analyzer.

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SourcePosition {
    // 1-based source line used by diagnostics and editor adapters.
    pub line: usize,
    // 1-based source column used to point at the exact token in a line.
    pub column: usize,
}

// Rust core ranges are 1-based, start-inclusive, and end-exclusive. The old
// Swift LSP SourceModel used zero-based editor positions and treated the end
// character as included; Rust keeps the parser/analyzer contract explicit and
// leaves LSP coordinate conversion to the adapter layer.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SourceRange {
    // Inclusive start position for the source construct or diagnostic.
    pub start: SourcePosition,
    // Exclusive end position for the source construct or diagnostic.
    pub end: SourcePosition,
}

impl SourceRange {
    /// Returns whether `position` falls within the range's start-inclusive,
    /// end-exclusive source span.
    pub fn contains(&self, position: &SourcePosition) -> bool {
        if position.line < self.start.line || position.line > self.end.line {
            return false;
        }
        if position.line == self.start.line && position.column < self.start.column {
            return false;
        }
        if position.line == self.end.line && position.column >= self.end.column {
            return false;
        }
        true
    }
}
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DiagnosticSeverity {
    Error,
    Warning,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DiagnosticScope {
    /// A parser or project-index error that invalidates the source as a whole.
    Structural,
    /// An error associated with one voicegroup slot.
    Slot,
    /// A runtime/materialization error associated with an external asset.
    Materialization,
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
    // Layer that owns the failure, matching the public project seam.
    pub scope: DiagnosticScope,
    // Actual voicegroup content file, or the authoritative preview path.
    pub source_path: Option<String>,
    // Referenced asset path when materialization reports an external failure.
    pub asset_path: Option<String>,
    // Voicegroup slot for slot/materialization diagnostics.
    pub slot: Option<usize>,
}

impl Diagnostic {
    pub fn error(range: SourceRange, code: &str, message: &str) -> Self {
        Self {
            range,
            severity: DiagnosticSeverity::Error,
            code: code.to_string(),
            message: message.to_string(),
            scope: DiagnosticScope::Structural,
            source_path: None,
            asset_path: None,
            slot: None,
        }
    }

    pub fn with_source_path(mut self, source_path: impl Into<String>) -> Self {
        self.source_path = Some(source_path.into());
        self
    }
    pub fn with_asset_path(mut self, asset_path: impl Into<String>) -> Self {
        self.asset_path = Some(asset_path.into());
        self.scope = DiagnosticScope::Materialization;
        self
    }

    pub fn with_slot(mut self, slot: usize) -> Self {
        self.scope = DiagnosticScope::Slot;
        self.slot = Some(slot);
        self
    }
    pub fn set_slot(&mut self, slot: usize) {
        self.scope = DiagnosticScope::Slot;
        self.slot = Some(slot);
    }
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
    // Source span of the full label declaration, including its trailing colon(s).
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
