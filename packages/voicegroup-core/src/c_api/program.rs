use crate::program_bank::{
    DirectSoundProgram, NoiseProgram, ProgramData, ProgramRecord, ProgrammableWaveProgram,
    Square1Program, Square2Program,
};

use super::bank::VoicegroupCoreBankResult;
use super::common::write_out;
use super::types::*;

pub(super) fn program_kind(data: &ProgramData) -> VoicegroupCoreProgramKind {
    match data {
        ProgramData::DirectSound(_) => VoicegroupCoreProgramKind::DirectSound,
        ProgramData::ProgrammableWave(_) => VoicegroupCoreProgramKind::ProgrammableWave,
        ProgramData::Square1(_) => VoicegroupCoreProgramKind::Square1,
        ProgramData::Square2(_) => VoicegroupCoreProgramKind::Square2,
        ProgramData::Noise(_) => VoicegroupCoreProgramKind::Noise,
        ProgramData::Keysplit(_) => VoicegroupCoreProgramKind::Keysplit,
        ProgramData::KeysplitAll(_) => VoicegroupCoreProgramKind::KeysplitAll,
        ProgramData::Cry(_) => VoicegroupCoreProgramKind::Cry,
    }
}
fn direct_sound_program(program: &DirectSoundProgram) -> VoicegroupCoreDirectSoundProgram {
    let (has_synth, synth_desc) = program
        .sample_synth_desc
        .map_or((false, [0; 6]), |descriptor| (true, descriptor));
    VoicegroupCoreDirectSoundProgram {
        key: program.key,
        pan: program.pan,
        attack: program.attack,
        decay: program.decay,
        sustain: program.sustain,
        release: program.release,
        has_synth,
        synth_desc,
    }
}

fn programmable_wave_program(
    program: &ProgrammableWaveProgram,
) -> VoicegroupCoreProgrammableWaveProgram {
    VoicegroupCoreProgrammableWaveProgram {
        key: program.key,
        pan: program.pan,
        attack: program.attack,
        decay: program.decay,
        sustain: program.sustain,
        release: program.release,
    }
}

fn square1_program(program: &Square1Program) -> VoicegroupCoreSquare1Program {
    VoicegroupCoreSquare1Program {
        key: program.key,
        pan: program.pan,
        sweep: program.sweep,
        duty: program.duty,
        attack: program.attack,
        decay: program.decay,
        sustain: program.sustain,
        release: program.release,
    }
}

fn square2_program(program: &Square2Program) -> VoicegroupCoreSquare2Program {
    VoicegroupCoreSquare2Program {
        key: program.key,
        pan: program.pan,
        duty: program.duty,
        attack: program.attack,
        decay: program.decay,
        sustain: program.sustain,
        release: program.release,
    }
}

fn noise_program(program: &NoiseProgram) -> VoicegroupCoreNoiseProgram {
    VoicegroupCoreNoiseProgram {
        key: program.key,
        pan: program.pan,
        period: program.period,
        attack: program.attack,
        decay: program.decay,
        sustain: program.sustain,
        release: program.release,
    }
}
pub(super) fn with_program_record<R>(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
    read: impl FnOnce(&ProgramRecord) -> R,
) -> Option<R> {
    unsafe { result.as_ref() }
        .and_then(|result| result.bank.as_ref())
        .and_then(|bank| bank.programs.get(slot))
        .and_then(Option::as_ref)
        .map(read)
}

pub(super) fn with_program_data<R>(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
    read: impl FnOnce(&ProgramData) -> R,
) -> Option<R> {
    with_program_record(result, slot, |record| read(&record.data))
}
#[no_mangle]
/// Writes DirectSound numeric fields for a bank slot.
///
/// # Safety
/// `result` must be a valid bank result handle. `out_program` must be writable.
pub unsafe extern "C" fn voicegroup_core_bank_result_program_direct_sound(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
    out_program: *mut VoicegroupCoreDirectSoundProgram,
) -> bool {
    with_program_data(result, slot, |data| match data {
        ProgramData::DirectSound(program) => write_out(out_program, direct_sound_program(program)),
        _ => false,
    })
    .unwrap_or(false)
}

#[no_mangle]
/// Writes programmable-wave numeric fields for a bank slot.
///
/// # Safety
/// `result` must be a valid bank result handle. `out_program` must be writable.
pub unsafe extern "C" fn voicegroup_core_bank_result_program_programmable_wave(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
    out_program: *mut VoicegroupCoreProgrammableWaveProgram,
) -> bool {
    with_program_data(result, slot, |data| match data {
        ProgramData::ProgrammableWave(program) => {
            write_out(out_program, programmable_wave_program(program))
        }
        _ => false,
    })
    .unwrap_or(false)
}

#[no_mangle]
/// Writes Square 1 numeric fields for a bank slot.
///
/// # Safety
/// `result` must be a valid bank result handle. `out_program` must be writable.
pub unsafe extern "C" fn voicegroup_core_bank_result_program_square1(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
    out_program: *mut VoicegroupCoreSquare1Program,
) -> bool {
    with_program_data(result, slot, |data| match data {
        ProgramData::Square1(program) => write_out(out_program, square1_program(program)),
        _ => false,
    })
    .unwrap_or(false)
}

#[no_mangle]
/// Writes Square 2 numeric fields for a bank slot.
///
/// # Safety
/// `result` must be a valid bank result handle. `out_program` must be writable.
pub unsafe extern "C" fn voicegroup_core_bank_result_program_square2(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
    out_program: *mut VoicegroupCoreSquare2Program,
) -> bool {
    with_program_data(result, slot, |data| match data {
        ProgramData::Square2(program) => write_out(out_program, square2_program(program)),
        _ => false,
    })
    .unwrap_or(false)
}

#[no_mangle]
/// Writes noise numeric fields for a bank slot.
///
/// # Safety
/// `result` must be a valid bank result handle. `out_program` must be writable.
pub unsafe extern "C" fn voicegroup_core_bank_result_program_noise(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
    out_program: *mut VoicegroupCoreNoiseProgram,
) -> bool {
    with_program_data(result, slot, |data| match data {
        ProgramData::Noise(program) => write_out(out_program, noise_program(program)),
        _ => false,
    })
    .unwrap_or(false)
}

#[no_mangle]
/// Writes keysplit table bytes for a bank slot.
///
/// # Safety
/// `result` must be a valid bank result handle. `out_program` must be writable.
pub unsafe extern "C" fn voicegroup_core_bank_result_program_keysplit(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
    out_program: *mut VoicegroupCoreKeysplitProgram,
) -> bool {
    with_program_data(result, slot, |data| match data {
        ProgramData::Keysplit(program) => write_out(
            out_program,
            VoicegroupCoreKeysplitProgram {
                table: program.table,
            },
        ),
        _ => false,
    })
    .unwrap_or(false)
}
