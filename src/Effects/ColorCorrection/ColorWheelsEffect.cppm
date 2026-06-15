module;
#include <utility>

#include <algorithm>
#include <memory>
#include <vector>
#include <QVariant>
#include <opencv2/opencv.hpp>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module ColorWheelsEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ColorCollection.ColorGrading;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

class ColorWheelsEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ColorWheelsProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        for (int y = 0; y < height; ++y) {
            processor_.process(pixels + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u, width, 1);
        }
    }
};

class ColorWheelsEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ColorWheelsProcessor processor_;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        for (int y = 0; y < height; ++y) {
            processor_.process(pixels + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u, width, 1);
        }
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (wheelType_ == ArtifactCore::ColorWheelType::ShadowsMidtonesHighlights) {
            applyCPU(src, dst);
            return;
        }
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src, dst); return; }
        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if (!paramsCB_) { Diligent::BufferDesc cbDesc; cbDesc.Name="ColorWheels/ParamsCB"; cbDesc.Size=sizeof(ParamsCB); cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src, dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "ColorWheelsParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc desc; desc.name="ColorWheels/PSO"; desc.shaderSource=kColorWheelsHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("ColorWheelsParams", paramsCB_)) { applyCPU(src, dst); return; } pipelineReady_=true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "ColorWheels/InputTexture")) { applyCPU(src, dst); return; }
        Diligent::TextureDesc outDesc=inputTex->GetDesc(); outDesc.Usage=Diligent::USAGE_DEFAULT; outDesc.BindFlags=Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name="ColorWheels/OutputTexture"; Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex; device_->CreateTexture(outDesc,nullptr,&outputTex); if (!outputTex) { applyCPU(src, dst); return; }
        void* mapped=nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { applyCPU(src, dst); return; }
        ParamsCB params{}; params.type=static_cast<int>(wheelType_);
        params.liftR = processor_.wheels().liftR; params.liftG = processor_.wheels().liftG; params.liftB = processor_.wheels().liftB; params.liftMaster = processor_.wheels().liftMaster;
        params.gammaR = processor_.wheels().gammaR; params.gammaG = processor_.wheels().gammaG; params.gammaB = processor_.wheels().gammaB; params.gammaMaster = processor_.wheels().gammaMaster;
        params.gainR = processor_.wheels().gainR; params.gainG = processor_.wheels().gainG; params.gainB = processor_.wheels().gainB; params.gainMaster = processor_.wheels().gainMaster;
        params.offsetR = processor_.wheels().offsetR; params.offsetG = processor_.wheels().offsetG; params.offsetB = processor_.wheels().offsetB; params.offsetMaster = processor_.wheels().offsetMaster;
        std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src, dst); return; }
        auto attribs=ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1); executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst, "ColorWheels/StagingTexture")) { applyCPU(src, dst); return; }
    }

private:
    struct ParamsCB {
        int type = 0;
        float liftR = 0.0f, liftG = 0.0f, liftB = 0.0f, liftMaster = 0.0f;
        float gammaR = 1.0f, gammaG = 1.0f, gammaB = 1.0f, gammaMaster = 1.0f;
        float gainR = 1.0f, gainG = 1.0f, gainB = 1.0f, gainMaster = 1.0f;
        float offsetR = 0.0f, offsetG = 0.0f, offsetB = 0.0f, offsetMaster = 0.0f;
    };
    static constexpr const char* kColorWheelsHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer ColorWheelsParams : register(b0){
    int g_Type; float g_LiftR; float g_LiftG; float g_LiftB; float g_LiftMaster;
    float g_GammaR; float g_GammaG; float g_GammaB; float g_GammaMaster;
    float g_GainR; float g_GainG; float g_GainB; float g_GainMaster;
    float g_OffsetR; float g_OffsetG; float g_OffsetB; float g_OffsetMaster;
};
float luminance(float3 rgb){ return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f)); }
float3 applyLift(float3 rgb){ float lum = luminance(rgb); float oneMinusLum = 1.0f - lum; return rgb + (float3(g_LiftR, g_LiftG, g_LiftB) + g_LiftMaster.xxx) * oneMinusLum; }
float3 applyGamma(float3 rgb){ float3 gamma = float3(g_GammaR, g_GammaG, g_GammaB) * g_GammaMaster.xxx; return float3(gamma.x > 0.0f ? pow(saturate(rgb.x), 1.0f / gamma.x) : rgb.x, gamma.y > 0.0f ? pow(saturate(rgb.y), 1.0f / gamma.y) : rgb.y, gamma.z > 0.0f ? pow(saturate(rgb.z), 1.0f / gamma.z) : rgb.z); }
float3 applyGain(float3 rgb){ float lum = luminance(rgb); return rgb * (float3(g_GainR, g_GainG, g_GainB) + g_GainMaster.xxx * lum); }
float3 applyOffset(float3 rgb){ return rgb + float3(g_OffsetR, g_OffsetG, g_OffsetB) + g_OffsetMaster.xxx; }
[numthreads(8,8,1)] void main(uint3 dtid:SV_DispatchThreadID){ uint w,h; g_OutputTexture.GetDimensions(w,h); if(dtid.x>=w||dtid.y>=h) return; float4 px=g_InputTexture[dtid.xy]; if(g_Type==2) px.rgb = applyOffset(applyGain(applyGamma(applyLift(px.rgb)))); else px.rgb = applyGain(applyGamma(applyLift(px.rgb))); g_OutputTexture[dtid.xy]=float4(saturate(px.rgb), px.a); }
)";
    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex, const char* name){ const auto& img=src.image(); const float* data=img.rgba32fData(); if(!device||!outTex||!data||img.width()<=0||img.height()<=0) return false; Diligent::TextureDesc desc; desc.Type=Diligent::RESOURCE_DIM_TEX_2D; desc.Width=img.width(); desc.Height=img.height(); desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; desc.ArraySize=1; desc.MipLevels=1; desc.SampleCount=1; desc.Usage=Diligent::USAGE_IMMUTABLE; desc.BindFlags=Diligent::BIND_SHADER_RESOURCE; desc.Name=name; Diligent::TextureSubResData sub{}; sub.pData=data; sub.Stride=static_cast<Diligent::Uint64>(img.width())*sizeof(float)*4ull; Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1; device->CreateTexture(desc,&init,outTex); return *outTex!=nullptr; }
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name){ if(!device||!ctx||!src) return false; const auto desc=src->GetDesc(); Diligent::TextureDesc stagingDesc; stagingDesc.Type=Diligent::RESOURCE_DIM_TEX_2D; stagingDesc.Width=desc.Width; stagingDesc.Height=desc.Height; stagingDesc.Format=desc.Format; stagingDesc.ArraySize=1; stagingDesc.MipLevels=1; stagingDesc.SampleCount=1; stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ; stagingDesc.Name=name; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging); if(!staging) return false; Diligent::CopyTextureAttribs copy(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); ctx->CopyTexture(copy); Diligent::MappedTextureSubresource mapped{}; ctx->Flush(); ctx->WaitForIdle(); ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,mapped); if(!mapped.pData||mapped.Stride==0) return false; cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride); dst.image().setFromCVMat(temp); ctx->UnmapTextureSubresource(staging,0,0); return true; }
};

ColorWheelsEffect::ColorWheelsEffect() {
    setEffectID(UniString("effect.colorcorrection.colorwheels"));
    setDisplayName(UniString("Color Wheels"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ColorWheelsEffectCPUImpl>());
    setGPUImpl(std::make_shared<ColorWheelsEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    syncImpls();
}

ColorWheelsEffect::~ColorWheelsEffect() = default;

void ColorWheelsEffect::setLift(float r, float g, float b) {
    wheels_.liftR = std::clamp(r, -2.0f, 2.0f);
    wheels_.liftG = std::clamp(g, -2.0f, 2.0f);
    wheels_.liftB = std::clamp(b, -2.0f, 2.0f);
    syncImpls();
}

void ColorWheelsEffect::setGamma(float r, float g, float b) {
    wheels_.gammaR = std::clamp(r, 0.1f, 5.0f);
    wheels_.gammaG = std::clamp(g, 0.1f, 5.0f);
    wheels_.gammaB = std::clamp(b, 0.1f, 5.0f);
    syncImpls();
}

void ColorWheelsEffect::setGain(float r, float g, float b) {
    wheels_.gainR = std::clamp(r, 0.0f, 4.0f);
    wheels_.gainG = std::clamp(g, 0.0f, 4.0f);
    wheels_.gainB = std::clamp(b, 0.0f, 4.0f);
    syncImpls();
}

void ColorWheelsEffect::setOffset(float r, float g, float b) {
    wheels_.offsetR = std::clamp(r, -2.0f, 2.0f);
    wheels_.offsetG = std::clamp(g, -2.0f, 2.0f);
    wheels_.offsetB = std::clamp(b, -2.0f, 2.0f);
    syncImpls();
}

void ColorWheelsEffect::setLiftMaster(float v) {
    wheels_.liftMaster = std::clamp(v, -2.0f, 2.0f);
    syncImpls();
}

void ColorWheelsEffect::setGammaMaster(float v) {
    wheels_.gammaMaster = std::clamp(v, 0.1f, 5.0f);
    syncImpls();
}

void ColorWheelsEffect::setGainMaster(float v) {
    wheels_.gainMaster = std::clamp(v, 0.0f, 4.0f);
    syncImpls();
}

void ColorWheelsEffect::setOffsetMaster(float v) {
    wheels_.offsetMaster = std::clamp(v, -2.0f, 2.0f);
    syncImpls();
}

void ColorWheelsEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<ColorWheelsEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setWheelType(wheelType_);
        cpu->processor_.wheels() = wheels_;
    }
    if (auto* gpu = dynamic_cast<ColorWheelsEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setWheelType(wheelType_);
        gpu->processor_.wheels() = wheels_;
    }
}

std::vector<AbstractProperty> ColorWheelsEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    auto addFloat = [&props](const char* name, float value) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        props.push_back(prop);
    };

    AbstractProperty modeProp;
    modeProp.setName("Wheel Type");
    modeProp.setType(PropertyType::Integer);
    modeProp.setValue(static_cast<int>(wheelType_));
    props.push_back(modeProp);

    addFloat("Lift Master", wheels_.liftMaster);
    addFloat("Lift R", wheels_.liftR);
    addFloat("Lift G", wheels_.liftG);
    addFloat("Lift B", wheels_.liftB);
    addFloat("Gamma Master", wheels_.gammaMaster);
    addFloat("Gamma R", wheels_.gammaR);
    addFloat("Gamma G", wheels_.gammaG);
    addFloat("Gamma B", wheels_.gammaB);
    addFloat("Gain Master", wheels_.gainMaster);
    addFloat("Gain R", wheels_.gainR);
    addFloat("Gain G", wheels_.gainG);
    addFloat("Gain B", wheels_.gainB);
    addFloat("Offset Master", wheels_.offsetMaster);
    addFloat("Offset R", wheels_.offsetR);
    addFloat("Offset G", wheels_.offsetG);
    addFloat("Offset B", wheels_.offsetB);

    return props;
}

void ColorWheelsEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Wheel Type")) {
        setWheelType(static_cast<ColorWheelType>(value.toInt()));
    } else if (key == QStringLiteral("Lift Master")) {
        setLiftMaster(value.toFloat());
    } else if (key == QStringLiteral("Lift R")) {
        setLift(value.toFloat(), wheels_.liftG, wheels_.liftB);
    } else if (key == QStringLiteral("Lift G")) {
        setLift(wheels_.liftR, value.toFloat(), wheels_.liftB);
    } else if (key == QStringLiteral("Lift B")) {
        setLift(wheels_.liftR, wheels_.liftG, value.toFloat());
    } else if (key == QStringLiteral("Gamma Master")) {
        setGammaMaster(value.toFloat());
    } else if (key == QStringLiteral("Gamma R")) {
        setGamma(value.toFloat(), wheels_.gammaG, wheels_.gammaB);
    } else if (key == QStringLiteral("Gamma G")) {
        setGamma(wheels_.gammaR, value.toFloat(), wheels_.gammaB);
    } else if (key == QStringLiteral("Gamma B")) {
        setGamma(wheels_.gammaR, wheels_.gammaG, value.toFloat());
    } else if (key == QStringLiteral("Gain Master")) {
        setGainMaster(value.toFloat());
    } else if (key == QStringLiteral("Gain R")) {
        setGain(value.toFloat(), wheels_.gainG, wheels_.gainB);
    } else if (key == QStringLiteral("Gain G")) {
        setGain(wheels_.gainR, value.toFloat(), wheels_.gainB);
    } else if (key == QStringLiteral("Gain B")) {
        setGain(wheels_.gainR, wheels_.gainG, value.toFloat());
    } else if (key == QStringLiteral("Offset Master")) {
        setOffsetMaster(value.toFloat());
    } else if (key == QStringLiteral("Offset R")) {
        setOffset(value.toFloat(), wheels_.offsetG, wheels_.offsetB);
    } else if (key == QStringLiteral("Offset G")) {
        setOffset(wheels_.offsetR, value.toFloat(), wheels_.offsetB);
    } else if (key == QStringLiteral("Offset B")) {
        setOffset(wheels_.offsetR, wheels_.offsetG, value.toFloat());
    }
}

} // namespace Artifact
