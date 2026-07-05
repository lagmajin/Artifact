module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.TemporalDenoise;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class TemporalDenoiseCPUImpl : public ArtifactEffectImplBase {
public:
    float strength_ = 0.5f;
    int   frameCount_ = 3;
    float varianceThreshold_ = 0.05f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& si = src.image();
        const float* sd = si.rgba32fData();
        if (!sd || si.width() <= 0) { dst = src; return; }
        const float s = std::clamp(strength_, 0.0f, 1.0f);
        const int fc = std::clamp(frameCount_, 1, 8);
        const float vt = std::clamp(varianceThreshold_, 0.0f, 1.0f);
        if (s <= 0.0f || !context_.sampler) { dst = src.DeepCopy(); return; }

        const int W = si.width(), H = si.height();
        const size_t n = (size_t)W * H;

        // Collect reference frames.
        struct RefFrame { ImageF32x4_RGBA img; };
        std::vector<RefFrame> refs;
        for (int i = 1; i <= fc; ++i) {
            ImageF32x4RGBAWithCache f;
            if (context_.sampler->sampleCurrentLayerFrameRelative(
                    static_cast<std::int64_t>(-i), f)
                && f.width() == W && f.height() == H
                && f.image().rgba32fData()) {
                refs.push_back({ f.image() });
            }
        }
        if (refs.empty()) { dst = src.DeepCopy(); return; }

        dst = src.DeepCopy();
        float* d = dst.image().rgba32fData();

        for (size_t px = 0; px < n; ++px) {
            // Compute per-channel variance across reference frames.
            float sumR = 0, sumG = 0, sumB = 0, sumA = 0;
            float sumR2 = 0, sumG2 = 0, sumB2 = 0;
            int rfCount = (int)refs.size();

            for (const auto& rf : refs) {
                const float* rp = rf.img.rgba32fData() + px * 4;
                sumR += rp[0]; sumG += rp[1]; sumB += rp[2]; sumA += rp[3];
                sumR2 += rp[0] * rp[0];
                sumG2 += rp[1] * rp[1];
                sumB2 += rp[2] * rp[2];
            }
            float invN = 1.0f / (float)rfCount;
            float meanR = sumR * invN, meanG = sumG * invN, meanB = sumB * invN;
            float varR = sumR2 * invN - meanR * meanR;
            float varG = sumG2 * invN - meanG * meanG;
            float varB = sumB2 * invN - meanB * meanB;
            float maxVar = std::max({ varR, varG, varB });

            // Variance-based blend factor: low variance → strong blend.
            float vf = 1.0f - std::min(maxVar / std::max(vt * vt, 1e-8f), 1.0f);
            float w = s * vf;
            float iw = 1.0f - w;

            float* p = d + px * 4;
            p[0] = p[0] * iw + meanR * w;
            p[1] = p[1] * iw + meanG * w;
            p[2] = p[2] * iw + meanB * w;
            p[3] = p[3] * iw + sumA * invN * w;
        }
    }
};

TemporalDenoiseEffect::TemporalDenoiseEffect() : ArtifactAbstractEffect() {
    setPipelineStage(EffectPipelineStage::Rasterizer);
    syncImpls();
}
TemporalDenoiseEffect::~TemporalDenoiseEffect() = default;
float TemporalDenoiseEffect::strength() const { return strength_; }
void TemporalDenoiseEffect::setStrength(float v) { strength_=std::clamp(v,0.0f,1.0f); syncImpls(); }
int TemporalDenoiseEffect::frameCount() const { return frameCount_; }
void TemporalDenoiseEffect::setFrameCount(int v) { frameCount_=std::clamp(v,1,8); syncImpls(); }
float TemporalDenoiseEffect::varianceThreshold() const { return varianceThreshold_; }
void TemporalDenoiseEffect::setVarianceThreshold(float v) { varianceThreshold_=std::clamp(v,0.0f,1.0f); syncImpls(); }

std::vector<AbstractProperty> TemporalDenoiseEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(3);

    auto addInt = [&props](const char* name, int value, int minValue, int maxValue) {
        AbstractProperty prop;
        prop.setName(QString::fromLatin1(name));
        prop.setType(PropertyType::Integer);
        const QVariant variantValue(value);
        prop.setValue(variantValue);
        prop.setDefaultValue(variantValue);
        prop.setMinValue(QVariant(minValue));
        prop.setMaxValue(QVariant(maxValue));
        props.push_back(std::move(prop));
    };

    auto addFloat = [&props](const char* name, float value, float minValue, float maxValue) {
        AbstractProperty prop;
        prop.setName(QString::fromLatin1(name));
        prop.setType(PropertyType::Float);
        const QVariant variantValue(static_cast<double>(value));
        prop.setValue(variantValue);
        prop.setDefaultValue(variantValue);
        prop.setMinValue(QVariant(static_cast<double>(minValue)));
        prop.setMaxValue(QVariant(static_cast<double>(maxValue)));
        props.push_back(std::move(prop));
    };

    addFloat("strength", strength_, 0.0f, 1.0f);
    addInt("frameCount", frameCount_, 1, 8);
    addFloat("varianceThreshold", varianceThreshold_, 0.0f, 1.0f);
    return props;
}
void TemporalDenoiseEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "strength") setStrength(v.toFloat());
    else if (k == "frameCount") setFrameCount(v.toInt());
    else if (k == "varianceThreshold") setVarianceThreshold(v.toFloat());
}
void TemporalDenoiseEffect::syncImpls() {
    auto c = std::make_shared<TemporalDenoiseCPUImpl>();
    c->strength_ = strength_; c->frameCount_ = frameCount_; c->varianceThreshold_ = varianceThreshold_;
    setCPUImpl(c);
}

} // namespace Artifact
