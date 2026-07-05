module;
#include <algorithm>
#include <cmath>
#include <numbers>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Stripes;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class StripesCPUImpl : public ArtifactEffectImplBase {
public:
    float frequency_=10,angle_=0,thickness_=0.5f,offset_=0;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image(); int W=si.width(),H=si.height();
        float f=std::max(frequency_,0.5f),th=std::clamp(thickness_,0.0f,1.0f);
        float rad=angle_*std::numbers::pi_v<float>/180.0f;
        float cs=std::cos(rad),sn=std::sin(rad);
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float proj=((float)x*cs+(float)y*sn)*f/(float)std::max(W,H)+offset_;
                float v=std::abs(std::fmod(proj,1.0f));
                v=v<th?1.0f:0.0f;
                p[0]=v;p[1]=v;p[2]=v;p[3]=1.0f;
            }
        }
    }
};

StripesEffect::StripesEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
StripesEffect::~StripesEffect()=default;
float StripesEffect::frequency()const{return frequency_;}void StripesEffect::setFrequency(float v){frequency_=std::max(v,0.5f);syncImpls();}
float StripesEffect::angle()const{return angle_;}void StripesEffect::setAngle(float v){angle_=v;syncImpls();}
float StripesEffect::thickness()const{return thickness_;}void StripesEffect::setThickness(float v){thickness_=std::clamp(v,0.0f,1.0f);syncImpls();}
float StripesEffect::offset()const{return offset_;}void StripesEffect::setOffset(float v){offset_=v;syncImpls();}
std::vector<AbstractProperty> StripesEffect::getProperties()const{
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

    addFloat("frequency", frequency_, 0.5f, 200.0f);
    addFloat("angle", angle_, 0.0f, 360.0f);
    addFloat("thickness", thickness_, 0.0f, 1.0f);
    addFloat("offset", offset_, -10.0f, 10.0f);
    return props;
}
void StripesEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="frequency")setFrequency(v.toFloat());else if(k=="angle")setAngle(v.toFloat());else if(k=="thickness")setThickness(v.toFloat());else if(k=="offset")setOffset(v.toFloat());}
void StripesEffect::syncImpls(){auto c=std::make_shared<StripesCPUImpl>();c->frequency_=frequency_;c->angle_=angle_;c->thickness_=thickness_;c->offset_=offset_;setCPUImpl(c);}
} // namespace Artifact
