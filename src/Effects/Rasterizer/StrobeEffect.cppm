module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Strobe;

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

class StrobeCPUImpl : public ArtifactEffectImplBase {
public:
    float frequency_=4.0f,mixAmount_=0.0f;
    bool refCaptured_=false;
    ImageF32x4RGBAWithCache ref_;

    void captureReference(){refCaptured_=true;}
    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        if(!refCaptured_){ref_=src.DeepCopy();refCaptured_=true;}
        double t=context_.timeSeconds;
        double period=1.0/std::max<double>(frequency_,0.1);
        double phase=std::fmod(t,period)/period;
        bool on=phase<0.5;

        dst=src.DeepCopy();
        if(on&&ref_.width()>0&&ref_.image().rgba32fData()){
            float* d=dst.image().rgba32fData();const float* rd=ref_.image().rgba32fData();
            const auto& si=src.image();int W=si.width(),H=si.height();
            int rw=ref_.width(),rh=ref_.height();
            float m=std::clamp(mixAmount_,0.0f,1.0f);
            Parallel::For(0,H,[&](int y){float* o=d+(size_t)y*W*4;int ry=std::min(y,rh-1);
                for(int x=0;x<W;++x){int rx=std::min(x,rw-1);
                    const float*rp=rd+((size_t)ry*rw+rx)*4;float*p=o+(size_t)x*4;
                    p[0]=p[0]*(1-m)+rp[0]*m;p[1]=p[1]*(1-m)+rp[1]*m;
                    p[2]=p[2]*(1-m)+rp[2]*m;p[3]=p[3]*(1-m)+rp[3]*m;
                }
            });
        }
    }
};

StrobeEffect::StrobeEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
StrobeEffect::~StrobeEffect()=default;
float StrobeEffect::frequency()const{return frequency_;}
void StrobeEffect::setFrequency(float v){frequency_=std::max<double>(v,0.1);syncImpls();}
float StrobeEffect::mixAmount()const{return mixAmount_;}
void StrobeEffect::setMixAmount(float v){mixAmount_=std::clamp(v,0.0f,1.0f);syncImpls();}
void StrobeEffect::captureReference(){if(auto* c=dynamic_cast<StrobeCPUImpl*>(cpuImpl().get()))c->captureReference();}
std::vector<AbstractProperty> StrobeEffect::getProperties()const{
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

    addFloat("frequency", frequency_, 0.1f, 60.0f);
    addFloat("mixAmount", mixAmount_, 0.0f, 1.0f);
    return props;
}
void StrobeEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="frequency")setFrequency(v.toFloat());
    else if(k=="mixAmount")setMixAmount(v.toFloat());
}
void StrobeEffect::syncImpls(){
    auto c=std::make_shared<StrobeCPUImpl>();c->frequency_=frequency_;c->mixAmount_=mixAmount_;setCPUImpl(c);
}
} // namespace Artifact
