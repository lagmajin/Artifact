module;
#include <memory>
#include <vector>
module Artifact.Audio.Effects.Manager;

import Artifact.Audio.Effects.Base;

namespace Artifact {

ArtifactAudioEffectManager& ArtifactAudioEffectManager::instance() {
    static ArtifactAudioEffectManager instance;
    return instance;
}

void ArtifactAudioEffectManager::registerEffectFactory(
    const std::string& effectId, AudioEffectFactory factory) {
    effectFactories_[effectId] = factory;
}

std::unique_ptr<ArtifactAbstractAudioEffect> ArtifactAudioEffectManager::createEffect(
    const std::string& effectId) {
    auto it = effectFactories_.find(effectId);
    if (it != effectFactories_.end()) {
        return it->second();
    }
    return nullptr;
}

std::vector<std::string> ArtifactAudioEffectManager::getAvailableEffects() const {
    std::vector<std::string> effects;
    for (const auto& pair : effectFactories_) {
        effects.push_back(pair.first);
    }
    return effects;
}

bool ArtifactAudioEffectManager::hasEffect(const std::string& effectId) const {
    return effectFactories_.find(effectId) != effectFactories_.end();
}

} // namespace Artifact