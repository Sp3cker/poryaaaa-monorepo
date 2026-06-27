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
    fn editor_advertises_and_applies_host_resize() {
        let params = Arc::new(PoryaaaaParams::default());
        let editor = crate::editor::create_editor(params.clone()).expect("editor");

        assert!(editor.resize_hint().can_resize);
        assert!(editor.set_size(800, 600));
        assert_eq!(editor.size(), (800, 600));

        assert!(editor.set_size(1, 1));
        assert_eq!(editor.size(), (420, 260));
    }

    #[test]
    fn directory_picker_selection_updates_draft_project_root_only() {
        let params = PoryaaaaParams::default();
        let root = temp_project("selected-root");
        let mut gui_state = crate::editor::GuiState::from_params(&params);

        crate::editor::apply_project_root_selection(&mut gui_state, &root);

        assert_eq!(gui_state.draft_project_root, root.to_string_lossy());
        assert_eq!(
            params
                .project_root
                .read()
                .expect("project root read")
                .as_str(),
            ""
        );

        fs::remove_dir_all(root).expect("remove temp project");
    }
    #[test]
    fn editor_frame_expands_to_resizable_window_area() {
        let ctx = egui::Context::default();

        let _ = ctx.run_ui(
            egui::RawInput {
                screen_rect: Some(egui::Rect::from_min_size(
                    egui::Pos2::ZERO,
                    egui::Vec2::new(640.0, 480.0),
                )),
                ..Default::default()
            },
            |ui| {
                let available = ui.available_rect_before_wrap();
                let response = crate::editor::show_editor_frame(ui, |ui| {
                    ui.label("content");
                })
                .response;

                assert!((response.rect.width() - available.width()).abs() <= 1.0);
                assert!((response.rect.height() - available.height()).abs() <= 1.0);
            },
        );
    }

    #[test]
    fn remaining_width_for_leading_field_reserves_trailing_control() {
        assert_eq!(
            crate::editor::remaining_width_for_leading_field(900.0, 72.0, 8.0),
            820.0
        );
        assert_eq!(
            crate::editor::remaining_width_for_leading_field(40.0, 72.0, 8.0),
            0.0
        );
    }

    #[test]
    fn project_root_selector_text_field_expands_with_window_width() {
        let ctx = egui::Context::default();
        let mut project_root = "/tmp/poryaaaa-project".to_string();

        let _ = ctx.run_ui(
            egui::RawInput {
                screen_rect: Some(egui::Rect::from_min_size(
                    egui::Pos2::ZERO,
                    egui::Vec2::new(900.0, 260.0),
                )),
                ..Default::default()
            },
            |ui| {
                let available = ui.available_width();
                let response = crate::editor::show_project_root_selector(ui, &mut project_root);

                assert!(
                    response.text_rect.width() > available * 0.75,
                    "project root text field width {} did not track available width {available}",
                    response.text_rect.width()
                );
            },
        );
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
    fn plugin_load_success_commits_params_and_emits_projects_json() {
        let root = temp_project("voicegroup-load-success");
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
        let params = PoryaaaaParams::default();
        let projects_json_path = root.join("out/projects.json");

        let status = PoryaaaaPlugin::load_voicegroup(
            &params,
            &root.to_string_lossy(),
            "route104",
            &projects_json_path,
        );

        assert!(!status.is_error);
        assert_eq!(status.text, "Loaded route104");
        assert_eq!(
            params
                .project_root
                .read()
                .expect("project root read")
                .as_str(),
            root.to_string_lossy()
        );
        assert_eq!(
            params.voicegroup.read().expect("voicegroup read").as_str(),
            "route104"
        );
        assert!(fs::read_to_string(&projects_json_path)
            .expect("projects.json emitted")
            .contains("\"bank\": \"route104\""));

        fs::remove_dir_all(root).expect("remove temp project");
    }

    #[test]
    fn restored_committed_voicegroup_loads_projects_json() {
        let root = temp_project("restored-voicegroup-load");
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
        let params = PoryaaaaParams::default();
        *params.project_root.write().expect("project root write") = root.to_string_lossy().into();
        *params.voicegroup.write().expect("voicegroup write") = "route104".to_string();
        let projects_json_path = root.join("out/projects.json");

        let status = PoryaaaaPlugin::load_committed_voicegroup(&params, &projects_json_path)
            .expect("committed state should request a load");

        assert!(!status.is_error);
        assert_eq!(status.text, "Loaded route104");
        assert!(fs::read_to_string(&projects_json_path)
            .expect("projects.json emitted from restored state")
            .contains("\"bank\": \"route104\""));

        fs::remove_dir_all(root).expect("remove temp project");
    }

    #[test]
    fn plugin_load_failure_preserves_committed_params_and_projects_json() {
        let root = temp_project("voicegroup-load-failure");
        write_file(
            &root,
            "sound/voice_groups.inc",
            "\
route104::
    voice_directsound 60, 0, DirectSoundWaveData_Missing, 255, 0, 255, 242 @ Kick
",
        );
        let projects_json_path = root.join("out/projects.json");
        write_file(
            &root,
            "out/projects.json",
            "{\"root\":\"old\",\"bank\":\"old\",\"slots\":[]}",
        );
        let params = PoryaaaaParams::default();
        *params.project_root.write().expect("project root write") = "/committed/root".to_string();
        *params.voicegroup.write().expect("voicegroup write") = "committed_bank".to_string();

        let status = PoryaaaaPlugin::load_voicegroup(
            &params,
            &root.to_string_lossy(),
            "route104",
            &projects_json_path,
        );

        assert!(status.is_error);
        assert!(status.text.contains("unknown-directsound-symbol"));
        assert_eq!(
            params
                .project_root
                .read()
                .expect("project root read")
                .as_str(),
            "/committed/root"
        );
        assert_eq!(
            params.voicegroup.read().expect("voicegroup read").as_str(),
            "committed_bank"
        );
        assert_eq!(
            fs::read_to_string(&projects_json_path).expect("projects.json preserved"),
            "{\"root\":\"old\",\"bank\":\"old\",\"slots\":[]}"
        );

        fs::remove_dir_all(root).expect("remove temp project");
    }

    #[test]
    fn gui_state_initializes_drafts_from_committed_params() {
        let params = PoryaaaaParams::default();
        *params.project_root.write().expect("project root write") = "/committed/root".to_string();
        *params.voicegroup.write().expect("voicegroup write") = "committed_bank".to_string();

        let gui_state = crate::editor::GuiState::from_params(&params);

        assert_eq!(gui_state.draft_project_root, "/committed/root");
        assert_eq!(gui_state.draft_voicegroup, "committed_bank");
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
