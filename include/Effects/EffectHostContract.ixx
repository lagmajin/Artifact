module;

#include <QRectF>
#include <QString>
#include <QVariant>
#include <memory>
#include <vector>
#include <functional>

export module Artifact.Effect.HostContract;

import Artifact.Render.Context;
import Artifact.Render.ROI;
import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;

export namespace Artifact {

using namespace ArtifactCore;

enum class EffectChannelRequirement {
    RGBA,
    AlphaOnly,
    RGBOnly,
    Depth,
    Custom
};

enum class EffectSourceKind {
    UpstreamColor,
    OriginalLayerSource,
    PreviousStageColor,
    AuxiliaryMap,
    TemporalSample,
    Custom
};

enum class EffectTemporalSampleMode {
    None,
    PreviousFrame,
    RelativeFrameOffset,
    AbsoluteFrame,
    PerPixelDisplacedFrame
};

struct EffectInputRequest {
    QString inputId = QStringLiteral("primary");
    QRectF roi = {};
    EffectChannelRequirement channels = EffectChannelRequirement::RGBA;
    EffectSourceKind sourceKind = EffectSourceKind::UpstreamColor;
    bool requiresOriginalSource = false;
    bool requiresPreviousFrame = false;
    int temporalLookback = 0;
    EffectTemporalSampleMode temporalSampleMode =
        EffectTemporalSampleMode::None;
    std::int64_t relativeFrameOffset = 0;
    std::int64_t absoluteFrame = 0;
    bool allowPerPixelTemporalSampling = false;
    bool requiresFrameCache = false;
};

struct EffectInputSurface {
    QString inputId = QStringLiteral("primary");
    ImageF32x4RGBAWithCache* image = nullptr;
    QRectF roi = {};
    std::int64_t compositionFrame = 0;
};

struct EffectInputBundle {
    std::vector<EffectInputSurface> surfaces;

    EffectInputSurface* findSurface(const QString& inputId)
    {
        for (auto& surface : surfaces) {
            if (surface.inputId == inputId) {
                return &surface;
            }
        }
        return nullptr;
    }

    const EffectInputSurface* findSurface(const QString& inputId) const
    {
        for (const auto& surface : surfaces) {
            if (surface.inputId == inputId) {
                return &surface;
            }
        }
        return nullptr;
    }
};

struct EffectOutputSurface {
    ImageF32x4RGBAWithCache* image = nullptr;
    QRectF writtenROI;
    bool fullyWritten = false;
};

struct EffectCapabilityDescriptor {
    bool supportsGPU = false;
    bool supportsPartialEvaluation = false;
    bool supportsTemporalProcessing = false;
    bool supportsPerPixelTemporalSampling = false;
    bool requiresDeterministicFrameCache = false;
    bool isGenerator = false;
    EffectPipelineStage pipelineStage = EffectPipelineStage::MaterialRender;
    float roiExpansionHint = 0.0f;
};

struct EffectDependencyDescriptor {
    std::vector<EffectInputRequest> inputRequests;
    bool dependsOnUpstreamEffect = false;
    bool dependsOnLayerTransform = false;
    bool dependsOnCompositionFrame = false;
};

class EffectHostContext {
public:
    EffectHostContext() = default;

    EffectHostContext(RenderPurpose purpose, const RenderContextSnapshot& snapshot)
        : purpose_(purpose), snapshot_(snapshot) {}

    RenderPurpose purpose() const { return purpose_; }
    void setPurpose(RenderPurpose p) { purpose_ = p; }

    const RenderContextSnapshot& snapshot() const { return snapshot_; }
    void setSnapshot(const RenderContextSnapshot& s) { snapshot_ = s; }

    const QRectF& roi() const { return roi_; }
    void setROI(const QRectF& roi) { roi_ = roi; }

    bool isInteractive() const
    {
        return isInteractiveRenderPurpose(purpose_);
    }

    double timeSeconds() const { return snapshot_.time; }
    float resolutionScale() const { return snapshot_.resolutionScale; }

private:
    RenderPurpose purpose_ = RenderPurpose::EditorInteractive;
    RenderContextSnapshot snapshot_;
    QRectF roi_;
};

class IEffectHostAdapter {
public:
    virtual ~IEffectHostAdapter() = default;

    virtual EffectHostContext hostContext() const = 0;
    virtual EffectInputRequest inputRequest() const = 0;
    virtual EffectOutputSurface outputSurface() = 0;
    virtual EffectCapabilityDescriptor capabilities() const = 0;
    virtual EffectDependencyDescriptor describeDependencies() const = 0;

    virtual void render(const EffectHostContext& context,
                        const EffectInputRequest& input,
                        EffectOutputSurface& output) = 0;

    virtual void renderBundle(const EffectHostContext& context,
                              const EffectInputBundle& inputs,
                              EffectOutputSurface& output)
    {
        const auto* primary = inputs.findSurface(QStringLiteral("primary"));
        if (!primary) {
            return;
        }
        EffectInputRequest request;
        request.inputId = primary->inputId;
        request.roi = primary->roi;
        render(context, request, output);
    }
};

class LegacyEffectAdapter : public IEffectHostAdapter {
public:
    explicit LegacyEffectAdapter(ArtifactAbstractEffect* effect)
        : effect_(effect) {}

    EffectHostContext hostContext() const override { return hostContext_; }
    EffectInputRequest inputRequest() const override { return inputRequest_; }
    EffectOutputSurface outputSurface() override { return outputSurface_; }

    EffectCapabilityDescriptor capabilities() const override
    {
        EffectCapabilityDescriptor desc;
        if (effect_) {
            desc.supportsGPU = effect_->supportsGPU();
            desc.pipelineStage = effect_->pipelineStage();
        }
        return desc;
    }

    EffectDependencyDescriptor describeDependencies() const override
    {
        EffectDependencyDescriptor dep;
        dep.inputRequests.push_back(inputRequest_);
        dep.dependsOnUpstreamEffect = true;
        return dep;
    }

    void render(const EffectHostContext& context,
                const EffectInputRequest& input,
                EffectOutputSurface& output) override
    {
        if (!effect_ || !output.image) {
            return;
        }

        EffectContext legacyCtx;
        legacyCtx.roi = input.roi;
        legacyCtx.isInteractive = context.isInteractive();
        legacyCtx.compositionFrame = context.snapshot().currentFrame;
        legacyCtx.layerFrame = context.snapshot().currentFrame;
        legacyCtx.frameRate = context.snapshot().frameRate;
        legacyCtx.timeSeconds = context.timeSeconds();
        legacyCtx.resolutionScale = context.resolutionScale();
        legacyCtx.sampler = frameSampler_;
        effect_->setContext(legacyCtx);
        effect_->applyConfigured(*inputSource_, *output.image);

        output.writtenROI = input.roi;
        output.fullyWritten = true;
    }

    void renderBundle(const EffectHostContext& context,
                      const EffectInputBundle& inputs,
                      EffectOutputSurface& output) override
    {
        const auto* primary = inputs.findSurface(QStringLiteral("primary"));
        if (!primary || !primary->image) {
            return;
        }

        setInputSource(primary->image);

        EffectInputRequest request = inputRequest_;
        request.inputId = primary->inputId;
        request.roi = primary->roi;
        render(context, request, output);
    }

    void setInputSource(const ImageF32x4RGBAWithCache* src) { inputSource_ = src; }
    void setHostContext(const EffectHostContext& ctx) { hostContext_ = ctx; }
    void setInputRequest(const EffectInputRequest& req) { inputRequest_ = req; }
    void setOutputSurface(const EffectOutputSurface& surf) { outputSurface_ = surf; }
    void setFrameSampler(IEffectFrameSampler* sampler) { frameSampler_ = sampler; }

private:
    ArtifactAbstractEffect* effect_ = nullptr;
    const ImageF32x4RGBAWithCache* inputSource_ = nullptr;
    IEffectFrameSampler* frameSampler_ = nullptr;
    EffectHostContext hostContext_;
    EffectInputRequest inputRequest_;
    EffectOutputSurface outputSurface_;
};

} // namespace Artifact
