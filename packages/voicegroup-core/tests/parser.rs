//! Parser contract tests for normal voicegroups, symbol labels, diagnostics, and source ranges.

use voicegroup_core::parser::{parse_document, DiagnosticSeverity, ParsedDocument, SourceRange};

fn range_text<'a>(source: &'a str, range: &SourceRange) -> &'a str {
    assert_eq!(
        range.start.line, range.end.line,
        "test helper only handles single-line ranges"
    );

    let line = source
        .lines()
        .nth(range.start.line - 1)
        .expect("range line should exist");
    &line[(range.start.column - 1)..(range.end.column - 1)]
}

fn assert_no_diagnostics(document: &ParsedDocument) {
    assert_eq!(document.diagnostics, []);
}

#[test]
fn parses_hearth_style_voice_group_with_comments_and_argument_ranges() {
    let source = "\
voice_group standard_midi
\tvoice_keysplit voicegroup_piano_keysplit, keysplit_piano @ 001 Acoustic Piano
\tvoice_directsound 60, 0, DirectSoundWaveData_sd90_classical_detuned_ep1_low, 255, 188, 128, 226 @ 005 Electric Piano 1
\tvoice_directsound_no_resample 60, 0, DirectSoundWaveData_dp_orchhitmajor60, 255, 200, 255, 171 @ 056 Orchestra Hit
";

    let document = parse_document(source);

    assert_no_diagnostics(&document);
    assert_eq!(document.voice_groups.len(), 1);

    let voice_group = &document.voice_groups[0];
    assert_eq!(voice_group.name.text, "standard_midi");
    assert_eq!(range_text(source, &voice_group.name.range), "standard_midi");
    assert_eq!(voice_group.start_slot, 0);
    assert_eq!(
        range_text(source, &voice_group.declaration_range),
        "voice_group standard_midi"
    );
    assert_eq!(voice_group.programs.len(), 3);

    let keysplit = &voice_group.programs[0];
    assert_eq!(keysplit.slot, 0);
    assert_eq!(keysplit.macro_name.text, "voice_keysplit");
    assert_eq!(
        range_text(source, &keysplit.macro_name.range),
        "voice_keysplit"
    );
    assert_eq!(
        keysplit.trailing_comment.as_deref(),
        Some("001 Acoustic Piano")
    );
    assert_eq!(
        keysplit
            .arguments
            .iter()
            .map(|argument| argument.text.as_str())
            .collect::<Vec<_>>(),
        ["voicegroup_piano_keysplit", "keysplit_piano"]
    );
    assert_eq!(
        range_text(source, &keysplit.arguments[0].range),
        "voicegroup_piano_keysplit"
    );
    assert_eq!(
        range_text(source, &keysplit.arguments[1].range),
        "keysplit_piano"
    );

    let directsound = &voice_group.programs[1];
    assert_eq!(directsound.slot, 1);
    assert_eq!(directsound.macro_name.text, "voice_directsound");
    assert_eq!(directsound.arguments.len(), 7);
    assert_eq!(
        range_text(source, &directsound.arguments[2].range),
        "DirectSoundWaveData_sd90_classical_detuned_ep1_low"
    );
    assert_eq!(
        directsound.trailing_comment.as_deref(),
        Some("005 Electric Piano 1")
    );

    let no_resample = &voice_group.programs[2];
    assert_eq!(no_resample.slot, 2);
    assert_eq!(no_resample.macro_name.text, "voice_directsound_no_resample");
    assert_eq!(no_resample.arguments.len(), 7);
}

#[test]
fn parses_drumset_start_slot_from_voice_group_declaration() {
    let source = "\
voice_group rs_drumset, 36
\tvoice_directsound_no_resample 60, 64, DirectSoundWaveData_sc88pro_rnd_kick, 255, 0, 255, 242
\tvoice_square_1 60, 0, 0, 2, 0, 0, 15, 0
";

    let document = parse_document(source);

    assert_no_diagnostics(&document);
    let voice_group = &document.voice_groups[0];
    assert_eq!(voice_group.name.text, "rs_drumset");
    assert_eq!(voice_group.start_slot, 36);

    let kick = &voice_group.programs[0];
    assert_eq!(kick.slot, 36);
    assert_eq!(kick.macro_name.text, "voice_directsound_no_resample");
    assert_eq!(range_text(source, &kick.arguments[0].range), "60");
    assert_eq!(range_text(source, &kick.arguments[1].range), "64");
    assert_eq!(
        range_text(source, &kick.arguments[2].range),
        "DirectSoundWaveData_sc88pro_rnd_kick"
    );

    let dummy = &voice_group.programs[1];
    assert_eq!(dummy.slot, 37);
    assert_eq!(dummy.macro_name.text, "voice_square_1");
    assert_eq!(dummy.arguments.len(), 8);
}

#[test]
fn parses_assembly_labels_without_turning_them_into_voice_groups() {
    let source = "\
\t.align 2
DirectSoundWaveData_sc88pro_glockenspiel::
\t.incbin \"sound/direct_sound_samples/sc88pro_glockenspiel.bin\"
";

    let document = parse_document(source);

    assert_no_diagnostics(&document);
    assert_eq!(document.voice_groups.len(), 0);
    assert_eq!(document.assembly_labels.len(), 1);

    let label = &document.assembly_labels[0];
    assert_eq!(label.name.text, "DirectSoundWaveData_sc88pro_glockenspiel");
    assert_eq!(
        range_text(source, &label.name.range),
        "DirectSoundWaveData_sc88pro_glockenspiel"
    );
    assert_eq!(
        range_text(source, &label.range),
        "DirectSoundWaveData_sc88pro_glockenspiel::"
    );
}

#[test]
fn parses_voice_macros_after_assembly_label_as_voice_group() {
    let source = "\
\t.align 2
voicegroup192::
\tvoice_square_1 60, 0, 0, 2, 0, 0, 15, 0
";

    let document = parse_document(source);

    assert_no_diagnostics(&document);
    assert_eq!(document.voice_groups.len(), 1);
    assert_eq!(document.assembly_labels.len(), 1);

    let voice_group = &document.voice_groups[0];
    assert_eq!(voice_group.name.text, "voicegroup192");
    assert_eq!(voice_group.start_slot, 0);
    assert_eq!(
        range_text(source, &voice_group.declaration_range),
        "voicegroup192::"
    );
    assert_eq!(voice_group.programs.len(), 1);
    assert_eq!(voice_group.programs[0].slot, 0);
    assert_eq!(voice_group.programs[0].macro_name.text, "voice_square_1");
}

#[test]
fn does_not_keep_assembly_label_pending_across_unrecognized_line() {
    let source = "\
voicegroup192::
not voicegroup source
\tvoice_square_1 60, 0, 0, 2, 0, 0, 15, 0
";

    let document = parse_document(source);

    assert_eq!(document.voice_groups.len(), 0);
    assert_eq!(document.assembly_labels.len(), 1);
    assert_eq!(
        document
            .diagnostics
            .iter()
            .map(|diagnostic| diagnostic.code.as_str())
            .collect::<Vec<_>>(),
        ["unrecognized-line", "macro-outside-voice-group"]
    );
}

#[test]
fn parses_cry_macro_family() {
    let source = "\
voice_group cries
\tcry CryData_Bulbasaur
\tcry_reverse CryData_Charmander @ reverse
\tcry_uncomp CryData_Squirtle
\tcry_reverse_uncomp CryData_Pikachu
";

    let document = parse_document(source);

    assert_no_diagnostics(&document);
    let programs = &document.voice_groups[0].programs;
    assert_eq!(programs.len(), 4);
    assert_eq!(programs[0].macro_name.text, "cry");
    assert_eq!(programs[1].macro_name.text, "cry_reverse");
    assert_eq!(programs[1].trailing_comment.as_deref(), Some("reverse"));
    assert_eq!(programs[2].macro_name.text, "cry_uncomp");
    assert_eq!(programs[3].macro_name.text, "cry_reverse_uncomp");
    assert_eq!(
        range_text(source, &programs[3].arguments[0].range),
        "CryData_Pikachu"
    );
}

#[test]
fn reports_syntax_diagnostics_without_discarding_valid_programs() {
    let source = "\
voice_group broken
\tvoice_directsound 60, , DirectSoundWaveData_bad, 255
\tvoice_noise 60, 0, 0, 0, 1, 0, 0
not a voice macro
";

    let document = parse_document(source);

    assert_eq!(document.voice_groups.len(), 1);
    assert_eq!(document.voice_groups[0].programs.len(), 2);
    assert_eq!(
        document.voice_groups[0].programs[1].macro_name.text,
        "voice_noise"
    );
    assert_eq!(document.diagnostics.len(), 2);
    assert_eq!(document.diagnostics[0].severity, DiagnosticSeverity::Error);
    assert_eq!(document.diagnostics[0].code, "empty-argument");
    assert_eq!(range_text(source, &document.diagnostics[0].range), "");
    assert_eq!(document.diagnostics[1].code, "unrecognized-line");
}

#[test]
fn reports_invalid_voice_group_declaration_once() {
    let source = "voice_group drums, nope\n";

    let document = parse_document(source);

    assert_eq!(document.voice_groups.len(), 0);
    assert_eq!(document.diagnostics.len(), 1);
    assert_eq!(document.diagnostics[0].severity, DiagnosticSeverity::Error);
    assert_eq!(document.diagnostics[0].code, "invalid-voice-group");
    assert_eq!(
        range_text(source, &document.diagnostics[0].range),
        "voice_group drums, nope"
    );
}

#[test]
fn parses_unknown_voice_prefixed_macros_for_later_analysis() {
    let source = "\
voice_group custom
\tvoice_group_like_macro 1, 2
";

    let document = parse_document(source);

    assert_no_diagnostics(&document);
    assert_eq!(document.voice_groups.len(), 1);
    assert_eq!(document.voice_groups[0].programs.len(), 1);
    assert_eq!(
        document.voice_groups[0].programs[0].macro_name.text,
        "voice_group_like_macro"
    );
    assert_eq!(
        document.voice_groups[0].programs[0]
            .arguments
            .iter()
            .map(|argument| argument.text.as_str())
            .collect::<Vec<_>>(),
        ["1", "2"]
    );
}

#[test]
fn parses_cry_prefixed_macros_for_later_analysis() {
    let source = "\
voice_group custom_cries
\tcry_custom CryData_Custom
";

    let document = parse_document(source);

    assert_no_diagnostics(&document);
    assert_eq!(
        document.voice_groups[0].programs[0].macro_name.text,
        "cry_custom"
    );
}

#[test]
fn reports_macro_before_any_voice_group_without_fabricating_section() {
    let source = "\
\tvoice_square_1 60, 0, 0, 2, 0, 0, 15, 0
";

    let document = parse_document(source);

    assert_eq!(document.voice_groups.len(), 0);
    assert_eq!(document.assembly_labels.len(), 0);
    assert_eq!(document.diagnostics.len(), 1);
    assert_eq!(document.diagnostics[0].code, "macro-outside-voice-group");
    assert_eq!(
        range_text(source, &document.diagnostics[0].range),
        "voice_square_1"
    );
}

#[test]
fn parse_document_is_referentially_transparent() {
    let source = "\
voice_group route104
\tvoice_keysplit_all voicegroup_rs_drumset
\tvoice_square_1 60, 0, 0, 2, 0, 0, 15, 0
\tvoice_directsound 60, 0, DirectSoundWaveData_sc88pro_synth_bass, 255, 252, 0, 115
";

    assert_eq!(parse_document(source), parse_document(source));
}

#[test]
fn parses_route104_voicegroup_fixture_shape() {
    let source = include_str!("fixtures/route104.inc");

    let document = parse_document(source);

    assert_no_diagnostics(&document);
    assert_eq!(document.voice_groups.len(), 1);
    assert_eq!(document.assembly_labels.len(), 0);
    assert_eq!(document.voice_groups[0].name.text, "route104");
    assert_eq!(document.voice_groups[0].start_slot, 0);
    assert_eq!(document.voice_groups[0].programs.len(), 8);
    assert_eq!(
        document.voice_groups[0].programs[0].macro_name.text,
        "voice_keysplit_all"
    );
    assert_eq!(document.voice_groups[0].programs[7].slot, 7);
}

#[test]
fn parses_rs_drumset_fixture_start_slot_shape() {
    let source = include_str!("fixtures/rs_drumset.inc");

    let document = parse_document(source);

    assert_no_diagnostics(&document);
    assert_eq!(document.voice_groups.len(), 1);
    assert_eq!(document.voice_groups[0].name.text, "rs_drumset");
    assert_eq!(document.voice_groups[0].start_slot, 36);
    assert_eq!(document.voice_groups[0].programs.len(), 8);
    assert_eq!(document.voice_groups[0].programs[0].slot, 36);
    assert_eq!(document.voice_groups[0].programs[7].slot, 43);
}

#[test]
fn parses_hearth_voicegroup192_fixture_shape() {
    let source = include_str!("fixtures/voicegroup192.inc");

    let document = parse_document(source);

    assert_no_diagnostics(&document);
    assert_eq!(document.voice_groups.len(), 1);
    assert_eq!(document.assembly_labels.len(), 1);

    let voice_group = &document.voice_groups[0];
    assert_eq!(voice_group.name.text, "voicegroup192");
    assert_eq!(voice_group.start_slot, 0);
    assert_eq!(voice_group.programs.len(), 114);
    assert_eq!(
        voice_group.programs[0].macro_name.text,
        "voice_directsound_no_resample"
    );
    assert_eq!(voice_group.programs[42].slot, 42);
    assert_eq!(voice_group.programs[42].macro_name.text, "voice_noise");
    assert_eq!(voice_group.programs[113].slot, 113);
}

#[test]
fn parses_direct_sound_fixture_as_assembly_labels_only() {
    let source = include_str!("fixtures/direct_sound_data.inc");

    let document = parse_document(source);

    assert_no_diagnostics(&document);
    assert_eq!(document.voice_groups.len(), 0);
    assert_eq!(document.assembly_labels.len(), 3);
    assert_eq!(
        document.assembly_labels[0].name.text,
        "DirectSoundWaveData_sc88pro_glockenspiel"
    );
    assert_eq!(
        document.assembly_labels[2].name.text,
        "DirectSoundWaveData_sc88pro_fretless_bass"
    );
}
