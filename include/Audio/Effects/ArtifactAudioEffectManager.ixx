module;
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
export module Artifact.Audio.Effects.Manager;

import std;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

class ArtifactAudioEffectManager {
public:
    // シングルトンインスタンス
    static ArtifactAudioEffectManager& instance();

    // エフェクトファクトリーの登録
    void registerEffectFactory(const std::string& effectId, AudioEffectFactory factory);
    
    // エフェクトインスタンスの作成
    std::unique_ptr<ArtifactAbstractAudioEffect> createEffect(const std::string& effectId);
    
    // 登録済みエフェクトの一覧取得
    std::vector<std::string> getAvailableEffects() const;
    
    // エフェクトの存在確認
    bool hasEffect(const std::string& effectId) const;

private:
    ArtifactAudioEffectManager() = default;
    ~ArtifactAudioEffectManager() = default;
    
    std::unordered_map<std::string, AudioEffectFactory> effectFactories_;
};

} // namespace Artifact