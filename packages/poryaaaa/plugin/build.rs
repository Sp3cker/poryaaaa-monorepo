use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR"));

    let mut build = cc::Build::new();
    build
        .std("c11")
        .warnings(true)
        .include(&manifest_dir)
        .include(manifest_dir.join("voicegroup"))
        .include(manifest_dir.join("../../../shared"))
        .include(manifest_dir.join("../../voicegroup-core/include"));

    if cfg!(target_env = "msvc") {
        build
            .define("_CRT_SECURE_NO_WARNINGS", None)
            .define("_CRT_NONSTDC_NO_WARNINGS", None);
    }

    for file in NATIVE_SOURCES {
        build.file(manifest_dir.join(file));
    }

    build.compile("poryaaaa_native_runtime");

    if !cfg!(target_env = "msvc") {
        println!("cargo:rustc-link-lib=m");
    }

    println!("cargo:rerun-if-changed=build.rs");
    for file in NATIVE_SOURCES.iter().chain(NATIVE_HEADERS) {
        println!(
            "cargo:rerun-if-changed={}",
            manifest_dir.join(file).display()
        );
    }
}

const NATIVE_SOURCES: &[&str] = &[
    "m4a_engine.c",
    "m4a_tables.c",
    "m4a/m4a_driver.c",
    "m4a/m4a_freq.c",
    "m4a/m4a_track.c",
    "m4a/m4a_cgb.c",
    "m4a/m4a_pcm.c",
    "m4a/m4a_main.c",
    "hw_audio/hw_audio.c",
    "hw_audio/hw_psg.c",
    "hw_audio/hw_pcm.c",
    "hw_audio/hw_mix.c",
    "hw_audio/hw_resample.c",
    "voicegroup/voicegroup_loader.c",
    "voicegroup/vg_log.c",
    "voicegroup/vg_paths.c",
    "voicegroup/vg_discovery.c",
    "voicegroup/vg_symbols.c",
    "voicegroup/vg_keysplit.c",
    "voicegroup/vg_source.c",
    "voicegroup/vg_wav.c",
    "voicegroup/vg_parser.c",
    "voicegroup/voicegroup_project_state.c",
];

const NATIVE_HEADERS: &[&str] = &[
    "m4a_engine.h",
    "m4a_tables.h",
    "m4a/m4a_driver.h",
    "m4a/m4a_freq.h",
    "m4a/m4a_internal.h",
    "m4a/m4a_pcm_ring.h",
    "m4a/m4a_register_file.h",
    "hw_audio/hw_audio.h",
    "hw_audio/hw_mix.h",
    "hw_audio/hw_pcm.h",
    "hw_audio/hw_psg.h",
    "hw_audio/hw_resample.h",
    "voicegroup/voicegroup_loader.h",
    "voicegroup/voicegroup_project_state.h",
    "voicegroup/voicegroup_types.h",
    "voicegroup/vg_alloc.h",
    "voicegroup/vg_discovery.h",
    "voicegroup/vg_keysplit.h",
    "voicegroup/vg_log.h",
    "voicegroup/vg_parser.h",
    "voicegroup/vg_paths.h",
    "voicegroup/vg_source.h",
    "voicegroup/vg_symbols.h",
    "voicegroup/vg_voice_macro.h",
    "voicegroup/vg_wav.h",
    "../../voicegroup-core/include/voicegroup_core.h",
    "../../../shared/projects_json_path.h",
];
