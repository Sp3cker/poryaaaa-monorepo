#ifndef VOICEGROUP_CORE_PROJECT_INDEX_HPP
#define VOICEGROUP_CORE_PROJECT_INDEX_HPP

#include "voicegroup_core/voicegroup_document.hpp"

#include <filesystem>
#include <string_view>
#include <vector>

namespace voicegroup
{

class ProjectIndex
{
public:
    static ProjectIndex load(const std::filesystem::path& projectRoot);

    LoadedBank loadBank(std::string_view bankName) const;

    bool hasDirectSoundSymbol(std::string_view symbol) const;
    bool hasProgrammableWaveSymbol(std::string_view symbol) const;
    bool hasKeysplitSymbol(std::string_view symbol) const;
    bool hasVoicegroup(std::string_view symbol) const;

    std::vector<ResolvedAsset> directSoundAssets() const;
    std::vector<ResolvedAsset> programmableWaveAssets() const;

private:
    std::filesystem::path projectRoot_;
    std::vector<ResolvedAsset> directSoundAssets_;
    std::vector<ResolvedAsset> programmableWaveAssets_;
};

} // namespace voicegroup

#endif // VOICEGROUP_CORE_PROJECT_INDEX_HPP
