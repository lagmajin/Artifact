module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Ghost;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class GhostCPUImpl : public ArtifactEffectImplBase {
public:
    float opacity_ = 0.3f; int ghostCount_ = 3;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image(); const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0||!context_.sampler){dst=src;return;}
        const int W=si.width(),H=si.height();
        const float op=std::clamp(opacity_,0.0f,1.0f);
        const int gc=std::clamp(ghostCount_,1,8);
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();

        for(int i=1;i<=gc;++i){
            float w=op*std::pow(0.7f,(float)(i-1));
            if(w<0.001f)continue;
            ImageF32x4RGBAWithCache gf;
            if(!context_.sampler->sampleCurrentLayerFrameRelative((std::int64_t)(-i),gf)
               ||gf.width()<=0||!gf.image().rgba32fData())continue;
            const float* gd=gf.image().rgba32fData();
            const int gw=gf.width(),gh=gf.height();
            for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;
                int gy=std::min(y,gh-1);
                for(int x=0;x<W;++x){int gx=std::min(x,gw-1);
                    const float* gp=gd+((size_t)gy*gw+gx)*4;
                    float* p=o+(size_t)x*4;
                    p[3]=std::min(p[3]+gp[3]*w,1.0f);
                }
            }
        }
    }
};

GhostEffect::GhostEffect():ArtifactAbstractEffect(){
    setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();
}
GhostEffect::~GhostEffect()=default;
float GhostEffect::opacity()const{return opacity_;}
void GhostEffect::setOpacity(float v){opacity_=std::clamp(v,0.0f,1.0f);syncImpls();}
int GhostEffect::ghostCount()const{return ghostCount_;}
void GhostEffect::setGhostCount(int v){ghostCount_=std::clamp(v,1,8);syncImpls();}
std::vector<AbstractProperty> GhostEffect::getProperties()const{
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

    addFloat("opacity", opacity_, 0.0f, 1.0f);
    addInt("ghostCount", ghostCount_, 1, 8);
    return props;
}
void GhostEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="opacity")setOpacity(v.toFloat());
    else if(k=="ghostCount")setGhostCount(v.toInt());
}
void GhostEffect::syncImpls(){
    auto c=std::make_shared<GhostCPUImpl>();
    c->opacity_=opacity_;c->ghostCount_=ghostCount_;setCPUImpl(c);
}
} // namespace Artifact
