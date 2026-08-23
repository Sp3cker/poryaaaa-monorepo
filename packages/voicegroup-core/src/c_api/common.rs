use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;
use std::slice;

use crate::ast::{Diagnostic, DiagnosticScope, SourcePosition, SourceRange};
use crate::catalog::{CatalogEntry, CatalogEntryKind};

use super::types::*;

pub(super) fn store_string(strings: &mut Vec<CString>, value: &str) -> *const c_char {
    let owned = CString::new(value).expect("indexed UTF-8 strings cannot contain NUL");
    let pointer = owned.as_ptr();
    strings.push(owned);
    pointer
}

pub(super) fn diagnostic_view(
    diagnostic: &Diagnostic,
    code: *const c_char,
    message: *const c_char,
    source_path: *const c_char,
    asset_path: *const c_char,
) -> VoicegroupCoreDiagnostic {
    VoicegroupCoreDiagnostic {
        code,
        message,
        scope: diagnostic_scope(&diagnostic.scope),
        source_path,
        asset_path,
        range: source_range(&diagnostic.range),
        has_range: diagnostic.range.start != diagnostic.range.end,
        has_slot: diagnostic.slot.is_some(),
        slot: diagnostic.slot.unwrap_or_default(),
    }
}

pub(super) fn catalog_entry_view(
    entry: &CatalogEntry,
    dependency_paths: *const *const c_char,
    dependency_path_count: usize,
    strings: &mut Vec<CString>,
) -> VoicegroupCoreCatalogEntry {
    let (subgroup, table) = entry
        .keysplit
        .as_ref()
        .map(|pair| {
            (
                store_string(strings, &pair.subgroup),
                store_string(strings, &pair.table),
            )
        })
        .unwrap_or((ptr::null(), ptr::null()));
    let drumkit = entry
        .drumkit
        .as_deref()
        .map(|value| store_string(strings, value))
        .unwrap_or(ptr::null());
    let (has_adsr, adsr) = entry
        .typical_adsr
        .map_or((false, [0; 4]), |adsr| (true, adsr));
    let (has_synth, synth_desc) = entry
        .synth_desc
        .map_or((false, [0; 6]), |descriptor| (true, descriptor));

    VoicegroupCoreCatalogEntry {
        kind: catalog_entry_kind(entry.kind),
        symbol: store_string(strings, &entry.symbol),
        display_name: store_string(strings, &entry.display_name),
        source_path: entry
            .source_path
            .as_deref()
            .map(|value| store_string(strings, value))
            .unwrap_or(ptr::null()),
        asset_path: entry
            .asset_path
            .as_deref()
            .map(|value| store_string(strings, value))
            .unwrap_or(ptr::null()),
        dependency_paths,
        dependency_path_count,
        subgroup,
        table,
        drumkit,
        has_adsr,
        adsr,
        has_synth,
        synth_desc,
    }
}

fn catalog_entry_kind(kind: CatalogEntryKind) -> VoicegroupCoreCatalogEntryKind {
    match kind {
        CatalogEntryKind::VoiceGroup => VoicegroupCoreCatalogEntryKind::VoiceGroup,
        CatalogEntryKind::DirectSound => VoicegroupCoreCatalogEntryKind::DirectSound,
        CatalogEntryKind::ProgrammableWave => VoicegroupCoreCatalogEntryKind::ProgrammableWave,
        CatalogEntryKind::Keysplit => VoicegroupCoreCatalogEntryKind::Keysplit,
        CatalogEntryKind::Drumkit => VoicegroupCoreCatalogEntryKind::Drumkit,
        CatalogEntryKind::Synth => VoicegroupCoreCatalogEntryKind::Synth,
    }
}

fn diagnostic_scope(scope: &DiagnosticScope) -> VoicegroupCoreDiagnosticScope {
    match scope {
        DiagnosticScope::Structural => VoicegroupCoreDiagnosticScope::Structural,
        DiagnosticScope::Slot => VoicegroupCoreDiagnosticScope::Slot,
        DiagnosticScope::Materialization => VoicegroupCoreDiagnosticScope::Materialization,
    }
}
pub(super) fn source_range(range: &SourceRange) -> VoicegroupCoreSourceRange {
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

pub(super) fn write_out<T: Copy>(out: *mut T, value: T) -> bool {
    if out.is_null() {
        return false;
    }
    unsafe {
        *out = value;
    }
    true
}
pub(super) unsafe fn read_utf8_buffer<'a>(
    pointer: *const c_char,
    length: usize,
) -> Result<&'a str, VoicegroupCoreStatus> {
    if pointer.is_null() {
        if length == 0 {
            return Ok("");
        }
        return Err(VoicegroupCoreStatus::NullArgument);
    }
    let bytes = unsafe { slice::from_raw_parts(pointer.cast::<u8>(), length) };
    if bytes.contains(&0) {
        return Err(VoicegroupCoreStatus::InvalidUtf8);
    }
    std::str::from_utf8(bytes).map_err(|_| VoicegroupCoreStatus::InvalidUtf8)
}

pub(super) fn slice_ptr_or_null<T>(values: &[T]) -> *const T {
    if values.is_empty() {
        ptr::null()
    } else {
        values.as_ptr()
    }
}

pub(super) fn read_c_string(pointer: *const c_char) -> Option<String> {
    unsafe { CStr::from_ptr(pointer) }
        .to_str()
        .ok()
        .map(ToOwned::to_owned)
}
pub(super) fn copy_string_to_buffer(value: &str, buffer: *mut c_char, buffer_len: usize) -> usize {
    let required_len = value.len() + 1;
    if buffer.is_null() || buffer_len == 0 {
        return required_len;
    }

    let bytes = value.as_bytes();
    let mut copy_len = bytes.len().min(buffer_len - 1);
    while !value.is_char_boundary(copy_len) {
        copy_len -= 1;
    }
    unsafe {
        ptr::copy_nonoverlapping(bytes.as_ptr(), buffer.cast::<u8>(), copy_len);
        *buffer.add(copy_len) = 0;
    }
    required_len
}

#[cfg(test)]
mod tests {
    use std::os::raw::c_char;

    use super::copy_string_to_buffer;

    #[test]
    fn string_copy_never_splits_a_utf8_code_point() {
        let mut buffer = [0x7f as c_char; 3];
        let required = copy_string_to_buffer("éx", buffer.as_mut_ptr(), buffer.len());
        assert_eq!(required, 4);
        assert_eq!(buffer[0] as u8, 0xc3);
        assert_eq!(buffer[1] as u8, 0xa9);
        assert_eq!(buffer[2], 0);

        let mut too_small = [0x7f as c_char; 2];
        copy_string_to_buffer("é", too_small.as_mut_ptr(), too_small.len());
        assert_eq!(too_small, [0, 0x7f]);
    }

    #[test]
    fn zero_length_string_copy_does_not_touch_the_pointer() {
        let mut sentinel = 0x7f as c_char;
        let required = copy_string_to_buffer("value", &mut sentinel, 0);
        assert_eq!(required, 6);
        assert_eq!(sentinel, 0x7f);
    }
}
