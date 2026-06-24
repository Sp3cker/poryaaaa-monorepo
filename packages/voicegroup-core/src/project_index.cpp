#include "voicegroup_core/project_index.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <optional>
#include <sstream>
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

std::string displayNameForRelativePath(std::string_view relativePath)
{
    const std::size_t slash = relativePath.find_last_of("/\\");
    if (slash == std::string_view::npos)
        return std::string(relativePath);
    return std::string(relativePath.substr(slash + 1));
}

void parseSymbolFile(const std::filesystem::path& filePath, std::vector<ResolvedAsset>& assets)
{
    std::ifstream file(filePath);
    if (!file)
        return;

    std::string currentSymbol;
    std::string line;
    while (std::getline(file, line))
    {
        std::string_view code = line;
        if (const std::size_t comment = code.find('@'); comment != std::string_view::npos)
            code = code.substr(0, comment);
        code = trim(code);

        if (const std::size_t label = code.find("::"); label != std::string_view::npos && label > 0)
        {
            currentSymbol = std::string(trim(code.substr(0, label)));
            continue;
        }

        if (currentSymbol.empty() || code.find(".incbin") == std::string_view::npos)
            continue;

        const std::size_t firstQuote = code.find('"');
        if (firstQuote == std::string_view::npos)
            continue;
        const std::size_t secondQuote = code.find('"', firstQuote + 1);
        if (secondQuote == std::string_view::npos)
            continue;

        const std::string relativePath(code.substr(firstQuote + 1, secondQuote - firstQuote - 1));
        assets.push_back({currentSymbol, relativePath, displayNameForRelativePath(relativePath)});
        currentSymbol.clear();
    }
}

bool containsSymbol(const std::vector<ResolvedAsset>& assets, std::string_view symbol)
{
    return std::any_of(
        assets.begin(), assets.end(), [symbol](const ResolvedAsset& asset) { return asset.symbol == symbol; });
}

const ResolvedAsset* findAsset(const std::vector<ResolvedAsset>& assets, std::string_view symbol)
{
    for (const ResolvedAsset& asset : assets)
    {
        if (asset.symbol == symbol)
            return &asset;
    }
    return nullptr;
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream file(path);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int parseRequiredInt(const std::string& text)
{
    int value = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    std::from_chars(begin, end, value);
    return value;
}

Envelope envelopeFromArguments(const ParsedProgram& program, std::size_t offset)
{
    return {parseRequiredInt(program.rawArguments[offset]),
            parseRequiredInt(program.rawArguments[offset + 1]),
            parseRequiredInt(program.rawArguments[offset + 2]),
            parseRequiredInt(program.rawArguments[offset + 3])};
}

ProgramData makeProgramData(const ParsedProgram& program,
                            const std::vector<ResolvedAsset>& directSoundAssets,
                            const std::vector<ResolvedAsset>& programmableWaveAssets)
{
    if (program.macro == nullptr)
        return NoiseProgram{};

    switch (program.macro->kind)
    {
    case MacroKind::DirectSound:
    case MacroKind::DirectSoundAlt:
    case MacroKind::DirectSoundNoResample:
        return DirectSoundProgram{parseRequiredInt(program.rawArguments[0]),
                                  parseRequiredInt(program.rawArguments[1]),
                                  *findAsset(directSoundAssets, program.rawArguments[2]),
                                  envelopeFromArguments(program, 3)};
    case MacroKind::Cry:
        return CryProgram{*findAsset(directSoundAssets, program.rawArguments[0])};
    case MacroKind::ProgrammableWave:
        return ProgrammableWaveProgram{parseRequiredInt(program.rawArguments[0]),
                                       parseRequiredInt(program.rawArguments[1]),
                                       *findAsset(programmableWaveAssets, program.rawArguments[2]),
                                       envelopeFromArguments(program, 3)};
    case MacroKind::Square1:
        return Square1Program{parseRequiredInt(program.rawArguments[0]),
                              parseRequiredInt(program.rawArguments[1]),
                              parseRequiredInt(program.rawArguments[2]),
                              parseRequiredInt(program.rawArguments[3]),
                              envelopeFromArguments(program, 4)};
    case MacroKind::Square2:
        return Square2Program{parseRequiredInt(program.rawArguments[0]),
                              parseRequiredInt(program.rawArguments[1]),
                              parseRequiredInt(program.rawArguments[2]),
                              envelopeFromArguments(program, 3)};
    case MacroKind::Noise:
        return NoiseProgram{parseRequiredInt(program.rawArguments[0]),
                            parseRequiredInt(program.rawArguments[1]),
                            parseRequiredInt(program.rawArguments[2]),
                            envelopeFromArguments(program, 3)};
    case MacroKind::Keysplit:
        return KeysplitProgram{program.rawArguments[0], {}};
    case MacroKind::KeysplitAll:
        return KeysplitAllProgram{program.rawArguments[0]};
    }
}

} // namespace

ProjectIndex ProjectIndex::load(const std::filesystem::path& projectRoot)
{
    ProjectIndex index;
    index.projectRoot_ = projectRoot;
    parseSymbolFile(projectRoot / "sound/direct_sound_data.inc", index.directSoundAssets_);
    parseSymbolFile(projectRoot / "sound/programmable_wave_data.inc", index.programmableWaveAssets_);
    return index;
}

LoadedBank ProjectIndex::loadBank(std::string_view bankName) const
{
    LoadedBank bank;
    bank.name = std::string(bankName);
    bank.sourceRelativePath = "sound/voicegroups/" + std::string(bankName) + ".inc";

    const std::filesystem::path sourcePath = projectRoot_ / bank.sourceRelativePath;
    if (!std::filesystem::exists(sourcePath))
    {
        bank.diagnostics.push_back({{{0, 0}, {0, 0}},
                                    DiagnosticSeverity::Error,
                                    "missing-bank",
                                    "Voicegroup bank source file was not found."});
        return bank;
    }

    ParsedDocument document = parseDocument(readFile(sourcePath));
    bank.diagnostics = analyze(document, *this);
    if (document.sections.empty())
        return bank;

    const ParsedSection& section = document.sections.front();
    for (ParsedProgram program : section.programs)
    {
        if (program.slot < 0 || program.slot >= static_cast<int>(bank.programs.size()))
            continue;

        ResolvedProgram resolved;
        resolved.data = makeProgramData(program, directSoundAssets_, programmableWaveAssets_);
        resolved.parsed = std::move(program);
        bank.programs[resolved.parsed.slot] = std::move(resolved);
    }

    return bank;
}

bool ProjectIndex::hasDirectSoundSymbol(std::string_view symbol) const
{
    return containsSymbol(directSoundAssets_, symbol);
}

bool ProjectIndex::hasProgrammableWaveSymbol(std::string_view symbol) const
{
    return containsSymbol(programmableWaveAssets_, symbol);
}

bool ProjectIndex::hasKeysplitSymbol(std::string_view) const
{
    return false;
}

bool ProjectIndex::hasVoicegroup(std::string_view) const
{
    return false;
}

std::vector<ResolvedAsset> ProjectIndex::directSoundAssets() const
{
    return directSoundAssets_;
}

std::vector<ResolvedAsset> ProjectIndex::programmableWaveAssets() const
{
    return programmableWaveAssets_;
}

} // namespace voicegroup
