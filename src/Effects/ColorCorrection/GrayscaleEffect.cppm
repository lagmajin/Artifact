module;
#include <algorithm>
#include <cmath>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <memory>
#include <QVariant>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module GrayscaleEffect;

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

namespace {
constexpr float kLumaR = 0.299f;
constexpr float kLumaG = 0.587f;
constexpr float kLumaB = 0.114f;
constexpr float kLinearR = 0.2126f;
constexpr float kLinearG = 0.7152f;
constexpr float kLinearB = 0.0722f;

float srgbToLinear(float v)
{
    v = std::clamp(v, 0.0f, 1.0f);
    if (v <= 0.04045f) {
        return v / 12.92f;
    }
    return std::pow((v + 0.055f) / 1.055f, 2.4f);
}

float linearToSrgb(float v)
{
    v = std::clamp(v, 0.0f, 1.0f);
    if (v <= 0.0031308f) {
        return v * 12.92f;
    }
    return 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

const char* kGrayscaleHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer GrayscaleParams : register(b0)
{
    float g_Strength;
    float g_Mode;
    float3 g_Pad;
};

float srgbToLinear(float v)
{
    return (v <= 0.04045f) ? (v / 12.92f) : pow((v + 0.055f) / 1.055f, 2.4f);
}

float linearToSrgb(float v)
{
    return (v <= 0.0031308f) ? (v * 12.92f) : (1.055f * pow(v, 1.0f / 2.4f) - 0.055f);
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;

    float4 c = g_InputTexture[dtid.xy];
    float gray = 0.0f;
    if (g_Mode < 0.5f) {
        gray = dot(c.rgb, float3(0.299f, 0.587f, 0.114f));
    } else if (g_Mode < 1.5f) {
        float3 linearRgb = float3(srgbToLinear(c.r), srgbToLinear(c.g), srgbToLinear(c.b));
        float linearGray = dot(linearRgb, float3(0.2126f, 0.7152f, 0.0722f));
        gray = linearToSrgb(linearGray);
    } else {
        gray = (max(c.r, max(c.g, c.b)) + min(c.r, min(c.g, c.b))) * 0.5f;
    }
    c.rgb = lerp(c.rgb, gray.xxx, saturate(g_Strength));
    g_OutputTexture[dtid.xy] = c;
}
)";
} // namespace

class GrayscaleEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float strength_ = 1.0f;
    int mode_ = 1;

    float grayscaleForPixel(float r, float g, float b) const {
        switch (mode_) {
        case 0:
            return r * kLumaR + g * kLumaG + b * kLumaB;
        case 1: {
            const float linearR = srgbToLinear(r);
            const float linearG = srgbToLinear(g);
            const float linearB = srgbToLinear(b);
            return linearToSrgb(linearR * kLinearR + linearG * kLinearG + linearB * kLinearB);
        }
        case 2:
        default:
            return (std::max({r, g, b}) + std::min({r, g, b})) * 0.5f;
        }
    }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        const float strength = std::clamp(strength_, 0.0f, 1.0f);

        Parallel::For(0, height, [&](int y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                const float gray = grayscaleForPixel(pixel[0], pixel[1], pixel[2]);
                pixel[0] = std::lerp(pixel[0], gray, strength);
                pixel[1] = std::lerp(pixel[1], gray, strength);
                pixel[2] = std::lerp(pixel[2], gray, strength);
            }
        });
    }
};

class GrayscaleEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float strength_ = 1.0f;
    int mode_ = 1;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.strength_ = strength_;
        cpuImpl_.mode_ = mode_;
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if(!acquireSharedRenderDeviceForCurrentBackend(device,context)){applyCPU(src,dst);return;}
        const auto& image=src.image();const float* pixels=image.rgba32fData();if(!pixels||image.width()<=0||image.height()<=0){applyCPU(src,dst);return;}
        Diligent::TextureDesc desc{};desc.Name="Grayscale/Input";desc.Type=Diligent::RESOURCE_DIM_TEX_2D;desc.Width=image.width();desc.Height=image.height();desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;desc.MipLevels=1;desc.ArraySize=1;desc.SampleCount=1;desc.Usage=Diligent::USAGE_IMMUTABLE;desc.BindFlags=Diligent::BIND_SHADER_RESOURCE;Diligent::TextureSubResData sub{};sub.pData=pixels;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(desc,&init,&input);if(!input){applyCPU(src,dst);return;}
        Diligent::TextureDesc outDesc=desc;outDesc.Name="Grayscale/Output";outDesc.Usage=Diligent::USAGE_DEFAULT;outDesc.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(outDesc,nullptr,&output);if(!output){applyCPU(src,dst);return;}
        Diligent::BufferDesc cbDesc{};cbDesc.Name="Grayscale/Params";cbDesc.Size=sizeof(Params);cbDesc.Usage=Diligent::USAGE_DYNAMIC;cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER;cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(cbDesc,nullptr,&params);if(!params){applyCPU(src,dst);return;}void* mapped=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped);if(!mapped){applyCPU(src,dst);return;}Params values{strength_,static_cast<float>(mode_)};std::memcpy(mapped,&values,sizeof(values));context->UnmapBuffer(params,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"GrayscaleParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gpuContext{device,context};ArtifactCore::ComputeExecutor executor{gpuContext};ArtifactCore::ComputePipelineDesc pipeline{};pipeline.name="Grayscale/PSO";pipeline.shaderSource=kGrayscaleHlsl;pipeline.entryPoint="main";pipeline.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pipeline.variables=vars;pipeline.variableCount=3;pipeline.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!executor.build(pipeline)||!executor.createShaderResourceBinding(true)||!executor.setBuffer("GrayscaleParams",params)||!executor.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!executor.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}executor.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Diligent::TextureDesc stagingDesc=outDesc;stagingDesc.Name="Grayscale/Readback";stagingDesc.Usage=Diligent::USAGE_STAGING;stagingDesc.BindFlags=Diligent::BIND_NONE;stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(stagingDesc,nullptr,&staging);if(!staging){applyCPU(src,dst);return;}Diligent::CopyTextureAttribs copy(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);context->CopyTexture(copy);context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){applyCPU(src,dst);return;}cv::Mat result(static_cast<int>(outDesc.Height),static_cast<int>(outDesc.Width),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result);context->UnmapTextureSubresource(staging,0,0);
    }

private:
    struct Params{float strength,mode,pad0=0.0f,pad1=0.0f;};
    GrayscaleEffectCPUImpl cpuImpl_;
};

GrayscaleEffect::GrayscaleEffect() {
    setEffectID(UniString("effect.colorcorrection.grayscale"));
    setDisplayName(UniString("Grayscale"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<GrayscaleEffectCPUImpl>());
    setGPUImpl(std::make_shared<GrayscaleEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

GrayscaleEffect::~GrayscaleEffect() = default;

void GrayscaleEffect::setStrength(float value) {
    strength_ = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void GrayscaleEffect::setMode(int value) {
    mode_ = static_cast<Mode>(std::clamp(value, 0, 2));
    syncImpls();
}

void GrayscaleEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<GrayscaleEffectCPUImpl*>(cpuImpl().get())) {
        cpu->strength_ = strength_;
        cpu->mode_ = static_cast<int>(mode_);
    }
    if (auto* gpu = dynamic_cast<GrayscaleEffectGPUImpl*>(gpuImpl().get())) {
        gpu->strength_ = strength_;
        gpu->mode_ = static_cast<int>(mode_);
    }
}

std::vector<AbstractProperty> GrayscaleEffect::getProperties() const {
    std::vector<AbstractProperty> props(2);
    props[0].setName("Mode");
    props[0].setType(PropertyType::Integer);
    props[0].setValue(QVariant(mode()));
    props[1].setName("Strength");
    props[1].setType(PropertyType::Float);
    props[1].setValue(QVariant(static_cast<double>(strength_)));
    return props;
}

void GrayscaleEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    if (name == "Mode") {
        setMode(value.toInt());
    } else if (name == "Strength") {
        setStrength(value.toFloat());
    }
}

} // namespace Artifact
