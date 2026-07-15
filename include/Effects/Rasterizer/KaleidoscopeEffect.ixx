module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Kaleidoscope;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Kaleidoscope effect: mirrors and rotates the image around
/// a center point with configurable segment count.
// Kept as an isolated experimental rasterizer implementation. The production
// Effect Service owns Artifact.Effect.Kaleidoscope::KaleidoscopeEffect.
class RasterizerKaleidoscopeEffect : public ArtifactAbstractEffect {
public:
    RasterizerKaleidoscopeEffect();
    ~RasterizerKaleidoscopeEffect() override;

    int   segments() const;
    void  setSegments(int v);
    float rotation() const;
    void  setRotation(float v);
    float centerX() const;
    void  setCenterX(float v);
    float centerY() const;
    void  setCenterY(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    int segments_=6; float rotation_=0.0f,cx_=0.5f,cy_=0.5f;
    void syncImpls();
};

} // namespace Artifact
