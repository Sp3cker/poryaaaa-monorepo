#include "voicegroup_core/macro_catalog.hpp"

#include <array>

namespace voicegroup
{
namespace
{

constexpr NumericRange kMidiRange{0, 127};
constexpr NumericRange kPanRange{0, 127};
constexpr NumericRange kByteRange{0, 255};
constexpr NumericRange kDutyRange{0, 3};
constexpr NumericRange kSquareAttackDecayReleaseRange{0, 7};
constexpr NumericRange kSquareSustainRange{0, 15};
constexpr NumericRange kNoisePeriodRange{0, 1};

constexpr MacroArgument kDirectSoundArgs[] = {
    {"base_midi_key", ArgumentKind::Integer, kMidiRange},
    {"pan", ArgumentKind::Integer, kPanRange},
    {"sample_data_pointer", ArgumentKind::DirectSoundSymbol, std::nullopt},
    {"attack", ArgumentKind::Integer, kByteRange},
    {"decay", ArgumentKind::Integer, kByteRange},
    {"sustain", ArgumentKind::Integer, kByteRange},
    {"release", ArgumentKind::Integer, kByteRange},
};

constexpr MacroArgument kSquare1Args[] = {
    {"base_midi_key", ArgumentKind::Integer, kMidiRange},
    {"pan", ArgumentKind::Integer, kPanRange},
    {"sweep", ArgumentKind::Integer, kByteRange},
    {"duty_cycle", ArgumentKind::Integer, kDutyRange},
    {"attack", ArgumentKind::Integer, kSquareAttackDecayReleaseRange},
    {"decay", ArgumentKind::Integer, kSquareAttackDecayReleaseRange},
    {"sustain", ArgumentKind::Integer, kSquareSustainRange},
    {"release", ArgumentKind::Integer, kSquareAttackDecayReleaseRange},
};

constexpr MacroArgument kSquare2Args[] = {
    {"base_midi_key", ArgumentKind::Integer, kMidiRange},
    {"pan", ArgumentKind::Integer, kPanRange},
    {"duty_cycle", ArgumentKind::Integer, kDutyRange},
    {"attack", ArgumentKind::Integer, kSquareAttackDecayReleaseRange},
    {"decay", ArgumentKind::Integer, kSquareAttackDecayReleaseRange},
    {"sustain", ArgumentKind::Integer, kSquareSustainRange},
    {"release", ArgumentKind::Integer, kSquareAttackDecayReleaseRange},
};

constexpr MacroArgument kProgrammableWaveArgs[] = {
    {"base_midi_key", ArgumentKind::Integer, kMidiRange},
    {"pan", ArgumentKind::Integer, kPanRange},
    {"wave_samples_pointer", ArgumentKind::ProgrammableWaveSymbol, std::nullopt},
    {"attack", ArgumentKind::Integer, kSquareAttackDecayReleaseRange},
    {"decay", ArgumentKind::Integer, kSquareAttackDecayReleaseRange},
    {"sustain", ArgumentKind::Integer, kSquareSustainRange},
    {"release", ArgumentKind::Integer, kSquareAttackDecayReleaseRange},
};

constexpr MacroArgument kNoiseArgs[] = {
    {"base_midi_key", ArgumentKind::Integer, kMidiRange},
    {"pan", ArgumentKind::Integer, kPanRange},
    {"period", ArgumentKind::Integer, kNoisePeriodRange},
    {"attack", ArgumentKind::Integer, kSquareAttackDecayReleaseRange},
    {"decay", ArgumentKind::Integer, kSquareAttackDecayReleaseRange},
    {"sustain", ArgumentKind::Integer, kSquareSustainRange},
    {"release", ArgumentKind::Integer, kSquareAttackDecayReleaseRange},
};

constexpr MacroArgument kKeysplitAllArgs[] = {
    {"voice_group_pointer", ArgumentKind::VoicegroupSymbol, std::nullopt},
};

constexpr MacroArgument kKeysplitArgs[] = {
    {"voice_group_pointer", ArgumentKind::VoicegroupSymbol, std::nullopt},
    {"keysplit_table_pointer", ArgumentKind::KeysplitSymbol, std::nullopt},
};

constexpr MacroArgument kCryArgs[] = {
    {"sample", ArgumentKind::DirectSoundSymbol, std::nullopt},
};

constexpr std::array<MacroDefinition, 15> kMacros{{
    {"voice_directsound_no_resample",
     VoiceType::DirectSoundNoResample,
     MacroKind::DirectSoundNoResample,
     kDirectSoundArgs},
    {"voice_directsound_alt", VoiceType::DirectSoundAlt, MacroKind::DirectSoundAlt, kDirectSoundArgs},
    {"voice_directsound", VoiceType::DirectSound, MacroKind::DirectSound, kDirectSoundArgs},
    {"voice_square_1_alt", VoiceType::Square1Alt, MacroKind::Square1, kSquare1Args},
    {"voice_square_1", VoiceType::Square1, MacroKind::Square1, kSquare1Args},
    {"voice_square_2_alt", VoiceType::Square2Alt, MacroKind::Square2, kSquare2Args},
    {"voice_square_2", VoiceType::Square2, MacroKind::Square2, kSquare2Args},
    {"voice_programmable_wave_alt", VoiceType::ProgrammableWaveAlt, MacroKind::ProgrammableWave, kProgrammableWaveArgs},
    {"voice_programmable_wave", VoiceType::ProgrammableWave, MacroKind::ProgrammableWave, kProgrammableWaveArgs},
    {"voice_noise_alt", VoiceType::NoiseAlt, MacroKind::Noise, kNoiseArgs},
    {"voice_noise", VoiceType::Noise, MacroKind::Noise, kNoiseArgs},
    {"voice_keysplit_all", VoiceType::KeysplitAll, MacroKind::KeysplitAll, kKeysplitAllArgs},
    {"voice_keysplit", VoiceType::Keysplit, MacroKind::Keysplit, kKeysplitArgs},
    {"cry_reverse", VoiceType::CryReverse, MacroKind::Cry, kCryArgs},
    {"cry", VoiceType::Cry, MacroKind::Cry, kCryArgs},
}};

} // namespace

std::span<const MacroDefinition> allMacros()
{
    return kMacros;
}

const MacroDefinition* findMacro(std::string_view name)
{
    for (const MacroDefinition& macro : kMacros)
    {
        if (macro.name == name)
            return &macro;
    }
    return nullptr;
}

} // namespace voicegroup
