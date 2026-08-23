//! Pest-backed parser that turns voicegroup source text into source-ranged parsed documents.

use pest::iterators::Pair;
use pest::Parser as PestParser;
use pest_derive::Parser;

pub use crate::ast::{
    Diagnostic, DiagnosticScope, DiagnosticSeverity, ParsedArgument, ParsedAssemblyLabel,
    ParsedDocument, ParsedIdentifier, ParsedProgram, ParsedVoiceGroup, SourcePosition, SourceRange,
};

#[derive(Parser)]
#[grammar = "voicegroup.pest"]
struct VoicegroupParser;

pub fn parse_document(text: &str) -> ParsedDocument {
    let mut document = ParsedDocument {
        voice_groups: Vec::new(),
        assembly_labels: Vec::new(),
        diagnostics: Vec::new(),
    };
    let mut current_voice_group = None;
    let mut current_slot = 0;
    let mut pending_label = None;

    for (line_index, line) in text.lines().enumerate() {
        let line_number = line_index + 1;
        let parsed_line = parse_source_line(line, line_number);
        let has_diagnostics = !parsed_line.diagnostics.is_empty();
        document.diagnostics.extend(parsed_line.diagnostics);

        match parsed_line.line {
            None => {
                if has_diagnostics && current_voice_group.is_none() {
                    pending_label = None;
                }
            }
            Some(ParsedLine::Directive) => {
                if current_voice_group.is_none() {
                    pending_label = None;
                }
            }
            Some(ParsedLine::VoiceGroup(voice_group)) => {
                current_slot = voice_group.start_slot;
                document.voice_groups.push(voice_group);
                current_voice_group = Some(document.voice_groups.len() - 1);
                pending_label = None;
            }
            Some(ParsedLine::AssemblyLabel(label)) => {
                document.assembly_labels.push(label.clone());
                pending_label = Some(label);
                current_voice_group = None;
                current_slot = 0;
            }
            Some(ParsedLine::MacroCall(macro_call)) => {
                let voice_group_index = match current_voice_group {
                    Some(index) => index,
                    None => {
                        let Some(label) = pending_label.take() else {
                            document.diagnostics.push(Diagnostic::error(
                                macro_call.macro_name.range,
                                "macro-outside-voice-group",
                                "voice macro appears before a voice_group declaration",
                            ));
                            continue;
                        };
                        document.voice_groups.push(ParsedVoiceGroup {
                            name: label.name,
                            start_slot: 0,
                            declaration_range: label.range,
                            programs: Vec::new(),
                        });
                        let index = document.voice_groups.len() - 1;
                        current_voice_group = Some(index);
                        current_slot = 0;
                        index
                    }
                };

                document.voice_groups[voice_group_index]
                    .programs
                    .push(macro_call.into_program(current_slot));
                current_slot += 1;
            }
        }
    }

    document
}

struct ParsedSourceLine {
    line: Option<ParsedLine>,
    diagnostics: Vec<Diagnostic>,
}

enum ParsedLine {
    Directive,
    VoiceGroup(ParsedVoiceGroup),
    AssemblyLabel(ParsedAssemblyLabel),
    MacroCall(ParsedMacroCall),
}

struct ParsedMacroCall {
    macro_name: ParsedIdentifier,
    arguments: Vec<ParsedArgument>,
    trailing_comment: Option<String>,
    range: SourceRange,
}

impl ParsedMacroCall {
    fn into_program(self, slot: usize) -> ParsedProgram {
        ParsedProgram {
            slot,
            macro_name: self.macro_name,
            arguments: self.arguments,
            trailing_comment: self.trailing_comment,
            range: self.range,
        }
    }
}

fn parse_source_line(line: &str, line_number: usize) -> ParsedSourceLine {
    let mut diagnostics = Vec::new();
    let mut parsed = match VoicegroupParser::parse(Rule::source_line, line) {
        Ok(parsed) => parsed,
        Err(_) => {
            let diagnostic = parse_invalid_line_diagnostic(line, line_number);
            return ParsedSourceLine {
                line: None,
                diagnostics: diagnostic.into_iter().collect(),
            };
        }
    };

    let source_line = parsed.next().expect("source_line pair exists");
    let mut source_code = None;
    let mut trailing_comment = None;

    for pair in source_line.into_inner() {
        match pair.as_rule() {
            Rule::source_code => source_code = Some(pair),
            Rule::comment => trailing_comment = Some(pair.as_str()[1..].trim().to_string()),
            _ => {}
        }
    }

    let line = source_code
        .map(|pair| parse_source_code(pair, line_number, trailing_comment, &mut diagnostics));

    ParsedSourceLine { line, diagnostics }
}

fn parse_source_code(
    source_code: Pair<'_, Rule>,
    line_number: usize,
    trailing_comment: Option<String>,
    diagnostics: &mut Vec<Diagnostic>,
) -> ParsedLine {
    let pair = source_code
        .into_inner()
        .next()
        .expect("source_code has child");

    match pair.as_rule() {
        Rule::directive => ParsedLine::Directive,
        Rule::voice_group_declaration => ParsedLine::VoiceGroup(parse_voice_group_declaration(
            pair,
            line_number,
            diagnostics,
        )),
        Rule::assembly_label => ParsedLine::AssemblyLabel(parse_assembly_label(pair, line_number)),
        Rule::macro_call => ParsedLine::MacroCall(parse_macro_call(
            pair,
            line_number,
            trailing_comment,
            diagnostics,
        )),
        _ => unreachable!("source_code child rule"),
    }
}

fn parse_voice_group_declaration(
    declaration: Pair<'_, Rule>,
    line_number: usize,
    diagnostics: &mut Vec<Diagnostic>,
) -> ParsedVoiceGroup {
    let declaration_range = range_for_span(line_number, declaration.as_span());
    let mut name = None;
    let mut start_slot = 0;

    for pair in declaration.into_inner() {
        match pair.as_rule() {
            Rule::voice_group_name => name = Some(identifier_from_pair(line_number, pair)),
            Rule::start_slot => match pair.as_str().parse::<usize>() {
                Ok(parsed) => start_slot = parsed,
                Err(_) => diagnostics.push(Diagnostic::error(
                    range_for_span(line_number, pair.as_span()),
                    "invalid-start-slot",
                    "voice_group start slot is too large",
                )),
            },
            _ => {}
        }
    }

    ParsedVoiceGroup {
        name: name.expect("voice_group_declaration has name"),
        start_slot,
        declaration_range,
        programs: Vec::new(),
    }
}

fn parse_assembly_label(label: Pair<'_, Rule>, line_number: usize) -> ParsedAssemblyLabel {
    let range = range_for_span(line_number, label.as_span());
    let mut name = None;

    for pair in label.into_inner() {
        if pair.as_rule() == Rule::label_name {
            name = Some(identifier_from_pair(line_number, pair));
        }
    }

    ParsedAssemblyLabel {
        name: name.expect("assembly_label has name"),
        range,
    }
}

fn parse_macro_call(
    macro_call: Pair<'_, Rule>,
    line_number: usize,
    trailing_comment: Option<String>,
    diagnostics: &mut Vec<Diagnostic>,
) -> ParsedMacroCall {
    let range = range_for_span(line_number, macro_call.as_span());
    let mut macro_name = None;
    let mut arguments = Vec::new();

    for pair in macro_call.into_inner() {
        match pair.as_rule() {
            Rule::macro_name => macro_name = Some(identifier_from_pair(line_number, pair)),
            Rule::argument_list => arguments = parse_argument_list(pair, line_number, diagnostics),
            _ => {}
        }
    }

    ParsedMacroCall {
        macro_name: macro_name.expect("macro_call has macro_name"),
        arguments,
        trailing_comment,
        range,
    }
}

fn parse_argument_list(
    argument_list: Pair<'_, Rule>,
    line_number: usize,
    diagnostics: &mut Vec<Diagnostic>,
) -> Vec<ParsedArgument> {
    argument_list
        .into_inner()
        .filter(|pair| pair.as_rule() == Rule::argument)
        .map(|pair| parse_argument(pair, line_number, diagnostics))
        .collect()
}

fn parse_argument(
    argument: Pair<'_, Rule>,
    line_number: usize,
    diagnostics: &mut Vec<Diagnostic>,
) -> ParsedArgument {
    let mut value = None;

    for pair in argument.clone().into_inner() {
        if pair.as_rule() == Rule::argument_value {
            value = Some(pair);
        }
    }

    let Some(value) = value else {
        let range = empty_range_at_span_end(line_number, argument.as_span());
        diagnostics.push(Diagnostic::error(
            range.clone(),
            "empty-argument",
            "voice macro argument is empty",
        ));
        return ParsedArgument {
            text: String::new(),
            range,
        };
    };

    ParsedArgument {
        text: value.as_str().to_string(),
        range: range_for_span(line_number, value.as_span()),
    }
}

fn parse_invalid_line_diagnostic(line: &str, line_number: usize) -> Option<Diagnostic> {
    if let Some(range) = parse_invalid_range(
        line,
        line_number,
        Rule::invalid_voice_group_line,
        Rule::invalid_voice_group_code,
    ) {
        return Some(Diagnostic::error(
            range,
            "invalid-voice-group",
            "voice_group declaration could not be parsed",
        ));
    }

    parse_invalid_range(
        line,
        line_number,
        Rule::invalid_source_line,
        Rule::invalid_code,
    )
    .map(|range| {
        Diagnostic::error(
            range,
            "unrecognized-line",
            "line is not a voicegroup declaration, label, directive, or voice macro",
        )
    })
}

fn parse_invalid_range(
    line: &str,
    line_number: usize,
    line_rule: Rule,
    code_rule: Rule,
) -> Option<SourceRange> {
    let mut parsed = VoicegroupParser::parse(line_rule, line).ok()?;
    let source_line = parsed.next()?;

    source_line
        .into_inner()
        .find(|pair| pair.as_rule() == code_rule)
        .map(|pair| range_for_span(line_number, pair.as_span()))
}

fn identifier_from_pair(line_number: usize, pair: Pair<'_, Rule>) -> ParsedIdentifier {
    ParsedIdentifier {
        text: pair.as_str().to_string(),
        range: range_for_span(line_number, pair.as_span()),
    }
}

fn range_for_span(line: usize, span: pest::Span<'_>) -> SourceRange {
    SourceRange {
        start: SourcePosition {
            line,
            column: span.start() + 1,
        },
        end: SourcePosition {
            line,
            column: span.end() + 1,
        },
    }
}

fn empty_range_at_span_end(line: usize, span: pest::Span<'_>) -> SourceRange {
    SourceRange {
        start: SourcePosition {
            line,
            column: span.end() + 1,
        },
        end: SourcePosition {
            line,
            column: span.end() + 1,
        },
    }
}
