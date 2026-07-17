module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Feedback;

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

class FeedbackCPUImpl : public ArtifactEffectImplBase {
public:
    float amount_ = 0.5f, decay_ = 0.9f;
    float cx_ = 0.0f, cy_ = 0.0f;
    float zoom_ = 1.0f, rotation_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& si = src.image();
        const float* sd = si.rgba32fData();
        if (!sd || si.width() <= 0) { dst = src; return; }
        const float amt = std::clamp(amount_, 0.0f, 1.0f);
        if (amt <= 0.0f || !context_.sampler) { dst = src.DeepCopy(); return; }

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
        const float dec = std::clamp(decay_, 0.0f, 1.0f);
        const float z = std::max(zoom_, 0.01f);
        const float rad = rotation_ * std::numbers::pi_v<float> / 180.0f;
        const float cs = std::cos(rad), sn = std::sin(rad);
        const float hw = W * 0.5f, hh = H * 0.5f;

        ArtifactCore::Parallel::For(0, H, [&](int y) {
            float* o = d + (size_t)y * W * 4;
            for (int x = 0; x < W; ++x) {
                float fx = (x - hw - cx_) / z;
                float fy = (y - hh - cy_) / z;
                float rx = fx * cs - fy * sn;
                float ry = fx * sn + fy * cs;
                float sx = rx + hw, sy = ry + hh;
                int ix = (int)sx, iy = (int)sy;
                if ((unsigned)ix < (unsigned)pw && (unsigned)iy < (unsigned)ph) {
                    const float* fp = pd + ((size_t)iy * pw + ix) * 4;
                    float w = amt * dec;
                    float* p = o + (size_t)x * 4;
                    p[0] = std::clamp(p[0] + fp[0] * w, 0.0f, 1.0f);
                    p[1] = std::clamp(p[1] + fp[1] * w, 0.0f, 1.0f);
                    p[2] = std::clamp(p[2] + fp[2] * w, 0.0f, 1.0f);
                }
            }
        });
    }
};

FeedbackEffect::FeedbackEffect() : ArtifactAbstractEffect() {
    setPipelineStage(EffectPipelineStage::Rasterizer);
    syncImpls();
}
FeedbackEffect::~FeedbackEffect() = default;
float FeedbackEffect::amount() const { return amount_; }
void FeedbackEffect::setAmount(float v) { amount_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float FeedbackEffect::decay() const { return decay_; }
void FeedbackEffect::setDecay(float v) { decay_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float FeedbackEffect::centerOffsetX() const { return cx_; }
void FeedbackEffect::setCenterOffsetX(float v) { cx_ = v; syncImpls(); }
float FeedbackEffect::centerOffsetY() const { return cy_; }
void FeedbackEffect::setCenterOffsetY(float v) { cy_ = v; syncImpls(); }
float FeedbackEffect::zoom() const { return zoom_; }
void FeedbackEffect::setZoom(float v) { zoom_ = std::max(v, 0.01f); syncImpls(); }
float FeedbackEffect::rotation() const { return rotation_; }
void FeedbackEffect::setRotation(float v) { rotation_ = v; syncImpls(); }

std::vector<AbstractProperty> FeedbackEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(6);

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

    addFloat("amount", amount_, 0.0f, 1.0f);
    addFloat("decay", decay_, 0.0f, 1.0f);
    addFloat("centerX", cx_, -1000.0f, 1000.0f);
    addFloat("centerY", cy_, -1000.0f, 1000.0f);
    addFloat("zoom", zoom_, 0.01f, 10.0f);
    addFloat("rotation", rotation_, -180.0f, 180.0f);
    return props;
}
void FeedbackEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "amount") setAmount(v.toFloat());
    else if (k == "decay") setDecay(v.toFloat());
    else if (k == "centerX") setCenterOffsetX(v.toFloat());
    else if (k == "centerY") setCenterOffsetY(v.toFloat());
    else if (k == "zoom") setZoom(v.toFloat());
    else if (k == "rotation") setRotation(v.toFloat());
}
void FeedbackEffect::syncImpls() {
    auto c = std::make_shared<FeedbackCPUImpl>();
    c->amount_ = amount_; c->decay_ = decay_;
    c->cx_ = cx_; c->cy_ = cy_;
    c->zoom_ = zoom_; c->rotation_ = rotation_;
    setCPUImpl(c);
}

} // namespace Artifact
