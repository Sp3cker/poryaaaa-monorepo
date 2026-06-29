//! Rust shell for the poryaaaa CLAP plugin.
//!
//! The audio engine stays in C. This crate owns the CLAP/plugin-facing layer
//! and calls the engine through an explicit FFI seam as the migration advances.

mod config;
mod editor;
mod ffi;
mod params;
mod plugin;
mod process;
mod runtime;
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
    fn process_without_runtime_clears_stereo_and_returns_normal() {
        use nice_plug::prelude::*;

        let mut plugin = PoryaaaaPlugin::default();
        let mut output = vec![vec![1.0; 8], vec![-1.0; 8]];
        let mut context = TestProcessContext::default();

        let status = process_with_output(&mut plugin, &mut output, &mut context);

        assert_eq!(status, ProcessStatus::Normal);
        assert!(output[0].iter().all(|sample| *sample == 0.0));
        assert!(output[1].iter().all(|sample| *sample == 0.0));
    }

    #[test]
    fn process_without_loaded_voicegroup_clears_stereo_and_returns_normal() {
        use nice_plug::prelude::*;

        let mut plugin = PoryaaaaPlugin::default();
        let mut init_context = TestInitContext;
        assert!(plugin.initialize(
            &PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0],
            &test_buffer_config(),
            &mut init_context,
        ));
        assert!(!plugin.runtime_has_loaded_voicegroup());
        let mut output = vec![vec![1.0; 8], vec![-1.0; 8]];
        let mut context = TestProcessContext::default();

        let status = process_with_output(&mut plugin, &mut output, &mut context);

        assert_eq!(status, ProcessStatus::Normal);
        assert!(output[0].iter().all(|sample| *sample == 0.0));
        assert!(output[1].iter().all(|sample| *sample == 0.0));
    }

    #[test]
    fn process_with_loaded_voicegroup_renders_audio_and_returns_keepalive() {
        use nice_plug::prelude::*;

        let root = temp_project("plugin-process-loaded");
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
        let mut init_context = TestInitContext;
        assert!(plugin.initialize(
            &PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0],
            &test_buffer_config(),
            &mut init_context,
        ));
        let execute = plugin.task_executor();
        execute(crate::plugin::PoryaaaaBackgroundTask::LoadVoicegroup {
            project_root: root.to_string_lossy().into_owned(),
            bank: "voicegroup000".to_owned(),
            projects_json_path: root.join("out/projects.json"),
        });
        assert!(plugin.runtime_has_loaded_voicegroup());

        let mut context = TestProcessContext::with_events(vec![
            NoteEvent::MidiCC {
                timing: 0,
                channel: 0,
                cc: 7,
                value: 1.0,
            },
            NoteEvent::MidiCC {
                timing: 0,
                channel: 0,
                cc: 10,
                value: 0.5,
            },
            NoteEvent::NoteOn {
                timing: 0,
                voice_id: None,
                channel: 0,
                note: 60,
                velocity: 1.0,
            },
        ]);
        let mut peak = 0.0f32;
        for _ in 0..8 {
            let mut output = vec![vec![0.0; 512], vec![0.0; 512]];
            let status = process_with_output(&mut plugin, &mut output, &mut context);
            assert_eq!(status, ProcessStatus::KeepAlive);
            peak = output
                .iter()
                .flatten()
                .fold(peak, |peak, sample| peak.max(sample.abs()));
        }

        assert!(peak > 0.0001);
        fs::remove_dir_all(root).expect("remove temp project");
    }

    fn test_buffer_config() -> nice_plug::prelude::BufferConfig {
        nice_plug::prelude::BufferConfig {
            sample_rate: 48_000.0,
            min_buffer_size: None,
            max_buffer_size: 512,
            process_mode: nice_plug::prelude::ProcessMode::Realtime,
        }
    }

    fn process_with_output(
        plugin: &mut PoryaaaaPlugin,
        output: &mut Vec<Vec<f32>>,
        context: &mut TestProcessContext,
    ) -> nice_plug::prelude::ProcessStatus {
        use nice_plug::prelude::*;

        let samples = output[0].len();
        let mut buffer = Buffer::default();
        unsafe {
            buffer.set_slices(samples, |output_slices| {
                let (left, right) = output.split_at_mut(1);
                *output_slices = vec![&mut left[0], &mut right[0]];
            });
        }
        let mut aux_inputs: Vec<Buffer> = Vec::new();
        let mut aux_outputs: Vec<Buffer> = Vec::new();
        let mut aux = AuxiliaryBuffers {
            inputs: aux_inputs.as_mut_slice(),
            outputs: aux_outputs.as_mut_slice(),
        };
        plugin.process(&mut buffer, &mut aux, context)
    }

    struct TestProcessContext {
        transport: nice_plug::prelude::Transport,
        events: std::vec::IntoIter<nice_plug::prelude::NoteEvent<()>>,
        sent_events: Vec<nice_plug::prelude::NoteEvent<()>>,
    }

    impl Default for TestProcessContext {
        fn default() -> Self {
            Self::with_events(Vec::new())
        }
    }

    impl TestProcessContext {
        fn with_events(events: Vec<nice_plug::prelude::NoteEvent<()>>) -> Self {
            Self {
                transport: nice_plug::prelude::Transport::new(48_000.0),
                events: events.into_iter(),
                sent_events: Vec::new(),
            }
        }
    }

    impl nice_plug::prelude::ProcessContext<PoryaaaaPlugin> for TestProcessContext {
        fn plugin_api(&self) -> nice_plug::prelude::PluginApi {
            nice_plug::prelude::PluginApi::Clap
        }

        fn execute_background(
            &self,
            _task: <PoryaaaaPlugin as nice_plug::prelude::Plugin>::BackgroundTask,
        ) {
        }

        fn execute_gui(
            &self,
            _task: <PoryaaaaPlugin as nice_plug::prelude::Plugin>::BackgroundTask,
        ) {
        }

        fn transport(&self) -> &nice_plug::prelude::Transport {
            &self.transport
        }

        fn next_event(&mut self) -> Option<nice_plug::prelude::PluginNoteEvent<PoryaaaaPlugin>> {
            self.events.next()
        }

        fn send_event(&mut self, event: nice_plug::prelude::PluginNoteEvent<PoryaaaaPlugin>) {
            self.sent_events.push(event);
        }

        fn set_latency_samples(&self, _samples: u32) {}

        fn set_current_voice_capacity(&self, _capacity: u32) {}
    }

    struct TestInitContext;

    impl nice_plug::prelude::InitContext<PoryaaaaPlugin> for TestInitContext {
        fn plugin_api(&self) -> nice_plug::prelude::PluginApi {
            nice_plug::prelude::PluginApi::Clap
        }

        fn execute(&self, _task: <PoryaaaaPlugin as nice_plug::prelude::Plugin>::BackgroundTask) {}

        fn set_latency_samples(&self, _samples: u32) {}

        fn set_current_voice_capacity(&self, _capacity: u32) {}
    }

    #[test]
    fn c_runtime_creates_resets_and_drops_engine() {
        let mut runtime = crate::runtime::CPluginRuntime::new(48_000.0).expect("runtime");

        assert!(runtime.reset());
        assert!(!runtime.has_loaded_voicegroup());
    }

    #[test]
    fn failed_runtime_voicegroup_load_keeps_no_loaded_voicegroup() {
        let mut runtime = crate::runtime::CPluginRuntime::new(48_000.0).expect("runtime");

        let result = runtime.load_voicegroup("/definitely/not/a/poryaaaa/project", "voicegroup000");

        assert!(result.is_err());
        assert!(!runtime.has_loaded_voicegroup());
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
        let mut runtime = crate::runtime::CPluginRuntime::new(48_000.0).expect("runtime");
        runtime
            .load_voicegroup(&root.to_string_lossy(), "voicegroup000")
            .expect("initial load");

        let result = runtime.load_voicegroup("/definitely/not/a/poryaaaa/project", "voicegroup000");

        assert!(result.is_err());
        assert!(runtime.has_loaded_voicegroup());
        assert!(runtime.reset());
        assert!(runtime.has_loaded_voicegroup());

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
