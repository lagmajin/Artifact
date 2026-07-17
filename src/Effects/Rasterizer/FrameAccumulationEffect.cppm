module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.FrameAccumulation;

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

class FrameAccumCPUImpl : public ArtifactEffectImplBase {
public:
    float persistence_ = 0.95f, blend_ = 0.3f;
    std::vector<float> accum_;
    int aw_ = 0, ah_ = 0;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& si = src.image();
        const float* sd = si.rgba32fData();
        if (!sd || si.width() <= 0) { dst = src; return; }
        const int W = si.width(), H = si.height();
        const float p = std::clamp(persistence_, 0.0f, 1.0f);
        const float b = std::clamp(blend_, 0.0f, 1.0f);

        const size_t n = (size_t)W * H * 4;
        if (aw_ != W || ah_ != H) { accum_.assign(n, 0.0f); aw_ = W; ah_ = H; }

        // Try reading previous accumulation from sampler (our own last output).
        bool havePrev = false;
        if (context_.sampler) {
            ImageF32x4RGBAWithCache prev;
            if (context_.sampler->sampleCurrentLayerFrameRelative(-1, prev)
                && prev.width() == W && prev.height() == H
                && prev.image().rgba32fData()) {
                const float* pd = prev.image().rgba32fData();
                std::memcpy(accum_.data(), pd, n * sizeof(float));
                havePrev = true;
            }
        }
        if (!havePrev) {
            // No previous frame — seed with current.
            std::memcpy(accum_.data(), sd, n * sizeof(float));
        }

        // Apply persistence decay to accumulation buffer.
        ArtifactCore::Parallel::For(0,H,[&](int y){
            const size_t begin=static_cast<size_t>(y)*W*4;
            for(int i=0;i<W*4;++i)accum_[begin+i]*=p;
        });

        // Add current frame contribution.
        ArtifactCore::Parallel::For(0,H,[&](int y){
            const size_t begin=static_cast<size_t>(y)*W*4;
            for(int i=0;i<W*4;++i)accum_[begin+i]=std::clamp(accum_[begin+i]+sd[begin+i]*(1.0f-p),0.0f,1.0f);
        });

        // Blend: accum ↔ current.
        dst = src.DeepCopy();
        float* d = dst.image().rgba32fData();
        ArtifactCore::Parallel::For(0,H,[&](int y){
            const size_t begin=static_cast<size_t>(y)*W*4;
            for(int i=0;i<W*4;++i)d[begin+i]=accum_[begin+i]*(1.0f-b)+sd[begin+i]*b;
        });
    }
};

FrameAccumulationEffect::FrameAccumulationEffect() : ArtifactAbstractEffect() {
    setPipelineStage(EffectPipelineStage::Rasterizer);
    syncImpls();
}
FrameAccumulationEffect::~FrameAccumulationEffect() = default;
float FrameAccumulationEffect::persistence() const { return persistence_; }
void FrameAccumulationEffect::setPersistence(float v) { persistence_=std::clamp(v,0.0f,1.0f); syncImpls(); }
float FrameAccumulationEffect::blend() const { return blend_; }
void FrameAccumulationEffect::setBlend(float v) { blend_=std::clamp(v,0.0f,1.0f); syncImpls(); }
void FrameAccumulationEffect::resetAccumulation() {
    if (auto* c = dynamic_cast<FrameAccumCPUImpl*>(cpuImpl().get())) {
        c->accum_.assign(c->accum_.size(), 0.0f);
    }
}

std::vector<AbstractProperty> FrameAccumulationEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(2);

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

    addFloat("persistence", persistence_, 0.0f, 1.0f);
    addFloat("blend", blend_, 0.0f, 1.0f);
    return props;
}
void FrameAccumulationEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "persistence") setPersistence(v.toFloat());
    else if (k == "blend") setBlend(v.toFloat());
}
void FrameAccumulationEffect::syncImpls() {
    auto c = std::make_shared<FrameAccumCPUImpl>();
    c->persistence_ = persistence_;
    c->blend_ = blend_;
    setCPUImpl(c);
}

} // namespace Artifact
