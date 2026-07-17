module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/TextureView.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Rasterizer.TemporalSmear;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;

namespace Artifact {
using namespace ArtifactCore;

namespace {
bool tsUpload(const ImageF32x4RGBAWithCache& src,Diligent::IRenderDevice* d,Diligent::IDeviceContext* c,Diligent::ITexture** out){
    if(!d||!out)return false;const auto& si=src.image();int W=si.width(),H=si.height();
    if(W<=0||H<=0||!si.rgba32fData())return false;
    Diligent::TextureDesc td;td.Name="TSmear/Src";td.Type=Diligent::RESOURCE_DIM_TEX_2D;td.Width=W;td.Height=H;td.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;td.MipLevels=1;td.ArraySize=1;td.SampleCount=1;td.Usage=Diligent::USAGE_DEFAULT;td.BindFlags=Diligent::BIND_SHADER_RESOURCE;
    Diligent::RefCntAutoPtr<Diligent::ITexture> t;d->CreateTexture(td,nullptr,&t);if(!t)return false;
    Diligent::TextureSubResData sub;sub.pData=si.rgba32fData();sub.Stride=W*16;
    if(!c)return false;
    c->UpdateTexture(t,0,0,Diligent::Box{0,0,0,(Diligent::Uint32)W,(Diligent::Uint32)H,1},sub,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);*out=t.Detach();return true;}

bool tsReadback(Diligent::IRenderDevice*d,Diligent::IDeviceContext*c,Diligent::ITexture*s,ImageF32x4RGBAWithCache&dst){
    if(!d||!c||!s)return false;auto sd=s->GetDesc();
    Diligent::TextureDesc st;st.Type=Diligent::RESOURCE_DIM_TEX_2D;st.Width=sd.Width;st.Height=sd.Height;st.Format=sd.Format;st.ArraySize=1;st.MipLevels=1;st.SampleCount=1;st.Usage=Diligent::USAGE_STAGING;st.CPUAccessFlags=Diligent::CPU_ACCESS_READ;st.Name="TSmear/Stg";
    Diligent::RefCntAutoPtr<Diligent::ITexture> sg;d->CreateTexture(st,nullptr,&sg);if(!sg)return false;
    Diligent::CopyTextureAttribs cp(s,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,sg,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);c->CopyTexture(cp);c->Flush();c->WaitForIdle();
    Diligent::MappedTextureSubresource m{};c->MapTextureSubresource(sg,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,m);if(!m.pData||m.Stride==0)return false;
    cv::Mat tmp((int)sd.Height,(int)sd.Width,CV_32FC4,m.pData,m.Stride);dst.image().setFromCVMat(tmp);c->UnmapTextureSubresource(sg,0,0);return true;}
}

class TemporalSmearCPUImpl : public ArtifactEffectImplBase {
public:
    float smearAmount_=0.5f; int sampleCount_=8; float sampleJitter_=0.3f,velocityScale_=1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image(); const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0||si.height()<=0){dst=src;return;}
        const int W=si.width(),H=si.height(); const float amt=std::clamp(smearAmount_,0.0f,1.0f);
        const int ns=std::clamp(sampleCount_,2,32); const float jit=std::clamp(sampleJitter_,0.0f,1.0f);
        const float vsc=std::clamp(velocityScale_,0.0f,16.0f);
        if(amt<=0.001f||!context_.sampler){dst=src.DeepCopy();return;}
        ImageF32x4RGBAWithCache prev;
        bool ok=context_.sampler->sampleCurrentLayerFrameRelative(-1,prev);
        dst=src.DeepCopy(); float* d=dst.image().rgba32fData();
        if(!ok||prev.width()<=0||!prev.image().rgba32fData())return;
        const float* pd=prev.image().rgba32fData(); const int pw=prev.width(),ph=prev.height();
        const int BS=8, vw=(W+BS-1)/BS, vh=(H+BS-1)/BS;
        std::vector<float> vx(vw*vh,0),vy(vw*vh,0);
        ArtifactCore::Parallel::For(0,vh,[&](int by){for(int bx=0;bx<vw;++bx){int sx=bx*BS,sy=by*BS,ex=std::min(sx+BS,W),ey=std::min(sy+BS,H);float best=1e12f,bdx=0,bdy=0;
            for(int dy=-16;dy<=16;dy+=4)for(int dx=-16;dx<=16;dx+=4){float diff=0; int cnt=0;
                for(int y=sy;y<ey;y+=2)for(int x=sx;x<ex;x+=2){int cx=x+dx,cy=y+dy;
                    if((unsigned)cx<(unsigned)pw&&(unsigned)cy<(unsigned)ph){auto*sp=sd+((size_t)y*W+x)*4,*pp=pd+((size_t)cy*pw+cx)*4;
                        float dr=sp[0]-pp[0],dg=sp[1]-pp[1],db=sp[2]-pp[2];diff+=dr*dr+dg*dg+db*db;++cnt;}}
                if(cnt>0){diff/=(float)cnt;if(diff<best){best=diff;bdx=(float)(-dx);bdy=(float)(-dy);}}}
            vx[by*vw+bx]=bdx*vsc; vy[by*vw+bx]=bdy*vsc;}}
        ArtifactCore::Parallel::For(0,H,[&](int y){int bvY=y/BS;float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){int bvX=x/BS;
                float mx=vx[std::min(bvY,vh-1)*vw+std::min(bvX,vw-1)],my=vy[std::min(bvY,vh-1)*vw+std::min(bvX,vw-1)];
                float* p=o+(size_t)x*4;float r=p[0],g=p[1],b=p[2],a=p[3];
                if(amt>0.001f&&(fabsf(mx)>0.5f||fabsf(my)>0.5f)){uint seed=(uint)x*0x1f1f1f1fu+(uint)y*0x2d7b2d7bu;
                    for(int i=1;i<=ns;++i){float t=(float)i/(float)ns*amt;
                        if(jit>0){seed=seed*1103515245u+12345u;float j=(float)(seed&0x7fffffffu)/(float)0x80000000u;j=(j-0.5f)*2.0f*jit*0.5f;t=std::clamp(t+j,0.0f,1.0f);}
                        int sx=(int)((float)x+mx*t+0.5f),sy=(int)((float)y+my*t+0.5f);sx=std::clamp(sx,0,W-1);sy=std::clamp(sy,0,H-1);
                        auto*sp=sd+((size_t)sy*W+sx)*4;r+=sp[0];g+=sp[1];b+=sp[2];a+=sp[3];}
                    float inv=1.0f/(float)(ns+1);p[0]=r*inv;p[1]=g*inv;p[2]=b*inv;p[3]=a*inv;}}});
    }
};

class TemporalSmearGPUImpl : public ArtifactEffectImplBase {
public:
    TemporalSmearCPUImpl cpu;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> dev_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> ctx_;
    mutable std::unique_ptr<ArtifactCore::GpuContext> gc_;
    mutable std::unique_ptr<ArtifactCore::ComputeExecutor> ex_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> cb_;
    mutable bool ok_=false,shared_=false;
    ~TemporalSmearGPUImpl(){release();}
    void release()const{if(ctx_){ctx_->Flush();ctx_->WaitForIdle();}ex_.reset();gc_.reset();cb_.Release();ctx_.Release();dev_.Release();ok_=false;if(shared_){releaseSharedRenderDevice();shared_=false;}}
    void applyCPU(const ImageF32x4RGBAWithCache& s,ImageF32x4RGBAWithCache& d)override{cpu.applyCPU(s,d);}
    void applyGPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst)override{
        if(!dev_&&!acquireSharedRenderDeviceForCurrentBackend(dev_,ctx_)){cpu.applyCPU(src,dst);return;}
        shared_=true;
        auto d=dev_;
        auto c=ctx_;
        if(!ex_){gc_=std::make_unique<ArtifactCore::GpuContext>(d,c);ex_=std::make_unique<ArtifactCore::ComputeExecutor>(*gc_);}
        if(!cb_){Diligent::BufferDesc b;b.Name="TS/CB";b.Size=32;b.Usage=Diligent::USAGE_DYNAMIC;b.BindFlags=Diligent::BIND_UNIFORM_BUFFER;b.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;d->CreateBuffer(b,nullptr,&cb_);if(!cb_){cpu.applyCPU(src,dst);return;}}
        static Diligent::ShaderResourceVariableDesc v[]={{Diligent::SHADER_TYPE_COMPUTE,"TS",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"InTex",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"OutTex",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        if(!ok_){ArtifactCore::ComputePipelineDesc pd;pd.name="TS";pd.shaderSource=kHLSL;pd.entryPoint="main";pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pd.variables=v;pd.variableCount=3;pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!ex_->build(pd)||!ex_->createShaderResourceBinding(true)||!ex_->setBuffer("TS",cb_)){cpu.applyCPU(src,dst);return;}ok_=true;}
        Diligent::RefCntAutoPtr<Diligent::ITexture> in;if(!tsUpload(src,d, c, &in)){cpu.applyCPU(src,dst);return;}
        auto od=in->GetDesc();od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_UNORDERED_ACCESS|Diligent::BIND_SHADER_RESOURCE;
        Diligent::RefCntAutoPtr<Diligent::ITexture> ot;d->CreateTexture(od,nullptr,&ot);if(!ot){cpu.applyCPU(src,dst);return;}
        void* m=nullptr;c->MapBuffer(cb_,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,m);if(m){float fp[8]={cpu.smearAmount_,cpu.sampleJitter_,cpu.velocityScale_,(float)cpu.sampleCount_,(float)od.Width,(float)od.Height,0,0};std::memcpy(m,fp,sizeof(fp));c->UnmapBuffer(cb_,Diligent::MAP_WRITE);}
        if(!ex_->setTextureView("InTex",in->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!ex_->setTextureView("OutTex",ot->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){cpu.applyCPU(src,dst);return;}
        auto at=ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1);ex_->dispatch(c,at,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if(!tsReadback(d,c,ot,dst))cpu.applyCPU(src,dst);
    }
private:
    static constexpr const char* kHLSL=R"(
cbuffer TS:register(b0){float smearAmount,sampleJitter,velocityScale,sampleCount,imageWidth,imageHeight,pad0,pad1;}
Texture2D<float4> InTex:register(t0);RWTexture2D<float4> OutTex:register(u0);
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){
    if(id.x>=(uint)imageWidth||id.y>=(uint)imageHeight)return;
    float4 cur=InTex[id.xy];OutTex[id.xy]=cur;
    if(smearAmount<=0.001||sampleCount<1)return;
    int2 sz=int2((int)imageWidth,(int)imageHeight);
    float2 vel=float2(0,0);
    float4 acc=cur;float tw=1.0;int sc=(int)sampleCount;uint seed=id.x*0x1f1f1f1fu+id.y*0x2d7b2d7bu;
    for(int i=1;i<=sc;++i){float t=(float)i/(float)sc*smearAmount;
        if(sampleJitter>0){seed=seed*1103515245u+12345u;float j=(float)(seed&0x7fffffffu)/(float)0x80000000u;j=(j-0.5)*2.0*sampleJitter*0.5;t=clamp(t+j,0,1);}
        int2 sp=int2((float)id.x+vel.x*t+0.5,(float)id.y+vel.y*t+0.5);sp=clamp(sp,int2(0,0),sz-1);
        acc+=InTex[sp];tw+=1.0;}
    OutTex[id.xy]=acc/tw;
}
)";
};


TemporalSmearEffect::TemporalSmearEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);setDisplayName(UniString("Temporal Smear"));syncImpls();}
TemporalSmearEffect::~TemporalSmearEffect()=default;
float TemporalSmearEffect::smearAmount()const{return smearAmount_;}
void TemporalSmearEffect::setSmearAmount(float v){smearAmount_=std::clamp(v,0.0f,1.0f);syncImpls();}
int TemporalSmearEffect::sampleCount()const{return sampleCount_;}
void TemporalSmearEffect::setSampleCount(int v){sampleCount_=std::clamp(v,2,32);syncImpls();}
float TemporalSmearEffect::sampleJitter()const{return sampleJitter_;}
void TemporalSmearEffect::setSampleJitter(float v){sampleJitter_=std::clamp(v,0.0f,1.0f);syncImpls();}
float TemporalSmearEffect::velocityScale()const{return velocityScale_;}
void TemporalSmearEffect::setVelocityScale(float v){velocityScale_=std::clamp(v,0.0f,16.0f);syncImpls();}
std::vector<AbstractProperty> TemporalSmearEffect::getProperties()const{
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

    addFloat("smearAmount", smearAmount_, 0.0f, 1.0f);
    addInt("sampleCount", sampleCount_, 2, 32);
    addFloat("sampleJitter", sampleJitter_, 0.0f, 1.0f);
    addFloat("velocityScale", velocityScale_, 0.0f, 16.0f);
    return props;
}
void TemporalSmearEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();if(k=="smearAmount")setSmearAmount(v.toFloat());else if(k=="sampleCount")setSampleCount(v.toInt());else if(k=="sampleJitter")setSampleJitter(v.toFloat());else if(k=="velocityScale")setVelocityScale(v.toFloat());}
void TemporalSmearEffect::syncImpls(){
    auto c=std::make_shared<TemporalSmearCPUImpl>();c->smearAmount_=smearAmount_;c->sampleCount_=sampleCount_;c->sampleJitter_=sampleJitter_;c->velocityScale_=velocityScale_;setCPUImpl(c);
    auto g=std::make_shared<TemporalSmearGPUImpl>();g->cpu.smearAmount_=smearAmount_;g->cpu.sampleCount_=sampleCount_;g->cpu.sampleJitter_=sampleJitter_;g->cpu.velocityScale_=velocityScale_;setGPUImpl(g);}
} // namespace Artifact
