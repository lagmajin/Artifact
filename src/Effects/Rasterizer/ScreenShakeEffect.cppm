module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.ScreenShake;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import ImageProcessing.Distortion;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

namespace {

// mirror (三角波) 座標: [0,size) を往復させる
inline float mirrorCoord(float v, int size) {
    if (size <= 1) return 0.0f;
    const float s = static_cast<float>(size);
    float t = v;
    t = std::fmod(t, 2.0f * s);
    if (t < 0.0f) t += 2.0f * s;
    if (t >= s) t = 2.0f * s - t;
    return t;
}

inline float clampCoord(float v, int size) {
    if (size <= 1) return 0.0f;
    return std::clamp(v, 0.0f, static_cast<float>(size - 1));
}

} // namespace

class ScreenShakeCPUImpl : public ArtifactEffectImplBase {
public:
    float ampX_=20.0f, ampY_=20.0f, freq_=2.0f, decay_=0.0f;
    int   seed_=0, wrap_=1;   // 0=clamp, 1=wrap, 2=mirror

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const ImageF32x4_RGBA& si = src.image();
        if (si.width()<=0 || si.height()<=0) { dst = src; return; }

        const float t  = static_cast<float>(context_.timeSeconds);
        // trauma-ish envelope: amplitude fades with decay over time
        const float env = std::max(0.0f, 1.0f - decay_ * t);
        // value noise in [0,1]; remap to [-1,1]
        const float nx = valueNoise2D(t * freq_, 0.0f, seed_) - 0.5f;
        const float ny = valueNoise2D(0.0f, t * freq_, seed_ + 17) - 0.5f;
        const float dx = 2.0f * ampX_ * nx * env;
        const float dy = 2.0f * ampY_ * ny * env;

        if (wrap_ == 1) {
            // free path: makeOffset + applyDisplacement (always wraps)
            ImageF32x4_RGBA tmp;
            applyDisplacement(si, tmp, makeOffset(dx, dy));
            dst.image().setFromRGBA32F(tmp.rgba32fData(), si.width(), si.height());
            return;
        }

        // clamp / mirror: custom sampler using Distortion::sampleBilinear
        const int W = si.width(), H = si.height();
        ImageF32x4_RGBA tmp;
        tmp.allocate(W, H);
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                const float sx = (wrap_ == 2) ? mirrorCoord(static_cast<float>(x) + dx, W)
                                              : clampCoord(static_cast<float>(x) + dx, W);
                const float sy = (wrap_ == 2) ? mirrorCoord(static_cast<float>(y) + dy, H)
                                              : clampCoord(static_cast<float>(y) + dy, H);
                tmp.setPixel(x, y, sampleBilinear(si, sx, sy));
            }
        }
        dst.image().setFromRGBA32F(tmp.rgba32fData(), W, H);
    }
};

ScreenShakeEffect::ScreenShakeEffect():ArtifactAbstractEffect(){
    setDisplayName(UniString("Screen Shake"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setComputeMode(ComputeMode::CPU);
    syncImpls();
}
ScreenShakeEffect::~ScreenShakeEffect()=default;

float ScreenShakeEffect::amplitudeX()const{return amplitudeX_;}
void  ScreenShakeEffect::setAmplitudeX(float v){amplitudeX_=std::max(v,0.0f);syncImpls();}
float ScreenShakeEffect::amplitudeY()const{return amplitudeY_;}
void  ScreenShakeEffect::setAmplitudeY(float v){amplitudeY_=std::max(v,0.0f);syncImpls();}
float ScreenShakeEffect::frequency()const{return frequency_;}
void  ScreenShakeEffect::setFrequency(float v){frequency_=std::max(v,0.01f);syncImpls();}
float ScreenShakeEffect::decay()const{return decay_;}
void  ScreenShakeEffect::setDecay(float v){decay_=std::max(v,0.0f);syncImpls();}
int   ScreenShakeEffect::seed()const{return seed_;}
void  ScreenShakeEffect::setSeed(int v){seed_=v;syncImpls();}
int   ScreenShakeEffect::wrapMode()const{return wrapMode_;}
void  ScreenShakeEffect::setWrapMode(int v){wrapMode_=std::clamp(v,0,2);syncImpls();}

std::vector<AbstractProperty> ScreenShakeEffect::getProperties()const{
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

    addFloat("amplitudeX", amplitudeX_, 0.0f, 500.0f);
    addFloat("amplitudeY", amplitudeY_, 0.0f, 500.0f);
    addFloat("frequency", frequency_, 0.01f, 60.0f);
    addFloat("decay", decay_, 0.0f, 10.0f);
    addInt("seed", seed_, 0, 9999);
    addInt("wrapMode", wrapMode_, 0, 2);
    return props;
}
void ScreenShakeEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="amplitudeX")setAmplitudeX(v.toFloat());
    else if(k=="amplitudeY")setAmplitudeY(v.toFloat());
    else if(k=="frequency")setFrequency(v.toFloat());
    else if(k=="decay")setDecay(v.toFloat());
    else if(k=="seed")setSeed(v.toInt());
    else if(k=="wrapMode")setWrapMode(v.toInt());
}
void ScreenShakeEffect::syncImpls(){
    auto c=std::make_shared<ScreenShakeCPUImpl>();
    c->ampX_=amplitudeX_; c->ampY_=amplitudeY_; c->freq_=frequency_;
    c->decay_=decay_; c->seed_=seed_; c->wrap_=wrapMode_;
    setCPUImpl(c);
}
} // namespace Artifact
