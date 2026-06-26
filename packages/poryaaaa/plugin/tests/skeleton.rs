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

#[test]
fn project_inputs_persist_through_plugin_state() {
    let params = PoryaaaaParams::default();
    *params.project_root.write().expect("project root write") = "/tmp/poryaaaa-project".to_string();
    *params.voicegroup.write().expect("voicegroup write") = "voicegroup001".to_string();

    let serialized = params.serialize_fields();
    assert_eq!(
        serialized.get("project-root").map(String::as_str),
        Some("\"/tmp/poryaaaa-project\"")
    );
    assert_eq!(
        serialized.get("voicegroup").map(String::as_str),
        Some("\"voicegroup001\"")
    );

    let restored = PoryaaaaParams::default();
    restored.deserialize_fields(&serialized);

    assert_eq!(
        restored
            .project_root
            .read()
            .expect("project root read")
            .as_str(),
        "/tmp/poryaaaa-project"
    );
    assert_eq!(
        restored
            .voicegroup
            .read()
            .expect("voicegroup read")
            .as_str(),
        "voicegroup001"
    );
}
