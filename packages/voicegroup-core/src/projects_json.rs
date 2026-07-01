//! Emits the ccomidi-compatible shared projects.json document.

use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use crate::ast::{Diagnostic, DiagnosticSeverity};
use crate::diagnostic_format::diagnostic_message;
use crate::program_bank::{ProgramBank, ProgramData};
use crate::project_index::ProjectIndex;

struct ProjectSlot {
    program: usize,
    name: String,
    type_code: u8,
    drumset: Vec<DrumPad>,
}
//todo: Do we need this? A struct specifically for voices in a drumset?
struct DrumPad {
    note: usize,
    name: String,
}

/// Emits projects.json after the caller has validated the selected bank.
pub(crate) fn emit(
    path: &Path,
    project_root: &Path,
    bank_name: &str,
    index: &ProjectIndex,
    bank: &ProgramBank,
) -> Result<(), String> {
    let slots = collect_slots(index, bank)?;
    let body = render(project_root, bank_name, &slots);
    write_published_file(path, &body)
        .map_err(|error| format!("projects.json write failed: {error}"))
}

fn collect_slots(index: &ProjectIndex, bank: &ProgramBank) -> Result<Vec<ProjectSlot>, String> {
    let mut slots = Vec::new();
    for (program, record) in bank.programs.iter().enumerate() {
        let Some(record) = record else {
            continue;
        };

        let drumset = match &record.data {
            ProgramData::KeysplitAll(data) => collect_drumset(index, &data.sub_voicegroup)?,
            _ => Vec::new(),
        };

        slots.push(ProjectSlot {
            program,
            name: record.display_name.clone(),
            type_code: record.type_code as u8,
            drumset,
        });
    }
    Ok(slots)
}

fn collect_drumset(index: &ProjectIndex, bank_name: &str) -> Result<Vec<DrumPad>, String> {
    let result = index.load_program_bank(bank_name);
    if let Some(diagnostic) = first_error(&result.diagnostics) {
        return Err(diagnostic_message(diagnostic));
    }

    let bank = result.bank.ok_or_else(|| {
        result
            .diagnostics
            .first()
            .map(diagnostic_message)
            .unwrap_or_else(|| "drumset voicegroup bank could not be loaded".to_string())
    })?;

    let mut pads = Vec::new();
    for (note, record) in bank.programs.iter().enumerate() {
        if let Some(record) = record {
            pads.push(DrumPad {
                note,
                name: record.display_name.clone(),
            });
        }
    }
    Ok(pads)
}

fn render(project_root: &Path, bank_name: &str, slots: &[ProjectSlot]) -> String {
    let mut output = String::new();
    output.push_str("{\n  \"root\": \"");
    push_escaped(&mut output, &project_root.to_string_lossy());
    output.push_str("\",\n  \"bank\": \"");
    push_escaped(&mut output, bank_name);
    output.push_str("\",\n  \"slots\": [\n");

    for (index, slot) in slots.iter().enumerate() {
        if index > 0 {
            output.push_str(",\n");
        }
        output.push_str("    {\"program\": ");
        output.push_str(&slot.program.to_string());
        output.push_str(", \"name\": \"");
        push_escaped(&mut output, &slot.name);
        output.push_str("\", \"typeCode\": ");
        output.push_str(&slot.type_code.to_string());
        if !slot.drumset.is_empty() {
            output.push_str(", \"drumset\": [");
            for (pad_index, pad) in slot.drumset.iter().enumerate() {
                if pad_index > 0 {
                    output.push_str(", ");
                }
                output.push_str("{\"note\": ");
                output.push_str(&pad.note.to_string());
                output.push_str(", \"name\": \"");
                push_escaped(&mut output, &pad.name);
                output.push_str("\"}");
            }
            output.push(']');
        }
        output.push('}');
    }

    output.push_str("\n  ]\n}\n");
    output
}

fn write_published_file(path: &Path, body: &str) -> io::Result<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }

    let temp_path = temp_path(path);
    fs::write(&temp_path, body)?;
    match replace_file(&temp_path, path) {
        Ok(()) => Ok(()),
        Err(error) => {
            let _ = fs::remove_file(&temp_path);
            Err(error)
        }
    }
}

#[cfg(windows)]
fn replace_file(temp_path: &Path, path: &Path) -> io::Result<()> {
    if path.exists() {
        return Err(io::Error::new(
            io::ErrorKind::AlreadyExists,
            "atomic replacement of an existing projects.json is not implemented on Windows",
        ));
    }
    fs::rename(temp_path, path)
}

#[cfg(not(windows))]
fn replace_file(temp_path: &Path, path: &Path) -> io::Result<()> {
    fs::rename(temp_path, path)
}

fn temp_path(path: &Path) -> PathBuf {
    let mut temp_path = path.to_path_buf();
    let temp_name = path
        .file_name()
        .map(|name| format!("{}.tmp", name.to_string_lossy()))
        .unwrap_or_else(|| "projects.json.tmp".to_string());
    temp_path.set_file_name(temp_name);
    temp_path
}

fn push_escaped(output: &mut String, text: &str) {
    for character in text.chars() {
        match character {
            '"' => output.push_str("\\\""),
            '\\' => output.push_str("\\\\"),
            '\n' => output.push_str("\\n"),
            '\r' => output.push_str("\\r"),
            '\t' => output.push_str("\\t"),
            '\u{00}'..='\u{1f}' => {
                output.push_str("\\u");
                output.push_str(&format!("{:04x}", character as u32));
            }
            _ => output.push(character),
        }
    }
}

fn first_error(diagnostics: &[Diagnostic]) -> Option<&Diagnostic> {
    diagnostics
        .iter()
        .find(|diagnostic| diagnostic.severity == DiagnosticSeverity::Error)
}
