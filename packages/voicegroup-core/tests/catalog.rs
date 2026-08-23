//! Catalog metadata contract tests for migrated macro summaries and argument help.

use voicegroup_core::catalog::{all_macros, find_macro, MacroArgument, MacroDefinition};

fn macro_definition(name: &str) -> &'static MacroDefinition {
    find_macro(name).unwrap_or_else(|| panic!("expected catalog macro {name}"))
}

fn macro_definition_from_all(name: &str) -> &'static MacroDefinition {
    all_macros()
        .iter()
        .find(|definition| definition.name == name)
        .unwrap_or_else(|| panic!("expected catalog macro {name} in all_macros"))
}

fn macro_argument(macro_name: &str, argument_name: &str) -> &'static MacroArgument {
    macro_definition(macro_name)
        .arguments
        .iter()
        .find(|argument| argument.name == argument_name)
        .unwrap_or_else(|| panic!("expected {macro_name} argument {argument_name}"))
}

fn assert_mentions_any(actual: &str, alternatives: &[&str]) {
    let actual_lowercase = actual.to_ascii_lowercase();
    assert!(
        alternatives
            .iter()
            .any(|alternative| actual_lowercase.contains(&alternative.to_ascii_lowercase())),
        "expected {actual:?} to mention one of {alternatives:?}"
    );
}

#[test]
fn catalog_metadata_is_complete_for_every_macro_and_argument() {
    for definition in all_macros() {
        assert!(
            !definition.summary.trim().is_empty(),
            "expected {} to have a non-empty summary",
            definition.name
        );
        for argument in definition.arguments {
            assert!(
                !argument.help.trim().is_empty(),
                "expected {}.{} to have non-empty help",
                definition.name,
                argument.name
            );
        }
    }
}

#[test]
fn catalog_metadata_matches_representative_swift_domain_text() {
    assert_eq!(
        macro_definition("voice_noise")
            .arguments
            .iter()
            .find(|argument| argument.name == "pan")
            .expect("voice_noise pan argument")
            .help,
        "Accepted for macro compatibility; poryaaaa runtime ignores it for noise voices."
    );
    assert_eq!(
        macro_argument("voice_square_1", "duty_cycle").help,
        "Hardware duty cycle masked to two bits by the assembler macro."
    );
    assert_eq!(
        macro_argument("voice_directsound", "sample_data_pointer").help,
        "DirectSoundWaveData_* symbol resolved through direct_sound_data.inc."
    );
    assert_eq!(
        macro_definition("voice_keysplit").summary,
        "Routes notes to slots in another voicegroup through a keysplit table."
    );
}

#[test]
fn catalog_summaries_carry_macro_domain_purpose() {
    assert_mentions_any(
        macro_definition("voice_directsound").summary,
        &["directsound", "sample voice"],
    );
    assert_mentions_any(
        macro_definition_from_all("voice_keysplit").summary,
        &["routes notes", "keysplit table"],
    );
}

#[test]
fn catalog_argument_help_carries_symbol_source_and_runtime_semantics() {
    assert_mentions_any(
        macro_argument("voice_directsound", "sample_data_pointer").help,
        &["direct_sound_data.inc"],
    );
    assert_mentions_any(
        macro_argument("voice_programmable_wave", "wave_samples_pointer").help,
        &["programmable_wave_data.inc"],
    );
    assert_mentions_any(
        macro_argument("voice_noise", "pan").help,
        &["poryaaaa runtime ignores", "ignored for noise voices"],
    );
    assert_mentions_any(
        macro_argument("voice_square_1", "duty_cycle").help,
        &["two bits", "masked"],
    );
}

#[test]
fn synth_descriptor_recognizes_all_aliases_without_prefix_collisions() {
    use voicegroup_core::catalog::synth_descriptor;

    assert_eq!(
        synth_descriptor("set_synth_custom 0x12, 0x34, 56, 78"),
        Some([0x80, 0, 0x12, 0x34, 56, 78])
    );
    assert_eq!(
        synth_descriptor("set_synth_pulse 1, 2, 3, 4 @ pending"),
        Some([0x80, 0, 1, 2, 3, 4])
    );
    assert_eq!(
        synth_descriptor("set_synth_25"),
        Some([0x80, 1, 0, 0, 0, 0])
    );
    assert_eq!(
        synth_descriptor("set_synth_saw\t"),
        Some([0x80, 1, 0, 0, 0, 0])
    );
    assert_eq!(
        synth_descriptor("set_synth_50"),
        Some([0x80, 2, 0, 0, 0, 0])
    );
    assert_eq!(
        synth_descriptor("set_synth_triangle // not a parser comment"),
        Some([0x80, 2, 0, 0, 0, 0])
    );
    assert_eq!(synth_descriptor("set_synth_50_extra"), None);
    assert_eq!(synth_descriptor("set_synth_custom 1, 2, 3"), None);
}
