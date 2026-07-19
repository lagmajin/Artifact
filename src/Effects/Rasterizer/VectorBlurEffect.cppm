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

module Artifact.Effect.Rasterizer.VectorBlur;

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
bool vbUp(const ImageF32x4RGBAWithCache& src,Diligent::IRenderDevice* d,Diligent::IDeviceContext* c,Diligent::ITexture** out){
    if(!d||!c||!out)return false;const auto& si=src.image();int W=si.width(),H=si.height();
    if(W<=0||H<=0||!si.rgba32fData())return false;
    Diligent::TextureDesc td;td.Name="VB";td.Type=Diligent::RESOURCE_DIM_TEX_2D;td.Width=W;td.Height=H;td.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;td.MipLevels=1;td.ArraySize=1;td.SampleCount=1;td.Usage=Diligent::USAGE_DEFAULT;td.BindFlags=Diligent::BIND_SHADER_RESOURCE;
    Diligent::RefCntAutoPtr<Diligent::ITexture> t;d->CreateTexture(td,nullptr,&t);if(!t)return false;
    Diligent::TextureSubResData sub;sub.pData=si.rgba32fData();sub.Stride=W*16;
    c->UpdateTexture(t,0,0,Diligent::Box{0,0,0,(Diligent::Uint32)W,(Diligent::Uint32)H,1},sub,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    *out=t.Detach();return true;
}
bool vbDown(Diligent::IRenderDevice*d,Diligent::IDeviceContext*c,Diligent::ITexture*s,ImageF32x4RGBAWithCache&dst){
    if(!d||!c||!s)return false;auto sd=s->GetDesc();Diligent::TextureDesc st;st.Type=Diligent::RESOURCE_DIM_TEX_2D;st.Width=sd.Width;st.Height=sd.Height;st.Format=sd.Format;st.ArraySize=1;st.MipLevels=1;st.SampleCount=1;st.Usage=Diligent::USAGE_STAGING;st.CPUAccessFlags=Diligent::CPU_ACCESS_READ;
    Diligent::RefCntAutoPtr<Diligent::ITexture> sg;d->CreateTexture(st,nullptr,&sg);if(!sg)return false;
    Diligent::CopyTextureAttribs cp(s,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,sg,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);c->CopyTexture(cp);c->Flush();c->WaitForIdle();
    Diligent::MappedTextureSubresource m{};c->MapTextureSubresource(sg,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,m);if(!m.pData||m.Stride==0)return false;
    cv::Mat tmp((int)sd.Height,(int)sd.Width,CV_32FC4,m.pData,m.Stride);dst.image().setFromCVMat(tmp);c->UnmapTextureSubresource(sg,0,0);return true;}
}

class VectorBlurCPUImpl : public ArtifactEffectImplBase {
public:
    float shutterAngle_=180.0f; int samples_=8; float exposureComp_=1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height();
        const float sa=std::clamp(shutterAngle_,0.0f,720.0f)/360.0f;const int ns=std::clamp(samples_,2,32);
        if(sa<=0.001f||!context_.sampler){dst=src.DeepCopy();return;}
        ImageF32x4RGBAWithCache prev;
        bool hp=context_.sampler->sampleCurrentLayerFrameRelative(-1,prev)&&prev.width()>0&&prev.image().rgba32fData();
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();if(!hp)return;
        const float* pd=prev.image().rgba32fData();int pw=prev.width(),ph=prev.height();
        int BS=8,vw=(W+BS-1)/BS,vh=(H+BS-1)/BS;std::vector<float> vx(vw*vh,0),vy(vw*vh,0);
        ArtifactCore::Parallel::For(0,vh,[&](int by){for(int bx=0;bx<vw;++bx){int sx=bx*BS,sy=by*BS,ex=std::min(sx+BS,W),ey=std::min(sy+BS,H);float best=1e12f,bdx=0,bdy=0;
            for(int dy=-16;dy<=16;dy+=4)for(int dx=-16;dx<=16;dx+=4){float diff=0;int cnt=0;
                for(int y=sy;y<ey;y+=2)for(int x=sx;x<ex;x+=2){int cx=x+dx,cy=y+dy;
                    if((unsigned)cx<(unsigned)pw&&(unsigned)cy<(unsigned)ph){auto*sp=sd+((size_t)y*W+x)*4,*pp=pd+((size_t)cy*pw+cx)*4;float dr=sp[0]-pp[0],dg=sp[1]-pp[1],db=sp[2]-pp[2];diff+=dr*dr+dg*dg+db*db;++cnt;}}
                if(cnt>0){diff/=(float)cnt;if(diff<best){best=diff;bdx=(float)(-dx);bdy=(float)(-dy);}}}
            vx[by*vw+bx]=bdx*sa;vy[by*vw+bx]=bdy*sa;}});
        ArtifactCore::Parallel::For(0,H,[&](int y){int bvY=y/BS;float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){int bvX=x/BS;float mx=vx[std::min(bvY,vh-1)*vw+std::min(bvX,vw-1)],my=vy[std::min(bvY,vh-1)*vw+std::min(bvX,vw-1)];float* p=o+(size_t)x*4;float r=p[0],g=p[1],b=p[2],a=p[3];
                for(int i=1;i<=ns;++i){float t=((float)i-0.5f)/(float)ns;int sx=(int)((float)x+mx*t+0.5f),sy=(int)((float)y+my*t+0.5f);sx=std::clamp(sx,0,W-1);sy=std::clamp(sy,0,H-1);auto*sp=sd+((size_t)sy*W+sx)*4;r+=sp[0];g+=sp[1];b+=sp[2];a+=sp[3];}
                float inv=1.0f/(float)(ns+1)*exposureComp_;p[0]=std::clamp(r*inv,0.0f,1.0f);p[1]=std::clamp(g*inv,0.0f,1.0f);p[2]=std::clamp(b*inv,0.0f,1.0f);p[3]=std::clamp(a*inv,0.0f,1.0f);}});
    }
};

class VectorBlurGPUImpl : public ArtifactEffectImplBase {
public:
    VectorBlurCPUImpl cpu;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> dev_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> ctx_;
    mutable std::unique_ptr<ArtifactCore::GpuContext> gc_;
    mutable std::unique_ptr<ArtifactCore::ComputeExecutor> ex_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> cb_;
    mutable bool ok_=false,shared_=false;
    ~VectorBlurGPUImpl(){release();}
    void release()const{if(ctx_){ctx_->Flush();ctx_->WaitForIdle();}ex_.reset();gc_.reset();cb_.Release();ctx_.Release();dev_.Release();ok_=false;if(shared_){releaseSharedRenderDevice();shared_=false;}}
    void applyCPU(const ImageF32x4RGBAWithCache& s,ImageF32x4RGBAWithCache& d)override{cpu.applyCPU(s,d);}
    void applyGPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst)override{
        if(!dev_&&!acquireSharedRenderDeviceForCurrentBackend(dev_,ctx_)){cpu.applyCPU(src,dst);return;}
        shared_=true;
        auto d=dev_;
        auto c=ctx_;
        if(!ex_){gc_=std::make_unique<ArtifactCore::GpuContext>(d,c);ex_=std::make_unique<ArtifactCore::ComputeExecutor>(*gc_);}
        if(!cb_){Diligent::BufferDesc b;b.Name="VB";b.Size=32;b.Usage=Diligent::USAGE_DYNAMIC;b.BindFlags=Diligent::BIND_UNIFORM_BUFFER;b.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;d->CreateBuffer(b,nullptr,&cb_);if(!cb_){cpu.applyCPU(src,dst);return;}}
        static Diligent::ShaderResourceVariableDesc v[]={{Diligent::SHADER_TYPE_COMPUTE,"VB",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"InTex",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"VelTex",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"OutTex",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        if(!ok_){ArtifactCore::ComputePipelineDesc pd;pd.name="VB";pd.shaderSource=kHLSL;pd.entryPoint="main";pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pd.variables=v;pd.variableCount=4;pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!ex_->build(pd)||!ex_->createShaderResourceBinding(true)||!ex_->setBuffer("VB",cb_)){cpu.applyCPU(src,dst);return;}ok_=true;}
        Diligent::RefCntAutoPtr<Diligent::ITexture> in;if(!vbUp(src,d,c,&in)){cpu.applyCPU(src,dst);return;}
        auto od=in->GetDesc();od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_UNORDERED_ACCESS|Diligent::BIND_SHADER_RESOURCE;
        Diligent::RefCntAutoPtr<Diligent::ITexture> ot;d->CreateTexture(od,nullptr,&ot);if(!ot){cpu.applyCPU(src,dst);return;}
        void* m=nullptr;c->MapBuffer(cb_,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,m);if(m){float fp[8]={cpu.shutterAngle_,cpu.exposureComp_,(float)cpu.samples_,(float)od.Width,(float)od.Height,0,0,0};std::memcpy(m,fp,sizeof(fp));c->UnmapBuffer(cb_,Diligent::MAP_WRITE);}
        if(!ex_->setTextureView("InTex",in->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!ex_->setTextureView("OutTex",ot->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){cpu.applyCPU(src,dst);return;}
        auto at=ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1);ex_->dispatch(c,at,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if(!vbDown(d,c,ot,dst))cpu.applyCPU(src,dst);
        dst.image().setColorDescriptor(src.image().colorDescriptor());
    }
private:
    static constexpr const char* kHLSL=R"(
cbuffer VB:register(b0){float shutterAngle,exposureComp,samples,imageWidth,imageHeight,pad0,pad1,pad2;}
Texture2D<float4> InTex:register(t0);Texture2D<float2> VelTex:register(t1);RWTexture2D<float4> OutTex:register(u0);
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){
    if(id.x>=(uint)imageWidth||id.y>=(uint)imageHeight)return;
    float4 cur=InTex[id.xy];OutTex[id.xy]=cur;
    if(shutterAngle<=0.001||samples<2)return;
    int2 sz=int2((int)imageWidth,(int)imageHeight);
    float2 vel=float2(0,0);
    float4 acc=cur;float tw=1.0;int sc=(int)samples;
    for(int i=1;i<=sc;++i){float t=((float)i-0.5)/(float)sc;
        int2 sp=int2((float)id.x+vel.x*t+0.5,(float)id.y+vel.y*t+0.5);sp=clamp(sp,int2(0,0),sz-1);
        acc+=InTex[sp];tw+=1.0;}
    OutTex[id.xy]=acc/tw*exposureComp;
}
)";
};

VectorBlurEffect::VectorBlurEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();}
VectorBlurEffect::~VectorBlurEffect()=default;
float VectorBlurEffect::shutterAngle()const{return shutterAngle_;}
void VectorBlurEffect::setShutterAngle(float v){shutterAngle_=std::clamp(v,0.0f,720.0f);syncImpls();}
int VectorBlurEffect::samples()const{return samples_;}
void VectorBlurEffect::setSamples(int v){samples_=std::clamp(v,2,32);syncImpls();}
float VectorBlurEffect::exposureCompensation()const{return exposureComp_;}
void VectorBlurEffect::setExposureCompensation(float v){exposureComp_=std::max(v,0.0f);syncImpls();}
std::vector<AbstractProperty> VectorBlurEffect::getProperties()const{
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

    addFloat("shutterAngle", shutterAngle_, 0.0f, 720.0f);
    addInt("samples", samples_, 2, 32);
    addFloat("exposureCompensation", exposureComp_, 0.0f, 4.0f);
    return props;
}
void VectorBlurEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();if(k=="shutterAngle")setShutterAngle(v.toFloat());else if(k=="samples")setSamples(v.toInt());else if(k=="exposureCompensation")setExposureCompensation(v.toFloat());
}
void VectorBlurEffect::syncImpls(){
    auto c=std::make_shared<VectorBlurCPUImpl>();c->shutterAngle_=shutterAngle_;c->samples_=samples_;c->exposureComp_=exposureComp_;setCPUImpl(c);
    auto g=std::make_shared<VectorBlurGPUImpl>();g->cpu.shutterAngle_=shutterAngle_;g->cpu.samples_=samples_;g->cpu.exposureComp_=exposureComp_;setGPUImpl(g);
}
} // namespace Artifact
