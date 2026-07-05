module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Vignette;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class VignetteCPUImpl : public ArtifactEffectImplBase {
public:
    float amount_=0.7f,radius_=0.8f,feather_=0.4f,cx_=0.5f,cy_=0.5f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height();
        float a=std::clamp(amount_,0.0f,1.0f),r=std::clamp(radius_,0.0f,2.0f),f=std::clamp(feather_,0.01f,2.0f);
        float cx=cx_*(float)W,cy=cy_*(float)H,maxD=std::sqrt(cx*cx+cy*cy)*r;
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float dx=(float)x-cx,dy=(float)y-cy,dist=std::sqrt(dx*dx+dy*dy);
                float mask=1.0f-std::clamp((dist-maxD*f)/(maxD*(1.0f-f)+0.001f),0.0f,1.0f)*a;
                p[0]*=mask;p[1]*=mask;p[2]*=mask;
            }
        }
    }
};

VignetteEffect::VignetteEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
VignetteEffect::~VignetteEffect()=default;
float VignetteEffect::amount()const{return amount_;}void VignetteEffect::setAmount(float v){amount_=std::clamp(v,0.0f,1.0f);syncImpls();}
float VignetteEffect::radius()const{return radius_;}void VignetteEffect::setRadius(float v){radius_=std::clamp(v,0.0f,2.0f);syncImpls();}
float VignetteEffect::feather()const{return feather_;}void VignetteEffect::setFeather(float v){feather_=std::clamp(v,0.01f,2.0f);syncImpls();}
float VignetteEffect::centerX()const{return cx_;}void VignetteEffect::setCenterX(float v){cx_=std::clamp(v,0.0f,1.0f);syncImpls();}
float VignetteEffect::centerY()const{return cy_;}void VignetteEffect::setCenterY(float v){cy_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> VignetteEffect::getProperties()const{
    std::vector<AbstractProperty> props;
    props.reserve(5);

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
    addFloat("radius", radius_, 0.0f, 2.0f);
    addFloat("feather", feather_, 0.01f, 2.0f);
    addFloat("centerX", cx_, 0.0f, 1.0f);
    addFloat("centerY", cy_, 0.0f, 1.0f);
    return props;
}
void VignetteEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="amount")setAmount(v.toFloat());else if(k=="radius")setRadius(v.toFloat());else if(k=="feather")setFeather(v.toFloat());else if(k=="centerX")setCenterX(v.toFloat());else if(k=="centerY")setCenterY(v.toFloat());}
void VignetteEffect::syncImpls(){auto c=std::make_shared<VignetteCPUImpl>();c->amount_=amount_;c->radius_=radius_;c->feather_=feather_;c->cx_=cx_;c->cy_=cy_;setCPUImpl(c);}
} // namespace Artifact
