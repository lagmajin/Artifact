module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Deflicker;

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

class DeflickerCPUImpl : public ArtifactEffectImplBase {
public:
    int windowSize_=16; float strength_=1.0f,lumaWeight_=1.0f;
    std::vector<double> lumaHistory_;
    double targetLuma_=0.5;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height(),ws=std::clamp(windowSize_,2,120);
        const float st=std::clamp(strength_,0.0f,1.0f),lw=std::clamp(lumaWeight_,0.0f,1.0f);

        // Compute mean luminance of current frame
        double totalL=0;size_t cnt=0;
        for(int y=0;y<H;y+=4)for(int x=0;x<W;x+=4){
            const float* p=sd+((size_t)y*W+x)*4;
            totalL+=p[0]*0.299+p[1]*0.587+p[2]*0.114;++cnt;
        }
        double curLuma=totalL/std::max(cnt,(size_t)1);

        // Rolling window
        lumaHistory_.push_back(curLuma);
        if((int)lumaHistory_.size()>ws)lumaHistory_.erase(lumaHistory_.begin());

        // Target = median of history
        std::vector<double> tmp=lumaHistory_;
        std::sort(tmp.begin(),tmp.end());
        targetLuma_=tmp[tmp.size()/2];

        // Correction factor
        double ratio=targetLuma_/std::max(curLuma,0.0001);
        ratio=1.0+(ratio-1.0)*st;
        float fac=(float)std::clamp(ratio,0.1,10.0);

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        ArtifactCore::Parallel::For(0,H,[&](int y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float adj=1.0f+(fac-1.0f)*lw;
                p[0]=std::clamp(p[0]*adj,0.0f,1.0f);p[1]=std::clamp(p[1]*adj,0.0f,1.0f);p[2]=std::clamp(p[2]*adj,0.0f,1.0f);
            }
        });
    }
};

DeflickerEffect::DeflickerEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
DeflickerEffect::~DeflickerEffect()=default;
int DeflickerEffect::windowSize()const{return windowSize_;}
void DeflickerEffect::setWindowSize(int v){windowSize_=std::clamp(v,2,120);syncImpls();}
float DeflickerEffect::strength()const{return strength_;}
void DeflickerEffect::setStrength(float v){strength_=std::clamp(v,0.0f,1.0f);syncImpls();}
float DeflickerEffect::lumaWeight()const{return lumaWeight_;}
void DeflickerEffect::setLumaWeight(float v){lumaWeight_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> DeflickerEffect::getProperties()const{
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

    addInt("windowSize", windowSize_, 2, 120);
    addFloat("strength", strength_, 0.0f, 1.0f);
    addFloat("lumaWeight", lumaWeight_, 0.0f, 1.0f);
    return props;
}
void DeflickerEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="windowSize")setWindowSize(v.toInt());else if(k=="strength")setStrength(v.toFloat());else if(k=="lumaWeight")setLumaWeight(v.toFloat());
}
void DeflickerEffect::syncImpls(){
    auto c=std::make_shared<DeflickerCPUImpl>();c->windowSize_=windowSize_;c->strength_=strength_;c->lumaWeight_=lumaWeight_;setCPUImpl(c);
}
} // namespace Artifact
