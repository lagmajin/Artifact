module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QVariant>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module GradientRampEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.GradientRamp;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

class GradientRampEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::GradientRampProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        const int width = dst.image().width();
        const int height = dst.image().height();
        ArtifactCore::Parallel::For(0, height, [&](int y) {
            processor_.applyRow(pixels, width, height, y);
        });
    }
};

class GradientRampEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::GradientRampProcessor processor_;
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
        ArtifactCore::Parallel::For(0, height, [&](int y) {
            processor_.applyRow(pixels, width, height, y);
        });
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src, dst); return; }
        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if (!paramsCB_) {
            Diligent::BufferDesc d; d.Name = "GradientRamp/ParamsCB"; d.Size = sizeof(ParamsCB);
            d.Usage = Diligent::USAGE_DYNAMIC; d.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
            d.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(d, nullptr, &paramsCB_);
        }
        if (!paramsCB_) { applyCPU(src, dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "GradientRampParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc d; d.name = "GradientRamp/PSO"; d.shaderSource = kHlsl;
            d.entryPoint = "main"; d.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            d.variables = vars; d.variableCount = 3;
            d.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor->build(d) || !executor->createShaderResourceBinding(true) ||
                !executor->setBuffer("GradientRampParams", paramsCB_)) { applyCPU(src, dst); return; }
            pipelineReady_ = true;
        }
        Diligent::RefCntAutoPtr<Diligent::ITexture> input;
        if (!createTexture(src, &input, "GradientRamp/Input")) { applyCPU(src, dst); return; }
        auto outDesc = input->GetDesc(); outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "GradientRamp/Output";
        Diligent::RefCntAutoPtr<Diligent::ITexture> output; device_->CreateTexture(outDesc, nullptr, &output);
        if (!output) { applyCPU(src, dst); return; }
        const auto& s = processor_.settings(); ParamsCB p{};
        p.startR = s.startColor.redF(); p.startG = s.startColor.greenF(); p.startB = s.startColor.blueF();
        p.endR = s.endColor.redF(); p.endG = s.endColor.greenF(); p.endB = s.endColor.blueF();
        p.startX = s.startX; p.startY = s.startY; p.endX = s.endX; p.endY = s.endY;
        p.opacity = s.opacity; p.preserveAlpha = s.preserveAlpha ? 1.0f : 0.0f;
        void* mapped = nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) { applyCPU(src, dst); return; } std::memcpy(mapped, &p, sizeof(p)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor->setTextureView("g_InputTexture", input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor->setTextureView("g_OutputTexture", output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src, dst); return; }
        executor->dispatch(context_, ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readback(device_, context_, output, dst, "GradientRamp/Readback")) { applyCPU(src, dst); }
    }

private:
    struct ParamsCB { float startR, startG, startB, startX; float endR, endG, endB, startY; float endX, endY, opacity, preserveAlpha; };
    static constexpr const char* kHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer GradientRampParams : register(b0) { float3 g_Start; float g_StartX; float3 g_End; float g_StartY; float g_EndX; float g_EndY; float g_Opacity; float g_PreserveAlpha; };
[numthreads(8,8,1)] void main(uint3 id : SV_DispatchThreadID) { uint w,h; g_OutputTexture.GetDimensions(w,h); if(id.x>=w||id.y>=h)return; float2 p=float2((float)id.x/max(1.0,w-1.0),(float)id.y/max(1.0,h-1.0)); float2 a=float2(g_StartX,g_StartY), b=float2(g_EndX,g_EndY); float2 d=b-a; float t=saturate(dot(p-a,d)/max(dot(d,d),0.000001)); float4 px=g_InputTexture[id.xy]; px.rgb=px.rgb*(1.0-g_Opacity)+lerp(g_Start,g_End,t)*g_Opacity; if(g_PreserveAlpha<0.5)px.a=px.a*(1.0-g_Opacity)+g_Opacity; g_OutputTexture[id.xy]=px; }
)";
    bool createTexture(const ImageF32x4RGBAWithCache& src, Diligent::ITexture** out, const char* name) { const auto& i=src.image(); const float* data=i.rgba32fData(); if(!out||!data||i.width()<=0||i.height()<=0)return false; Diligent::TextureDesc d; d.Type=Diligent::RESOURCE_DIM_TEX_2D; d.Width=i.width(); d.Height=i.height(); d.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; d.ArraySize=1; d.MipLevels=1; d.SampleCount=1; d.Usage=Diligent::USAGE_IMMUTABLE; d.BindFlags=Diligent::BIND_SHADER_RESOURCE; d.Name=name; Diligent::TextureSubResData sub{}; sub.pData=data; sub.Stride=static_cast<Diligent::Uint64>(i.width())*sizeof(float)*4ull; Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1; device_->CreateTexture(d,&init,out); return *out!=nullptr; }
    static bool readback(Diligent::IRenderDevice* dev, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name) { if(!dev||!ctx||!src)return false; auto d=src->GetDesc(); Diligent::TextureDesc s; s.Type=Diligent::RESOURCE_DIM_TEX_2D;s.Width=d.Width;s.Height=d.Height;s.Format=d.Format;s.ArraySize=1;s.MipLevels=1;s.SampleCount=1;s.Usage=Diligent::USAGE_STAGING;s.CPUAccessFlags=Diligent::CPU_ACCESS_READ;s.Name=name;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;dev->CreateTexture(s,nullptr,&staging);if(!staging)return false;ctx->CopyTexture(Diligent::CopyTextureAttribs(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));ctx->Flush();ctx->WaitForIdle();Diligent::MappedTextureSubresource m{};ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,m);if(!m.pData||!m.Stride)return false;cv::Mat temp((int)d.Height,(int)d.Width,CV_32FC4,m.pData,m.Stride);dst.image().setFromCVMat(temp);ctx->UnmapTextureSubresource(staging,0,0);return true; }
};

GradientRampEffect::GradientRampEffect() {
    setEffectID(UniString("effect.colorcorrection.gradientramp"));
    setDisplayName(UniString("Gradient Ramp"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<GradientRampEffectCPUImpl>());
    setGPUImpl(std::make_shared<GradientRampEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

GradientRampEffect::~GradientRampEffect() = default;

void GradientRampEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Sunrise:
        settings_ = ArtifactCore::GradientRampSettings::sunrise();
        break;
    case Preset::Ocean:
        settings_ = ArtifactCore::GradientRampSettings::ocean();
        break;
    case Preset::Neon:
        settings_ = ArtifactCore::GradientRampSettings::neon();
        break;
    case Preset::Mono:
        settings_ = ArtifactCore::GradientRampSettings::mono();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void GradientRampEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 3);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void GradientRampEffect::setStartColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.startColor = color;
    syncImpls();
}

void GradientRampEffect::setEndColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.endColor = color;
    syncImpls();
}

void GradientRampEffect::setStartPoint(float x, float y) {
    preset_ = Preset::Custom;
    settings_.startX = std::clamp(x, 0.0f, 1.0f);
    settings_.startY = std::clamp(y, 0.0f, 1.0f);
    syncImpls();
}

void GradientRampEffect::setEndPoint(float x, float y) {
    preset_ = Preset::Custom;
    settings_.endX = std::clamp(x, 0.0f, 1.0f);
    settings_.endY = std::clamp(y, 0.0f, 1.0f);
    syncImpls();
}

void GradientRampEffect::setOpacity(float value) {
    preset_ = Preset::Custom;
    settings_.opacity = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void GradientRampEffect::setPreserveAlpha(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveAlpha = value;
    syncImpls();
}

void GradientRampEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<GradientRampEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<GradientRampEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> GradientRampEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(8);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    auto addColor = [&props](const char* name, const QColor& color, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Color);
        prop.setColorValue(color);
        prop.setValue(color);
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };
    auto addFloat = [&props](const char* name, float value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    addColor("Start Color", settings_.startColor, -20);
    addColor("End Color", settings_.endColor, -19);
    addFloat("Start X", settings_.startX, -10);
    addFloat("Start Y", settings_.startY, -9);
    addFloat("End X", settings_.endX, -8);
    addFloat("End Y", settings_.endY, -7);
    addFloat("Opacity", settings_.opacity, 0);

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Alpha");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveAlpha);
    preserveProp.setDisplayPriority(10);
    props.push_back(preserveProp);

    return props;
}

void GradientRampEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Start Color")) {
        setStartColor(value.value<QColor>());
    } else if (key == QStringLiteral("End Color")) {
        setEndColor(value.value<QColor>());
    } else if (key == QStringLiteral("Start X")) {
        setStartPoint(value.toFloat(), settings_.startY);
    } else if (key == QStringLiteral("Start Y")) {
        setStartPoint(settings_.startX, value.toFloat());
    } else if (key == QStringLiteral("End X")) {
        setEndPoint(value.toFloat(), settings_.endY);
    } else if (key == QStringLiteral("End Y")) {
        setEndPoint(settings_.endX, value.toFloat());
    } else if (key == QStringLiteral("Opacity")) {
        setOpacity(value.toFloat());
    } else if (key == QStringLiteral("Preserve Alpha")) {
        setPreserveAlpha(value.toBool());
    }
}

} // namespace Artifact
