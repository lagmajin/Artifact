module;
#include <utility>
#include <memory>
#include <vector>
#include <QVariant>

export module Artifact.Effect.Creative;

import Artifact.Effect.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Graphics.Effect.Creative;
import Memory.SharedPtr;

export namespace Artifact {

class ArtifactGlitchEffect : public ArtifactAbstractEffect {
public:
    ArtifactGlitchEffect();
    void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
    bool supportsGPU() const override { return true; }
};

class ArtifactHalftoneEffect : public ArtifactAbstractEffect {
public:
    ArtifactHalftoneEffect();
    void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
    bool supportsGPU() const override { return true; }
};

class ArtifactOldTVEffect : public ArtifactAbstractEffect {
public:
    ArtifactOldTVEffect();
    void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
    bool supportsGPU() const override { return true; }
};

// Adapter for the Core creative-effect implementations.  Keeping this
// bridge in the Artifact layer makes the Core algorithms available to the
// normal effect service, inspector, presets, and render path.
class ArtifactCoreCreativeEffect : public ArtifactAbstractEffect {
public:
    ArtifactCoreCreativeEffect(const char* coreName,
                               const char* effectId,
                               const char* displayName,
                               EffectPipelineStage stage = EffectPipelineStage::Rasterizer);

    void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override;

protected:
    void onContextUpdated(const EffectContext& context) override;

private:
    ArtifactCore::SharedPtr<ArtifactCore::CreativeEffect> coreEffect_;
    ArtifactCore::CreativeEffectContext context_;
    bool hasHostContext_ = false;
};

class ArtifactFilmGrungeEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;

protected:
    void apply(const ImageF32x4RGBAWithCache& src,
               ImageF32x4RGBAWithCache& dst) override;
    void onContextUpdated(const EffectContext& context) override;

public:
    ArtifactFilmGrungeEffect();
    ~ArtifactFilmGrungeEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name,
                          const QVariant& value) override;
    EffectROIHint roiHint() const override;
};

class ArtifactHeatwaveEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;

protected:
    void apply(const ImageF32x4RGBAWithCache& src,
               ImageF32x4RGBAWithCache& dst) override;
    void onContextUpdated(const EffectContext& context) override;

public:
    ArtifactHeatwaveEffect();
    ~ArtifactHeatwaveEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name,
                          const QVariant& value) override;
    EffectROIHint roiHint() const override;
};

class ArtifactCinematicLensFlareEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;

protected:
    void apply(const ImageF32x4RGBAWithCache& src,
               ImageF32x4RGBAWithCache& dst) override;

public:
    ArtifactCinematicLensFlareEffect();
    ~ArtifactCinematicLensFlareEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name,
                          const QVariant& value) override;
    EffectROIHint roiHint() const override;
};

class ArtifactTexturizeMotionEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
    void onContextUpdated(const EffectContext&) override;
public:
    ArtifactTexturizeMotionEffect();
    ~ArtifactTexturizeMotionEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactDebandEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected: void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
public:
    ArtifactDebandEffect();
    ~ArtifactDebandEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactDeblockEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected: void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
public:
    ArtifactDeblockEffect();
    ~ArtifactDeblockEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactBeautyStudioEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected: void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
public:
    ArtifactBeautyStudioEffect();
    ~ArtifactBeautyStudioEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactEnergyZapEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
    void onContextUpdated(const EffectContext&) override;
public:
    ArtifactEnergyZapEffect();
    ~ArtifactEnergyZapEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactLightWrapProEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
    void onContextUpdated(const EffectContext&) override;
public:
    ArtifactLightWrapProEffect();
    ~ArtifactLightWrapProEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactMatchGrainEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
    void onContextUpdated(const EffectContext&) override;
public:
    ArtifactMatchGrainEffect();
    ~ArtifactMatchGrainEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactWireObjectRemoverEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
    void onContextUpdated(const EffectContext&) override;
public:
    ArtifactWireObjectRemoverEffect();
    ~ArtifactWireObjectRemoverEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactDepthRelightEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
    void onContextUpdated(const EffectContext&) override;
public:
    ArtifactDepthRelightEffect();
    ~ArtifactDepthRelightEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactMatteRefineEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
    void onContextUpdated(const EffectContext&) override;
public:
    ArtifactMatteRefineEffect();
    ~ArtifactMatteRefineEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactSpillKillerProEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
public:
    ArtifactSpillKillerProEffect();
    ~ArtifactSpillKillerProEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactPixelDustFixerEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
    void onContextUpdated(const EffectContext&) override;
public:
    ArtifactPixelDustFixerEffect();
    ~ArtifactPixelDustFixerEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactReflectionComposerEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
    void onContextUpdated(const EffectContext&) override;
public:
    ArtifactReflectionComposerEffect();
    ~ArtifactReflectionComposerEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactLensProfileMatcherEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
public:
    ArtifactLensProfileMatcherEffect();
    ~ArtifactLensProfileMatcherEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactAtmosphericDepthEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
    void onContextUpdated(const EffectContext&) override;
public:
    ArtifactAtmosphericDepthEffect();
    ~ArtifactAtmosphericDepthEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

class ArtifactEdgeColorCompositeEffect : public ArtifactAbstractEffect {
private: class Impl; Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache&, ImageF32x4RGBAWithCache&) override;
    void onContextUpdated(const EffectContext&) override;
public:
    ArtifactEdgeColorCompositeEffect();
    ~ArtifactEdgeColorCompositeEffect() override;
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString&, const QVariant&) override;
    EffectROIHint roiHint() const override;
};

} // namespace Artifact
