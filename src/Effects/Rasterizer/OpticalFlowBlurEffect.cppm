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

module Artifact.Effect.Rasterizer.OpticalFlowBlur;

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

namespace Artifact {
using namespace ArtifactCore;

namespace {
bool createTexFromImage(const ImageF32x4RGBAWithCache& src,Diligent::IRenderDevice* d,Diligent::IDeviceContext* c,Diligent::ITexture** out){
    if(!d||!out)return false;
    const auto& si=src.image();int W=si.width(),H=si.height();
    if(W<=0||H<=0||!si.rgba32fData())return false;
    Diligent::TextureDesc td;td.Name="OFB/Src";td.Type=Diligent::RESOURCE_DIM_TEX_2D;td.Width=W;td.Height=H;td.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;td.MipLevels=1;td.ArraySize=1;td.SampleCount=1;td.Usage=Diligent::USAGE_DEFAULT;td.BindFlags=Diligent::BIND_SHADER_RESOURCE;
    Diligent::RefCntAutoPtr<Diligent::ITexture> t;d->CreateTexture(td,nullptr,&t);if(!t)return false;
    Diligent::TextureSubResData sub;sub.pData=si.rgba32fData();sub.Stride=W*16;
    if(!c)return false;
    c->UpdateTexture(t,0,0,Diligent::Box{0,0,0,(Diligent::Uint32)W,(Diligent::Uint32)H,1},sub,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    *out=t.Detach();return true;
}

bool readbackTex(Diligent::IRenderDevice* d,Diligent::IDeviceContext* c,Diligent::ITexture* s,ImageF32x4RGBAWithCache& dst){
    if(!d||!c||!s)return false;
    auto sd=s->GetDesc();Diligent::TextureDesc st;st.Type=Diligent::RESOURCE_DIM_TEX_2D;st.Width=sd.Width;st.Height=sd.Height;st.Format=sd.Format;st.ArraySize=1;st.MipLevels=1;st.SampleCount=1;st.Usage=Diligent::USAGE_STAGING;st.CPUAccessFlags=Diligent::CPU_ACCESS_READ;st.Name="OFB/Staging";
    Diligent::RefCntAutoPtr<Diligent::ITexture> staging;d->CreateTexture(st,nullptr,&staging);if(!staging)return false;
    Diligent::CopyTextureAttribs cp(s,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);c->CopyTexture(cp);c->Flush();c->WaitForIdle();
    Diligent::MappedTextureSubresource m{};c->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,m);if(!m.pData||m.Stride==0)return false;
    cv::Mat tmp((int)sd.Height,(int)sd.Width,CV_32FC4,m.pData,m.Stride);dst.image().setFromCVMat(tmp);c->UnmapTextureSubresource(staging,0,0);return true;
}
} // namespace

class OpticalFlowBlurCPUImpl : public ArtifactEffectImplBase {
public:
    float blurAmount_=0.5f,flowSmoothness_=0.5f,velocityScale_=1.0f; int sampleCount_=8;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0||!context_.sampler){dst=src;return;}
        const int W=si.width(),H=si.height();
        const float amt=std::clamp(blurAmount_,0.0f,1.0f);
        const int ns=std::clamp(sampleCount_,2,32);
        const float sm=std::clamp(flowSmoothness_,0.0f,1.0f);

        ImageF32x4RGBAWithCache prev;
        bool hp=context_.sampler->sampleCurrentLayerFrameRelative(-1,prev)&&prev.width()>0&&prev.image().rgba32fData();
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        if(!hp||amt<=0.001f)return;

        const float* pd=prev.image().rgba32fData();
        const int pw=prev.width(),ph=prev.height();

        // Lucas-Kanade optical flow per pixel (3x3 window)
        std::vector<float> vx(W*H,0),vy(W*H,0);
        const int r=1;
        for(int y=r;y<H-r;++y)for(int x=r;x<W-r;++x){
            float Ix=0,Iy=0,It=0,Ixx=0,Iyy=0,Ixy=0,Ixt=0,Iyt=0;
            for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx){
                int cx=x+dx,cy=y+dy,px=cx,py=cy;
                if(cx>=W||cy>=H)continue;
                const float* csp=sd+((size_t)cy*W+cx)*4;
                float cl=csp[0]*0.299f+csp[1]*0.587f+csp[2]*0.114f;

                float dxc=0,dyc=0;
                if(x>0&&x<W-1){dxc=(sd[((size_t)cy*W+x+1)*4]*0.299f+sd[((size_t)cy*W+x+1)*4+1]*0.587f+sd[((size_t)cy*W+x+1)*4+2]*0.114f)-(sd[((size_t)cy*W+x-1)*4]*0.299f+sd[((size_t)cy*W+x-1)*4+1]*0.587f+sd[((size_t)cy*W+x-1)*4+2]*0.114f);dxc*=0.5f;}
                if(y>0&&y<H-1){dyc=(sd[((size_t)(y+1)*W+x)*4]*0.299f+sd[((size_t)(y+1)*W+x)*4+1]*0.587f+sd[((size_t)(y+1)*W+x)*4+2]*0.114f)-(sd[((size_t)(y-1)*W+x)*4]*0.299f+sd[((size_t)(y-1)*W+x)*4+1]*0.587f+sd[((size_t)(y-1)*W+x)*4+2]*0.114f);dyc*=0.5f;}

                float pl=0;if(cx>=0&&cx<pw&&cy>=0&&cy<ph){const float* pp=pd+((size_t)cy*pw+cx)*4;pl=pp[0]*0.299f+pp[1]*0.587f+pp[2]*0.114f;}
                Ix+=dxc;Iy+=dyc;It+=cl-pl;
                Ixx+=dxc*dxc;Iyy+=dyc*dyc;Ixy+=dxc*dyc;Ixt+=dxc*It;Iyt+=dyc*It;
            }
            float det=Ixx*Iyy-Ixy*Ixy;
            if(fabsf(det)>1e-6f){vx[y*W+x]=(Iyy*Ixt-Ixy*Iyt)/det;vy[y*W+x]=(Ixx*Iyt-Ixy*Ixt)/det;}
        }

        // Apply smear along optical flow
        for(int y=0;y<H;++y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){
                float mx=vx[y*W+x]*velocityScale_,my=vy[y*W+x]*velocityScale_;
                float* p=o+(size_t)x*4;
                float r=p[0],g=p[1],b=p[2],a=p[3];
                if(amt>0.001f&&(fabsf(mx)>0.1f||fabsf(my)>0.1f)){
                    uint seed=(uint)x*0x1f1f1f1fu+(uint)y*0x2d7b2d7bu;
                    for(int i=1;i<=ns;++i){float t=(float)i/(float)ns*amt;
                        if(sm>0){seed=seed*1103515245u+12345u;float j=(float)(seed&0x7fffffffu)/(float)0x80000000u;j=(j-0.5f)*2.0f*sm*0.5f;t=std::clamp(t+j,0.0f,1.0f);}
                        int sx=(int)((float)x+mx*t+0.5f),sy=(int)((float)y+my*t+0.5f);
                        sx=std::clamp(sx,0,W-1);sy=std::clamp(sy,0,H-1);
                        auto*sp=sd+((size_t)sy*W+sx)*4;r+=sp[0];g+=sp[1];b+=sp[2];a+=sp[3];
                    }
                    float inv=1.0f/(float)(ns+1);
                    p[0]=r*inv;p[1]=g*inv;p[2]=b*inv;p[3]=a*inv;
                }
            }
        }
    }
};

OpticalFlowBlurEffect::OpticalFlowBlurEffect():ArtifactAbstractEffect(){
    setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();
}
OpticalFlowBlurEffect::~OpticalFlowBlurEffect()=default;
float OpticalFlowBlurEffect::blurAmount()const{return blurAmount_;}
void OpticalFlowBlurEffect::setBlurAmount(float v){blurAmount_=std::clamp(v,0.0f,1.0f);syncImpls();}
int OpticalFlowBlurEffect::sampleCount()const{return sampleCount_;}
void OpticalFlowBlurEffect::setSampleCount(int v){sampleCount_=std::clamp(v,2,32);syncImpls();}
float OpticalFlowBlurEffect::flowSmoothness()const{return flowSmoothness_;}
void OpticalFlowBlurEffect::setFlowSmoothness(float v){flowSmoothness_=std::clamp(v,0.0f,1.0f);syncImpls();}
float OpticalFlowBlurEffect::velocityScale()const{return velocityScale_;}
void OpticalFlowBlurEffect::setVelocityScale(float v){velocityScale_=std::clamp(v,0.0f,16.0f);syncImpls();}
std::vector<AbstractProperty> OpticalFlowBlurEffect::getProperties()const{
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

    addFloat("blurAmount", blurAmount_, 0.0f, 1.0f);
    addInt("sampleCount", sampleCount_, 2, 32);
    addFloat("flowSmoothness", flowSmoothness_, 0.0f, 1.0f);
    addFloat("velocityScale", velocityScale_, 0.0f, 16.0f);
    return props;
}
void OpticalFlowBlurEffect::setPropertyValue(const UniString& n,const QVariant& v){
    const QString k=n.toQString();
    if(k=="blurAmount")setBlurAmount(v.toFloat());
    else if(k=="sampleCount")setSampleCount(v.toInt());
    else if(k=="flowSmoothness")setFlowSmoothness(v.toFloat());
    else if(k=="velocityScale")setVelocityScale(v.toFloat());
}
// ---------------------------------------------------------------------------
// GPU impl
// ---------------------------------------------------------------------------
class OpticalFlowBlurGPUImpl : public ArtifactEffectImplBase {
public:
    OpticalFlowBlurCPUImpl cpu;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> dev_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> ctx_;
    mutable std::unique_ptr<ArtifactCore::GpuContext> gc_;
    mutable std::unique_ptr<ArtifactCore::ComputeExecutor> ex_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> cb_;
    mutable bool ok_=false,shared_=false;

    ~OpticalFlowBlurGPUImpl(){release();}
    void release()const{if(ctx_){ctx_->Flush();ctx_->WaitForIdle();}ex_.reset();gc_.reset();cb_.Release();ctx_.Release();dev_.Release();ok_=false;if(shared_){releaseSharedRenderDevice();shared_=false;}}
    void applyCPU(const ImageF32x4RGBAWithCache& s,ImageF32x4RGBAWithCache& d)override{cpu.applyCPU(s,d);}

    void applyGPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst)override{
        if(!dev_&&!acquireSharedRenderDeviceForCurrentBackend(dev_,ctx_)){cpu.applyCPU(src,dst);return;}
        shared_=true;
        auto d=dev_;
        auto c=ctx_;
        if(!ex_){gc_=std::make_unique<ArtifactCore::GpuContext>(d,c);ex_=std::make_unique<ArtifactCore::ComputeExecutor>(*gc_);}

        if(!cb_){Diligent::BufferDesc b;b.Name="OFB";b.Size=32;b.Usage=Diligent::USAGE_DYNAMIC;b.BindFlags=Diligent::BIND_UNIFORM_BUFFER;b.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;d->CreateBuffer(b,nullptr,&cb_);if(!cb_){cpu.applyCPU(src,dst);return;}}

        static Diligent::ShaderResourceVariableDesc v[]={
            {Diligent::SHADER_TYPE_COMPUTE,"OFB",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE,"InTex",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE,"OutTex",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};

        if(!ok_){ArtifactCore::ComputePipelineDesc pd;pd.name="OFB";pd.shaderSource=kHLSL;pd.entryPoint="main";
            pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pd.variables=v;pd.variableCount=3;
            pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if(!ex_->build(pd)||!ex_->createShaderResourceBinding(true)||!ex_->setBuffer("OFB",cb_)){cpu.applyCPU(src,dst);return;}
            ok_=true;}

        Diligent::RefCntAutoPtr<Diligent::ITexture> in;if(!createTexFromImage(src,d,c,&in)){cpu.applyCPU(src,dst);return;}
        auto od=in->GetDesc();od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_UNORDERED_ACCESS|Diligent::BIND_SHADER_RESOURCE;
        Diligent::RefCntAutoPtr<Diligent::ITexture> ot;d->CreateTexture(od,nullptr,&ot);if(!ot){cpu.applyCPU(src,dst);return;}

        void* m=nullptr;c->MapBuffer(cb_,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,m);
        if(m){float fp[8]={cpu.blurAmount_,cpu.flowSmoothness_,cpu.velocityScale_,(float)cpu.sampleCount_,(float)od.Width,(float)od.Height,0,0};
            std::memcpy(m,fp,sizeof(fp));c->UnmapBuffer(cb_,Diligent::MAP_WRITE);}

        if(!ex_->setTextureView("InTex",in->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||
           !ex_->setTextureView("OutTex",ot->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){cpu.applyCPU(src,dst);return;}

        auto at=ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1);
        ex_->dispatch(c,at,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if(!readbackTex(d,c,ot,dst))cpu.applyCPU(src,dst);
    }

private:
    static constexpr const char* kHLSL=R"(
cbuffer OFB:register(b0){float blurAmount,flowSmoothness,velocityScale,sampleCount,imageWidth,imageHeight,pad0,pad1;}
Texture2D<float4> InTex:register(t0);
RWTexture2D<float4> OutTex:register(u0);

[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){
    if(id.x>=(uint)imageWidth||id.y>=(uint)imageHeight)return;
    float4 cur=InTex[id.xy];OutTex[id.xy]=cur;
    if(blurAmount<=0.001||sampleCount<1)return;
    int2 sz=int2((int)imageWidth,(int)imageHeight);
    float fx=0,fxx=0,fxy=0,fyy=0,fxt=0,fyt=0;
    for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){
        int2 cp=clamp(int2(id.xy)+int2(dx,dy),int2(0,0),sz-1);
        float lx=InTex[int2(min(cp.x+1,sz.x-1),cp.y)].r-InTex[int2(max(cp.x-1,0),cp.y)].r;
        float ly=InTex[int2(cp.x,min(cp.y+1,sz.y-1))].r-InTex[int2(cp.x,max(cp.y-1,0))].r;
        float lt=(InTex[cp].r*0.299+InTex[cp].g*0.587+InTex[cp].b*0.114-(InTex[cp].r*0.299+InTex[cp].g*0.587+InTex[cp].b*0.114));
        fxx+=lx*lx;fyy+=ly*ly;fxy+=lx*ly;fxt+=lx*lt;fyt+=ly*lt;
    }
    float d=fxx*fyy-fxy*fxy;float vx=0,vy=0;
    if(abs(d)>1e-6){vx=(fyy*fxt-fxy*fyt)/d*velocityScale;vy=(fxx*fyt-fxy*fxt)/d*velocityScale;}
    float4 acc=cur;float tw=1.0;int sc=(int)sampleCount;
    for(int i=1;i<=sc;++i){float t=(float)i/(float)sc*blurAmount;
        int2 sp=int2((float)id.x+vx*t+0.5,(float)id.y+vy*t+0.5);sp=clamp(sp,int2(0,0),sz-1);
        acc+=InTex[sp];tw+=1.0;
    }
    OutTex[id.xy]=acc/tw;
}
)";
};


void OpticalFlowBlurEffect::syncImpls(){
    auto c=std::make_shared<OpticalFlowBlurCPUImpl>();
    c->blurAmount_=blurAmount_;c->sampleCount_=sampleCount_;c->flowSmoothness_=flowSmoothness_;c->velocityScale_=velocityScale_;
    setCPUImpl(c);
    auto g=std::make_shared<OpticalFlowBlurGPUImpl>();
    g->cpu.blurAmount_=blurAmount_;g->cpu.sampleCount_=sampleCount_;g->cpu.flowSmoothness_=flowSmoothness_;g->cpu.velocityScale_=velocityScale_;
    setGPUImpl(g);
}
} // namespace Artifact
