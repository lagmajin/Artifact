module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.TimeBlur;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class TimeBlurCPUImpl : public ArtifactEffectImplBase {
public:
    float sigma_=3.0f; int lookback_=8; float direction_=0.0f;

    static float gauss(float x,float sig){return std::exp(-0.5f*(x*x)/std::max(sig*sig,0.001f));}

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0||!context_.sampler){dst=src;return;}
        const int W=si.width(),H=si.height(),lb=std::clamp(lookback_,1,32);
        const float sg=std::max(sigma_,0.5f),dir=std::clamp(direction_,0.0f,1.0f);
        const size_t n=(size_t)W*H;

        // Gaussian-weighted accumulation across frames
        struct Rf{ImageF32x4_RGBA img; float w;};
        std::vector<Rf> refs;
        float totalW=1.0f; // current frame weight = 1.0

        for(int i=1;i<=lb;++i){
            float w=gauss((float)i,sg);
            if(w<0.001f)continue;
            ImageF32x4RGBAWithCache f;
            if(context_.sampler->sampleCurrentLayerFrameRelative((std::int64_t)(-i),f)&&f.width()==W&&f.height()==H&&f.image().rgba32fData()){
                refs.push_back({f.image(),w});totalW+=w;
            }
        }
        // Also sample forward if centered mode
        if(dir<=0.5f){
            for(int i=1;i<=lb;++i){
                float w=gauss((float)i,sg);
                if(w<0.001f)continue;
                ImageF32x4RGBAWithCache f;
                if(context_.sampler->sampleCurrentLayerFrameRelative((std::int64_t)(i),f)&&f.width()==W&&f.height()==H&&f.image().rgba32fData()){
                    refs.push_back({f.image(),w});totalW+=w;
                }
            }
        }

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        if(refs.empty())return;
        float inv=1.0f/totalW;

        for(size_t i=0;i<n;++i){
            float acc[4]={d[i*4],d[i*4+1],d[i*4+2],d[i*4+3]};
            for(auto& rf:refs){const float* rp=rf.img.rgba32fData()+i*4;
                acc[0]+=rp[0]*rf.w;acc[1]+=rp[1]*rf.w;acc[2]+=rp[2]*rf.w;acc[3]+=rp[3]*rf.w;}
            d[i*4]=acc[0]*inv;d[i*4+1]=acc[1]*inv;d[i*4+2]=acc[2]*inv;d[i*4+3]=acc[3]*inv;
        }
    }
};

TimeBlurEffect::TimeBlurEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
TimeBlurEffect::~TimeBlurEffect()=default;
float TimeBlurEffect::sigma()const{return sigma_;}
void TimeBlurEffect::setSigma(float v){sigma_=std::max(v,0.5f);syncImpls();}
int TimeBlurEffect::lookback()const{return lookback_;}
void TimeBlurEffect::setLookback(int v){lookback_=std::clamp(v,1,32);syncImpls();}
float TimeBlurEffect::direction()const{return direction_;}
void TimeBlurEffect::setDirection(float v){direction_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> TimeBlurEffect::getProperties()const{
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

    addFloat("sigma", sigma_, 0.5f, 16.0f);
    addInt("lookback", lookback_, 1, 32);
    addFloat("direction", direction_, 0.0f, 1.0f);
    return props;
}
void TimeBlurEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="sigma")setSigma(v.toFloat());else if(k=="lookback")setLookback(v.toInt());else if(k=="direction")setDirection(v.toFloat());
}
void TimeBlurEffect::syncImpls(){
    auto c=std::make_shared<TimeBlurCPUImpl>();c->sigma_=sigma_;c->lookback_=lookback_;c->direction_=direction_;setCPUImpl(c);
}
} // namespace Artifact
