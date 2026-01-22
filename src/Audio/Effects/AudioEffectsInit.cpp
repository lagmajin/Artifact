module;
#include <memory>
module Artifact.Audio.Effects.Init;

import Artifact.Audio.Effects.Base;
import Artifact.Audio.Effects.Manager;
import Artifact.Audio.Effects.Equalizer;
import Artifact.Audio.Effects.Reverb;

namespace Artifact {

// エフェクトの自動登録
struct AudioEffectsInitializer {
    AudioEffectsInitializer() {
        auto& manager = ArtifactAudioEffectManager::instance();
        
        // イコライザーエフェクトの登録
        manager.registerEffectFactory("equalizer", &createEqualizerEffect);
        
        // リバーブエフェクトの登録
        manager.registerEffectFactory("reverb", &createReverbEffect);
        
        // ここに追加のエフェクトを登録
    }
};

// 静的な初期化子
static AudioEffectsInitializer effectsInitializer;

} // namespace Artifact