module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <vector>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.DataMosh;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class DataMoshCPUImpl : public ArtifactEffectImplBase {
public:
    float intensity_=0.5f; int holdFrames_=4,blockSize_=16; float blend_=0.8f;

    struct MoshedBlock{int bx,by,holdLeft;ImageF32x4RGBAWithCache data;};
    std::vector<MoshedBlock> blocks_;
    int lastW_=0,lastH_=0;
    std::mt19937 rng_{42};

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height(),BS=std::clamp(blockSize_,4,64);
        const int BW=(W+BS-1)/BS,BH=(H+BS-1)/BS;
        const float it=std::clamp(intensity_,0.0f,1.0f);
        const float bl=std::clamp(blend_,0.0f,1.0f);

        if(lastW_!=W||lastH_!=H){blocks_.clear();lastW_=W;lastH_=H;}

        // Age existing moshed blocks
        for(size_t i=0;i<blocks_.size();){
            blocks_[i].holdLeft--;
            if(blocks_[i].holdLeft<=0)blocks_.erase(blocks_.begin()+i);
            else ++i;
        }

        // Randomly create new moshed blocks
        std::uniform_real_distribution<float> dist01(0,1);
        for(int by=0;by<BH;++by)for(int bx=0;bx<BW;++bx){
            if(dist01(rng_)<it){
                int sx=bx*BS,sy=by*BS,ex=std::min(sx+BS,W),ey=std::min(sy+BS,H);
                ImageF32x4_RGBA blockImg;
                blockImg.resize(ex-sx, ey-sy);
                float* bd=blockImg.rgba32fData();
                for(int y=sy;y<ey;++y)for(int x=sx;x<ex;++x){
                    const float*sp=sd+((size_t)y*W+x)*4;
                    size_t off=((size_t)(y-sy)*(ex-sx)+(x-sx))*4;
                    bd[off]=sp[0];bd[off+1]=sp[1];bd[off+2]=sp[2];bd[off+3]=sp[3];
                }
                blocks_.push_back({bx,by,std::max(1,holdFrames_),ImageF32x4RGBAWithCache(blockImg)});
            }
        }

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        for(auto& mb:blocks_){
            int sx=mb.bx*BS,sy=mb.by*BS;
            int ex=std::min(sx+BS,W),ey=std::min(sy+BS,H);
            if(!mb.data.image().rgba32fData())continue;
            const float* md=mb.data.image().rgba32fData();
            int mW=mb.data.width(),mH=mb.data.height();
            for(int y=sy;y<ey;++y){float* o=d+(size_t)y*W*4;
                for(int x=sx;x<ex;++x){
                    int mx=x-sx,my=y-sy;
                    if(mx<mW&&my<mH){
                        const float* mp=md+((size_t)my*mW+mx)*4;
                        float* p=o+(size_t)x*4;
                        p[0]=p[0]*(1-bl)+mp[0]*bl;p[1]=p[1]*(1-bl)+mp[1]*bl;
                        p[2]=p[2]*(1-bl)+mp[2]*bl;p[3]=p[3]*(1-bl)+mp[3]*bl;
                    }
                }
            }
        }
    }
};

DataMoshEffect::DataMoshEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
DataMoshEffect::~DataMoshEffect()=default;
float DataMoshEffect::intensity()const{return intensity_;}
void DataMoshEffect::setIntensity(float v){intensity_=std::clamp(v,0.0f,1.0f);syncImpls();}
int DataMoshEffect::holdFrames()const{return holdFrames_;}
void DataMoshEffect::setHoldFrames(int v){holdFrames_=std::clamp(v,1,120);syncImpls();}
int DataMoshEffect::blockSize()const{return blockSize_;}
void DataMoshEffect::setBlockSize(int v){blockSize_=std::clamp(v,4,64);syncImpls();}
float DataMoshEffect::blend()const{return blend_;}
void DataMoshEffect::setBlend(float v){blend_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> DataMoshEffect::getProperties()const{
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
    addInt("holdFrames", holdFrames_, 1, 120);
    addInt("blockSize", blockSize_, 4, 64);
    addFloat("blend", blend_, 0.0f, 1.0f);
    return props;
}
void DataMoshEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="intensity")setIntensity(v.toFloat());
    else if(k=="holdFrames")setHoldFrames(v.toInt());
    else if(k=="blockSize")setBlockSize(v.toInt());
    else if(k=="blend")setBlend(v.toFloat());
}
void DataMoshEffect::syncImpls(){
    auto c=std::make_shared<DataMoshCPUImpl>();
    c->intensity_=intensity_;c->holdFrames_=holdFrames_;c->blockSize_=blockSize_;c->blend_=blend_;setCPUImpl(c);
}
} // namespace Artifact
