module;
#include <memory>

export module Artifact.Audio.Effects.Init;

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

struct AudioEffectsInitializer {
    AudioEffectsInitializer() {
        auto& manager = ArtifactAudioEffectManager::instance();
        manager.registerEffectFactory("compressor", &createCompressorEffect);
        manager.registerEffectFactory("limiter",    &createLimiterEffect);
        manager.registerEffectFactory("reverb",  &createReverbEffect);
        manager.registerEffectFactory("delay",   &createDelayEffect);
        manager.registerEffectFactory("chorus",  &createChorusEffect);
        manager.registerEffectFactory("equalizer",  &createEqualizerEffect);
        manager.registerEffectFactory("distortion", &createDistortionEffect);
    }
};

static AudioEffectsInitializer effectsInitializer;

} // namespace Artifact
