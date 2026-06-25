//! Typed 128-slot program bank records consumed by poryaaaa adapters.

use std::collections::BTreeMap;

use crate::ast::{Diagnostic, DiagnosticSeverity, ParsedProgram, ParsedVoiceGroup, SourceRange};
use crate::catalog::{find_macro, MacroDefinition, MacroKind, VoiceType};

pub const VOICEGROUP_CORE_PROGRAM_BANK_SIZE: usize = 128;
const PROGRAM_BANK_SIZE: usize = VOICEGROUP_CORE_PROGRAM_BANK_SIZE;
pub type ProgramBankDiagnostic = Diagnostic;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProgramBank {
    pub name: String,
    pub source_relative_path: String,
    pub programs: [Option<ProgramRecord>; PROGRAM_BANK_SIZE],
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProgramBankBuildResult {
    pub bank: ProgramBank,
    pub diagnostics: Vec<ProgramBankDiagnostic>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProgramRecord {
    pub slot: usize,
    pub macro_name: String,
    pub type_code: VoiceType,
    pub display_name: String,
    pub trailing_comment: Option<String>,
    pub source_range: SourceRange,
    pub data: ProgramData,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ProgramData {
    DirectSound(DirectSoundProgram),
    ProgrammableWave(ProgrammableWaveProgram),
    Square1(Square1Program),
    Square2(Square2Program),
    Noise(NoiseProgram),
    Keysplit(KeysplitProgram),
    KeysplitAll(KeysplitAllProgram),
    Cry(CryProgram),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DirectSoundProgram {
    pub key: u8,
    pub pan: u8,
    pub sample_symbol: String,
    pub sample_relative_path: String,
    pub attack: u8,
    pub decay: u8,
    pub sustain: u8,
    pub release: u8,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProgrammableWaveProgram {
    pub key: u8,
    pub pan: u8,
    pub wave_symbol: String,
    pub wave_relative_path: String,
    pub attack: u8,
    pub decay: u8,
    pub sustain: u8,
    pub release: u8,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Square1Program {
    pub key: u8,
    pub pan: u8,
    pub sweep: u8,
    pub duty: u8,
    pub attack: u8,
    pub decay: u8,
    pub sustain: u8,
    pub release: u8,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Square2Program {
    pub key: u8,
    pub pan: u8,
    pub duty: u8,
    pub attack: u8,
    pub decay: u8,
    pub sustain: u8,
    pub release: u8,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct NoiseProgram {
    pub key: u8,
    pub pan: u8,
    pub period: u8,
    pub attack: u8,
    pub decay: u8,
    pub sustain: u8,
    pub release: u8,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct KeysplitProgram {
    pub sub_voicegroup: String,
    pub table_symbol: String,
    pub table: [u8; 128],
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct KeysplitAllProgram {
    pub sub_voicegroup: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CryProgram {
    pub sample_symbol: String,
    pub sample_relative_path: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ResolvedAsset {
    pub symbol: String,
    pub relative_path: String,
    pub display_name: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ProgramBankContext {
    direct_sound_assets: BTreeMap<String, ResolvedAsset>,
    programmable_wave_assets: BTreeMap<String, ResolvedAsset>,
    keysplit_tables: BTreeMap<String, [u8; 128]>,
    voice_groups: BTreeMap<String, ()>,
}

impl ProgramBankContext {
    pub fn with_direct_sound_asset(mut self, asset: ResolvedAsset) -> Self {
        self.direct_sound_assets.insert(asset.symbol.clone(), asset);
        self
    }

    pub fn with_programmable_wave_asset(mut self, asset: ResolvedAsset) -> Self {
        self.programmable_wave_assets
            .insert(asset.symbol.clone(), asset);
        self
    }

    pub fn with_keysplit_table(mut self, symbol: impl Into<String>, table: [u8; 128]) -> Self {
        self.keysplit_tables.insert(symbol.into(), table);
        self
    }

    pub fn with_voice_group(mut self, symbol: impl Into<String>) -> Self {
        self.voice_groups.insert(symbol.into(), ());
        self
    }

    fn resolve_sub_voicegroup(&self, symbol: &str) -> String {
        symbol
            .strip_prefix("voicegroup_")
            .filter(|stripped| self.voice_groups.contains_key(*stripped))
            .unwrap_or(symbol)
            .to_string()
    }
}

pub fn build_program_bank(
    voice_group: &ParsedVoiceGroup,
    source_relative_path: impl Into<String>,
    context: &ProgramBankContext,
) -> ProgramBankBuildResult {
    let mut programs = std::array::from_fn(|_| None);
    let mut diagnostics = Vec::new();

    for program in &voice_group.programs {
        if program.slot >= PROGRAM_BANK_SIZE {
            diagnostics.push(diagnostic(
                program.range.clone(),
                "slot-out-of-range",
                "voice macro program slot is outside the 0..127 bank range",
            ));
            continue;
        }

        if programs[program.slot].is_some() {
            diagnostics.push(diagnostic(
                program.range.clone(),
                "duplicate-slot",
                "voice macro populates a slot already used by this voice_group",
            ));
            continue;
        }

        match build_program_record(program, context) {
            Ok(record) => {
                let slot = record.slot;
                programs[slot] = Some(record);
            }
            Err(diagnostic) => diagnostics.push(diagnostic),
        }
    }

    ProgramBankBuildResult {
        bank: ProgramBank {
            name: voice_group.name.text.clone(),
            source_relative_path: source_relative_path.into(),
            programs,
        },
        diagnostics,
    }
}

fn build_program_record(
    program: &ParsedProgram,
    context: &ProgramBankContext,
) -> Result<ProgramRecord, ProgramBankDiagnostic> {
    let definition = find_macro(&program.macro_name.text).ok_or_else(|| {
        diagnostic(
            program.macro_name.range.clone(),
            "unknown-macro",
            "voice macro is not defined in the macro catalog",
        )
    })?;
    let data = build_program_data(program, definition, context)?;
    let display_name = display_name(program, definition.type_code, &data, context);

    Ok(ProgramRecord {
        slot: program.slot,
        macro_name: program.macro_name.text.clone(),
        type_code: definition.type_code,
        display_name,
        trailing_comment: program.trailing_comment.clone(),
        source_range: program.range.clone(),
        data,
    })
}

fn build_program_data(
    program: &ParsedProgram,
    definition: &MacroDefinition,
    context: &ProgramBankContext,
) -> Result<ProgramData, ProgramBankDiagnostic> {
    if program.arguments.len() != definition.arguments.len() {
        return Err(diagnostic(
            program.range.clone(),
            "wrong-argument-count",
            "voice macro argument count does not match the macro catalog",
        ));
    }

    match definition.kind {
        MacroKind::DirectSound | MacroKind::DirectSoundAlt | MacroKind::DirectSoundNoResample => {
            let args = DirectSoundArgs::from_program(program)?;
            let sample_asset = direct_sound_asset(program, args.sample_symbol, context)?;
            Ok(ProgramData::DirectSound(DirectSoundProgram {
                key: args.key,
                pan: args.pan,
                sample_symbol: sample_asset.symbol.clone(),
                sample_relative_path: sample_asset.relative_path.clone(),
                attack: args.attack,
                decay: args.decay,
                sustain: args.sustain,
                release: args.release,
            }))
        }
        MacroKind::ProgrammableWave => {
            let args = ProgrammableWaveArgs::from_program(program)?;
            let wave_asset = programmable_wave_asset(program, args.wave_symbol, context)?;
            Ok(ProgramData::ProgrammableWave(ProgrammableWaveProgram {
                key: args.key,
                pan: args.pan,
                wave_symbol: wave_asset.symbol.clone(),
                wave_relative_path: wave_asset.relative_path.clone(),
                attack: args.attack & 0x07,
                decay: args.decay & 0x07,
                sustain: args.sustain & 0x0f,
                release: args.release & 0x07,
            }))
        }
        MacroKind::Square1 => Square1Args::from_program(program).map(|args| {
            ProgramData::Square1(Square1Program {
                key: args.key,
                pan: args.pan,
                sweep: args.sweep,
                duty: args.duty & 0x03,
                attack: args.attack & 0x07,
                decay: args.decay & 0x07,
                sustain: args.sustain & 0x0f,
                release: args.release & 0x07,
            })
        }),
        MacroKind::Square2 => Square2Args::from_program(program).map(|args| {
            ProgramData::Square2(Square2Program {
                key: args.key,
                pan: args.pan,
                duty: args.duty & 0x03,
                attack: args.attack & 0x07,
                decay: args.decay & 0x07,
                sustain: args.sustain & 0x0f,
                release: args.release & 0x07,
            })
        }),
        MacroKind::Noise => NoiseArgs::from_program(program).map(|args| {
            ProgramData::Noise(NoiseProgram {
                key: args.key,
                pan: args.pan,
                period: args.period & 0x01,
                attack: args.attack & 0x07,
                decay: args.decay & 0x07,
                sustain: args.sustain & 0x0f,
                release: args.release & 0x07,
            })
        }),
        MacroKind::Keysplit => {
            let args = KeysplitArgs::from_program(program)?;
            let Some(table) = context.keysplit_tables.get(args.table_symbol) else {
                return Err(diagnostic(
                    program.arguments[1].range.clone(),
                    "unknown-keysplit-symbol",
                    "keysplit table is not declared in the program bank context",
                ));
            };
            Ok(ProgramData::Keysplit(KeysplitProgram {
                sub_voicegroup: context.resolve_sub_voicegroup(args.sub_voicegroup),
                table_symbol: args.table_symbol.to_string(),
                table: *table,
            }))
        }
        MacroKind::KeysplitAll => {
            let args = KeysplitAllArgs::from_program(program)?;
            Ok(ProgramData::KeysplitAll(KeysplitAllProgram {
                sub_voicegroup: context.resolve_sub_voicegroup(args.sub_voicegroup),
            }))
        }
        MacroKind::Cry => {
            let args = CryArgs::from_program(program)?;
            let sample_asset = direct_sound_asset(program, args.sample_symbol, context)?;
            Ok(ProgramData::Cry(CryProgram {
                sample_symbol: sample_asset.symbol.clone(),
                sample_relative_path: sample_asset.relative_path.clone(),
            }))
        }
    }
}

fn display_name(
    program: &ParsedProgram,
    voice_type: VoiceType,
    data: &ProgramData,
    context: &ProgramBankContext,
) -> String {
    match data {
        ProgramData::DirectSound(data) => data
            .asset_display_name(&context.direct_sound_assets)
            .unwrap_or_else(|| path_basename(&data.sample_relative_path)),
        ProgramData::ProgrammableWave(data) => data
            .asset_display_name(&context.programmable_wave_assets)
            .unwrap_or_else(|| path_basename(&data.wave_relative_path)),
        ProgramData::Square1(_) => match voice_type {
            VoiceType::Square1Alt => "Square 1 (alt)".to_string(),
            _ => "Square 1".to_string(),
        },
        ProgramData::Square2(_) => match voice_type {
            VoiceType::Square2Alt => "Square 2 (alt)".to_string(),
            _ => "Square 2".to_string(),
        },
        ProgramData::Noise(_) => match voice_type {
            VoiceType::NoiseAlt => "Noise (alt)".to_string(),
            _ => "Noise".to_string(),
        },
        ProgramData::Keysplit(data) => program
            .trailing_comment
            .clone()
            .unwrap_or_else(|| data.sub_voicegroup.clone()),
        ProgramData::KeysplitAll(data) => program
            .trailing_comment
            .clone()
            .unwrap_or_else(|| data.sub_voicegroup.clone()),
        ProgramData::Cry(data) => data
            .asset_display_name(&context.direct_sound_assets)
            .unwrap_or_else(|| path_basename(&data.sample_relative_path)),
    }
}

impl DirectSoundProgram {
    fn asset_display_name(&self, assets: &BTreeMap<String, ResolvedAsset>) -> Option<String> {
        assets
            .get(&self.sample_symbol)
            .map(|asset| asset.display_name.clone())
    }
}

impl ProgrammableWaveProgram {
    fn asset_display_name(&self, assets: &BTreeMap<String, ResolvedAsset>) -> Option<String> {
        assets
            .get(&self.wave_symbol)
            .map(|asset| asset.display_name.clone())
    }
}

impl CryProgram {
    fn asset_display_name(&self, assets: &BTreeMap<String, ResolvedAsset>) -> Option<String> {
        assets
            .get(&self.sample_symbol)
            .map(|asset| asset.display_name.clone())
    }
}

fn path_basename(path: &str) -> String {
    path.rsplit('/').next().unwrap_or(path).to_string()
}

struct DirectSoundArgs<'a> {
    key: u8,
    pan: u8,
    sample_symbol: &'a str,
    attack: u8,
    decay: u8,
    sustain: u8,
    release: u8,
}

impl<'a> DirectSoundArgs<'a> {
    fn from_program(program: &'a ParsedProgram) -> Result<Self, ProgramBankDiagnostic> {
        Ok(Self {
            key: argument_u8(program, 0)?,
            pan: argument_u8(program, 1)?,
            sample_symbol: argument_text(program, 2)?,
            attack: argument_u8(program, 3)?,
            decay: argument_u8(program, 4)?,
            sustain: argument_u8(program, 5)?,
            release: argument_u8(program, 6)?,
        })
    }
}

struct ProgrammableWaveArgs<'a> {
    key: u8,
    pan: u8,
    wave_symbol: &'a str,
    attack: u8,
    decay: u8,
    sustain: u8,
    release: u8,
}

impl<'a> ProgrammableWaveArgs<'a> {
    fn from_program(program: &'a ParsedProgram) -> Result<Self, ProgramBankDiagnostic> {
        Ok(Self {
            key: argument_u8(program, 0)?,
            pan: argument_u8(program, 1)?,
            wave_symbol: argument_text(program, 2)?,
            attack: argument_u8(program, 3)?,
            decay: argument_u8(program, 4)?,
            sustain: argument_u8(program, 5)?,
            release: argument_u8(program, 6)?,
        })
    }
}

struct Square1Args {
    key: u8,
    pan: u8,
    sweep: u8,
    duty: u8,
    attack: u8,
    decay: u8,
    sustain: u8,
    release: u8,
}

impl Square1Args {
    fn from_program(program: &ParsedProgram) -> Result<Self, ProgramBankDiagnostic> {
        Ok(Self {
            key: argument_u8(program, 0)?,
            pan: argument_u8(program, 1)?,
            sweep: argument_u8(program, 2)?,
            duty: argument_u8(program, 3)?,
            attack: argument_u8(program, 4)?,
            decay: argument_u8(program, 5)?,
            sustain: argument_u8(program, 6)?,
            release: argument_u8(program, 7)?,
        })
    }
}

struct Square2Args {
    key: u8,
    pan: u8,
    duty: u8,
    attack: u8,
    decay: u8,
    sustain: u8,
    release: u8,
}

impl Square2Args {
    fn from_program(program: &ParsedProgram) -> Result<Self, ProgramBankDiagnostic> {
        Ok(Self {
            key: argument_u8(program, 0)?,
            pan: argument_u8(program, 1)?,
            duty: argument_u8(program, 2)?,
            attack: argument_u8(program, 3)?,
            decay: argument_u8(program, 4)?,
            sustain: argument_u8(program, 5)?,
            release: argument_u8(program, 6)?,
        })
    }
}

struct NoiseArgs {
    key: u8,
    pan: u8,
    period: u8,
    attack: u8,
    decay: u8,
    sustain: u8,
    release: u8,
}

impl NoiseArgs {
    fn from_program(program: &ParsedProgram) -> Result<Self, ProgramBankDiagnostic> {
        Ok(Self {
            key: argument_u8(program, 0)?,
            pan: argument_u8(program, 1)?,
            period: argument_u8(program, 2)?,
            attack: argument_u8(program, 3)?,
            decay: argument_u8(program, 4)?,
            sustain: argument_u8(program, 5)?,
            release: argument_u8(program, 6)?,
        })
    }
}

struct KeysplitArgs<'a> {
    sub_voicegroup: &'a str,
    table_symbol: &'a str,
}

impl<'a> KeysplitArgs<'a> {
    fn from_program(program: &'a ParsedProgram) -> Result<Self, ProgramBankDiagnostic> {
        Ok(Self {
            sub_voicegroup: argument_text(program, 0)?,
            table_symbol: argument_text(program, 1)?,
        })
    }
}

struct KeysplitAllArgs<'a> {
    sub_voicegroup: &'a str,
}

impl<'a> KeysplitAllArgs<'a> {
    fn from_program(program: &'a ParsedProgram) -> Result<Self, ProgramBankDiagnostic> {
        Ok(Self {
            sub_voicegroup: argument_text(program, 0)?,
        })
    }
}

struct CryArgs<'a> {
    sample_symbol: &'a str,
}

impl<'a> CryArgs<'a> {
    fn from_program(program: &'a ParsedProgram) -> Result<Self, ProgramBankDiagnostic> {
        Ok(Self {
            sample_symbol: argument_text(program, 0)?,
        })
    }
}

fn argument_text(program: &ParsedProgram, index: usize) -> Result<&str, ProgramBankDiagnostic> {
    program
        .arguments
        .get(index)
        .map(|argument| argument.text.as_str())
        .ok_or_else(|| {
            diagnostic(
                program.range.clone(),
                "wrong-argument-count",
                "voice macro argument count does not match the macro catalog",
            )
        })
}

fn argument_u8(program: &ParsedProgram, index: usize) -> Result<u8, ProgramBankDiagnostic> {
    let argument = program.arguments.get(index).ok_or_else(|| {
        diagnostic(
            program.range.clone(),
            "wrong-argument-count",
            "voice macro argument count does not match the macro catalog",
        )
    })?;

    argument.text.parse::<u8>().map_err(|_| {
        diagnostic(
            argument.range.clone(),
            "invalid-integer",
            "macro argument must be an integer in the 0..255 range",
        )
    })
}

fn direct_sound_asset<'a>(
    program: &ParsedProgram,
    symbol: &str,
    context: &'a ProgramBankContext,
) -> Result<&'a ResolvedAsset, ProgramBankDiagnostic> {
    asset(
        program,
        symbol,
        &context.direct_sound_assets,
        "unknown-directsound-symbol",
    )
}

fn programmable_wave_asset<'a>(
    program: &ParsedProgram,
    symbol: &str,
    context: &'a ProgramBankContext,
) -> Result<&'a ResolvedAsset, ProgramBankDiagnostic> {
    asset(
        program,
        symbol,
        &context.programmable_wave_assets,
        "unknown-programmable-wave-symbol",
    )
}

fn asset<'a>(
    program: &ParsedProgram,
    symbol: &str,
    assets: &'a BTreeMap<String, ResolvedAsset>,
    code: &'static str,
) -> Result<&'a ResolvedAsset, ProgramBankDiagnostic> {
    assets.get(symbol).ok_or_else(|| {
        let argument_range = program
            .arguments
            .iter()
            .find(|argument| argument.text == symbol)
            .map(|argument| argument.range.clone())
            .unwrap_or_else(|| program.range.clone());
        diagnostic(
            argument_range,
            code,
            "asset symbol is not declared in the program bank context",
        )
    })
}

fn diagnostic(range: SourceRange, code: &'static str, message: &str) -> ProgramBankDiagnostic {
    ProgramBankDiagnostic {
        range,
        severity: DiagnosticSeverity::Error,
        code: code.to_string(),
        message: message.to_string(),
    }
}
