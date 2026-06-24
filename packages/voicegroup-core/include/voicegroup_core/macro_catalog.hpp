#ifndef VOICEGROUP_CORE_MACRO_CATALOG_HPP
#define VOICEGROUP_CORE_MACRO_CATALOG_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace voicegroup
{

enum class MacroKind
{
    // DirectSound voice loaded from a DirectSound sample symbol.
    DirectSound,
    // Alternate DirectSound voice type using the same argument shape.
    DirectSoundAlt,
    // DirectSound voice that disables m4a resampling.
    DirectSoundNoResample,
    // Hardware square channel 1 voice.
    Square1,
    // Hardware square channel 2 voice.
    Square2,
    // Hardware programmable-wave voice.
    ProgrammableWave,
    // Hardware noise voice.
    Noise,
    // Keysplit voice routed through another voicegroup and table.
    Keysplit,
    // Keysplit voice routing all notes through another voicegroup.
    KeysplitAll,
    // Cry macro backed by a DirectSound sample symbol.
    Cry,
};

enum class VoiceType : std::uint8_t
{
    // GBA/poryaaaa DirectSound type byte.
    DirectSound = 0x00,
    // GBA/poryaaaa square channel 1 type byte.
    Square1 = 0x01,
    // GBA/poryaaaa square channel 2 type byte.
    Square2 = 0x02,
    // GBA/poryaaaa programmable-wave type byte.
    ProgrammableWave = 0x03,
    // GBA/poryaaaa noise type byte.
    Noise = 0x04,
    // GBA/poryaaaa DirectSound no-resample type byte.
    DirectSoundNoResample = 0x08,
    // GBA/poryaaaa alternate square channel 1 type byte.
    Square1Alt = 0x09,
    // GBA/poryaaaa alternate square channel 2 type byte.
    Square2Alt = 0x0A,
    // GBA/poryaaaa alternate programmable-wave type byte.
    ProgrammableWaveAlt = 0x0B,
    // GBA/poryaaaa alternate noise type byte.
    NoiseAlt = 0x0C,
    // GBA/poryaaaa alternate DirectSound type byte.
    DirectSoundAlt = 0x10,
    // GBA/poryaaaa cry type byte.
    Cry = 0x20,
    // GBA/poryaaaa reverse cry type byte.
    CryReverse = 0x30,
    // GBA/poryaaaa keysplit type byte.
    Keysplit = 0x40,
    // GBA/poryaaaa keysplit-all type byte.
    KeysplitAll = 0x80,
};

enum class ArgumentKind
{
    Integer,
    DirectSoundSymbol,
    ProgrammableWaveSymbol,
    VoicegroupSymbol,
    KeysplitSymbol,
};

struct NumericRange
{
    // Inclusive lower bound for an integer macro argument.
    int min = 0;
    // Inclusive upper bound for an integer macro argument.
    int max = 0;
};

struct MacroArgument
{
    // Canonical argument name used by diagnostics, hovers, and generated docs.
    std::string_view name;
    // Argument category that drives parsing and semantic analysis.
    ArgumentKind kind;
    // Present only for integer arguments with a known valid range; absent for symbols and unconstrained integers.
    std::optional<NumericRange> validRange;
};

struct MacroDefinition
{
    // Assembly macro name exactly as it appears in voicegroup source.
    std::string_view name;
    // Exact GBA/poryaaaa ToneData.type byte emitted by this macro.
    VoiceType typeCode;
    // Coarser macro family used by analyzers and runtime materializers.
    MacroKind kind;
    // Ordered schema for the comma-separated macro arguments.
    std::span<const MacroArgument> arguments;
};

std::span<const MacroDefinition> allMacros();
const MacroDefinition* findMacro(std::string_view name);

} // namespace voicegroup

#endif // VOICEGROUP_CORE_MACRO_CATALOG_HPP
