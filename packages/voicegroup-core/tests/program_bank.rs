//! ProgramBank tests for poryaaaa-facing typed voice records.

use voicegroup_core::catalog::VoiceType;
use voicegroup_core::parser::parse_document;
use voicegroup_core::program_bank::{
    build_program_bank, ProgramBankContext, ProgramData, ResolvedAsset, PROGRAM_BANK_SIZE,
};

fn diagnostic_codes(
    diagnostics: &[voicegroup_core::program_bank::ProgramBankDiagnostic],
) -> Vec<&str> {
    diagnostics
        .iter()
        .map(|diagnostic| diagnostic.code.as_str())
        .collect()
}

#[test]
fn builds_poryaaaa_facing_bank_from_checked_voice_group() {
    let source = "\
voice_group route104
\tvoice_directsound 60, 0, DirectSoundWaveData_Brass1, 255, 252, 0, 115 @ Brass
\tvoice_programmable_wave 61, 12, ProgrammableWaveData_Pulse1, 1, 2, 8, 3
\tvoice_square_1_alt 62, 0, 5, 2, 1, 2, 8, 3
\tvoice_square_2 63, 0, 3, 1, 2, 8, 3
\tvoice_noise_alt 64, 0, 1, 1, 2, 8, 3
\tvoice_keysplit voicegroup_strings, keysplit_strings @ Strings
\tvoice_keysplit_all voicegroup_drums
\tcry_reverse DirectSoundWaveData_Cry
";
    let document = parse_document(source);
    let context = ProgramBankContext::default()
        .with_direct_sound_asset(ResolvedAsset {
            symbol: "DirectSoundWaveData_Brass1".to_string(),
            relative_path: "sound/direct_sound_samples/brass_1.bin".to_string(),
            display_name: "brass_1.bin".to_string(),
        })
        .with_direct_sound_asset(ResolvedAsset {
            symbol: "DirectSoundWaveData_Cry".to_string(),
            relative_path: "sound/direct_sound_samples/cry.bin".to_string(),
            display_name: "cry.bin".to_string(),
        })
        .with_programmable_wave_asset(ResolvedAsset {
            symbol: "ProgrammableWaveData_Pulse1".to_string(),
            relative_path: "sound/programmable_wave_samples/pulse_1.pcm".to_string(),
            display_name: "pulse_1.pcm".to_string(),
        })
        .with_keysplit_table("keysplit_strings", [42; 128]);

    let result = build_program_bank(
        &document.voice_groups[0],
        "sound/voicegroups/route104.inc",
        &context,
    );
    let bank = result.bank;

    assert_eq!(result.diagnostics, []);
    assert_eq!(bank.name, "route104");
    assert_eq!(bank.source_relative_path, "sound/voicegroups/route104.inc");
    assert_eq!(bank.programs.len(), PROGRAM_BANK_SIZE);

    let directsound = bank.programs[0].as_ref().expect("slot 0");
    assert_eq!(directsound.macro_name, "voice_directsound");
    assert_eq!(directsound.type_code, VoiceType::DirectSound);
    assert_eq!(directsound.display_name, "brass_1.bin");
    assert_eq!(directsound.trailing_comment.as_deref(), Some("Brass"));
    assert_eq!(
        directsound.data,
        ProgramData::DirectSound(voicegroup_core::program_bank::DirectSoundProgram {
            key: 60,
            pan: 0,
            sample_symbol: "DirectSoundWaveData_Brass1".to_string(),
            sample_relative_path: "sound/direct_sound_samples/brass_1.bin".to_string(),
            attack: 255,
            decay: 252,
            sustain: 0,
            release: 115,
        })
    );

    let prog_wave = bank.programs[1].as_ref().expect("slot 1");
    assert_eq!(prog_wave.type_code, VoiceType::ProgrammableWave);
    assert_eq!(prog_wave.display_name, "pulse_1.pcm");
    assert_eq!(
        prog_wave.data,
        ProgramData::ProgrammableWave(voicegroup_core::program_bank::ProgrammableWaveProgram {
            key: 61,
            pan: 12,
            wave_symbol: "ProgrammableWaveData_Pulse1".to_string(),
            wave_relative_path: "sound/programmable_wave_samples/pulse_1.pcm".to_string(),
            attack: 1,
            decay: 2,
            sustain: 8,
            release: 3,
        })
    );

    assert_eq!(
        bank.programs[2].as_ref().expect("slot 2").data,
        ProgramData::Square1(voicegroup_core::program_bank::Square1Program {
            key: 62,
            pan: 0,
            sweep: 5,
            duty: 2,
            attack: 1,
            decay: 2,
            sustain: 8,
            release: 3,
        })
    );
    assert_eq!(
        bank.programs[2].as_ref().expect("slot 2").display_name,
        "Square 1 (alt)"
    );
    assert_eq!(
        bank.programs[3].as_ref().expect("slot 3").data,
        ProgramData::Square2(voicegroup_core::program_bank::Square2Program {
            key: 63,
            pan: 0,
            duty: 3,
            attack: 1,
            decay: 2,
            sustain: 8,
            release: 3,
        })
    );
    assert_eq!(
        bank.programs[4].as_ref().expect("slot 4").data,
        ProgramData::Noise(voicegroup_core::program_bank::NoiseProgram {
            key: 64,
            pan: 0,
            period: 1,
            attack: 1,
            decay: 2,
            sustain: 8,
            release: 3,
        })
    );
    assert_eq!(
        bank.programs[4].as_ref().expect("slot 4").display_name,
        "Noise (alt)"
    );

    assert_eq!(
        bank.programs[5].as_ref().expect("slot 5").data,
        ProgramData::Keysplit(voicegroup_core::program_bank::KeysplitProgram {
            child_bank: "voicegroup_strings".to_string(),
            table_symbol: "keysplit_strings".to_string(),
            table: [42; 128],
        })
    );
    assert_eq!(
        bank.programs[5].as_ref().expect("slot 5").display_name,
        "Strings"
    );
    assert_eq!(
        bank.programs[6].as_ref().expect("slot 6").data,
        ProgramData::KeysplitAll(voicegroup_core::program_bank::KeysplitAllProgram {
            child_bank: "voicegroup_drums".to_string(),
        })
    );
    assert_eq!(
        bank.programs[6].as_ref().expect("slot 6").display_name,
        "voicegroup_drums"
    );

    let cry = bank.programs[7].as_ref().expect("slot 7");
    assert_eq!(cry.type_code, VoiceType::CryReverse);
    assert_eq!(cry.display_name, "cry.bin");
    assert_eq!(
        cry.data,
        ProgramData::Cry(voicegroup_core::program_bank::CryProgram {
            sample_symbol: "DirectSoundWaveData_Cry".to_string(),
            sample_relative_path: "sound/direct_sound_samples/cry.bin".to_string(),
        })
    );
}

#[test]
fn respects_start_slot_when_populating_program_bank() {
    let source = "\
voice_group rs_drumset, 36
\tvoice_directsound_no_resample 60, 64, DirectSoundWaveData_Kick, 255, 0, 255, 242
";
    let document = parse_document(source);
    let context = ProgramBankContext::default().with_direct_sound_asset(ResolvedAsset {
        symbol: "DirectSoundWaveData_Kick".to_string(),
        relative_path: "sound/direct_sound_samples/kick.bin".to_string(),
        display_name: "kick.bin".to_string(),
    });

    let result = build_program_bank(
        &document.voice_groups[0],
        "sound/voicegroups/rs_drumset.inc",
        &context,
    );
    let bank = result.bank;

    assert_eq!(result.diagnostics, []);
    assert!(bank.programs[35].is_none());
    assert_eq!(
        bank.programs[36].as_ref().expect("slot 36").type_code,
        VoiceType::DirectSoundNoResample
    );
}

#[test]
fn accumulates_diagnostics_and_keeps_unambiguous_records() {
    let source = "\
voice_group broken
\tvoice_directsound 60, 0, DirectSoundWaveData_Kick, 255, 0, 255, 242
\tvoice_directsound 61, 0, DirectSoundWaveData_Snare, 255, 0, 255, 242
\tvoice_programmable_wave 62, 0, ProgrammableWaveData_Missing, 1, 2, 8, 3
";
    let document = parse_document(source);
    let mut voice_group = document.voice_groups[0].clone();
    voice_group.programs[1].slot = 0;
    let context = ProgramBankContext::default()
        .with_direct_sound_asset(ResolvedAsset {
            symbol: "DirectSoundWaveData_Kick".to_string(),
            relative_path: "sound/direct_sound_samples/kick.bin".to_string(),
            display_name: "kick.bin".to_string(),
        })
        .with_direct_sound_asset(ResolvedAsset {
            symbol: "DirectSoundWaveData_Snare".to_string(),
            relative_path: "sound/direct_sound_samples/snare.bin".to_string(),
            display_name: "snare.bin".to_string(),
        });

    let result = build_program_bank(&voice_group, "sound/voicegroups/broken.inc", &context);

    assert_eq!(
        diagnostic_codes(&result.diagnostics),
        ["duplicate-slot", "unknown-programmable-wave-symbol"]
    );
    let slot_zero = result.bank.programs[0].as_ref().expect("slot 0");
    assert_eq!(slot_zero.display_name, "kick.bin");
    assert_eq!(
        result.bank.programs[2], None,
        "missing asset record must not be fabricated"
    );
}
