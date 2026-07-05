module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Kaleidoscope;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class KaleidoscopeCPUImpl : public ArtifactEffectImplBase {
public:
    int segments_=6; float rotation_=0,cx_=0.5f,cy_=0.5f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height(),seg=std::clamp(segments_,2,32);
        float cx=cx_*(float)W,cy=cy_*(float)H;
        float rot=rotation_*std::numbers::pi_v<float>/180.0f;
        float segAng=std::numbers::pi_v<float>*2.0f/(float)seg;

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float dx=(float)x-cx,dy=(float)y-cy;
                float ang=std::atan2(dy,dx)+rot;
                float segIdx=ang/segAng;
                int si=(int)segIdx;
                float frac=segIdx-(float)si;
                if(si%2==1)frac=1.0f-frac;
                float mang=(float)si*segAng+(si%2==1?1.0f-frac:frac)*segAng-rot;
                float dist=std::sqrt(dx*dx+dy*dy);
                int sx=(int)(cx+std::cos(mang)*dist+0.5f);
                int sy=(int)(cy+std::sin(mang)*dist+0.5f);
                sx=std::clamp(sx,0,W-1);sy=std::clamp(sy,0,H-1);
                const float*sp=sd+((size_t)sy*W+sx)*4;
                p[0]=sp[0];p[1]=sp[1];p[2]=sp[2];p[3]=sp[3];
            }
        }
    }
};

KaleidoscopeEffect::KaleidoscopeEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
KaleidoscopeEffect::~KaleidoscopeEffect()=default;
int KaleidoscopeEffect::segments()const{return segments_;}void KaleidoscopeEffect::setSegments(int v){segments_=std::clamp(v,2,32);syncImpls();}
float KaleidoscopeEffect::rotation()const{return rotation_;}void KaleidoscopeEffect::setRotation(float v){rotation_=v;syncImpls();}
float KaleidoscopeEffect::centerX()const{return cx_;}void KaleidoscopeEffect::setCenterX(float v){cx_=std::clamp(v,0.0f,1.0f);syncImpls();}
float KaleidoscopeEffect::centerY()const{return cy_;}void KaleidoscopeEffect::setCenterY(float v){cy_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> KaleidoscopeEffect::getProperties()const{
    std::vector<AbstractProperty> props;
    props.reserve(4);

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

    addInt("segments", segments_, 2, 32);
    addFloat("rotation", rotation_, -180.0f, 180.0f);
    addFloat("centerX", cx_, 0.0f, 1.0f);
    addFloat("centerY", cy_, 0.0f, 1.0f);
    return props;
}
void KaleidoscopeEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="segments")setSegments(v.toInt());else if(k=="rotation")setRotation(v.toFloat());else if(k=="centerX")setCenterX(v.toFloat());else if(k=="centerY")setCenterY(v.toFloat());}
void KaleidoscopeEffect::syncImpls(){auto c=std::make_shared<KaleidoscopeCPUImpl>();c->segments_=segments_;c->rotation_=rotation_;c->cx_=cx_;c->cy_=cy_;setCPUImpl(c);}
} // namespace Artifact
