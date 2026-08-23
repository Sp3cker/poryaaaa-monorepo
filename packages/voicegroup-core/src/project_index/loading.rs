use std::{fs, io};

use crate::analyzer::analyze_document;
use crate::ast::{SourcePosition, SourceRange};
use crate::catalog::SymbolNamespace;
use crate::parser::parse_document;
use crate::program_bank::{build_program_bank, ProgramBankBuildResult, ProgramBankContext};

use super::parsing;
use super::{
    annotate_source_path, append_unique_diagnostics, diagnostic, empty_range,
    ProgramBankLoadResult, ProjectIndex, SynthOverlay, VoiceGroupContents,
};

// Keeps sparse parsed snippets tied to their source file lines for diagnostics.
struct BankSourceText {
    relative_path: String,
    text: String,
    line_map: Vec<usize>, // 1-based parsed line -> 1-based original file line
}

impl BankSourceText {
    // Rewrites parser/analyzer ranges back onto the original source file.
    fn remap_range(&self, range: &SourceRange) -> SourceRange {
        SourceRange {
            start: SourcePosition {
                line: self.file_line(range.start.line),
                column: range.start.column,
            },
            end: SourcePosition {
                line: self.file_line(range.end.line),
                column: range.end.column,
            },
        }
    }

    fn file_line(&self, parsed_line: usize) -> usize {
        self.line_map
            .get(parsed_line.saturating_sub(1))
            .copied()
            .unwrap_or(parsed_line)
    }
}

pub(super) fn load_program_bank(index: &ProjectIndex, bank_name: &str) -> ProgramBankLoadResult {
    let Some(voice_group) = index.voice_groups.get(bank_name) else {
        return ProgramBankLoadResult {
            bank: None,
            diagnostics: vec![diagnostic(
                empty_range(),
                "missing-voicegroup",
                "voicegroup bank is not declared in the project index",
            )],
        };
    };

    let source_path = voice_group.contents.relative_path().to_string();
    let Ok(source_text) = read_bank_source(index, &voice_group.contents) else {
        return ProgramBankLoadResult {
            bank: None,
            diagnostics: vec![diagnostic(
                empty_range(),
                "voicegroup-read-failed",
                "voicegroup source file could not be read",
            )
            .with_source_path(source_path)],
        };
    };

    let document = parse_document(&source_text.text);
    let mut diagnostics = analyze_document(&document, &index.analysis_context());
    let Some(voice_group) = document
        .voice_groups
        .iter()
        .find(|voice_group| voice_group.name.text == bank_name)
    else {
        diagnostics.push(
            diagnostic(
                empty_range(),
                "missing-voicegroup",
                "voicegroup bank is not declared in the selected source",
            )
            .with_source_path(source_text.relative_path.clone()),
        );
        annotate_diagnostics(&mut diagnostics, &source_text);
        return ProgramBankLoadResult {
            bank: None,
            diagnostics,
        };
    };

    let ProgramBankBuildResult {
        mut bank,
        diagnostics: build_diagnostics,
    } = build_program_bank(
        voice_group,
        source_text.relative_path.clone(),
        &program_bank_context(index),
    );
    append_unique_diagnostics(&mut diagnostics, build_diagnostics);
    annotate_diagnostics(&mut diagnostics, &source_text);

    for program in &mut bank.programs {
        if let Some(record) = program {
            record.source_range = source_text.remap_range(&record.source_range);
        }
    }

    ProgramBankLoadResult {
        bank: Some(bank),
        diagnostics,
    }
}

pub(super) fn load_program_bank_source(
    index: &ProjectIndex,
    bank_name: &str,
    project_relative_path: impl Into<String>,
    source_bytes: &str,
    synth_overlay: Option<&SynthOverlay>,
) -> ProgramBankLoadResult {
    let source_relative_path = project_relative_path.into();
    let document = parse_document(source_bytes);
    let mut analysis_context = index.analysis_context();
    if let Some(overlay) = synth_overlay {
        analysis_context = analysis_context.with_symbols(
            SymbolNamespace::DirectSound,
            overlay.iter().map(|(symbol, _)| symbol.to_string()),
        );
    }
    let mut diagnostics = analyze_document(&document, &analysis_context);
    let Some(voice_group) = document
        .voice_groups
        .iter()
        .find(|voice_group| voice_group.name.text == bank_name)
    else {
        diagnostics.push(
            diagnostic(
                empty_range(),
                "missing-voicegroup",
                "voicegroup bank is not declared in the supplied source",
            )
            .with_source_path(source_relative_path.clone()),
        );
        annotate_source_path(&mut diagnostics, &source_relative_path);
        return ProgramBankLoadResult {
            bank: None,
            diagnostics,
        };
    };

    let mut context = program_bank_context(index);
    context = context.with_voice_groups(
        document
            .voice_groups
            .iter()
            .map(|voice_group| voice_group.name.text.clone()),
    );
    if let Some(overlay) = synth_overlay {
        context = context.with_synth_overlay(overlay);
    }
    let ProgramBankBuildResult {
        bank,
        diagnostics: build_diagnostics,
    } = build_program_bank(voice_group, source_relative_path.clone(), &context);
    append_unique_diagnostics(&mut diagnostics, build_diagnostics);
    annotate_source_path(&mut diagnostics, &source_relative_path);
    ProgramBankLoadResult {
        bank: Some(bank),
        diagnostics,
    }
}

fn annotate_diagnostics(diagnostics: &mut [crate::ast::Diagnostic], source_text: &BankSourceText) {
    for diagnostic in diagnostics {
        diagnostic.range = source_text.remap_range(&diagnostic.range);
        if diagnostic.source_path.is_none() {
            diagnostic.source_path = Some(source_text.relative_path.clone());
        }
    }
}

fn read_bank_source(
    index: &ProjectIndex,
    contents: &VoiceGroupContents,
) -> io::Result<BankSourceText> {
    match contents {
        VoiceGroupContents::StandaloneFile { relative_path } => {
            let text = std::fs::read_to_string(index.root.join(relative_path))?;
            let line_map = (1..=text.lines().count()).collect();
            Ok(BankSourceText {
                relative_path: relative_path.clone(),
                text,
                line_map,
            })
        }
        VoiceGroupContents::LabeledSubsection {
            relative_path,
            label,
        } => {
            let text = std::fs::read_to_string(index.root.join(relative_path))?;
            let (section, line_map) = parsing::extract_combined_section_with_map(&text, label)
                .ok_or_else(|| {
                    io::Error::new(
                        io::ErrorKind::InvalidData,
                        "labeled voicegroup subsection is no longer present",
                    )
                })?;
            Ok(BankSourceText {
                relative_path: relative_path.clone(),
                text: section,
                line_map,
            })
        }
    }
}

fn program_bank_context(index: &ProjectIndex) -> ProgramBankContext {
    let mut context = ProgramBankContext::default();
    for asset in index.direct_sound_assets.values() {
        context = context.with_direct_sound_asset(asset.clone());
    }
    for asset in index.programmable_wave_assets.values() {
        context = context.with_programmable_wave_asset(asset.clone());
    }
    for (symbol, table) in &index.keysplit_tables {
        context = context.with_keysplit_table(symbol.clone(), *table);
    }
    for symbol in index.voice_groups.keys() {
        context = context.with_voice_group(symbol.clone());
    }
    context
}
/// One source-ordered successor used by the C materializer's ROM-contiguity
/// compatibility path. Empty `bank_name` marks an indexed include whose
/// contents could not be resolved into a declared bank.
#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) struct VoicegroupContinuation {
    pub bank_name: String,
    pub source_path: String,
}

/// Returns the included voicegroup files after `bank_name` in the actual
/// include table order. This is deliberately an index-owned seam: callers do
/// not rescan project directories to infer ROM adjacency.
pub(crate) fn voicegroup_continuations(
    index: &ProjectIndex,
    bank_name: &str,
) -> Vec<VoicegroupContinuation> {
    let Some(current_path) = index.voicegroup_content_path(bank_name) else {
        return Vec::new();
    };
    if let Some(continuations) = monolithic_continuations(index, bank_name, &current_path) {
        return continuations;
    }
    let Some(include_paths) = ordered_include_paths(index, &current_path) else {
        return Vec::new();
    };

    let Some(current_index) = include_paths.iter().position(|path| path == &current_path) else {
        return Vec::new();
    };

    let mut continuations = Vec::new();
    for source_path in include_paths.into_iter().skip(current_index + 1) {
        let path = index.root.join(&source_path);
        let Ok(source) = fs::read_to_string(path) else {
            continuations.push(VoicegroupContinuation {
                bank_name: String::new(),
                source_path,
            });
            break;
        };

        // The legacy noSubRecurse continuation stops before a comma-form
        // declaration. Label-only files are the layouts whose macros were
        // assembled directly into the following ROM region.
        if source
            .lines()
            .map(parsing::strip_comment)
            .any(|line| line.trim_start().starts_with("voice_group "))
        {
            break;
        }

        let document = parse_document(&source);
        let mut resolved = false;
        for voice_group in document.voice_groups {
            let name = voice_group.name.text;
            if index.voicegroup_content_path(&name).as_deref() != Some(source_path.as_str()) {
                continue;
            }
            continuations.push(VoicegroupContinuation {
                bank_name: name,
                source_path: source_path.clone(),
            });
            resolved = true;
        }
        if !resolved {
            continuations.push(VoicegroupContinuation {
                bank_name: String::new(),
                source_path,
            });
            break;
        }
    }

    continuations
}
fn monolithic_continuations(
    index: &ProjectIndex,
    bank_name: &str,
    current_path: &str,
) -> Option<Vec<VoicegroupContinuation>> {
    if current_path != "sound/voice_groups.inc" && current_path != "sound/voicegroups.inc" {
        return None;
    }
    let Ok(source) = fs::read_to_string(index.root.join(current_path)) else {
        return Some(Vec::new());
    };
    let document = parse_document(&source);
    let Some(current_index) = document
        .voice_groups
        .iter()
        .position(|voice_group| voice_group.name.text == bank_name)
    else {
        return Some(Vec::new());
    };

    let lines = source.lines().collect::<Vec<_>>();
    let mut continuations = Vec::new();
    for voice_group in document.voice_groups.into_iter().skip(current_index + 1) {
        let line = lines
            .get(voice_group.declaration_range.start.line.saturating_sub(1))
            .copied()
            .unwrap_or_default();
        if parsing::strip_comment(line)
            .trim_start()
            .starts_with("voice_group ")
        {
            break;
        }
        let name = voice_group.name.text;
        if index.voicegroup_content_path(&name).as_deref() != Some(current_path) {
            continuations.push(VoicegroupContinuation {
                bank_name: String::new(),
                source_path: current_path.to_string(),
            });
            break;
        }
        continuations.push(VoicegroupContinuation {
            bank_name: name,
            source_path: current_path.to_string(),
        });
    }
    Some(continuations)
}

fn ordered_include_paths(index: &ProjectIndex, current_path: &str) -> Option<Vec<String>> {
    for include_table in ["sound/voice_groups.inc", "sound/voicegroups.inc"] {
        let Ok(source) = fs::read_to_string(index.root.join(include_table)) else {
            continue;
        };
        let paths = source
            .lines()
            .filter_map(|line| parsing::include_path_with_range(parsing::strip_comment(line), 1))
            .map(|(path, _)| path)
            .filter(|path| parsing::is_voicegroup_include_path(path))
            .collect::<Vec<_>>();
        if paths.iter().any(|path| path == current_path) {
            return Some(paths);
        }
    }
    None
}
