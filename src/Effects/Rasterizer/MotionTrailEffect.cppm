module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.MotionTrail;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;

namespace Artifact {
using namespace ArtifactCore;

class MotionTrailCPUImpl : public ArtifactEffectImplBase {
public:
    int trailLength_=6; float decay_=0.6f,velocityScale_=1.0f,blendMode_=0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image(); const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height();
        const int tl=std::clamp(trailLength_,1,16);
        const float dec=std::clamp(decay_,0.0f,1.0f),vsc=std::clamp(velocityScale_,0.0f,16.0f);
        const float bm=std::clamp(blendMode_,0.0f,1.0f);
        if(!context_.sampler){dst=src.DeepCopy();return;}

        ImageF32x4RGBAWithCache prev;
        bool havePrev=context_.sampler->sampleCurrentLayerFrameRelative(-1,prev)
                     &&prev.width()>0&&prev.image().rgba32fData();
        dst=src.DeepCopy(); float* d=dst.image().rgba32fData();
        if(!havePrev)return;

        const float* pd=prev.image().rgba32fData();
        const int pw=prev.width(),ph=prev.height(),BS=8;
        const int vw=(W+BS-1)/BS,vh=(H+BS-1)/BS;
        std::vector<float> vx(vw*vh,0),vy(vw*vh,0);

        ArtifactCore::Parallel::For(0,vh,[&](int by){for(int bx=0;bx<vw;++bx){
            int sx=bx*BS,sy=by*BS,ex=std::min(sx+BS,W),ey=std::min(sy+BS,H);
            float best=1e12f,bdx=0,bdy=0;
            for(int dy=-16;dy<=16;dy+=4)for(int dx=-16;dx<=16;dx+=4){
                float diff=0; int cnt=0;
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
            vx[by*vw+bx]=bdx*vsc;vy[by*vw+bx]=bdy*vsc;
        }});

        ArtifactCore::Parallel::For(0,H,[&](int y){int bvY=y/BS;float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){int bvX=x/BS;
                float mx=vx[std::min(bvY,vh-1)*vw+std::min(bvX,vw-1)];
                float my=vy[std::min(bvY,vh-1)*vw+std::min(bvX,vw-1)];
                float* p=o+(size_t)x*4;
                float r=p[0],g=p[1],b=p[2],a=p[3],totalW=1.0f;
                if(fabsf(mx)>0.5f||fabsf(my)>0.5f){float w=1.0f;
                    for(int i=1;i<=tl;++i){w*=dec;if(w<0.001f)break;
                        int sx=(int)((float)x-mx*(float)i+0.5f),sy=(int)((float)y-my*(float)i+0.5f);
                        sx=std::clamp(sx,0,W-1);sy=std::clamp(sy,0,H-1);
                        const float*sp=sd+((size_t)sy*W+sx)*4;
                        if(bm<=0.0001f){r+=sp[0]*w;g+=sp[1]*w;b+=sp[2]*w;a+=sp[3]*w;totalW+=w;}
                        else{float t=w*bm;r=r*(1-t)+sp[0]*t;g=g*(1-t)+sp[1]*t;b=b*(1-t)+sp[2]*t;a=a*(1-t)+sp[3]*t;}
                    }
                }
                if(bm<=0.0001f&&totalW>1.0f){float inv=1.0f/totalW;
                    p[0]=std::clamp(r*inv,0.0f,1.0f);p[1]=std::clamp(g*inv,0.0f,1.0f);
                    p[2]=std::clamp(b*inv,0.0f,1.0f);p[3]=std::clamp(a*inv,0.0f,1.0f);
                }else if(bm>0.0001f){p[0]=r;p[1]=g;p[2]=b;p[3]=a;}
            }
        });
    }
};

MotionTrailEffect::MotionTrailEffect():ArtifactAbstractEffect(){
    setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();
}
MotionTrailEffect::~MotionTrailEffect()=default;
int MotionTrailEffect::trailLength()const{return trailLength_;}
void MotionTrailEffect::setTrailLength(int v){trailLength_=std::clamp(v,1,16);syncImpls();}
float MotionTrailEffect::decay()const{return decay_;}
void MotionTrailEffect::setDecay(float v){decay_=std::clamp(v,0.0f,1.0f);syncImpls();}
float MotionTrailEffect::velocityScale()const{return velocityScale_;}
void MotionTrailEffect::setVelocityScale(float v){velocityScale_=std::clamp(v,0.0f,16.0f);syncImpls();}
float MotionTrailEffect::blendMode()const{return blendMode_;}
void MotionTrailEffect::setBlendMode(float v){blendMode_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> MotionTrailEffect::getProperties()const{
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

    addInt("trailLength", trailLength_, 1, 16);
    addFloat("decay", decay_, 0.0f, 1.0f);
    addFloat("velocityScale", velocityScale_, 0.0f, 16.0f);
    addFloat("blendMode", blendMode_, 0.0f, 1.0f);
    return props;
}
void MotionTrailEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="trailLength")setTrailLength(v.toInt());
    else if(k=="decay")setDecay(v.toFloat());
    else if(k=="velocityScale")setVelocityScale(v.toFloat());
    else if(k=="blendMode")setBlendMode(v.toFloat());
}
void MotionTrailEffect::syncImpls(){
    auto c=std::make_shared<MotionTrailCPUImpl>();
    c->trailLength_=trailLength_;c->decay_=decay_;c->velocityScale_=velocityScale_;c->blendMode_=blendMode_;
    setCPUImpl(c);
}
} // namespace Artifact
