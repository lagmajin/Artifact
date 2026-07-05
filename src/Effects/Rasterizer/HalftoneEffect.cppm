module;
#include <algorithm>
#include <cmath>
#include <numbers>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Halftone;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class HalftoneCPUImpl : public ArtifactEffectImplBase {
public:
    float dotSize_=8,angle_=45,contrast_=1;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        int W=si.width(),H=si.height();if(!sd||W<=0){dst=src;return;}
        float ds=std::max(dotSize_,1.0f),ct=std::clamp(contrast_,0.0f,3.0f);
        float rad=angle_*std::numbers::pi_v<float>/180.0f;
        float cs=std::cos(rad),sn=std::sin(rad);

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float rx=(float)x*cs-(float)y*sn,ry=(float)x*sn+(float)y*cs;
                float cx=std::round(rx/ds)*ds,cy=std::round(ry/ds)*ds;
                float dx=rx-cx,dy=ry-cy;
                float dist=std::sqrt(dx*dx+dy*dy)/(ds*0.5f);
                float luma=p[0]*0.299f+p[1]*0.587f+p[2]*0.114f;
                float sz=1.0f-luma*ct;sz=std::clamp(sz,0.0f,1.0f);
                float v=dist<=sz?0.0f:1.0f;
                p[0]=v;p[1]=v;p[2]=v;p[3]=1.0f;
            }
        }
    }
};

HalftoneEffect::HalftoneEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
HalftoneEffect::~HalftoneEffect()=default;
float HalftoneEffect::dotSize()const{return dotSize_;}void HalftoneEffect::setDotSize(float v){dotSize_=std::max(v,1.0f);syncImpls();}
float HalftoneEffect::angle()const{return angle_;}void HalftoneEffect::setAngle(float v){angle_=v;syncImpls();}
float HalftoneEffect::contrast()const{return contrast_;}void HalftoneEffect::setContrast(float v){contrast_=std::clamp(v,0.0f,3.0f);syncImpls();}
std::vector<AbstractProperty> HalftoneEffect::getProperties()const{
    std::vector<AbstractProperty> props;
    props.reserve(3);

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

    addFloat("dotSize", dotSize_, 1.0f, 100.0f);
    addFloat("angle", angle_, 0.0f, 360.0f);
    addFloat("contrast", contrast_, 0.0f, 3.0f);
    return props;
}
void HalftoneEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="dotSize")setDotSize(v.toFloat());else if(k=="angle")setAngle(v.toFloat());else if(k=="contrast")setContrast(v.toFloat());}
void HalftoneEffect::syncImpls(){auto c=std::make_shared<HalftoneCPUImpl>();c->dotSize_=dotSize_;c->angle_=angle_;c->contrast_=contrast_;setCPUImpl(c);}
} // namespace Artifact
