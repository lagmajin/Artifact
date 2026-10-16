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
import Artifact.Audio.Effects.NoiseGate;
import Artifact.Audio.Effects.HighLowPass;
import Artifact.Audio.Effects.PlateReverb;
import Artifact.Audio.Effects.BassTreble;
import Artifact.Audio.Effects.BitCrusher;
import Artifact.Audio.Effects.DCBlocker;
import Artifact.Audio.Effects.DeEsser;
import Artifact.Audio.Effects.Expander;
import Artifact.Audio.Effects.Phaser;
import Artifact.Audio.Effects.PingPongDelay;
import Artifact.Audio.Effects.SpringReverb;
import Artifact.Audio.Effects.StereoMixer;
import Artifact.Audio.Effects.StereoWidener;
import Artifact.Audio.Effects.TapeSaturation;
import Artifact.Audio.Effects.TransientShaper;
import Artifact.Audio.Effects.Tremolo;
import Artifact.Audio.Effects.Vibrato;

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
        manager.registerEffectFactory("noise_gate", &createNoiseGateEffect);
        manager.registerEffectFactory("high_low_pass", &createHighLowPassEffect);
        manager.registerEffectFactory("plate_reverb", &createPlateReverbEffect);
        manager.registerEffectFactory("bass_treble", &createBassTrebleEffect);
        manager.registerEffectFactory("bit_crusher", &createBitCrusherEffect);
        manager.registerEffectFactory("dc_blocker", &createDCBlockerEffect);
        manager.registerEffectFactory("de_esser", &createDeEsserEffect);
        manager.registerEffectFactory("expander", &createExpanderEffect);
        manager.registerEffectFactory("phaser", &createPhaserEffect);
        manager.registerEffectFactory("ping_pong_delay", &createPingPongDelayEffect);
        manager.registerEffectFactory("spring_reverb", &createSpringReverbEffect);
        manager.registerEffectFactory("stereo_mixer", &createStereoMixerEffect);
        manager.registerEffectFactory("stereo_widener", &createStereoWidenerEffect);
        manager.registerEffectFactory("tape_saturation", &createTapeSaturationEffect);
        manager.registerEffectFactory("transient_shaper", &createTransientShaperEffect);
        manager.registerEffectFactory("tremolo", &createTremoloEffect);
        manager.registerEffectFactory("vibrato", &createVibratoEffect);
    }
};

static AudioEffectsInitializer effectsInitializer;

} // namespace Artifact
