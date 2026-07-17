module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Bricks;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;

namespace Artifact {
using namespace ArtifactCore;

class BricksCPUImpl : public ArtifactEffectImplBase {
public:
    float bw_=64,bh_=32,mortar_=3,offset_=0.5f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image(); int W=si.width(),H=si.height();
        float w=std::max(bw_,8.0f),h=std::max(bh_,4.0f),mw=std::max(mortar_,0.0f),off=std::clamp(offset_,0.0f,1.0f);
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        Parallel::For(0,H,[&](int y){float* o=d+(size_t)y*W*4;
            int row=(int)std::floor((float)y/h);
            float rowOff=(row%2==1)?off*w:0.0f;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float bx=std::fmod((float)x-rowOff+w*1000.0f,w);
                float by=std::fmod((float)y,h);
                bool inMortar=bx<mw||bx>w-mw||by<mw||by>h-mw;
                p[0]=inMortar?0.0f:1.0f;p[1]=inMortar?0.0f:1.0f;
                p[2]=inMortar?0.0f:1.0f;p[3]=1.0f;
            }
        });
    }
};

BricksEffect::BricksEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
BricksEffect::~BricksEffect()=default;
float BricksEffect::brickWidth()const{return bw_;}void BricksEffect::setBrickWidth(float v){bw_=std::max(v,8.0f);syncImpls();}
float BricksEffect::brickHeight()const{return bh_;}void BricksEffect::setBrickHeight(float v){bh_=std::max(v,4.0f);syncImpls();}
float BricksEffect::mortarWidth()const{return mortar_;}void BricksEffect::setMortarWidth(float v){mortar_=std::max(v,0.0f);syncImpls();}
float BricksEffect::offset()const{return offset_;}void BricksEffect::setOffset(float v){offset_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> BricksEffect::getProperties()const{
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

    addFloat("brickWidth", bw_, 8.0f, 500.0f);
    addFloat("brickHeight", bh_, 4.0f, 500.0f);
    addFloat("mortarWidth", mortar_, 0.0f, 50.0f);
    addFloat("offset", offset_, 0.0f, 1.0f);
    return props;
}
void BricksEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="brickWidth")setBrickWidth(v.toFloat());else if(k=="brickHeight")setBrickHeight(v.toFloat());else if(k=="mortarWidth")setMortarWidth(v.toFloat());else if(k=="offset")setOffset(v.toFloat());}
void BricksEffect::syncImpls(){auto c=std::make_shared<BricksCPUImpl>();c->bw_=bw_;c->bh_=bh_;c->mortar_=mortar_;c->offset_=offset_;setCPUImpl(c);}
} // namespace Artifact
