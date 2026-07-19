module;
#include <algorithm>
#include <cmath>
#include <numbers>
#include <cstdint>
#include <memory>
#include <QString>
#include <QVariant>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Rasterizer.Stripes;

import Artifact.Effect.Abstract;
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

class StripesCPUImpl : public ArtifactEffectImplBase {
public:
    float frequency_=10,angle_=0,thickness_=0.5f,offset_=0;

    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        const auto& si=src.image(); int W=si.width(),H=si.height();
        float f=std::max(frequency_,0.5f),th=std::clamp(thickness_,0.0f,1.0f);
        float rad=angle_*std::numbers::pi_v<float>/180.0f;
        float cs=std::cos(rad),sn=std::sin(rad);
        dst=src.DeepCopy();float* d=dst.image().rgba32fData();
        Parallel::For(0,H,[&](int y){float* o=d+(size_t)y*W*4;
            for(int x=0;x<W;++x){float* p=o+(size_t)x*4;
                float proj=((float)x*cs+(float)y*sn)*f/(float)std::max(W,H)+offset_;
                float v=std::abs(std::fmod(proj,1.0f));
                v=v<th?1.0f:0.0f;
                p[0]=v;p[1]=v;p[2]=v;p[3]=1.0f;
            }
        });
    }
};

class StripesGPUImpl : public ArtifactEffectImplBase {
public:
    float frequency_=10,angle_=0,thickness_=0.5f,offset_=0;
    void applyCPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        StripesCPUImpl cpu; cpu.frequency_=frequency_;cpu.angle_=angle_;cpu.thickness_=thickness_;cpu.offset_=offset_;cpu.applyCPU(src,dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src,ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device; Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if(!acquireSharedRenderDeviceForCurrentBackend(device,context)){applyCPU(src,dst);return;}
        const auto& image=src.image();const float* pixels=image.rgba32fData();if(!pixels||image.width()<=0||image.height()<=0){applyCPU(src,dst);return;}
        Diligent::TextureDesc desc{};desc.Name="Stripes/Input";desc.Type=Diligent::RESOURCE_DIM_TEX_2D;desc.Width=image.width();desc.Height=image.height();desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;desc.MipLevels=1;desc.ArraySize=1;desc.SampleCount=1;desc.Usage=Diligent::USAGE_IMMUTABLE;desc.BindFlags=Diligent::BIND_SHADER_RESOURCE;
        Diligent::TextureSubResData sub{};sub.pData=pixels;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;
        Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(desc,&init,&input);if(!input){applyCPU(src,dst);return;}
        Diligent::TextureDesc outDesc=desc;outDesc.Name="Stripes/Output";outDesc.Usage=Diligent::USAGE_DEFAULT;outDesc.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(outDesc,nullptr,&output);if(!output){applyCPU(src,dst);return;}
        Diligent::BufferDesc cbDesc{};cbDesc.Name="Stripes/Params";cbDesc.Size=sizeof(Params);cbDesc.Usage=Diligent::USAGE_DYNAMIC;cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER;cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(cbDesc,nullptr,&params);if(!params){applyCPU(src,dst);return;}
        void* mapped=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped);if(!mapped){applyCPU(src,dst);return;}Params values{frequency_,angle_,thickness_,offset_};std::memcpy(mapped,&values,sizeof(values));context->UnmapBuffer(params,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"StripesParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        ArtifactCore::GpuContext gpuContext{device,context};ArtifactCore::ComputeExecutor executor{gpuContext};ArtifactCore::ComputePipelineDesc pipeline{};pipeline.name="Stripes/PSO";pipeline.shaderSource=kHlsl;pipeline.entryPoint="main";pipeline.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pipeline.variables=vars;pipeline.variableCount=3;pipeline.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        if(!executor.build(pipeline)||!executor.createShaderResourceBinding(true)||!executor.setBuffer("StripesParams",params)||!executor.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!executor.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}
        executor.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Diligent::TextureDesc stagingDesc=outDesc;stagingDesc.Name="Stripes/Readback";stagingDesc.Usage=Diligent::USAGE_STAGING;stagingDesc.BindFlags=Diligent::BIND_NONE;stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(stagingDesc,nullptr,&staging);if(!staging){applyCPU(src,dst);return;}
        Diligent::CopyTextureAttribs copy(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);context->CopyTexture(copy);context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){applyCPU(src,dst);return;}cv::Mat result(static_cast<int>(outDesc.Height),static_cast<int>(outDesc.Width),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result,image.colorDescriptor());context->UnmapTextureSubresource(staging,0,0);
    }
private:
    struct Params{float frequency,angle,thickness,offset;};
    static constexpr const char* kHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer StripesParams:register(b0){float g_Frequency;float g_Angle;float g_Thickness;float g_Offset;}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float rad=g_Angle*3.14159265359/180.0;float cs=cos(rad),sn=sin(rad);float f=max(g_Frequency,0.5),th=saturate(g_Thickness);float proj=((float)id.x*cs+(float)id.y*sn)*f/(float)max(w,h)+g_Offset;float v=abs(proj-trunc(proj));v=v<th?1.0:0.0;g_OutputTexture[id.xy]=float4(v,v,v,1);}
)";
};

StripesEffect::StripesEffect():ArtifactAbstractEffect(){setPipelineStage(EffectPipelineStage::Rasterizer);syncImpls();setGPUImpl(std::make_shared<StripesGPUImpl>());setComputeMode(ComputeMode::AUTO);}
StripesEffect::~StripesEffect()=default;
float StripesEffect::frequency()const{return frequency_;}void StripesEffect::setFrequency(float v){frequency_=std::max(v,0.5f);syncImpls();}
float StripesEffect::angle()const{return angle_;}void StripesEffect::setAngle(float v){angle_=v;syncImpls();}
float StripesEffect::thickness()const{return thickness_;}void StripesEffect::setThickness(float v){thickness_=std::clamp(v,0.0f,1.0f);syncImpls();}
float StripesEffect::offset()const{return offset_;}void StripesEffect::setOffset(float v){offset_=v;syncImpls();}
std::vector<AbstractProperty> StripesEffect::getProperties()const{
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

    addFloat("frequency", frequency_, 0.5f, 200.0f);
    addFloat("angle", angle_, 0.0f, 360.0f);
    addFloat("thickness", thickness_, 0.0f, 1.0f);
    addFloat("offset", offset_, -10.0f, 10.0f);
    return props;
}
void StripesEffect::setPropertyValue(const UniString& n,const QVariant& v){const QString k=n.toQString();if(k=="frequency")setFrequency(v.toFloat());else if(k=="angle")setAngle(v.toFloat());else if(k=="thickness")setThickness(v.toFloat());else if(k=="offset")setOffset(v.toFloat());}
void StripesEffect::syncImpls(){auto c=std::make_shared<StripesCPUImpl>();c->frequency_=frequency_;c->angle_=angle_;c->thickness_=thickness_;c->offset_=offset_;setCPUImpl(c);if(auto* g=dynamic_cast<StripesGPUImpl*>(gpuImpl().get())){g->frequency_=frequency_;g->angle_=angle_;g->thickness_=thickness_;g->offset_=offset_;}}
} // namespace Artifact
