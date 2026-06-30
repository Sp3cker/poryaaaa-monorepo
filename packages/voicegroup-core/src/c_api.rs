//! C ABI for poryaaaa consumers that need project-indexed program banks.

use std::ffi::CStr;
use std::os::raw::c_char;
use std::ptr;

use crate::ast::{Diagnostic, DiagnosticSeverity, SourcePosition, SourceRange};
use crate::program_bank::{
    DirectSoundProgram, NoiseProgram, ProgramBank, ProgramData, ProgramRecord,
    ProgrammableWaveProgram, Square1Program, Square2Program,
};
use crate::project_index::{ProgramBankLoadResult, ProjectIndex};

pub struct VoicegroupCoreProjectIndex {
    /// Owns discovered project symbols and file locations across C ABI calls.
    index: ProjectIndex,
}

pub struct VoicegroupCoreBankResult {
    /// Holds the loaded bank when resolution succeeds so C callers can query slots.
    bank: Option<ProgramBank>,
    /// Stores diagnostics beside the optional bank so failed loads remain inspectable.
    diagnostics: Vec<Diagnostic>,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VoicegroupCoreStatus {
    Ok = 0,
    NullArgument = 1,
    InvalidUtf8 = 2,
    LoadFailed = 3,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VoicegroupCoreProgramKind {
    Empty = 0,
    DirectSound = 1,
    ProgrammableWave = 2,
    Square1 = 3,
    Square2 = 4,
    Noise = 5,
    Keysplit = 6,
    KeysplitAll = 7,
    Cry = 8,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VoicegroupCoreDiagnosticSeverity {
    Error = 0,
    Warning = 1,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct VoicegroupCoreSourcePosition {
    /// One-based source line for diagnostics crossing the C ABI.
    pub line: usize,
    /// One-based source column for diagnostics crossing the C ABI.
    pub column: usize,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct VoicegroupCoreSourceRange {
    /// First source position covered by a diagnostic.
    pub start: VoicegroupCoreSourcePosition,
    /// Position immediately after the diagnostic span.
    pub end: VoicegroupCoreSourcePosition,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct VoicegroupCoreDirectSoundProgram {
    /// Base MIDI key used to pitch the DirectSound sample.
    pub key: u8,
    /// C-compatible pan value copied from the voice macro.
    pub pan: u8,
    /// Envelope attack byte used by the poryaaaa engine.
    pub attack: u8,
    /// Envelope decay byte used by the poryaaaa engine.
    pub decay: u8,
    /// Envelope sustain byte used by the poryaaaa engine.
    pub sustain: u8,
    /// Envelope release byte used by the poryaaaa engine.
    pub release: u8,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct VoicegroupCoreProgrammableWaveProgram {
    /// Base MIDI key used to pitch the programmable-wave sample.
    pub key: u8,
    pub pan: u8,
    pub attack: u8,
    pub decay: u8,
    pub sustain: u8,
    pub release: u8,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct VoicegroupCoreSquare1Program {
    /// Base MIDI key used to pitch square channel 1.
    pub key: u8,
    /// C-compatible pan value copied from the voice macro.
    pub pan: u8,
    /// Sweep byte used by square channel 1.
    pub sweep: u8,
    /// Masked duty value used by square channel 1.
    pub duty: u8,
    /// Masked envelope attack value matching C loader materialization.
    pub attack: u8,
    /// Masked envelope decay value matching C loader materialization.
    pub decay: u8,
    /// Masked envelope sustain value matching C loader materialization.
    pub sustain: u8,
    /// Masked envelope release value matching C loader materialization.
    pub release: u8,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct VoicegroupCoreSquare2Program {
    /// Base MIDI key used to pitch square channel 2.
    pub key: u8,
    /// C-compatible pan value copied from the voice macro.
    pub pan: u8,
    /// Masked duty value used by square channel 2.
    pub duty: u8,
    /// Masked envelope attack value matching C loader materialization.
    pub attack: u8,
    /// Masked envelope decay value matching C loader materialization.
    pub decay: u8,
    /// Masked envelope sustain value matching C loader materialization.
    pub sustain: u8,
    /// Masked envelope release value matching C loader materialization.
    pub release: u8,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct VoicegroupCoreNoiseProgram {
    /// Base MIDI key retained for parity with the source macro.
    pub key: u8,
    /// C-compatible pan value copied from the voice macro.
    pub pan: u8,
    /// Masked noise period value used by the poryaaaa engine.
    pub period: u8,
    /// Masked envelope attack value matching C loader materialization.
    pub attack: u8,
    /// Masked envelope decay value matching C loader materialization.
    pub decay: u8,
    /// Masked envelope sustain value matching C loader materialization.
    pub sustain: u8,
    /// Masked envelope release value matching C loader materialization.
    pub release: u8,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct VoicegroupCoreKeysplitProgram {
    /// Fully expanded 128-entry note-to-sub-voicegroup slot table.
    pub table: [u8; 128],
}

impl Default for VoicegroupCoreKeysplitProgram {
    fn default() -> Self {
        Self { table: [0; 128] }
    }
}

#[no_mangle]
/// Loads a project index and writes an opaque handle to `out_index`.
///
/// # Safety
/// `project_root` must be a valid NUL-terminated UTF-8 string. `out_index` must
/// be writable.
pub unsafe extern "C" fn voicegroup_core_project_index_load(
    project_root: *const c_char,
    out_index: *mut *mut VoicegroupCoreProjectIndex,
) -> VoicegroupCoreStatus {
    if out_index.is_null() {
        return VoicegroupCoreStatus::NullArgument;
    }
    *out_index = ptr::null_mut();

    if project_root.is_null() {
        return VoicegroupCoreStatus::NullArgument;
    }

    let Some(root) = read_c_string(project_root) else {
        return VoicegroupCoreStatus::InvalidUtf8;
    };

    match ProjectIndex::load(root) {
        Ok(index) => {
            *out_index = Box::into_raw(Box::new(VoicegroupCoreProjectIndex { index }));
            VoicegroupCoreStatus::Ok
        }
        Err(_) => VoicegroupCoreStatus::LoadFailed,
    }
}

#[no_mangle]
/// Frees a project index handle.
///
/// # Safety
/// `index` must be null or a handle returned by this library that has not been freed.
pub unsafe extern "C" fn voicegroup_core_project_index_free(
    index: *mut VoicegroupCoreProjectIndex,
) {
    if !index.is_null() {
        drop(Box::from_raw(index));
    }
}

#[no_mangle]
/// Loads one program bank from an existing project index.
///
/// # Safety
/// `index` must be a valid project index handle. `bank_name` must be a valid
/// NUL-terminated UTF-8 string. `out_result` must be writable.
pub unsafe extern "C" fn voicegroup_core_project_index_load_program_bank(
    index: *const VoicegroupCoreProjectIndex,
    bank_name: *const c_char,
    out_result: *mut *mut VoicegroupCoreBankResult,
) -> VoicegroupCoreStatus {
    if out_result.is_null() {
        return VoicegroupCoreStatus::NullArgument;
    }
    *out_result = ptr::null_mut();

    if index.is_null() || bank_name.is_null() {
        return VoicegroupCoreStatus::NullArgument;
    }

    let Some(bank_name) = read_c_string(bank_name) else {
        return VoicegroupCoreStatus::InvalidUtf8;
    };
    let ProgramBankLoadResult { bank, diagnostics } = (*index).index.load_program_bank(&bank_name);
    *out_result = Box::into_raw(Box::new(VoicegroupCoreBankResult { bank, diagnostics }));
    VoicegroupCoreStatus::Ok
}

#[no_mangle]
/// Frees a bank result handle.
///
/// # Safety
/// `result` must be null or a handle returned by this library that has not been freed.
pub unsafe extern "C" fn voicegroup_core_bank_result_free(result: *mut VoicegroupCoreBankResult) {
    if !result.is_null() {
        drop(Box::from_raw(result));
    }
}

#[no_mangle]
/// Returns whether a bank result contains a loaded bank.
///
/// # Safety
/// `result` must be null or a valid bank result handle.
pub unsafe extern "C" fn voicegroup_core_bank_result_has_bank(
    result: *const VoicegroupCoreBankResult,
) -> bool {
    result.as_ref().is_some_and(|result| result.bank.is_some())
}

#[no_mangle]
/// Returns the number of diagnostics in a bank result.
///
/// # Safety
/// `result` must be null or a valid bank result handle.
pub unsafe extern "C" fn voicegroup_core_bank_result_diagnostic_count(
    result: *const VoicegroupCoreBankResult,
) -> usize {
    result
        .as_ref()
        .map(|result| result.diagnostics.len())
        .unwrap_or_default()
}

#[no_mangle]
/// Copies one diagnostic code into a caller-provided buffer.
///
/// # Safety
/// `result` must be a valid bank result handle. `buffer` may be null only when
/// `buffer_len` is zero; otherwise it must be writable for `buffer_len` bytes.
pub unsafe extern "C" fn voicegroup_core_bank_result_diagnostic_code(
    result: *const VoicegroupCoreBankResult,
    index: usize,
    buffer: *mut c_char,
    buffer_len: usize,
) -> usize {
    result
        .as_ref()
        .and_then(|result| result.diagnostics.get(index))
        .map(|diagnostic| copy_string_to_buffer(&diagnostic.code, buffer, buffer_len))
        .unwrap_or_default()
}

#[no_mangle]
/// Copies one diagnostic message into a caller-provided buffer.
///
/// # Safety
/// `result` must be a valid bank result handle. `buffer` may be null only when
/// `buffer_len` is zero; otherwise it must be writable for `buffer_len` bytes.
pub unsafe extern "C" fn voicegroup_core_bank_result_diagnostic_message(
    result: *const VoicegroupCoreBankResult,
    index: usize,
    buffer: *mut c_char,
    buffer_len: usize,
) -> usize {
    result
        .as_ref()
        .and_then(|result| result.diagnostics.get(index))
        .map(|diagnostic| copy_string_to_buffer(&diagnostic.message, buffer, buffer_len))
        .unwrap_or_default()
}

#[no_mangle]
/// Returns one diagnostic severity.
///
/// # Safety
/// `result` must be null or a valid bank result handle.
pub unsafe extern "C" fn voicegroup_core_bank_result_diagnostic_severity(
    result: *const VoicegroupCoreBankResult,
    index: usize,
) -> VoicegroupCoreDiagnosticSeverity {
    result
        .as_ref()
        .and_then(|result| result.diagnostics.get(index))
        .map(|diagnostic| diagnostic_severity(&diagnostic.severity))
        .unwrap_or(VoicegroupCoreDiagnosticSeverity::Error)
}

#[no_mangle]
/// Writes one diagnostic source range.
///
/// # Safety
/// `result` must be a valid bank result handle. `out_range` must be writable.
pub unsafe extern "C" fn voicegroup_core_bank_result_diagnostic_range(
    result: *const VoicegroupCoreBankResult,
    index: usize,
    out_range: *mut VoicegroupCoreSourceRange,
) -> bool {
    let Some(diagnostic) = result
        .as_ref()
        .and_then(|result| result.diagnostics.get(index))
    else {
        return false;
    };
    write_out(out_range, source_range(&diagnostic.range))
}

#[no_mangle]
/// Returns the program kind for a bank slot.
///
/// # Safety
/// `result` must be null or a valid bank result handle.
pub unsafe extern "C" fn voicegroup_core_bank_result_program_kind(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
) -> VoicegroupCoreProgramKind {
    with_program_data(result, slot, program_kind).unwrap_or(VoicegroupCoreProgramKind::Empty)
}

#[no_mangle]
/// Returns the poryaaaa/GBA voice type code for a bank slot.
///
/// # Safety
/// `result` must be null or a valid bank result handle.
pub unsafe extern "C" fn voicegroup_core_bank_result_program_type_code(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
) -> u8 {
    with_program_record(result, slot, |record| record.type_code as u8).unwrap_or_default()
}

#[no_mangle]
/// Copies a bank slot display name into a caller-provided buffer.
///
/// # Safety
/// `result` must be a valid bank result handle. `buffer` may be null only when
/// `buffer_len` is zero; otherwise it must be writable for `buffer_len` bytes.
pub unsafe extern "C" fn voicegroup_core_bank_result_program_display_name(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
    buffer: *mut c_char,
    buffer_len: usize,
) -> usize {
    with_program_record(result, slot, |record| {
        copy_string_to_buffer(&record.display_name, buffer, buffer_len)
    })
    .unwrap_or_default()
}

#[no_mangle]
/// Copies the relative asset path for a sample-backed bank slot.
///
/// # Safety
/// `result` must be a valid bank result handle. `buffer` may be null only when
/// `buffer_len` is zero; otherwise it must be writable for `buffer_len` bytes.
pub unsafe extern "C" fn voicegroup_core_bank_result_program_relative_path(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
    buffer: *mut c_char,
    buffer_len: usize,
) -> usize {
    with_program_data(result, slot, |data| match data {
        ProgramData::DirectSound(program) => {
            copy_string_to_buffer(&program.sample_relative_path, buffer, buffer_len)
        }
        ProgramData::ProgrammableWave(program) => {
            copy_string_to_buffer(&program.wave_relative_path, buffer, buffer_len)
        }
        ProgramData::Cry(program) => {
            copy_string_to_buffer(&program.sample_relative_path, buffer, buffer_len)
        }
        _ => 0,
    })
    .unwrap_or_default()
}

#[no_mangle]
/// Copies the sub-voicegroup symbol for keysplit slots.
///
/// # Safety
/// `result` must be a valid bank result handle. `buffer` may be null only when
/// `buffer_len` is zero; otherwise it must be writable for `buffer_len` bytes.
pub unsafe extern "C" fn voicegroup_core_bank_result_program_sub_voicegroup(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
    buffer: *mut c_char,
    buffer_len: usize,
) -> usize {
    with_program_data(result, slot, |data| match data {
        ProgramData::Keysplit(program) => {
            copy_string_to_buffer(&program.sub_voicegroup, buffer, buffer_len)
        }
        ProgramData::KeysplitAll(program) => {
            copy_string_to_buffer(&program.sub_voicegroup, buffer, buffer_len)
        }
        _ => 0,
    })
    .unwrap_or_default()
}

#[no_mangle]
/// Copies the keysplit table symbol for keysplit slots.
///
/// # Safety
/// `result` must be a valid bank result handle. `buffer` may be null only when
/// `buffer_len` is zero; otherwise it must be writable for `buffer_len` bytes.
pub unsafe extern "C" fn voicegroup_core_bank_result_program_keysplit_table_symbol(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
    buffer: *mut c_char,
    buffer_len: usize,
) -> usize {
    with_program_data(result, slot, |data| match data {
        ProgramData::Keysplit(program) => {
            copy_string_to_buffer(&program.table_symbol, buffer, buffer_len)
        }
        _ => 0,
    })
    .unwrap_or_default()
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

fn read_c_string(pointer: *const c_char) -> Option<String> {
    unsafe { CStr::from_ptr(pointer) }
        .to_str()
        .ok()
        .map(ToOwned::to_owned)
}

fn copy_string_to_buffer(value: &str, buffer: *mut c_char, buffer_len: usize) -> usize {
    let required_len = value.len() + 1;
    if buffer.is_null() || buffer_len == 0 {
        return required_len;
    }

    let bytes = value.as_bytes();
    let copy_len = bytes.len().min(buffer_len - 1);
    unsafe {
        ptr::copy_nonoverlapping(bytes.as_ptr(), buffer.cast::<u8>(), copy_len);
        *buffer.add(copy_len) = 0;
    }
    required_len
}

fn with_program_record<R>(
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

fn with_program_data<R>(
    result: *const VoicegroupCoreBankResult,
    slot: usize,
    read: impl FnOnce(&ProgramData) -> R,
) -> Option<R> {
    with_program_record(result, slot, |record| read(&record.data))
}

fn program_kind(data: &ProgramData) -> VoicegroupCoreProgramKind {
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

fn diagnostic_severity(severity: &DiagnosticSeverity) -> VoicegroupCoreDiagnosticSeverity {
    match severity {
        DiagnosticSeverity::Error => VoicegroupCoreDiagnosticSeverity::Error,
        DiagnosticSeverity::Warning => VoicegroupCoreDiagnosticSeverity::Warning,
    }
}

fn source_range(range: &SourceRange) -> VoicegroupCoreSourceRange {
    VoicegroupCoreSourceRange {
        start: source_position(&range.start),
        end: source_position(&range.end),
    }
}

fn source_position(position: &SourcePosition) -> VoicegroupCoreSourcePosition {
    VoicegroupCoreSourcePosition {
        line: position.line,
        column: position.column,
    }
}

fn write_out<T: Copy>(out: *mut T, value: T) -> bool {
    if out.is_null() {
        return false;
    }
    unsafe {
        *out = value;
    }
    true
}

fn direct_sound_program(program: &DirectSoundProgram) -> VoicegroupCoreDirectSoundProgram {
    VoicegroupCoreDirectSoundProgram {
        key: program.key,
        pan: program.pan,
        attack: program.attack,
        decay: program.decay,
        sustain: program.sustain,
        release: program.release,
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
