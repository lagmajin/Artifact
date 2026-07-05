module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Glitch;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class GlitchCPUImpl : public ArtifactEffectImplBase {
public:
    float intensity_=0.3f,colorShift_=0.5f,scanlines_=0.5f; int seed_=0;
    mutable std::mt19937 rng_{42};

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height();
        const float it=std::clamp(intensity_,0.0f,1.0f),cs=std::clamp(colorShift_,0.0f,1.0f);
        const float sl=std::clamp(scanlines_,0.0f,1.0f);

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        rng_.seed((unsigned)(42+context_.compositionFrame+seed_));
        std::uniform_real_distribution<float> d01(0,1);

        // Scanline corruption
        if(sl>0.001f){for(int y=0;y<H;++y){if(d01(rng_)<sl*0.3f){
            int sx=std::clamp((int)(d01(rng_)*W),0,W-1),sw=std::clamp((int)(d01(rng_)*W*0.5f),1,W/4);
            float* o=d+(size_t)y*W*4;
            for(int i=0;i<sw;++i){int x=std::clamp(sx+i,0,W-1);float* p=o+(size_t)x*4;
                p[0]=d01(rng_);p[1]=d01(rng_);p[2]=d01(rng_);}
        }}}

        // Block displacement
        int BS=16;for(int by=0;by<(H+BS-1)/BS;++by)for(int bx=0;bx<(W+BS-1)/BS;++bx){
            if(d01(rng_)<it*0.3f){int sx=bx*BS,sy=by*BS,ex=std::min(sx+BS,W),ey=std::min(sy+BS,H);
                int dx=(int)((d01(rng_)-0.5f)*BS*2),dy=(int)((d01(rng_)-0.5f)*BS*2);
                int tx=std::clamp(sx+dx,0,W-(ex-sx)),ty=std::clamp(sy+dy,0,H-(ey-sy));
                for(int y=sy;y<ey;++y){float* o=d+(size_t)y*W*4;float* tgt=d+(size_t)(ty+y-sy)*W*4;
                    for(int x=sx;x<ex;++x){int ox=x-sx;float* p=o+(size_t)x*4;float* tp=tgt+(size_t)(tx+ox)*4;
                        p[0]=tp[0];p[1]=tp[1];p[2]=tp[2];}}
            }
        }

        // Color channel shift
        if(cs>0.001f){for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;
            for(int x=1;x<W-1;++x){if(d01(rng_)<cs*0.5f){float* p=o+(size_t)x*4;
                float tmp=p[0];p[0]=p[2];p[2]=tmp;
            }}
        }}
    }
};

GlitchEffect::GlitchEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
GlitchEffect::~GlitchEffect()=default;
float GlitchEffect::intensity()const{return intensity_;}void GlitchEffect::setIntensity(float v){intensity_=std::clamp(v,0.0f,1.0f);syncImpls();}
float GlitchEffect::colorShift()const{return colorShift_;}void GlitchEffect::setColorShift(float v){colorShift_=std::clamp(v,0.0f,1.0f);syncImpls();}
float GlitchEffect::scanlines()const{return scanlines_;}void GlitchEffect::setScanlines(float v){scanlines_=std::clamp(v,0.0f,1.0f);syncImpls();}
int GlitchEffect::seed()const{return seed_;}void GlitchEffect::setSeed(int v){seed_=v;syncImpls();}
std::vector<AbstractProperty> GlitchEffect::getProperties()const{
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

    addFloat("intensity", intensity_, 0.0f, 1.0f);
    addFloat("colorShift", colorShift_, 0.0f, 1.0f);
    addFloat("scanlines", scanlines_, 0.0f, 1.0f);
    addInt("seed", seed_, 0, 9999);
    return props;
}
void GlitchEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="intensity")setIntensity(v.toFloat());else if(k=="colorShift")setColorShift(v.toFloat());else if(k=="scanlines")setScanlines(v.toFloat());else if(k=="seed")setSeed(v.toInt());}
void GlitchEffect::syncImpls(){auto c=std::make_shared<GlitchCPUImpl>();c->intensity_=intensity_;c->colorShift_=colorShift_;c->scanlines_=scanlines_;c->seed_=seed_;setCPUImpl(c);}
} // namespace Artifact
