module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Rasterizer.Vignette;

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

class VignetteCPUImpl : public ArtifactEffectImplBase {
public:
    float amount_=0.7f,radius_=0.8f,feather_=0.4f,cx_=0.5f,cy_=0.5f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image();const float* sd=si.rgba32fData();
        if(!sd||si.width()<=0){dst=src;return;}
        const int W=si.width(),H=si.height();
        float a=std::clamp(amount_,0.0f,1.0f),r=std::clamp(radius_,0.0f,2.0f),f=std::clamp(feather_,0.01f,2.0f);
        float cx=cx_*(float)W,cy=cy_*(float)H,maxD=std::sqrt(cx*cx+cy*cy)*r;
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        Parallel::For(0,H,[&](int y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float dx=(float)x-cx,dy=(float)y-cy,dist=std::sqrt(dx*dx+dy*dy);
                float mask=1.0f-std::clamp((dist-maxD*f)/(maxD*(1.0f-f)+0.001f),0.0f,1.0f)*a;
                p[0]*=mask;p[1]*=mask;p[2]*=mask;
            }
        });
    }
};

class VignetteGPUImpl : public ArtifactEffectImplBase {
public:
    float amount_=0.7f,radius_=0.8f,feather_=0.4f,cx_=0.5f,cy_=0.5f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        VignetteCPUImpl cpu;
        cpu.amount_=amount_; cpu.radius_=radius_; cpu.feather_=feather_; cpu.cx_=cx_; cpu.cy_=cy_;
        cpu.applyCPU(src,dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
        Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if(!acquireSharedRenderDeviceForCurrentBackend(device,context)) { applyCPU(src,dst); return; }
        const auto& image=src.image(); const float* pixels=image.rgba32fData();
        if(!pixels||image.width()<=0||image.height()<=0) { applyCPU(src,dst); return; }

        Diligent::TextureDesc desc{};
        desc.Name="Vignette/Input"; desc.Type=Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width=image.width(); desc.Height=image.height(); desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.MipLevels=1; desc.ArraySize=1; desc.SampleCount=1;
        desc.Usage=Diligent::USAGE_IMMUTABLE; desc.BindFlags=Diligent::BIND_SHADER_RESOURCE;
        Diligent::TextureSubResData sub{}; sub.pData=pixels;
        sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;
        Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1;
        Diligent::RefCntAutoPtr<Diligent::ITexture> input; device->CreateTexture(desc,&init,&input);
        if(!input) { applyCPU(src,dst); return; }
        Diligent::TextureDesc outDesc=desc; outDesc.Name="Vignette/Output";
        outDesc.Usage=Diligent::USAGE_DEFAULT;
        outDesc.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;
        Diligent::RefCntAutoPtr<Diligent::ITexture> output; device->CreateTexture(outDesc,nullptr,&output);
        if(!output) { applyCPU(src,dst); return; }

        Diligent::BufferDesc cbDesc{}; cbDesc.Name="Vignette/Params"; cbDesc.Size=sizeof(Params);
        cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;
        Diligent::RefCntAutoPtr<Diligent::IBuffer> params; device->CreateBuffer(cbDesc,nullptr,&params);
        if(!params) { applyCPU(src,dst); return; }
        void* mapped=nullptr; context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped);
        if(!mapped) { applyCPU(src,dst); return; }
        Params values{amount_,radius_,feather_,cx_,cy_}; std::memcpy(mapped,&values,sizeof(values));
        context->UnmapBuffer(params,Diligent::MAP_WRITE);

        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE,"VignetteParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        ArtifactCore::GpuContext gpuContext{device,context}; ArtifactCore::ComputeExecutor executor{gpuContext};
        ArtifactCore::ComputePipelineDesc pipeline{}; pipeline.name="Vignette/PSO";
        pipeline.shaderSource=kHlsl; pipeline.entryPoint="main";
        pipeline.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
        pipeline.variables=vars; pipeline.variableCount=3;
        pipeline.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        if(!executor.build(pipeline)||!executor.createShaderResourceBinding(true)||
           !executor.setBuffer("VignetteParams",params)||
           !executor.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||
           !executor.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src,dst); return; }
        executor.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Diligent::TextureDesc stagingDesc=outDesc; stagingDesc.Name="Vignette/Readback";
        stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.BindFlags=Diligent::BIND_NONE;
        stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ;
        Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging);
        if(!staging) { applyCPU(src,dst); return; }
        Diligent::CopyTextureAttribs copy(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->CopyTexture(copy); context->Flush(); context->WaitForIdle();
        Diligent::MappedTextureSubresource read{};
        context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);
        if(!read.pData||!read.Stride) { applyCPU(src,dst); return; }
        cv::Mat result(static_cast<int>(outDesc.Height),static_cast<int>(outDesc.Width),CV_32FC4,read.pData,read.Stride);
        dst.image().setFromCVMat(result,image.colorDescriptor()); context->UnmapTextureSubresource(staging,0,0);
    }

private:
    struct Params { float amount,radius,feather,cx,cy; float padding[3]{}; };
    static constexpr const char* kHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0); RWTexture2D<float4> g_OutputTexture:register(u0);
cbuffer VignetteParams:register(b0){float g_Amount;float g_Radius;float g_Feather;float g_Cx;float g_Cy;float3 g_Pad;}
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float cx=g_Cx*w,cy=g_Cy*h;float maxD=sqrt(cx*cx+cy*cy)*g_Radius;float dx=(float)id.x-cx,dy=(float)id.y-cy;float dist=sqrt(dx*dx+dy*dy);float mask=1.0-saturate((dist-maxD*g_Feather)/(maxD*(1.0-g_Feather)+0.001))*g_Amount;float4 c=g_InputTexture[id.xy];g_OutputTexture[id.xy]=float4(c.rgb*mask,c.a);}
)";
};

VignetteEffect::VignetteEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();setGPUImpl(std::make_shared<VignetteGPUImpl>());setComputeMode(ComputeMode::AUTO);}
VignetteEffect::~VignetteEffect()=default;
float VignetteEffect::amount()const{return amount_;}void VignetteEffect::setAmount(float v){amount_=std::clamp(v,0.0f,1.0f);syncImpls();}
float VignetteEffect::radius()const{return radius_;}void VignetteEffect::setRadius(float v){radius_=std::clamp(v,0.0f,2.0f);syncImpls();}
float VignetteEffect::feather()const{return feather_;}void VignetteEffect::setFeather(float v){feather_=std::clamp(v,0.01f,2.0f);syncImpls();}
float VignetteEffect::centerX()const{return cx_;}void VignetteEffect::setCenterX(float v){cx_=std::clamp(v,0.0f,1.0f);syncImpls();}
float VignetteEffect::centerY()const{return cy_;}void VignetteEffect::setCenterY(float v){cy_=std::clamp(v,0.0f,1.0f);syncImpls();}
std::vector<AbstractProperty> VignetteEffect::getProperties()const{
    std::vector<AbstractProperty> props;
    props.reserve(5);

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

    addFloat("amount", amount_, 0.0f, 1.0f);
    addFloat("radius", radius_, 0.0f, 2.0f);
    addFloat("feather", feather_, 0.01f, 2.0f);
    addFloat("centerX", cx_, 0.0f, 1.0f);
    addFloat("centerY", cy_, 0.0f, 1.0f);
    return props;
}
void VignetteEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="amount")setAmount(v.toFloat());else if(k=="radius")setRadius(v.toFloat());else if(k=="feather")setFeather(v.toFloat());else if(k=="centerX")setCenterX(v.toFloat());else if(k=="centerY")setCenterY(v.toFloat());}
void VignetteEffect::syncImpls(){
    auto c=std::make_shared<VignetteCPUImpl>();c->amount_=amount_;c->radius_=radius_;c->feather_=feather_;c->cx_=cx_;c->cy_=cy_;setCPUImpl(c);
    if(auto* g=dynamic_cast<VignetteGPUImpl*>(gpuImpl().get())){g->amount_=amount_;g->radius_=radius_;g->feather_=feather_;g->cx_=cx_;g->cy_=cy_;}
}
} // namespace Artifact
