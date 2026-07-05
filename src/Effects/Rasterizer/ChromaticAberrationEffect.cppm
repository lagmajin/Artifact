module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.ChromaticAberration;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class ChromaticAberrationCPUImpl : public ArtifactEffectImplBase {
public:
    float redShift_=2.0f,blueShift_=2.0f,cx_=0.5f,cy_=0.5f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height();
        float rx=std::clamp(redShift_,0.0f,50.0f),bx=std::clamp(blueShift_,0.0f,50.0f);
        float cx=cx_*(float)W,cy=cy_*(float)H;

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float dx=(float)x-cx,dy=(float)y-cy,dist=std::sqrt(dx*dx+dy*dy);
                float nrm=dist/std::max((float)std::max(W,H)*0.7f,1.0f);
                float rs=nrm*rx,bs=nrm*bx;
                int rxi=std::clamp((int)((float)x+dx/dist*rs+0.5f),0,W-1);
                int ryi=std::clamp((int)((float)y+dy/dist*rs+0.5f),0,H-1);
                int bxi=std::clamp((int)((float)x+dx/dist*(-bs)+0.5f),0,W-1);
                int byi=std::clamp((int)((float)y+dy/dist*(-bs)+0.5f),0,H-1);
                float rc[4],bc[4];
                auto clp=[&](int ix,int iy,float* out){const float*sp=sd+((size_t)iy*W+ix)*4;out[0]=sp[0];out[1]=sp[1];out[2]=sp[2];out[3]=sp[3];};
                clp(rxi,ryi,rc);clp(bxi,byi,bc);
                p[0]=rc[0];p[2]=bc[2];
            }
        }
    }
};

ChromaticAberrationEffect::ChromaticAberrationEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
ChromaticAberrationEffect::~ChromaticAberrationEffect()=default;
float ChromaticAberrationEffect::redShift()const{return redShift_;}void ChromaticAberrationEffect::setRedShift(float v){redShift_=std::clamp(v,0.0f,50.0f);syncImpls();}
float ChromaticAberrationEffect::blueShift()const{return blueShift_;}void ChromaticAberrationEffect::setBlueShift(float v){blueShift_=std::clamp(v,0.0f,50.0f);syncImpls();}
float ChromaticAberrationEffect::centerX()const{return cx_;}void ChromaticAberrationEffect::setCenterX(float v){cx_=std::clamp(v,0.0f,1.0f);syncImpls();}
float ChromaticAberrationEffect::centerY()const{return cy_;}void ChromaticAberrationEffect::setCenterY(float v){cy_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> ChromaticAberrationEffect::getProperties()const{
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

    addFloat("redShift", redShift_, 0.0f, 50.0f);
    addFloat("blueShift", blueShift_, 0.0f, 50.0f);
    addFloat("centerX", cx_, 0.0f, 1.0f);
    addFloat("centerY", cy_, 0.0f, 1.0f);
    return props;
}
void ChromaticAberrationEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="redShift")setRedShift(v.toFloat());else if(k=="blueShift")setBlueShift(v.toFloat());else if(k=="centerX")setCenterX(v.toFloat());else if(k=="centerY")setCenterY(v.toFloat());}
void ChromaticAberrationEffect::syncImpls(){auto c=std::make_shared<ChromaticAberrationCPUImpl>();c->redShift_=redShift_;c->blueShift_=blueShift_;c->cx_=cx_;c->cy_=cy_;setCPUImpl(c);}
} // namespace Artifact
