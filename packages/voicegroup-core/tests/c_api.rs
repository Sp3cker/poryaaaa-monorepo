//! C ABI tests for poryaaaa-facing opaque handles and primitive payload accessors.

use std::ffi::{CStr, CString};
use std::fs;
use std::os::raw::c_char;
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use voicegroup_core::c_api::{
    voicegroup_core_bank_result_diagnostic_code, voicegroup_core_bank_result_diagnostic_count,
    voicegroup_core_bank_result_diagnostic_range, voicegroup_core_bank_result_diagnostic_severity,
    voicegroup_core_bank_result_free, voicegroup_core_bank_result_has_bank,
    voicegroup_core_bank_result_program_direct_sound, voicegroup_core_bank_result_program_keysplit,
    voicegroup_core_bank_result_program_keysplit_table_symbol,
    voicegroup_core_bank_result_program_kind, voicegroup_core_bank_result_program_relative_path,
    voicegroup_core_bank_result_program_sub_voicegroup,
    voicegroup_core_bank_result_program_type_code, voicegroup_core_project_index_free,
    voicegroup_core_project_index_load, voicegroup_core_project_index_load_program_bank,
    VoicegroupCoreDiagnosticSeverity, VoicegroupCoreDirectSoundProgram,
    VoicegroupCoreKeysplitProgram, VoicegroupCoreProgramKind, VoicegroupCoreSourceRange,
    VoicegroupCoreStatus,
};

fn temp_project(name: &str) -> PathBuf {
    let nonce = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("clock should be after epoch")
        .as_nanos();
    let root = std::env::temp_dir().join(format!("voicegroup-core-c-api-{name}-{nonce}"));
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

unsafe fn copied_string<F>(copy: F) -> String
where
    F: FnOnce(*mut c_char, usize) -> usize,
{
    let mut buffer = vec![0; 256];
    let required = copy(buffer.as_mut_ptr(), buffer.len());
    assert!(required > 0);
    CStr::from_ptr(buffer.as_ptr())
        .to_string_lossy()
        .into_owned()
}

#[test]
fn c_api_loads_project_and_exposes_directsound_program_payload() {
    let root = temp_project("load");
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
\tvoice_directsound 60, 7, DirectSoundWaveData_Kick, 255, 0, 255, 242 @ Kick
\tvoice_keysplit voicegroup_sub_voicegroup, keysplit_drums

sub_voicegroup::
\tvoice_noise 60, 0, 0, 1, 2, 8, 3
",
    );
    write_file(
        &root,
        "sound/keysplit_tables.inc",
        "\
keysplit drums, 0
split 1, 64
split 2, 128
",
    );

    let root_c = CString::new(root.to_string_lossy().as_bytes()).expect("root CString");
    let mut index = std::ptr::null_mut();

    unsafe {
        assert_eq!(
            voicegroup_core_project_index_load(root_c.as_ptr(), &mut index),
            VoicegroupCoreStatus::Ok
        );
        assert!(!index.is_null());

        let bank_name = CString::new("main").expect("bank name CString");
        let mut result = std::ptr::null_mut();
        assert_eq!(
            voicegroup_core_project_index_load_program_bank(index, bank_name.as_ptr(), &mut result),
            VoicegroupCoreStatus::Ok
        );
        assert!(!result.is_null());
        assert!(voicegroup_core_bank_result_has_bank(result));
        assert_eq!(voicegroup_core_bank_result_diagnostic_count(result), 0);
        assert_eq!(
            voicegroup_core_bank_result_program_kind(result, 0),
            VoicegroupCoreProgramKind::DirectSound
        );
        assert_eq!(
            voicegroup_core_bank_result_program_type_code(result, 0),
            0x00
        );

        let mut directsound = VoicegroupCoreDirectSoundProgram::default();
        assert!(voicegroup_core_bank_result_program_direct_sound(
            result,
            0,
            &mut directsound,
        ));
        assert_eq!(directsound.key, 60);
        assert_eq!(directsound.pan, 7);
        assert_eq!(directsound.attack, 255);
        assert_eq!(directsound.release, 242);

        let relative_path = copied_string(|buffer, len| {
            voicegroup_core_bank_result_program_relative_path(result, 0, buffer, len)
        });
        assert_eq!(relative_path, "sound/direct_sound_samples/kick.bin");

        assert_eq!(
            voicegroup_core_bank_result_program_kind(result, 1),
            VoicegroupCoreProgramKind::Keysplit
        );
        let mut keysplit = VoicegroupCoreKeysplitProgram::default();
        assert!(voicegroup_core_bank_result_program_keysplit(
            result,
            1,
            &mut keysplit,
        ));
        assert_eq!(keysplit.table[0], 1);
        assert_eq!(keysplit.table[64], 2);
        let sub_voicegroup = copied_string(|buffer, len| {
            voicegroup_core_bank_result_program_sub_voicegroup(result, 1, buffer, len)
        });
        assert_eq!(sub_voicegroup, "sub_voicegroup");
        let table_symbol = copied_string(|buffer, len| {
            voicegroup_core_bank_result_program_keysplit_table_symbol(result, 1, buffer, len)
        });
        assert_eq!(table_symbol, "keysplit_drums");

        voicegroup_core_bank_result_free(result);
        voicegroup_core_project_index_free(index);
    }

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn c_api_clears_output_handles_on_argument_failures() {
    let mut index = std::ptr::dangling_mut();

    unsafe {
        assert_eq!(
            voicegroup_core_project_index_load(std::ptr::null(), &mut index),
            VoicegroupCoreStatus::NullArgument
        );
        assert!(index.is_null());
    }
}

#[test]
fn c_api_reports_missing_bank_diagnostics_without_rust_types() {
    let root = temp_project("missing");
    let root_c = CString::new(root.to_string_lossy().as_bytes()).expect("root CString");
    let mut index = std::ptr::null_mut();

    unsafe {
        assert_eq!(
            voicegroup_core_project_index_load(root_c.as_ptr(), &mut index),
            VoicegroupCoreStatus::Ok
        );

        let bank_name = CString::new("missing").expect("bank name CString");
        let mut result = std::ptr::null_mut();
        assert_eq!(
            voicegroup_core_project_index_load_program_bank(index, bank_name.as_ptr(), &mut result),
            VoicegroupCoreStatus::Ok
        );
        assert!(!voicegroup_core_bank_result_has_bank(result));
        assert_eq!(voicegroup_core_bank_result_diagnostic_count(result), 1);

        let code = copied_string(|buffer, len| {
            voicegroup_core_bank_result_diagnostic_code(result, 0, buffer, len)
        });
        assert_eq!(code, "missing-voicegroup");
        assert_eq!(
            voicegroup_core_bank_result_diagnostic_severity(result, 0),
            VoicegroupCoreDiagnosticSeverity::Error
        );
        let mut range = VoicegroupCoreSourceRange::default();
        assert!(voicegroup_core_bank_result_diagnostic_range(
            result, 0, &mut range,
        ));
        assert_eq!(range.start.line, 1);
        assert_eq!(range.start.column, 1);
        assert_eq!(range.end.line, 1);
        assert_eq!(range.end.column, 1);

        voicegroup_core_bank_result_free(result);
        voicegroup_core_project_index_free(index);
    }

    fs::remove_dir_all(root).expect("remove temp project");
}
