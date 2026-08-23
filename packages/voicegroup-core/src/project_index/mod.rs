//! Project-level discovery and selected-bank loading for voicegroup source trees.

mod catalog;
mod discovery;
mod loading;
mod parsing;
pub(crate) use loading::voicegroup_continuations;

use crate::analyzer::AnalysisContext;
use crate::ast::{Diagnostic, SourcePosition, SourceRange};
use crate::catalog::{find_macro, ArgumentSchema, SymbolNamespace};
pub use crate::catalog::{
    CatalogEntry, CatalogEntryKind, KeysplitCatalogPair, ProjectCatalog, ProjectSnapshot,
};
use crate::parser::parse_document;
use crate::program_bank::{ProgramBank, ResolvedAsset};
pub use crate::program_bank::{SynthOverlay, VoicegroupSynthOverlay};
use std::collections::{BTreeMap, BTreeSet};
use std::io;
use std::path::PathBuf;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProgramBankLoadResult {
    pub bank: Option<ProgramBank>,
    pub diagnostics: Vec<Diagnostic>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DefinitionLocation {
    pub relative_path: String,
    pub range: SourceRange,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProjectIndex {
    root: PathBuf,
    voice_groups: BTreeMap<String, ProjectVoiceGroup>,
    // Physical source files under sound/voicegroups/. These feed include completion only;
    // they deliberately do not make a voicegroup symbol valid for analysis.
    voicegroup_files: BTreeSet<String>,
    // Assembly macro files are watched independently because definitions can be
    // added without a synth invocation in any indexed sound data file.
    synth_macro_paths: BTreeSet<String>,
    synth_macro_words: BTreeSet<String>,
    direct_sound_assets: BTreeMap<String, ResolvedAsset>,
    programmable_wave_assets: BTreeMap<String, ResolvedAsset>,
    direct_sound_definitions: BTreeMap<String, DefinitionLocation>,
    programmable_wave_definitions: BTreeMap<String, DefinitionLocation>,
    keysplit_tables: BTreeMap<String, [u8; 128]>,
    keysplit_definitions: BTreeMap<String, DefinitionLocation>,
    project_diagnostics: Vec<Diagnostic>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct ProjectVoiceGroup {
    declaration: DefinitionLocation,
    contents: VoiceGroupContents,
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum VoiceGroupContents {
    // Normally named voicegroup contents
    StandaloneFile {
        relative_path: String,
    },
    // pokered style, everything in 1 file.
    LabeledSubsection {
        relative_path: String,
        label: String,
    },
}

impl VoiceGroupContents {
    fn relative_path(&self) -> &str {
        match self {
            Self::StandaloneFile { relative_path }
            | Self::LabeledSubsection { relative_path, .. } => relative_path,
        }
    }
}

impl ProjectIndex {
    pub fn load(root: impl AsRef<std::path::Path>) -> io::Result<Self> {
        discovery::load(root)
    }

    pub fn analysis_context(&self) -> AnalysisContext {
        AnalysisContext::default()
            .with_symbols(
                SymbolNamespace::DirectSound,
                self.direct_sound_assets.keys().cloned(),
            )
            .with_symbols(
                SymbolNamespace::ProgrammableWave,
                self.programmable_wave_assets.keys().cloned(),
            )
            .with_symbols(
                SymbolNamespace::Keysplit,
                self.keysplit_tables.keys().cloned(),
            )
            .with_symbols(
                SymbolNamespace::VoiceGroup,
                self.voice_groups.keys().cloned(),
            )
    }

    /// Iterates voicegroup bank names declared through the project index.
    pub fn voicegroup_names(&self) -> impl Iterator<Item = &str> {
        self.voice_groups.keys().map(String::as_str)
    }

    /// Locates the project-level definition entry for a voicegroup bank name.
    pub fn voicegroup_definition_location(&self, name: &str) -> Option<DefinitionLocation> {
        self.voice_groups
            .get(name)
            .map(|voice_group| voice_group.declaration.clone())
    }

    /// Returns the actual source file containing a bank's contents. This is
    /// separate from `voicegroup_definition_location`, which may point at the
    /// include table that selected an individual file.
    pub fn voicegroup_content_path(&self, name: &str) -> Option<String> {
        self.voice_groups
            .get(name)
            .map(|voice_group| voice_group.contents.relative_path().to_string())
    }

    /// Locates the source header for a keysplit table symbol.
    pub fn keysplit_definition_location(&self, name: &str) -> Option<DefinitionLocation> {
        self.keysplit_definitions.get(name).cloned()
    }

    /// Locates the declaration label for a DirectSound sample symbol.
    pub fn direct_sound_definition_location(&self, name: &str) -> Option<DefinitionLocation> {
        self.direct_sound_definitions.get(name).cloned()
    }

    /// Locates the declaration label for a programmable-wave sample symbol.
    pub fn programmable_wave_definition_location(&self, name: &str) -> Option<DefinitionLocation> {
        self.programmable_wave_definitions.get(name).cloned()
    }

    /// Resolves the project source target under a cursor in an unsaved document.
    pub fn definition_at(
        &self,
        relative_path: &str,
        text: &str,
        position: SourcePosition,
    ) -> Option<DefinitionLocation> {
        if matches!(
            relative_path,
            "sound/voice_groups.inc" | "sound/voicegroups.inc"
        ) {
            if let Some(location) = self.voicegroup_include_target_at(text, &position) {
                return Some(location);
            }
        }

        let document = parse_document(text);
        for voice_group in &document.voice_groups {
            if voice_group.name.range.contains(&position) {
                return self.voicegroup_definition_location(&voice_group.name.text);
            }

            for program in &voice_group.programs {
                let Some(definition) = find_macro(&program.macro_name.text) else {
                    continue;
                };
                for (argument, schema) in program.arguments.iter().zip(definition.arguments.iter())
                {
                    if !argument.range.contains(&position) {
                        continue;
                    }
                    let ArgumentSchema::Symbol { namespace } = schema.schema else {
                        continue;
                    };
                    return match namespace {
                        SymbolNamespace::VoiceGroup => {
                            self.voicegroup_symbol_location(&argument.text)
                        }
                        SymbolNamespace::Keysplit => {
                            self.keysplit_definition_location(&argument.text)
                        }
                        SymbolNamespace::DirectSound => {
                            self.direct_sound_definition_location(&argument.text)
                        }
                        SymbolNamespace::ProgrammableWave => {
                            self.programmable_wave_definition_location(&argument.text)
                        }
                    };
                }
            }
        }

        None
    }

    fn voicegroup_include_target_at(
        &self,
        text: &str,
        position: &SourcePosition,
    ) -> Option<DefinitionLocation> {
        for (line_index, line) in text.lines().enumerate() {
            let source = parsing::strip_comment(line);
            let Some((relative_path, range)) =
                parsing::include_path_with_range(source, line_index + 1)
            else {
                continue;
            };
            if !range.contains(position) || !parsing::is_voicegroup_include_path(&relative_path) {
                continue;
            }
            if !self.root.join(&relative_path).is_file() {
                return None;
            }
            return Some(DefinitionLocation {
                relative_path,
                range: empty_range(),
            });
        }

        None
    }

    fn voicegroup_symbol_location(&self, symbol: &str) -> Option<DefinitionLocation> {
        self.voicegroup_definition_location(symbol).or_else(|| {
            symbol
                .strip_prefix("voicegroup_")
                .and_then(|stripped| self.voicegroup_definition_location(stripped))
        })
    }

    /// Iterates supported Golden Sun synth macro aliases found in assembly
    /// macro definitions, in lexicographic order.
    pub fn synth_macro_words(&self) -> impl Iterator<Item = &str> {
        self.synth_macro_words.iter().map(String::as_str)
    }

    /// Iterates physical voicegroup source files available for include completions.
    pub fn voicegroup_files(&self) -> impl Iterator<Item = &str> {
        self.voicegroup_files.iter().map(String::as_str)
    }

    pub fn direct_sound_assets(&self) -> impl Iterator<Item = &ResolvedAsset> {
        self.direct_sound_assets.values()
    }

    pub fn programmable_wave_assets(&self) -> impl Iterator<Item = &ResolvedAsset> {
        self.programmable_wave_assets.values()
    }

    /// Returns the Rust-owned bulk snapshot consumed by the project adapter.
    /// Invalid banks remain visible in the catalog and contribute structured
    /// diagnostics; callers install no runtime bank when `succeeded` is false.
    pub fn snapshot(&self) -> ProjectSnapshot {
        let mut diagnostics = self.project_diagnostics.clone();
        let catalog = self.build_catalog(&mut diagnostics);
        ProjectSnapshot {
            succeeded: diagnostics.is_empty(),
            catalog,
            diagnostics,
        }
    }

    pub fn catalog(&self) -> ProjectCatalog {
        self.build_catalog(&mut Vec::new())
    }

    fn build_catalog(&self, diagnostics: &mut Vec<Diagnostic>) -> ProjectCatalog {
        catalog::build_catalog(self, diagnostics)
    }

    pub fn keysplit_table(&self, symbol: &str) -> Option<&[u8; 128]> {
        self.keysplit_tables.get(symbol)
    }

    pub fn load_program_bank(&self, bank_name: &str) -> ProgramBankLoadResult {
        loading::load_program_bank(self, bank_name)
    }

    /// Builds a loadable bank from one complete unsaved document. The retained
    /// project index supplies external symbols while local voicegroups shadow
    /// matching on-disk names for this call only.
    pub fn load_program_bank_source(
        &self,
        bank_name: &str,
        project_relative_path: impl Into<String>,
        source_bytes: &str,
        synth_overlay: Option<&SynthOverlay>,
    ) -> ProgramBankLoadResult {
        loading::load_program_bank_source(
            self,
            bank_name,
            project_relative_path,
            source_bytes,
            synth_overlay,
        )
    }
}

fn diagnostic(range: SourceRange, code: &str, message: &str) -> Diagnostic {
    Diagnostic::error(range, code, message)
}

fn annotate_source_path(diagnostics: &mut [Diagnostic], source_path: &str) {
    for diagnostic in diagnostics {
        if diagnostic.source_path.is_none() {
            diagnostic.source_path = Some(source_path.to_string());
        }
    }
}

fn append_unique_diagnostics(diagnostics: &mut Vec<Diagnostic>, new_diagnostics: Vec<Diagnostic>) {
    for diagnostic in new_diagnostics {
        if diagnostics
            .iter()
            .any(|existing| existing.code == diagnostic.code && existing.range == diagnostic.range)
        {
            continue;
        }
        diagnostics.push(diagnostic);
    }
}

fn empty_range() -> SourceRange {
    SourceRange {
        start: SourcePosition { line: 1, column: 1 },
        end: SourcePosition { line: 1, column: 1 },
    }
}
