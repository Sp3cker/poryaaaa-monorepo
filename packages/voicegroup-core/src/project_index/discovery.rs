use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::io;

use crate::ast::SourceRange;
use crate::catalog::{is_synth_macro_word, synth_descriptor};
use crate::parser::parse_document;
use crate::program_bank::ResolvedAsset;

use super::parsing;
use super::{DefinitionLocation, ProjectIndex, ProjectVoiceGroup, VoiceGroupContents};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum AssetKind {
    DirectSound,
    ProgrammableWave,
}

pub(super) fn load(root: impl AsRef<std::path::Path>) -> io::Result<ProjectIndex> {
    let root = root.as_ref().to_path_buf();
    let mut index = ProjectIndex {
        root,
        voice_groups: BTreeMap::new(),
        voicegroup_files: BTreeSet::new(),
        synth_macro_paths: BTreeSet::new(),
        synth_macro_words: BTreeSet::new(),
        direct_sound_assets: BTreeMap::new(),
        programmable_wave_assets: BTreeMap::new(),
        direct_sound_definitions: BTreeMap::new(),
        programmable_wave_definitions: BTreeMap::new(),
        keysplit_tables: BTreeMap::new(),
        keysplit_definitions: BTreeMap::new(),
        project_diagnostics: Vec::new(),
    };

    // Discover files before declarations so editor features can see un-included
    // source files without changing what counts as a declared voicegroup.
    discover_voicegroup_files(&mut index)?;
    discover_synth_macro_definitions(&mut index)?;
    discover_monolithic_voicegroups(&mut index)?;
    discover_included_voicegroup_files(&mut index, "sound/voice_groups.inc")?;
    discover_included_voicegroup_files(&mut index, "sound/voicegroups.inc")?;
    discover_unsupported_layouts(&mut index)?;
    index_standard_symbol_files(&mut index)?;
    index_keysplit_tables(&mut index)?;
    Ok(index)
}

/// Collects supported `.macro` definitions from the Golden Sun macro directory.
///
/// This deliberately scans definitions rather than synth data invocations so a
/// project can advertise a creatable alias before any sound uses it.
fn discover_synth_macro_definitions(index: &mut ProjectIndex) -> io::Result<()> {
    let relative_dir = "asm/macros";
    let dir = index.root.join(relative_dir);
    if !dir.is_dir() {
        return Ok(());
    }

    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let file_type = entry.file_type()?;
        let file_name = match entry.file_name().into_string() {
            Ok(file_name) => file_name,
            Err(_) => continue,
        };
        if !file_type.is_file() || !file_name.ends_with(".inc") {
            continue;
        }
        let relative_path = format!("{relative_dir}/{file_name}");
        index.synth_macro_paths.insert(relative_path.clone());
        let text = fs::read_to_string(index.root.join(&relative_path))?;
        for line in text.lines() {
            let source = parsing::strip_comment(line).trim();
            let Some(rest) = source.strip_prefix(".macro") else {
                continue;
            };
            if !rest
                .chars()
                .next()
                .is_some_and(|character| character.is_ascii_whitespace())
            {
                continue;
            }
            let Some(name) = rest.split_whitespace().next() else {
                continue;
            };
            if is_synth_macro_word(name) {
                index.synth_macro_words.insert(name.to_string());
            }
        }
    }

    Ok(())
}

/// Records heuristic legacy layouts that this index deliberately does not ingest.
///
/// Explicit include-table paths remain supported; only otherwise-unindexed
/// fallback-probe candidates receive a project-level diagnostic.
fn discover_unsupported_layouts(index: &mut ProjectIndex) -> io::Result<()> {
    discover_unsupported_voicegroup_files(index, "sound/voicegroups")?;
    discover_unsupported_sound_tree(index, "sound", 3)?;
    Ok(())
}

fn discover_unsupported_voicegroup_files(
    index: &mut ProjectIndex,
    relative_dir: &str,
) -> io::Result<()> {
    let dir = index.root.join(relative_dir);
    if !dir.is_dir() {
        return Ok(());
    }

    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let file_name = match entry.file_name().into_string() {
            Ok(file_name) => file_name,
            Err(_) => continue,
        };
        let relative_path = format!("{relative_dir}/{file_name}");
        let file_type = entry.file_type()?;
        if file_type.is_dir() {
            discover_unsupported_voicegroup_files(index, &relative_path)?;
            continue;
        }
        if !file_type.is_file() {
            continue;
        }

        let is_suffix_layout =
            file_name.ends_with("_keysplit.inc") || file_name.ends_with("_drumset.inc");
        let is_eventide_layout = file_name.starts_with("vg_") && file_name.ends_with(".inc");
        let is_assembly_voicegroup = file_name.ends_with(".s");
        if !is_suffix_layout && !is_eventide_layout && !is_assembly_voicegroup {
            continue;
        }
        let path = index.root.join(&relative_path);
        let text = fs::read_to_string(&path)?;
        if is_suffix_layout {
            if contains_voicegroup_source(&text) {
                record_unsupported_layout(
                    index,
                    &relative_path,
                    "unsupported-voicegroup-suffix",
                    "voicegroup _keysplit/_drumset filename probing is not supported",
                    &text,
                );
            }
        } else if is_eventide_layout {
            if contains_voicegroup_source(&text) {
                record_unsupported_layout(
                    index,
                    &relative_path,
                    "unsupported-vg-prefix",
                    "vg_ eventide voicegroup filename probing is not supported",
                    &text,
                );
            }
        } else if is_assembly_voicegroup && contains_voicegroup_source(&text) {
            record_unsupported_layout(
                index,
                &relative_path,
                "unsupported-voicegroup-s",
                "assembly .s voicegroup files are not supported",
                &text,
            );
        }
    }

    Ok(())
}

fn discover_unsupported_sound_tree(
    index: &mut ProjectIndex,
    relative_dir: &str,
    max_depth: usize,
) -> io::Result<()> {
    let dir = index.root.join(relative_dir);
    if !dir.is_dir() {
        return Ok(());
    }

    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let file_name = match entry.file_name().into_string() {
            Ok(file_name) => file_name,
            Err(_) => continue,
        };
        let relative_path = format!("{relative_dir}/{file_name}");
        let file_type = entry.file_type()?;
        if file_type.is_dir() {
            if relative_dir == "sound/voicegroups" {
                continue;
            }
            let depth = relative_path
                .strip_prefix("sound/")
                .unwrap_or(&relative_path)
                .split('/')
                .count();
            if depth <= max_depth {
                discover_unsupported_sound_tree(index, &relative_path, max_depth)?;
            }
            continue;
        }
        if !file_type.is_file() || relative_dir == "sound/voicegroups" {
            continue;
        }

        let depth = relative_path
            .strip_prefix("sound/")
            .unwrap_or(&relative_path)
            .split('/')
            .count()
            .saturating_sub(1);
        if depth == 0 || depth > max_depth || !is_voicegroup_source_path(&relative_path) {
            continue;
        }
        let path = index.root.join(&relative_path);
        let text = fs::read_to_string(path)?;
        if contains_voicegroup_source(&text) {
            record_unsupported_layout(
                index,
                &relative_path,
                "unsupported-sound-depth",
                "voicegroup discovery outside sound/voicegroups is not supported",
                &text,
            );
        }
    }

    Ok(())
}

fn is_voicegroup_source_path(relative_path: &str) -> bool {
    relative_path.ends_with(".inc") || relative_path.ends_with(".s")
}

fn contains_voicegroup_source(text: &str) -> bool {
    let document = parse_document(text);
    !document.voice_groups.is_empty()
        || document
            .assembly_labels
            .iter()
            .any(|label| parsing::combined_section_has_voice_macro(text, &label.name.text))
}

fn record_unsupported_layout(
    index: &mut ProjectIndex,
    relative_path: &str,
    code: &str,
    message: &str,
    text: &str,
) {
    if index
        .voice_groups
        .values()
        .any(|voice_group| voice_group.contents.relative_path() == relative_path)
    {
        return;
    }
    if index.project_diagnostics.iter().any(|diagnostic| {
        diagnostic.code == code && diagnostic.source_path.as_deref() == Some(relative_path)
    }) {
        return;
    }
    index.project_diagnostics.push(
        crate::ast::Diagnostic::error(full_source_range(text), code, message)
            .with_source_path(relative_path),
    );
}

fn full_source_range(text: &str) -> SourceRange {
    let lines = text.split('\n').collect::<Vec<_>>();
    let end_line = lines.len();
    let end_column = lines.last().map_or(1, |line| line.len() + 1);
    SourceRange {
        start: crate::ast::SourcePosition { line: 1, column: 1 },
        end: crate::ast::SourcePosition {
            line: end_line,
            column: end_column,
        },
    }
}

/// Finds physical `.inc` files under sound/voicegroups without declaring them as banks.
fn discover_voicegroup_files(index: &mut ProjectIndex) -> io::Result<()> {
    discover_voicegroup_files_in(index, "sound/voicegroups")
}

/// Recurses over a project-relative directory and records voicegroup source file paths.
fn discover_voicegroup_files_in(index: &mut ProjectIndex, relative_dir: &str) -> io::Result<()> {
    let dir = index.root.join(relative_dir);
    if !dir.is_dir() {
        return Ok(());
    }

    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let file_name = match entry.file_name().into_string() {
            Ok(file_name) => file_name,
            Err(_) => continue,
        };
        let relative_path = format!("{relative_dir}/{file_name}");
        let file_type = entry.file_type()?;
        // Keep this as file inventory only. Declared banks are still discovered
        // through sound/voice_groups.inc below.
        if file_type.is_dir() {
            discover_voicegroup_files_in(index, &relative_path)?;
        } else if file_type.is_file() && parsing::is_voicegroup_include_path(&relative_path) {
            index.voicegroup_files.insert(relative_path);
        }
    }

    Ok(())
}

fn discover_monolithic_voicegroups(index: &mut ProjectIndex) -> io::Result<()> {
    discover_combined_voicegroup_file(index, "sound/voice_groups.inc")?;
    discover_combined_voicegroup_file(index, "sound/voicegroups.inc")
}

fn discover_combined_voicegroup_file(
    index: &mut ProjectIndex,
    relative_path: &str,
) -> io::Result<()> {
    let path = index.root.join(relative_path);
    if !path.is_file() {
        return Ok(());
    }

    let text = fs::read_to_string(path)?;
    let document = parse_document(&text);
    let voice_group_names = document
        .voice_groups
        .iter()
        .map(|voice_group| voice_group.name.text.as_str())
        .collect::<BTreeSet<_>>();
    for label in document.assembly_labels {
        if !voice_group_names.contains(label.name.text.as_str()) {
            continue;
        }
        let definition = DefinitionLocation {
            relative_path: relative_path.to_string(),
            range: label.name.range.clone(),
        };
        index
            .voice_groups
            .entry(label.name.text.clone())
            .or_insert_with(|| ProjectVoiceGroup {
                declaration: definition,
                contents: VoiceGroupContents::LabeledSubsection {
                    relative_path: relative_path.to_string(),
                    label: label.name.text,
                },
            });
    }

    Ok(())
}

fn discover_included_voicegroup_files(
    index: &mut ProjectIndex,
    include_table_relative_path: &str,
) -> io::Result<()> {
    let path = index.root.join(include_table_relative_path);
    if !path.is_file() {
        return Ok(());
    }

    let text = fs::read_to_string(path)?;
    for (line_index, line) in text.lines().enumerate() {
        let stripped = parsing::strip_comment(line);
        let Some((relative_path, range)) =
            parsing::include_path_with_range(stripped, line_index + 1)
        else {
            continue;
        };
        if !parsing::is_voicegroup_include_path(&relative_path) {
            continue;
        }
        let definition = DefinitionLocation {
            relative_path: include_table_relative_path.to_string(),
            range,
        };
        discover_voicegroup_file(index, &relative_path, definition)?;
    }

    Ok(())
}

fn discover_voicegroup_file(
    index: &mut ProjectIndex,
    relative_path: &str,
    definition: DefinitionLocation,
) -> io::Result<()> {
    let path = index.root.join(relative_path);
    if !path.is_file() {
        return Ok(());
    }

    let text = fs::read_to_string(path)?;
    for voice_group in parse_document(&text).voice_groups {
        index
            .voice_groups
            .entry(voice_group.name.text)
            .or_insert_with(|| ProjectVoiceGroup {
                declaration: definition.clone(),
                contents: VoiceGroupContents::StandaloneFile {
                    relative_path: relative_path.to_string(),
                },
            });
    }

    Ok(())
}

fn index_standard_symbol_files(index: &mut ProjectIndex) -> io::Result<()> {
    index_symbol_file(index, "sound/direct_sound_data.inc", AssetKind::DirectSound)?;
    index_symbol_file(
        index,
        "sound/direct_sound_synth_data.inc",
        AssetKind::DirectSound,
    )?;
    index_symbol_file(
        index,
        "sound/programmable_wave_data.inc",
        AssetKind::ProgrammableWave,
    )
}

fn index_symbol_file(
    index: &mut ProjectIndex,
    relative_path: &str,
    kind: AssetKind,
) -> io::Result<()> {
    let path = index.root.join(relative_path);
    if !path.is_file() {
        return Ok(());
    }

    let text = fs::read_to_string(path)?;
    let mut current_symbol = None;
    for (line_index, line) in text.lines().enumerate() {
        let stripped = parsing::strip_comment(line).trim();
        let line_document = parse_document(stripped);
        if let Some(label) = line_document.assembly_labels.first() {
            let symbol = label.name.text.clone();
            let mut range = label.name.range.clone();
            range.start.line = line_index + 1;
            range.end.line = line_index + 1;
            insert_definition(index, kind, symbol.clone(), relative_path, range);
            current_symbol = Some(symbol);
            continue;
        }

        if let Some(asset_path) = parsing::incbin_path(stripped) {
            if let Some(symbol) = current_symbol.take() {
                insert_asset(index, kind, symbol, asset_path);
            }
        } else if let Some(descriptor) = synth_descriptor(stripped) {
            if let Some(symbol) = current_symbol.take() {
                insert_synth(index, kind, symbol, descriptor);
            }
        }
    }

    Ok(())
}

fn insert_asset(index: &mut ProjectIndex, kind: AssetKind, symbol: String, relative_path: String) {
    let asset = ResolvedAsset {
        symbol: symbol.clone(),
        display_name: path_basename(&relative_path),
        relative_path,
        synth_desc: None,
    };
    match kind {
        AssetKind::DirectSound => {
            index.direct_sound_assets.entry(symbol).or_insert(asset);
        }
        AssetKind::ProgrammableWave => {
            index
                .programmable_wave_assets
                .entry(symbol)
                .or_insert(asset);
        }
    }
}

fn insert_synth(index: &mut ProjectIndex, kind: AssetKind, symbol: String, descriptor: [u8; 6]) {
    if kind != AssetKind::DirectSound {
        return;
    }
    let asset = ResolvedAsset {
        symbol: symbol.clone(),
        relative_path: String::new(),
        display_name: symbol.clone(),
        synth_desc: Some(descriptor),
    };
    index.direct_sound_assets.entry(symbol).or_insert(asset);
}

fn insert_definition(
    index: &mut ProjectIndex,
    kind: AssetKind,
    symbol: String,
    relative_path: &str,
    range: SourceRange,
) {
    let definition = DefinitionLocation {
        relative_path: relative_path.to_string(),
        range,
    };
    match kind {
        AssetKind::DirectSound => {
            index
                .direct_sound_definitions
                .entry(symbol)
                .or_insert(definition);
        }
        AssetKind::ProgrammableWave => {
            index
                .programmable_wave_definitions
                .entry(symbol)
                .or_insert(definition);
        }
    }
}

fn index_keysplit_tables(index: &mut ProjectIndex) -> io::Result<()> {
    for relative_path in ["sound/keysplit_tables.inc", "sound/keysplit_tables.s"] {
        let path = index.root.join(relative_path);
        if !path.is_file() {
            continue;
        }

        let text = fs::read_to_string(path)?;
        let parsed = parsing::parse_keysplit_tables(relative_path, &text);
        index.keysplit_tables.extend(parsed.tables);
        index.keysplit_definitions.extend(parsed.definitions);
    }
    Ok(())
}

fn path_basename(path: &str) -> String {
    path.rsplit(['/', '\\']).next().unwrap_or(path).to_string()
}
