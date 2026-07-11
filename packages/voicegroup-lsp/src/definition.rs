use std::path::Path;

use lsp_types::{Location, Position, SemanticToken, SemanticTokens};
use url::Url;
use voicegroup_core::{
    catalog::{find_macro, ArgumentSchema, SymbolNamespace},
    parser::{parse_document, SourcePosition, SourceRange},
    project_index::ProjectIndex,
};

use crate::diagnostics::to_lsp_range;

pub const SEMANTIC_TOKEN_TYPES: [&str; 2] = ["subVoiceGroup", "keysplitReference"];
const SUB_VOICEGROUP_TOKEN_INDEX: u32 = 0;
const KEYSPLIT_REFERENCE_TOKEN_INDEX: u32 = 1;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ReferenceKind {
    SubVoiceGroup,
    Keysplit,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct ReferenceToken {
    symbol: String,
    range: SourceRange,
    kind: ReferenceKind,
}

/// Adapts an LSP goto-definition request to core's project navigation result.
pub fn goto_definition(
    index: &ProjectIndex,
    project_root: &Path,
    relative_path: &str,
    text: &str,
    position: Position,
) -> Option<Location> {
    let definition = index.definition_at(relative_path, text, lsp_to_source_position(position))?;
    let uri = Url::from_file_path(project_root.join(definition.relative_path)).ok()?;
    Some(Location {
        uri: uri.as_str().parse().ok()?,
        range: to_lsp_range(&definition.range),
    })
}

/// Builds full-document semantic tokens for voicegroup-domain references.
pub fn semantic_tokens_for_text(text: &str) -> SemanticTokens {
    let mut tokens: Vec<_> = reference_tokens(text)
        .into_iter()
        .map(|reference| (reference.range, semantic_token_type(reference.kind)))
        .collect();
    tokens.sort_by_key(|(range, _)| (range.start.line, range.start.column));

    let mut previous_line = 0;
    let mut previous_start = 0;
    let data = tokens
        .into_iter()
        .map(|(range, token_type)| {
            let line = range.start.line - 1;
            let start = range.start.column - 1;
            let delta_line = line - previous_line;
            let delta_start = if delta_line == 0 {
                start - previous_start
            } else {
                start
            };
            previous_line = line;
            previous_start = start;
            SemanticToken {
                delta_line: delta_line as u32,
                delta_start: delta_start as u32,
                length: (range.end.column - range.start.column) as u32,
                token_type,
                token_modifiers_bitset: 0,
            }
        })
        .collect();

    SemanticTokens {
        result_id: None,
        data,
    }
}

/// Returns source tokens that editor features treat as voicegroup-domain references.
fn reference_tokens(text: &str) -> Vec<ReferenceToken> {
    let document = parse_document(text);
    let mut tokens = Vec::new();
    for voice_group in &document.voice_groups {
        for program in &voice_group.programs {
            let Some(definition) = find_macro(&program.macro_name.text) else {
                continue;
            };
            for (argument, schema) in program.arguments.iter().zip(definition.arguments.iter()) {
                let ArgumentSchema::Symbol { namespace } = schema.schema else {
                    continue;
                };
                let kind = match namespace {
                    SymbolNamespace::VoiceGroup => ReferenceKind::SubVoiceGroup,
                    SymbolNamespace::Keysplit => ReferenceKind::Keysplit,
                    SymbolNamespace::DirectSound | SymbolNamespace::ProgrammableWave => continue,
                };
                tokens.push(ReferenceToken {
                    symbol: argument.text.clone(),
                    range: argument.range.clone(),
                    kind,
                });
            }
        }
    }
    tokens
}

fn semantic_token_type(kind: ReferenceKind) -> u32 {
    match kind {
        ReferenceKind::SubVoiceGroup => SUB_VOICEGROUP_TOKEN_INDEX,
        ReferenceKind::Keysplit => KEYSPLIT_REFERENCE_TOKEN_INDEX,
    }
}
fn lsp_to_source_position(position: Position) -> SourcePosition {
    SourcePosition {
        line: position.line as usize + 1,
        column: position.character as usize + 1,
    }
}

#[cfg(test)]
mod tests {
    use super::{goto_definition, reference_tokens, semantic_tokens_for_text, ReferenceKind};
    use lsp_types::{Location, Position, Range, SemanticToken};
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

        let location = goto_definition(
            &index,
            &project,
            "sound/voicegroups/route_one.inc",
            text,
            Position::new(1, 25),
        )
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

        let location = goto_definition(
            &index,
            &project,
            "sound/voicegroups/route_one.inc",
            text,
            Position::new(2, 25),
        )
        .expect("definition location");

        assert_village_bridge_include_location(location, &project);
        let _ = fs::remove_dir_all(project);
    }

    #[test]
    fn goto_definition_resolves_keysplit_argument_to_table_header() {
        let project = project_with_included_village_bridge("keysplit-definition");
        fs::write(
            project.join("sound/keysplit_tables.inc"),
            "\
keysplit strings, 0
split 1, 64
",
        )
        .expect("write keysplit table");
        let index = ProjectIndex::load(&project).expect("load project index");
        let text = "\
voice_group route_one
\tvoice_keysplit voicegroup_village_bridge, keysplit_strings
";

        let location = goto_definition(
            &index,
            &project,
            "sound/voicegroups/route_one.inc",
            text,
            Position::new(1, 48),
        )
        .expect("definition location");

        let expected_uri = Url::from_file_path(project.join("sound/keysplit_tables.inc"))
            .expect("convert definition path to url")
            .as_str()
            .parse()
            .expect("convert definition url to lsp uri");
        let _ = fs::remove_dir_all(project);
        assert_eq!(
            location,
            Location {
                uri: expected_uri,
                range: Range::new(Position::new(0, 9), Position::new(0, 16)),
            }
        );
    }

    #[test]
    fn goto_definition_resolves_directsound_argument_to_data_label() {
        let project = temp_project_root("directsound-definition");
        fs::create_dir_all(project.join("sound")).expect("create sound dir");
        fs::write(
            project.join("sound/direct_sound_data.inc"),
            "\
\t.align 2
DirectSoundWaveData_Kick::
\t.incbin \"sound/direct_sound_samples/kick.bin\"
",
        )
        .expect("write direct sound data");
        let index = ProjectIndex::load(&project).expect("load project index");
        let text = "\
voice_group route_one
\tvoice_directsound 60, 0, DirectSoundWaveData_Kick, 255, 0, 255, 242
";

        let location = goto_definition(
            &index,
            &project,
            "sound/voicegroups/route_one.inc",
            text,
            Position::new(1, 27),
        )
        .expect("definition location");

        let expected_uri = Url::from_file_path(project.join("sound/direct_sound_data.inc"))
            .expect("convert definition path to url")
            .as_str()
            .parse()
            .expect("convert definition url to lsp uri");
        let _ = fs::remove_dir_all(project);
        assert_eq!(
            location,
            Location {
                uri: expected_uri,
                range: Range::new(Position::new(1, 0), Position::new(1, 24)),
            }
        );
    }

    #[test]
    fn goto_definition_resolves_programmable_wave_argument_to_data_label() {
        let project = temp_project_root("programmable-wave-definition");
        fs::create_dir_all(project.join("sound")).expect("create sound dir");
        fs::write(
            project.join("sound/programmable_wave_data.inc"),
            "\
\t.align 2
ProgrammableWaveData_Pulse::
\t.incbin \"sound/programmable_wave_samples/pulse.pcm\"
",
        )
        .expect("write programmable wave data");
        let index = ProjectIndex::load(&project).expect("load project index");
        let text = "\
voice_group route_one
\tvoice_programmable_wave 60, 0, ProgrammableWaveData_Pulse, 1, 2, 8, 3
";

        let location = goto_definition(
            &index,
            &project,
            "sound/voicegroups/route_one.inc",
            text,
            Position::new(1, 34),
        )
        .expect("definition location");

        let expected_uri = Url::from_file_path(project.join("sound/programmable_wave_data.inc"))
            .expect("convert definition path to url")
            .as_str()
            .parse()
            .expect("convert definition url to lsp uri");
        let _ = fs::remove_dir_all(project);
        assert_eq!(
            location,
            Location {
                uri: expected_uri,
                range: Range::new(Position::new(1, 0), Position::new(1, 26)),
            }
        );
    }

    #[test]
    fn goto_definition_resolves_voice_groups_include_path_to_included_file() {
        let project = project_with_included_village_bridge("include-target");
        let index = ProjectIndex::load(&project).expect("load project index");
        let text = ".include \"sound/voicegroups/village_bridge.inc\"\n";

        let location = goto_definition(
            &index,
            &project,
            "sound/voice_groups.inc",
            text,
            Position::new(0, 23),
        )
        .expect("definition location");

        let expected_uri =
            Url::from_file_path(project.join("sound/voicegroups/village_bridge.inc"))
                .expect("convert definition path to url")
                .as_str()
                .parse()
                .expect("convert definition url to lsp uri");
        let _ = fs::remove_dir_all(project);
        assert_eq!(
            location,
            Location {
                uri: expected_uri,
                range: Range::new(Position::new(0, 0), Position::new(0, 0)),
            }
        );
    }

    #[test]
    fn reference_tokens_extract_subvoicegroup_and_keysplit_arguments() {
        let text = "\
voice_group route_one
\tvoice_keysplit voicegroup_village_bridge, KeySplitTable_route_one
";

        let tokens = reference_tokens(text);

        assert!(tokens.iter().any(|token| {
            token.kind == ReferenceKind::SubVoiceGroup
                && token.symbol == "voicegroup_village_bridge"
        }));
        assert!(tokens.iter().any(|token| {
            token.kind == ReferenceKind::Keysplit && token.symbol == "KeySplitTable_route_one"
        }));
    }

    #[test]
    fn semantic_tokens_encode_subvoicegroup_and_keysplit_arguments() {
        let text = "\
voice_group route_one
\tvoice_keysplit voicegroup_village_bridge, KeySplitTable_route_one
";

        let tokens = semantic_tokens_for_text(text);

        assert_eq!(
            tokens.data,
            vec![
                SemanticToken {
                    delta_line: 1,
                    delta_start: 16,
                    length: 25,
                    token_type: 0,
                    token_modifiers_bitset: 0,
                },
                SemanticToken {
                    delta_line: 0,
                    delta_start: 27,
                    length: 23,
                    token_type: 1,
                    token_modifiers_bitset: 0,
                },
            ]
        );
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
