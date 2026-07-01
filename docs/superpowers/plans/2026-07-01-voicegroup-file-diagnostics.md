# Voicegroup File-Level Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose file-level line numbers in diagnostics out of `voicegroup-core` so that tools can pinpoint the exact line causing macro validation issues.

**Architecture:** Implement sparse-to-file line mapping for combined monolithic files inside `ProjectIndex`. Remap diagnostics/bank record coordinates before returning them so that callers always see standard file-relative positions. Update formatting logic to prepend the 1-based line number.

**Tech Stack:** Rust (crate `voicegroup-core`).

## Global Constraints

- Git Identity: Use `Sp3cker` / `speker97@protonmail.com` for commits.
- For C, C++, Objective-C, and Objective-C++ changes, use the root `.clang-format` with Xcode's formatter: `xcrun clang-format -i path/to/file.cpp path/to/file.h`.
- Avoid non-obvious helper/utility abstractions. Inlining boolean/simple logic is preferred over verbose helpers.
- Preserve the exact behavior of existing compiler-accurate warnings and diagnostics.
- Synthetic `voice_group <label>` statements (generated for combined voicegroups lacking declarations) have no real physical representation. Map them to the start of the label definition (`label_line`) as a pragmatic fallback.

---

### Task 1: Core Line Remapping & Sparse-to-File Mapping

**Files:**
- Modify: `packages/voicegroup-core/src/project_index.rs`
- Test: `packages/voicegroup-core/tests/project_index.rs`

**Interfaces:**
- Consumes: Existing parser and builder APIs.
- Produces: Updated `ProgramBankLoadResult` diagnostics with line coordinates mapped to original file coordinates.

- [ ] **Step 1: Define `BankSourceText` and range remapping**

Add the following structures and helpers in `packages/voicegroup-core/src/project_index.rs`:

```rust
struct BankSourceText {
    relative_path: String,
    text: String,
    // 1-based parsed line -> 1-based original file line
    line_map: Vec<usize>,
}

impl BankSourceText {
    fn remap_range(&self, range: &SourceRange) -> SourceRange {
        SourceRange {
            start: SourcePosition {
                line: self.file_line(range.start.line),
                column: range.start.column,
            },
            end: SourcePosition {
                line: self.file_line(range.end.line),
                column: range.end.column,
            },
        }
    }

    fn file_line(&self, parsed_line: usize) -> usize {
        self.line_map
            .get(parsed_line.saturating_sub(1))
            .copied()
            .unwrap_or(parsed_line)
    }
}
```

- [ ] **Step 2: Implement sparse-to-file line extraction for combined files**

Add `extract_combined_section_with_map` to `project_index.rs`, replacing the old `extract_combined_section`:

```rust
fn extract_combined_section_with_map(
    text: &str,
    label: &str,
) -> Option<(String, Vec<usize>)> {
    let search_label = format!("{label}::");
    let mut in_section = false;
    let mut voices_seen = false;
    let mut has_voice_group_declaration = false;
    let mut output = String::new();
    let mut line_map = Vec::new();
    let mut label_line = 1;

    for (i, line) in text.lines().enumerate() {
        let original_line_number = i + 1;
        let trimmed = strip_comment(line).trim();
        if !in_section {
            if trimmed == search_label {
                in_section = true;
                label_line = original_line_number;
            }
            continue;
        }

        if is_combined_boundary(trimmed) {
            break;
        }

        if trimmed.is_empty() {
            continue;
        }

        if trimmed.starts_with("voice_group ") {
            has_voice_group_declaration = true;
            output.push_str(line);
            output.push('\n');
            line_map.push(original_line_number);
        } else if trimmed.starts_with("voice_") || trimmed.starts_with("cry") {
            voices_seen = true;
            output.push_str(line);
            output.push('\n');
            line_map.push(original_line_number);
        }
    }

    if !in_section || !voices_seen {
        return None;
    }

    if has_voice_group_declaration {
        Some((output, line_map))
    } else {
        let mut final_text = format!("voice_group {label}\n");
        final_text.push_str(&output);
        let mut final_map = vec![label_line]; // Map synthetic line to label declaration
        final_map.extend(line_map);
        Some((final_text, final_map))
    }
}
```

- [ ] **Step 3: Update `read_bank_source` to return `BankSourceText`**

Modify `read_bank_source` signature and implementation in `project_index.rs`:

```rust
    fn read_bank_source(&self, source: &VoiceGroupSource) -> io::Result<BankSourceText> {
        match source {
            VoiceGroupSource::Combined {
                relative_path,
                label,
            } => {
                let text = fs::read_to_string(self.root.join(relative_path))?;
                let (section, line_map) = extract_combined_section_with_map(&text, label)
                    .ok_or_else(|| {
                        io::Error::new(
                            io::ErrorKind::InvalidData,
                            "combined voicegroup section is no longer present",
                        )
                    })?;
                Ok(BankSourceText {
                    relative_path: relative_path.clone(),
                    text: section,
                    line_map,
                })
            }
            VoiceGroupSource::File { relative_path } => {
                let text = fs::read_to_string(self.root.join(relative_path))?;
                let line_map = (1..=text.lines().count()).collect();
                Ok(BankSourceText {
                    relative_path: relative_path.clone(),
                    text,
                    line_map,
                })
            }
        }
    }
```

- [ ] **Step 4: Remap diagnostics and record coordinates in `load_program_bank`**

Modify `load_program_bank` to remap all ranges using `source_text.remap_range()`:

```rust
    pub fn load_program_bank(&self, bank_name: &str) -> ProgramBankLoadResult {
        let Some(source) = self.voice_groups.get(bank_name) else {
            return ProgramBankLoadResult {
                bank: None,
                diagnostics: vec![diagnostic(
                    empty_range(),
                    "missing-voicegroup",
                    "voicegroup bank is not declared in the project index",
                )],
            };
        };

        let Ok(source_text) = self.read_bank_source(source) else {
            return ProgramBankLoadResult {
                bank: None,
                diagnostics: vec![diagnostic(
                    empty_range(),
                    "voicegroup-read-failed",
                    "voicegroup source file could not be read",
                )],
            };
        };

        let document = parse_document(&source_text.text);
        let mut diagnostics = analyze_document(&document, &self.analysis_context());
        let Some(voice_group) = document
            .voice_groups
            .iter()
            .find(|voice_group| voice_group.name.text == bank_name)
        else {
            diagnostics.push(diagnostic(
                empty_range(),
                "missing-voicegroup",
                "voicegroup bank is not declared in the selected source",
            ));
            for diagnostic in &mut diagnostics {
                diagnostic.range = source_text.remap_range(&diagnostic.range);
            }
            return ProgramBankLoadResult {
                bank: None,
                diagnostics,
            };
        };

        let ProgramBankBuildResult {
            mut bank,
            diagnostics: build_diagnostics,
        } = build_program_bank(
            voice_group,
            source_text.relative_path.clone(),
            &self.program_bank_context(),
        );
        append_unique_diagnostics(&mut diagnostics, build_diagnostics);

        for diagnostic in &mut diagnostics {
            diagnostic.range = source_text.remap_range(&diagnostic.range);
        }
        for program in &mut bank.programs {
            if let Some(record) = program {
                record.source_range = source_text.remap_range(&record.source_range);
            }
        }

        ProgramBankLoadResult {
            bank: Some(bank),
            diagnostics,
        }
    }
```

- [ ] **Step 5: Write unit tests in `project_index.rs`**

Add the following tests to `packages/voicegroup-core/tests/project_index.rs`:

```rust
#[test]
fn test_monolithic_line_remapping() {
    let root = temp_project("remapping");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\
@ comment line 1
@ comment line 2
route104::
\tvoice_directsound 60, 0, DirectSoundWaveData_Kick, 255, 0, 255, 242
\tvoice_square_2 61, 0, 0, 0, 10, 10, 1
",
    );
    write_file(
        &root,
        "sound/direct_sound_data.inc",
        "DirectSoundWaveData_Kick::\n\t.incbin \"sound/direct_sound_samples/kick.bin\"\n",
    );

    let index = ProjectIndex::load(&root).unwrap();
    let result = index.load_program_bank("route104");

    // Assert voice_square_2 decay=10 is checked. It sits at line 5 of sound/voice_groups.inc
    let diag = result.diagnostics.iter().find(|d| d.code == "integer-out-of-range").unwrap();
    assert_eq!(diag.range.start.line, 5);
}

#[test]
fn test_included_file_line_remapping() {
    let root = temp_project("included_remapping");
    write_file(
        &root,
        "sound/voice_groups.inc",
        "\t.include \"sound/voicegroups/hanabi.inc\"\n",
    );
    write_file(
        &root,
        "sound/voicegroups/hanabi.inc",
        "\
voice_group hanabi
\tvoice_square_2 60, 0, 0, 0, 10, 10, 1
",
    );

    let index = ProjectIndex::load(&root).unwrap();
    let result = index.load_program_bank("hanabi");

    let diag = result.diagnostics.iter().find(|d| d.code == "integer-out-of-range").unwrap();
    assert_eq!(diag.range.start.line, 2);
}
```

- [ ] **Step 6: Run unit tests to verify they pass**

Run: `cargo test -p voicegroup-core --test project_index`
Expected: PASS

---

### Task 2: Diagnostic Formatters Update

**Files:**
- Modify: `packages/voicegroup-core/src/plugin_load.rs`
- Modify: `packages/voicegroup-core/src/projects_json.rs`

**Interfaces:**
- Consumes: Updated load results with remapped ranges.
- Produces: Formatted diagnostic strings prepended with line numbers.

- [ ] **Step 1: Update formatted diagnostic helper in `plugin_load.rs`**

Modify `format_diagnostic` in `packages/voicegroup-core/src/plugin_load.rs`:

```rust
fn format_diagnostic(diagnostic: &Diagnostic) -> String {
    let is_synthetic = diagnostic.code == "missing-voicegroup" || diagnostic.code == "voicegroup-read-failed";
    if is_synthetic {
        format!("{}: {}", diagnostic.code, diagnostic.message)
    } else {
        format!(
            "line {}: {}: {}",
            diagnostic.range.start.line,
            diagnostic.code,
            diagnostic.message
        )
    }
}
```

- [ ] **Step 2: Update formatted diagnostic helper in `projects_json.rs`**

Modify `format_diagnostic` in `packages/voicegroup-core/src/projects_json.rs`:

```rust
fn format_diagnostic(diagnostic: &Diagnostic) -> String {
    let is_synthetic = diagnostic.code == "missing-voicegroup" || diagnostic.code == "voicegroup-read-failed";
    if is_synthetic {
        format!("{}: {}", diagnostic.code, diagnostic.message)
    } else {
        format!(
            "line {}: {}: {}",
            diagnostic.range.start.line,
            diagnostic.code,
            diagnostic.message
        )
    }
}
```

- [ ] **Step 3: Verify all workspace checks pass**

Run package-local tests and check compiling status:
`cargo test`
Expected: ALL PASS
