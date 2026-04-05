module;
#include <memory>
#include <vector>

export module Artifact.Effect.Creative;

import Artifact.Effect.Abstract;
import Image.ImageF32x4RGBAWithCache;

export namespace Artifact {

class ArtifactGlitchEffect : public ArtifactAbstractEffect {
public:
    ArtifactGlitchEffect();
    void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class ArtifactHalftoneEffect : public ArtifactAbstractEffect {
public:
    ArtifactHalftoneEffect();
    void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class ArtifactOldTVEffect : public ArtifactAbstractEffect {
public:
    ArtifactOldTVEffect();
    void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

} // namespace Artifact
