module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.TrailFade;

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

class TrailFadeCPUImpl : public ArtifactEffectImplBase {
public:
    int trailLen_=8; float fadePower_=2.0f,vScale_=1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0||!context_.sampler){dst=src;return;}
        const int W=si.width(),H=si.height(),tl=std::clamp(trailLen_,1,32);
        const float fp=std::max(fadePower_,0.5f);

        ImageF32x4RGBAWithCache prev;bool hp=context_.sampler->sampleCurrentLayerFrameRelative(-1,prev)&&prev.width()>0&&prev.image().rgba32fData();
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        if(!hp)return;

        const float* pd=prev.image().rgba32fData();
        const int pw=prev.width(),ph=prev.height(),BS=8;
        const int vw=(W+BS-1)/BS,vh=(H+BS-1)/BS;
        std::vector<float> vx(vw*vh,0),vy(vw*vh,0);

        Parallel::For(0,vh,[&](int by){for(int bx=0;bx<vw;++bx){
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
            vx[by*vw+bx]=bdx*vScale_;vy[by*vw+bx]=bdy*vScale_;
        }});

        Parallel::For(0,H,[&](int y){int bvY=y/BS;float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){int bvX=x/BS;
                float mx=vx[std::min(bvY,vh-1)*vw+std::min(bvX,vw-1)];
                float my=vy[std::min(bvY,vh-1)*vw+std::min(bvX,vw-1)];
                float* p=o+(size_t)x*4;
                float r=p[0],g=p[1],b=p[2],a=p[3],totalW=1.0f;
                if(fabsf(mx)>0.5f||fabsf(my)>0.5f){
                    for(int i=1;i<=tl;++i){
                        float dist=(float)i/(float)tl;
                        float w=1.0f-std::pow(dist,fp);
                        if(w<0.001f)continue;
                        int sx=(int)((float)x-mx*(float)i+0.5f),sy=(int)((float)y-my*(float)i+0.5f);
                        sx=std::clamp(sx,0,W-1);sy=std::clamp(sy,0,H-1);
                        const float*sp=sd+((size_t)sy*W+sx)*4;
                        r+=sp[0]*w;g+=sp[1]*w;b+=sp[2]*w;a+=sp[3]*w;totalW+=w;
                    }
                    float inv=1.0f/totalW;
                    p[0]=std::clamp(r*inv,0.0f,1.0f);p[1]=std::clamp(g*inv,0.0f,1.0f);
                    p[2]=std::clamp(b*inv,0.0f,1.0f);p[3]=std::clamp(a*inv,0.0f,1.0f);
                }
            }
        });
    }
};

TrailFadeEffect::TrailFadeEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
TrailFadeEffect::~TrailFadeEffect()=default;
int TrailFadeEffect::trailLength()const{return trailLen_;}
void TrailFadeEffect::setTrailLength(int v){trailLen_=std::clamp(v,1,32);syncImpls();}
float TrailFadeEffect::fadePower()const{return fadePower_;}
void TrailFadeEffect::setFadePower(float v){fadePower_=std::max(v,0.5f);syncImpls();}
float TrailFadeEffect::velocityScale()const{return vScale_;}
void TrailFadeEffect::setVelocityScale(float v){vScale_=std::clamp(v,0.0f,16.0f);syncImpls();}
std::vector<AbstractProperty> TrailFadeEffect::getProperties()const{
    std::vector<AbstractProperty> props;
    props.reserve(3);

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

    addInt("trailLength", trailLen_, 1, 32);
    addFloat("fadePower", fadePower_, 0.5f, 5.0f);
    addFloat("velocityScale", vScale_, 0.0f, 16.0f);
    return props;
}
void TrailFadeEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="trailLength")setTrailLength(v.toInt());
    else if(k=="fadePower")setFadePower(v.toFloat());
    else if(k=="velocityScale")setVelocityScale(v.toFloat());
}
void TrailFadeEffect::syncImpls(){
    auto c=std::make_shared<TrailFadeCPUImpl>();
    c->trailLen_=trailLen_;c->fadePower_=fadePower_;c->vScale_=vScale_;setCPUImpl(c);
}
} // namespace Artifact
