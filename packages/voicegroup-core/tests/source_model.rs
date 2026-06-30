use voicegroup_core::ast::{SourcePosition, SourceRange};

#[test]
fn source_range_contains_positions_using_rust_exclusive_end_contract() {
    let range = SourceRange {
        start: SourcePosition { line: 2, column: 4 },
        end: SourcePosition { line: 4, column: 3 },
    };

    assert!(!range.contains(&SourcePosition {
        line: 1,
        column: 99
    }));
    assert!(!range.contains(&SourcePosition { line: 2, column: 3 }));
    assert!(range.contains(&SourcePosition { line: 2, column: 4 }));
    assert!(range.contains(&SourcePosition { line: 3, column: 1 }));
    assert!(range.contains(&SourcePosition { line: 4, column: 2 }));
    assert!(!range.contains(&SourcePosition { line: 4, column: 3 }));
    assert!(!range.contains(&SourcePosition { line: 5, column: 1 }));
}
