module;
#include <memory>
module Artifact.Audio.Effects.Init;

import Artifact.Audio.Effects.Base;
import Artifact.Audio.Effects.Manager;
import Artifact.Audio.Effects.Equalizer;
import Artifact.Audio.Effects.Reverb;
import Artifact.Audio.Effects.Compressor;
import Artifact.Audio.Effects.Delay;
import Artifact.Audio.Effects.Chorus;
import Artifact.Audio.Effects.Limiter;
import Artifact.Audio.Effects.Distortion;

namespace Artifact {

// エフェクトの自動登録
struct AudioEffectsInitializer {
    AudioEffectsInitializer() {
        auto& manager = ArtifactAudioEffectManager::instance();
        
        // ダイナミクス系
        manager.registerEffectFactory("compressor", &createCompressorEffect);
        manager.registerEffectFactory("limiter",    &createLimiterEffect);
        
        // 空間系
        manager.registerEffectFactory("reverb",  &createReverbEffect);
        manager.registerEffectFactory("delay",   &createDelayEffect);
        manager.registerEffectFactory("chorus",  &createChorusEffect);
        
        // EQ / Tone
        manager.registerEffectFactory("equalizer",  &createEqualizerEffect);
        manager.registerEffectFactory("distortion", &createDistortionEffect);
    }
};

// 静的な初期化子
static AudioEffectsInitializer effectsInitializer;

} // namespace Artifact