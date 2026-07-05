module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.PixelSort;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Sorts pixels along their estimated motion direction.
/// Creates painterly/glitch-art directional streaks.
class PixelSortEffect : public ArtifactAbstractEffect {
public:
    PixelSortEffect();
    ~PixelSortEffect() override;

    /// Length of the sort window along velocity (pixels).
    int   sortLength() const;
    void  setSortLength(int v);

    /// 0=sort by luminance, 1=sort by hue.
    float sortKey() const;
    void  setSortKey(float v);

    /// 0=ascending (dark→bright), 1=descending (bright→dark).
    float sortOrder() const;
    void  setSortOrder(float v);

    /// Blend sorted with original (0=sorted only, 1=original only).
    float blend() const;
    void  setBlend(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    int sortLen_=16; float sortKey_=0.0f,sortOrder_=1.0f,blend_=0.5f;
    void syncImpls();
};

} // namespace Artifact
