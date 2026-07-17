module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.FrameAverage;

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

class FrameAverageCPUImpl : public ArtifactEffectImplBase {
public:
    int frameCount_=4; float temporalWeight_=0.9f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image(); const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0||!context_.sampler){dst=src;return;}
        const int W=si.width(),H=si.height(),fc=std::clamp(frameCount_,1,16);
        const float tw=std::clamp(temporalWeight_,0.0f,1.0f);
        const size_t n=(size_t)W*H;

        struct Ref{ImageF32x4_RGBA img; float weight;};
        std::vector<Ref> refs;
        float totalW=1.0f,w=1.0f;
        for(int i=1;i<=fc;++i){w*=tw;if(w<0.001f)break;
            ImageF32x4RGBAWithCache f;
            if(context_.sampler->sampleCurrentLayerFrameRelative((std::int64_t)(-i),f)
               &&f.width()==W&&f.height()==H&&f.image().rgba32fData()){
                refs.push_back({f.image(),w});totalW+=w;
            }
        }
        if(refs.empty()){dst=src.DeepCopy();return;}

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        float inv=1.0f/totalW;
        ArtifactCore::Parallel::For(0,H,[&](int y){
            for(int x=0;x<W;++x){const size_t i=(static_cast<size_t>(y)*W+x);float acc[4]={d[i*4],d[i*4+1],d[i*4+2],d[i*4+3]};
                for(const auto& rf:refs){const float* rp=rf.img.rgba32fData()+i*4;
                    acc[0]+=rp[0]*rf.weight;acc[1]+=rp[1]*rf.weight;acc[2]+=rp[2]*rf.weight;acc[3]+=rp[3]*rf.weight;
                }
                d[i*4]=acc[0]*inv;d[i*4+1]=acc[1]*inv;d[i*4+2]=acc[2]*inv;d[i*4+3]=acc[3]*inv;
            }
        });
    }
};

FrameAverageEffect::FrameAverageEffect():ArtifactAbstractEffect(){
    setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();
}
FrameAverageEffect::~FrameAverageEffect()=default;
int FrameAverageEffect::frameCount()const{return frameCount_;}
void FrameAverageEffect::setFrameCount(int v){frameCount_=std::clamp(v,1,16);syncImpls();}
float FrameAverageEffect::temporalWeight()const{return temporalWeight_;}
void FrameAverageEffect::setTemporalWeight(float v){temporalWeight_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> FrameAverageEffect::getProperties()const{
    std::vector<AbstractProperty> props;
    props.reserve(2);

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

    addInt("frameCount", frameCount_, 1, 16);
    addFloat("temporalWeight", temporalWeight_, 0.0f, 1.0f);
    return props;
}
void FrameAverageEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="frameCount")setFrameCount(v.toInt());
    else if(k=="temporalWeight")setTemporalWeight(v.toFloat());
}
void FrameAverageEffect::syncImpls(){
    auto c=std::make_shared<FrameAverageCPUImpl>();
    c->frameCount_=frameCount_;c->temporalWeight_=temporalWeight_;setCPUImpl(c);
}
} // namespace Artifact
