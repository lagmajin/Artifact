module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.SlitScan;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class SlitScanCPUImpl : public ArtifactEffectImplBase {
public:
    float direction_=0,speed_=2,persistence_=0.95f,position_=-1;
    std::vector<float> accum_; int aw_=0,ah_=0; float slitPos_=0;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height();
        const float p=std::clamp(persistence_,0.0f,1.0f),sp=std::max(speed_,0.0f);
        const size_t n=(size_t)W*H*4;
        if(aw_!=W||ah_!=H){accum_.assign(n,0);aw_=W;ah_=H;slitPos_=0;}

        float pos=position_>=0?position_:slitPos_;
        float maxDim=direction_<=0.5f?(float)W:(float)H;
        if(position_<0){slitPos_+=sp;if(slitPos_>=maxDim)slitPos_-=maxDim;}
        pos=std::fmod(pos,maxDim);if(pos<0)pos+=maxDim;

        // Apply persistence to accumulation
        for(size_t i=0;i<n;++i)accum_[i]*=p;

        // Write current frame's slit line into accumulation
        if(direction_<=0.5f){// horizontal
            int sx=std::clamp((int)pos,0,W-1);
            for(int y=0;y<H;++y){
                const float*sp=sd+((size_t)y*W+sx)*4;
                float*ap=accum_.data()+((size_t)y*W+sx)*4;
                ap[0]=sp[0];ap[1]=sp[1];ap[2]=sp[2];ap[3]=sp[3];
            }
        }else{// vertical
            int sy=std::clamp((int)pos,0,H-1);
            const float*sp=sd+((size_t)sy*W)*4;
            float*ap=accum_.data()+((size_t)sy*W)*4;
            for(int x=0;x<W;++x){
                ap[x*4]=sp[x*4];ap[x*4+1]=sp[x*4+1];ap[x*4+2]=sp[x*4+2];ap[x*4+3]=sp[x*4+3];
            }
        }

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        for(size_t i=0;i<n;++i)d[i]=std::clamp(accum_[i],0.0f,1.0f);
    }
};

SlitScanEffect::SlitScanEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
SlitScanEffect::~SlitScanEffect()=default;
float SlitScanEffect::direction()const{return direction_;}
void SlitScanEffect::setDirection(float v){direction_=std::clamp(v,0.0f,1.0f);syncImpls();}
float SlitScanEffect::speed()const{return speed_;}
void SlitScanEffect::setSpeed(float v){speed_=std::max(v,0.0f);syncImpls();}
float SlitScanEffect::persistence()const{return persistence_;}
void SlitScanEffect::setPersistence(float v){persistence_=std::clamp(v,0.0f,1.0f);syncImpls();}
float SlitScanEffect::position()const{return position_;}
void SlitScanEffect::setPosition(float v){position_=v;syncImpls();}
std::vector<AbstractProperty> SlitScanEffect::getProperties()const{
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

    addFloat("direction", direction_, 0.0f, 1.0f);
    addFloat("speed", speed_, 0.0f, 100.0f);
    addFloat("persistence", persistence_, 0.0f, 1.0f);
    addFloat("position", position_, -1.0f, 1.0f);
    return props;
}
void SlitScanEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="direction")setDirection(v.toFloat());
    else if(k=="speed")setSpeed(v.toFloat());
    else if(k=="persistence")setPersistence(v.toFloat());
    else if(k=="position")setPosition(v.toFloat());
}
void SlitScanEffect::syncImpls(){
    auto c=std::make_shared<SlitScanCPUImpl>();
    c->direction_=direction_;c->speed_=speed_;c->persistence_=persistence_;c->position_=position_;setCPUImpl(c);
}
} // namespace Artifact
