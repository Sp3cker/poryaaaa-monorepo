use crate::{editor, process, runtime::CPluginRuntime, voicegroup, PoryaaaaParams, PROGRAM_COUNT};
use nice_plug::prelude::*;
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
    runtime: Arc<Mutex<Option<CPluginRuntime>>>,
    runtime_programs: [u8; PROGRAM_COUNT],
    last_seen_program_params: [u8; PROGRAM_COUNT],
}

impl Default for PoryaaaaPlugin {
    fn default() -> Self {
        let params = Arc::new(PoryaaaaParams::default());
        let programs = read_program_params(params.as_ref());
        Self {
            params,
            runtime: Arc::new(Mutex::new(None)),
            runtime_programs: programs,
            last_seen_program_params: programs,
        }
    }
}

impl PoryaaaaPlugin {
    /// Commits a draft voicegroup selection after voicegroup-core accepts it.
    pub(crate) fn load_voicegroup(
        params: &PoryaaaaParams,
        draft_project_root: &str,
        draft_bank: &str,
        projects_json_path: &Path,
    ) -> voicegroup::VoicegroupLoadStatus {
        match Self::publish_voicegroup(draft_project_root, draft_bank, projects_json_path) {
            Ok(()) => {
                Self::commit_voicegroup_params(params, draft_project_root, draft_bank);
                Self::loaded_status(draft_bank)
            }
            Err(message) => voicegroup::VoicegroupLoadStatus {
                text: message,
                is_error: true,
            },
        }
    }

    /// Loads the committed CLAP-state voicegroup selection when restored state is present.
    pub(crate) fn load_committed_voicegroup(
        params: &PoryaaaaParams,
        projects_json_path: &Path,
    ) -> Option<voicegroup::VoicegroupLoadStatus> {
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
            .is_some_and(CPluginRuntime::has_loaded_voicegroup)
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
        *params.project_root.write().expect("project root write") = project_root.to_string();
        *params.voicegroup.write().expect("voicegroup write") = bank.to_string();
    }

    /// Builds the editor-facing success status for a loaded runtime bank.
    fn loaded_status(bank: &str) -> voicegroup::VoicegroupLoadStatus {
        voicegroup::VoicegroupLoadStatus {
            text: format!("Loaded {bank}"),
            is_error: false,
        }
    }

    /// Mirrors runtime voicegroup status into shared params for the editor.
    fn write_runtime_voicegroup_status(
        params: &PoryaaaaParams,
        status: Option<voicegroup::VoicegroupLoadStatus>,
    ) {
        *params
            .runtime_voicegroup_status
            .write()
            .expect("runtime voicegroup status write") = status;
    }

    /// Records a runtime load error in the shared editor status.
    fn set_runtime_voicegroup_error(&mut self, message: String) {
        Self::write_runtime_voicegroup_status(
            self.params.as_ref(),
            Some(voicegroup::VoicegroupLoadStatus {
                text: message,
                is_error: true,
            }),
        );
    }

    /// Runs a GUI-dispatched voicegroup load against the active runtime when present.
    fn run_background_task(
        params: &PoryaaaaParams,
        runtime: &Mutex<Option<CPluginRuntime>>,
        task: PoryaaaaBackgroundTask,
    ) {
        match task {
            PoryaaaaBackgroundTask::LoadVoicegroup {
                project_root,
                bank,
                projects_json_path,
            } => {
                if let Err(message) =
                    Self::publish_voicegroup(&project_root, &bank, &projects_json_path)
                {
                    Self::write_runtime_voicegroup_status(
                        params,
                        Some(voicegroup::VoicegroupLoadStatus {
                            text: message,
                            is_error: true,
                        }),
                    );
                    return;
                }

                let mut runtime = runtime.lock().expect("runtime lock");
                if let Some(runtime) = runtime.as_mut() {
                    if let Err(message) = runtime.load_voicegroup(&project_root, &bank) {
                        Self::write_runtime_voicegroup_status(
                            params,
                            Some(voicegroup::VoicegroupLoadStatus {
                                text: message,
                                is_error: true,
                            }),
                        );
                        return;
                    }
                }

                Self::commit_voicegroup_params(params, &project_root, &bank);
                Self::write_runtime_voicegroup_status(params, Some(Self::loaded_status(&bank)));
            }
        }
    }

    /// Mirrors host program params only when they actually change.
    fn poll_program_params(&mut self) {
        let programs = read_program_params(self.params.as_ref());
        for channel in 0..PROGRAM_COUNT {
            if programs[channel] != self.last_seen_program_params[channel] {
                self.runtime_programs[channel] = programs[channel];
                if let Some(runtime) = self.runtime.lock().expect("runtime lock").as_mut() {
                    runtime.program_change(channel as i32, programs[channel]);
                }
            }
        }
        self.last_seen_program_params = programs;
    }

    /// Pushes the current private program mirror into the runtime engine.
    fn sync_program_params_to_runtime(&mut self) {
        let mut runtime = self.runtime.lock().expect("runtime lock");
        let Some(runtime) = runtime.as_mut() else {
            return;
        };
        for channel in 0..PROGRAM_COUNT {
            runtime.program_change(channel as i32, self.runtime_programs[channel]);
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
        let runtime = self.runtime.clone();
        Box::new(move |task| Self::run_background_task(params.as_ref(), runtime.as_ref(), task))
    }

    fn initialize(
        &mut self,
        _audio_io_layout: &AudioIOLayout,
        buffer_config: &BufferConfig,
        _context: &mut impl InitContext<Self>,
    ) -> bool {
        let mut runtime = match CPluginRuntime::new(buffer_config.sample_rate) {
            Ok(runtime) => runtime,
            Err(message) => {
                self.set_runtime_voicegroup_error(message);
                *self.runtime.lock().expect("runtime lock") = None;
                return false;
            }
        };

        let project_root = self
            .params
            .project_root
            .read()
            .expect("project root read")
            .clone();
        let bank = self
            .params
            .voicegroup
            .read()
            .expect("voicegroup read")
            .clone();
        if !project_root.is_empty() && !bank.is_empty() {
            let committed_load = voicegroup::default_projects_json_path()
                .and_then(|path| Self::load_committed_voicegroup(self.params.as_ref(), &path));
            match committed_load {
                Some(status) if status.is_error => {
                    self.set_runtime_voicegroup_error(status.text);
                    runtime.clear_voicegroup();
                }
                Some(_) => match runtime.load_voicegroup(&project_root, &bank) {
                    Ok(()) => Self::write_runtime_voicegroup_status(self.params.as_ref(), None),
                    Err(message) => {
                        self.set_runtime_voicegroup_error(message);
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

        self.poll_program_params();
        *self.runtime.lock().expect("runtime lock") = Some(runtime);
        self.sync_program_params_to_runtime();
        true
    }

    fn reset(&mut self) {
        let reset = self
            .runtime
            .lock()
            .expect("runtime lock")
            .as_mut()
            .is_some_and(CPluginRuntime::reset);
        if reset {
            self.sync_program_params_to_runtime();
        }
    }

    fn deactivate(&mut self) {
        *self.runtime.lock().expect("runtime lock") = None;
    }

    fn params(&self) -> Arc<dyn Params> {
        self.params.clone()
    }

    fn editor(&mut self, async_executor: AsyncExecutor<Self>) -> Option<Box<dyn Editor>> {
        editor::create_editor(self.params.clone(), async_executor)
    }

    fn process(
        &mut self,
        buffer: &mut Buffer,
        _aux: &mut AuxiliaryBuffers,
        context: &mut impl ProcessContext<Self>,
    ) -> ProcessStatus {
        self.poll_program_params();

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
        let mut runtime = self.runtime.lock().expect("runtime lock");
        let Some(runtime) = runtime.as_mut() else {
            process::clear_stereo(left, right);
            return ProcessStatus::Normal;
        };
        if !runtime.has_loaded_voicegroup() {
            process::clear_stereo(left, right);
            return ProcessStatus::Normal;
        }

        let tempo = context.transport().tempo;
        process::process_stereo(runtime, left, right, tempo, || context.next_event());

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
