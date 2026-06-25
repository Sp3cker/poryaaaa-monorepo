use std::collections::{BTreeMap, BTreeSet};

use crate::ast::{
    Diagnostic, DiagnosticSeverity, ParsedArgument, ParsedDocument, ParsedProgram,
    ParsedVoiceGroup, SourceRange,
};
use crate::catalog::{find_macro, ArgumentSchema, MacroArgument, MacroDefinition, SymbolNamespace};

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct AnalysisContext {
    symbols: BTreeMap<SymbolNamespace, BTreeSet<String>>,
}

impl AnalysisContext {
    pub fn with_symbols<I, S>(mut self, namespace: SymbolNamespace, symbols: I) -> Self
    where
        I: IntoIterator<Item = S>,
        S: Into<String>,
    {
        self.symbols
            .entry(namespace)
            .or_default()
            .extend(symbols.into_iter().map(Into::into));
        self
    }

    fn has_symbol(&self, namespace: SymbolNamespace, symbol: &str) -> bool {
        self.symbols
            .get(&namespace)
            .is_some_and(|symbols| symbols.contains(symbol))
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum VoiceGroupSymbolStatus {
    Known,
    Ambiguous,
    Unknown,
}

pub fn analyze_document(document: &ParsedDocument, context: &AnalysisContext) -> Vec<Diagnostic> {
    let mut diagnostics = document.diagnostics.clone();
    diagnostics.extend(analyze_semantics(document, context));
    diagnostics
}

pub fn analyze_semantics(document: &ParsedDocument, context: &AnalysisContext) -> Vec<Diagnostic> {
    let mut diagnostics = Vec::new();
    let local_voice_groups = local_voice_group_symbols(document);
    let mut group_slots: BTreeMap<String, [bool; 128]> = BTreeMap::new();
    let mut seen_group_names = BTreeSet::new();

    for voice_group in &document.voice_groups {
        if !seen_group_names.insert(voice_group.name.text.as_str()) {
            diagnostics.push(error(
                voice_group.name.range.clone(),
                "duplicate-voice-group",
                "voice_group name appears more than once in this document",
            ));
        }

        analyze_voice_group_slots(voice_group, &mut group_slots, &mut diagnostics);

        for program in &voice_group.programs {
            let Some(definition) = find_macro(&program.macro_name.text) else {
                diagnostics.push(error(
                    program.macro_name.range.clone(),
                    "unknown-macro",
                    "voice macro is not defined in the macro catalog",
                ));
                continue;
            };

            analyze_program_arguments(
                program,
                definition,
                context,
                &local_voice_groups,
                &mut diagnostics,
            );
        }
    }

    diagnostics
}

fn local_voice_group_symbols(document: &ParsedDocument) -> BTreeMap<String, usize> {
    let mut symbols = BTreeMap::new();
    for voice_group in &document.voice_groups {
        *symbols.entry(voice_group.name.text.clone()).or_insert(0) += 1;
    }
    symbols
}

fn analyze_voice_group_slots(
    voice_group: &ParsedVoiceGroup,
    group_slots: &mut BTreeMap<String, [bool; 128]>,
    diagnostics: &mut Vec<Diagnostic>,
) {
    let slots = group_slots
        .entry(voice_group.name.text.clone())
        .or_insert([false; 128]);

    for program in &voice_group.programs {
        if program.slot >= 128 {
            diagnostics.push(error(
                program.range.clone(),
                "slot-out-of-range",
                "voice macro program slot is outside the 0..127 bank range",
            ));
            continue;
        }

        if slots[program.slot] {
            diagnostics.push(error(
                program.range.clone(),
                "duplicate-slot",
                "voice macro populates a slot already used by this voice_group",
            ));
        } else {
            slots[program.slot] = true;
        }
    }
}

fn analyze_program_arguments(
    program: &ParsedProgram,
    definition: &MacroDefinition,
    context: &AnalysisContext,
    local_voice_groups: &BTreeMap<String, usize>,
    diagnostics: &mut Vec<Diagnostic>,
) {
    if program.arguments.len() != definition.arguments.len() {
        diagnostics.push(error(
            program.range.clone(),
            "wrong-argument-count",
            "voice macro argument count does not match the macro catalog",
        ));
        return;
    }

    for (argument, schema) in program.arguments.iter().zip(definition.arguments) {
        analyze_argument(argument, schema, context, local_voice_groups, diagnostics);
    }
}

fn analyze_argument(
    argument: &ParsedArgument,
    schema: &MacroArgument,
    context: &AnalysisContext,
    local_voice_groups: &BTreeMap<String, usize>,
    diagnostics: &mut Vec<Diagnostic>,
) {
    if argument.text.is_empty() {
        return;
    }

    match schema.schema {
        ArgumentSchema::Integer { range } => analyze_integer_argument(argument, range, diagnostics),
        ArgumentSchema::Symbol { namespace } => analyze_symbol_argument(
            argument,
            namespace,
            context,
            local_voice_groups,
            diagnostics,
        ),
    }
}

fn analyze_integer_argument(
    argument: &ParsedArgument,
    valid_range: crate::catalog::NumericRange,
    diagnostics: &mut Vec<Diagnostic>,
) {
    let Ok(value) = argument.text.parse::<i32>() else {
        diagnostics.push(error(
            argument.range.clone(),
            "invalid-integer",
            "macro argument must be an integer",
        ));
        return;
    };

    if value < valid_range.min || value > valid_range.max {
        diagnostics.push(error(
            argument.range.clone(),
            "integer-out-of-range",
            "macro integer argument is outside the valid range",
        ));
    }
}

fn analyze_symbol_argument(
    argument: &ParsedArgument,
    namespace: SymbolNamespace,
    context: &AnalysisContext,
    local_voice_groups: &BTreeMap<String, usize>,
    diagnostics: &mut Vec<Diagnostic>,
) {
    if namespace == SymbolNamespace::VoiceGroup {
        match voice_group_symbol_status(&argument.text, context, local_voice_groups) {
            VoiceGroupSymbolStatus::Known => {}
            VoiceGroupSymbolStatus::Ambiguous => diagnostics.push(error(
                argument.range.clone(),
                "ambiguous-voicegroup-symbol",
                "voice_group symbol has multiple local declarations",
            )),
            VoiceGroupSymbolStatus::Unknown => diagnostics.push(error(
                argument.range.clone(),
                unknown_symbol_code(namespace),
                "symbol is not declared in the analysis context",
            )),
        }
        return;
    }

    if !context.has_symbol(namespace, &argument.text) {
        diagnostics.push(error(
            argument.range.clone(),
            unknown_symbol_code(namespace),
            "symbol is not declared in the analysis context",
        ));
    }
}

fn voice_group_symbol_status(
    symbol: &str,
    context: &AnalysisContext,
    local_voice_groups: &BTreeMap<String, usize>,
) -> VoiceGroupSymbolStatus {
    match local_voice_groups.get(symbol).copied().unwrap_or_default() {
        0 => {
            if context.has_symbol(SymbolNamespace::VoiceGroup, symbol) {
                VoiceGroupSymbolStatus::Known
            } else {
                VoiceGroupSymbolStatus::Unknown
            }
        }
        1 => VoiceGroupSymbolStatus::Known,
        _ => VoiceGroupSymbolStatus::Ambiguous,
    }
}

fn unknown_symbol_code(namespace: SymbolNamespace) -> &'static str {
    match namespace {
        SymbolNamespace::DirectSound => "unknown-directsound-symbol",
        SymbolNamespace::ProgrammableWave => "unknown-programmable-wave-symbol",
        SymbolNamespace::VoiceGroup => "unknown-voicegroup-symbol",
        SymbolNamespace::Keysplit => "unknown-keysplit-symbol",
    }
}

fn error(range: SourceRange, code: &str, message: &str) -> Diagnostic {
    Diagnostic {
        range,
        severity: DiagnosticSeverity::Error,
        code: code.to_string(),
        message: message.to_string(),
    }
}
