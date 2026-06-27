use crate::{editor, voicegroup, PoryaaaaParams};
use nice_plug::prelude::*;
use std::path::Path;
use std::sync::Arc;
use voicegroup_core::plugin_load;

pub struct PoryaaaaPlugin {
    params: Arc<PoryaaaaParams>,
}

impl Default for PoryaaaaPlugin {
    fn default() -> Self {
        Self {
            params: Arc::new(PoryaaaaParams::default()),
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
        match plugin_load::load_for_plugin(
            Path::new(draft_project_root),
            draft_bank,
            projects_json_path,
        ) {
            Ok(()) => {
                *params.project_root.write().expect("project root write") =
                    draft_project_root.to_string();
                *params.voicegroup.write().expect("voicegroup write") = draft_bank.to_string();
                voicegroup::VoicegroupLoadStatus {
                    text: format!("Loaded {draft_bank}"),
                    is_error: false,
                }
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
    type BackgroundTask = ();

    fn initialize(
        &mut self,
        _audio_io_layout: &AudioIOLayout,
        _buffer_config: &BufferConfig,
        _context: &mut impl InitContext<Self>,
    ) -> bool {
        if let Some(path) = voicegroup::default_projects_json_path() {
            let _ = Self::load_committed_voicegroup(self.params.as_ref(), &path);
        }
        true
    }

    fn params(&self) -> Arc<dyn Params> {
        self.params.clone()
    }

    fn editor(&mut self, _async_executor: AsyncExecutor<Self>) -> Option<Box<dyn Editor>> {
        editor::create_editor(self.params.clone())
    }

    fn process(
        &mut self,
        buffer: &mut Buffer,
        _aux: &mut AuxiliaryBuffers,
        _context: &mut impl ProcessContext<Self>,
    ) -> ProcessStatus {
        for channel in buffer.as_slice() {
            channel.fill(0.0);
        }

        ProcessStatus::Normal
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
