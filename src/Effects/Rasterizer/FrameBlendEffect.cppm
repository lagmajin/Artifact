module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.FrameBlend;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;

namespace Artifact {
using namespace ArtifactCore;

class FrameBlendCPUImpl : public ArtifactEffectImplBase {
public:
    float blend_ = 0.5f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& si = src.image();
        const float* sd = si.rgba32fData();
        if (!sd || si.width() <= 0) { dst = src; return; }
        const float b = std::clamp(blend_, 0.0f, 1.0f);
        if (b <= 0.0f || !context_.sampler) { dst = src.DeepCopy(); return; }

        ImageF32x4RGBAWithCache prev;
        if (!context_.sampler->sampleCurrentLayerFrameRelative(-1, prev)
            || prev.width() <= 0 || !prev.image().rgba32fData()) {
            dst = src.DeepCopy(); return;
        }

        dst = src.DeepCopy();
        float* d = dst.image().rgba32fData();
        const float* pd = prev.image().rgba32fData();
        const int W = si.width(), H = si.height();
        const int pw = prev.width(), ph = prev.height();
        const float a = 1.0f - b;

        ArtifactCore::Parallel::For(0, H, [&](int y) {
            float* o = d + (size_t)y * W * 4;
            const float* pp = pd + (size_t)std::min(y, ph - 1) * pw * 4;
            for (int x = 0; x < W; ++x) {
                int px = std::min(x, pw - 1);
                float* p = o + (size_t)x * 4;
                const float* ep = pp + (size_t)px * 4;
                p[0] = p[0] * a + ep[0] * b;
                p[1] = p[1] * a + ep[1] * b;
                p[2] = p[2] * a + ep[2] * b;
                p[3] = p[3] * a + ep[3] * b;
            }
        });
    }
};

FrameBlendEffect::FrameBlendEffect() : ArtifactAbstractEffect() {
    setPipelineStage(EffectPipelineStage::Rasterizer);
    syncImpls();
}
FrameBlendEffect::~FrameBlendEffect() = default;
float FrameBlendEffect::blend() const { return blend_; }
void FrameBlendEffect::setBlend(float v) { blend_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

std::vector<AbstractProperty> FrameBlendEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(1);

    AbstractProperty prop;
    prop.setName(QString::fromLatin1("blend"));
    prop.setType(PropertyType::Float);
    const QVariant variantValue(static_cast<double>(blend_));
    prop.setValue(variantValue);
    prop.setDefaultValue(variantValue);
    prop.setMinValue(QVariant(0.0));
    prop.setMaxValue(QVariant(1.0));
    props.push_back(std::move(prop));

    return props;
}
void FrameBlendEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    if (n.toQString() == "blend") setBlend(v.toFloat());
}
void FrameBlendEffect::syncImpls() {
    auto c = std::make_shared<FrameBlendCPUImpl>();
    c->blend_ = blend_;
    setCPUImpl(c);
}

} // namespace Artifact
