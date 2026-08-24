//! ProjectIndex tests for project discovery, symbol indexing, and selected-bank loading.

use std::collections::BTreeSet;
use std::fs;
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use voicegroup_core::ast::{SourcePosition, SourceRange};
use voicegroup_core::catalog::VoiceType;
use voicegroup_core::program_bank::ProgramData;
use voicegroup_core::project_index::ProjectIndex;

fn temp_project(name: &str) -> PathBuf {
    let nonce = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("clock should be after epoch")
        .as_nanos();
    let root = std::env::temp_dir().join(format!("voicegroup-core-{name}-{nonce}"));
    fs::create_dir_all(&root).expect("create temp project");
    root
}

fn write_file(root: &Path, relative_path: &str, contents: &str) {
    let path = root.join(relative_path);
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).expect("create parent directories");
    }
    fs::write(path, contents).expect("write fixture file");
}

fn diagnostic_codes(diagnostics: &[voicegroup_core::parser::Diagnostic]) -> Vec<&str> {
    diagnostics
        .iter()
        .map(|diagnostic| diagnostic.code.as_str())
        .collect()
}

// Builds the minimal symbol index needed to validate a fixture voicegroup structurally.
fn direct_sound_data_for_voicegroup(source: &str) -> String {
    let mut symbols = BTreeSet::new();
    for line in source.lines() {
        for token in
            line.split(|character: char| character != '_' && !character.is_ascii_alphanumeric())
        {
            if token.starts_with("DirectSoundWaveData_") {
                symbols.insert(token);
            }
        }
    }

    let mut output = String::new();
    for symbol in symbols {
        output.push_str(symbol);
        output.push_str("::\n\t.incbin \"sound/direct_sound_samples/");
        output.push_str(symbol.trim_start_matches("DirectSoundWaveData_"));
        output.push_str(".bin\"\n");
    }
    output
}

#[test]
fn indexes_standard_project_and_loads_monolithic_program_bank() {
    let root = temp_project("standard");
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        "\
DirectSoundWaveData_Kick::
\t.incbin \"sound/direct_sound_samples/kick.bin\"
DirectSoundWaveData_Cry::
\t@ comment between label and incbin must not drop the pending symbol
\t.incbin \"sound/direct_sound_samples/cry.bin\"
",
    );
    write_file(
        &root,
        "sound/programmable_wave_data.inc",
        "\
ProgrammableWaveData_Pulse::
\t.incbin \"sound/programmable_wave_samples/pulse.pcm\"
",
    );
    write_file(
        &root,
        "sound/keysplit_tables.inc",
        "\
keysplit strings, 0
split 1, 64
split 2, 128
",
    );
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
route104::
\tvoice_directsound 60, 0, DirectSoundWaveData_Kick, 255, 0, 255, 242 @ Kick
\tvoice_programmable_wave 61, 0, ProgrammableWaveData_Pulse, 1, 2, 8, 3
\tvoice_keysplit voicegroup_strings, keysplit_strings
\tcry DirectSoundWaveData_Cry

voicegroup_strings::
\tvoice_noise 60, 0, 0, 1, 2, 8, 3
",
    );

    let index = ProjectIndex::load(&root).expect("load project index");

    assert_eq!(
        index
            .direct_sound_assets()
            .map(|asset| (asset.symbol.as_str(), asset.relative_path.as_str()))
            .collect::<Vec<_>>(),
        [
            (
                "DirectSoundWaveData_Cry",
                "sound/direct_sound_samples/cry.bin"
            ),
            (
                "DirectSoundWaveData_Kick",
                "sound/direct_sound_samples/kick.bin"
            ),
        ]
    );
    assert_eq!(
        index
            .programmable_wave_assets()
            .map(|asset| (asset.symbol.as_str(), asset.relative_path.as_str()))
            .collect::<Vec<_>>(),
        [(
            "ProgrammableWaveData_Pulse",
            "sound/programmable_wave_samples/pulse.pcm"
        )]
    );
    assert_eq!(
        index.keysplit_table("keysplit_strings").expect("keysplit")[0],
        1
    );
    assert_eq!(
        index.keysplit_table("keysplit_strings").expect("keysplit")[63],
        1
    );
    assert_eq!(
        index.keysplit_table("keysplit_strings").expect("keysplit")[64],
        2
    );

    let result = index.load_program_bank("route104");
    let bank = result.bank.expect("bank should load");

    assert_eq!(result.diagnostics, []);
    assert_eq!(bank.name, "route104");
    assert_eq!(bank.source_relative_path, "sound/voice_groups.inc");
    assert_eq!(
        bank.programs[0].as_ref().expect("slot 0").type_code,
        VoiceType::DirectSound
    );
    assert_eq!(
        bank.programs[1].as_ref().expect("slot 1").display_name,
        "pulse.pcm"
    );
    assert_eq!(
        bank.programs[2].as_ref().expect("slot 2").data,
        ProgramData::Keysplit(voicegroup_core::program_bank::KeysplitProgram {
            sub_voicegroup: "voicegroup_strings".to_string(),
            table_symbol: "keysplit_strings".to_string(),
            table: *index.keysplit_table("keysplit_strings").expect("keysplit"),
        })
    );

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn does_not_load_per_file_voicegroup_without_project_voice_groups_file() {
    let root = temp_project("per-file-ignored");
    write_file(
        &root,
        "sound/voicegroups/route104.inc",
        "\
voice_group route104
\tvoice_square_2 60, 0, 2, 1, 2, 8, 3
",
    );

    let index = ProjectIndex::load(&root).expect("load project index");
    let result = index.load_program_bank("route104");

    assert!(result.bank.is_none());
    assert_eq!(
        diagnostic_codes(&result.diagnostics),
        ["missing-voicegroup"]
    );

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn loads_per_file_voicegroups_referenced_by_project_include_table() {
    let root = temp_project("included-voicegroups");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
.include \"sound/voicegroups/petalburg.inc\"
.include \"sound/voicegroups/drumsets/petalburg.inc\"
",
    );
    write_file(
        &root,
        "sound/voicegroups/petalburg.inc",
        "\
voice_group petalburg
\tvoice_keysplit_all voicegroup_petalburg_drumset
",
    );
    write_file(
        &root,
        "sound/voicegroups/drumsets/petalburg.inc",
        "\
voice_group petalburg_drumset, 36
\tvoice_square_2 60, 0, 2, 1, 2, 8, 3
",
    );

    let index = ProjectIndex::load(&root).expect("load project index");

    let result = index.load_program_bank("petalburg");
    let bank = result.bank.expect("included bank should load");
    assert_eq!(result.diagnostics, []);
    assert_eq!(bank.name, "petalburg");
    assert_eq!(bank.source_relative_path, "sound/voicegroups/petalburg.inc");
    assert_eq!(
        bank.programs[0].as_ref().expect("slot 0").data,
        ProgramData::KeysplitAll(voicegroup_core::program_bank::KeysplitAllProgram {
            sub_voicegroup: "petalburg_drumset".to_string(),
        })
    );

    let drumset = index.load_program_bank("petalburg_drumset");
    let drumset_bank = drumset.bank.expect("included drumset should load");
    assert_eq!(drumset.diagnostics, []);
    assert_eq!(
        drumset_bank.source_relative_path,
        "sound/voicegroups/drumsets/petalburg.inc"
    );
    assert!(drumset_bank.programs[35].is_none());
    assert_eq!(
        drumset_bank.programs[36].as_ref().expect("slot 36").data,
        ProgramData::Square2(voicegroup_core::program_bank::Square2Program {
            key: 60,
            pan: 0,
            duty: 2,
            attack: 1,
            decay: 2,
            sustain: 8,
            release: 3,
        })
    );

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn voicegroup_definition_location_for_included_file_points_to_include_path() {
    let root = temp_project("included-definition");
    write_file(
        &root,
        "sound/voice_groups.inc",
        ".include \"sound/voicegroups/village_bridge.inc\"\n",
    );
    write_file(
        &root,
        "sound/voicegroups/village_bridge.inc",
        "\
voice_group village_bridge
\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
",
    );
    let index = ProjectIndex::load(&root).expect("load project index");

    let location = index
        .voicegroup_definition_location("village_bridge")
        .expect("definition location for included voicegroup");

    fs::remove_dir_all(root).expect("remove temp project");
    assert_eq!(location.relative_path, "sound/voice_groups.inc");
    assert_eq!(
        location.range,
        SourceRange {
            start: SourcePosition {
                line: 1,
                column: 11
            },
            end: SourcePosition {
                line: 1,
                column: 47
            },
        }
    );
}

#[test]
fn voicegroup_definition_location_for_combined_voicegroup_points_to_label() {
    let root = temp_project("combined-definition");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
village_bridge::
\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
",
    );
    let index = ProjectIndex::load(&root).expect("load project index");

    let location = index
        .voicegroup_definition_location("village_bridge")
        .expect("definition location for combined voicegroup");

    fs::remove_dir_all(root).expect("remove temp project");
    assert_eq!(location.relative_path, "sound/voice_groups.inc");
    assert_eq!(
        location.range,
        SourceRange {
            start: SourcePosition { line: 1, column: 1 },
            end: SourcePosition {
                line: 1,
                column: 15
            },
        }
    );
}

#[test]
fn keysplit_definition_location_points_to_emerald_header_name() {
    let root = temp_project("keysplit-definition");
    write_file(
        &root,
        "sound/keysplit_tables.inc",
        "\
keysplit strings, 0
split 1, 64
",
    );
    let index = ProjectIndex::load(&root).expect("load project index");

    let location = index
        .keysplit_definition_location("keysplit_strings")
        .expect("definition location for keysplit table");

    fs::remove_dir_all(root).expect("remove temp project");
    assert_eq!(location.relative_path, "sound/keysplit_tables.inc");
    assert_eq!(
        location.range,
        SourceRange {
            start: SourcePosition {
                line: 1,
                column: 10
            },
            end: SourcePosition {
                line: 1,
                column: 17
            },
        }
    );
}

#[test]
fn direct_sound_definition_location_points_to_sample_label() {
    let root = temp_project("directsound-definition");
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        "\
\t.align 2
DirectSoundWaveData_Kick:
\t.incbin \"sound/direct_sound_samples/kick.bin\"
",
    );
    let index = ProjectIndex::load(&root).expect("load project index");

    let location = index
        .direct_sound_definition_location("DirectSoundWaveData_Kick")
        .expect("definition location for direct sound sample");

    fs::remove_dir_all(root).expect("remove temp project");
    assert_eq!(location.relative_path, "sound/direct_sound_data.inc");
    assert_eq!(
        location.range,
        SourceRange {
            start: SourcePosition { line: 2, column: 1 },
            end: SourcePosition {
                line: 2,
                column: 25
            },
        }
    );
}

#[test]
fn programmable_wave_definition_location_points_to_sample_label() {
    let root = temp_project("programmable-wave-definition");
    write_file(
        &root,
        "sound/programmable_wave_data.inc",
        "\
\t.align 2
ProgrammableWaveData_Pulse:
\t.incbin \"sound/programmable_wave_samples/pulse.pcm\"
",
    );
    let index = ProjectIndex::load(&root).expect("load project index");

    let location = index
        .programmable_wave_definition_location("ProgrammableWaveData_Pulse")
        .expect("definition location for programmable wave sample");

    fs::remove_dir_all(root).expect("remove temp project");
    assert_eq!(location.relative_path, "sound/programmable_wave_data.inc");
    assert_eq!(
        location.range,
        SourceRange {
            start: SourcePosition { line: 2, column: 1 },
            end: SourcePosition {
                line: 2,
                column: 27
            },
        }
    );
}

#[test]
fn definition_at_voice_groups_include_path_points_to_included_file() {
    let root = temp_project("include-navigation");
    write_file(
        &root,
        "sound/voice_groups.inc",
        ".include \"sound/voicegroups/village_bridge.inc\"\n",
    );
    write_file(
        &root,
        "sound/voicegroups/village_bridge.inc",
        "\
voice_group village_bridge
\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
",
    );
    let index = ProjectIndex::load(&root).expect("load project index");

    let location = index
        .definition_at(
            "sound/voice_groups.inc",
            ".include \"sound/voicegroups/village_bridge.inc\"\n",
            SourcePosition {
                line: 1,
                column: 24,
            },
        )
        .expect("definition location for included voicegroup file");

    fs::remove_dir_all(root).expect("remove temp project");
    assert_eq!(
        location.relative_path,
        "sound/voicegroups/village_bridge.inc"
    );
    assert_eq!(
        location.range,
        SourceRange {
            start: SourcePosition { line: 1, column: 1 },
            end: SourcePosition { line: 1, column: 1 },
        }
    );
}

#[test]
fn definition_at_directsound_argument_points_to_data_label() {
    let root = temp_project("directsound-navigation");
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        "\
DirectSoundWaveData_Kick::
\t.incbin \"sound/direct_sound_samples/kick.bin\"
",
    );
    let index = ProjectIndex::load(&root).expect("load project index");
    let text = "\
voice_group route_one
\tvoice_directsound 60, 0, DirectSoundWaveData_Kick, 255, 0, 255, 242
";

    let location = index
        .definition_at(
            "sound/voicegroups/route_one.inc",
            text,
            SourcePosition {
                line: 2,
                column: 34,
            },
        )
        .expect("definition location for directsound sample");

    fs::remove_dir_all(root).expect("remove temp project");
    assert_eq!(location.relative_path, "sound/direct_sound_data.inc");
    assert_eq!(
        location.range,
        SourceRange {
            start: SourcePosition { line: 1, column: 1 },
            end: SourcePosition {
                line: 1,
                column: 25,
            },
        }
    );
}

#[test]
fn lists_project_index_voicegroups_without_directory_scanning() {
    let root = temp_project("listed-voicegroups");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
.include \"sound/voicegroups/petalburg.inc\"
",
    );
    write_file(
        &root,
        "sound/voicegroups/petalburg.inc",
        "\
voice_group petalburg
\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
",
    );
    write_file(
        &root,
        "sound/voicegroups/unlisted.inc",
        "\
voice_group unlisted
\tvoice_square_2 60, 0, 2, 1, 2, 8, 3
",
    );

    let index = ProjectIndex::load(&root).expect("load project index");

    assert_eq!(
        index.voicegroup_names().collect::<Vec<_>>(),
        vec!["petalburg"]
    );

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn lists_voicegroup_files_without_declaring_unincluded_banks() {
    let root = temp_project("voicegroup-files");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
.include \"sound/voicegroups/main.inc\"
",
    );
    write_file(
        &root,
        "sound/voicegroups/main.inc",
        "\
voice_group main
\tvoice_keysplit_all voicegroup_unincluded
",
    );
    write_file(
        &root,
        "sound/voicegroups/nested/drumset.inc",
        "\
voice_group drumset
\tvoice_noise 60, 0, 0, 1, 2, 8, 3
",
    );
    write_file(
        &root,
        "sound/voicegroups/unincluded.inc",
        "\
voice_group unincluded
\tvoice_square_2 60, 0, 2, 1, 2, 8, 3
",
    );
    write_file(
        &root,
        "sound/voicegroups/readme.txt",
        "not a voicegroup source\n",
    );
    write_file(
        &root,
        "sound/voicegroups/generated.s",
        "voice_group generated\n",
    );

    let index = ProjectIndex::load(&root).expect("load project index");

    assert_eq!(
        index.voicegroup_files().collect::<Vec<_>>(),
        vec![
            "sound/voicegroups/main.inc",
            "sound/voicegroups/nested/drumset.inc",
            "sound/voicegroups/unincluded.inc",
        ]
    );

    let result = index.load_program_bank("main");
    assert!(result.bank.is_some());
    assert_eq!(
        diagnostic_codes(&result.diagnostics),
        ["unknown-voicegroup-symbol"]
    );

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn loads_selected_bank_from_monolithic_voice_groups_file() {
    let root = temp_project("monolithic");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
\t.align 2
NotAVoiceGroup::
\t.byte 1, 2, 3

\t.align 2
voicegroup000::
\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3

\t.align 2
voicegroup001::
\tvoice_noise 61, 0, 1, 1, 2, 8, 3
",
    );

    let index = ProjectIndex::load(&root).expect("load project index");
    let result = index.load_program_bank("voicegroup001");
    let bank = result.bank.expect("monolithic bank should load");

    assert_eq!(result.diagnostics, []);
    assert_eq!(bank.name, "voicegroup001");
    assert_eq!(bank.source_relative_path, "sound/voice_groups.inc");
    assert_eq!(
        bank.programs[0].as_ref().expect("slot 0").data,
        ProgramData::Noise(voicegroup_core::program_bank::NoiseProgram {
            key: 61,
            pan: 0,
            period: 1,
            attack: 1,
            decay: 2,
            sustain: 8,
            release: 3,
        })
    );

    let data_label = index.load_program_bank("NotAVoiceGroup");
    assert!(data_label.bank.is_none());
    assert_eq!(
        diagnostic_codes(&data_label.diagnostics),
        ["missing-voicegroup"]
    );

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn selected_bank_loading_deduplicates_analyzer_and_builder_diagnostics() {
    let root = temp_project("diagnostics");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
broken::
\tvoice_directsound 60, 0, DirectSoundWaveData_Missing, 255, 0, 255, 242
",
    );

    let index = ProjectIndex::load(&root).expect("load project index");
    let result = index.load_program_bank("broken");

    assert!(result.bank.is_some());
    assert_eq!(
        diagnostic_codes(&result.diagnostics),
        ["unknown-directsound-symbol"]
    );

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn loads_included_assembly_labeled_voicegroup_file() {
    let root = temp_project("assembly-labeled-include");
    let voicegroup192 = include_str!("fixtures/voicegroup192.inc");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
.include \"sound/voicegroups/main.inc\"
.include \"sound/voicegroups/voicegroup192.inc\"
",
    );
    write_file(
        &root,
        "sound/voicegroups/main.inc",
        "\
voice_group main
\tvoice_keysplit_all voicegroup192
",
    );
    write_file(&root, "sound/voicegroups/voicegroup192.inc", voicegroup192);
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        &direct_sound_data_for_voicegroup(voicegroup192),
    );

    let index = ProjectIndex::load(&root).expect("load project index");
    let result = index.load_program_bank("main");

    let bank = result.bank.expect("referencing bank should load");
    assert_eq!(result.diagnostics, []);
    assert_eq!(
        bank.programs[0].as_ref().expect("slot 0").data,
        ProgramData::KeysplitAll(voicegroup_core::program_bank::KeysplitAllProgram {
            sub_voicegroup: "voicegroup192".to_string(),
        })
    );

    let labeled_result = index.load_program_bank("voicegroup192");
    let labeled_bank = labeled_result
        .bank
        .expect("assembly-labeled bank should load");
    assert_eq!(labeled_result.diagnostics, []);
    assert_eq!(labeled_bank.name, "voicegroup192");
    assert_eq!(
        labeled_bank.source_relative_path,
        "sound/voicegroups/voicegroup192.inc"
    );
    assert_eq!(
        labeled_bank.programs[0]
            .as_ref()
            .expect("slot 0")
            .macro_name
            .as_str(),
        "voice_directsound_no_resample"
    );
    assert_eq!(
        labeled_bank.programs[42].as_ref().expect("slot 42").data,
        ProgramData::Noise(voicegroup_core::program_bank::NoiseProgram {
            key: 86,
            pan: 0,
            period: 0,
            attack: 0,
            decay: 1,
            sustain: 0,
            release: 0,
        })
    );
    assert_eq!(
        labeled_bank.programs[113]
            .as_ref()
            .expect("slot 113")
            .macro_name
            .as_str(),
        "voice_directsound_no_resample"
    );

    fs::remove_dir_all(root).expect("remove temp project");
}
#[test]
fn reports_missing_selected_bank_without_fabricating_programs() {
    let root = temp_project("missing");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
existing::
\tvoice_square_2 60, 0, 2, 1, 2, 8, 3
",
    );

    let index = ProjectIndex::load(&root).expect("load project index");
    let result = index.load_program_bank("missing");

    assert!(result.bank.is_none());
    assert_eq!(
        diagnostic_codes(&result.diagnostics),
        ["missing-voicegroup"]
    );

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn test_monolithic_line_remapping() {
    let root = temp_project("remapping");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
@ comment line 1
@ comment line 2
route104::
\tvoice_directsound 60, 0, DirectSoundWaveData_Kick, 255, 0, 255, 242
\tvoice_square_2 61, 0, 0, 0, 10, 10, 1
",
    );
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        "DirectSoundWaveData_Kick::\n\t.incbin \"sound/direct_sound_samples/kick.bin\"\n",
    );

    let index = ProjectIndex::load(&root).unwrap();
    let result = index.load_program_bank("route104");

    // Assert voice_square_2 decay=10 is checked. It sits at line 5 of sound/voice_groups.inc
    let diag = result
        .diagnostics
        .iter()
        .find(|d| d.code == "integer-out-of-range")
        .unwrap();
    assert_eq!(diag.range.start.line, 5);

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn test_included_file_line_remapping() {
    let root = temp_project("included_remapping");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\t.include \"sound/voicegroups/hanabi.inc\"\n",
    );
    write_file(
        &root,
        "sound/voicegroups/hanabi.inc",
        "\
voice_group hanabi
\tvoice_square_2 60, 0, 0, 0, 10, 10, 1
",
    );

    let index = ProjectIndex::load(&root).unwrap();
    let result = index.load_program_bank("hanabi");

    let diag = result
        .diagnostics
        .iter()
        .find(|d| d.code == "integer-out-of-range")
        .unwrap();
    assert_eq!(diag.range.start.line, 2);

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn source_preview_uses_full_document_ranges_local_shadowing_and_overlay() {
    let root = temp_project("source-preview");
    write_file(
        &root,
        "sound/voice_groups.inc",
        ".include \"sound/voicegroups/child.inc\"\n",
    );
    write_file(
        &root,
        "sound/voicegroups/child.inc",
        "voice_group child\n\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n",
    );
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        "DiskSynth::\n\t.incbin \"sound/direct_sound_samples/disk.bin\"\n",
    );
    write_file(
        &root,
        "sound/keysplit_tables.inc",
        "keysplit preview_table, 0\nsplit 1, 128\n",
    );

    let index = ProjectIndex::load(&root).expect("load preview project");
    let mut overlay = voicegroup_core::program_bank::SynthOverlay::new();
    overlay.insert("PreviewSynth", [0x80, 2, 0, 0, 0, 0]);
    let source = "\
voice_group child
\tvoice_directsound 60, 0, PreviewSynth, 255, 0, 255, 165
voice_group preview
\tvoice_keysplit child, keysplit_preview_table
";

    let result = index.load_program_bank_source(
        "preview",
        "sound/voicegroups/preview.inc",
        source,
        Some(&overlay),
    );
    assert_eq!(result.diagnostics, []);
    let bank = result.bank.expect("preview bank");
    assert_eq!(bank.source_relative_path, "sound/voicegroups/preview.inc");
    assert_eq!(
        bank.programs[0].as_ref().expect("keysplit slot").data,
        ProgramData::Keysplit(voicegroup_core::program_bank::KeysplitProgram {
            sub_voicegroup: "child".to_string(),
            table_symbol: "keysplit_preview_table".to_string(),
            table: [1; 128],
        })
    );
    let monolithic = index.load_program_bank_source(
        "voicegroup_preview",
        "sound/voice_groups.inc",
        "voicegroup_preview::\n\tvoice_keysplit child, keysplit_preview_table\n",
        None,
    );
    assert!(monolithic.bank.is_some());
    assert_eq!(
        monolithic
            .bank
            .as_ref()
            .expect("monolithic preview")
            .source_relative_path,
        "sound/voice_groups.inc"
    );

    let invalid = index.load_program_bank_source(
        "preview",
        "sound/voicegroups/preview.inc",
        "voice_group preview\n\tvoice_directsound 60, 0, Missing, 255, 0, 255, 165\n",
        None,
    );
    let diagnostic = invalid
        .diagnostics
        .iter()
        .find(|diagnostic| diagnostic.code == "unknown-directsound-symbol")
        .expect("unknown symbol diagnostic");
    assert_eq!(
        diagnostic.scope,
        voicegroup_core::ast::DiagnosticScope::Slot
    );
    assert_eq!(diagnostic.slot, Some(0));
    assert_eq!(
        (
            diagnostic.source_path.as_deref(),
            diagnostic.range.start.line,
            diagnostic.range.start.column,
            diagnostic.range.end.line,
            diagnostic.range.end.column,
        ),
        (Some("sound/voicegroups/preview.inc"), 2, 27, 2, 34,)
    );

    fs::remove_dir_all(root).expect("remove preview project");
}

#[test]
fn catalog_owns_content_paths_dependencies_metadata_and_synth_aliases() {
    let root = temp_project("catalog-bulk");
    write_file(
        &root,
        "sound/voice_groups.inc",
        ".include \"sound/voicegroups/declared.inc\"\n.include \"sound/voicegroups/child.inc\"\n",
    );
    write_file(
        &root,
        "sound/voicegroups/declared.inc",
        "voice_group declared\n\
\tvoice_directsound 60, 0, Sample, 255, 0, 255, 165\n\
\tvoice_directsound 60, 0, SynthOne, 255, 0, 255, 165\n\
\tvoice_keysplit child, keysplit_declared\n\
\tvoice_keysplit_all child\n",
    );
    write_file(
        &root,
        "sound/voicegroups/child.inc",
        "voice_group child\n\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n",
    );
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        "Sample::\n\t.incbin \"sound/direct_sound_samples/sample.bin\"\n",
    );
    write_file(
        &root,
        "sound/direct_sound_synth_data.inc",
        "SynthOne::\n\tset_synth_50\n",
    );
    write_file(
        &root,
        "sound/programmable_wave_data.inc",
        "Wave::\n\t.incbin \"sound/programmable_wave_samples/wave.pcm\"\n",
    );
    write_file(
        &root,
        "sound/keysplit_tables.inc",
        "keysplit declared, 0\nsplit 1, 128\n",
    );

    let index = ProjectIndex::load(&root).expect("load catalog project");
    let catalog = index.catalog();
    let declared = catalog
        .entries
        .iter()
        .find(|entry| {
            entry.kind == voicegroup_core::catalog::CatalogEntryKind::VoiceGroup
                && entry.symbol == "voicegroup_declared"
        })
        .expect("voicegroup catalog row");
    assert_eq!(
        declared.source_path.as_deref(),
        Some("sound/voicegroups/declared.inc")
    );
    assert_eq!(
        declared.keysplit,
        Some(voicegroup_core::catalog::KeysplitCatalogPair {
            subgroup: "child".to_string(),
            table: "keysplit_declared".to_string(),
        })
    );
    assert_eq!(declared.drumkit.as_deref(), Some("child"));
    assert_eq!(
        declared.dependency_paths,
        vec!["sound/direct_sound_samples/sample.bin".to_string()]
    );
    assert!(catalog
        .dependency_paths
        .contains(&"sound/direct_sound_samples/sample.bin".to_string()));
    assert!(catalog
        .watch_paths
        .contains(&"sound/direct_sound_synth_data.inc".to_string()));
    let synth = catalog
        .entries
        .iter()
        .find(|entry| entry.symbol == "SynthOne")
        .expect("synth catalog row");
    assert_eq!(synth.synth_desc, Some([0x80, 2, 0, 0, 0, 0]));
    assert_eq!(
        catalog.typical_adsr_by_symbol.get("Sample"),
        Some(&[255, 0, 255, 165])
    );
    assert_eq!(
        catalog.typical_adsr_by_family.get("directsound"),
        Some(&[255, 0, 255, 165])
    );

    let bank = index
        .load_program_bank("declared")
        .bank
        .expect("declared bank");
    let synth_program = bank.programs[1].as_ref().expect("synth voice");
    assert_eq!(
        synth_program.data,
        ProgramData::DirectSound(voicegroup_core::program_bank::DirectSoundProgram {
            key: 60,
            pan: 0,
            sample_symbol: "SynthOne".to_string(),
            sample_relative_path: String::new(),
            sample_synth_desc: Some([0x80, 2, 0, 0, 0, 0]),
            attack: 255,
            decay: 0,
            sustain: 255,
            release: 165,
        })
    );
    fs::remove_dir_all(root).expect("remove catalog project");
}

#[test]
fn snapshot_succeeds_with_diagnostics_and_keeps_invalid_bank_visible() {
    let root = temp_project("catalog-invalid");
    write_file(
        &root,
        "sound/voice_groups.inc",
        ".include \"sound/voicegroups/broken.inc\"\n",
    );
    write_file(
        &root,
        "sound/voicegroups/broken.inc",
        "voice_group broken\n\tvoice_directsound 60, 0, Missing, 255, 0, 255, 165\n",
    );

    let index = ProjectIndex::load(&root).expect("load invalid catalog project");
    let snapshot = index.snapshot();

    assert!(snapshot.succeeded);
    assert!(snapshot.catalog.entries.iter().any(|entry| {
        entry.kind == voicegroup_core::catalog::CatalogEntryKind::VoiceGroup
            && entry.symbol == "voicegroup_broken"
    }));
    assert!(snapshot.diagnostics.iter().any(|diagnostic| {
        diagnostic.code == "unknown-directsound-symbol"
            && diagnostic.source_path.as_deref() == Some("sound/voicegroups/broken.inc")
    }));
    fs::remove_dir_all(root).expect("remove invalid catalog project");
}

#[test]
fn snapshot_succeeds_in_mixed_health_project_and_loads_healthy_bank() {
    let root = temp_project("catalog-mixed-health");
    write_file(
        &root,
        "sound/voice_groups.inc",
        ".include \"sound/voicegroups/healthy.inc\"\n\
.include \"sound/voicegroups/broken.inc\"\n",
    );
    write_file(
        &root,
        "sound/voicegroups/healthy.inc",
        "voice_group healthy\n\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n",
    );
    write_file(
        &root,
        "sound/voicegroups/broken.inc",
        "voice_group broken\n\tvoice_directsound 60, 0, Missing, 255, 0, 255, 165\n",
    );

    let index = ProjectIndex::load(&root).expect("load mixed-health project");
    let snapshot = index.snapshot();

    assert!(snapshot.succeeded);
    assert!(!snapshot.diagnostics.is_empty());
    for symbol in ["voicegroup_healthy", "voicegroup_broken"] {
        assert!(snapshot.catalog.entries.iter().any(|entry| {
            entry.kind == voicegroup_core::catalog::CatalogEntryKind::VoiceGroup
                && entry.symbol == symbol
        }));
    }
    assert!(snapshot.diagnostics.iter().any(|diagnostic| {
        diagnostic.code == "unknown-directsound-symbol"
            && diagnostic.source_path.as_deref() == Some("sound/voicegroups/broken.inc")
    }));

    let healthy = index.load_program_bank("healthy");
    assert!(healthy.bank.is_some());
    assert!(healthy.diagnostics.is_empty());

    let broken = index.load_program_bank("broken");
    let diagnostic = broken
        .diagnostics
        .iter()
        .find(|diagnostic| diagnostic.code == "unknown-directsound-symbol")
        .expect("unknown symbol diagnostic");
    assert_eq!(
        diagnostic.scope,
        voicegroup_core::ast::DiagnosticScope::Slot
    );
    assert_eq!(diagnostic.slot, Some(0));
    assert_eq!(
        diagnostic.source_path.as_deref(),
        Some("sound/voicegroups/broken.inc")
    );
    let broken_bank = broken.bank.expect("broken bank retains a partial bank");
    assert!(broken_bank.programs[0].is_none());

    fs::remove_dir_all(root).expect("remove mixed-health project");
}

#[test]
fn snapshot_ignores_non_candidate_binary_file_in_voicegroup_directory() {
    let root = temp_project("catalog-non-candidate-binary");
    write_file(
        &root,
        "sound/voice_groups.inc",
        ".include \"sound/voicegroups/healthy.inc\"\n",
    );
    write_file(
        &root,
        "sound/voicegroups/healthy.inc",
        "voice_group healthy\n\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n",
    );
    fs::write(root.join("sound/voicegroups/.DS_Store"), [0xff, 0xfe, 0xfd])
        .expect("write non-UTF-8 non-candidate file");

    let index = ProjectIndex::load(&root).expect("load project with non-candidate file");
    let snapshot = index.snapshot();

    assert!(snapshot.succeeded);
    assert!(snapshot.diagnostics.is_empty());
    assert!(snapshot.catalog.entries.iter().any(|entry| {
        entry.kind == voicegroup_core::catalog::CatalogEntryKind::VoiceGroup
            && entry.symbol == "voicegroup_healthy"
    }));

    let healthy = index.load_program_bank("healthy");
    assert!(healthy.bank.is_some());
    assert!(healthy.diagnostics.is_empty());

    fs::remove_dir_all(root).expect("remove non-candidate binary project");
}

fn assert_layout_diagnostic(
    root: PathBuf,
    source_path: &str,
    code: &str,
    expected_end: SourcePosition,
) {
    let index = ProjectIndex::load(&root).expect("load layout fixture");
    let snapshot = index.snapshot();

    assert!(snapshot.succeeded);
    assert_eq!(diagnostic_codes(&snapshot.diagnostics), [code]);
    let diagnostic = snapshot.diagnostics.first().expect("layout diagnostic");
    assert_eq!(diagnostic.source_path.as_deref(), Some(source_path));
    assert_eq!(
        diagnostic.range,
        SourceRange {
            start: SourcePosition { line: 1, column: 1 },
            end: expected_end,
        }
    );

    fs::remove_dir_all(root).expect("remove layout fixture");
}

#[test]
fn discovers_voicegroups_inc_as_an_alternate_monolithic_layout() {
    let root = temp_project("alternate-voicegroups");
    write_file(
        &root,
        "sound/voicegroups.inc",
        "alternate::\n\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n",
    );

    let index = ProjectIndex::load(&root).expect("load alternate monolithic project");
    let result = index.load_program_bank("alternate");
    let bank = result.bank.expect("alternate bank");

    assert_eq!(result.diagnostics, []);
    assert_eq!(bank.source_relative_path, "sound/voicegroups.inc");
    assert_eq!(
        bank.programs[0]
            .as_ref()
            .expect("alternate slot")
            .macro_name
            .as_str(),
        "voice_square_1"
    );
    let snapshot = index.snapshot();
    assert!(snapshot.succeeded);
    assert!(snapshot
        .catalog
        .watch_paths
        .contains(&"sound/voicegroups.inc".to_string()));

    fs::remove_dir_all(root).expect("remove alternate monolithic project");
}

#[test]
fn discovers_voicegroups_inc_as_an_alternate_include_table() {
    let root = temp_project("alternate-include-table");
    write_file(
        &root,
        "sound/voicegroups.inc",
        ".include \"sound/voicegroups/alternate.inc\"\n",
    );
    write_file(
        &root,
        "sound/voicegroups/alternate.inc",
        "voice_group alternate\n\tvoice_noise 60, 0, 0, 1, 2, 8, 3\n",
    );

    let index = ProjectIndex::load(&root).expect("load alternate include table");
    let result = index.load_program_bank("alternate");
    let bank = result.bank.expect("alternate included bank");

    assert_eq!(result.diagnostics, []);
    assert_eq!(bank.source_relative_path, "sound/voicegroups/alternate.inc");
    assert_eq!(
        bank.programs[0]
            .as_ref()
            .expect("alternate slot")
            .macro_name
            .as_str(),
        "voice_noise"
    );
    assert!(index.snapshot().succeeded);

    fs::remove_dir_all(root).expect("remove alternate include-table project");
}

#[test]
fn discovers_keysplit_tables_s_with_exact_table_content_and_watch_path() {
    let root = temp_project("keysplit-tables-s");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "route::\n\tvoice_keysplit voicegroup_child, keysplit_assembly\n\
child::\n\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n",
    );
    write_file(
        &root,
        "sound/keysplit_tables.s",
        ".set keysplit_assembly, . - 0\n.byte 7, 9\n",
    );

    let index = ProjectIndex::load(&root).expect("load assembly keysplit project");
    let table = index
        .keysplit_table("keysplit_assembly")
        .expect("assembly table");
    assert_eq!(table[0], 7);
    assert_eq!(table[1], 9);
    assert_eq!(table[2], 0);

    let snapshot = index.snapshot();
    assert!(snapshot.succeeded);
    assert!(snapshot
        .catalog
        .watch_paths
        .contains(&"sound/keysplit_tables.s".to_string()));

    fs::remove_dir_all(root).expect("remove assembly keysplit project");
}

#[test]
fn reports_unindexed_keysplit_suffix_probe_without_dropping_it() {
    let root = temp_project("keysplit-suffix");
    let source = "voice_group route\n\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n";
    write_file(&root, "sound/voicegroups/route_keysplit.inc", source);

    assert_layout_diagnostic(
        root,
        "sound/voicegroups/route_keysplit.inc",
        "unsupported-voicegroup-suffix",
        SourcePosition { line: 3, column: 1 },
    );
}

#[test]
fn reports_unindexed_drumset_suffix_probe_without_dropping_it() {
    let root = temp_project("drumset-suffix");
    let source = "voice_group route\n\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n";
    write_file(&root, "sound/voicegroups/route_drumset.inc", source);

    assert_layout_diagnostic(
        root,
        "sound/voicegroups/route_drumset.inc",
        "unsupported-voicegroup-suffix",
        SourcePosition { line: 3, column: 1 },
    );
}

#[test]
fn reports_unindexed_vg_prefix_probe_without_dropping_it() {
    let root = temp_project("eventide-prefix");
    let source = "voice_group route\n\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n";
    write_file(&root, "sound/voicegroups/vg_route.inc", source);

    assert_layout_diagnostic(
        root,
        "sound/voicegroups/vg_route.inc",
        "unsupported-vg-prefix",
        SourcePosition { line: 3, column: 1 },
    );
}

#[test]
fn reports_unindexed_assembly_voicegroup_file_without_dropping_it() {
    let root = temp_project("assembly-voicegroup");
    let source = "voice_group route\n\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n";
    write_file(&root, "sound/voicegroups/route.s", source);

    assert_layout_diagnostic(
        root,
        "sound/voicegroups/route.s",
        "unsupported-voicegroup-s",
        SourcePosition { line: 3, column: 1 },
    );
}

#[test]
fn reports_unindexed_sound_tree_voicegroup_at_depth_three() {
    let root = temp_project("sound-depth-three");
    let source = "voice_group route\n\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n";
    write_file(&root, "sound/legacy/one/two/route.inc", source);

    assert_layout_diagnostic(
        root,
        "sound/legacy/one/two/route.inc",
        "unsupported-sound-depth",
        SourcePosition { line: 3, column: 1 },
    );
}
