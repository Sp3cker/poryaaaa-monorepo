use crate::{
    editor,
    params::VoicegroupLoadStatus,
    process::{self, ProcessRuntime},
    runtime::{EngineConfig, M4aEngine},
    shared_projects_json, PoryaaaaParams, PROGRAM_COUNT,
};
use nice_plug::prelude::*;
use nice_plug_iced::iced::PollSubNotifier;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use voicegroup_core::plugin_load;

pub enum PoryaaaaBackgroundTask {
    LoadVoicegroup {
        project_root: String,
        bank: String,
        projects_json_path: PathBuf,
    },
}

pub struct PoryaaaaPlugin {
    params: Arc<PoryaaaaParams>,
    runtime: Arc<Mutex<Option<M4aEngine>>>,
    gui_notifier: PollSubNotifier,
    last_host_tempo_bpm: Option<f64>,
    last_applied_audio_settings: Option<crate::params::AudioSettings>,
}

impl Default for PoryaaaaPlugin {
    fn default() -> Self {
        let config = crate::config::load_default_config();
        let params = PoryaaaaParams::with_audio_defaults(config.volume, config.reverb);
        Self::apply_config_defaults_to_params(&params, &config);
        Self {
            params: Arc::new(params),
            runtime: Arc::new(Mutex::new(None)),
            gui_notifier: PollSubNotifier::new(),
            last_host_tempo_bpm: None,
            last_applied_audio_settings: None,
        }
    }
}

impl PoryaaaaPlugin {
    /// Seeds persisted params from poryaaaa.cfg defaults before host state restore can override them.
    fn apply_config_defaults_to_params(
        params: &PoryaaaaParams,
        config: &crate::config::PluginConfig,
    ) {
        *params.project_root.write().expect("project root write") = config.project_root.clone();
        *params.voicegroup.write().expect("voicegroup write") = config.voicegroup.clone();
    }

    /// Commits a draft voicegroup selection after voicegroup-core accepts it.
    pub(crate) fn load_voicegroup(
        params: &PoryaaaaParams,
        draft_project_root: &str,
        draft_bank: &str,
        projects_json_path: &Path,
    ) -> VoicegroupLoadStatus {
        match Self::publish_voicegroup(draft_project_root, draft_bank, projects_json_path) {
            Ok(()) => {
                Self::commit_voicegroup_params(params, draft_project_root, draft_bank);
                Self::loaded_status(draft_bank)
            }
            Err(message) => VoicegroupLoadStatus {
                text: message,
                is_error: true,
            },
        }
    }

    /// Loads the committed CLAP-state voicegroup selection when restored state is present.
    pub(crate) fn load_committed_voicegroup(
        params: &PoryaaaaParams,
        projects_json_path: &Path,
    ) -> Option<VoicegroupLoadStatus> {
        let project_root = params
            .project_root
            .read()
            .expect("project root read")
            .clone();
        let bank = params.voicegroup.read().expect("voicegroup read").clone();
        if project_root.is_empty() || bank.is_empty() {
            return None;
        }

        Some(Self::load_voicegroup(
            params,
            &project_root,
            &bank,
            projects_json_path,
        ))
    }

    /// Reports whether activation currently owns a C audio runtime.
    #[cfg(test)]
    pub(crate) fn has_runtime(&self) -> bool {
        self.runtime.lock().expect("runtime lock").is_some()
    }

    /// Reports whether the activation runtime has a loaded C voicegroup handle.
    #[cfg(test)]
    pub(crate) fn runtime_has_loaded_voicegroup(&self) -> bool {
        self.runtime
            .lock()
            .expect("runtime lock")
            .as_ref()
            .is_some_and(M4aEngine::is_ready)
    }

    /// Lets tests seed persisted params without exposing the production field.
    #[cfg(test)]
    pub(crate) fn params_for_test(&self) -> &PoryaaaaParams {
        self.params.as_ref()
    }

    /// Validates and publishes the Rust voicegroup index without mutating committed params.
    fn publish_voicegroup(
        draft_project_root: &str,
        draft_bank: &str,
        projects_json_path: &Path,
    ) -> Result<(), String> {
        plugin_load::load_for_plugin(
            Path::new(draft_project_root),
            draft_bank,
            projects_json_path,
        )
    }

    /// Commits the CLAP-persisted voicegroup selection after all active load steps succeed.
    fn commit_voicegroup_params(params: &PoryaaaaParams, project_root: &str, bank: &str) {
        params.commit_voicegroup_selection(project_root, bank);
    }

    /// Builds the editor-facing success status for a loaded runtime bank.
    fn loaded_status(bank: &str) -> VoicegroupLoadStatus {
        VoicegroupLoadStatus {
            text: format!("Loaded {bank}"),
            is_error: false,
        }
    }

    /// Mirrors runtime voicegroup status into shared params for the editor.
    fn write_runtime_voicegroup_status(
        params: &PoryaaaaParams,
        status: Option<VoicegroupLoadStatus>,
    ) {
        params.write_voicegroup_status(status);
    }

    /// Records a runtime load error in the shared editor status.
    fn set_runtime_voicegroup_error(&mut self, message: String) {
        Self::write_runtime_voicegroup_status(
            self.params.as_ref(),
            Some(VoicegroupLoadStatus {
                text: message,
                is_error: true,
            }),
        );
    }

    /// Runs a GUI-dispatched voicegroup Load transaction outside the audio runtime.
    fn run_background_task(params: &PoryaaaaParams, task: PoryaaaaBackgroundTask) {
        match task {
            PoryaaaaBackgroundTask::LoadVoicegroup {
                project_root,
                bank,
                projects_json_path,
            } => {
                let status =
                    Self::load_voicegroup(params, &project_root, &bank, &projects_json_path);
                let should_restart = !status.is_error;
                Self::write_runtime_voicegroup_status(params, Some(status));
                if should_restart {
                    params.request_host_restart();
                }
            }
        }
    }

    /// Pushes changed global audio controls into the active C runtime.
    fn apply_audio_settings_to_runtime(&mut self, force: bool) {
        let settings = self.params.audio_settings();
        if !force && self.last_applied_audio_settings == Some(settings) {
            return;
        }

        let mut runtime = self.runtime.lock().expect("runtime lock");
        let Some(runtime) = runtime.as_mut() else {
            self.last_applied_audio_settings = None;
            return;
        };

        runtime.set_volume(settings.volume);
        runtime.set_reverb_amount(settings.reverb);
        self.last_applied_audio_settings = Some(settings);
    }

    /// Reapplies restored/default host state after engine reinit/reset.
    fn reapply_host_state_to_runtime(&mut self) {
        self.apply_audio_settings_to_runtime(true);
        let programs = read_program_params(self.params.as_ref());
        let mut runtime = self.runtime.lock().expect("runtime lock");
        let Some(runtime) = runtime.as_mut() else {
            return;
        };
        for (channel, &program) in programs.iter().enumerate() {
            runtime.program_change(channel as i32, program);
        }
        if let Some(bpm) = self.last_host_tempo_bpm {
            runtime.set_tempo_bpm(bpm);
        }
    }

    /// Keeps only finite positive host tempo values for future runtimes.
    fn remember_host_tempo(&mut self, tempo_bpm: Option<f64>) {
        if let Some(bpm) = process::valid_host_tempo(tempo_bpm) {
            self.last_host_tempo_bpm = Some(bpm);
        }
    }
}
fn read_program_params(params: &PoryaaaaParams) -> [u8; PROGRAM_COUNT] {
    std::array::from_fn(|channel| {
        params
            .program(channel)
            .map(|program| program.value() as u8)
            .unwrap_or(channel as u8)
    })
}

impl Plugin for PoryaaaaPlugin {
    const NAME: &'static str = "poryaaaa-rs";
    const VENDOR: &'static str = "sp3cker";
    const URL: &'static str = "";
    const EMAIL: &'static str = "";
    const VERSION: &'static str = env!("CARGO_PKG_VERSION");
    const AUDIO_IO_LAYOUTS: &'static [AudioIOLayout] = &[AudioIOLayout {
        main_output_channels: NonZeroU32::new(2),
        ..AudioIOLayout::const_default()
    }];
    const MIDI_INPUT: MidiConfig = MidiConfig::MidiCCs;

    type SysExMessage = ();
    type BackgroundTask = PoryaaaaBackgroundTask;

    fn task_executor(&mut self) -> TaskExecutor<Self> {
        let params = self.params.clone();
        Box::new(move |task| Self::run_background_task(params.as_ref(), task))
    }

    fn initialize(
        &mut self,
        _audio_io_layout: &AudioIOLayout,
        buffer_config: &BufferConfig,
        _context: &mut impl InitContext<Self>,
    ) -> bool {
        let audio_settings = self.params.audio_settings();
        let config = EngineConfig {
            sample_rate: buffer_config.sample_rate,
            volume: audio_settings.volume,
            reverb: audio_settings.reverb,
        };
        let mut runtime = match M4aEngine::new(config) {
            Ok(runtime) => runtime,
            Err(err) => {
                self.set_runtime_voicegroup_error(err.to_string());
                *self.runtime.lock().expect("runtime lock") = None;
                return false;
            }
        };
        if let Some((project_root, bank)) = self.params.committed_voicegroup_selection() {
            let committed_load = shared_projects_json::default_projects_json_path()
                .and_then(|path| Self::load_committed_voicegroup(self.params.as_ref(), &path));
            match committed_load {
                Some(status) if status.is_error => {
                    self.set_runtime_voicegroup_error(status.text);
                    runtime.clear_voicegroup();
                }
                Some(_) => match runtime.load_voicegroup(&project_root, &bank) {
                    Ok(()) => Self::write_runtime_voicegroup_status(self.params.as_ref(), None),
                    Err(err) => {
                        self.set_runtime_voicegroup_error(err.to_string());
                        runtime.clear_voicegroup();
                    }
                },
                None => {
                    self.set_runtime_voicegroup_error(
                        "default projects.json path is unavailable".to_owned(),
                    );
                    runtime.clear_voicegroup();
                }
            }
        }

        self.last_applied_audio_settings = Some(audio_settings);
        *self.runtime.lock().expect("runtime lock") = Some(runtime);
        self.reapply_host_state_to_runtime();
        true
    }

    fn reset(&mut self) {
        let reset_result = self
            .runtime
            .lock()
            .expect("runtime lock")
            .as_mut()
            .map(|runtime| runtime.reset());
        match reset_result {
            Some(Ok(())) => {
                self.reapply_host_state_to_runtime();
            }
            Some(Err(err)) => {
                self.set_runtime_voicegroup_error(err.to_string());
            }
            None => {}
        }
    }

    fn deactivate(&mut self) {
        *self.runtime.lock().expect("runtime lock") = None;
        self.last_applied_audio_settings = None;
    }

    fn params(&self) -> Arc<dyn Params> {
        self.params.clone()
    }

    fn editor(&mut self, async_executor: AsyncExecutor<Self>) -> Option<Box<dyn Editor>> {
        editor::create_editor(
            self.params.clone(),
            async_executor,
            self.gui_notifier.clone(),
        )
    }

    fn process(
        &mut self,
        buffer: &mut Buffer,
        _aux: &mut AuxiliaryBuffers,
        context: &mut impl ProcessContext<Self>,
    ) -> ProcessStatus {
        let channels = buffer.as_slice();
        if channels.len() < 2 {
            for channel in channels {
                channel.fill(0.0);
            }
            return ProcessStatus::Normal;
        }

        let (left_channel, remaining_channels) = channels.split_at_mut(1);
        let left = &mut left_channel[0];
        let right = &mut remaining_channels[0];
        let tempo = context.transport().tempo;
        self.remember_host_tempo(tempo);
        self.apply_audio_settings_to_runtime(false);
        let mut runtime = self.runtime.lock().expect("runtime lock");
        let Some(runtime) = runtime.as_mut() else {
            process::clear_stereo(left, right);
            process::drain_midi_activity(self.params.midi_activity.as_ref(), || {
                context.next_event()
            });
            if self.params.window_state.is_open() {
                self.gui_notifier.notify();
            }
            return ProcessStatus::Normal;
        };
        if !runtime.is_ready() {
            process::clear_stereo(left, right);
            process::drain_midi_activity(self.params.midi_activity.as_ref(), || {
                context.next_event()
            });
            if self.params.window_state.is_open() {
                self.gui_notifier.notify();
            }
            return ProcessStatus::Normal;
        }
        process::process_stereo(
            runtime,
            left,
            right,
            tempo,
            Some(self.params.midi_activity.as_ref()),
            || context.next_event(),
        );

        if self.params.window_state.is_open() {
            self.gui_notifier.notify();
        }

        ProcessStatus::KeepAlive
    }
}

impl ClapPlugin for PoryaaaaPlugin {
    const CLAP_ID: &'static str = "com.sp3cker.poryaaaa-rs";
    const CLAP_DESCRIPTION: Option<&'static str> = Some("poryaaaa Rust CLAP instrument");
    const CLAP_MANUAL_URL: Option<&'static str> = None;
    const CLAP_SUPPORT_URL: Option<&'static str> = None;
    const CLAP_FEATURES: &'static [ClapFeature] = &[
        ClapFeature::Instrument,
        ClapFeature::Synthesizer,
        ClapFeature::Sampler,
        ClapFeature::Stereo,
    ];
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_support::{
        temp_project, write_file, IsolatedHome, TestInitContext, TEST_ENV_LOCK,
    };
    use std::fs;

    #[test]
    fn remember_host_tempo_keeps_only_finite_positive_values() {
        let mut plugin = PoryaaaaPlugin::default();

        plugin.remember_host_tempo(Some(123.5));
        assert_eq!(plugin.last_host_tempo_bpm, Some(123.5));

        plugin.remember_host_tempo(Some(f64::NAN));
        plugin.remember_host_tempo(Some(0.0));
        plugin.remember_host_tempo(Some(-1.0));
        plugin.remember_host_tempo(None);
        assert_eq!(plugin.last_host_tempo_bpm, Some(123.5));
    }

    #[test]
    fn audio_param_config_seeds_volume_and_reverb_params() {
        let config_dir = temp_project("audio-param-config-dir");
        write_file(
            &config_dir,
            "poryaaaa.cfg",
            "\
volume=64
reverb=23
",
        );
        let _env_lock = TEST_ENV_LOCK.lock().expect("test env lock");
        crate::config::set_default_config_dir_for_test(Some(config_dir.clone()));

        let plugin = PoryaaaaPlugin::default();

        crate::config::set_default_config_dir_for_test(None);
        fs::remove_dir_all(config_dir).expect("remove temp config dir");
        assert_eq!(plugin.params_for_test().volume.value(), 64);
        assert_eq!(plugin.params_for_test().reverb.value(), 23);
    }

    #[test]
    fn audio_param_changes_are_applied_to_active_runtime() {
        let root = temp_project("audio-param-runtime-project");
        write_file(
            &root,
            "sound/voice_groups.inc",
            "\
                \t.align 2
                voicegroup000::
                \tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
            ",
        );
        let _home = IsolatedHome::new("audio-param-runtime-home");
        let mut plugin = PoryaaaaPlugin::default();
        plugin
            .params_for_test()
            .commit_voicegroup_selection(&root.to_string_lossy(), "voicegroup000");
        let mut init_context = TestInitContext;
        assert!(plugin.initialize(
            &PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0],
            &test_buffer_config(),
            &mut init_context,
        ));
        assert!(plugin.runtime_has_loaded_voicegroup());

        unsafe {
            use nice_plug::params::InternalParamMut;
            plugin.params_for_test().volume._internal_set_plain_value(0);
            plugin.params_for_test().reverb._internal_set_plain_value(0);
        }
        assert_eq!(plugin.params_for_test().volume.value(), 0);
        assert_eq!(plugin.params_for_test().reverb.value(), 0);

        let mut context = TestProcessContext::with_events(vec![NoteEvent::NoteOn {
            timing: 0,
            voice_id: None,
            channel: 0,
            note: 60,
            velocity: 1.0,
        }]);
        let mut muted_peak = 0.0f32;
        for _ in 0..8 {
            let mut output = vec![vec![0.0; 512], vec![0.0; 512]];
            assert_eq!(
                process_with_output(&mut plugin, &mut output, &mut context),
                ProcessStatus::KeepAlive,
            );
            muted_peak = output
                .iter()
                .flatten()
                .fold(muted_peak, |peak, sample| peak.max(sample.abs()));
        }
        assert!(muted_peak < 0.0001);

        plugin.deactivate();
        fs::remove_dir_all(root).expect("remove temp project");
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

        let _home = IsolatedHome::new("plugin-process-loaded-home");
        let mut plugin = PoryaaaaPlugin::default();
        plugin
            .params_for_test()
            .commit_voicegroup_selection(&root.to_string_lossy(), "voicegroup000");
        let mut init_context = TestInitContext;
        assert!(plugin.initialize(
            &PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0],
            &test_buffer_config(),
            &mut init_context,
        ));
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
        let mut left_overwritten = false;
        let mut right_overwritten = false;
        for _ in 0..8 {
            let mut output = vec![vec![9.0; 512], vec![-9.0; 512]];
            let status = process_with_output(&mut plugin, &mut output, &mut context);
            assert_eq!(status, ProcessStatus::KeepAlive);
            left_overwritten |= output[0].iter().any(|sample| *sample != 9.0);
            right_overwritten |= output[1].iter().any(|sample| *sample != -9.0);
            peak = output
                .iter()
                .flatten()
                .fold(peak, |peak, sample| peak.max(sample.abs()));
        }

        assert!(left_overwritten);
        assert!(right_overwritten);
        assert!(peak > 0.0001);

        fs::remove_dir_all(root).expect("remove temp project");
    }

    #[test]
    fn plugin_default_config_loads_voicegroup_before_first_process() {
        use nice_plug::prelude::*;

        let project = temp_project("default-config-project");
        write_file(
            &project,
            "sound/voice_groups.inc",
            "\
                \t.align 2
                voicegroup000::
                \tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
            ",
        );
        let config_dir = temp_project("default-config-dir");
        write_file(
            &config_dir,
            "poryaaaa.cfg",
            &format!(
                "\
project_root={}
voicegroup=voicegroup000
reverb=0
volume=127
",
                project.to_string_lossy()
            ),
        );
        let _home = IsolatedHome::new("default-config-home");
        crate::config::set_default_config_dir_for_test(Some(config_dir.clone()));

        let mut plugin = PoryaaaaPlugin::default();
        assert_eq!(
            plugin
                .params_for_test()
                .project_root
                .read()
                .expect("project root read")
                .as_str(),
            project.to_string_lossy()
        );
        assert_eq!(
            plugin
                .params_for_test()
                .voicegroup
                .read()
                .expect("voicegroup read")
                .as_str(),
            "voicegroup000"
        );
        let mut init_context = TestInitContext;
        assert!(plugin.initialize(
            &PoryaaaaPlugin::AUDIO_IO_LAYOUTS[0],
            &test_buffer_config(),
            &mut init_context,
        ));
        assert!(plugin.runtime_has_loaded_voicegroup());

        let mut context = TestProcessContext::with_events(vec![NoteEvent::NoteOn {
            timing: 0,
            voice_id: None,
            channel: 0,
            note: 60,
            velocity: 1.0,
        }]);
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

        plugin.deactivate();
        crate::config::set_default_config_dir_for_test(None);
        fs::remove_dir_all(project).expect("remove temp project");
        fs::remove_dir_all(config_dir).expect("remove temp config dir");
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
        output: &mut [Vec<f32>],
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
}
