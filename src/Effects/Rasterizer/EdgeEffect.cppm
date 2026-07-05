module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Edge;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class EdgeCPUImpl : public ArtifactEffectImplBase {
public:
    float mode_=0,intensity_=1,threshold_=0.1f,invert_=0;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height();
        const float it=std::clamp(intensity_,0.0f,10.0f);
        const float th=std::clamp(threshold_,0.0f,1.0f);
        const float inv=invert_>0.5f?1.0f:0.0f;

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();

        if(mode_>0.5f&&context_.sampler){
            // Motion edge: frame difference
            ImageF32x4RGBAWithCache prev;
            if(context_.sampler->sampleCurrentLayerFrameRelative(-1,prev)&&prev.width()>0&&prev.image().rgba32fData()){
                const float* pd=prev.image().rgba32fData();int pw=prev.width(),ph=prev.height();
                for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;int py=std::min(y,ph-1);
                    for(int x=0;x<W;++x){int px=std::min(x,pw-1);
                        const float* pp=pd+((size_t)py*pw+px)*4;float* p=o+(size_t)x*4;
                        float dr=fabsf(p[0]-pp[0]),dg=fabsf(p[1]-pp[1]),db=fabsf(p[2]-pp[2]);
                        float e=std::max({dr,dg,db})*it;
                        if(e<th)e=0;if(inv>0.5f)e=1.0f-e;
                        p[0]=e;p[1]=e;p[2]=e;p[3]=1.0f;
                    }
                }
            }
        }else{
            // Spatial Sobel edge
            for(int y=1;y<H-1;++y){float* o=d+(size_t)y*W*4;
                for(int x=1;x<W-1;++x){
                    auto L=[&](int ox,int oy)->float{const float*sp=sd+((size_t)(y+oy)*W+(x+ox))*4;return sp[0]*0.299f+sp[1]*0.587f+sp[2]*0.114f;};
                    float gx=-L(-1,-1)-2*L(-1,0)-L(-1,1)+L(1,-1)+2*L(1,0)+L(1,1);
                    float gy=-L(-1,-1)-2*L(0,-1)-L(1,-1)+L(-1,1)+2*L(0,1)+L(1,1);
                    float e=std::sqrt(gx*gx+gy*gy)*it;
                    if(e<th)e=0;if(inv>0.5f)e=1.0f-e;
                    float* p=o+(size_t)x*4;p[0]=e;p[1]=e;p[2]=e;p[3]=1.0f;
                }
            }
        }
    }
};

EdgeEffect::EdgeEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
EdgeEffect::~EdgeEffect()=default;
float EdgeEffect::mode()const{return mode_;}
void EdgeEffect::setMode(float v){mode_=std::clamp(v,0.0f,1.0f);syncImpls();}
float EdgeEffect::intensity()const{return intensity_;}
void EdgeEffect::setIntensity(float v){intensity_=std::clamp(v,0.0f,10.0f);syncImpls();}
float EdgeEffect::threshold()const{return threshold_;}
void EdgeEffect::setThreshold(float v){threshold_=std::clamp(v,0.0f,1.0f);syncImpls();}
float EdgeEffect::invert()const{return invert_;}
void EdgeEffect::setInvert(float v){invert_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> EdgeEffect::getProperties()const{
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

    addFloat("mode", mode_, 0.0f, 1.0f);
    addFloat("intensity", intensity_, 0.0f, 10.0f);
    addFloat("threshold", threshold_, 0.0f, 1.0f);
    addFloat("invert", invert_, 0.0f, 1.0f);
    return props;
}
void EdgeEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="mode")setMode(v.toFloat());else if(k=="intensity")setIntensity(v.toFloat());else if(k=="threshold")setThreshold(v.toFloat());else if(k=="invert")setInvert(v.toFloat());
}
void EdgeEffect::syncImpls(){
    auto c=std::make_shared<EdgeCPUImpl>();c->mode_=mode_;c->intensity_=intensity_;c->threshold_=threshold_;c->invert_=invert_;setCPUImpl(c);
}
} // namespace Artifact
