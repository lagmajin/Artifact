module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.PixelSort;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {
using namespace ArtifactCore;

class PixelSortCPUImpl : public ArtifactEffectImplBase {
public:
    int sortLen_=16; float sortKey_=0,sortOrder_=1,blend_=0.5f;

    static float keyVal(const float* p,float k){
        if(k<=0.5f)return p[0]*0.299f+p[1]*0.587f+p[2]*0.114f; // luma
        else{float hue=std::atan2(p[1]-p[2],p[0]-p[1]);return hue;} // approximate hue
    }

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0||!context_.sampler){dst=src;return;}
        const int W=si.width(),H=si.height(),sl=std::clamp(sortLen_,2,64);
        const float b=std::clamp(blend_,0.0f,1.0f);

        ImageF32x4RGBAWithCache prev;bool hp=context_.sampler->sampleCurrentLayerFrameRelative(-1,prev)&&prev.width()>0&&prev.image().rgba32fData();
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        if(!hp)return;

        const float* pd=prev.image().rgba32fData();
        const int pw=prev.width(),ph=prev.height(),BS=8;
        const int vw=(W+BS-1)/BS,vh=(H+BS-1)/BS;
        std::vector<float> vx(vw*vh,0),vy(vw*vh,0);

        for(int by=0;by<vh;++by)for(int bx=0;bx<vw;++bx){
            int sx=bx*BS,sy=by*BS,ex=std::min(sx+BS,W),ey=std::min(sy+BS,H);
            float best=1e12f,bdx=0,bdy=0;
            for(int dy=-16;dy<=16;dy+=4)for(int dx=-16;dx<=16;dx+=4){
                float diff=0;int cnt=0;
                for(int y=sy;y<ey;y+=2)for(int x=sx;x<ex;x+=2){
                    int cx=x+dx,cy=y+dy;
                    if((unsigned)cx<(unsigned)pw&&(unsigned)cy<(unsigned)ph){
                        auto*sp=sd+((size_t)y*W+x)*4,*pp=pd+((size_t)cy*pw+cx)*4;
                        float dr=sp[0]-pp[0],dg=sp[1]-pp[1],db=sp[2]-pp[2];
                        diff+=dr*dr+dg*dg+db*db;++cnt;
                    }
                }
                if(cnt>0){diff/=(float)cnt;if(diff<best){best=diff;bdx=(float)(-dx);bdy=(float)(-dy);}}
            }
            vx[by*vw+bx]=bdx;vy[by*vw+bx]=bdy;
        }

        for(int y=0;y<H;++y){int bvY=y/BS;float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){int bvX=x/BS;
                float mx=vx[std::min(bvY,vh-1)*vw+std::min(bvX,vw-1)];
                float my=vy[std::min(bvY,vh-1)*vw+std::min(bvX,vw-1)];
                float mag=std::sqrt(mx*mx+my*my);
                if(mag<1.0f)continue;
                float dx=mx/mag,dy=my/mag;
                // Gather pixels along motion direction
                struct Sample{float val;float r,g,b,a;};
                std::vector<Sample> samples;
                for(int i=-sl;i<=sl;++i){
                    int sx=(int)((float)x+dx*(float)i+0.5f),sy=(int)((float)y+dy*(float)i+0.5f);
                    sx=std::clamp(sx,0,W-1);sy=std::clamp(sy,0,H-1);
                    const float*sp=sd+((size_t)sy*W+sx)*4;
                    samples.push_back({keyVal(sp,sortKey_),sp[0],sp[1],sp[2],sp[3]});
                }
                bool asc=sortOrder_<=0.5f;
                std::sort(samples.begin(),samples.end(),[asc](const Sample& a,const Sample& b){return asc?a.val<b.val:a.val>b.val;});

                float* p=o+(size_t)x*4;
                if(samples.size()>0){
                    auto& mid=samples[samples.size()/2];
                    p[0]=p[0]*(1-b)+mid.r*b;p[1]=p[1]*(1-b)+mid.g*b;
                    p[2]=p[2]*(1-b)+mid.b*b;p[3]=p[3]*(1-b)+mid.a*b;
                }
            }
        }
    }
};

PixelSortEffect::PixelSortEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
PixelSortEffect::~PixelSortEffect()=default;
int PixelSortEffect::sortLength()const{return sortLen_;}
void PixelSortEffect::setSortLength(int v){sortLen_=std::clamp(v,2,64);syncImpls();}
float PixelSortEffect::sortKey()const{return sortKey_;}
void PixelSortEffect::setSortKey(float v){sortKey_=std::clamp(v,0.0f,1.0f);syncImpls();}
float PixelSortEffect::sortOrder()const{return sortOrder_;}
void PixelSortEffect::setSortOrder(float v){sortOrder_=std::clamp(v,0.0f,1.0f);syncImpls();}
float PixelSortEffect::blend()const{return blend_;}
void PixelSortEffect::setBlend(float v){blend_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> PixelSortEffect::getProperties()const{
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

    addInt("sortLength", sortLen_, 2, 64);
    addFloat("sortKey", sortKey_, 0.0f, 1.0f);
    addFloat("sortOrder", sortOrder_, 0.0f, 1.0f);
    addFloat("blend", blend_, 0.0f, 1.0f);
    return props;
}
void PixelSortEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="sortLength")setSortLength(v.toInt());
    else if(k=="sortKey")setSortKey(v.toFloat());
    else if(k=="sortOrder")setSortOrder(v.toFloat());
    else if(k=="blend")setBlend(v.toFloat());
}
void PixelSortEffect::syncImpls(){
    auto c=std::make_shared<PixelSortCPUImpl>();
    c->sortLen_=sortLen_;c->sortKey_=sortKey_;c->sortOrder_=sortOrder_;c->blend_=blend_;setCPUImpl(c);
}
} // namespace Artifact
