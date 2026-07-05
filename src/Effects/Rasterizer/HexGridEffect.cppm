module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.HexGrid;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class HexGridCPUImpl : public ArtifactEffectImplBase {
public:
    float cellSize_=32,lineWidth_=2,angle_=0;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image(); int W=si.width(),H=si.height();
        float cs=std::max(cellSize_,4.0f),lw=std::max(lineWidth_,0.5f);
        float rad=angle_*std::numbers::pi_v<float>/180.0f;
        float cr=std::cos(rad),sr=std::sin(rad);
        float hw=cs,hh=cs*0.8660254f; // sqrt(3)/2
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float rx=(float)x*cr-(float)y*sr,ry=(float)x*sr+(float)y*cr;
                // Convert to hex coordinates
                float q=rx/hw,r=ry/hh;
                float qf=q-std::floor(q),rf=r-std::floor(r);
                int qi=(int)std::floor(q),ri=(int)std::floor(r);
                float dx,dy;
                if((ri&1)==0){// even rows
                    dx=(qf-0.5f)*hw;dy=(rf-0.5f)*hh;
                }else{// odd rows offset
                    dx=(qf-0.0f)*hw;dy=(rf-0.5f)*hh;
                }
                float dist=std::max(std::abs(dx)/hw,std::abs(dy)/hh)*2.0f;
                float v=dist>1.0f-lw/cs?0.0f:1.0f;
                p[0]=v;p[1]=v;p[2]=v;p[3]=1.0f;
            }
        }
    }
};

HexGridEffect::HexGridEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
HexGridEffect::~HexGridEffect()=default;
float HexGridEffect::cellSize()const{return cellSize_;}void HexGridEffect::setCellSize(float v){cellSize_=std::max(v,4.0f);syncImpls();}
float HexGridEffect::lineWidth()const{return lineWidth_;}void HexGridEffect::setLineWidth(float v){lineWidth_=std::max(v,0.5f);syncImpls();}
float HexGridEffect::angle()const{return angle_;}void HexGridEffect::setAngle(float v){angle_=v;syncImpls();}
std::vector<AbstractProperty> HexGridEffect::getProperties()const{
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

    addFloat("cellSize", cellSize_, 4.0f, 200.0f);
    addFloat("lineWidth", lineWidth_, 0.5f, 20.0f);
    addFloat("angle", angle_, 0.0f, 360.0f);
    return props;
}
void HexGridEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="cellSize")setCellSize(v.toFloat());else if(k=="lineWidth")setLineWidth(v.toFloat());else if(k=="angle")setAngle(v.toFloat());}
void HexGridEffect::syncImpls(){auto c=std::make_shared<HexGridCPUImpl>();c->cellSize_=cellSize_;c->lineWidth_=lineWidth_;c->angle_=angle_;setCPUImpl(c);}
} // namespace Artifact
