#include "voicegroup_core/project_index.hpp"
#include "voicegroup_core/voicegroup_document.hpp"

#include <charconv>
#include <cctype>
#include <string>

namespace voicegroup
{
namespace
{

std::string_view trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
        text.remove_suffix(1);
    return text;
}

std::optional<int> parseInt(std::string_view text)
{
    text = trim(text);
    int value = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end)
        return std::nullopt;
    return value;
}

std::vector<std::string> splitArguments(std::string_view text)
{
    std::vector<std::string> arguments;
    while (true)
    {
        const std::size_t comma = text.find(',');
        const std::string_view argument = trim(text.substr(0, comma));
        arguments.emplace_back(argument);
        if (comma == std::string_view::npos)
            break;
        text.remove_prefix(comma + 1);
    }
    return arguments;
}

bool parseVoiceGroup(std::string_view line, int& voiceIndex)
{
    constexpr std::string_view prefix = "voice_group";
    if (!line.starts_with(prefix))
        return false;

    std::string_view rest = trim(line.substr(prefix.size()));
    const std::size_t comma = rest.find(',');
    if (comma == std::string_view::npos)
    {
        voiceIndex = 0;
        return true;
    }

    if (const std::optional<int> parsed = parseInt(rest.substr(comma + 1)))
        voiceIndex = *parsed;
    return true;
}

} // namespace

ParsedDocument parseDocument(std::string_view text)
{
    ParsedDocument document;
    ParsedSection* currentSection = nullptr;
    int voiceIndex = 0;
    int lineNumber = 0;

    while (!text.empty())
    {
        std::string_view line = text;
        const std::size_t newline = text.find('\n');
        if (newline != std::string_view::npos)
        {
            line = text.substr(0, newline);
            text.remove_prefix(newline + 1);
        }
        else
        {
            text = {};
        }

        const std::size_t commentStart = line.find('@');
        std::optional<std::string> trailingComment;
        std::string_view code = line;
        if (commentStart != std::string_view::npos)
        {
            code = line.substr(0, commentStart);
            const std::string_view comment = trim(line.substr(commentStart + 1));
            if (!comment.empty())
                trailingComment = std::string(comment);
        }

        const std::string_view trimmed = trim(code);
        if (trimmed.empty())
        {
            ++lineNumber;
            continue;
        }

        if (trimmed.ends_with("::"))
        {
            ParsedSection section;
            section.label = std::string(trim(trimmed.substr(0, trimmed.size() - 2)));
            section.range = {{lineNumber, 0}, {lineNumber, static_cast<int>(line.size())}};
            document.sections.push_back(std::move(section));
            currentSection = &document.sections.back();
            voiceIndex = 0;
            ++lineNumber;
            continue;
        }

        if (parseVoiceGroup(trimmed, voiceIndex))
        {
            if (currentSection == nullptr)
            {
                document.sections.push_back({});
                currentSection = &document.sections.back();
            }
            ++lineNumber;
            continue;
        }

        const std::size_t macroEnd = trimmed.find_first_of(" \t");
        const std::string_view macroName = macroEnd == std::string_view::npos ? trimmed : trimmed.substr(0, macroEnd);
        const MacroDefinition* macro = findMacro(macroName);
        if (macro == nullptr)
        {
            document.diagnostics.push_back({{{lineNumber, 0}, {lineNumber, static_cast<int>(line.size())}},
                                            DiagnosticSeverity::Warning,
                                            "unknown-macro",
                                            "Unknown voice macro."});
            ++lineNumber;
            continue;
        }

        if (currentSection == nullptr)
        {
            document.sections.push_back({});
            currentSection = &document.sections.back();
        }

        ParsedProgram program;
        program.slot = voiceIndex++;
        program.macro = macro;
        program.trailingComment = std::move(trailingComment);
        program.range = {{lineNumber, 0}, {lineNumber, static_cast<int>(line.size())}};
        if (macroEnd != std::string_view::npos)
            program.rawArguments = splitArguments(trimmed.substr(macroEnd + 1));
        currentSection->programs.push_back(std::move(program));
        ++lineNumber;
    }

    return document;
}

std::vector<Diagnostic> analyze(const ParsedDocument& document, const ProjectIndex& index)
{
    std::vector<Diagnostic> diagnostics = document.diagnostics;

    for (const ParsedSection& section : document.sections)
    {
        bool seenSlots[128] = {};
        for (const ParsedProgram& program : section.programs)
        {
            if (program.slot < 0 || program.slot >= 128)
            {
                diagnostics.push_back(
                    {program.range, DiagnosticSeverity::Error, "slot-out-of-range", "Voice slot is outside 0...127."});
                continue;
            }
            if (seenSlots[program.slot])
                diagnostics.push_back({program.range,
                                       DiagnosticSeverity::Error,
                                       "duplicate-slot",
                                       "Voice slot is defined more than once."});
            seenSlots[program.slot] = true;

            if (program.macro == nullptr)
                continue;
            if (program.rawArguments.size() != program.macro->arguments.size())
            {
                diagnostics.push_back({program.range,
                                       DiagnosticSeverity::Error,
                                       "wrong-argument-count",
                                       "Voice macro has the wrong argument count."});
                continue;
            }

            for (std::size_t i = 0; i < program.rawArguments.size(); ++i)
            {
                const MacroArgument& expected = program.macro->arguments[i];
                const std::string& argument = program.rawArguments[i];
                switch (expected.kind)
                {
                case ArgumentKind::Integer:
                {
                    const std::optional<int> value = parseInt(argument);
                    if (!value)
                    {
                        diagnostics.push_back({program.range,
                                               DiagnosticSeverity::Error,
                                               "invalid-integer",
                                               "Voice macro argument must be an integer."});
                        break;
                    }
                    if (expected.validRange && (*value < expected.validRange->min || *value > expected.validRange->max))
                        diagnostics.push_back({program.range,
                                               DiagnosticSeverity::Warning,
                                               "invalid-range",
                                               "Voice macro argument is outside its valid range."});
                    break;
                }
                case ArgumentKind::DirectSoundSymbol:
                    if (!index.hasDirectSoundSymbol(argument))
                        diagnostics.push_back({program.range,
                                               DiagnosticSeverity::Warning,
                                               "unknown-directsound-symbol",
                                               "Unknown DirectSound symbol."});
                    break;
                case ArgumentKind::ProgrammableWaveSymbol:
                    if (!index.hasProgrammableWaveSymbol(argument))
                        diagnostics.push_back({program.range,
                                               DiagnosticSeverity::Warning,
                                               "unknown-programmable-wave-symbol",
                                               "Unknown programmable wave symbol."});
                    break;
                case ArgumentKind::VoicegroupSymbol:
                    if (!index.hasVoicegroup(argument))
                        diagnostics.push_back({program.range,
                                               DiagnosticSeverity::Warning,
                                               "unknown-voicegroup-symbol",
                                               "Unknown voicegroup symbol."});
                    break;
                case ArgumentKind::KeysplitSymbol:
                    if (!index.hasKeysplitSymbol(argument))
                        diagnostics.push_back({program.range,
                                               DiagnosticSeverity::Warning,
                                               "unknown-keysplit-symbol",
                                               "Unknown keysplit symbol."});
                    break;
                }
            }
        }
    }

    return diagnostics;
}

} // namespace voicegroup
