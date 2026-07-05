module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.RadialBlur;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class RadialBlurCPUImpl : public ArtifactEffectImplBase {
public:
    float amount_=0.5f,cx_=0.5f,cy_=0.5f,mode_=0; int quality_=8;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height(),q=std::clamp(quality_,2,32);
        const float a=std::clamp(amount_,0.0f,2.0f);
        float cx=cx_*(float)W,cy=cy_*(float)H;

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float dx=(float)x-cx,dy=(float)y-cy;
                float r0=p[0],g0=p[1],b0=p[2],a0=p[3],tw=1.0f;
                for(int i=1;i<=q;++i){float t=((float)i-0.5f)/(float)q;
                    int sx, sy;
                    if(mode_<=0.5f){// zoom
                        sx=(int)(cx+dx*(1.0f+a*t)+0.5f);sy=(int)(cy+dy*(1.0f+a*t)+0.5f);
                    }else{// spin
                        float ang=a*t;float cs=std::cos(ang),sn=std::sin(ang);
                        float rx=dx*cs-dy*sn,ry=dx*sn+dy*cs;
                        sx=(int)(cx+rx+0.5f);sy=(int)(cy+ry+0.5f);
                    }
                    sx=std::clamp(sx,0,W-1);sy=std::clamp(sy,0,H-1);
                    auto*sp=sd+((size_t)sy*W+sx)*4;r0+=sp[0];g0+=sp[1];b0+=sp[2];a0+=sp[3];tw+=1.0f;
                }
                float inv=1.0f/tw;p[0]=r0*inv;p[1]=g0*inv;p[2]=b0*inv;p[3]=a0*inv;
            }
        }
    }
};

RadialBlurEffect::RadialBlurEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
RadialBlurEffect::~RadialBlurEffect()=default;
float RadialBlurEffect::amount()const{return amount_;}void RadialBlurEffect::setAmount(float v){amount_=std::clamp(v,0.0f,2.0f);syncImpls();}
int RadialBlurEffect::quality()const{return quality_;}void RadialBlurEffect::setQuality(int v){quality_=std::clamp(v,2,32);syncImpls();}
float RadialBlurEffect::centerX()const{return cx_;}void RadialBlurEffect::setCenterX(float v){cx_=std::clamp(v,0.0f,1.0f);syncImpls();}
float RadialBlurEffect::centerY()const{return cy_;}void RadialBlurEffect::setCenterY(float v){cy_=std::clamp(v,0.0f,1.0f);syncImpls();}
float RadialBlurEffect::mode()const{return mode_;}void RadialBlurEffect::setMode(float v){mode_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> RadialBlurEffect::getProperties()const{
    std::vector<AbstractProperty> props;
    props.reserve(5);

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

    addFloat("amount", amount_, 0.0f, 2.0f);
    addInt("quality", quality_, 2, 32);
    addFloat("centerX", cx_, 0.0f, 1.0f);
    addFloat("centerY", cy_, 0.0f, 1.0f);
    addFloat("mode", mode_, 0.0f, 1.0f);
    return props;
}
void RadialBlurEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="amount")setAmount(v.toFloat());else if(k=="quality")setQuality(v.toInt());else if(k=="centerX")setCenterX(v.toFloat());else if(k=="centerY")setCenterY(v.toFloat());else if(k=="mode")setMode(v.toFloat());}
void RadialBlurEffect::syncImpls(){auto c=std::make_shared<RadialBlurCPUImpl>();c->amount_=amount_;c->quality_=quality_;c->cx_=cx_;c->cy_=cy_;c->mode_=mode_;setCPUImpl(c);}
} // namespace Artifact
