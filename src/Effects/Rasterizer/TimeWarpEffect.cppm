module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.TimeWarp;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class TimeWarpCPUImpl : public ArtifactEffectImplBase {
public:
    float maxOffset_=8, smoothness_=0.5f; int channel_=0;

    static float sampleCh(const float* p,int ch){
        switch(ch){case 1:return p[0];case 2:return p[1];case 3:return p[2];case 4:return p[3];
        default:return p[0]*0.299f+p[1]*0.587f+p[2]*0.114f;}
    }

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0||!context_.sampler){dst=src;return;}
        const int W=si.width(),H=si.height();
        const float mo=std::max(maxOffset_,1.0f),sm=std::clamp(smoothness_,0.0f,1.0f);
        const int kSize=std::max(1,(int)(sm*16.0f));
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();

        for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){
                // Blur the driving channel for smooth temporal mapping
                float avg=0;int cnt=0;
                for(int dy=-kSize;dy<=kSize;dy+=std::max(1,kSize/2))
                for(int dx=-kSize;dx<=kSize;dx+=std::max(1,kSize/2)){
                    int cx=x+dx,cy=y+dy;
                    if((unsigned)cx<(unsigned)W&&(unsigned)cy<(unsigned)H){
                        avg+=sampleCh(sd+((size_t)cy*W+cx)*4,channel_);++cnt;
                    }
                }
                if(cnt>0)avg/=(float)cnt;
                float off=(avg-0.5f)*2.0f*mo;
                auto frameOff=(std::int64_t)std::llround(off);

                ImageF32x4RGBAWithCache tf;
                bool ok=context_.sampler->sampleCurrentLayerFrameRelative(frameOff,tf)
                       &&tf.width()>0&&tf.image().rgba32fData();
                if(ok){
                    const float* td=tf.image().rgba32fData();
                    int tw=tf.width(),th=tf.height();
                    int tx=std::clamp(x,0,tw-1),ty=std::clamp(y,0,th-1);
                    const float* tp=td+((size_t)ty*tw+tx)*4;
                    float* p=o+(size_t)x*4;
                    p[0]=tp[0];p[1]=tp[1];p[2]=tp[2];p[3]=tp[3];
                }
            }
        }
    }
};

TimeWarpEffect::TimeWarpEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
TimeWarpEffect::~TimeWarpEffect()=default;
float TimeWarpEffect::maxOffsetFrames()const{return maxOffset_;}
void TimeWarpEffect::setMaxOffsetFrames(float v){maxOffset_=std::max(v,1.0f);syncImpls();}
int TimeWarpEffect::channel()const{return channel_;}
void TimeWarpEffect::setChannel(int v){channel_=std::clamp(v,0,4);syncImpls();}
float TimeWarpEffect::smoothness()const{return smoothness_;}
void TimeWarpEffect::setSmoothness(float v){smoothness_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> TimeWarpEffect::getProperties()const{
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

    addFloat("maxOffsetFrames", maxOffset_, 1.0f, 120.0f);
    addInt("channel", channel_, 0, 4);
    addFloat("smoothness", smoothness_, 0.0f, 1.0f);
    return props;
}
void TimeWarpEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="maxOffsetFrames")setMaxOffsetFrames(v.toFloat());
    else if(k=="channel")setChannel(v.toInt());
    else if(k=="smoothness")setSmoothness(v.toFloat());
}
void TimeWarpEffect::syncImpls(){
    auto c=std::make_shared<TimeWarpCPUImpl>();
    c->maxOffset_=maxOffset_;c->smoothness_=smoothness_;c->channel_=channel_;setCPUImpl(c);
}
} // namespace Artifact
