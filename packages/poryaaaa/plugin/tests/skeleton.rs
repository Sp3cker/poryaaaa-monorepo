use nice_plug::prelude::*;
use nice_plug::prelude::{ClapFeature, ClapPlugin};
use poryaaaa_clap_plugin::{PoryaaaaParams, PoryaaaaPlugin, PROGRAM_COUNT};
use std::num::NonZeroU32;

#[test]
fn plugin_declares_new_clap_identity_and_stereo_midi_shape() {
    assert_eq!(<PoryaaaaPlugin as Plugin>::NAME, "poryaaaa-rs");
    assert_eq!(
        <PoryaaaaPlugin as ClapPlugin>::CLAP_ID,
        "com.sp3cker.poryaaaa-rs"
    );
    assert_eq!(<PoryaaaaPlugin as Plugin>::MIDI_INPUT, MidiConfig::MidiCCs);

    let layout = &<PoryaaaaPlugin as Plugin>::AUDIO_IO_LAYOUTS[0];
    assert_eq!(layout.main_input_channels, None);
    assert_eq!(layout.main_output_channels, NonZeroU32::new(2));

    let features = <PoryaaaaPlugin as ClapPlugin>::CLAP_FEATURES;
    assert!(features.contains(&ClapFeature::Instrument));
    assert!(features.contains(&ClapFeature::Synthesizer));
    assert!(features.contains(&ClapFeature::Sampler));
    assert!(features.contains(&ClapFeature::Stereo));
}

#[test]
fn params_default_channel_programs_match_channel_numbers() {
    let params = PoryaaaaParams::default();

    for channel in 0..PROGRAM_COUNT {
        let program = params.program(channel).expect("channel program param");
        assert_eq!(program.value(), channel as i32);
        assert!(matches!(
            program.range(),
            IntRange::Linear { min: 0, max: 127 }
        ));
    }

    assert!(params.program(PROGRAM_COUNT).is_none());
}
