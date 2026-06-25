//! Contract checks for the checked-in C header consumed by poryaaaa.

use std::fs;
use std::path::Path;

#[test]
fn generated_c_header_exposes_public_abi_without_rust_layouts() {
    let header_path = Path::new(env!("CARGO_MANIFEST_DIR")).join("include/voicegroup_core.h");
    let header = fs::read_to_string(header_path).expect("read generated C header");

    assert!(
        header.contains("typedef struct VoicegroupCoreProjectIndex VoicegroupCoreProjectIndex;")
    );
    assert!(header.contains("typedef struct VoicegroupCoreBankResult VoicegroupCoreBankResult;"));
    assert!(header.contains("#define VOICEGROUP_CORE_PROGRAM_BANK_SIZE 128"));
    assert!(!header.contains("#define PROGRAM_BANK_SIZE"));
    assert!(header.contains("voicegroup_core_project_index_load("));
    assert!(header.contains("voicegroup_core_bank_result_program_sub_voicegroup("));
    assert!(!header.contains("voicegroup_core_bank_result_program_child_bank("));
    assert!(!header.contains("voicegroup_core_bank_result_program_macro_name("));
    assert!(!header.contains("ProjectIndex index;"));
    assert!(!header.contains("ProgramBank bank;"));
}
