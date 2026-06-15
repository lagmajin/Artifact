module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <QColor>
#include <random>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Rasterizer.AddNoise;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

using namespace ArtifactCore;

class AddNoiseEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float amount_ = 0.15f;
    float size_ = 1.0f;
    bool colorNoise_ = true;
    bool monochrome_ = false;
    int seed_ = 0;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) {
            dst = src;
            return;
        }
        dst = src;
        cv::Mat mat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());

        std::mt19937 rng(seed_);
        std::uniform_real_distribution<float> dist(-amount_, amount_);

        if (monochrome_) {
            for (int y = 0; y < mat.rows; ++y) {
                for (int x = 0; x < mat.cols; ++x) {
                    float n = dist(rng);
                    cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
                    p[0] = std::clamp(p[0] + n, 0.0f, 1.0f);
                    p[1] = std::clamp(p[1] + n, 0.0f, 1.0f);
                    p[2] = std::clamp(p[2] + n, 0.0f, 1.0f);
                }
            }
        } else if (colorNoise_) {
            for (int y = 0; y < mat.rows; ++y) {
                for (int x = 0; x < mat.cols; ++x) {
                    cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
                    p[0] = std::clamp(p[0] + dist(rng), 0.0f, 1.0f);
                    p[1] = std::clamp(p[1] + dist(rng), 0.0f, 1.0f);
                    p[2] = std::clamp(p[2] + dist(rng), 0.0f, 1.0f);
                }
            }
        } else {
            // luminance-only noise
            for (int y = 0; y < mat.rows; ++y) {
                for (int x = 0; x < mat.cols; ++x) {
                    float luma = mat.at<cv::Vec4f>(y, x)[0] * 0.299f
                               + mat.at<cv::Vec4f>(y, x)[1] * 0.587f
                               + mat.at<cv::Vec4f>(y, x)[2] * 0.114f;
                    float n = dist(rng);
                    float newLuma = std::clamp(luma + n, 0.0f, 1.0f);
                    float diff = newLuma - luma;
                    cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
                    p[0] = std::clamp(p[0] + diff, 0.0f, 1.0f);
                    p[1] = std::clamp(p[1] + diff, 0.0f, 1.0f);
                    p[2] = std::clamp(p[2] + diff, 0.0f, 1.0f);
                }
            }
        }
    }
};

class AddNoiseEffectGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src, dst); return; }
        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if (!paramsCB_) { Diligent::BufferDesc cbDesc; cbDesc.Name="AddNoise/ParamsCB"; cbDesc.Size=sizeof(ParamsCB); cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src, dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "AddNoiseParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc desc; desc.name="AddNoise/PSO"; desc.shaderSource=kAddNoiseHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("AddNoiseParams", paramsCB_)) { applyCPU(src, dst); return; } pipelineReady_=true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "AddNoise/InputTexture")) { applyCPU(src, dst); return; }
        Diligent::TextureDesc outDesc=inputTex->GetDesc(); outDesc.Usage=Diligent::USAGE_DEFAULT; outDesc.BindFlags=Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name="AddNoise/OutputTexture"; Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex; device_->CreateTexture(outDesc,nullptr,&outputTex); if (!outputTex) { applyCPU(src, dst); return; }
        void* mapped=nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { applyCPU(src, dst); return; }
        ParamsCB params{}; params.amount=amount_; params.size=size_; params.colorNoise=colorNoise_ ? 1u : 0u; params.monochrome=monochrome_ ? 1u : 0u; params.seed=seed_; std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src, dst); return; }
        auto attribs=ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1); executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst, "AddNoise/StagingTexture")) { applyCPU(src, dst); return; }
    }
private:
    AddNoiseEffectCPUImpl cpuImpl_;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;
    struct ParamsCB { float amount = 0.15f; float size = 1.0f; unsigned int colorNoise = 1u; unsigned int monochrome = 0u; int seed = 0; float pad = 0.0f; };
    static constexpr const char* kAddNoiseHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer AddNoiseParams : register(b0){ float g_Amount; float g_Size; uint g_ColorNoise; uint g_Monochrome; int g_Seed; float g_Pad; };
uint hash(uint2 p, uint seed){ uint h = p.x * 1664525u + p.y * 1013904223u + seed * 747796405u + 2891336453u; h ^= h >> 16; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16; return h; }
float rand01(uint2 p, uint seed){ return (hash(p, seed) & 0x00FFFFFFu) / 16777215.0f; }
float noiseSigned(uint2 p, uint seed){ return rand01(p, seed) * 2.0f - 1.0f; }
[numthreads(8,8,1)] void main(uint3 dtid:SV_DispatchThreadID){
    uint w,h; g_OutputTexture.GetDimensions(w,h); if(dtid.x>=w||dtid.y>=h) return;
    float4 px = g_InputTexture[dtid.xy];
    float sizeScale = max(g_Size, 0.0001f);
    uint2 coord = uint2((float2)dtid.xy / sizeScale);
    if (g_Monochrome != 0u) {
        float n = noiseSigned(coord, (uint)g_Seed);
        px.rgb = saturate(px.rgb + n * g_Amount);
    } else if (g_ColorNoise != 0u) {
        float3 n = float3(
            noiseSigned(coord, (uint)g_Seed + 1u),
            noiseSigned(coord, (uint)g_Seed + 2u),
            noiseSigned(coord, (uint)g_Seed + 3u));
        px.rgb = saturate(px.rgb + n * g_Amount);
    } else {
        float luma = dot(px.rgb, float3(0.299f, 0.587f, 0.114f));
        float n = noiseSigned(coord, (uint)g_Seed);
        float newLuma = saturate(luma + n * g_Amount);
        px.rgb = saturate(px.rgb + (newLuma - luma));
    }
    g_OutputTexture[dtid.xy] = px;
}
)";
    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex, const char* name){ const auto& img=src.image(); const float* data=img.rgba32fData(); if(!device||!outTex||!data||img.width()<=0||img.height()<=0) return false; Diligent::TextureDesc desc; desc.Type=Diligent::RESOURCE_DIM_TEX_2D; desc.Width=img.width(); desc.Height=img.height(); desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; desc.ArraySize=1; desc.MipLevels=1; desc.SampleCount=1; desc.Usage=Diligent::USAGE_IMMUTABLE; desc.BindFlags=Diligent::BIND_SHADER_RESOURCE; desc.Name=name; Diligent::TextureSubResData sub{}; sub.pData=data; sub.Stride=static_cast<Diligent::Uint64>(img.width())*sizeof(float)*4ull; Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1; device->CreateTexture(desc,&init,outTex); return *outTex!=nullptr; }
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name){ if(!device||!ctx||!src) return false; const auto desc=src->GetDesc(); Diligent::TextureDesc stagingDesc; stagingDesc.Type=Diligent::RESOURCE_DIM_TEX_2D; stagingDesc.Width=desc.Width; stagingDesc.Height=desc.Height; stagingDesc.Format=desc.Format; stagingDesc.ArraySize=1; stagingDesc.MipLevels=1; stagingDesc.SampleCount=1; stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ; stagingDesc.Name=name; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging); if(!staging) return false; Diligent::CopyTextureAttribs copy(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); ctx->CopyTexture(copy); Diligent::MappedTextureSubresource mapped{}; ctx->Flush(); ctx->WaitForIdle(); ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,mapped); if(!mapped.pData||mapped.Stride==0) return false; cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride); dst.image().setFromCVMat(temp); ctx->UnmapTextureSubresource(staging,0,0); return true; }
};

AddNoiseEffect::AddNoiseEffect() {
    setDisplayName(UniString("Add Noise"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<AddNoiseEffectCPUImpl>());
    setGPUImpl(std::make_shared<AddNoiseEffectGPUImpl>());
}
AddNoiseEffect::~AddNoiseEffect() = default;

float AddNoiseEffect::amount() const { return amount_; }
void AddNoiseEffect::setAmount(float v) { amount_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float AddNoiseEffect::size() const { return size_; }
void AddNoiseEffect::setSize(float v) { size_ = std::max(0.1f, v); syncImpls(); }
bool AddNoiseEffect::colorNoise() const { return colorNoise_; }
void AddNoiseEffect::setColorNoise(bool v) { colorNoise_ = v; syncImpls(); }
bool AddNoiseEffect::monochrome() const { return monochrome_; }
void AddNoiseEffect::setMonochrome(bool v) { monochrome_ = v; syncImpls(); }
int AddNoiseEffect::seed() const { return seed_; }
void AddNoiseEffect::setSeed(int v) { seed_ = v; syncImpls(); }

void AddNoiseEffect::syncImpls() {
    if (auto* c = dynamic_cast<AddNoiseEffectCPUImpl*>(cpuImpl().get())) {
        c->amount_ = amount_;
        c->size_ = size_;
        c->colorNoise_ = colorNoise_;
        c->monochrome_ = monochrome_;
        c->seed_ = seed_;
    }
    if (auto* g = dynamic_cast<AddNoiseEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.amount_ = amount_;
        g->cpuImpl_.size_ = size_;
        g->cpuImpl_.colorNoise_ = colorNoise_;
        g->cpuImpl_.monochrome_ = monochrome_;
        g->cpuImpl_.seed_ = seed_;
    }
}

std::vector<AbstractProperty> AddNoiseEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Amount"); a.setType(PropertyType::Float); a.setValue(amount_);
    auto& s = props.emplace_back(); s.setName("Size"); s.setType(PropertyType::Float); s.setValue(size_);
    auto& c = props.emplace_back(); c.setName("Color Noise"); c.setType(PropertyType::Boolean); c.setValue(colorNoise_);
    auto& m = props.emplace_back(); m.setName("Monochrome"); m.setType(PropertyType::Boolean); m.setValue(monochrome_);
    auto& sd = props.emplace_back(); sd.setName("Seed"); sd.setType(PropertyType::Integer); sd.setValue(seed_);
    return props;
}

void AddNoiseEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Amount") setAmount(v.toFloat());
    else if (k == "Size") setSize(v.toFloat());
    else if (k == "Color Noise") setColorNoise(v.toBool());
    else if (k == "Monochrome") setMonochrome(v.toBool());
    else if (k == "Seed") setSeed(v.toInt());
}

}
