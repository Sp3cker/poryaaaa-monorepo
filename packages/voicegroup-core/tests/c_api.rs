//! C ABI tests for poryaaaa-facing opaque handles and primitive payload accessors.

use std::ffi::{CStr, CString};
use std::fs;
use std::os::raw::c_char;
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use voicegroup_core::c_api::{
    voicegroup_core_abi_version, voicegroup_core_bank_result_diagnostic,
    voicegroup_core_bank_result_diagnostic_code, voicegroup_core_bank_result_diagnostic_count,
    voicegroup_core_bank_result_diagnostic_range, voicegroup_core_bank_result_free,
    voicegroup_core_bank_result_has_bank, voicegroup_core_bank_result_program_direct_sound,
    voicegroup_core_bank_result_program_keysplit,
    voicegroup_core_bank_result_program_keysplit_table_symbol,
    voicegroup_core_bank_result_program_kind, voicegroup_core_bank_result_program_relative_path,
    voicegroup_core_bank_result_program_sub_voicegroup,
    voicegroup_core_bank_result_program_type_code, voicegroup_core_project_index_bank_continuation,
    voicegroup_core_project_index_bank_continuation_count, voicegroup_core_project_index_free,
    voicegroup_core_project_index_keysplit_table, voicegroup_core_project_index_load,
    voicegroup_core_project_index_load_program_bank,
    voicegroup_core_project_index_load_program_bank_source, voicegroup_core_project_index_snapshot,
    voicegroup_core_project_snapshot_result_catalog,
    voicegroup_core_project_snapshot_result_content_paths,
    voicegroup_core_project_snapshot_result_dependency_paths,
    voicegroup_core_project_snapshot_result_diagnostics,
    voicegroup_core_project_snapshot_result_free,
    voicegroup_core_project_snapshot_result_succeeded,
    voicegroup_core_project_snapshot_result_watch_paths, voicegroup_core_synth_overlay_add,
    voicegroup_core_synth_overlay_create, voicegroup_core_synth_overlay_free,
    VoicegroupCoreCatalogEntryKind, VoicegroupCoreDiagnostic, VoicegroupCoreDiagnosticScope,
    VoicegroupCoreDirectSoundProgram, VoicegroupCoreKeysplitProgram, VoicegroupCoreProgramKind,
    VoicegroupCoreSourceRange, VoicegroupCoreStatus, VOICEGROUP_CORE_ABI_VERSION,
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
        assert_eq!(voicegroup_core_abi_version(), VOICEGROUP_CORE_ABI_VERSION);
        assert_eq!(
            voicegroup_core_project_index_load(root_c.as_ptr(), &mut index),
            VoicegroupCoreStatus::Ok
        );
        assert!(!index.is_null());
        let mut snapshot = std::ptr::null_mut();
        assert_eq!(
            voicegroup_core_project_index_snapshot(index, &mut snapshot),
            VoicegroupCoreStatus::Ok
        );
        assert!(voicegroup_core_project_snapshot_result_succeeded(snapshot));
        let mut catalog_count = 0;
        let catalog = voicegroup_core_project_snapshot_result_catalog(snapshot, &mut catalog_count);
        assert!(catalog_count >= 3);
        assert!(!catalog.is_null());
        let catalog = std::slice::from_raw_parts(catalog, catalog_count);
        let main = catalog
            .iter()
            .find(|entry| CStr::from_ptr(entry.symbol).to_str().unwrap() == "voicegroup_main")
            .expect("main catalog row");
        assert_eq!(main.kind, VoicegroupCoreCatalogEntryKind::VoiceGroup);
        assert_eq!(
            CStr::from_ptr(main.source_path).to_str().unwrap(),
            "sound/voice_groups.inc"
        );
        assert_eq!(main.dependency_path_count, 1);
        assert_eq!(
            CStr::from_ptr(*main.dependency_paths).to_str().unwrap(),
            "sound/direct_sound_samples/kick.bin"
        );
        let mut content_count = 0;
        let content_paths =
            voicegroup_core_project_snapshot_result_content_paths(snapshot, &mut content_count);
        assert!(content_count > 0);
        assert!(std::slice::from_raw_parts(content_paths, content_count)
            .iter()
            .any(|path| CStr::from_ptr(*path).to_str().unwrap() == "sound/voice_groups.inc"));
        let mut dependency_count = 0;
        let dependency_paths = voicegroup_core_project_snapshot_result_dependency_paths(
            snapshot,
            &mut dependency_count,
        );
        assert!(
            std::slice::from_raw_parts(dependency_paths, dependency_count)
                .iter()
                .any(|path| CStr::from_ptr(*path).to_str().unwrap()
                    == "sound/direct_sound_samples/kick.bin")
        );
        let mut snapshot_diagnostic_count = 0;
        assert!(voicegroup_core_project_snapshot_result_diagnostics(
            snapshot,
            &mut snapshot_diagnostic_count,
        )
        .is_null());
        assert_eq!(snapshot_diagnostic_count, 0);
        let mut watch_count = 0;
        let watch_paths =
            voicegroup_core_project_snapshot_result_watch_paths(snapshot, &mut watch_count);
        assert!(std::slice::from_raw_parts(watch_paths, watch_count)
            .iter()
            .any(|path| CStr::from_ptr(*path).to_str().unwrap()
                == "sound/direct_sound_samples/kick.bin"));
        voicegroup_core_project_snapshot_result_free(snapshot);

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
        assert_eq!(
            voicegroup_core_bank_result_program_keysplit_table_symbol(
                result,
                1,
                std::ptr::null_mut(),
                0,
            ),
            "keysplit_drums".len() + 1
        );
        let mut copied_table = [0u8; 128];
        assert!(voicegroup_core_project_index_keysplit_table(
            index,
            CString::new("keysplit_drums").unwrap().as_ptr(),
            copied_table.as_mut_ptr(),
        ));
        assert_eq!(copied_table, keysplit.table);

        let overlay = voicegroup_core_synth_overlay_create();
        assert!(!overlay.is_null());
        let synth_name = CString::new("PendingSynth").expect("synth name CString");
        let descriptor = [0x80, 0x12, 0x34, 0x56, 0x78, 0x9a];
        assert_eq!(
            voicegroup_core_synth_overlay_add(
                overlay,
                synth_name.as_ptr(),
                synth_name.as_bytes().len(),
                descriptor.as_ptr(),
            ),
            VoicegroupCoreStatus::Ok
        );
        let source_path =
            CString::new("sound/voicegroups/preview.inc").expect("source path CString");
        let source = CString::new(
            "voice_group preview\n\tvoice_directsound 60, 7, PendingSynth, 255, 0, 255, 242\n",
        )
        .expect("source CString");
        let source_bank = CString::new("preview").expect("source bank CString");
        let mut source_result = std::ptr::null_mut();
        assert_eq!(
            voicegroup_core_project_index_load_program_bank_source(
                index,
                source_path.as_ptr(),
                source_path.as_bytes().len(),
                source.as_ptr(),
                source.as_bytes().len(),
                source_bank.as_ptr(),
                source_bank.as_bytes().len(),
                overlay,
                &mut source_result,
            ),
            VoicegroupCoreStatus::Ok
        );
        assert!(voicegroup_core_bank_result_has_bank(source_result));
        assert_eq!(
            voicegroup_core_bank_result_diagnostic_count(source_result),
            0
        );
        let mut source_program = VoicegroupCoreDirectSoundProgram::default();
        assert!(voicegroup_core_bank_result_program_direct_sound(
            source_result,
            0,
            &mut source_program,
        ));
        assert!(source_program.has_synth);
        assert_eq!(source_program.synth_desc, descriptor);
        voicegroup_core_bank_result_free(source_result);
        voicegroup_core_synth_overlay_free(overlay);
        voicegroup_core_bank_result_free(result);
        voicegroup_core_project_index_free(index);
    }

    fs::remove_dir_all(root).expect("remove temp project");
}

#[test]
fn c_api_exposes_cry_and_keysplit_all_metadata() {
    let root = temp_project("metadata");
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        "\
DirectSoundWaveData_Cry::
\t.incbin \"sound/direct_sound_samples/cry.bin\"
",
    );
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
main::
\tvoice_keysplit_all voicegroup_drums
\tcry_reverse DirectSoundWaveData_Cry

drums::
\tvoice_noise 60, 0, 0, 1, 2, 8, 3
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
            VoicegroupCoreProgramKind::KeysplitAll
        );
        let sub_voicegroup = copied_string(|buffer, len| {
            voicegroup_core_bank_result_program_sub_voicegroup(result, 0, buffer, len)
        });
        assert_eq!(sub_voicegroup, "drums");

        assert_eq!(
            voicegroup_core_bank_result_program_kind(result, 1),
            VoicegroupCoreProgramKind::Cry
        );
        let relative_path = copied_string(|buffer, len| {
            voicegroup_core_bank_result_program_relative_path(result, 1, buffer, len)
        });
        assert_eq!(relative_path, "sound/direct_sound_samples/cry.bin");

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

        let invalid_utf8 = [0xff_u8 as c_char, 0];
        index = std::ptr::dangling_mut();
        assert_eq!(
            voicegroup_core_project_index_load(invalid_utf8.as_ptr(), &mut index),
            VoicegroupCoreStatus::InvalidUtf8
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
        let mut diagnostic = VoicegroupCoreDiagnostic::default();
        assert!(voicegroup_core_bank_result_diagnostic(
            result,
            0,
            &mut diagnostic,
        ));
        assert_eq!(diagnostic.scope, VoicegroupCoreDiagnosticScope::Structural);
        assert!(!diagnostic.has_range);
        assert!(!diagnostic.has_slot);
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

#[test]
fn c_api_continuations_preserve_order_and_output_ownership() {
    let root = temp_project("continuations");
    write_file(
        &root,
        "sound/voice_groups.inc",
        concat!(
            ".include \"sound/voicegroups/first.inc\"\n",
            ".include \"sound/voicegroups/second.inc\"\n",
        ),
    );
    write_file(
        &root,
        "sound/voicegroups/first.inc",
        "first::\n\tvoice_square_1 60, 0, 0, 0, 1, 2, 3, 4\n",
    );
    write_file(
        &root,
        "sound/voicegroups/second.inc",
        "second::\n\tvoice_square_2 61, 0, 1, 2, 3, 4, 5\n",
    );

    let root_c = CString::new(root.to_string_lossy().as_bytes()).expect("root CString");
    let bank_name = CString::new("first").expect("bank CString");
    let mut index = std::ptr::null_mut();
    unsafe {
        assert_eq!(
            voicegroup_core_project_index_load(root_c.as_ptr(), &mut index),
            VoicegroupCoreStatus::Ok
        );
        assert_eq!(
            voicegroup_core_project_index_bank_continuation_count(index, bank_name.as_ptr()),
            1
        );

        let mut next_bank = [0 as c_char; 64];
        let mut next_path = [0 as c_char; 128];
        assert!(voicegroup_core_project_index_bank_continuation(
            index,
            bank_name.as_ptr(),
            0,
            next_bank.as_mut_ptr(),
            next_bank.len(),
            next_path.as_mut_ptr(),
            next_path.len(),
        ));
        assert_eq!(
            CStr::from_ptr(next_bank.as_ptr()).to_str().unwrap(),
            "second"
        );
        assert_eq!(
            CStr::from_ptr(next_path.as_ptr()).to_str().unwrap(),
            "sound/voicegroups/second.inc"
        );

        let mut short_bank = [0x7f as c_char; 2];
        let mut untouched_path = [0x7f as c_char; 128];
        assert!(!voicegroup_core_project_index_bank_continuation(
            index,
            bank_name.as_ptr(),
            0,
            short_bank.as_mut_ptr(),
            short_bank.len(),
            untouched_path.as_mut_ptr(),
            untouched_path.len(),
        ));
        assert_eq!(short_bank, [0x7f as c_char; 2]);
        assert_eq!(untouched_path, [0x7f as c_char; 128]);

        voicegroup_core_project_index_free(index);
    }
    fs::remove_dir_all(root).expect("remove temp project");
}
