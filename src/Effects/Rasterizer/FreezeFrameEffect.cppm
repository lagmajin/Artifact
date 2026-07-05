module;
#include <algorithm>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.FreezeFrame;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class FreezeFrameCPUImpl : public ArtifactEffectImplBase {
public:
    bool frozen_ = false;
    ImageF32x4RGBAWithCache freezeFrame_;

    void freeze() { frozen_ = true; }
    void release() { frozen_ = false; freezeFrame_ = {}; }
    bool isFrozen() const { return frozen_; }

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        if (!frozen_) {
            freezeFrame_ = src.DeepCopy();
            dst = src.DeepCopy();
            return;
        }
        if (freezeFrame_.width() > 0 && freezeFrame_.image().rgba32fData()) {
            dst = freezeFrame_.DeepCopy();
        } else {
            dst = src.DeepCopy();
        }
    }
};

FreezeFrameEffect::FreezeFrameEffect():ArtifactAbstractEffect(){
    setPipelineStage(EffectPipelineStage::Rasterizer); syncImpls();
}
FreezeFrameEffect::~FreezeFrameEffect()=default;
void FreezeFrameEffect::freeze() {
    if (auto* c = dynamic_cast<FreezeFrameCPUImpl*>(cpuImpl().get())) c->freeze();
}
void FreezeFrameEffect::release() {
    if (auto* c = dynamic_cast<FreezeFrameCPUImpl*>(cpuImpl().get())) c->release();
}
bool FreezeFrameEffect::isFrozen() const {
    if (auto* c = dynamic_cast<FreezeFrameCPUImpl*>(cpuImpl().get())) return c->isFrozen();
    return false;
}
std::vector<AbstractProperty> FreezeFrameEffect::getProperties()const{
    return{};
}
void FreezeFrameEffect::setPropertyValue(const UniString&,const QVariant&){}
void FreezeFrameEffect::syncImpls(){
    auto c=std::make_shared<FreezeFrameCPUImpl>();setCPUImpl(c);
}
} // namespace Artifact
