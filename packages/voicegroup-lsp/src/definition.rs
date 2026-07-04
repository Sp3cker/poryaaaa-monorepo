use std::path::Path;

use lsp_types::{Location, Position};
use url::Url;
use voicegroup_core::{
    catalog::{find_macro, ArgumentSchema, SymbolNamespace},
    parser::{parse_document, SourcePosition},
    project_index::ProjectIndex,
};

use crate::diagnostics::to_lsp_range;

/// Resolves a voicegroup declaration or reference under the cursor to its project index entry.
pub fn goto_definition(
    index: &ProjectIndex,
    project_root: &Path,
    text: &str,
    position: Position,
) -> Option<Location> {
    let symbol = voicegroup_symbol_at_position(text, position)?;
    let definition = index.voicegroup_definition_location(&symbol).or_else(|| {
        symbol
            .strip_prefix("voicegroup_")
            .and_then(|stripped| index.voicegroup_definition_location(stripped))
    })?;
    let uri = Url::from_file_path(project_root.join(definition.relative_path)).ok()?;
    Some(Location {
        uri: uri.as_str().parse().ok()?,
        range: to_lsp_range(&definition.range),
    })
}

/// Returns the voicegroup declaration or voicegroup-reference argument under an LSP cursor.
fn voicegroup_symbol_at_position(text: &str, position: Position) -> Option<String> {
    let position = lsp_to_source_position(position);
    let document = parse_document(text);
    for voice_group in &document.voice_groups {
        if voice_group.name.range.contains(&position) {
            return Some(voice_group.name.text.clone());
        }
        for program in &voice_group.programs {
            let Some(definition) = find_macro(&program.macro_name.text) else {
                continue;
            };
            for (argument, schema) in program.arguments.iter().zip(definition.arguments) {
                if argument.range.contains(&position)
                    && schema.schema
                        == (ArgumentSchema::Symbol {
                            namespace: SymbolNamespace::VoiceGroup,
                        })
                {
                    return Some(argument.text.clone());
                }
            }
        }
    }
    None
}

/// Converts LSP's zero-based cursor into core's one-based source position.
fn lsp_to_source_position(position: Position) -> SourcePosition {
    SourcePosition {
        line: position.line as usize + 1,
        column: position.character as usize + 1,
    }
}

#[cfg(test)]
mod tests {
    use super::goto_definition;
    use lsp_types::{Location, Position, Range};
    use std::{
        fs,
        path::Path,
        time::{SystemTime, UNIX_EPOCH},
    };
    use url::Url;
    use voicegroup_core::project_index::ProjectIndex;

    #[test]
    fn goto_definition_resolves_prefixed_keysplit_all_argument_to_include_entry() {
        let project = project_with_included_village_bridge("prefixed-keysplit-all");
        let index = ProjectIndex::load(&project).expect("load project index");
        let text = "\
voice_group route_one
\tvoice_keysplit_all voicegroup_village_bridge
";

        let location = goto_definition(&index, &project, text, Position::new(1, 25))
            .expect("definition location");

        assert_village_bridge_include_location(location, &project);
        let _ = fs::remove_dir_all(project);
    }

    #[test]
    fn goto_definition_ignores_unknown_macros_before_later_voicegroup_reference() {
        let project = project_with_included_village_bridge("unknown-macro-before-target");
        let index = ProjectIndex::load(&project).expect("load project index");
        let text = "\
voice_group route_one
\tunknown_macro ignored, 1, 2
\tvoice_keysplit_all voicegroup_village_bridge
";

        let location = goto_definition(&index, &project, text, Position::new(2, 25))
            .expect("definition location");

        assert_village_bridge_include_location(location, &project);
        let _ = fs::remove_dir_all(project);
    }

    fn project_with_included_village_bridge(name: &str) -> std::path::PathBuf {
        let project = temp_project_root(name);
        fs::create_dir_all(project.join("sound/voicegroups")).expect("create voicegroup dir");
        fs::write(
            project.join("sound/voice_groups.inc"),
            ".include \"sound/voicegroups/village_bridge.inc\"\n",
        )
        .expect("write voicegroup include table");
        fs::write(
            project.join("sound/voicegroups/village_bridge.inc"),
            "\
voice_group village_bridge
\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
",
        )
        .expect("write village bridge voicegroup");
        project
    }

    fn assert_village_bridge_include_location(location: Location, project: &Path) {
        let expected_uri = Url::from_file_path(project.join("sound/voice_groups.inc"))
            .expect("convert definition path to url")
            .as_str()
            .parse()
            .expect("convert definition url to lsp uri");
        assert_eq!(
            location,
            Location {
                uri: expected_uri,
                range: Range::new(Position::new(0, 10), Position::new(0, 46)),
            }
        );
    }

    fn temp_project_root(name: &str) -> std::path::PathBuf {
        let nonce = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("system time after epoch")
            .as_nanos();
        std::env::temp_dir().join(format!("voicegroup-lsp-definition-{name}-{nonce}"))
    }
}
