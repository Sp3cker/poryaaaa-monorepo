//! C ABI for poryaaaa consumers that need project-indexed program banks.

mod bank;
mod common;
mod program;
mod project;
mod types;

pub use bank::*;
pub use program::*;
pub use project::*;
pub use types::*;

#[no_mangle]
/// Returns the ABI revision expected by the generated C header.
pub extern "C" fn voicegroup_core_abi_version() -> u32 {
    VOICEGROUP_CORE_ABI_VERSION
}
