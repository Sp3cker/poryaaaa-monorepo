//! Project-level discovery and selected-bank loading for voicegroup source trees.

use std::collections::BTreeMap;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use crate::analyzer::{analyze_document, AnalysisContext};
use crate::ast::{Diagnostic, DiagnosticSeverity, SourcePosition, SourceRange};
use crate::catalog::SymbolNamespace;
use crate::parser::parse_document;
use crate::program_bank::{
    build_program_bank, ProgramBank, ProgramBankBuildResult, ProgramBankContext, ResolvedAsset,
};

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ProjectConfig {
    pub extra_sound_data_paths: Vec<String>,
    pub extra_voicegroup_paths: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProgramBankLoadResult {
    pub bank: Option<ProgramBank>,
    pub diagnostics: Vec<Diagnostic>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProjectIndex {
    root: PathBuf,
    voice_groups: BTreeMap<String, VoiceGroupSource>,
    direct_sound_assets: BTreeMap<String, ResolvedAsset>,
    programmable_wave_assets: BTreeMap<String, ResolvedAsset>,
    keysplit_tables: BTreeMap<String, [u8; 128]>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum VoiceGroupSource {
    File {
        relative_path: String,
    },
    Combined {
        relative_path: String,
        label: String,
    },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum AssetKind {
    DirectSound,
    ProgrammableWave,
}

impl ProjectIndex {
    pub fn load(root: impl AsRef<Path>, config: ProjectConfig) -> io::Result<Self> {
        let root = root.as_ref().to_path_buf();
        let mut index = ProjectIndex {
            root,
            voice_groups: BTreeMap::new(),
            direct_sound_assets: BTreeMap::new(),
            programmable_wave_assets: BTreeMap::new(),
            keysplit_tables: BTreeMap::new(),
        };

        index.discover_standard_voicegroups()?;
        index.discover_extra_voicegroups(&config)?;
        index.discover_monolithic_voicegroups()?;
        index.index_standard_symbol_files()?;
        index.index_extra_sound_data_files(&config)?;
        index.index_keysplit_tables()?;

        Ok(index)
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

    pub fn direct_sound_assets(&self) -> Vec<ResolvedAsset> {
        self.direct_sound_assets.values().cloned().collect()
    }

    pub fn programmable_wave_assets(&self) -> Vec<ResolvedAsset> {
        self.programmable_wave_assets.values().cloned().collect()
    }

    pub fn keysplit_table(&self, symbol: &str) -> Option<&[u8; 128]> {
        self.keysplit_tables.get(symbol)
    }

    pub fn load_program_bank(&self, bank_name: &str) -> ProgramBankLoadResult {
        let Some(source) = self.voice_groups.get(bank_name) else {
            return ProgramBankLoadResult {
                bank: None,
                diagnostics: vec![diagnostic(
                    empty_range(),
                    "missing-voicegroup",
                    "voicegroup bank is not declared in the project index",
                )],
            };
        };

        let Ok((source_relative_path, source_text)) = self.read_bank_source(source) else {
            return ProgramBankLoadResult {
                bank: None,
                diagnostics: vec![diagnostic(
                    empty_range(),
                    "voicegroup-read-failed",
                    "voicegroup source file could not be read",
                )],
            };
        };

        let document = parse_document(&source_text);
        let mut diagnostics = analyze_document(&document, &self.analysis_context());
        let Some(voice_group) = document
            .voice_groups
            .iter()
            .find(|voice_group| voice_group.name.text == bank_name)
        else {
            diagnostics.push(diagnostic(
                empty_range(),
                "missing-voicegroup",
                "voicegroup bank is not declared in the selected source",
            ));
            return ProgramBankLoadResult {
                bank: None,
                diagnostics,
            };
        };

        let ProgramBankBuildResult {
            bank,
            diagnostics: build_diagnostics,
        } = build_program_bank(
            voice_group,
            source_relative_path,
            &self.program_bank_context(),
        );
        append_unique_diagnostics(&mut diagnostics, build_diagnostics);

        ProgramBankLoadResult {
            bank: Some(bank),
            diagnostics,
        }
    }

    fn discover_standard_voicegroups(&mut self) -> io::Result<()> {
        self.discover_voicegroup_directory("sound/voicegroups")
    }

    fn discover_extra_voicegroups(&mut self, config: &ProjectConfig) -> io::Result<()> {
        for relative_path in &config.extra_voicegroup_paths {
            let path = self.root.join(relative_path);
            if path.is_dir() {
                self.discover_voicegroup_directory(relative_path)?;
            } else if path.is_file() {
                self.discover_voicegroup_file(relative_path)?;
                self.discover_combined_voicegroup_file(relative_path)?;
            }
        }
        Ok(())
    }

    fn discover_monolithic_voicegroups(&mut self) -> io::Result<()> {
        self.discover_combined_voicegroup_file("sound/voice_groups.inc")
    }

    fn discover_voicegroup_directory(&mut self, relative_path: &str) -> io::Result<()> {
        let directory = self.root.join(relative_path);
        if !directory.is_dir() {
            return Ok(());
        }

        for file in source_files_recursive(&directory)? {
            let relative_file = relative_path_string(&self.root, &file);
            self.discover_voicegroup_file(&relative_file)?;
        }

        Ok(())
    }

    fn discover_voicegroup_file(&mut self, relative_path: &str) -> io::Result<()> {
        let text = fs::read_to_string(self.root.join(relative_path))?;
        let document = parse_document(&text);
        for voice_group in document.voice_groups {
            self.voice_groups
                .entry(voice_group.name.text)
                .or_insert_with(|| VoiceGroupSource::File {
                    relative_path: relative_path.to_string(),
                });
        }
        Ok(())
    }

    fn discover_combined_voicegroup_file(&mut self, relative_path: &str) -> io::Result<()> {
        let path = self.root.join(relative_path);
        if !path.is_file() {
            return Ok(());
        }

        let text = fs::read_to_string(path)?;
        for label in parse_document(&text).assembly_labels {
            if combined_section_has_voice_macro(&text, &label.name.text) {
                self.voice_groups
                    .entry(label.name.text.clone())
                    .or_insert_with(|| VoiceGroupSource::Combined {
                        relative_path: relative_path.to_string(),
                        label: label.name.text,
                    });
            }
        }

        Ok(())
    }

    fn index_standard_symbol_files(&mut self) -> io::Result<()> {
        self.index_symbol_file("sound/direct_sound_data.inc", AssetKind::DirectSound)?;
        self.index_symbol_file(
            "sound/programmable_wave_data.inc",
            AssetKind::ProgrammableWave,
        )
    }

    fn index_extra_sound_data_files(&mut self, config: &ProjectConfig) -> io::Result<()> {
        for relative_path in &config.extra_sound_data_paths {
            self.index_symbol_file(relative_path, AssetKind::DirectSound)?;
        }
        Ok(())
    }

    fn index_symbol_file(&mut self, relative_path: &str, kind: AssetKind) -> io::Result<()> {
        let path = self.root.join(relative_path);
        if !path.is_file() {
            return Ok(());
        }

        let text = fs::read_to_string(path)?;
        let mut current_symbol = None;
        for line in text.lines() {
            let stripped = strip_comment(line).trim();
            let line_document = parse_document(stripped);
            if let Some(label) = line_document.assembly_labels.first() {
                current_symbol = Some(label.name.text.clone());
                continue;
            }

            if let Some(asset_path) = incbin_path(stripped) {
                if let Some(symbol) = current_symbol.take() {
                    self.insert_asset(kind, symbol, asset_path);
                }
            }
        }

        Ok(())
    }

    fn insert_asset(&mut self, kind: AssetKind, symbol: String, relative_path: String) {
        let asset = ResolvedAsset {
            symbol: symbol.clone(),
            display_name: path_basename(&relative_path),
            relative_path,
        };
        match kind {
            AssetKind::DirectSound => {
                self.direct_sound_assets.entry(symbol).or_insert(asset);
            }
            AssetKind::ProgrammableWave => {
                self.programmable_wave_assets.entry(symbol).or_insert(asset);
            }
        }
    }

    fn index_keysplit_tables(&mut self) -> io::Result<()> {
        let relative_path = "sound/keysplit_tables.inc";
        let path = self.root.join(relative_path);
        if !path.is_file() {
            return Ok(());
        }

        let text = fs::read_to_string(path)?;
        self.keysplit_tables.extend(parse_keysplit_tables(&text));
        Ok(())
    }

    fn read_bank_source(&self, source: &VoiceGroupSource) -> io::Result<(String, String)> {
        match source {
            VoiceGroupSource::File { relative_path } => {
                fs::read_to_string(self.root.join(relative_path))
                    .map(|text| (relative_path.clone(), text))
            }
            VoiceGroupSource::Combined {
                relative_path,
                label,
            } => {
                let text = fs::read_to_string(self.root.join(relative_path))?;
                extract_combined_section(&text, label)
                    .map(|section| (relative_path.clone(), section))
                    .ok_or_else(|| {
                        io::Error::new(
                            io::ErrorKind::InvalidData,
                            "combined voicegroup section is no longer present",
                        )
                    })
            }
        }
    }

    fn program_bank_context(&self) -> ProgramBankContext {
        let mut context = ProgramBankContext::default();
        for asset in self.direct_sound_assets.values() {
            context = context.with_direct_sound_asset(asset.clone());
        }
        for asset in self.programmable_wave_assets.values() {
            context = context.with_programmable_wave_asset(asset.clone());
        }
        for (symbol, table) in &self.keysplit_tables {
            context = context.with_keysplit_table(symbol.clone(), *table);
        }
        context
    }
}

fn source_files_recursive(directory: &Path) -> io::Result<Vec<PathBuf>> {
    let mut files = Vec::new();
    for entry in fs::read_dir(directory)? {
        let path = entry?.path();
        if path.is_dir() {
            files.extend(source_files_recursive(&path)?);
        } else if is_source_file(&path) {
            files.push(path);
        }
    }
    files.sort();
    Ok(files)
}

fn is_source_file(path: &Path) -> bool {
    path.extension()
        .and_then(|extension| extension.to_str())
        .is_some_and(|extension| {
            extension.eq_ignore_ascii_case("inc") || extension.eq_ignore_ascii_case("s")
        })
}

fn relative_path_string(root: &Path, path: &Path) -> String {
    path.strip_prefix(root)
        .unwrap_or(path)
        .components()
        .map(|component| component.as_os_str().to_string_lossy())
        .collect::<Vec<_>>()
        .join("/")
}

fn combined_section_has_voice_macro(text: &str, label: &str) -> bool {
    extract_combined_section(text, label).is_some()
}

fn extract_combined_section(text: &str, label: &str) -> Option<String> {
    let search_label = format!("{label}::");
    let mut in_section = false;
    let mut voices_seen = false;
    let mut output = format!("voice_group {label}\n");

    for line in text.lines() {
        let trimmed = strip_comment(line).trim();
        if !in_section {
            if trimmed == search_label {
                in_section = true;
            }
            continue;
        }

        if is_combined_boundary(trimmed) {
            break;
        }

        if trimmed.is_empty() {
            continue;
        }

        if trimmed.starts_with("voice_") || trimmed.starts_with("cry") {
            voices_seen = true;
            output.push_str(line);
            output.push('\n');
        }
    }

    (in_section && voices_seen).then_some(output)
}

fn is_combined_boundary(trimmed: &str) -> bool {
    trimmed.starts_with(".align") || !parse_document(trimmed).assembly_labels.is_empty()
}

fn strip_comment(line: &str) -> &str {
    line.split_once('@')
        .map(|(source, _)| source)
        .unwrap_or(line)
}

fn incbin_path(line: &str) -> Option<String> {
    let trimmed = line.trim_start();
    if !trimmed.starts_with(".incbin") {
        return None;
    }
    let start = trimmed.find('"')? + 1;
    let end = trimmed[start..].find('"')? + start;
    Some(trimmed[start..end].to_string())
}

fn parse_keysplit_tables(text: &str) -> BTreeMap<String, [u8; 128]> {
    let mut tables = BTreeMap::new();
    let mut current_name: Option<String> = None;
    let mut last_note = 0usize;

    for line in text.lines() {
        let trimmed = strip_comment(line).trim();
        if let Some((name, start_note)) = emerald_keysplit_header(trimmed) {
            tables.entry(name.clone()).or_insert([0; 128]);
            current_name = Some(name);
            last_note = start_note;
            continue;
        }

        if let Some(end_note) = emerald_split_end(trimmed) {
            if let Some(table) = current_name.as_ref().and_then(|name| tables.get_mut(name)) {
                fill_table_range(table, last_note, end_note.0, end_note.1);
                last_note = end_note.0;
            }
            continue;
        }

        if let Some((name, start_note)) = firered_keysplit_header(trimmed) {
            tables.entry(name.clone()).or_insert([0; 128]);
            current_name = Some(name);
            last_note = start_note;
            continue;
        }

        if let Some(values) = firered_byte_values(trimmed) {
            if let Some(table) = current_name.as_ref().and_then(|name| tables.get_mut(name)) {
                for value in values {
                    if last_note < 128 {
                        table[last_note] = value;
                    }
                    last_note += 1;
                }
            }
        }
    }

    tables
}

fn emerald_keysplit_header(line: &str) -> Option<(String, usize)> {
    let rest = line.strip_prefix("keysplit ")?;
    let (name, start_note) = rest.split_once(',')?;
    Some((
        format!("keysplit_{}", name.trim()),
        start_note.trim().parse().ok()?,
    ))
}

fn emerald_split_end(line: &str) -> Option<(usize, u8)> {
    let rest = line.strip_prefix("split ")?;
    let (value, end_note) = rest.split_once(',')?;
    Some((end_note.trim().parse().ok()?, value.trim().parse().ok()?))
}

fn firered_keysplit_header(line: &str) -> Option<(String, usize)> {
    let rest = line.strip_prefix(".set ")?;
    let (name, offset) = rest.split_once(", . - ")?;
    Some((name.trim().to_string(), offset.trim().parse().ok()?))
}

fn firered_byte_values(line: &str) -> Option<Vec<u8>> {
    let rest = line.strip_prefix(".byte ")?;
    Some(
        rest.split(',')
            .filter_map(|value| value.trim().parse::<u8>().ok())
            .collect(),
    )
}

fn fill_table_range(table: &mut [u8; 128], from: usize, to: usize, value: u8) {
    for slot in table.iter_mut().take(to.min(128)).skip(from) {
        *slot = value;
    }
}

fn path_basename(path: &str) -> String {
    path.rsplit(['/', '\\']).next().unwrap_or(path).to_string()
}

fn diagnostic(range: SourceRange, code: &str, message: &str) -> Diagnostic {
    Diagnostic {
        range,
        severity: DiagnosticSeverity::Error,
        code: code.to_string(),
        message: message.to_string(),
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
