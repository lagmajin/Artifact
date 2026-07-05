module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.PosterizeTime;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class PosterizeTimeCPUImpl : public ArtifactEffectImplBase {
public:
    float frameRate_ = 12.0f;
    std::int64_t lastUpdateFrame_ = -1;
    ImageF32x4RGBAWithCache held_;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const float fps = std::max(frameRate_, 1.0f);
        const double srcFps = context_.frameRate > 0 ? context_.frameRate : 30.0;
        const std::int64_t holdFrames = (std::int64_t)std::max(1.0, srcFps / fps);

        if (lastUpdateFrame_ < 0 ||
            context_.compositionFrame >= lastUpdateFrame_ + holdFrames) {
            held_ = src.DeepCopy();
            lastUpdateFrame_ = context_.compositionFrame;
        }

        if (held_.width() > 0 && held_.image().rgba32fData()) {
            dst = held_.DeepCopy();
        } else {
            dst = src.DeepCopy();
        }
    }
};

PosterizeTimeEffect::PosterizeTimeEffect():ArtifactAbstractEffect(){
    setPipelineStage(EffectPipelineStage::Rasterizer); syncImpls();
}
PosterizeTimeEffect::~PosterizeTimeEffect()=default;
float PosterizeTimeEffect::frameRate()const{return frameRate_;}
void PosterizeTimeEffect::setFrameRate(float v){frameRate_=std::max(v,1.0f);syncImpls();}
std::vector<AbstractProperty> PosterizeTimeEffect::getProperties()const{
    std::vector<AbstractProperty> props;
    props.reserve(1);

    AbstractProperty prop;
    prop.setName(QString::fromLatin1("frameRate"));
    prop.setType(PropertyType::Float);
    const QVariant variantValue(static_cast<double>(frameRate_));
    prop.setValue(variantValue);
    prop.setDefaultValue(variantValue);
    prop.setMinValue(QVariant(1.0));
    prop.setMaxValue(QVariant(120.0));
    props.push_back(std::move(prop));

    return props;
}
void PosterizeTimeEffect::setPropertyValue(const UniString& n,const QVariant& v){
    if(n.toQString()=="frameRate")setFrameRate(v.toFloat());
}
void PosterizeTimeEffect::syncImpls(){
    auto c=std::make_shared<PosterizeTimeCPUImpl>();c->frameRate_=frameRate_;setCPUImpl(c);
}
} // namespace Artifact
