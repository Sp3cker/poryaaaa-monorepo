//! Canonical voice macro catalog: names, type codes, macro families, and argument schemas.
use crate::ast::Diagnostic;
use std::collections::BTreeMap;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MacroKind {
    DirectSound,
    DirectSoundAlt,
    DirectSoundNoResample,
    Square1,
    Square2,
    ProgrammableWave,
    Noise,
    Keysplit,
    KeysplitAll,
    Cry,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VoiceType {
    DirectSound = 0x00,
    Square1 = 0x01,
    Square2 = 0x02,
    ProgrammableWave = 0x03,
    Noise = 0x04,
    DirectSoundNoResample = 0x08,
    Square1Alt = 0x09,
    Square2Alt = 0x0A,
    ProgrammableWaveAlt = 0x0B,
    NoiseAlt = 0x0C,
    DirectSoundAlt = 0x10,
    Cry = 0x20,
    CryReverse = 0x30,
    Keysplit = 0x40,
    KeysplitAll = 0x80,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ArgumentSchema {
    Integer { range: NumericRange },
    Symbol { namespace: SymbolNamespace },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum SymbolNamespace {
    DirectSound,
    ProgrammableWave,
    VoiceGroup,
    Keysplit,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct NumericRange {
    pub min: i32,
    pub max: i32,
}

impl NumericRange {
    pub const fn contains(&self, value: i32) -> bool {
        value >= self.min && value <= self.max
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MacroArgument {
    pub name: &'static str,
    pub schema: ArgumentSchema,
    pub help: &'static str,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MacroDefinition {
    pub name: &'static str,
    pub type_code: VoiceType,
    pub kind: MacroKind,
    pub arguments: &'static [MacroArgument],
    pub summary: &'static str,
}

const MIDI_RANGE: NumericRange = NumericRange { min: 0, max: 127 };
const PAN_RANGE: NumericRange = NumericRange { min: 0, max: 127 };
const BYTE_RANGE: NumericRange = NumericRange { min: 0, max: 255 };

const DIRECT_SOUND_ARGUMENTS: &[MacroArgument] = &[
    integer_argument(
        "base_midi_key",
        MIDI_RANGE,
        "Root MIDI note used when pitching the DirectSound sample.",
    ),
    integer_argument(
        "pan",
        PAN_RANGE,
        "m4a pan value. Zero means centered/disabled in the emitted macro. Squares accept 0, 64, 127 enumerated.",
    ),
    symbol_argument(
        "sample_data_pointer",
        SymbolNamespace::DirectSound,
        "DirectSoundWaveData_* symbol resolved through direct_sound_data.inc.",
    ),
    integer_argument("attack", BYTE_RANGE, "Envelope attack byte."),
    integer_argument("decay", BYTE_RANGE, "Envelope decay byte."),
    integer_argument("sustain", BYTE_RANGE, "Envelope sustain byte."),
    integer_argument("release", BYTE_RANGE, "Envelope release byte."),
];

const SQUARE_1_ARGUMENTS: &[MacroArgument] = &[
    integer_argument(
        "base_midi_key",
        MIDI_RANGE,
        "Root MIDI note used when pitching the DirectSound sample.",
    ),
    integer_argument(
        "pan",
        PAN_RANGE,
        "m4a pan value. Zero means centered/disabled in the emitted macro. Squares accept 0, 64, 127 enumerated.",
    ),
    integer_argument("sweep", BYTE_RANGE, "Square 1 sweep byte."),
    integer_argument(
        "duty_cycle",
        NumericRange { min: 0, max: 3 },
        "Hardware duty cycle masked to two bits by the assembler macro.",
    ),
    integer_argument(
        "attack",
        NumericRange { min: 0, max: 7 },
        "3-bit hardware envelope attack.",
    ),
    integer_argument(
        "decay",
        NumericRange { min: 0, max: 7 },
        "3-bit hardware envelope decay.",
    ),
    integer_argument(
        "sustain",
        NumericRange { min: 0, max: 15 },
        "4-bit hardware envelope sustain.",
    ),
    integer_argument(
        "release",
        NumericRange { min: 0, max: 7 },
        "3-bit hardware envelope release.",
    ),
];

const SQUARE_2_ARGUMENTS: &[MacroArgument] = &[
    integer_argument(
        "base_midi_key",
        MIDI_RANGE,
        "Root MIDI note used when pitching the DirectSound sample.",
    ),
    integer_argument(
        "pan",
        PAN_RANGE,
        "m4a pan value. Zero means centered/disabled in the emitted macro. Squares accept 0, 64, 127 enumerated.",
    ),
    integer_argument(
        "duty_cycle",
        NumericRange { min: 0, max: 3 },
        "Hardware duty cycle masked to two bits by the assembler macro.",
    ),
    integer_argument(
        "attack",
        NumericRange { min: 0, max: 7 },
        "3-bit hardware envelope attack.",
    ),
    integer_argument(
        "decay",
        NumericRange { min: 0, max: 7 },
        "3-bit hardware envelope decay.",
    ),
    integer_argument(
        "sustain",
        NumericRange { min: 0, max: 15 },
        "4-bit hardware envelope sustain.",
    ),
    integer_argument(
        "release",
        NumericRange { min: 0, max: 7 },
        "3-bit hardware envelope release.",
    ),
];

const PROGRAMMABLE_WAVE_ARGUMENTS: &[MacroArgument] = &[
    integer_argument(
        "base_midi_key",
        MIDI_RANGE,
        "Root MIDI note used when pitching the DirectSound sample.",
    ),
    integer_argument(
        "pan",
        PAN_RANGE,
        "m4a pan value. Zero means centered/disabled in the emitted macro. Squares accept 0, 64, 127 enumerated.",
    ),
    symbol_argument(
        "wave_samples_pointer",
        SymbolNamespace::ProgrammableWave,
        "ProgrammableWaveData_* symbol resolved through programmable_wave_data.inc.",
    ),
    integer_argument(
        "attack",
        NumericRange { min: 0, max: 7 },
        "3-bit hardware envelope attack.",
    ),
    integer_argument(
        "decay",
        NumericRange { min: 0, max: 7 },
        "3-bit hardware envelope decay.",
    ),
    integer_argument(
        "sustain",
        NumericRange { min: 0, max: 15 },
        "4-bit hardware envelope sustain.",
    ),
    integer_argument(
        "release",
        NumericRange { min: 0, max: 7 },
        "3-bit hardware envelope release.",
    ),
];

const NOISE_ARGUMENTS: &[MacroArgument] = &[
    integer_argument(
        "base_midi_key",
        MIDI_RANGE,
        "Root MIDI note for the noise voice.",
    ),
    integer_argument(
        "pan",
        PAN_RANGE,
        "Accepted for macro compatibility; poryaaaa runtime ignores it for noise voices.",
    ),
    integer_argument(
        "period",
        NumericRange { min: 0, max: 1 },
        "Noise period bit. The assembler macro masks this to one bit.",
    ),
    integer_argument(
        "attack",
        NumericRange { min: 0, max: 7 },
        "3-bit hardware envelope attack.",
    ),
    integer_argument(
        "decay",
        NumericRange { min: 0, max: 7 },
        "3-bit hardware envelope decay.",
    ),
    integer_argument(
        "sustain",
        NumericRange { min: 0, max: 15 },
        "4-bit hardware envelope sustain.",
    ),
    integer_argument(
        "release",
        NumericRange { min: 0, max: 7 },
        "3-bit hardware envelope release.",
    ),
];

const KEYSPLIT_ALL_ARGUMENTS: &[MacroArgument] = &[symbol_argument(
    "voice_group_pointer",
    SymbolNamespace::VoiceGroup,
    "Sub-voicegroup used for all notes.",
)];

const KEYSPLIT_ARGUMENTS: &[MacroArgument] = &[
    symbol_argument(
        "voice_group_pointer",
        SymbolNamespace::VoiceGroup,
        "Sub-voicegroup selected by the keysplit table.",
    ),
    symbol_argument(
        "keysplit_table_pointer",
        SymbolNamespace::Keysplit,
        "Keysplit table that maps notes to sub-voice slots.",
    ),
];

const CRY_ARGUMENTS: &[MacroArgument] = &[symbol_argument(
    "sample",
    SymbolNamespace::DirectSound,
    "Cry sample symbol from direct sound data.",
)];

const MACROS: &[MacroDefinition] = &[
    MacroDefinition {
        name: "voice_directsound_no_resample",
        type_code: VoiceType::DirectSoundNoResample,
        kind: MacroKind::DirectSoundNoResample,
        arguments: DIRECT_SOUND_ARGUMENTS,
        summary: "DirectSound sample without m4a resampling.",
    },
    MacroDefinition {
        name: "voice_directsound_alt",
        type_code: VoiceType::DirectSoundAlt,
        kind: MacroKind::DirectSoundAlt,
        arguments: DIRECT_SOUND_ARGUMENTS,
        summary: "DirectSound sample using the alternate fixed voice type.",
    },
    MacroDefinition {
        name: "voice_directsound",
        type_code: VoiceType::DirectSound,
        kind: MacroKind::DirectSound,
        arguments: DIRECT_SOUND_ARGUMENTS,
        summary: "DirectSound sample voice.",
    },
    MacroDefinition {
        name: "voice_square_1_alt",
        type_code: VoiceType::Square1Alt,
        kind: MacroKind::Square1,
        arguments: SQUARE_1_ARGUMENTS,
        summary: "Square channel 1 alternate hardware voice.",
    },
    MacroDefinition {
        name: "voice_square_1",
        type_code: VoiceType::Square1,
        kind: MacroKind::Square1,
        arguments: SQUARE_1_ARGUMENTS,
        summary: "Square channel 1 hardware voice.",
    },
    MacroDefinition {
        name: "voice_square_2_alt",
        type_code: VoiceType::Square2Alt,
        kind: MacroKind::Square2,
        arguments: SQUARE_2_ARGUMENTS,
        summary: "Square channel 2 alternate hardware voice.",
    },
    MacroDefinition {
        name: "voice_square_2",
        type_code: VoiceType::Square2,
        kind: MacroKind::Square2,
        arguments: SQUARE_2_ARGUMENTS,
        summary: "Square channel 2 hardware voice.",
    },
    MacroDefinition {
        name: "voice_programmable_wave_alt",
        type_code: VoiceType::ProgrammableWaveAlt,
        kind: MacroKind::ProgrammableWave,
        arguments: PROGRAMMABLE_WAVE_ARGUMENTS,
        summary: "Programmable wave alternate hardware voice.",
    },
    MacroDefinition {
        name: "voice_programmable_wave",
        type_code: VoiceType::ProgrammableWave,
        kind: MacroKind::ProgrammableWave,
        arguments: PROGRAMMABLE_WAVE_ARGUMENTS,
        summary: "Programmable wave hardware voice.",
    },
    MacroDefinition {
        name: "voice_noise_alt",
        type_code: VoiceType::NoiseAlt,
        kind: MacroKind::Noise,
        arguments: NOISE_ARGUMENTS,
        summary: "Noise channel alternate hardware voice.",
    },
    MacroDefinition {
        name: "voice_noise",
        type_code: VoiceType::Noise,
        kind: MacroKind::Noise,
        arguments: NOISE_ARGUMENTS,
        summary: "Noise channel hardware voice.",
    },
    MacroDefinition {
        name: "voice_keysplit_all",
        type_code: VoiceType::KeysplitAll,
        kind: MacroKind::KeysplitAll,
        arguments: KEYSPLIT_ALL_ARGUMENTS,
        summary: "Routes all notes into a sub-voicegroup, commonly a drumset.",
    },
    MacroDefinition {
        name: "voice_keysplit",
        type_code: VoiceType::Keysplit,
        kind: MacroKind::Keysplit,
        arguments: KEYSPLIT_ARGUMENTS,
        summary: "Routes notes to slots in another voicegroup through a keysplit table.",
    },
    MacroDefinition {
        name: "cry_reverse",
        type_code: VoiceType::CryReverse,
        kind: MacroKind::Cry,
        arguments: CRY_ARGUMENTS,
        summary: "Reverse cry sample voice.",
    },
    MacroDefinition {
        name: "cry",
        type_code: VoiceType::Cry,
        kind: MacroKind::Cry,
        arguments: CRY_ARGUMENTS,
        summary: "Cry sample voice.",
    },
];

pub fn all_macros() -> &'static [MacroDefinition] {
    MACROS
}

pub fn find_macro(name: &str) -> Option<&'static MacroDefinition> {
    MACROS.iter().find(|definition| definition.name == name)
}

/// Golden Sun synth macro definitions that can be created by the project picker.
pub(crate) const SYNTH_MACRO_WORDS: &[&str] = &[
    "set_synth_25",
    "set_synth_50",
    "set_synth_custom",
    "set_synth_pulse",
    "set_synth_saw",
    "set_synth_triangle",
];

pub(crate) fn is_synth_macro_word(name: &str) -> bool {
    SYNTH_MACRO_WORDS.contains(&name)
}

/// Parses one Golden Sun `set_synth_*` macro invocation into the six bytes
/// stored in a zero-length DirectSound descriptor.
///
/// The token boundary check is intentional: aliases such as `set_synth_50`
/// must not accept a longer symbol that merely starts with the alias.
pub fn synth_descriptor(line: &str) -> Option<[u8; 6]> {
    const ALIASES: &[(&str, u8, bool)] = &[
        ("set_synth_custom", 0, true),
        ("set_synth_pulse", 0, true),
        ("set_synth_25", 1, false),
        ("set_synth_saw", 1, false),
        ("set_synth_50", 2, false),
        ("set_synth_triangle", 2, false),
    ];

    let line = line
        .split_once('@')
        .map_or(line, |(source, _)| source)
        .trim();
    for &(name, waveform, has_parameters) in ALIASES {
        let Some(rest) = line.strip_prefix(name) else {
            continue;
        };
        if rest
            .chars()
            .next()
            .is_some_and(|character| !character.is_ascii_whitespace() && character != ',')
        {
            continue;
        }

        let mut descriptor = [0; 6];
        descriptor[0] = 0x80;
        descriptor[1] = waveform;
        if has_parameters {
            let mut values = rest
                .split(|character: char| character == ',' || character.is_ascii_whitespace())
                .filter(|part| !part.is_empty());
            for byte in &mut descriptor[2..] {
                *byte = parse_integer(values.next()?)?;
            }
            if values.next().is_some() {
                return None;
            }
        }
        return Some(descriptor);
    }

    None
}

fn parse_integer(text: &str) -> Option<u8> {
    let text = text.trim();
    let (radix, digits) = text
        .strip_prefix("0x")
        .or_else(|| text.strip_prefix("0X"))
        .map_or((10, text), |digits| (16, digits));
    u8::from_str_radix(digits, radix).ok()
}

/// The catalog row families crossing the bulk project snapshot seam.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum CatalogEntryKind {
    VoiceGroup,
    DirectSound,
    ProgrammableWave,
    Keysplit,
    Drumkit,
    Synth,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct KeysplitCatalogPair {
    pub subgroup: String,
    pub table: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CatalogEntry {
    pub kind: CatalogEntryKind,
    pub symbol: String,
    pub display_name: String,
    /// The source file that declares this row, never an include-table path for
    /// voicegroup rows.
    pub source_path: Option<String>,
    pub asset_path: Option<String>,
    pub dependency_paths: Vec<String>,
    pub keysplit: Option<KeysplitCatalogPair>,
    pub drumkit: Option<String>,
    pub typical_adsr: Option<[u8; 4]>,
    pub synth_desc: Option<[u8; 6]>,
}

/// Rust-owned bulk catalog and exact watch candidates for one project index.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ProjectCatalog {
    pub entries: Vec<CatalogEntry>,
    pub content_paths: Vec<String>,
    pub dependency_paths: Vec<String>,
    pub watch_paths: Vec<String>,
    pub typical_adsr_by_symbol: BTreeMap<String, [u8; 4]>,
    pub typical_adsr_by_family: BTreeMap<String, [u8; 4]>,
    pub synth_macro_words: Vec<String>,
}
/// One complete project-index read for the C project adapter.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ProjectSnapshot {
    /// Whether the project index was successfully built and is available for loading.
    /// Always `true` when a snapshot exists; independent of catalog diagnostics.
    pub succeeded: bool,
    pub catalog: ProjectCatalog,
    pub diagnostics: Vec<Diagnostic>,
}

const fn integer_argument(
    name: &'static str,
    valid_range: NumericRange,
    help: &'static str,
) -> MacroArgument {
    MacroArgument {
        name,
        schema: ArgumentSchema::Integer { range: valid_range },
        help,
    }
}

const fn symbol_argument(
    name: &'static str,
    namespace: SymbolNamespace,
    help: &'static str,
) -> MacroArgument {
    MacroArgument {
        name,
        schema: ArgumentSchema::Symbol { namespace },
        help,
    }
}
