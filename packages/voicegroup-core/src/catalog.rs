//! Canonical voice macro catalog: names, type codes, macro families, and argument schemas.

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

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MacroArgument {
    pub name: &'static str,
    pub schema: ArgumentSchema,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MacroDefinition {
    pub name: &'static str,
    pub type_code: VoiceType,
    pub kind: MacroKind,
    pub arguments: &'static [MacroArgument],
}

const MIDI_RANGE: NumericRange = NumericRange { min: 0, max: 127 };
const PAN_RANGE: NumericRange = NumericRange { min: 0, max: 127 };
const BYTE_RANGE: NumericRange = NumericRange { min: 0, max: 255 };

const DIRECT_SOUND_ARGUMENTS: &[MacroArgument] = &[
    integer_argument("base_midi_key", MIDI_RANGE),
    integer_argument("pan", PAN_RANGE),
    MacroArgument {
        name: "sample_data_pointer",
        schema: ArgumentSchema::Symbol {
            namespace: SymbolNamespace::DirectSound,
        },
    },
    integer_argument("attack", BYTE_RANGE),
    integer_argument("decay", BYTE_RANGE),
    integer_argument("sustain", BYTE_RANGE),
    integer_argument("release", BYTE_RANGE),
];

const SQUARE_1_ARGUMENTS: &[MacroArgument] = &[
    integer_argument("base_midi_key", MIDI_RANGE),
    integer_argument("pan", PAN_RANGE),
    integer_argument("sweep", BYTE_RANGE),
    integer_argument("duty_cycle", BYTE_RANGE),
    integer_argument("attack", BYTE_RANGE),
    integer_argument("decay", BYTE_RANGE),
    integer_argument("sustain", BYTE_RANGE),
    integer_argument("release", BYTE_RANGE),
];

const SQUARE_2_ARGUMENTS: &[MacroArgument] = &[
    integer_argument("base_midi_key", MIDI_RANGE),
    integer_argument("pan", PAN_RANGE),
    integer_argument("duty_cycle", BYTE_RANGE),
    integer_argument("attack", BYTE_RANGE),
    integer_argument("decay", BYTE_RANGE),
    integer_argument("sustain", BYTE_RANGE),
    integer_argument("release", BYTE_RANGE),
];

const PROGRAMMABLE_WAVE_ARGUMENTS: &[MacroArgument] = &[
    integer_argument("base_midi_key", MIDI_RANGE),
    integer_argument("pan", PAN_RANGE),
    MacroArgument {
        name: "wave_samples_pointer",
        schema: ArgumentSchema::Symbol {
            namespace: SymbolNamespace::ProgrammableWave,
        },
    },
    integer_argument("attack", BYTE_RANGE),
    integer_argument("decay", BYTE_RANGE),
    integer_argument("sustain", BYTE_RANGE),
    integer_argument("release", BYTE_RANGE),
];

const NOISE_ARGUMENTS: &[MacroArgument] = &[
    integer_argument("base_midi_key", MIDI_RANGE),
    integer_argument("pan", PAN_RANGE),
    integer_argument("period", BYTE_RANGE),
    integer_argument("attack", BYTE_RANGE),
    integer_argument("decay", BYTE_RANGE),
    integer_argument("sustain", BYTE_RANGE),
    integer_argument("release", BYTE_RANGE),
];

const KEYSPLIT_ALL_ARGUMENTS: &[MacroArgument] = &[MacroArgument {
    name: "voice_group_pointer",
    schema: ArgumentSchema::Symbol {
        namespace: SymbolNamespace::VoiceGroup,
    },
}];

const KEYSPLIT_ARGUMENTS: &[MacroArgument] = &[
    MacroArgument {
        name: "voice_group_pointer",
        schema: ArgumentSchema::Symbol {
            namespace: SymbolNamespace::VoiceGroup,
        },
    },
    MacroArgument {
        name: "keysplit_table_pointer",
        schema: ArgumentSchema::Symbol {
            namespace: SymbolNamespace::Keysplit,
        },
    },
];

const CRY_ARGUMENTS: &[MacroArgument] = &[MacroArgument {
    name: "sample",
    schema: ArgumentSchema::Symbol {
        namespace: SymbolNamespace::DirectSound,
    },
}];

const MACROS: &[MacroDefinition] = &[
    MacroDefinition {
        name: "voice_directsound_no_resample",
        type_code: VoiceType::DirectSoundNoResample,
        kind: MacroKind::DirectSoundNoResample,
        arguments: DIRECT_SOUND_ARGUMENTS,
    },
    MacroDefinition {
        name: "voice_directsound_alt",
        type_code: VoiceType::DirectSoundAlt,
        kind: MacroKind::DirectSoundAlt,
        arguments: DIRECT_SOUND_ARGUMENTS,
    },
    MacroDefinition {
        name: "voice_directsound",
        type_code: VoiceType::DirectSound,
        kind: MacroKind::DirectSound,
        arguments: DIRECT_SOUND_ARGUMENTS,
    },
    MacroDefinition {
        name: "voice_square_1_alt",
        type_code: VoiceType::Square1Alt,
        kind: MacroKind::Square1,
        arguments: SQUARE_1_ARGUMENTS,
    },
    MacroDefinition {
        name: "voice_square_1",
        type_code: VoiceType::Square1,
        kind: MacroKind::Square1,
        arguments: SQUARE_1_ARGUMENTS,
    },
    MacroDefinition {
        name: "voice_square_2_alt",
        type_code: VoiceType::Square2Alt,
        kind: MacroKind::Square2,
        arguments: SQUARE_2_ARGUMENTS,
    },
    MacroDefinition {
        name: "voice_square_2",
        type_code: VoiceType::Square2,
        kind: MacroKind::Square2,
        arguments: SQUARE_2_ARGUMENTS,
    },
    MacroDefinition {
        name: "voice_programmable_wave_alt",
        type_code: VoiceType::ProgrammableWaveAlt,
        kind: MacroKind::ProgrammableWave,
        arguments: PROGRAMMABLE_WAVE_ARGUMENTS,
    },
    MacroDefinition {
        name: "voice_programmable_wave",
        type_code: VoiceType::ProgrammableWave,
        kind: MacroKind::ProgrammableWave,
        arguments: PROGRAMMABLE_WAVE_ARGUMENTS,
    },
    MacroDefinition {
        name: "voice_noise_alt",
        type_code: VoiceType::NoiseAlt,
        kind: MacroKind::Noise,
        arguments: NOISE_ARGUMENTS,
    },
    MacroDefinition {
        name: "voice_noise",
        type_code: VoiceType::Noise,
        kind: MacroKind::Noise,
        arguments: NOISE_ARGUMENTS,
    },
    MacroDefinition {
        name: "voice_keysplit_all",
        type_code: VoiceType::KeysplitAll,
        kind: MacroKind::KeysplitAll,
        arguments: KEYSPLIT_ALL_ARGUMENTS,
    },
    MacroDefinition {
        name: "voice_keysplit",
        type_code: VoiceType::Keysplit,
        kind: MacroKind::Keysplit,
        arguments: KEYSPLIT_ARGUMENTS,
    },
    MacroDefinition {
        name: "cry_reverse",
        type_code: VoiceType::CryReverse,
        kind: MacroKind::Cry,
        arguments: CRY_ARGUMENTS,
    },
    MacroDefinition {
        name: "cry",
        type_code: VoiceType::Cry,
        kind: MacroKind::Cry,
        arguments: CRY_ARGUMENTS,
    },
];

pub fn all_macros() -> &'static [MacroDefinition] {
    MACROS
}

pub fn find_macro(name: &str) -> Option<&'static MacroDefinition> {
    MACROS.iter().find(|definition| definition.name == name)
}

const fn integer_argument(name: &'static str, valid_range: NumericRange) -> MacroArgument {
    MacroArgument {
        name,
        schema: ArgumentSchema::Integer { range: valid_range },
    }
}
