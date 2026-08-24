use std::ffi::CString;
use std::os::raw::c_char;
use std::ptr;

use crate::catalog::ProjectSnapshot;
use crate::project_index::ProjectIndex;

use super::common::{
    catalog_entry_view, diagnostic_view, read_c_string, slice_ptr_or_null, store_string,
};
use super::types::*;

struct VoicegroupCoreSnapshotStorage {
    diagnostics: Vec<VoicegroupCoreDiagnostic>,
    catalog: Vec<VoicegroupCoreCatalogEntry>,
    content_paths: Vec<*const c_char>,
    dependency_paths: Vec<*const c_char>,
    watch_paths: Vec<*const c_char>,
    family_adsr: Vec<VoicegroupCoreFamilyAdsr>,
    synth_macro_words: Vec<*const c_char>,
    dependency_views: Vec<Vec<*const c_char>>,
    _strings: Vec<CString>,
}

pub struct VoicegroupCoreProjectSnapshotResult {
    succeeded: bool,
    storage: VoicegroupCoreSnapshotStorage,
}

impl VoicegroupCoreProjectSnapshotResult {
    fn from_snapshot(snapshot: ProjectSnapshot) -> Self {
        let mut storage = VoicegroupCoreSnapshotStorage {
            diagnostics: Vec::with_capacity(snapshot.diagnostics.len()),
            catalog: Vec::with_capacity(snapshot.catalog.entries.len()),
            content_paths: Vec::with_capacity(snapshot.catalog.content_paths.len()),
            dependency_paths: Vec::with_capacity(snapshot.catalog.dependency_paths.len()),
            watch_paths: Vec::with_capacity(snapshot.catalog.watch_paths.len()),
            family_adsr: Vec::with_capacity(snapshot.catalog.typical_adsr_by_family.len()),
            synth_macro_words: Vec::with_capacity(snapshot.catalog.synth_macro_words.len()),
            dependency_views: Vec::with_capacity(snapshot.catalog.entries.len()),
            _strings: Vec::new(),
        };

        let diagnostics = snapshot
            .diagnostics
            .iter()
            .map(|diagnostic| {
                let code = store_string(&mut storage._strings, &diagnostic.code);
                let message = store_string(&mut storage._strings, &diagnostic.message);
                let source_path = diagnostic
                    .source_path
                    .as_deref()
                    .map(|path| store_string(&mut storage._strings, path))
                    .unwrap_or(ptr::null());
                let asset_path = diagnostic
                    .asset_path
                    .as_deref()
                    .map(|path| store_string(&mut storage._strings, path))
                    .unwrap_or(ptr::null());
                diagnostic_view(diagnostic, code, message, source_path, asset_path)
            })
            .collect();
        storage.diagnostics = diagnostics;

        let content_paths = snapshot
            .catalog
            .content_paths
            .iter()
            .map(|path| store_string(&mut storage._strings, path))
            .collect();
        storage.content_paths = content_paths;
        let dependency_paths = snapshot
            .catalog
            .dependency_paths
            .iter()
            .map(|path| store_string(&mut storage._strings, path))
            .collect();
        storage.dependency_paths = dependency_paths;
        let watch_paths = snapshot
            .catalog
            .watch_paths
            .iter()
            .map(|path| store_string(&mut storage._strings, path))
            .collect();
        storage.watch_paths = watch_paths;

        storage.family_adsr = snapshot
            .catalog
            .typical_adsr_by_family
            .iter()
            .map(|(family, adsr)| VoicegroupCoreFamilyAdsr {
                family: store_string(&mut storage._strings, family),
                adsr: *adsr,
            })
            .collect();
        storage.synth_macro_words = snapshot
            .catalog
            .synth_macro_words
            .iter()
            .map(|word| store_string(&mut storage._strings, word))
            .collect();

        for entry in &snapshot.catalog.entries {
            let dependency_paths = entry
                .dependency_paths
                .iter()
                .map(|path| store_string(&mut storage._strings, path))
                .collect::<Vec<_>>();
            let dependency_path_count = dependency_paths.len();
            storage.dependency_views.push(dependency_paths);
            let dependency_paths = storage
                .dependency_views
                .last()
                .expect("dependency view was just inserted");
            let dependency_paths = slice_ptr_or_null(dependency_paths);
            let catalog_entry = catalog_entry_view(
                entry,
                dependency_paths,
                dependency_path_count,
                &mut storage._strings,
            );
            storage.catalog.push(catalog_entry);
        }

        Self {
            succeeded: snapshot.succeeded,
            storage,
        }
    }
}
#[no_mangle]
/// Builds one opaque bulk snapshot of the project index.
///
/// # Safety
/// `index` must be a valid project index handle. `out_result` must be writable.
pub unsafe extern "C" fn voicegroup_core_project_index_snapshot(
    index: *const VoicegroupCoreProjectIndex,
    out_result: *mut *mut VoicegroupCoreProjectSnapshotResult,
) -> VoicegroupCoreStatus {
    if out_result.is_null() {
        return VoicegroupCoreStatus::NullArgument;
    }
    *out_result = ptr::null_mut();
    if index.is_null() {
        return VoicegroupCoreStatus::NullArgument;
    }

    let snapshot = (*index).index.snapshot();
    *out_result = Box::into_raw(Box::new(
        VoicegroupCoreProjectSnapshotResult::from_snapshot(snapshot),
    ));
    VoicegroupCoreStatus::Ok
}

#[no_mangle]
/// Frees an opaque project snapshot result.
///
/// # Safety
/// `result` must be null or a snapshot result returned by this library.
pub unsafe extern "C" fn voicegroup_core_project_snapshot_result_free(
    result: *mut VoicegroupCoreProjectSnapshotResult,
) {
    if !result.is_null() {
        drop(Box::from_raw(result));
    }
}

#[no_mangle]
/// Returns whether the project index was successfully built and is available
/// for loading. This is always true when the snapshot handle is valid and does
/// not reflect catalog diagnostics.
///
/// # Safety
/// `result` must be null or a valid snapshot result handle.
pub unsafe extern "C" fn voicegroup_core_project_snapshot_result_succeeded(
    result: *const VoicegroupCoreProjectSnapshotResult,
) -> bool {
    result.as_ref().is_some_and(|result| result.succeeded)
}

#[no_mangle]
/// Returns all catalog rows and optionally writes their count.
///
/// The returned pointer remains valid until the matching snapshot free.
///
/// # Safety
/// `result` must be null or a valid snapshot result handle. `out_count` may be
/// null when the caller does not need the count.
pub unsafe extern "C" fn voicegroup_core_project_snapshot_result_catalog(
    result: *const VoicegroupCoreProjectSnapshotResult,
    out_count: *mut usize,
) -> *const VoicegroupCoreCatalogEntry {
    let Some(result) = result.as_ref() else {
        if !out_count.is_null() {
            *out_count = 0;
        }
        return ptr::null();
    };
    if !out_count.is_null() {
        *out_count = result.storage.catalog.len();
    }
    slice_ptr_or_null(&result.storage.catalog)
}

#[no_mangle]
/// Returns all structured diagnostics and optionally writes their count.
///
/// The returned pointer and every string in each diagnostic remain valid until
/// the matching snapshot free.
///
/// # Safety
/// `result` must be null or a valid snapshot result handle. `out_count` may be
/// null when the caller does not need the count.
pub unsafe extern "C" fn voicegroup_core_project_snapshot_result_diagnostics(
    result: *const VoicegroupCoreProjectSnapshotResult,
    out_count: *mut usize,
) -> *const VoicegroupCoreDiagnostic {
    let Some(result) = result.as_ref() else {
        if !out_count.is_null() {
            *out_count = 0;
        }
        return ptr::null();
    };
    if !out_count.is_null() {
        *out_count = result.storage.diagnostics.len();
    }
    slice_ptr_or_null(&result.storage.diagnostics)
}

#[no_mangle]
/// Returns all actual indexed source paths and optionally writes their count.
///
/// # Safety
/// `result` must be null or a valid snapshot result handle. `out_count` may be
/// null when the caller does not need the count.
pub unsafe extern "C" fn voicegroup_core_project_snapshot_result_content_paths(
    result: *const VoicegroupCoreProjectSnapshotResult,
    out_count: *mut usize,
) -> *const *const c_char {
    snapshot_path_slice(result, out_count, |result| {
        result.storage.content_paths.as_slice()
    })
}

#[no_mangle]
/// Returns all referenced dependency paths and optionally writes their count.
///
/// # Safety
/// `result` must be null or a valid snapshot result handle. `out_count` may be
/// null when the caller does not need the count.
pub unsafe extern "C" fn voicegroup_core_project_snapshot_result_dependency_paths(
    result: *const VoicegroupCoreProjectSnapshotResult,
    out_count: *mut usize,
) -> *const *const c_char {
    snapshot_path_slice(result, out_count, |result| {
        result.storage.dependency_paths.as_slice()
    })
}

#[no_mangle]
/// Returns all deduplicated watch paths and optionally writes their count.
///
/// # Safety
/// `result` must be null or a valid snapshot result handle. `out_count` may be
/// null when the caller does not need the count.
pub unsafe extern "C" fn voicegroup_core_project_snapshot_result_watch_paths(
    result: *const VoicegroupCoreProjectSnapshotResult,
    out_count: *mut usize,
) -> *const *const c_char {
    snapshot_path_slice(result, out_count, |result| {
        result.storage.watch_paths.as_slice()
    })
}

#[no_mangle]
/// Returns typical ADSR envelopes grouped by family in lexicographic order.
///
/// # Safety
/// `result` must be null or a valid snapshot result handle. `out_count` may be
/// null when the caller does not need the count.
pub unsafe extern "C" fn voicegroup_core_project_snapshot_result_family_adsr(
    result: *const VoicegroupCoreProjectSnapshotResult,
    out_count: *mut usize,
) -> *const VoicegroupCoreFamilyAdsr {
    let Some(result) = result.as_ref() else {
        if !out_count.is_null() {
            *out_count = 0;
        }
        return ptr::null();
    };
    if !out_count.is_null() {
        *out_count = result.storage.family_adsr.len();
    }
    slice_ptr_or_null(&result.storage.family_adsr)
}

#[no_mangle]
/// Returns supported synth macro words in lexicographic order.
///
/// # Safety
/// `result` must be null or a valid snapshot result handle. `out_count` may be
/// null when the caller does not need the count.
pub unsafe extern "C" fn voicegroup_core_project_snapshot_result_synth_macro_words(
    result: *const VoicegroupCoreProjectSnapshotResult,
    out_count: *mut usize,
) -> *const *const c_char {
    snapshot_path_slice(result, out_count, |result| {
        result.storage.synth_macro_words.as_slice()
    })
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
unsafe fn snapshot_path_slice(
    result: *const VoicegroupCoreProjectSnapshotResult,
    out_count: *mut usize,
    select: impl for<'a> FnOnce(&'a VoicegroupCoreProjectSnapshotResult) -> &'a [*const c_char],
) -> *const *const c_char {
    let Some(result) = result.as_ref() else {
        if !out_count.is_null() {
            *out_count = 0;
        }
        return ptr::null();
    };
    let paths = select(result);
    if !out_count.is_null() {
        *out_count = paths.len();
    }
    slice_ptr_or_null(paths)
}
