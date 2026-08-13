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
mod shared_projects_json;
#[cfg(test)]
mod test_support;

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
    use crate::test_support::{temp_project, write_file, IsolatedHome, TestInitContext};
    use std::fs;
    use std::path::PathBuf;
    use std::sync::Arc;

    fn test_async_executor() -> nice_plug::prelude::AsyncExecutor<PoryaaaaPlugin> {
        nice_plug::prelude::AsyncExecutor::new(Arc::new(|_| {}), Arc::new(|_| {}))
    }

    #[test]
    fn editor_factory_returns_editor_for_default_params() {
        let params = Arc::new(PoryaaaaParams::default());

        assert!(crate::editor::create_editor(
            params,
            test_async_executor(),
            nice_plug_iced::iced::PollSubNotifier::new()
        )
        .is_some());
    }

    #[test]
    fn params_restart_request_is_one_shot() {
        let params = PoryaaaaParams::default();

        assert!(!params.take_host_restart_request());
        params.request_host_restart();
        assert!(params.take_host_restart_request());
        assert!(!params.take_host_restart_request());
    }

    #[test]
    fn params_committed_voicegroup_helpers_read_and_write_state() {
        let params = PoryaaaaParams::default();

        assert_eq!(params.committed_voicegroup_selection(), None);
        assert_eq!(params.voicegroup_status(), None);

        params.commit_voicegroup_selection("/tmp/project", "voicegroup000");
        params.write_voicegroup_status(Some(crate::params::VoicegroupLoadStatus {
            text: "Loaded voicegroup000".to_string(),
            is_error: false,
        }));

        assert_eq!(
            params.committed_voicegroup_selection(),
            Some(("/tmp/project".to_string(), "voicegroup000".to_string()))
        );
        assert_eq!(
            params.voicegroup_status(),
            Some(crate::params::VoicegroupLoadStatus {
                text: "Loaded voicegroup000".to_string(),
                is_error: false,
            })
        );
        params.write_voicegroup_status(None);
        assert_eq!(params.voicegroup_status(), None);
    }

    #[test]
    fn editor_uses_fixed_size_large_enough_for_all_sections() {
        let params = Arc::new(PoryaaaaParams::default());
        let editor = crate::editor::create_editor(
            params.clone(),
            test_async_executor(),
            nice_plug_iced::iced::PollSubNotifier::new(),
        )
        .expect("editor");

        assert_eq!(
            editor.size(),
            (
                crate::params::DEFAULT_EDITOR_WIDTH,
                crate::params::DEFAULT_EDITOR_HEIGHT
            )
        );
        assert!(crate::params::DEFAULT_EDITOR_HEIGHT >= 420);
        assert!(!editor.resize_hint().can_resize);
        assert!(!editor.set_size(800, 600));
        assert_eq!(
            editor.size(),
            (
                crate::params::DEFAULT_EDITOR_WIDTH,
                crate::params::DEFAULT_EDITOR_HEIGHT
            )
        );
    }

    #[test]
    fn params_window_state_uses_default_editor_size() {
        let params = PoryaaaaParams::default();

        assert_eq!(
            params.window_state.logical_size(),
            (
                crate::params::DEFAULT_EDITOR_WIDTH,
                crate::params::DEFAULT_EDITOR_HEIGHT
            )
        );
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
    fn voicegroup_status_presentation_uses_error_and_success_colors() {
        let error = Some(crate::params::VoicegroupLoadStatus {
            text: "Bad bank".to_string(),
            is_error: true,
        });
        let success = Some(crate::params::VoicegroupLoadStatus {
            text: "Loaded petalburg".to_string(),
            is_error: false,
        });
        let loading = Some(crate::params::VoicegroupLoadStatus {
            text: "Loading petalburg".to_string(),
            is_error: false,
        });

        assert_eq!(
            crate::editor::voicegroup_status_presentation(&error),
            Some(("Bad bank".to_string(), crate::editor::STATUS_ERROR_COLOR))
        );
        assert_eq!(
            crate::editor::voicegroup_status_presentation(&success),
            Some((
                "Loaded petalburg".to_string(),
                crate::editor::STATUS_SUCCESS_COLOR
            ))
        );
        assert_eq!(
            crate::editor::voicegroup_status_presentation(&loading),
            Some((
                "Loading petalburg".to_string(),
                crate::editor::STATUS_PENDING_COLOR
            ))
        );
        assert_eq!(crate::editor::voicegroup_status_presentation(&None), None);
    }

    #[test]
    fn voicegroup_load_request_uses_drafts_and_does_not_commit_until_background_success() {
        let _home = IsolatedHome::new("home");

        let params = PoryaaaaParams::default();
        let mut gui_state = crate::editor::GuiState::from_params(&params);
        gui_state.draft_project_root = "/draft/project".to_string();
        gui_state.draft_voicegroup = "voicegroup123".to_string();

        let request = crate::editor::prepare_voicegroup_load_request(&params, &mut gui_state)
            .expect("load request");

        assert_eq!(request.project_root, "/draft/project");
        assert_eq!(request.bank, "voicegroup123");
        assert!(request.projects_json_path.ends_with("projects.json"));
        assert_eq!(params.committed_voicegroup_selection(), None);
        assert_eq!(
            params.voicegroup_status(),
            Some(crate::params::VoicegroupLoadStatus {
                text: "Loading voicegroup123".to_string(),
                is_error: false,
            })
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
        let home = IsolatedHome::new("initialize-home");
        let projects_json_path = home.projects_json_path();

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
        assert!(fs::read_to_string(projects_json_path)
            .expect("projects.json emitted during initialize")
            .contains("\"bank\": \"route104\""));

        plugin.deactivate();
        fs::remove_dir_all(root).expect("remove temp project");
    }

    #[test]
    fn initialize_loads_committed_voicegroup_into_fresh_runtime() {
        use nice_plug::prelude::*;

        let root = temp_project("initialize-committed-load");
        write_file(
            &root,
            "sound/voice_groups.inc",
            "\
                \t.align 2
                voicegroup000::
                \tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
            ",
        );

        let home = IsolatedHome::new("initialize-committed-load-home");
        let projects_json_path = home.projects_json_path();

        let mut plugin = PoryaaaaPlugin::default();
        plugin
            .params_for_test()
            .commit_voicegroup_selection(&root.to_string_lossy(), "voicegroup000");
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
        assert!(fs::read_to_string(projects_json_path)
            .expect("projects.json emitted during initialize")
            .contains("\"bank\": \"voicegroup000\""));

        plugin.deactivate();
        fs::remove_dir_all(root).expect("remove temp project");
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
        let _home = IsolatedHome::new("initialize-load-error-home");

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
        fs::remove_dir_all(root).expect("remove temp project");
    }
    #[test]
    fn load_voicegroup_task_commits_params_and_requests_restart_without_runtime_swap() {
        use nice_plug::prelude::*;

        let root = temp_project("runtime-task-restart");
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
        assert!(!plugin.runtime_has_loaded_voicegroup());

        let execute = plugin.task_executor();
        execute(crate::plugin::PoryaaaaBackgroundTask::LoadVoicegroup {
            project_root: root.to_string_lossy().into_owned(),
            bank: "voicegroup000".to_owned(),
            projects_json_path: root.join("out/projects.json"),
        });

        assert!(!plugin.runtime_has_loaded_voicegroup());
        assert_eq!(
            plugin
                .params_for_test()
                .voicegroup
                .read()
                .expect("voicegroup read")
                .as_str(),
            "voicegroup000",
        );
        assert!(plugin.params_for_test().take_host_restart_request());
        assert!(fs::read_to_string(root.join("out/projects.json"))
            .expect("projects.json")
            .contains("\"bank\": \"voicegroup000\""));

        fs::remove_dir_all(root).expect("remove temp project");
    }

    #[test]
    fn load_voicegroup_task_failure_keeps_committed_params_and_does_not_request_restart() {
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
        plugin
            .params_for_test()
            .commit_voicegroup_selection(&root.to_string_lossy(), "voicegroup000");

        let execute = plugin.task_executor();
        execute(crate::plugin::PoryaaaaBackgroundTask::LoadVoicegroup {
            project_root: "/definitely/not/a/poryaaaa/project".to_owned(),
            bank: "voicegroup000".to_owned(),
            projects_json_path: root.join("out/projects.json"),
        });

        assert_eq!(
            plugin
                .params_for_test()
                .voicegroup
                .read()
                .expect("voicegroup read")
                .as_str(),
            "voicegroup000",
        );
        assert!(!plugin.params_for_test().take_host_restart_request());

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
    fn direct_runtime_creates_resets_and_drops_native_handles() {
        let config = crate::runtime::RuntimeConfig {
            sample_rate: 48_000.0,
            volume: 127,
            reverb: 0,
        };
        let mut runtime = crate::runtime::M4aRuntime::new(config).expect("runtime");

        assert!(runtime.reset().is_ok());
        assert!(!runtime.is_ready());
    }

    #[test]
    fn failed_runtime_voicegroup_load_keeps_no_loaded_voicegroup() {
        let config = crate::runtime::RuntimeConfig {
            sample_rate: 48_000.0,
            volume: 127,
            reverb: 0,
        };
        let mut runtime = crate::runtime::M4aRuntime::new(config).expect("runtime");

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
        let config = crate::runtime::RuntimeConfig {
            sample_rate: 48_000.0,
            volume: 127,
            reverb: 0,
        };
        let mut runtime = crate::runtime::M4aRuntime::new(config).expect("runtime");
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
