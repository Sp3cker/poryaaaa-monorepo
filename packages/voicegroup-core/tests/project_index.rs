//! ProjectIndex tests for project discovery, symbol indexing, and selected-bank loading.

use std::collections::BTreeSet;
use std::fs;
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

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
