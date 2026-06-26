//! Rust shell for the poryaaaa CLAP plugin.
//!
//! The audio engine stays in C. This crate owns the CLAP/plugin-facing layer
//! and calls the engine through an explicit FFI seam as the migration advances.

mod config;
mod editor;
mod params;
mod plugin;
mod voicegroup;

pub use config::PluginConfig;
pub use params::{PoryaaaaParams, PROGRAM_COUNT};
pub use plugin::PoryaaaaPlugin;
pub use voicegroup::{probe_voicegroup_bank, VoicegroupProbeResult, VoicegroupProbeStatus};

nice_plug::nice_export_clap!(PoryaaaaPlugin);

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::path::{Path, PathBuf};
    use std::sync::Arc;
    use std::time::{SystemTime, UNIX_EPOCH};

    #[test]
    fn editor_factory_returns_editor_for_default_params() {
        let params = Arc::new(PoryaaaaParams::default());

        assert!(crate::editor::create_editor(params).is_some());
    }
    #[test]
    fn config_loads_first_pass_poryaaaa_cfg_keys() {
        let root = temp_project("config");
        write_file(
            &root,
            "poryaaaa.cfg",
            "\
# retained C config key, intentionally ignored by Rust
log=/tmp/poryaaaa.log
project_root=/pokeemerald
voicegroup=petalburg
reverb=17
volume=96
",
        );

        let config = crate::config::PluginConfig::load_from_dir(&root).expect("load config");

        assert_eq!(config.project_root, "/pokeemerald");
        assert_eq!(config.voicegroup, "petalburg");
        assert_eq!(config.reverb, 17);
        assert_eq!(config.volume, 96);

        fs::remove_dir_all(root).expect("remove temp project");
    }

    #[test]
    fn config_clamps_volume_and_reverb_to_m4a_range() {
        let root = temp_project("config-clamp");
        write_file(
            &root,
            "poryaaaa.cfg",
            "\
reverb=200
volume=255
",
        );

        let config = crate::config::PluginConfig::load_from_dir(&root).expect("load config");

        assert_eq!(config.reverb, 127);
        assert_eq!(config.volume, 127);

        fs::remove_dir_all(root).expect("remove temp project");
    }

    #[test]
    fn voicegroup_probe_loads_bank_with_typed_core_status() {
        let root = temp_project("voicegroup-probe");
        write_file(
            &root,
            "sound/direct_sound_data.inc",
            "\
DirectSoundWaveData_Kick::
    .incbin \"sound/direct_sound_samples/kick.bin\"
",
        );
        write_file(
            &root,
            "sound/voice_groups.inc",
            "\
route104::
    voice_directsound 60, 0, DirectSoundWaveData_Kick, 255, 0, 255, 242 @ Kick
",
        );
        let config = crate::config::PluginConfig {
            project_root: root.to_string_lossy().into_owned(),
            voicegroup: "route104".to_string(),
            ..Default::default()
        };

        let result = crate::voicegroup::probe_voicegroup_bank(&config).expect("probe voicegroup");

        assert_eq!(
            result.status,
            crate::voicegroup::VoicegroupProbeStatus::Loaded {
                bank_name: "route104".to_string(),
                source_relative_path: "sound/voice_groups.inc".to_string(),
                occupied_slots: 1,
            }
        );
        assert_eq!(result.diagnostics, []);

        fs::remove_dir_all(root).expect("remove temp project");
    }

    #[test]
    fn voicegroup_probe_does_not_scan_cwd_without_config() {
        let result = crate::voicegroup::probe_voicegroup_bank(&PluginConfig::default())
            .expect("probe default config");

        assert_eq!(
            result.status,
            crate::voicegroup::VoicegroupProbeStatus::NotConfigured
        );
        assert_eq!(result.diagnostics, []);
    }

    fn temp_project(name: &str) -> PathBuf {
        let nonce = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock should be after unix epoch")
            .as_nanos();
        let root = std::env::temp_dir().join(format!("poryaaaa-rs-{name}-{nonce}"));
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
}
