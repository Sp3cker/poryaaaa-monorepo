use std::ffi::{CStr, CString};
use std::fs;
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use voicegroup_core::c_api::{
    voicegroup_core_project_index_free, voicegroup_core_project_index_load,
    voicegroup_core_project_index_snapshot, voicegroup_core_project_snapshot_result_family_adsr,
    voicegroup_core_project_snapshot_result_free,
    voicegroup_core_project_snapshot_result_succeeded,
    voicegroup_core_project_snapshot_result_synth_macro_words, VoicegroupCoreFamilyAdsr,
    VoicegroupCoreProjectSnapshotResult, VoicegroupCoreStatus,
};
use voicegroup_core::project_index::ProjectIndex;

const ALIASES: [&str; 6] = [
    "set_synth_25",
    "set_synth_50",
    "set_synth_custom",
    "set_synth_pulse",
    "set_synth_saw",
    "set_synth_triangle",
];

fn temp_project(name: &str) -> PathBuf {
    let nonce = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("clock should be after epoch")
        .as_nanos();
    let root = std::env::temp_dir().join(format!("voicegroup-core-catalog-{name}-{nonce}"));
    fs::create_dir_all(&root).expect("create temp project");
    root
}

fn write_file(root: &Path, relative_path: &str, contents: &str) {
    let path = root.join(relative_path);
    fs::create_dir_all(path.parent().expect("fixture parent")).expect("create fixture parent");
    fs::write(path, contents).expect("write fixture");
}

fn macro_definitions() -> &'static str {
    ".macro set_synth_custom p1:req, p2:req, p3:req, p4:req\n.endm\n\
.macro set_synth_pulse\n.endm\n\
.macro set_synth_25\n.endm\n\
.macro set_synth_saw\n.endm\n\
.macro set_synth_50\n.endm\n\
.macro set_synth_triangle\n.endm\n"
}

#[test]
fn project_catalog_discovers_definitions_without_invocations() {
    let root = temp_project("definitions");
    write_file(&root, "asm/macros/music_voice.inc", macro_definitions());
    write_file(
        &root,
        "asm/macros/other.inc",
        "set_synth_pulse 1, 2, 3, 4\n\
.macro set_synth_500\n.endm\n\
.macro set_synth_custom_v2\n.endm\n\
.macro set_synth_sawtooth\n.endm\n\
@ .macro set_synth_triangle\n",
    );

    let index = ProjectIndex::load(&root).expect("load macro project");
    assert_eq!(
        index.synth_macro_words().collect::<Vec<_>>(),
        ALIASES.to_vec()
    );
    let catalog = index.catalog();
    assert_eq!(
        catalog.synth_macro_words,
        ALIASES
            .iter()
            .map(|alias| (*alias).to_string())
            .collect::<Vec<_>>()
    );
    assert!(catalog.watch_paths.contains(&"asm/macros".to_string()));
    assert!(catalog
        .watch_paths
        .contains(&"asm/macros/music_voice.inc".to_string()));
    assert!(catalog
        .watch_paths
        .contains(&"asm/macros/other.inc".to_string()));

    fs::remove_dir_all(root).expect("remove macro project");
}

#[test]
fn c_snapshot_exposes_sorted_family_adsr_and_macro_words_with_null_contract() {
    let root = temp_project("snapshot");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "main::\n\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3\n",
    );
    write_file(&root, "sound/direct_sound_data.inc", "");
    write_file(&root, "sound/programmable_wave_data.inc", "");
    write_file(&root, "sound/keysplit_tables.inc", "");
    write_file(&root, "asm/macros/music_voice.inc", macro_definitions());
    let root_c = CString::new(root.to_string_lossy().as_bytes()).expect("root CString");

    unsafe {
        let mut index = std::ptr::null_mut();
        assert_eq!(
            voicegroup_core_project_index_load(root_c.as_ptr(), &mut index),
            VoicegroupCoreStatus::Ok
        );
        let mut snapshot = std::ptr::null_mut();
        assert_eq!(
            voicegroup_core_project_index_snapshot(index, &mut snapshot),
            VoicegroupCoreStatus::Ok
        );
        assert!(voicegroup_core_project_snapshot_result_succeeded(snapshot));

        let mut family_count = 0;
        let families =
            voicegroup_core_project_snapshot_result_family_adsr(snapshot, &mut family_count);
        assert_eq!(family_count, 1);
        let families: &[VoicegroupCoreFamilyAdsr] =
            std::slice::from_raw_parts(families, family_count);
        assert_eq!(
            CStr::from_ptr(families[0].family).to_str().unwrap(),
            "square_1"
        );
        assert_eq!(families[0].adsr, [1, 2, 8, 3]);

        let mut macro_count = 0;
        let words =
            voicegroup_core_project_snapshot_result_synth_macro_words(snapshot, &mut macro_count);
        assert_eq!(macro_count, ALIASES.len());
        let words = std::slice::from_raw_parts(words, macro_count)
            .iter()
            .map(|word| CStr::from_ptr(*word).to_str().unwrap())
            .collect::<Vec<_>>();
        assert_eq!(words, ALIASES.to_vec());

        let null_result: *const VoicegroupCoreProjectSnapshotResult = std::ptr::null();
        let mut count = 99;
        assert!(
            voicegroup_core_project_snapshot_result_family_adsr(null_result, &mut count,).is_null()
        );
        assert_eq!(count, 0);
        count = 99;
        assert!(
            voicegroup_core_project_snapshot_result_synth_macro_words(null_result, &mut count,)
                .is_null()
        );
        assert_eq!(count, 0);

        voicegroup_core_project_snapshot_result_free(snapshot);
        voicegroup_core_project_index_free(index);
    }

    fs::remove_dir_all(root).expect("remove snapshot project");
}
