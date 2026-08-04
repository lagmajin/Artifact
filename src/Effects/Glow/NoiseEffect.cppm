module;
#include <utility>
#include <QString>

module NoiseEffect;

namespace Artifact
{

class NoiseEffect::Impl {};

NoiseEffect::NoiseEffect() : impl_(new Impl()) {
  setEffectID(ArtifactCore::UniString(QStringLiteral("noise")));
  setDisplayName(ArtifactCore::UniString(QStringLiteral("Noise")));
  setPipelineStage(EffectPipelineStage::Rasterizer);
}

NoiseEffect::~NoiseEffect() { delete impl_; }


};
