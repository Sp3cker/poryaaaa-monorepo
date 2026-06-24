#include "voicegroup_core/macro_catalog.hpp"
#include "voicegroup_core/project_index.hpp"
#include "voicegroup_core/voicegroup_document.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace voicegroup;

static void test_catalog_contains_poryaaaa_voice_macros()
{
    struct ExpectedMacro
    {
        std::string_view name;
        VoiceType typeCode;
        MacroKind kind;
        std::size_t argumentCount;
    };

    constexpr ExpectedMacro expected[] = {
        {"voice_directsound_no_resample", VoiceType::DirectSoundNoResample, MacroKind::DirectSoundNoResample, 7},
        {"voice_directsound_alt", VoiceType::DirectSoundAlt, MacroKind::DirectSoundAlt, 7},
        {"voice_directsound", VoiceType::DirectSound, MacroKind::DirectSound, 7},
        {"voice_square_1_alt", VoiceType::Square1Alt, MacroKind::Square1, 8},
        {"voice_square_1", VoiceType::Square1, MacroKind::Square1, 8},
        {"voice_square_2_alt", VoiceType::Square2Alt, MacroKind::Square2, 7},
        {"voice_square_2", VoiceType::Square2, MacroKind::Square2, 7},
        {"voice_programmable_wave_alt", VoiceType::ProgrammableWaveAlt, MacroKind::ProgrammableWave, 7},
        {"voice_programmable_wave", VoiceType::ProgrammableWave, MacroKind::ProgrammableWave, 7},
        {"voice_noise_alt", VoiceType::NoiseAlt, MacroKind::Noise, 7},
        {"voice_noise", VoiceType::Noise, MacroKind::Noise, 7},
        {"voice_keysplit_all", VoiceType::KeysplitAll, MacroKind::KeysplitAll, 1},
        {"voice_keysplit", VoiceType::Keysplit, MacroKind::Keysplit, 2},
        {"cry_reverse", VoiceType::CryReverse, MacroKind::Cry, 1},
        {"cry", VoiceType::Cry, MacroKind::Cry, 1},
    };

    for (const ExpectedMacro& macro : expected)
    {
        const MacroDefinition* definition = findMacro(macro.name);
        assert(definition != nullptr);
        assert(definition->name == macro.name);
        assert(definition->typeCode == macro.typeCode);
        assert(definition->kind == macro.kind);
        assert(definition->arguments.size() == macro.argumentCount);
    }

    assert(findMacro("voice_noise_altitude") == nullptr);
    assert(allMacros().size() == std::size(expected));
}

static void test_parse_document_tracks_sections_slots_arguments_and_comments()
{
    constexpr std::string_view text = R"(voicegroup_main::
    voice_group voicegroup_main, 12
    voice_directsound 60, 0, DirectSoundWaveData_Brass1, 1, 2, 3, 4 @ Brass
    voice_square_1 64, 0, 8, 2, 1, 2, 8, 3
)";

    ParsedDocument document = parseDocument(text);
    assert(document.diagnostics.empty());
    assert(document.sections.size() == 1);

    const ParsedSection& section = document.sections[0];
    assert(section.label == "voicegroup_main");
    assert(section.programs.size() == 2);

    const ParsedProgram& directSound = section.programs[0];
    assert(directSound.slot == 12);
    assert(directSound.macro != nullptr);
    assert(directSound.macro->name == "voice_directsound");
    assert(directSound.rawArguments.size() == 7);
    assert(directSound.rawArguments[2] == "DirectSoundWaveData_Brass1");
    assert(directSound.trailingComment == "Brass");

    const ParsedProgram& square = section.programs[1];
    assert(square.slot == 13);
    assert(square.macro != nullptr);
    assert(square.macro->name == "voice_square_1");
    assert(square.rawArguments.size() == 8);
    assert(!square.trailingComment.has_value());
}

static bool hasDiagnostic(const std::vector<Diagnostic>& diagnostics, std::string_view code)
{
    for (const Diagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.code == code)
            return true;
    }
    return false;
}

static void test_analyze_validates_arguments_symbols_and_duplicate_slots()
{
    constexpr std::string_view text = R"(voicegroup_main::
    voice_directsound nope, 0, DirectSoundWaveData_Missing, 1, 2, 3, 4
    voice_square_2 60
    voice_group voicegroup_main, 4
    voice_noise 60, 0, 0, 1, 2, 8, 3
    voice_group voicegroup_main, 4
    voice_noise 61, 0, 0, 1, 2, 8, 3
)";

    ProjectIndex index;
    const std::vector<Diagnostic> diagnostics = analyze(parseDocument(text), index);
    assert(hasDiagnostic(diagnostics, "invalid-integer"));
    assert(hasDiagnostic(diagnostics, "unknown-directsound-symbol"));
    assert(hasDiagnostic(diagnostics, "wrong-argument-count"));
    assert(hasDiagnostic(diagnostics, "duplicate-slot"));
}

static const ResolvedAsset* findAsset(const std::vector<ResolvedAsset>& assets, std::string_view symbol)
{
    for (const ResolvedAsset& asset : assets)
    {
        if (asset.symbol == symbol)
            return &asset;
    }
    return nullptr;
}

static void writeTextFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    file << text;
}

static void test_project_index_loads_standard_asset_sources()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "voicegroup_core_asset_index_test";
    std::filesystem::remove_all(root);

    writeTextFile(root / "sound/direct_sound_data.inc",
                  R"(DirectSoundWaveData_Brass1::
    .incbin "sound/direct_sound_samples/brass_1.bin"
)");
    writeTextFile(root / "sound/programmable_wave_data.inc",
                  R"(ProgrammableWaveData_Pulse1::
    .incbin "sound/wave_samples/pulse_1.bin"
)");

    const ProjectIndex index = ProjectIndex::load(root);

    const std::vector<ResolvedAsset> directSoundAssets = index.directSoundAssets();
    const ResolvedAsset* directSound = findAsset(directSoundAssets, "DirectSoundWaveData_Brass1");
    assert(directSound != nullptr);
    assert(directSound->relativePath == "sound/direct_sound_samples/brass_1.bin");
    assert(directSound->displayName == "brass_1.bin");
    assert(index.hasDirectSoundSymbol("DirectSoundWaveData_Brass1"));

    const std::vector<ResolvedAsset> programmableWaveAssets = index.programmableWaveAssets();
    const ResolvedAsset* progWave = findAsset(programmableWaveAssets, "ProgrammableWaveData_Pulse1");
    assert(progWave != nullptr);
    assert(progWave->relativePath == "sound/wave_samples/pulse_1.bin");
    assert(progWave->displayName == "pulse_1.bin");
    assert(index.hasProgrammableWaveSymbol("ProgrammableWaveData_Pulse1"));

    std::filesystem::remove_all(root);
}

static void test_load_bank_returns_ordered_program_with_resolved_asset()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "voicegroup_core_load_bank_test";
    std::filesystem::remove_all(root);

    writeTextFile(root / "sound/direct_sound_data.inc",
                  R"(DirectSoundWaveData_Brass1::
    .incbin "sound/direct_sound_samples/brass_1.bin"
)");
    writeTextFile(root / "sound/voicegroups/main.inc",
                  R"(voicegroup_main::
    voice_group voicegroup_main, 4
    voice_directsound 60, 0, DirectSoundWaveData_Brass1, 1, 2, 3, 4 @ Brass
)");

    const ProjectIndex index = ProjectIndex::load(root);
    const LoadedBank bank = index.loadBank("main");

    assert(bank.name == "main");
    assert(bank.sourceRelativePath == "sound/voicegroups/main.inc");
    assert(bank.diagnostics.empty());
    assert(bank.programs[4].has_value());

    const ResolvedProgram& program = *bank.programs[4];
    assert(program.parsed.macro != nullptr);
    assert(program.parsed.macro->name == "voice_directsound");
    assert(std::holds_alternative<DirectSoundProgram>(program.data));
    const DirectSoundProgram& directSound = std::get<DirectSoundProgram>(program.data);
    assert(directSound.asset.symbol == "DirectSoundWaveData_Brass1");
    assert(directSound.asset.relativePath == "sound/direct_sound_samples/brass_1.bin");
    assert(directSound.asset.displayName == "brass_1.bin");
    assert(directSound.baseMidiKey == 60);
    assert(directSound.pan == 0);
    assert(directSound.envelope.attack == 1);
    assert(directSound.envelope.decay == 2);
    assert(directSound.envelope.sustain == 3);
    assert(directSound.envelope.release == 4);
    assert(program.parsed.trailingComment == "Brass");

    std::filesystem::remove_all(root);
}

int main()
{
    test_catalog_contains_poryaaaa_voice_macros();
    test_parse_document_tracks_sections_slots_arguments_and_comments();
    test_analyze_validates_arguments_symbols_and_duplicate_slots();
    test_project_index_loads_standard_asset_sources();
    test_load_bank_returns_ordered_program_with_resolved_asset();
    return 0;
}
