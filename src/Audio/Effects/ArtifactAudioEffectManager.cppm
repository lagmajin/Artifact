module;
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

module Artifact.Audio.Effects.Manager;

namespace Artifact {

ArtifactAudioEffectManager& ArtifactAudioEffectManager::instance()
{
    static ArtifactAudioEffectManager manager;
    return manager;
}

void ArtifactAudioEffectManager::registerEffectFactory(const std::string& effectId, AudioEffectFactory factory)
{
    if (effectId.empty() || factory == nullptr) {
        return;
    }
    effectFactories_[effectId] = factory;
}

std::unique_ptr<ArtifactAbstractAudioEffect> ArtifactAudioEffectManager::createEffect(const std::string& effectId)
{
    const auto it = effectFactories_.find(effectId);
    if (it == effectFactories_.end() || it->second == nullptr) {
        return {};
    }
    return it->second();
}

std::vector<std::string> ArtifactAudioEffectManager::getAvailableEffects() const
{
    std::vector<std::string> effects;
    effects.reserve(effectFactories_.size());
    for (const auto& [id, factory] : effectFactories_) {
        if (factory != nullptr) {
            effects.push_back(id);
        }
    }
    std::sort(effects.begin(), effects.end());
    return effects;
}

bool ArtifactAudioEffectManager::hasEffect(const std::string& effectId) const
{
    return effectFactories_.find(effectId) != effectFactories_.end();
}

} // namespace Artifact
