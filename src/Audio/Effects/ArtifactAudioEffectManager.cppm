module;
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

module Artifact.Audio.Effects.Manager;

import Memory.SharedPtr;

namespace Artifact {

ArtifactAudioEffectManager& ArtifactAudioEffectManager::instance()
{
    static ArtifactAudioEffectManager manager;
    return manager;
}

void ArtifactAudioEffectManager::registerEffectFactory(const String& effectId, AudioEffectFactory factory)
{
    if (effectId.isEmpty() || factory == nullptr) {
        return;
    }
    effectFactories_[ArtifactCore::toStdString(effectId)] = factory;
}

ArtifactCore::SharedPtr<ArtifactAbstractAudioEffect> ArtifactAudioEffectManager::createEffect(const String& effectId)
{
    const auto it = effectFactories_.find(ArtifactCore::toStdString(effectId));
    if (it == effectFactories_.end() || it->second == nullptr) {
        return {};
    }
    auto effect = it->second();
    if (!effect) {
        return {};
    }
    return ArtifactCore::makeShared(
        effect.release(), std::default_delete<ArtifactAbstractAudioEffect>{});
}

std::vector<String> ArtifactAudioEffectManager::getAvailableEffects() const
{
    std::vector<String> effects;
    effects.reserve(effectFactories_.size());
    for (const auto& [id, factory] : effectFactories_) {
        if (factory != nullptr) {
            effects.emplace_back(id);
        }
    }
    std::sort(effects.begin(), effects.end());
    return effects;
}

bool ArtifactAudioEffectManager::hasEffect(const String& effectId) const
{
    return effectFactories_.find(ArtifactCore::toStdString(effectId)) != effectFactories_.end();
}

} // namespace Artifact
