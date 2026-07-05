module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Rasterizer.Glow;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class GlowCPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_=0.5f,radius_=20.0f,intensity_=1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height();
        const float th=std::clamp(threshold_,0.0f,1.0f),rad=std::max(radius_,1.0f);
        const float it=std::clamp(intensity_,0.0f,5.0f);
        cv::Mat srcM(H,W,CV_32FC4,(void*)sd);
        cv::Mat bright; srcM.copyTo(bright);
        for(int y=0;y<H;++y){auto* r=bright.ptr<cv::Vec4f>(y);for(int x=0;x<W;++x){
            float l=r[x][0]*0.299f+r[x][1]*0.587f+r[x][2]*0.114f;
            if(l<th)r[x]=cv::Vec4f(0,0,0,0);else{float m=(l-th)/(1.0f-th);r[x]*=m;}}}
        cv::Mat blur; int kr=std::min((int)rad,128);
        cv::GaussianBlur(bright,blur,cv::Size(kr*2+1,kr*2+1),rad*0.3f);
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        for(int y=0;y<H;++y){auto* br=blur.ptr<cv::Vec4f>(y);float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                p[0]=std::clamp(p[0]+br[x][0]*it,0.0f,1.0f);p[1]=std::clamp(p[1]+br[x][1]*it,0.0f,1.0f);
                p[2]=std::clamp(p[2]+br[x][2]*it,0.0f,1.0f);}}
    }
};

GlowEffect::GlowEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
GlowEffect::~GlowEffect()=default;
float GlowEffect::threshold()const{return threshold_;}void GlowEffect::setThreshold(float v){threshold_=std::clamp(v,0.0f,1.0f);syncImpls();}
float GlowEffect::radius()const{return radius_;}void GlowEffect::setRadius(float v){radius_=std::max(v,1.0f);syncImpls();}
float GlowEffect::intensity()const{return intensity_;}void GlowEffect::setIntensity(float v){intensity_=std::clamp(v,0.0f,5.0f);syncImpls();}
std::vector<AbstractProperty> GlowEffect::getProperties()const{
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

    addFloat("threshold", threshold_, 0.0f, 1.0f);
    addFloat("radius", radius_, 1.0f, 200.0f);
    addFloat("intensity", intensity_, 0.0f, 5.0f);
    return props;
}
void GlowEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="threshold")setThreshold(v.toFloat());else if(k=="radius")setRadius(v.toFloat());else if(k=="intensity")setIntensity(v.toFloat());}
void GlowEffect::syncImpls(){auto c=std::make_shared<GlowCPUImpl>();c->threshold_=threshold_;c->radius_=radius_;c->intensity_=intensity_;setCPUImpl(c);}
} // namespace Artifact
