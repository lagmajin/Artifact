module;

#include <QRectF>
#include <QString>
#include <QVariant>
#include <QByteArray>
#include <QStringList>
#include <memory>
#include <vector>
#include <functional>
#include <cmath>

export module Artifact.Effect.HostContract;

import Artifact.Render.Context;
import Artifact.Render.ROI;
import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Image.ImageF32x4RGBAWithCache;
import Color.ColorSpace;

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
    ArtifactCore::ColorSpace colorSpace = ArtifactCore::ColorSpace::Linear;
    QString colorConfigIdentity;
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
    ArtifactCore::ColorSpace inputColorSpace = ArtifactCore::ColorSpace::Linear;
    ArtifactCore::ColorSpace workingColorSpace = ArtifactCore::ColorSpace::Linear;
    ArtifactCore::ColorSpace outputColorSpace = ArtifactCore::ColorSpace::Linear;
    QString colorConfigIdentity;
};

struct EffectDependencyDescriptor {
    std::vector<EffectInputRequest> inputRequests;
    bool dependsOnUpstreamEffect = false;
    bool dependsOnLayerTransform = false;
    bool dependsOnCompositionFrame = false;
    // Stable names consumed by cache invalidation and diagnostics.
    QStringList dependencyKeys;
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

    ArtifactCore::ColorSpace inputColorSpace() const { return inputColorSpace_; }
    void setInputColorSpace(ArtifactCore::ColorSpace space) { inputColorSpace_ = space; }
    ArtifactCore::ColorSpace workingColorSpace() const { return workingColorSpace_; }
    void setWorkingColorSpace(ArtifactCore::ColorSpace space) { workingColorSpace_ = space; }
    ArtifactCore::ColorSpace outputColorSpace() const { return outputColorSpace_; }
    void setOutputColorSpace(ArtifactCore::ColorSpace space) { outputColorSpace_ = space; }
    const QString& colorConfigIdentity() const { return colorConfigIdentity_; }
    void setColorConfigIdentity(const QString& identity) { colorConfigIdentity_ = identity; }

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
    ArtifactCore::ColorSpace inputColorSpace_ = ArtifactCore::ColorSpace::Linear;
    ArtifactCore::ColorSpace workingColorSpace_ = ArtifactCore::ColorSpace::Linear;
    ArtifactCore::ColorSpace outputColorSpace_ = ArtifactCore::ColorSpace::Linear;
    QString colorConfigIdentity_;
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
        desc.inputColorSpace = hostContext_.inputColorSpace();
        desc.workingColorSpace = hostContext_.workingColorSpace();
        desc.outputColorSpace = hostContext_.outputColorSpace();
        desc.colorConfigIdentity = hostContext_.colorConfigIdentity();
        return desc;
    }

    EffectDependencyDescriptor describeDependencies() const override
    {
        EffectDependencyDescriptor dep;
        dep.inputRequests.push_back(inputRequest_);
        dep.dependsOnUpstreamEffect = true;
        dep.dependencyKeys.push_back(inputRequest_.inputId);
        if (inputRequest_.requiresPreviousFrame || inputRequest_.requiresFrameCache) {
            dep.dependencyKeys.push_back(QStringLiteral("previous-frame"));
        }
        if (!inputRequest_.colorConfigIdentity.isEmpty()) {
            dep.dependencyKeys.push_back(
                QStringLiteral("color-config:%1").arg(inputRequest_.colorConfigIdentity));
        }
        dep.dependencyKeys.push_back(
            QStringLiteral("color-space:%1:%2:%3")
                .arg(static_cast<int>(inputRequest_.colorSpace))
                .arg(static_cast<int>(hostContext_.workingColorSpace()))
                .arg(static_cast<int>(hostContext_.outputColorSpace())));
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
        legacyCtx.inputColorSpace = context.inputColorSpace();
        legacyCtx.workingColorSpace = context.workingColorSpace();
        legacyCtx.outputColorSpace = context.outputColorSpace();
        legacyCtx.colorConfigIdentity = context.colorConfigIdentity();
        legacyCtx.evaluationCacheKey = buildEvaluationCacheKey(context, input);
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
    static QByteArray buildEvaluationCacheKey(const EffectHostContext& context,
                                              const EffectInputRequest& input)
    {
        QByteArray key = QByteArrayLiteral("effect-cache-v1|");
        key += input.inputId.toUtf8();
        key += '|';
        key += QByteArray::number(context.snapshot().currentFrame);
        key += '|';
        key += QByteArray::number(static_cast<int>(context.inputColorSpace()));
        key += '|';
        key += QByteArray::number(static_cast<int>(context.workingColorSpace()));
        key += '|';
        key += QByteArray::number(static_cast<int>(context.outputColorSpace()));
        key += '|';
        key += context.colorConfigIdentity().toUtf8();
        key += '|';
        key += QByteArray::number(static_cast<int>(input.colorSpace));
        key += '|';
        key += input.colorConfigIdentity.toUtf8();
        key += '|';
        key += QByteArray::number(static_cast<qint64>(std::llround(input.roi.x() * 1000000.0)));
        key += ',';
        key += QByteArray::number(static_cast<qint64>(std::llround(input.roi.y() * 1000000.0)));
        key += ',';
        key += QByteArray::number(static_cast<qint64>(std::llround(input.roi.width() * 1000000.0)));
        key += ',';
        key += QByteArray::number(static_cast<qint64>(std::llround(input.roi.height() * 1000000.0)));
        return key;
    }

    ArtifactAbstractEffect* effect_ = nullptr;
    const ImageF32x4RGBAWithCache* inputSource_ = nullptr;
    IEffectFrameSampler* frameSampler_ = nullptr;
    EffectHostContext hostContext_;
    EffectInputRequest inputRequest_;
    EffectOutputSurface outputSurface_;
};

} // namespace Artifact
