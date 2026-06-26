use crate::{editor, PoryaaaaParams};
use nice_plug::prelude::*;
use std::sync::Arc;

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
