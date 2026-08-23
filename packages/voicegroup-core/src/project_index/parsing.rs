use std::collections::BTreeMap;

use crate::ast::{SourcePosition, SourceRange};
use crate::parser::parse_document;

use super::DefinitionLocation;

pub(super) fn combined_section_has_voice_macro(text: &str, label: &str) -> bool {
    extract_combined_section_with_map(text, label).is_some()
}

// Extracts one combined-bank section while retaining sparse snippet line origins.
pub(super) fn extract_combined_section_with_map(
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

fn is_combined_boundary(trimmed: &str) -> bool {
    trimmed.starts_with(".align") || !parse_document(trimmed).assembly_labels.is_empty()
}

pub(super) fn strip_comment(line: &str) -> &str {
    line.split_once('@')
        .map(|(source, _)| source)
        .unwrap_or(line)
}

pub(super) fn include_path_with_range(
    line: &str,
    line_number: usize,
) -> Option<(String, SourceRange)> {
    let relative_path = include_path(line)?;
    let quote = line.find('"')?;
    let start = quote + 2;
    let range = SourceRange {
        start: SourcePosition {
            line: line_number,
            column: start,
        },
        end: SourcePosition {
            line: line_number,
            column: start + relative_path.len(),
        },
    };
    Some((relative_path, range))
}

pub(super) fn incbin_path(line: &str) -> Option<String> {
    let trimmed = line.trim_start();
    if !trimmed.starts_with(".incbin") {
        return None;
    }
    let start = trimmed.find('"')? + 1;
    let end = trimmed[start..].find('"')? + start;
    Some(trimmed[start..end].to_string())
}

fn include_path(line: &str) -> Option<String> {
    let trimmed = line.trim_start();
    if !trimmed.starts_with(".include") {
        return None;
    }
    let start = trimmed.find('"')? + 1;
    let end = trimmed[start..].find('"')? + start;
    Some(trimmed[start..end].to_string())
}

pub(super) fn is_voicegroup_include_path(path: &str) -> bool {
    path.starts_with("sound/voicegroups/")
        && path.ends_with(".inc")
        && !path.contains('\\')
        && path
            .split('/')
            .all(|component| !component.is_empty() && component != "." && component != "..")
}

pub(super) struct ParsedKeysplitTables {
    pub(super) tables: BTreeMap<String, [u8; 128]>,
    pub(super) definitions: BTreeMap<String, DefinitionLocation>,
}

pub(super) fn parse_keysplit_tables(relative_path: &str, text: &str) -> ParsedKeysplitTables {
    let mut tables = BTreeMap::new();
    let mut definitions = BTreeMap::new();
    let mut current_name: Option<String> = None;
    let mut last_note = 0usize;

    for (line_index, line) in text.lines().enumerate() {
        let source = strip_comment(line);
        let trimmed = source.trim();
        if let Some((name, start_note)) = emerald_keysplit_header(trimmed) {
            tables.entry(name.clone()).or_insert([0; 128]);
            if let Some(range) = keysplit_name_range(
                source,
                name.strip_prefix("keysplit_").unwrap_or(&name),
                line_index + 1,
            ) {
                definitions
                    .entry(name.clone())
                    .or_insert_with(|| DefinitionLocation {
                        relative_path: relative_path.to_string(),
                        range,
                    });
            }
            current_name = Some(name);
            last_note = start_note;
            continue;
        }

        if let Some(end_note) = emerald_split_end(trimmed) {
            if let Some(table) = current_name.as_ref().and_then(|name| tables.get_mut(name)) {
                fill_table_range(table, last_note, end_note.0, end_note.1);
                last_note = end_note.0;
            }
            continue;
        }

        if let Some((name, start_note)) = firered_keysplit_header(trimmed) {
            tables.entry(name.clone()).or_insert([0; 128]);
            if let Some(range) = keysplit_name_range(source, &name, line_index + 1) {
                definitions
                    .entry(name.clone())
                    .or_insert_with(|| DefinitionLocation {
                        relative_path: relative_path.to_string(),
                        range,
                    });
            }
            current_name = Some(name);
            last_note = start_note;
            continue;
        }

        if let Some(values) = firered_byte_values(trimmed) {
            if let Some(table) = current_name.as_ref().and_then(|name| tables.get_mut(name)) {
                for value in values {
                    if last_note < 128 {
                        table[last_note] = value;
                    }
                    last_note += 1;
                }
            }
        }
    }

    ParsedKeysplitTables {
        tables,
        definitions,
    }
}

fn keysplit_name_range(line: &str, name: &str, line_number: usize) -> Option<SourceRange> {
    let start = line.find(name)? + 1;
    Some(SourceRange {
        start: SourcePosition {
            line: line_number,
            column: start,
        },
        end: SourcePosition {
            line: line_number,
            column: start + name.len(),
        },
    })
}

fn emerald_keysplit_header(line: &str) -> Option<(String, usize)> {
    let rest = line.strip_prefix("keysplit ")?;
    let (name, start_note) = rest.split_once(',')?;
    Some((
        format!("keysplit_{}", name.trim()),
        start_note.trim().parse().ok()?,
    ))
}

fn emerald_split_end(line: &str) -> Option<(usize, u8)> {
    let rest = line.strip_prefix("split ")?;
    let (value, end_note) = rest.split_once(',')?;
    Some((end_note.trim().parse().ok()?, value.trim().parse().ok()?))
}

fn firered_keysplit_header(line: &str) -> Option<(String, usize)> {
    let rest = line.strip_prefix(".set ")?;
    let (name, offset) = rest.split_once(", . - ")?;
    Some((name.trim().to_string(), offset.trim().parse().ok()?))
}

fn firered_byte_values(line: &str) -> Option<Vec<u8>> {
    let rest = line.strip_prefix(".byte ")?;
    Some(
        rest.split(',')
            .filter_map(|value| value.trim().parse::<u8>().ok())
            .collect(),
    )
}

fn fill_table_range(table: &mut [u8; 128], from: usize, to: usize, value: u8) {
    for slot in table.iter_mut().take(to.min(128)).skip(from) {
        *slot = value;
    }
}
