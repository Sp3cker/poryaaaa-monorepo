//! Rust shell for the poryaaaa CLAP plugin.
//!
//! The audio engine stays in C. This crate owns the CLAP/plugin-facing layer
//! and calls the engine through an explicit FFI seam as the migration advances.

mod config;
mod editor;
mod ffi;
mod midi_activity;
mod params;
mod plugin;
mod process;
mod runtime;
#[cfg(test)]
mod test_support;
mod voicegroup;

pub use config::PluginConfig;
pub use params::{PoryaaaaParams, PROGRAM_COUNT};
pub use plugin::PoryaaaaPlugin;

#[doc(hidden)]
mod clap {
    use nice_plug::wrapper::clap::{
        clap_host, clap_plugin, clap_plugin_descriptor, clap_plugin_entry, clap_plugin_factory,
        PluginDescriptor, Wrapper, CLAP_PLUGIN_FACTORY_ID, CLAP_VERSION,
    };
    use nice_plug::wrapper::setup_logger;
    use std::ffi::{c_void, CStr};
    use std::os::raw::c_char;
    use std::sync::{Arc, LazyLock};

    use super::*;

    static PLUGIN_DESCRIPTOR: LazyLock<PluginDescriptor> =
        LazyLock::new(PluginDescriptor::for_plugin::<PoryaaaaPlugin>);

    const CLAP_PLUGIN_FACTORY: clap_plugin_factory = clap_plugin_factory {
        get_plugin_count: Some(get_plugin_count),
        get_plugin_descriptor: Some(get_plugin_descriptor),
        create_plugin: Some(create_plugin),
    };

    unsafe extern "C" fn get_plugin_count(_factory: *const clap_plugin_factory) -> u32 {
        1
    }

    unsafe extern "C" fn get_plugin_descriptor(
        _factory: *const clap_plugin_factory,
        index: u32,
    ) -> *const clap_plugin_descriptor {
        if index == 0 {
            PLUGIN_DESCRIPTOR.clap_plugin_descriptor()
        } else {
            std::ptr::null()
        }
    }

    unsafe extern "C" fn create_plugin(
        _factory: *const clap_plugin_factory,
        host: *const clap_host,
        plugin_id: *const c_char,
    ) -> *const clap_plugin {
        if plugin_id.is_null() {
            return std::ptr::null();
        }
        let plugin_id = unsafe { CStr::from_ptr(plugin_id) };
        if plugin_id != PLUGIN_DESCRIPTOR.clap_id() {
            return std::ptr::null();
        }

        unsafe {
            (*Arc::into_raw(Wrapper::<PoryaaaaPlugin>::new(host)))
                .clap_plugin
                .as_ptr()
        }
    }

    pub extern "C" fn init(plugin_path: *const c_char) -> bool {
        crate::config::set_default_config_dir_from_clap_path(plugin_path);
        setup_logger();
        true
    }

    pub extern "C" fn deinit() {}

    pub extern "C" fn get_factory(factory_id: *const c_char) -> *const c_void {
        if !factory_id.is_null() && unsafe { CStr::from_ptr(factory_id) } == CLAP_PLUGIN_FACTORY_ID
        {
            &CLAP_PLUGIN_FACTORY as *const _ as *const c_void
        } else {
            std::ptr::null()
        }
    }

    #[unsafe(no_mangle)]
    #[used]
    pub static clap_entry: clap_plugin_entry = clap_plugin_entry {
        clap_version: CLAP_VERSION,
        init: Some(init),
        deinit: Some(deinit),
        get_factory: Some(get_factory),
    };
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::process::ProcessRuntime;
    use crate::test_support::{temp_project, write_file, TestInitContext};
    use std::fs;
    use std::path::PathBuf;
    use std::sync::{Arc, LazyLock, Mutex};
    use std::time::{Duration, Instant};

    static TEST_ENV_LOCK: LazyLock<Mutex<()>> = LazyLock::new(|| Mutex::new(()));

    fn test_async_executor() -> nice_plug::prelude::AsyncExecutor<PoryaaaaPlugin> {
        nice_plug::prelude::AsyncExecutor::new(Arc::new(|_| {}), Arc::new(|_| {}))
    }

    #[test]
    fn editor_factory_returns_editor_for_default_params() {
        let params = Arc::new(PoryaaaaParams::default());

        assert!(crate::editor::create_editor(params, test_async_executor()).is_some());
    }

    #[test]
    fn editor_advertises_and_applies_host_resize() {
        let params = Arc::new(PoryaaaaParams::default());
        let editor =
            crate::editor::create_editor(params.clone(), test_async_executor()).expect("editor");

        assert_eq!(
            editor.size(),
            (
                crate::params::DEFAULT_EDITOR_WIDTH,
                crate::params::DEFAULT_EDITOR_HEIGHT
            )
        );
        assert!(editor.resize_hint().can_resize);
        assert!(editor.set_size(800, 600));
        assert_eq!(editor.size(), (800, 600));

        assert!(editor.set_size(1, 1));
        assert_eq!(editor.size(), (420, 260));
    }

    #[test]
    fn editor_font_definitions_use_calamity_faces() {
        let fonts = crate::editor::calamity_font_definitions();

        assert!(fonts
            .font_data
            .contains_key(crate::editor::CALAMITY_REGULAR_FONT));
        assert!(fonts
            .font_data
            .contains_key(crate::editor::CALAMITY_BOLD_FONT));
        assert_eq!(
            fonts
                .families
                .get(&egui::FontFamily::Proportional)
                .and_then(|family| family.first())
                .map(String::as_str),
            Some(crate::editor::CALAMITY_REGULAR_FONT)
        );

        let bold_family = egui::FontFamily::Name(crate::editor::CALAMITY_BOLD_FAMILY.into());
        assert_eq!(
            fonts
                .families
                .get(&bold_family)
                .and_then(|family| family.first())
                .map(String::as_str),
            Some(crate::editor::CALAMITY_BOLD_FONT)
        );
    }

    #[test]
    fn midi_activity_light_holds_recent_counter_changes_briefly() {
        let mut light = crate::editor::MidiActivityLight::default();
        let now = Instant::now();

        assert!(!light.is_active(0, now));
        assert!(light.is_active(1, now));
        assert!(light.is_active(1, now + Duration::from_millis(90)));
        assert!(!light.is_active(
            1,
            now + crate::editor::MIDI_ACTIVITY_HOLD + Duration::from_millis(1)
        ));
        assert!(light.is_active(2, now + Duration::from_secs(1)));
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

    #[cfg(target_os = "macos")]
    #[test]
    fn clap_entry_path_uses_clap_bundle_parent_for_default_config() {
        let dir = crate::config::config_dir_from_clap_path_for_test(
            "/Users/me/Library/Audio/Plug-Ins/CLAP/poryaaaa.clap/Contents/MacOS/poryaaaa",
        )
        .expect("config dir");

        assert_eq!(dir, PathBuf::from("/Users/me/Library/Audio/Plug-Ins/CLAP"));
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
    #[test]
    fn plugin_initialize_creates_and_deactivate_drops_runtime() {
        use nice_plug::prelude::*;

        let mut plugin = PoryaaaaPlugin::default();
        let layout = PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0];
        let buffer_config = BufferConfig {
            sample_rate: 48_000.0,
            min_buffer_size: None,
            max_buffer_size: 512,
            process_mode: ProcessMode::Realtime,
        };
        let mut context = TestInitContext;

        assert!(plugin.initialize(&layout, &buffer_config, &mut context));
        assert!(plugin.has_runtime());
        assert!(!plugin.runtime_has_loaded_voicegroup());

        plugin.deactivate();

        assert!(!plugin.has_runtime());
    }

    #[test]
    fn initialize_with_restored_committed_voicegroup_republishes_projects_json() {
        use nice_plug::prelude::*;

        let root = temp_project("initialize-restored-voicegroup");
        write_file(
            &root,
            "sound/voice_groups.inc",
            "\
                \t.align 2
                route104::
                \tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
            ",
        );
        let home = temp_project("initialize-home");
        let _env_lock = TEST_ENV_LOCK.lock().expect("test env lock");
        let old_home = std::env::var_os("HOME");
        std::env::set_var("HOME", &home);
        let projects_json_path =
            crate::voicegroup::default_projects_json_path().expect("default projects path");
        let _ = fs::remove_file(&projects_json_path);

        let mut plugin = PoryaaaaPlugin::default();
        *plugin
            .params_for_test()
            .project_root
            .write()
            .expect("project root write") = root.to_string_lossy().into();
        *plugin
            .params_for_test()
            .voicegroup
            .write()
            .expect("voicegroup write") = "route104".to_string();
        let layout = PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0];
        let buffer_config = BufferConfig {
            sample_rate: 48_000.0,
            min_buffer_size: None,
            max_buffer_size: 512,
            process_mode: ProcessMode::Realtime,
        };
        let mut context = TestInitContext;

        assert!(plugin.initialize(&layout, &buffer_config, &mut context));

        assert!(plugin.runtime_has_loaded_voicegroup());
        assert!(fs::read_to_string(&projects_json_path)
            .expect("projects.json emitted during initialize")
            .contains("\"bank\": \"route104\""));

        plugin.deactivate();
        if let Some(old_home) = old_home {
            std::env::set_var("HOME", old_home);
        } else {
            std::env::remove_var("HOME");
        }
        fs::remove_dir_all(root).expect("remove temp project");
        fs::remove_dir_all(home).expect("remove temp home");
    }

    #[test]
    fn initialize_exposes_voicegroup_load_error_to_editor_status() {
        use nice_plug::prelude::*;

        let root = temp_project("initialize-load-error");
        write_file(
            &root,
            "sound/voice_groups.inc",
            "\
                route104::
                    voice_directsound 60, 0, DirectSoundWaveData_Kick, 255, 0, 255, 242 @ Kick
            ",
        );
        let home = temp_project("initialize-load-error-home");
        let _env_lock = TEST_ENV_LOCK.lock().expect("test env lock");
        let old_home = std::env::var_os("HOME");
        std::env::set_var("HOME", &home);

        let mut plugin = PoryaaaaPlugin::default();
        *plugin
            .params_for_test()
            .project_root
            .write()
            .expect("project root write") = root.to_string_lossy().into();
        *plugin
            .params_for_test()
            .voicegroup
            .write()
            .expect("voicegroup write") = "route104".to_string();
        let layout = PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0];
        let buffer_config = BufferConfig {
            sample_rate: 48_000.0,
            min_buffer_size: None,
            max_buffer_size: 512,
            process_mode: ProcessMode::Realtime,
        };
        let mut context = TestInitContext;

        assert!(plugin.initialize(&layout, &buffer_config, &mut context));

        assert!(!plugin.runtime_has_loaded_voicegroup());
        let gui_state = crate::editor::GuiState::from_params(plugin.params_for_test());
        let status = gui_state
            .voicegroup_status
            .expect("runtime load error should be visible to editor");
        assert!(status.is_error);
        assert!(!status.text.is_empty());

        plugin.deactivate();
        if let Some(old_home) = old_home {
            std::env::set_var("HOME", old_home);
        } else {
            std::env::remove_var("HOME");
        }
        fs::remove_dir_all(root).expect("remove temp project");
        fs::remove_dir_all(home).expect("remove temp home");
    }
    #[test]
    fn load_voicegroup_task_swaps_active_runtime_without_reinitialize() {
        use nice_plug::prelude::*;

        let root = temp_project("runtime-task-load");
        write_file(
            &root,
            "sound/voice_groups.inc",
            "\
                \t.align 2
                voicegroup000::
                \tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
                route104::
                \tvoice_square_1 61, 0, 0, 2, 1, 2, 8, 3
            ",
        );
        let mut plugin = PoryaaaaPlugin::default();
        let layout = PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0];
        let buffer_config = BufferConfig {
            sample_rate: 48_000.0,
            min_buffer_size: None,
            max_buffer_size: 512,
            process_mode: ProcessMode::Realtime,
        };
        let mut context = TestInitContext;
        assert!(plugin.initialize(&layout, &buffer_config, &mut context));

        let execute = plugin.task_executor();
        execute(crate::plugin::PoryaaaaBackgroundTask::LoadVoicegroup {
            project_root: root.to_string_lossy().into_owned(),
            bank: "voicegroup000".to_owned(),
            projects_json_path: root.join("out/projects.json"),
        });
        assert!(plugin.runtime_has_loaded_voicegroup());

        execute(crate::plugin::PoryaaaaBackgroundTask::LoadVoicegroup {
            project_root: root.to_string_lossy().into_owned(),
            bank: "route104".to_owned(),
            projects_json_path: root.join("out/projects.json"),
        });

        assert!(plugin.runtime_has_loaded_voicegroup());
        assert_eq!(
            plugin
                .params_for_test()
                .voicegroup
                .read()
                .expect("voicegroup read")
                .as_str(),
            "route104",
        );

        fs::remove_dir_all(root).expect("remove temp project");
    }

    #[test]
    fn load_voicegroup_task_failure_keeps_loaded_runtime_and_committed_params() {
        use nice_plug::prelude::*;

        let root = temp_project("runtime-task-load-failure");
        write_file(
            &root,
            "sound/voice_groups.inc",
            "\
                \t.align 2
                voicegroup000::
                \tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
            ",
        );
        let mut plugin = PoryaaaaPlugin::default();
        let layout = PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0];
        let buffer_config = BufferConfig {
            sample_rate: 48_000.0,
            min_buffer_size: None,
            max_buffer_size: 512,
            process_mode: ProcessMode::Realtime,
        };
        let mut context = TestInitContext;
        assert!(plugin.initialize(&layout, &buffer_config, &mut context));

        let execute = plugin.task_executor();
        execute(crate::plugin::PoryaaaaBackgroundTask::LoadVoicegroup {
            project_root: root.to_string_lossy().into_owned(),
            bank: "voicegroup000".to_owned(),
            projects_json_path: root.join("out/projects.json"),
        });
        assert!(plugin.runtime_has_loaded_voicegroup());

        execute(crate::plugin::PoryaaaaBackgroundTask::LoadVoicegroup {
            project_root: "/definitely/not/a/poryaaaa/project".to_owned(),
            bank: "voicegroup000".to_owned(),
            projects_json_path: root.join("out/projects.json"),
        });

        assert!(plugin.runtime_has_loaded_voicegroup());
        assert_eq!(
            plugin
                .params_for_test()
                .voicegroup
                .read()
                .expect("voicegroup read")
                .as_str(),
            "voicegroup000",
        );

        fs::remove_dir_all(root).expect("remove temp project");
    }

    #[test]
    fn plugin_reset_keeps_initialized_runtime() {
        use nice_plug::prelude::*;

        let mut plugin = PoryaaaaPlugin::default();
        let layout = PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0];
        let buffer_config = BufferConfig {
            sample_rate: 48_000.0,
            min_buffer_size: None,
            max_buffer_size: 512,
            process_mode: ProcessMode::Realtime,
        };
        let mut context = TestInitContext;

        assert!(plugin.initialize(&layout, &buffer_config, &mut context));

        plugin.reset();

        assert!(plugin.has_runtime());
    }

    #[test]
    fn c_runtime_creates_resets_and_drops_engine() {
        let config = crate::runtime::EngineConfig {
            sample_rate: 48_000.0,
            volume: 127,
            reverb: 0,
        };
        let mut runtime = crate::runtime::M4aEngine::new(config).expect("runtime");

        assert!(runtime.reset().is_ok());
        assert!(!runtime.is_ready());
    }

    #[test]
    fn failed_runtime_voicegroup_load_keeps_no_loaded_voicegroup() {
        let config = crate::runtime::EngineConfig {
            sample_rate: 48_000.0,
            volume: 127,
            reverb: 0,
        };
        let mut runtime = crate::runtime::M4aEngine::new(config).expect("runtime");

        let result = runtime.load_voicegroup("/definitely/not/a/poryaaaa/project", "voicegroup000");

        assert!(result.is_err());
        assert!(!runtime.is_ready());
    }

    #[test]
    fn failed_runtime_voicegroup_replacement_keeps_loaded_voicegroup() {
        let root = temp_project("runtime-replace");
        write_file(
            &root,
            "sound/voice_groups.inc",
            "\
                \t.align 2
                voicegroup000::
                \tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
            ",
        );
        let config = crate::runtime::EngineConfig {
            sample_rate: 48_000.0,
            volume: 127,
            reverb: 0,
        };
        let mut runtime = crate::runtime::M4aEngine::new(config).expect("runtime");
        runtime
            .load_voicegroup(&root.to_string_lossy(), "voicegroup000")
            .expect("initial load");

        let result = runtime.load_voicegroup("/definitely/not/a/poryaaaa/project", "voicegroup000");

        assert!(result.is_err());
        assert!(runtime.is_ready());
        assert!(runtime.reset().is_ok());
        assert!(runtime.is_ready());

        runtime.program_change(0, 0);
        runtime.cc(0, 7, 127);
        runtime.cc(0, 10, 64);
        runtime.note_on(0, 60, 100);
        let mut peak = 0.0f32;
        for _ in 0..8 {
            let mut left = [0.0f32; 512];
            let mut right = [0.0f32; 512];
            runtime.process(&mut left, &mut right);
            peak = left
                .iter()
                .chain(right.iter())
                .fold(peak, |peak, sample| peak.max(sample.abs()));
        }
        assert!(peak > 0.0001);

        fs::remove_dir_all(root).expect("remove temp project");
    }
}
