use std::os::raw::c_char;

use crate::project_index::ProjectIndex;

/// ABI revision consumed by the source-built Pory A C adapter.
pub const VOICEGROUP_CORE_ABI_VERSION: u32 = 2;

pub struct VoicegroupCoreProjectIndex {
    /// Owns discovered project symbols and file locations across C ABI calls.
    pub(super) index: ProjectIndex,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VoicegroupCoreDiagnosticScope {
    Structural = 0,
    Slot = 1,
    Materialization = 2,
}
impl Default for VoicegroupCoreDiagnosticScope {
    fn default() -> Self {
        Self::Structural
    }
}
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VoicegroupCoreCatalogEntryKind {
    VoiceGroup = 0,
    DirectSound = 1,
    ProgrammableWave = 2,
    Keysplit = 3,
    Drumkit = 4,
    Synth = 5,
}
impl Default for VoicegroupCoreCatalogEntryKind {
    fn default() -> Self {
        Self::VoiceGroup
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct VoicegroupCoreDiagnostic {
    /// NUL-terminated UTF-8 stable diagnostic identifier.
    pub code: *const c_char,
    /// NUL-terminated UTF-8 human-readable diagnostic message.
    pub message: *const c_char,
    /// Layer that owns the diagnostic.
    pub scope: VoicegroupCoreDiagnosticScope,
    /// Actual source file, or NULL when the diagnostic has no source.
    pub source_path: *const c_char,
    /// Referenced asset path, or NULL when no asset failed.
    pub asset_path: *const c_char,
    /// Full one-based, start-inclusive/end-exclusive source range.
    pub range: VoicegroupCoreSourceRange,
    /// Whether `range` points at a non-empty source span.
    pub has_range: bool,
    /// Whether `slot` identifies a voicegroup slot.
    pub has_slot: bool,
    pub slot: usize,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct VoicegroupCoreCatalogEntry {
    /// Catalog family for this row.
    pub kind: VoicegroupCoreCatalogEntryKind,
    /// Canonical symbol.
    pub symbol: *const c_char,
    /// Rust-provided display name.
    pub display_name: *const c_char,
    /// Actual indexed source file, or NULL when none exists.
    pub source_path: *const c_char,
    /// Direct dependency path, or NULL when this row has no single asset.
    pub asset_path: *const c_char,
    /// Every indexed dependency referenced by this row.
    pub dependency_paths: *const *const c_char,
    pub dependency_path_count: usize,
    /// Keysplit subgroup/table pair, or NULL for non-keysplit rows.
    pub subgroup: *const c_char,
    pub table: *const c_char,
    /// Drumkit subgroup, or NULL for non-drumkit rows.
    pub drumkit: *const c_char,
    /// Typical ADSR envelope, when one was observed.
    pub has_adsr: bool,
    pub adsr: [u8; 4],
    /// Six-byte DirectSound synth descriptor, when this is a synth row.
    pub has_synth: bool,
    pub synth_desc: [u8; 6],
}

/// Typical ADSR envelope for one catalog voice family.
#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct VoicegroupCoreFamilyAdsr {
    /// Stable family name, owned by the snapshot result.
    pub family: *const c_char,
    /// Attack, decay, sustain, and release bytes.
    pub adsr: [u8; 4],
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
    /// Whether this sample resolves to a six-byte DirectSound synth descriptor.
    pub has_synth: bool,
    /// Six-byte descriptor used by Golden Sun synth aliases.
    pub synth_desc: [u8; 6],
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
