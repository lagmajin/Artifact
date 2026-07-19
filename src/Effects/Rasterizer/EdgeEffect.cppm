module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>
#include <QColor>

module Artifact.Effect.Rasterizer.Edge;

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

class EdgeCPUImpl : public ArtifactEffectImplBase {
public:
    float mode_=0,intensity_=1,threshold_=0.1f,invert_=0;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height();
        const float it=std::clamp(intensity_,0.0f,10.0f);
        const float th=std::clamp(threshold_,0.0f,1.0f);
        const float inv=invert_>0.5f?1.0f:0.0f;

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();

        if(mode_>0.5f&&context_.sampler){
            // Motion edge: frame difference
            ImageF32x4RGBAWithCache prev;
            if(context_.sampler->sampleCurrentLayerFrameRelative(-1,prev)&&prev.width()>0&&prev.image().rgba32fData()){
                const float* pd=prev.image().rgba32fData();int pw=prev.width(),ph=prev.height();
                Parallel::For(0,H,[&](int y){float* o=d+(size_t)y*W*4;int py=std::min(y,ph-1);
                    for(int x=0;x<W;++x){int px=std::min(x,pw-1);
                        const float* pp=pd+((size_t)py*pw+px)*4;float* p=o+(size_t)x*4;
                        float dr=fabsf(p[0]-pp[0]),dg=fabsf(p[1]-pp[1]),db=fabsf(p[2]-pp[2]);
                        float e=std::max({dr,dg,db})*it;
                        if(e<th)e=0;if(inv>0.5f)e=1.0f-e;
                        p[0]=e;p[1]=e;p[2]=e;p[3]=1.0f;
                    }
                });
            }
        }else{
            // Spatial Sobel edge
            Parallel::For(1,H-1,[&](int y){float* o=d+(size_t)y*W*4;
                for(int x=1;x<W-1;++x){
                    auto L=[&](int ox,int oy)->float{const float*sp=sd+((size_t)(y+oy)*W+(x+ox))*4;return sp[0]*0.299f+sp[1]*0.587f+sp[2]*0.114f;};
                    float gx=-L(-1,-1)-2*L(-1,0)-L(-1,1)+L(1,-1)+2*L(1,0)+L(1,1);
                    float gy=-L(-1,-1)-2*L(0,-1)-L(1,-1)+L(-1,1)+2*L(0,1)+L(1,1);
                    float e=std::sqrt(gx*gx+gy*gy)*it;
                    if(e<th)e=0;if(inv>0.5f)e=1.0f-e;
                    float* p=o+(size_t)x*4;p[0]=e;p[1]=e;p[2]=e;p[3]=1.0f;
                }
            });
        }
    }
};

class RimLightCPUImpl : public ArtifactEffectImplBase {
public:
    float angle_ = 315.0f;
    float width_ = 8.0f;
    float softness_ = 0.55f;
    float intensity_ = 1.5f;
    float colorR_ = 1.0f;
    float colorG_ = 0.933f;
    float colorB_ = 0.745f;
    float colorA_ = 1.0f;
    float mix_ = 1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& image = src.image();
        const float* source = image.rgba32fData();
        const int width = static_cast<int>(image.width());
        const int height = static_cast<int>(image.height());
        if (!source || width <= 0 || height <= 0) {
            dst = src;
            return;
        }

        dst = src.DeepCopy();
        float* output = dst.image().rgba32fData();
        if (!output) return;

        constexpr float kPi = 3.14159265358979323846f;
        const float radians = angle_ * kPi / 180.0f;
        const float directionX = std::cos(radians);
        const float directionY = std::sin(radians);
        const int radius = std::clamp(static_cast<int>(std::ceil(width_)), 1, 64);
        const int sampleCount = std::min(radius, 8);
        const float softness = std::clamp(softness_, 0.0f, 1.0f);
        const float amount = std::clamp(intensity_, 0.0f, 5.0f) *
                             std::clamp(mix_, 0.0f, 1.0f) *
                             std::clamp(colorA_, 0.0f, 1.0f);

        const auto sampleAlpha = [&](float x, float y) {
            if (x < 0.0f || y < 0.0f ||
                x > static_cast<float>(width - 1) ||
                y > static_cast<float>(height - 1)) {
                return 0.0f;
            }
            const float cx = std::clamp(x, 0.0f, static_cast<float>(width - 1));
            const float cy = std::clamp(y, 0.0f, static_cast<float>(height - 1));
            const int x0 = static_cast<int>(std::floor(cx));
            const int y0 = static_cast<int>(std::floor(cy));
            const int x1 = std::min(x0 + 1, width - 1);
            const int y1 = std::min(y0 + 1, height - 1);
            const float tx = cx - static_cast<float>(x0);
            const float ty = cy - static_cast<float>(y0);
            const auto alphaAt = [&](int px, int py) {
                return source[(static_cast<size_t>(py) * width + px) * 4 + 3];
            };
            const float top = std::lerp(alphaAt(x0, y0), alphaAt(x1, y0), tx);
            const float bottom = std::lerp(alphaAt(x0, y1), alphaAt(x1, y1), tx);
            return std::lerp(top, bottom, ty);
        };

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const size_t index = (static_cast<size_t>(y) * width + x) * 4;
                const float alpha = std::clamp(source[index + 3], 0.0f, 1.0f);
                if (alpha <= 0.0001f) continue;

                float rim = 0.0f;
                for (int sampleIndex = 1; sampleIndex <= sampleCount; ++sampleIndex) {
                    const float step = std::max(
                        1.0f, static_cast<float>(radius) * sampleIndex /
                                  static_cast<float>(sampleCount));
                    const float sampledAlpha = sampleAlpha(
                        static_cast<float>(x) + directionX * step,
                        static_cast<float>(y) + directionY * step);
                    const float boundary = std::max(0.0f, alpha - sampledAlpha);
                    const float distanceWeight = 1.0f -
                        (step - 1.0f) / static_cast<float>(radius);
                    const float hardFalloff = distanceWeight * distanceWeight *
                                              distanceWeight * distanceWeight;
                    const float softFalloff = std::sqrt(std::max(0.0f, distanceWeight));
                    const float falloff = std::lerp(hardFalloff, softFalloff, softness);
                    rim = std::max(rim, boundary * falloff);
                }

                const float light = std::clamp(rim * amount, 0.0f, 1.0f) * alpha;
                output[index + 0] = source[index + 0] + colorR_ * light;
                output[index + 1] = source[index + 1] + colorG_ * light;
                output[index + 2] = source[index + 2] + colorB_ * light;
                output[index + 3] = source[index + 3];
            }
        }
    }
};

EdgeEffect::EdgeEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
EdgeEffect::~EdgeEffect()=default;
float EdgeEffect::mode()const{return mode_;}
void EdgeEffect::setMode(float v){mode_=std::clamp(v,0.0f,1.0f);syncImpls();}
float EdgeEffect::intensity()const{return intensity_;}
void EdgeEffect::setIntensity(float v){intensity_=std::clamp(v,0.0f,10.0f);syncImpls();}
float EdgeEffect::threshold()const{return threshold_;}
void EdgeEffect::setThreshold(float v){threshold_=std::clamp(v,0.0f,1.0f);syncImpls();}
float EdgeEffect::invert()const{return invert_;}
void EdgeEffect::setInvert(float v){invert_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> EdgeEffect::getProperties()const{
    std::vector<AbstractProperty> props;
    props.reserve(4);

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

    addFloat("mode", mode_, 0.0f, 1.0f);
    addFloat("intensity", intensity_, 0.0f, 10.0f);
    addFloat("threshold", threshold_, 0.0f, 1.0f);
    addFloat("invert", invert_, 0.0f, 1.0f);
    return props;
}
void EdgeEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="mode")setMode(v.toFloat());else if(k=="intensity")setIntensity(v.toFloat());else if(k=="threshold")setThreshold(v.toFloat());else if(k=="invert")setInvert(v.toFloat());
}
void EdgeEffect::syncImpls(){
    auto c=std::make_shared<EdgeCPUImpl>();c->mode_=mode_;c->intensity_=intensity_;c->threshold_=threshold_;c->invert_=invert_;setCPUImpl(c);
}

RimLightEffect::RimLightEffect() : ArtifactAbstractEffect() {
    setDisplayName(UniString("Rim Light / Edge Light"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    syncImpl();
}
RimLightEffect::~RimLightEffect() = default;
float RimLightEffect::angle() const { return angle_; }
void RimLightEffect::setAngle(float v) {
    angle_ = std::fmod(v, 360.0f);
    if (angle_ < 0.0f) angle_ += 360.0f;
    syncImpl();
}
float RimLightEffect::width() const { return width_; }
void RimLightEffect::setWidth(float v) { width_ = std::clamp(v, 1.0f, 64.0f); syncImpl(); }
float RimLightEffect::softness() const { return softness_; }
void RimLightEffect::setSoftness(float v) { softness_ = std::clamp(v, 0.0f, 1.0f); syncImpl(); }
float RimLightEffect::intensity() const { return intensity_; }
void RimLightEffect::setIntensity(float v) { intensity_ = std::clamp(v, 0.0f, 5.0f); syncImpl(); }
QColor RimLightEffect::color() const { return color_; }
void RimLightEffect::setColor(const QColor& v) { if (v.isValid()) color_ = v; syncImpl(); }
float RimLightEffect::mix() const { return mix_; }
void RimLightEffect::setMix(float v) { mix_ = std::clamp(v, 0.0f, 1.0f); syncImpl(); }

std::vector<AbstractProperty> RimLightEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(6);
    const auto addFloat = [&props](const char* name, float value,
                                   float minValue, float maxValue) {
        auto& property = props.emplace_back();
        property.setName(QString::fromLatin1(name));
        property.setType(PropertyType::Float);
        property.setValue(QVariant(static_cast<double>(value)));
        property.setDefaultValue(QVariant(static_cast<double>(value)));
        property.setMinValue(QVariant(static_cast<double>(minValue)));
        property.setMaxValue(QVariant(static_cast<double>(maxValue)));
    };
    addFloat("Angle", angle_, 0.0f, 360.0f);
    addFloat("Width", width_, 1.0f, 64.0f);
    addFloat("Softness", softness_, 0.0f, 1.0f);
    addFloat("Intensity", intensity_, 0.0f, 5.0f);
    auto& colorProperty = props.emplace_back();
    colorProperty.setName(QStringLiteral("Color"));
    colorProperty.setType(PropertyType::Color);
    colorProperty.setValue(QVariant::fromValue(color_));
    colorProperty.setDefaultValue(QVariant::fromValue(color_));
    addFloat("Mix", mix_, 0.0f, 1.0f);
    return props;
}

void RimLightEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Angle")) setAngle(value.toFloat());
    else if (key == QStringLiteral("Width")) setWidth(value.toFloat());
    else if (key == QStringLiteral("Softness")) setSoftness(value.toFloat());
    else if (key == QStringLiteral("Intensity")) setIntensity(value.toFloat());
    else if (key == QStringLiteral("Color")) setColor(value.value<QColor>());
    else if (key == QStringLiteral("Mix")) setMix(value.toFloat());
}

void RimLightEffect::syncImpl() {
    auto impl = std::make_shared<RimLightCPUImpl>();
    impl->angle_ = angle_;
    impl->width_ = width_;
    impl->softness_ = softness_;
    impl->intensity_ = intensity_;
    impl->colorR_ = static_cast<float>(color_.redF());
    impl->colorG_ = static_cast<float>(color_.greenF());
    impl->colorB_ = static_cast<float>(color_.blueF());
    impl->colorA_ = static_cast<float>(color_.alphaF());
    impl->mix_ = mix_;
    setCPUImpl(impl);
}
} // namespace Artifact
