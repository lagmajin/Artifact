module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>
#include <opencv2/opencv.hpp>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Rasterizer.ChromaticAberration;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {
using namespace ArtifactCore;

class ChromaticAberrationCPUImpl : public ArtifactEffectImplBase {
public:
    float redShift_=2.0f,blueShift_=2.0f,cx_=0.5f,cy_=0.5f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height();
        float rx=std::clamp(redShift_,0.0f,50.0f),bx=std::clamp(blueShift_,0.0f,50.0f);
        float cx=cx_*(float)W,cy=cy_*(float)H;

        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        Parallel::For(0,H,[&](int y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float dx=(float)x-cx,dy=(float)y-cy,dist=std::sqrt(dx*dx+dy*dy);
                float nrm=dist/std::max((float)std::max(W,H)*0.7f,1.0f);
                float rs=nrm*rx,bs=nrm*bx;
                int rxi=std::clamp((int)((float)x+dx/dist*rs+0.5f),0,W-1);
                int ryi=std::clamp((int)((float)y+dy/dist*rs+0.5f),0,H-1);
                int bxi=std::clamp((int)((float)x+dx/dist*(-bs)+0.5f),0,W-1);
                int byi=std::clamp((int)((float)y+dy/dist*(-bs)+0.5f),0,H-1);
                float rc[4],bc[4];
                auto clp=[&](int ix,int iy,float* out){const float*sp=sd+((size_t)iy*W+ix)*4;out[0]=sp[0];out[1]=sp[1];out[2]=sp[2];out[3]=sp[3];};
                clp(rxi,ryi,rc);clp(bxi,byi,bc);
                p[0]=rc[0];p[2]=bc[2];
            }
        });
    }
};

class ChromaticAberrationGPUImpl : public ArtifactEffectImplBase {
public:
    float redShift_=2.0f,blueShift_=2.0f,cx_=0.5f,cy_=0.5f;
    void applyCPU(const ImageF32x4RGBAWithCache& s,ImageF32x4RGBAWithCache& d) override { cpu_.redShift_=redShift_;cpu_.blueShift_=blueShift_;cpu_.cx_=cx_;cpu_.cy_=cy_;cpu_.applyCPU(s,d); }
    void applyGPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device; Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if(!acquireSharedRenderDeviceForCurrentBackend(device,context)){applyCPU(src,dst);return;}
        const auto& image=src.image();const float* data=image.rgba32fData();if(!data||image.width()<=0||image.height()<=0){applyCPU(src,dst);return;}
        Diligent::TextureDesc d{};d.Name="ChromaticAberration/Input";d.Type=Diligent::RESOURCE_DIM_TEX_2D;d.Width=image.width();d.Height=image.height();d.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;d.MipLevels=1;d.ArraySize=1;d.SampleCount=1;d.Usage=Diligent::USAGE_IMMUTABLE;d.BindFlags=Diligent::BIND_SHADER_RESOURCE;Diligent::TextureSubResData sub{};sub.pData=data;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(d,&init,&input);if(!input){applyCPU(src,dst);return;}
        auto od=d;od.Name="ChromaticAberration/Output";od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(od,nullptr,&output);if(!output){applyCPU(src,dst);return;}
        struct Params{float red,blue,cx,cy;};Diligent::BufferDesc bd{};bd.Name="ChromaticAberration/Params";bd.Size=sizeof(Params);bd.Usage=Diligent::USAGE_DYNAMIC;bd.BindFlags=Diligent::BIND_UNIFORM_BUFFER;bd.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(bd,nullptr,&params);if(!params){applyCPU(src,dst);return;}void*m=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,m);if(!m){applyCPU(src,dst);return;}Params p{redShift_,blueShift_,cx_,cy_};std::memcpy(m,&p,sizeof(p));context->UnmapBuffer(params,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"ChromaticAberrationParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gc{device,context};ArtifactCore::ComputeExecutor ex{gc};ArtifactCore::ComputePipelineDesc pd{};pd.name="ChromaticAberration/PSO";pd.shaderSource=kHlsl;pd.entryPoint="main";pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pd.variables=vars;pd.variableCount=3;pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!ex.build(pd)||!ex.createShaderResourceBinding(true)||!ex.setBuffer("ChromaticAberrationParams",params)||!ex.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!ex.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}ex.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        auto sd=od;sd.Name="ChromaticAberration/Readback";sd.Usage=Diligent::USAGE_STAGING;sd.BindFlags=Diligent::BIND_NONE;sd.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(sd,nullptr,&staging);if(!staging){applyCPU(src,dst);return;}context->CopyTexture(Diligent::CopyTextureAttribs(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){applyCPU(src,dst);return;}cv::Mat result(image.height(),image.width(),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result,image.colorDescriptor());context->UnmapTextureSubresource(staging,0,0);
    }
private:
    ChromaticAberrationCPUImpl cpu_;
    static constexpr const char* kHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer ChromaticAberrationParams:register(b0){float g_Red;float g_Blue;float g_Cx;float g_Cy;}
float4 atp(int2 p,uint w,uint h){return g_InputTexture[uint2(clamp(p.x,0,(int)w-1),clamp(p.y,0,(int)h-1))];}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float2 q=float2(id.xy),c=float2(g_Cx*w,g_Cy*h),v=q-c;float dist=length(v),n=dist/max(max(w,h)*0.7,1);float2 dir=dist>0?v/dist:float2(0,0);float rs=n*g_Red,bs=n*g_Blue;float4 r=atp(int2(q+dir*rs+0.5),w,h),b=atp(int2(q-dir*bs+0.5),w,h),o=g_InputTexture[id.xy];o.r=r.r;o.b=b.b;g_OutputTexture[id.xy]=o;}
)";
};

ChromaticAberrationEffect::ChromaticAberrationEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();setGPUImpl(std::make_shared<ChromaticAberrationGPUImpl>());}
ChromaticAberrationEffect::~ChromaticAberrationEffect()=default;
float ChromaticAberrationEffect::redShift()const{return redShift_;}void ChromaticAberrationEffect::setRedShift(float v){redShift_=std::clamp(v,0.0f,50.0f);syncImpls();}
float ChromaticAberrationEffect::blueShift()const{return blueShift_;}void ChromaticAberrationEffect::setBlueShift(float v){blueShift_=std::clamp(v,0.0f,50.0f);syncImpls();}
float ChromaticAberrationEffect::centerX()const{return cx_;}void ChromaticAberrationEffect::setCenterX(float v){cx_=std::clamp(v,0.0f,1.0f);syncImpls();}
float ChromaticAberrationEffect::centerY()const{return cy_;}void ChromaticAberrationEffect::setCenterY(float v){cy_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> ChromaticAberrationEffect::getProperties()const{
    std::vector<AbstractProperty> props;
    props.reserve(4);

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

    addFloat("redShift", redShift_, 0.0f, 50.0f);
    addFloat("blueShift", blueShift_, 0.0f, 50.0f);
    addFloat("centerX", cx_, 0.0f, 1.0f);
    addFloat("centerY", cy_, 0.0f, 1.0f);
    return props;
}
void ChromaticAberrationEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="redShift")setRedShift(v.toFloat());else if(k=="blueShift")setBlueShift(v.toFloat());else if(k=="centerX")setCenterX(v.toFloat());else if(k=="centerY")setCenterY(v.toFloat());}
void ChromaticAberrationEffect::syncImpls(){auto c=std::make_shared<ChromaticAberrationCPUImpl>();c->redShift_=redShift_;c->blueShift_=blueShift_;c->cx_=cx_;c->cy_=cy_;setCPUImpl(c);if(auto* g=dynamic_cast<ChromaticAberrationGPUImpl*>(gpuImpl().get())){g->redShift_=redShift_;g->blueShift_=blueShift_;g->cx_=cx_;g->cy_=cy_;}}
} // namespace Artifact
