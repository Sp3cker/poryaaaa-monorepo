use std::ffi::CString;
use std::os::raw::c_char;
use std::ptr;
use std::slice;

use crate::ast::Diagnostic;
use crate::program_bank::{ProgramBank, ProgramData, SynthOverlay};
use crate::project_index::ProgramBankLoadResult;

use super::common::{
    copy_string_to_buffer, diagnostic_view, read_c_string, read_utf8_buffer, source_range,
    store_string, write_out,
};
use super::program::{program_kind, with_program_data, with_program_record};
use super::types::*;

pub struct VoicegroupCoreBankResult {
    /// Holds the loaded bank when resolution succeeds so C callers can query slots.
    pub(super) bank: Option<ProgramBank>,
    /// Stores diagnostics beside the optional bank so failed loads remain inspectable.
    pub(super) diagnostics: Vec<Diagnostic>,
    pub(super) diagnostic_views: Vec<VoicegroupCoreDiagnostic>,
    pub(super) _diagnostic_strings: Vec<CString>,
}

impl VoicegroupCoreBankResult {
    fn new(bank: Option<ProgramBank>, diagnostics: Vec<Diagnostic>) -> Self {
        let mut diagnostic_strings = Vec::with_capacity(diagnostics.len() * 4);
        let diagnostic_views = diagnostics
            .iter()
            .map(|diagnostic| {
                let code = store_string(&mut diagnostic_strings, &diagnostic.code);
                let message = store_string(&mut diagnostic_strings, &diagnostic.message);
                let source_path = diagnostic
                    .source_path
                    .as_deref()
                    .map(|path| store_string(&mut diagnostic_strings, path))
                    .unwrap_or(ptr::null());
                let asset_path = diagnostic
                    .asset_path
                    .as_deref()
                    .map(|path| store_string(&mut diagnostic_strings, path))
                    .unwrap_or(ptr::null());
                diagnostic_view(diagnostic, code, message, source_path, asset_path)
            })
            .collect();

        Self {
            bank,
            diagnostics,
            diagnostic_views,
            _diagnostic_strings: diagnostic_strings,
        }
    }
}

pub struct VoicegroupCoreSynthOverlay {
    pub(super) overlay: SynthOverlay,
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
    *out_result = Box::into_raw(Box::new(VoicegroupCoreBankResult::new(bank, diagnostics)));
    VoicegroupCoreStatus::Ok
}

#[no_mangle]
/// Returns the number of source-ordered included banks after `bank_name`.
///
/// This is an internal materializer seam. The order comes from the indexed
/// include table and never from a C-side directory scan.
///
/// # Safety
/// `index` must be a valid project index handle and `bank_name` must be a
/// valid NUL-terminated UTF-8 string.
pub unsafe extern "C" fn voicegroup_core_project_index_bank_continuation_count(
    index: *const VoicegroupCoreProjectIndex,
    bank_name: *const c_char,
) -> usize {
    if index.is_null() || bank_name.is_null() {
        return 0;
    }
    let Some(bank_name) = read_c_string(bank_name) else {
        return 0;
    };
    crate::project_index::voicegroup_continuations(&(*index).index, &bank_name).len()
}

#[no_mangle]
/// Copies one source-ordered included bank after `bank_name`.
///
/// Empty `bank_buffer` identifies an unresolved indexed successor; its source
/// path is still copied so the C materializer can retain a structured failure.
/// Returns false when the ordinal is absent, an input is invalid, or either
/// caller-owned output buffer is too short. No output is touched on failure.
///
/// # Safety
/// `index` must be a valid project index handle, `bank_name` must be a valid
/// NUL-terminated UTF-8 string, and each non-null output buffer must be
/// writable for its declared length.
pub unsafe extern "C" fn voicegroup_core_project_index_bank_continuation(
    index: *const VoicegroupCoreProjectIndex,
    bank_name: *const c_char,
    continuation_index: usize,
    bank_buffer: *mut c_char,
    bank_buffer_len: usize,
    path_buffer: *mut c_char,
    path_buffer_len: usize,
) -> bool {
    if index.is_null() || bank_name.is_null() {
        return false;
    }
    let Some(bank_name) = read_c_string(bank_name) else {
        return false;
    };
    let continuations = crate::project_index::voicegroup_continuations(&(*index).index, &bank_name);
    let Some(continuation) = continuations.get(continuation_index) else {
        return false;
    };
    let bank_required = continuation.bank_name.len() + 1;
    let path_required = continuation.source_path.len() + 1;
    if bank_buffer.is_null()
        || path_buffer.is_null()
        || bank_buffer_len < bank_required
        || path_buffer_len < path_required
    {
        return false;
    }
    copy_string_to_buffer(&continuation.bank_name, bank_buffer, bank_buffer_len);
    copy_string_to_buffer(&continuation.source_path, path_buffer, path_buffer_len);
    true
}

#[no_mangle]
/// Copies one indexed keysplit table into a caller-provided 128-byte buffer.
///
/// # Safety
/// `index` must be a valid project index handle, `symbol` must be a valid
/// NUL-terminated UTF-8 string, and `out_table` must point to 128 writable
/// bytes.
pub unsafe extern "C" fn voicegroup_core_project_index_keysplit_table(
    index: *const VoicegroupCoreProjectIndex,
    symbol: *const c_char,
    out_table: *mut u8,
) -> bool {
    if index.is_null() || symbol.is_null() || out_table.is_null() {
        return false;
    }
    let Some(symbol) = read_c_string(symbol) else {
        return false;
    };
    let Some(table) = (*index).index.keysplit_table(&symbol) else {
        return false;
    };
    ptr::copy_nonoverlapping(table.as_ptr(), out_table, table.len());
    true
}
#[no_mangle]
/// Loads one bank from a complete unsaved source file.
///
/// `source_relative_path`, `source_bytes`, and `bank_name` are length-delimited
/// UTF-8 inputs. The path is retained as the authoritative diagnostic path.
///
/// # Safety
/// `index` and `overlay` (when non-null) must be valid handles. Every non-empty
/// input range must point to readable memory. `out_result` must be writable.
pub unsafe extern "C" fn voicegroup_core_project_index_load_program_bank_source(
    index: *const VoicegroupCoreProjectIndex,
    source_relative_path: *const c_char,
    source_relative_path_len: usize,
    source_bytes: *const c_char,
    source_len: usize,
    bank_name: *const c_char,
    bank_name_len: usize,
    overlay: *const VoicegroupCoreSynthOverlay,
    out_result: *mut *mut VoicegroupCoreBankResult,
) -> VoicegroupCoreStatus {
    if out_result.is_null() {
        return VoicegroupCoreStatus::NullArgument;
    }
    *out_result = ptr::null_mut();
    if index.is_null() {
        return VoicegroupCoreStatus::NullArgument;
    }

    let source_relative_path =
        match read_utf8_buffer(source_relative_path, source_relative_path_len) {
            Ok(path) => path,
            Err(status) => return status,
        };
    let source_bytes = match read_utf8_buffer(source_bytes, source_len) {
        Ok(source) => source,
        Err(status) => return status,
    };
    let bank_name = match read_utf8_buffer(bank_name, bank_name_len) {
        Ok(name) => name,
        Err(status) => return status,
    };

    let overlay = overlay.as_ref().map(|overlay| &overlay.overlay);
    let ProgramBankLoadResult { bank, diagnostics } = (*index).index.load_program_bank_source(
        bank_name,
        source_relative_path,
        source_bytes,
        overlay,
    );
    *out_result = Box::into_raw(Box::new(VoicegroupCoreBankResult::new(bank, diagnostics)));
    VoicegroupCoreStatus::Ok
}

#[no_mangle]
/// Creates an empty typed DirectSound synth overlay.
pub extern "C" fn voicegroup_core_synth_overlay_create() -> *mut VoicegroupCoreSynthOverlay {
    Box::into_raw(Box::new(VoicegroupCoreSynthOverlay {
        overlay: SynthOverlay::new(),
    }))
}

#[no_mangle]
/// Adds one six-byte synth definition to an overlay.
///
/// # Safety
/// `overlay` must be a valid overlay handle. `name` must point to
/// `name_len` UTF-8 bytes (a null pointer is accepted only for an empty name).
/// `descriptor` must point to six readable bytes.
pub unsafe extern "C" fn voicegroup_core_synth_overlay_add(
    overlay: *mut VoicegroupCoreSynthOverlay,
    name: *const c_char,
    name_len: usize,
    descriptor: *const u8,
) -> VoicegroupCoreStatus {
    if overlay.is_null() || descriptor.is_null() {
        return VoicegroupCoreStatus::NullArgument;
    }
    let name = match read_utf8_buffer(name, name_len) {
        Ok(name) => name,
        Err(status) => return status,
    };
    let descriptor = slice::from_raw_parts(descriptor, 6);
    let descriptor: [u8; 6] = descriptor
        .try_into()
        .expect("descriptor slice has the fixed six-byte length");
    (*overlay).overlay.insert(name, descriptor);
    VoicegroupCoreStatus::Ok
}

#[no_mangle]
/// Frees a typed synth overlay.
///
/// # Safety
/// `overlay` must be null or a handle returned by this library.
pub unsafe extern "C" fn voicegroup_core_synth_overlay_free(
    overlay: *mut VoicegroupCoreSynthOverlay,
) {
    if !overlay.is_null() {
        drop(Box::from_raw(overlay));
    }
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
/// Writes one complete diagnostic view, including scope, paths, range, and slot.
///
/// The returned pointers remain valid until the matching bank-result free.
///
/// # Safety
/// `result` must be a valid bank result handle. `out_diagnostic` must be writable.
pub unsafe extern "C" fn voicegroup_core_bank_result_diagnostic(
    result: *const VoicegroupCoreBankResult,
    index: usize,
    out_diagnostic: *mut VoicegroupCoreDiagnostic,
) -> bool {
    let Some(diagnostic) = result
        .as_ref()
        .and_then(|result| result.diagnostic_views.get(index))
        .copied()
    else {
        return false;
    };
    write_out(out_diagnostic, diagnostic)
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
