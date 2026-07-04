//! Analyzer and catalog contract tests for macro schemas, diagnostics, and symbol context.

use voicegroup_core::analyzer::{analyze_document, AnalysisContext};
use voicegroup_core::catalog::{
    all_macros, find_macro, ArgumentSchema, MacroKind, NumericRange, SymbolNamespace, VoiceType,
};
use voicegroup_core::parser::{parse_document, Diagnostic};

fn diagnostic_codes(diagnostics: &[Diagnostic]) -> Vec<&str> {
    diagnostics
        .iter()
        .map(|diagnostic| diagnostic.code.as_str())
        .collect()
}

#[test]
fn catalog_contains_poryaaaa_macro_contract() {
    struct ExpectedMacro {
        name: &'static str,
        type_code: VoiceType,
        kind: MacroKind,
        arguments: &'static [ArgumentSchema],
    }

    let expected = [
        ExpectedMacro {
            name: "voice_directsound_no_resample",
            type_code: VoiceType::DirectSoundNoResample,
            kind: MacroKind::DirectSoundNoResample,
            arguments: DIRECT_SOUND_ARGUMENTS,
        },
        ExpectedMacro {
            name: "voice_directsound_alt",
            type_code: VoiceType::DirectSoundAlt,
            kind: MacroKind::DirectSoundAlt,
            arguments: DIRECT_SOUND_ARGUMENTS,
        },
        ExpectedMacro {
            name: "voice_directsound",
            type_code: VoiceType::DirectSound,
            kind: MacroKind::DirectSound,
            arguments: DIRECT_SOUND_ARGUMENTS,
        },
        ExpectedMacro {
            name: "voice_square_1_alt",
            type_code: VoiceType::Square1Alt,
            kind: MacroKind::Square1,
            arguments: SQUARE_1_ARGUMENTS,
        },
        ExpectedMacro {
            name: "voice_square_1",
            type_code: VoiceType::Square1,
            kind: MacroKind::Square1,
            arguments: SQUARE_1_ARGUMENTS,
        },
        ExpectedMacro {
            name: "voice_square_2_alt",
            type_code: VoiceType::Square2Alt,
            kind: MacroKind::Square2,
            arguments: SQUARE_2_ARGUMENTS,
        },
        ExpectedMacro {
            name: "voice_square_2",
            type_code: VoiceType::Square2,
            kind: MacroKind::Square2,
            arguments: SQUARE_2_ARGUMENTS,
        },
        ExpectedMacro {
            name: "voice_programmable_wave_alt",
            type_code: VoiceType::ProgrammableWaveAlt,
            kind: MacroKind::ProgrammableWave,
            arguments: PROGRAMMABLE_WAVE_ARGUMENTS,
        },
        ExpectedMacro {
            name: "voice_programmable_wave",
            type_code: VoiceType::ProgrammableWave,
            kind: MacroKind::ProgrammableWave,
            arguments: PROGRAMMABLE_WAVE_ARGUMENTS,
        },
        ExpectedMacro {
            name: "voice_noise_alt",
            type_code: VoiceType::NoiseAlt,
            kind: MacroKind::Noise,
            arguments: NOISE_ARGUMENTS,
        },
        ExpectedMacro {
            name: "voice_noise",
            type_code: VoiceType::Noise,
            kind: MacroKind::Noise,
            arguments: NOISE_ARGUMENTS,
        },
        ExpectedMacro {
            name: "voice_keysplit_all",
            type_code: VoiceType::KeysplitAll,
            kind: MacroKind::KeysplitAll,
            arguments: KEYSPLIT_ALL_ARGUMENTS,
        },
        ExpectedMacro {
            name: "voice_keysplit",
            type_code: VoiceType::Keysplit,
            kind: MacroKind::Keysplit,
            arguments: KEYSPLIT_ARGUMENTS,
        },
        ExpectedMacro {
            name: "cry_reverse",
            type_code: VoiceType::CryReverse,
            kind: MacroKind::Cry,
            arguments: CRY_ARGUMENTS,
        },
        ExpectedMacro {
            name: "cry",
            type_code: VoiceType::Cry,
            kind: MacroKind::Cry,
            arguments: CRY_ARGUMENTS,
        },
    ];

    assert_eq!(all_macros().len(), expected.len());
    assert_eq!(
        all_macros()
            .iter()
            .map(|definition| definition.name)
            .collect::<Vec<_>>(),
        expected
            .iter()
            .map(|macro_| macro_.name)
            .collect::<Vec<_>>()
    );

    for expected_macro in &expected {
        let definition = find_macro(expected_macro.name).expect("macro should exist");
        assert_eq!(definition.name, expected_macro.name);
        assert_eq!(definition.type_code, expected_macro.type_code);
        assert_eq!(definition.kind, expected_macro.kind);
        assert_eq!(
            definition
                .arguments
                .iter()
                .map(|argument| argument.schema)
                .collect::<Vec<_>>(),
            expected_macro.arguments
        );
    }

    assert!(find_macro("voice_custom").is_none());
}

#[test]
fn analyzer_accepts_valid_document_with_contextual_symbols() {
    let source = "\
voice_group route104
\tvoice_directsound 60, 0, DirectSoundWaveData_Brass1, 1, 2, 3, 4
\tvoice_programmable_wave 60, 0, ProgrammableWaveData_Pulse1, 1, 2, 8, 3
\tvoice_keysplit voicegroup_rs_drumset, keysplit_strings
\tvoice_keysplit_all voicegroup_shared
\tcry_reverse DirectSoundWaveData_Cry
voice_group voicegroup_rs_drumset
\tvoice_noise 60, 0, 0, 1, 2, 8, 3
";

    let document = parse_document(source);
    let context = AnalysisContext::default()
        .with_symbols(
            SymbolNamespace::DirectSound,
            ["DirectSoundWaveData_Brass1", "DirectSoundWaveData_Cry"],
        )
        .with_symbols(
            SymbolNamespace::ProgrammableWave,
            ["ProgrammableWaveData_Pulse1"],
        )
        .with_symbols(SymbolNamespace::Keysplit, ["keysplit_strings"])
        .with_symbols(SymbolNamespace::VoiceGroup, ["voicegroup_shared"]);

    assert_eq!(analyze_document(&document, &context), []);
}

#[test]
fn analyzer_reports_unknown_macro_at_macro_name() {
    let source = "\
voice_group custom
\tvoice_custom 1, 2
\tcry_custom CryData_Custom
";

    let document = parse_document(source);
    let diagnostics = analyze_document(&document, &AnalysisContext::default());

    assert_eq!(
        diagnostic_codes(&diagnostics),
        ["unknown-macro", "unknown-macro"]
    );
    assert_eq!(
        diagnostics[0].range,
        document.voice_groups[0].programs[0].macro_name.range
    );
    assert_eq!(
        diagnostics[1].range,
        document.voice_groups[0].programs[1].macro_name.range
    );
}

#[test]
fn analyzer_reports_wrong_argument_counts() {
    let source = "\
voice_group broken
\tvoice_square_2 60
\tvoice_keysplit voicegroup_rs_drumset
";

    let document = parse_document(source);
    let diagnostics = analyze_document(&document, &AnalysisContext::default());

    assert_eq!(
        diagnostic_codes(&diagnostics),
        ["wrong-argument-count", "wrong-argument-count"]
    );
    assert_eq!(
        diagnostics[0].range,
        document.voice_groups[0].programs[0].range
    );
    assert_eq!(
        diagnostics[1].range,
        document.voice_groups[0].programs[1].range
    );
}

#[test]
fn analyzer_validates_integer_arguments_and_ranges() {
    let source = "\
voice_group broken
\tvoice_directsound nope, 0, DirectSoundWaveData_Brass1, 1, 2, 3, 4
\tvoice_square_1 128, 0, 0, 3, 1, 2, 8, 3
";

    let document = parse_document(source);
    let context = AnalysisContext::default()
        .with_symbols(SymbolNamespace::DirectSound, ["DirectSoundWaveData_Brass1"]);
    let diagnostics = analyze_document(&document, &context);

    assert_eq!(
        diagnostic_codes(&diagnostics),
        ["invalid-integer", "integer-out-of-range"]
    );
    assert_eq!(
        diagnostics[0].range,
        document.voice_groups[0].programs[0].arguments[0].range
    );
    assert_eq!(
        diagnostics[1].range,
        document.voice_groups[0].programs[1].arguments[0].range
    );
}

#[test]
fn analyzer_enforces_hardware_bit_width_ranges_from_catalog() {
    let source = "\
voice_group hardware_limits
\tvoice_square_1 60, 0, 0, 4, 8, 8, 16, 8
\tvoice_noise 60, 0, 2, 8, 8, 16, 8
";

    let document = parse_document(source);
    let diagnostics = analyze_document(&document, &AnalysisContext::default());

    assert_eq!(
        diagnostic_codes(&diagnostics),
        [
            "integer-out-of-range",
            "integer-out-of-range",
            "integer-out-of-range",
            "integer-out-of-range",
            "integer-out-of-range",
            "integer-out-of-range",
            "integer-out-of-range",
            "integer-out-of-range",
            "integer-out-of-range",
            "integer-out-of-range",
        ]
    );
    assert_eq!(
        diagnostics
            .iter()
            .map(|diagnostic| diagnostic.range.clone())
            .collect::<Vec<_>>(),
        [
            document.voice_groups[0].programs[0].arguments[3]
                .range
                .clone(),
            document.voice_groups[0].programs[0].arguments[4]
                .range
                .clone(),
            document.voice_groups[0].programs[0].arguments[5]
                .range
                .clone(),
            document.voice_groups[0].programs[0].arguments[6]
                .range
                .clone(),
            document.voice_groups[0].programs[0].arguments[7]
                .range
                .clone(),
            document.voice_groups[0].programs[1].arguments[2]
                .range
                .clone(),
            document.voice_groups[0].programs[1].arguments[3]
                .range
                .clone(),
            document.voice_groups[0].programs[1].arguments[4]
                .range
                .clone(),
            document.voice_groups[0].programs[1].arguments[5]
                .range
                .clone(),
            document.voice_groups[0].programs[1].arguments[6]
                .range
                .clone(),
        ]
    );
}

#[test]
fn analyzer_reports_program_slots_outside_bank() {
    let source = "\
voice_group too_high, 127
\tvoice_square_2 60, 0, 2, 1, 2, 8, 3
\tvoice_square_2 61, 0, 2, 1, 2, 8, 3
";

    let document = parse_document(source);
    let diagnostics = analyze_document(&document, &AnalysisContext::default());

    assert_eq!(diagnostic_codes(&diagnostics), ["slot-out-of-range"]);
    assert_eq!(
        diagnostics[0].range,
        document.voice_groups[0].programs[1].range
    );
}

#[test]
fn analyzer_reports_missing_contextual_symbols_by_argument_kind() {
    let source = "\
voice_group missing_symbols
\tvoice_directsound 60, 0, DirectSoundWaveData_Missing, 1, 2, 3, 4
\tvoice_programmable_wave 60, 0, ProgrammableWaveData_Missing, 1, 2, 8, 3
\tvoice_keysplit voicegroup_missing, keysplit_missing
\tvoice_keysplit_all voicegroup_missing_all
\tcry DirectSoundWaveData_CryMissing
";

    let document = parse_document(source);
    let diagnostics = analyze_document(&document, &AnalysisContext::default());

    assert_eq!(
        diagnostic_codes(&diagnostics),
        [
            "unknown-directsound-symbol",
            "unknown-programmable-wave-symbol",
            "unknown-voicegroup-symbol",
            "unknown-keysplit-symbol",
            "unknown-voicegroup-symbol",
            "unknown-directsound-symbol"
        ]
    );
    assert_eq!(
        diagnostics[0].range,
        document.voice_groups[0].programs[0].arguments[2].range
    );
    assert_eq!(
        diagnostics[1].range,
        document.voice_groups[0].programs[1].arguments[2].range
    );
    assert_eq!(
        diagnostics[2].range,
        document.voice_groups[0].programs[2].arguments[0].range
    );
    assert_eq!(
        diagnostics[3].range,
        document.voice_groups[0].programs[2].arguments[1].range
    );
    assert_eq!(
        diagnostics[4].range,
        document.voice_groups[0].programs[3].arguments[0].range
    );
    assert_eq!(
        diagnostics[5].range,
        document.voice_groups[0].programs[4].arguments[0].range
    );
}

#[test]
fn analyzer_unknown_directsound_message_names_source_file() {
    let source = "\
voice_group test
\tvoice_directsound 60, 0, DirectSoundWaveData_Missing, 1, 2, 3, 4
";

    let document = parse_document(source);
    let diagnostics = analyze_document(&document, &AnalysisContext::default());

    assert_eq!(
        diagnostic_codes(&diagnostics),
        ["unknown-directsound-symbol"]
    );
    assert!(
        !diagnostics[0].message.contains("analysis context"),
        "message should not expose analyzer internals: {}",
        diagnostics[0].message
    );
    assert!(
        diagnostics[0].message.contains("direct_sound_data.inc"),
        "message should name the DirectSound declaration source: {}",
        diagnostics[0].message
    );
}

#[test]
fn analyzer_unknown_programmable_wave_message_names_source_file() {
    let source = "\
voice_group test
\tvoice_programmable_wave 60, 0, ProgrammableWaveData_Missing, 1, 2, 8, 3
";

    let document = parse_document(source);
    let diagnostics = analyze_document(&document, &AnalysisContext::default());

    assert_eq!(
        diagnostic_codes(&diagnostics),
        ["unknown-programmable-wave-symbol"]
    );
    assert!(
        !diagnostics[0].message.contains("analysis context"),
        "message should not expose analyzer internals: {}",
        diagnostics[0].message
    );
    assert!(
        diagnostics[0]
            .message
            .contains("programmable_wave_data.inc"),
        "message should name the ProgrammableWave declaration source: {}",
        diagnostics[0].message
    );
}

#[test]
fn analyzer_unknown_keysplit_message_names_source_file() {
    let source = "\
voice_group local
\tvoice_keysplit local, keysplit_missing
";

    let document = parse_document(source);
    let diagnostics = analyze_document(&document, &AnalysisContext::default());

    assert_eq!(diagnostic_codes(&diagnostics), ["unknown-keysplit-symbol"]);
    assert!(
        !diagnostics[0].message.contains("analysis context"),
        "message should not expose analyzer internals: {}",
        diagnostics[0].message
    );
    assert!(
        diagnostics[0].message.contains("keysplit_tables.inc"),
        "message should name the Keysplit declaration source: {}",
        diagnostics[0].message
    );
}

#[test]
fn analyzer_unknown_voicegroup_message_names_source_layout() {
    let source = "\
voice_group consumer
\tvoice_keysplit_all voicegroup_missing
";

    let document = parse_document(source);
    let diagnostics = analyze_document(&document, &AnalysisContext::default());

    assert_eq!(
        diagnostic_codes(&diagnostics),
        ["unknown-voicegroup-symbol"]
    );
    assert!(
        !diagnostics[0].message.contains("analysis context"),
        "message should not expose analyzer internals: {}",
        diagnostics[0].message
    );
    assert!(
        diagnostics[0].message.contains("voice_groups.inc"),
        "message should name the VoiceGroup declaration layout: {}",
        diagnostics[0].message
    );
}

#[test]
fn analyzer_reports_duplicate_voice_group_names_and_duplicate_slots_within_name() {
    let source = "\
voice_group repeated, 4
\tvoice_noise 60, 0, 0, 1, 2, 8, 3
voice_group repeated, 4
\tvoice_noise 61, 0, 0, 1, 2, 8, 3
";

    let document = parse_document(source);
    let diagnostics = analyze_document(&document, &AnalysisContext::default());

    assert_eq!(
        diagnostic_codes(&diagnostics),
        ["duplicate-voice-group", "duplicate-slot"]
    );
    assert_eq!(diagnostics[0].range, document.voice_groups[1].name.range);
    assert_eq!(
        diagnostics[1].range,
        document.voice_groups[1].programs[0].range
    );
}

#[test]
fn analyzer_reports_ambiguous_duplicate_voice_group_references() {
    let source = "\
voice_group repeated
\tvoice_noise 60, 0, 0, 1, 2, 8, 3
voice_group repeated, 1
\tvoice_noise 61, 0, 0, 1, 2, 8, 3
voice_group consumer
\tvoice_keysplit_all repeated
";

    let document = parse_document(source);
    let diagnostics = analyze_document(&document, &AnalysisContext::default());

    assert_eq!(
        diagnostic_codes(&diagnostics),
        ["duplicate-voice-group", "ambiguous-voicegroup-symbol"]
    );
    assert_eq!(diagnostics[0].range, document.voice_groups[1].name.range);
    assert_eq!(
        diagnostics[1].range,
        document.voice_groups[2].programs[0].arguments[0].range
    );
}

#[test]
fn analyze_document_preserves_parser_diagnostics_before_semantic_diagnostics() {
    let source = "\
voice_group broken
\tvoice_directsound 60, , DirectSoundWaveData_Brass1, 1, 2, 3, 4
";

    let document = parse_document(source);
    let diagnostics = analyze_document(&document, &AnalysisContext::default());

    assert_eq!(
        diagnostic_codes(&diagnostics),
        ["empty-argument", "unknown-directsound-symbol"]
    );
    assert_eq!(diagnostics[0], document.diagnostics[0]);
}

#[test]
fn analyzer_is_referentially_transparent() {
    let document = parse_document(
        "\
voice_group route104
\tvoice_square_2 60, 0, 2, 1, 2, 8, 3
",
    );

    let context = AnalysisContext::default();

    assert_eq!(
        analyze_document(&document, &context),
        analyze_document(&document, &context)
    );
}

const MIDI_RANGE: NumericRange = NumericRange { min: 0, max: 127 };
const PAN_RANGE: NumericRange = NumericRange { min: 0, max: 127 };
const BYTE_RANGE: NumericRange = NumericRange { min: 0, max: 255 };
const DUTY_RANGE: NumericRange = NumericRange { min: 0, max: 3 };
const PERIOD_RANGE: NumericRange = NumericRange { min: 0, max: 1 };
const ATTACK_RANGE: NumericRange = NumericRange { min: 0, max: 7 };
const DECAY_RANGE: NumericRange = NumericRange { min: 0, max: 7 };
const SUSTAIN_RANGE: NumericRange = NumericRange { min: 0, max: 15 };
const RELEASE_RANGE: NumericRange = NumericRange { min: 0, max: 7 };

const DIRECT_SOUND_ARGUMENTS: &[ArgumentSchema] = &[
    ArgumentSchema::Integer { range: MIDI_RANGE },
    ArgumentSchema::Integer { range: PAN_RANGE },
    ArgumentSchema::Symbol {
        namespace: SymbolNamespace::DirectSound,
    },
    ArgumentSchema::Integer { range: BYTE_RANGE },
    ArgumentSchema::Integer { range: BYTE_RANGE },
    ArgumentSchema::Integer { range: BYTE_RANGE },
    ArgumentSchema::Integer { range: BYTE_RANGE },
];

const SQUARE_1_ARGUMENTS: &[ArgumentSchema] = &[
    ArgumentSchema::Integer { range: MIDI_RANGE },
    ArgumentSchema::Integer { range: PAN_RANGE },
    ArgumentSchema::Integer { range: BYTE_RANGE },
    ArgumentSchema::Integer { range: DUTY_RANGE },
    ArgumentSchema::Integer {
        range: ATTACK_RANGE,
    },
    ArgumentSchema::Integer { range: DECAY_RANGE },
    ArgumentSchema::Integer {
        range: SUSTAIN_RANGE,
    },
    ArgumentSchema::Integer {
        range: RELEASE_RANGE,
    },
];

const SQUARE_2_ARGUMENTS: &[ArgumentSchema] = &[
    ArgumentSchema::Integer { range: MIDI_RANGE },
    ArgumentSchema::Integer { range: PAN_RANGE },
    ArgumentSchema::Integer { range: DUTY_RANGE },
    ArgumentSchema::Integer {
        range: ATTACK_RANGE,
    },
    ArgumentSchema::Integer { range: DECAY_RANGE },
    ArgumentSchema::Integer {
        range: SUSTAIN_RANGE,
    },
    ArgumentSchema::Integer {
        range: RELEASE_RANGE,
    },
];

const PROGRAMMABLE_WAVE_ARGUMENTS: &[ArgumentSchema] = &[
    ArgumentSchema::Integer { range: MIDI_RANGE },
    ArgumentSchema::Integer { range: PAN_RANGE },
    ArgumentSchema::Symbol {
        namespace: SymbolNamespace::ProgrammableWave,
    },
    ArgumentSchema::Integer {
        range: ATTACK_RANGE,
    },
    ArgumentSchema::Integer { range: DECAY_RANGE },
    ArgumentSchema::Integer {
        range: SUSTAIN_RANGE,
    },
    ArgumentSchema::Integer {
        range: RELEASE_RANGE,
    },
];

const NOISE_ARGUMENTS: &[ArgumentSchema] = &[
    ArgumentSchema::Integer { range: MIDI_RANGE },
    ArgumentSchema::Integer { range: PAN_RANGE },
    ArgumentSchema::Integer {
        range: PERIOD_RANGE,
    },
    ArgumentSchema::Integer {
        range: ATTACK_RANGE,
    },
    ArgumentSchema::Integer { range: DECAY_RANGE },
    ArgumentSchema::Integer {
        range: SUSTAIN_RANGE,
    },
    ArgumentSchema::Integer {
        range: RELEASE_RANGE,
    },
];

const KEYSPLIT_ALL_ARGUMENTS: &[ArgumentSchema] = &[ArgumentSchema::Symbol {
    namespace: SymbolNamespace::VoiceGroup,
}];

const KEYSPLIT_ARGUMENTS: &[ArgumentSchema] = &[
    ArgumentSchema::Symbol {
        namespace: SymbolNamespace::VoiceGroup,
    },
    ArgumentSchema::Symbol {
        namespace: SymbolNamespace::Keysplit,
    },
];

const CRY_ARGUMENTS: &[ArgumentSchema] = &[ArgumentSchema::Symbol {
    namespace: SymbolNamespace::DirectSound,
}];
