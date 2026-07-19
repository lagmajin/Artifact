module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QVariant>
#include <QStringList>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.WhiteBalance;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

static void kelvinToRGB(float kelvin, float& r, float& g, float& b) {
    const float temp = kelvin / 100.0f;

    if (temp <= 66.0f) r = 1.0f;
    else r = std::clamp(1.2929361860603f * std::pow(temp - 60.0f, -0.1332047592f), 0.0f, 1.0f);

    if (temp <= 66.0f) g = std::clamp(0.39008157876902f * std::log(temp) - 0.63184144378863f, 0.0f, 1.0f);
    else g = std::clamp(1.1298908608953f * std::pow(temp - 60.0f, -0.0755148492f), 0.0f, 1.0f);

    if (temp >= 66.0f) b = 1.0f;
    else if (temp <= 19.0f) b = 0.0f;
    else b = std::clamp(0.54320678911019f * std::log(temp - 10.0f) - 1.19625408914f, 0.0f, 1.0f);
}

class WhiteBalanceCPUImpl : public ArtifactEffectImplBase {
public:
    float temperature_ = 6500.0f;
    float tint_ = 0.0f;
    float brightness_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        float refR = 1.0f;
        float refG = 1.0f;
        float refB = 1.0f;
        kelvinToRGB(6500.0f, refR, refG, refB);

        float targetR = 1.0f;
        float targetG = 1.0f;
        float targetB = 1.0f;
        kelvinToRGB(temperature_, targetR, targetG, targetB);

        const float corrR = targetR / std::max(refR, 0.001f);
        const float corrG = targetG / std::max(refG, 0.001f);
        const float corrB = targetB / std::max(refB, 0.001f);
        const float tintG = 1.0f + tint_ * 0.5f;
        const float tintM = 1.0f - tint_ * 0.5f;
        const float brightMul = std::pow(2.0f, brightness_);

        Parallel::For(0, height, [&](int y) {
            for (int x = 0; x < width; ++x) {
                float* p = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                p[2] = std::clamp(p[2] * corrR * tintM * brightMul, 0.0f, 1.0f);
                p[1] = std::clamp(p[1] * corrG * tintG * brightMul, 0.0f, 1.0f);
                p[0] = std::clamp(p[0] * corrB * tintM * brightMul, 0.0f, 1.0f);
            }
        });
    }
};

class WhiteBalanceGPUImpl : public ArtifactEffectImplBase {
public:
    float temperature_ = 6500.0f;
    float tint_ = 0.0f;
    float brightness_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;if(!acquireSharedRenderDeviceForCurrentBackend(device,context)){applyCPU(src,dst);return;}
        const auto& image=src.image();const float* pixels=image.rgba32fData();if(!pixels||image.width()<=0||image.height()<=0){applyCPU(src,dst);return;}
        float rr=1,rg=1,rb=1,tr=1,tg=1,tb=1;kelvinToRGB(6500,rr,rg,rb);kelvinToRGB(temperature_,tr,tg,tb);
        Params values{tr/std::max(rr,0.001f),tg/std::max(rg,0.001f),tb/std::max(rb,0.001f),1.0f+tint_*0.5f,1.0f-tint_*0.5f,std::pow(2.0f,brightness_)};
        Diligent::TextureDesc desc{};desc.Name="WhiteBalance/Input";desc.Type=Diligent::RESOURCE_DIM_TEX_2D;desc.Width=image.width();desc.Height=image.height();desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;desc.MipLevels=1;desc.ArraySize=1;desc.SampleCount=1;desc.Usage=Diligent::USAGE_IMMUTABLE;desc.BindFlags=Diligent::BIND_SHADER_RESOURCE;Diligent::TextureSubResData sub{};sub.pData=pixels;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(desc,&init,&input);if(!input){applyCPU(src,dst);return;}
        Diligent::TextureDesc outDesc=desc;outDesc.Name="WhiteBalance/Output";outDesc.Usage=Diligent::USAGE_DEFAULT;outDesc.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(outDesc,nullptr,&output);if(!output){applyCPU(src,dst);return;}
        Diligent::BufferDesc cbDesc{};cbDesc.Name="WhiteBalance/Params";cbDesc.Size=sizeof(Params);cbDesc.Usage=Diligent::USAGE_DYNAMIC;cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER;cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(cbDesc,nullptr,&params);if(!params){applyCPU(src,dst);return;}void* mapped=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped);if(!mapped){applyCPU(src,dst);return;}std::memcpy(mapped,&values,sizeof(values));context->UnmapBuffer(params,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"WhiteBalanceParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gpuContext{device,context};ArtifactCore::ComputeExecutor executor{gpuContext};ArtifactCore::ComputePipelineDesc pipeline{};pipeline.name="WhiteBalance/PSO";pipeline.shaderSource=kHlsl;pipeline.entryPoint="main";pipeline.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pipeline.variables=vars;pipeline.variableCount=3;pipeline.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!executor.build(pipeline)||!executor.createShaderResourceBinding(true)||!executor.setBuffer("WhiteBalanceParams",params)||!executor.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!executor.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}executor.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Diligent::TextureDesc stagingDesc=outDesc;stagingDesc.Name="WhiteBalance/Readback";stagingDesc.Usage=Diligent::USAGE_STAGING;stagingDesc.BindFlags=Diligent::BIND_NONE;stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(stagingDesc,nullptr,&staging);if(!staging){applyCPU(src,dst);return;}Diligent::CopyTextureAttribs copy(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);context->CopyTexture(copy);context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){applyCPU(src,dst);return;}cv::Mat result(static_cast<int>(outDesc.Height),static_cast<int>(outDesc.Width),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result,image.colorDescriptor());context->UnmapTextureSubresource(staging,0,0);
    }

private:
    struct Params{float corrR,corrG,corrB,tintG,tintM,brightMul;float pad0=0.0f,pad1=0.0f;};
    static constexpr const char* kHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer WhiteBalanceParams:register(b0){float g_CorrR;float g_CorrG;float g_CorrB;float g_TintG;float g_TintM;float g_BrightMul;float2 g_Pad;}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float4 p=g_InputTexture[id.xy];p.r=saturate(p.r*g_CorrR*g_TintM*g_BrightMul);p.g=saturate(p.g*g_CorrG*g_TintG*g_BrightMul);p.b=saturate(p.b*g_CorrB*g_TintM*g_BrightMul);g_OutputTexture[id.xy]=p;}
)";
    WhiteBalanceCPUImpl cpuImpl_;
};

WhiteBalanceEffect::WhiteBalanceEffect() {
    setDisplayName(UniString("White Balance"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<WhiteBalanceCPUImpl>());
    setGPUImpl(std::make_shared<WhiteBalanceGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

WhiteBalanceEffect::~WhiteBalanceEffect() = default;

void WhiteBalanceEffect::syncImpls() {
    if (auto cpu = std::dynamic_pointer_cast<WhiteBalanceCPUImpl>(cpuImpl())) {
        cpu->temperature_ = temperature_;
        cpu->tint_ = tint_;
        cpu->brightness_ = brightness_;
    }
    if (auto gpu = std::dynamic_pointer_cast<WhiteBalanceGPUImpl>(gpuImpl())) {
        gpu->temperature_ = temperature_;
        gpu->tint_ = tint_;
        gpu->brightness_ = brightness_;
    }
}

void WhiteBalanceEffect::setPreset(const QString& preset) {
    if (preset == QStringLiteral("Daylight")) setTemperature(5500.0f);
    else if (preset == QStringLiteral("Cloudy")) setTemperature(6500.0f);
    else if (preset == QStringLiteral("Shade")) setTemperature(7500.0f);
    else if (preset == QStringLiteral("Tungsten")) setTemperature(3200.0f);
    else if (preset == QStringLiteral("Fluorescent")) setTemperature(4000.0f);
    else if (preset == QStringLiteral("Flash")) setTemperature(6000.0f);
}

std::vector<AbstractProperty> WhiteBalanceEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    AbstractProperty tempProp;
    tempProp.setName("Temperature (K)");
    tempProp.setType(PropertyType::Float);
    tempProp.setValue(temperature_);
    tempProp.setDisplayPriority(-10);
    props.push_back(tempProp);

    AbstractProperty tintProp;
    tintProp.setName("Tint");
    tintProp.setType(PropertyType::Float);
    tintProp.setValue(tint_);
    tintProp.setDisplayPriority(0);
    props.push_back(tintProp);

    AbstractProperty brightnessProp;
    brightnessProp.setName("Brightness");
    brightnessProp.setType(PropertyType::Float);
    brightnessProp.setValue(brightness_);
    brightnessProp.setDisplayPriority(10);
    props.push_back(brightnessProp);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(0);
    presetProp.setDisplayPriority(-20);
    props.push_back(presetProp);

    return props;
}

void WhiteBalanceEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Temperature (K)")) setTemperature(value.toFloat());
    else if (key == QStringLiteral("Tint")) setTint(value.toFloat());
    else if (key == QStringLiteral("Brightness")) setBrightness(value.toFloat());
    else if (key == QStringLiteral("Preset")) {
        const QStringList presets = {
            QStringLiteral("Custom"),
            QStringLiteral("Daylight"),
            QStringLiteral("Cloudy"),
            QStringLiteral("Shade"),
            QStringLiteral("Tungsten"),
            QStringLiteral("Fluorescent"),
            QStringLiteral("Flash")
        };
        const int idx = value.toInt();
        if (idx > 0 && idx < presets.size()) {
            setPreset(presets[idx]);
        }
    }
}

} // namespace Artifact
