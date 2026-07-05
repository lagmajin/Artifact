module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.TemporalMedian;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class TemporalMedianCPUImpl : public ArtifactEffectImplBase {
public:
    int frameCount_=5; float blend_=1.0f; int channel_=0;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0||!context_.sampler){dst=src;return;}
        const int W=si.width(),H=si.height(),fc=std::clamp(frameCount_,3,16);
        const float bl=std::clamp(blend_,0.0f,1.0f);
        const size_t n=(size_t)W*H;

        // Collect past frames
        struct Rf{ImageF32x4_RGBA img;};
        std::vector<Rf> refs;
        for(int i=1;i<=fc;++i){
            ImageF32x4RGBAWithCache f;
            if(context_.sampler->sampleCurrentLayerFrameRelative((std::int64_t)(-i),f)&&f.width()==W&&f.height()==H&&f.image().rgba32fData())
                refs.push_back({f.image()});
        }
        if(refs.empty()){dst=src.DeepCopy();return;}

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        int rfCount=(int)refs.size();

        for(size_t px=0;px<n;++px){
            // Gather per-channel values
            float vals[4][16]; int vi=0;
            for(auto& rf:refs){
                const float* rp=rf.img.rgba32fData()+px*4;
                vals[0][vi]=rp[0];vals[1][vi]=rp[1];vals[2][vi]=rp[2];vals[3][vi]=rp[3];++vi;
            }
            float* p=d+px*4;
            for(int c=0;c<4;++c){
                std::sort(vals[c],vals[c]+rfCount);
                float med=vals[c][rfCount/2];
                p[c]=p[c]*(1.0f-bl)+med*bl;
            }
            if(channel_>0&&channel_<=3){
                // Only apply to single channel, restore others
                int ch=channel_-1;
                std::sort(vals[ch],vals[ch]+rfCount);
                p[ch]=p[ch]*(1.0f-bl)+vals[ch][rfCount/2]*bl;
            }else if(channel_==4){
                // Luma only
                float sl=p[0]*0.299f+p[1]*0.587f+p[2]*0.114f;
                float lr[16];for(int i=0;i<rfCount;++i){const float*rp=refs[i].img.rgba32fData()+px*4;lr[i]=rp[0]*0.299f+rp[1]*0.587f+rp[2]*0.114f;}
                std::sort(lr,lr+rfCount);float ml=lr[rfCount/2];
                float ratio=ml/std::max(sl,0.001f);
                p[0]=std::clamp(p[0]*ratio,0.0f,1.0f);p[1]=std::clamp(p[1]*ratio,0.0f,1.0f);p[2]=std::clamp(p[2]*ratio,0.0f,1.0f);
            }
        }
    }
};

TemporalMedianEffect::TemporalMedianEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
TemporalMedianEffect::~TemporalMedianEffect()=default;
int TemporalMedianEffect::frameCount()const{return frameCount_;}
void TemporalMedianEffect::setFrameCount(int v){frameCount_=std::clamp(v,3,16);syncImpls();}
float TemporalMedianEffect::blend()const{return blend_;}
void TemporalMedianEffect::setBlend(float v){blend_=std::clamp(v,0.0f,1.0f);syncImpls();}
int TemporalMedianEffect::channel()const{return channel_;}
void TemporalMedianEffect::setChannel(int v){channel_=std::clamp(v,0,4);syncImpls();}
std::vector<AbstractProperty> TemporalMedianEffect::getProperties()const{
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

    addInt("frameCount", frameCount_, 3, 16);
    addFloat("blend", blend_, 0.0f, 1.0f);
    addInt("channel", channel_, 0, 4);
    return props;
}
void TemporalMedianEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="frameCount")setFrameCount(v.toInt());else if(k=="blend")setBlend(v.toFloat());else if(k=="channel")setChannel(v.toInt());
}
void TemporalMedianEffect::syncImpls(){
    auto c=std::make_shared<TemporalMedianCPUImpl>();c->frameCount_=frameCount_;c->blend_=blend_;c->channel_=channel_;setCPUImpl(c);
}
} // namespace Artifact
