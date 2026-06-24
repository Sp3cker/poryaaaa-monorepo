#ifndef VOICEGROUP_CORE_VOICEGROUP_DOCUMENT_HPP
#define VOICEGROUP_CORE_VOICEGROUP_DOCUMENT_HPP

#include "voicegroup_core/diagnostic.hpp"
#include "voicegroup_core/macro_catalog.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace voicegroup
{

class ProjectIndex;

struct ResolvedAsset
{
    // Assembly symbol referenced by a macro argument.
    std::string symbol;
    // Project-relative path declared for the symbol in source data files.
    std::string relativePath;
    // User-facing basename derived from relativePath.
    std::string displayName;
};

struct ParsedProgram
{
    // Program slot in the bank's 128-entry ToneData array.
    int slot = 0;
    // Macro catalog entry matched from the source line; null only if future parser recovery keeps unknown macros as
    // records.
    const MacroDefinition* macro = nullptr;
    // Raw comma-separated argument text, kept in source order.
    std::vector<std::string> rawArguments;
    // Present only when the source line has non-empty text after '@'; absent when the macro line has no display
    // comment.
    std::optional<std::string> trailingComment;
    // Full source range of the macro call.
    SourceRange range;
};

struct Envelope
{
    // Attack byte from the voice macro.
    int attack = 0;
    // Decay byte from the voice macro.
    int decay = 0;
    // Sustain byte from the voice macro.
    int sustain = 0;
    // Release byte from the voice macro.
    int release = 0;
};

struct DirectSoundProgram
{
    // Root MIDI key argument used by poryaaaa pitch materialization.
    int baseMidiKey = 0;
    // m4a pan argument from the source macro.
    int pan = 0;
    // DirectSound sample asset resolved from the macro's sample symbol.
    ResolvedAsset asset;
    // ADSR envelope bytes from the DirectSound macro.
    Envelope envelope;
};

struct ProgrammableWaveProgram
{
    // Root MIDI key argument used by poryaaaa pitch materialization.
    int baseMidiKey = 0;
    // m4a pan argument from the source macro.
    int pan = 0;
    // Programmable-wave asset resolved from the macro's wave symbol.
    ResolvedAsset asset;
    // ADSR envelope nibbles from the programmable-wave macro.
    Envelope envelope;
};

struct Square1Program
{
    // Root MIDI key argument used by poryaaaa pitch materialization.
    int baseMidiKey = 0;
    // m4a pan argument from the source macro.
    int pan = 0;
    // Square 1 sweep byte from the source macro.
    int sweep = 0;
    // Hardware duty-cycle argument from the source macro.
    int dutyCycle = 0;
    // ADSR envelope nibbles from the square macro.
    Envelope envelope;
};

struct Square2Program
{
    // Root MIDI key argument used by poryaaaa pitch materialization.
    int baseMidiKey = 0;
    // m4a pan argument from the source macro.
    int pan = 0;
    // Hardware duty-cycle argument from the source macro.
    int dutyCycle = 0;
    // ADSR envelope nibbles from the square macro.
    Envelope envelope;
};

struct NoiseProgram
{
    // Root MIDI key argument used by poryaaaa noise materialization.
    int baseMidiKey = 0;
    // m4a pan argument accepted by the macro.
    int pan = 0;
    // Hardware noise period bit from the source macro.
    int period = 0;
    // ADSR envelope nibbles from the noise macro.
    Envelope envelope;
};

struct KeysplitProgram
{
    // Sub-bank symbol poryaaaa materializes recursively.
    std::string subVoicegroupSymbol;
    // Parsed keysplit table bytes poryaaaa copies into owned runtime memory.
    std::vector<unsigned char> keysplitTable;
};

struct KeysplitAllProgram
{
    // Sub-bank symbol poryaaaa materializes recursively for all notes.
    std::string subVoicegroupSymbol;
};

struct CryProgram
{
    // Cry sample asset resolved from the macro's DirectSound symbol.
    ResolvedAsset asset;
};

using ProgramData = std::variant<DirectSoundProgram,
                                 ProgrammableWaveProgram,
                                 Square1Program,
                                 Square2Program,
                                 NoiseProgram,
                                 KeysplitProgram,
                                 KeysplitAllProgram,
                                 CryProgram>;

struct ResolvedProgram
{
    // Syntax-level program record returned by parseDocument().
    ParsedProgram parsed;
    // Typed program payload; consumers inspect the active alternative instead of probing nullable fields.
    ProgramData data;
};

struct ParsedSection
{
    // Source label that delimits a voicegroup section.
    std::string label;
    // Source range of the section label.
    SourceRange range;
    // Macro calls parsed within this section.
    std::vector<ParsedProgram> programs;
};

struct ParsedDocument
{
    // Voicegroup sections found in the document.
    std::vector<ParsedSection> sections;
    // Syntax diagnostics produced without consulting project files.
    std::vector<Diagnostic> diagnostics;
};

struct LoadedBank
{
    // Requested bank name, normalized as the caller supplied it.
    std::string name;
    // Project-relative source file path used to load this bank.
    std::string sourceRelativePath;
    // Each index is a ToneData slot; entries are absent for slots not defined by the source bank.
    std::array<std::optional<ResolvedProgram>, 128> programs;
    // Syntax and semantic diagnostics gathered while loading the bank.
    std::vector<Diagnostic> diagnostics;
};

ParsedDocument parseDocument(std::string_view text);
std::vector<Diagnostic> analyze(const ParsedDocument& document, const ProjectIndex& index);

} // namespace voicegroup

#endif // VOICEGROUP_CORE_VOICEGROUP_DOCUMENT_HPP
