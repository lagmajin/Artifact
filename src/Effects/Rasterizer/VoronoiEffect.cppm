module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Voronoi;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

inline float hash11(float p){p*=0.1031f;p=fmodf(p*289.0f,289.0f);p=(p*34.0f+1.0f)*p;return fmodf(p,289.0f)/289.0f;}
inline float hash12(float x,float y){return hash11(x+hash11(y)*57.0f);}

class VoronoiCPUImpl : public ArtifactEffectImplBase {
public:
    float scale_=20,jitter_=1; int mode_=0,seed_=0;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image(); int W=si.width(),H=si.height();
        float sc=std::max(scale_,1.0f),jt=std::clamp(jitter_,0.0f,2.0f);
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        float so=(float)seed_*123.456f;

        for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float fx=(float)x/sc,fy=(float)y/sc;
                int cx=(int)std::floor(fx),cy=(int)std::floor(fy);
                float md=1e12f,md2=1e12f,cellVal=0;
                for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
                    int gx=cx+dx,gy=cy+dy;
                    float rn=hash12((float)gx+so,(float)gy-so);
                    float px=(float)gx+(rn-0.5f)*jt,py=(float)gy+(hash12((float)gy+so,(float)gx+so)-0.5f)*jt;
                    float d2=(fx-px)*(fx-px)+(fy-py)*(fy-py);
                    if(d2<md){md2=md;md=d2;cellVal=hash12((float)gx*3.7f+so,(float)gy*7.3f-so);}
                    else if(d2<md2)md2=d2;
                }
                float v;
                if(mode_==0)v=std::sqrt(md);
                else if(mode_==1)v=std::sqrt(md2)-std::sqrt(md);
                else if(mode_==2)v=std::sqrt(md2);
                else v=cellVal;
                v=std::clamp(v,0.0f,1.0f);
                p[0]=v;p[1]=v;p[2]=v;p[3]=1.0f;
            }
        }
    }
};

VoronoiEffect::VoronoiEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
VoronoiEffect::~VoronoiEffect()=default;
float VoronoiEffect::scale()const{return scale_;}void VoronoiEffect::setScale(float v){scale_=std::max(v,1.0f);syncImpls();}
float VoronoiEffect::jitter()const{return jitter_;}void VoronoiEffect::setJitter(float v){jitter_=std::clamp(v,0.0f,2.0f);syncImpls();}
int VoronoiEffect::mode()const{return mode_;}void VoronoiEffect::setMode(int v){mode_=std::clamp(v,0,3);syncImpls();}
int VoronoiEffect::seed()const{return seed_;}void VoronoiEffect::setSeed(int v){seed_=v;syncImpls();}
std::vector<AbstractProperty> VoronoiEffect::getProperties()const{
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

    addFloat("scale", scale_, 1.0f, 200.0f);
    addFloat("jitter", jitter_, 0.0f, 2.0f);
    addInt("mode", mode_, 0, 3);
    addInt("seed", seed_, 0, 9999);
    return props;
}
void VoronoiEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="scale")setScale(v.toFloat());else if(k=="jitter")setJitter(v.toFloat());else if(k=="mode")setMode(v.toInt());else if(k=="seed")setSeed(v.toInt());}
void VoronoiEffect::syncImpls(){auto c=std::make_shared<VoronoiCPUImpl>();c->scale_=scale_;c->jitter_=jitter_;c->mode_=mode_;c->seed_=seed_;setCPUImpl(c);}
} // namespace Artifact
