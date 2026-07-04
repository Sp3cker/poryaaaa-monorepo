//! Plugin-facing Load tests for validation plus projects.json emission.

use std::fs;
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use serde_json::Value;
use voicegroup_core::plugin_load::load_for_plugin;

fn temp_project(name: &str) -> PathBuf {
    let nonce = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("clock should be after epoch")
        .as_nanos();
    let root = std::env::temp_dir().join(format!("voicegroup-core-plugin-load-{name}-{nonce}"));
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

#[test]
fn valid_bank_emits_ccomidi_projects_json_shape() {
    let root = temp_project("valid-json");
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        "\
DirectSoundWaveData_Kick::
\t.incbin \"sound/direct_sound_samples/kick.bin\"
DirectSoundWaveData_Snare::
\t.incbin \"sound/direct_sound_samples/snare.bin\"
",
    );
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
main::
\tvoice_directsound 60, 0, DirectSoundWaveData_Kick, 255, 0, 255, 242 @ Kick
\tvoice_keysplit_all voicegroup_main_drumset @ Drums

main_drumset::
\tvoice_group main_drumset, 36
\tvoice_directsound 60, 0, DirectSoundWaveData_Kick, 255, 0, 255, 242 @ Kick
\tvoice_directsound 60, 0, DirectSoundWaveData_Snare, 255, 0, 255, 242 @ Snare
",
    );
    let projects_json_path = root.join("out/projects.json");

    load_for_plugin(&root, "main", &projects_json_path).expect("valid load commits");

    let json = read_projects_json(&projects_json_path);
    assert_eq!(json["root"], root.to_string_lossy().as_ref());
    assert_eq!(json["bank"], "main");

    let slots = json["slots"].as_array().expect("slots array");
    assert_eq!(slots.len(), 2);
    assert_eq!(slots[0]["program"], 0);
    assert_eq!(slots[0]["name"], "kick.bin");
    assert_eq!(slots[0]["typeCode"], 0);
    assert!(slots[0].get("drumset").is_none());

    assert_eq!(slots[1]["program"], 1);
    assert_eq!(slots[1]["name"], "Drums");
    assert_eq!(slots[1]["typeCode"], 128);
    let drumset = slots[1]["drumset"].as_array().expect("drumset array");
    assert_eq!(drumset.len(), 2);
    assert_eq!(drumset[0]["note"], 36);
    assert_eq!(drumset[0]["name"], "kick.bin");
    assert_eq!(drumset[1]["note"], 37);
    assert_eq!(drumset[1]["name"], "snare.bin");

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn validation_failure_preserves_existing_projects_json() {
    let root = temp_project("preserve-existing");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
broken::
\tvoice_directsounnd 60, 0, DirectSoundWaveData_Missing, 255, 0, 255, 242
",
    );
    let projects_json_path = root.join("out/projects.json");
    write_file(
        &root,
        "out/projects.json",
        "{\"root\":\"old\",\"slots\":[]}",
    );

    let error = load_for_plugin(&root, "broken", &projects_json_path)
        .expect_err("invalid bank is rejected");

    assert!(error.contains("voice macro is not defined"));
    assert_eq!(
        fs::read_to_string(&projects_json_path).expect("existing projects.json remains"),
        "{\"root\":\"old\",\"slots\":[]}"
    );

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn structural_load_does_not_require_sample_file_bytes() {
    let root = temp_project("no-sample-bytes");
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        "\
DirectSoundWaveData_MissingFile::
\t.incbin \"sound/direct_sound_samples/missing.bin\"
",
    );
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
main::
\tvoice_directsound 60, 0, DirectSoundWaveData_MissingFile, 255, 0, 255, 242
",
    );
    let projects_json_path = root.join("out/projects.json");

    load_for_plugin(&root, "main", &projects_json_path)
        .expect("known sample symbol is structurally valid");

    let json = read_projects_json(&projects_json_path);
    assert_eq!(json["slots"][0]["name"], "missing.bin");

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn valid_load_replaces_existing_projects_json() {
    let root = temp_project("replace-existing");
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        "\
DirectSoundWaveData_Kick::
\t.incbin \"sound/direct_sound_samples/kick.bin\"
",
    );
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
main::
\tvoice_directsound 60, 0, DirectSoundWaveData_Kick, 255, 0, 255, 242
",
    );
    let projects_json_path = root.join("out/projects.json");
    write_file(
        &root,
        "out/projects.json",
        "{\"root\":\"old\",\"bank\":\"old\",\"slots\":[]}",
    );

    load_for_plugin(&root, "main", &projects_json_path).expect("valid load commits");

    let json = read_projects_json(&projects_json_path);
    assert_eq!(json["root"], root.to_string_lossy().as_ref());
    assert_eq!(json["bank"], "main");
    assert_eq!(json["slots"].as_array().expect("slots array").len(), 1);

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn nested_drumset_validation_failure_preserves_existing_projects_json() {
    let root = temp_project("nested-drumset-failure");
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        "\
DirectSoundWaveData_Kick::
\t.incbin \"sound/direct_sound_samples/kick.bin\"
",
    );
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
main::
\tvoice_keysplit_all voicegroup_main_drumset @ Drums

main_drumset::
\tvoice_group main_drumset, 36
\tvoice_directsound 60, 0, DirectSoundWaveData_Missing, 255, 0, 255, 242
",
    );
    let projects_json_path = root.join("out/projects.json");
    write_file(
        &root,
        "out/projects.json",
        "{\"root\":\"old\",\"bank\":\"old\",\"slots\":[]}",
    );

    let error =
        load_for_plugin(&root, "main", &projects_json_path).expect_err("nested bank is rejected");

    assert_eq!(
        error,
        "line 6: unknown-directsound-symbol: DirectSound sample symbol is not declared in direct_sound_data.inc"
    );
    assert_eq!(
        fs::read_to_string(&projects_json_path).expect("existing projects.json remains"),
        "{\"root\":\"old\",\"bank\":\"old\",\"slots\":[]}"
    );

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn projects_json_escapes_string_values() {
    let root = temp_project("json-escaping");
    let quoted_root = root.join("quote\"slash\\control");
    fs::create_dir_all(&quoted_root).expect("create quoted root");
    write_file(
        &quoted_root,
        "sound/direct_sound_data.inc",
        "\
DirectSoundWaveData_Kick::
\t.incbin \"sound/direct_sound_samples/kick.bin\"
",
    );
    write_file(
        &quoted_root,
        "sound/voice_groups.inc",
        "\
main::
\tvoice_keysplit_all voicegroup_main_drumset @ Drums \"slash\\name

main_drumset::
\tvoice_group main_drumset, 36
\tvoice_directsound 60, 0, DirectSoundWaveData_Kick, 255, 0, 255, 242
",
    );
    let projects_json_path = quoted_root.join("out/projects.json");

    load_for_plugin(&quoted_root, "main", &projects_json_path).expect("valid load commits");

    let json = read_projects_json(&projects_json_path);
    assert_eq!(json["root"], quoted_root.to_string_lossy().as_ref());
    assert_eq!(json["slots"][0]["name"], "Drums \"slash\\name");

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn invalid_project_root_reports_root_error() {
    let root = std::env::temp_dir().join(format!(
        "voicegroup-core-missing-root-{}",
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock should be after epoch")
            .as_nanos()
    ));
    let projects_json_path = std::env::temp_dir().join("voicegroup-core-missing-root.json");

    let error = load_for_plugin(&root, "main", &projects_json_path).expect_err("root is rejected");

    assert!(error.starts_with("Bad project root:"));
}

#[test]
fn writer_failure_cleans_temp_file() {
    let root = temp_project("writer-cleanup");
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        "\
DirectSoundWaveData_Kick::
\t.incbin \"sound/direct_sound_samples/kick.bin\"
",
    );
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
main::
\tvoice_directsound 60, 0, DirectSoundWaveData_Kick, 255, 0, 255, 242
",
    );
    let projects_json_path = root.join("out/projects.json");
    fs::create_dir_all(&projects_json_path).expect("create conflicting directory");

    let error = load_for_plugin(&root, "main", &projects_json_path)
        .expect_err("directory target rejects write");

    assert!(error.starts_with("projects.json write failed:"));
    assert!(!root.join("out/projects.json.tmp").exists());

    fs::remove_dir_all(root).expect("remove temp project");
}

fn read_projects_json(path: &Path) -> Value {
    serde_json::from_str(&fs::read_to_string(path).expect("projects.json emitted"))
        .expect("projects.json is valid json")
}

#[test]
fn diagnostic_formatting_prepends_line_numbers() {
    let root = temp_project("formatting-lines");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
broken::
	voice_directsounnd 60, 0, DirectSoundWaveData_Missing, 255, 0, 255, 242
",
    );
    let projects_json_path = root.join("out/projects.json");

    let error = load_for_plugin(&root, "broken", &projects_json_path)
        .expect_err("invalid bank is rejected");

    assert_eq!(
        error,
        "line 2: unknown-macro: voice macro is not defined in the macro catalog"
    );

    fs::remove_dir_all(root).expect("remove temp project");
}
