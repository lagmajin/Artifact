module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.DifferenceMatte;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class DifferenceMatteCPUImpl : public ArtifactEffectImplBase {
public:
    int refOffset_=1; float threshold_=0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image(); const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0||!context_.sampler){dst=src;return;}
        const int W=si.width(),H=si.height();
        ImageF32x4RGBAWithCache ref;
        if(!context_.sampler->sampleCurrentLayerFrameRelative(
            static_cast<std::int64_t>(-refOffset_),ref)
           ||ref.width()<=0||!ref.image().rgba32fData()){dst=src;return;}

        dst=src.DeepCopy(); float* d=dst.image().rgba32fData();
        const float* rd=ref.image().rgba32fData();
        const int rw=ref.width(),rh=ref.height();
        const float th=std::clamp(threshold_,0.0f,1.0f);

        for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;
            int ry=std::min(y,rh-1);
            for(int x=0;x<W;++x){int rx=std::min(x,rw-1);
                const float* rp=rd+((size_t)ry*rw+rx)*4;
                float* p=o+(size_t)x*4;
                float dr=fabsf(p[0]-rp[0]),dg=fabsf(p[1]-rp[1]),db=fabsf(p[2]-rp[2]);
                float diff=std::max({dr,dg,db});
                if(th>0.0f)diff=diff>=th?1.0f:0.0f;
                p[0]=diff;p[1]=diff;p[2]=diff;p[3]=1.0f;
            }
        }
    }
};

DifferenceMatteEffect::DifferenceMatteEffect():ArtifactAbstractEffect(){
    setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();
}
DifferenceMatteEffect::~DifferenceMatteEffect()=default;
int DifferenceMatteEffect::referenceOffset()const{return refOffset_;}
void DifferenceMatteEffect::setReferenceOffset(int v){refOffset_=std::clamp(v,1,60);syncImpls();}
float DifferenceMatteEffect::threshold()const{return threshold_;}
void DifferenceMatteEffect::setThreshold(float v){threshold_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> DifferenceMatteEffect::getProperties()const{
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

    addInt("referenceOffset", refOffset_, 1, 60);
    addFloat("threshold", threshold_, 0.0f, 1.0f);
    return props;
}
void DifferenceMatteEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="referenceOffset")setReferenceOffset(v.toInt());
    else if(k=="threshold")setThreshold(v.toFloat());
}
void DifferenceMatteEffect::syncImpls(){
    auto c=std::make_shared<DifferenceMatteCPUImpl>();c->refOffset_=refOffset_;c->threshold_=threshold_;setCPUImpl(c);
}
} // namespace Artifact
