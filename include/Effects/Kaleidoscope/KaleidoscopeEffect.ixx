module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Kaleidoscope;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class KaleidoscopeEffect : public ArtifactAbstractEffect {
private:
    int segments_ = 6;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float rotation_ = 0.0f;
    float zoom_ = 1.0f;
    float feather_ = 0.0f;
    bool mirror_ = true;
    void syncImpls();

public:
    KaleidoscopeEffect();
    ~KaleidoscopeEffect() override;

    int segments() const;
    void setSegments(int v);
    float centerX() const;
    void setCenterX(float v);
    float centerY() const;
    void setCenterY(float v);
    float rotation() const;
    void setRotation(float v);
    float zoom() const;
    void setZoom(float v);
    float feather() const;
    void setFeather(float v);
    bool mirror() const;
    void setMirror(bool v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Kaleidoscope;
        node.parameters[0] = static_cast<float>(segments_);
        node.parameters[1] = centerX_;
        node.parameters[2] = centerY_;
        node.parameters[3] = rotation_;
        node.parameters[4] = zoom_;
        node.parameters[5] = feather_;
        node.parameters[6] = mirror_ ? 1.0f : 0.0f;
        node.resolutionScaledParameterMask = 0;
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

}

