module;
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <QVariant>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Keying.IBKKeyer;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import ImageProcessing:IBKKeyer;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {
using namespace ArtifactCore;

static void applyIBKPreview(ImageF32x4_RGBA& image, int mode) {
    if (mode == 0) return;
    float* data = image.rgba32fData();
    if (!data) return;
    const std::size_t count = static_cast<std::size_t>(image.width()) *
                              static_cast<std::size_t>(image.height());
    for (std::size_t i = 0; i < count; ++i) {
        float* pixel = data + i * 4;
        const float alpha = std::clamp(pixel[3], 0.0f, 1.0f);
        if (mode == 1) {
            pixel[0] = alpha;
            pixel[1] = alpha;
            pixel[2] = alpha;
            pixel[3] = 1.0f;
        } else {
            pixel[0] *= alpha;
            pixel[1] *= alpha;
            pixel[2] *= alpha;
            pixel[3] = 1.0f;
        }
    }
}

void IBKKeyerEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src,
                                     ImageF32x4RGBAWithCache& dst) {
    const auto& source = src.image();
    const float* foreground = source.rgba32fData();
    const float* cleanPlate = cleanPlate_.rgba32fData();
    if (!foreground || !cleanPlate || source.width() != cleanPlate_.width() ||
        source.height() != cleanPlate_.height()) {
        dst = src;
        return;
    }

    ImageF32x4_RGBA result = source;
    auto* output = result.rgba32fData();
    if (!output) {
        dst = src;
        return;
    }
    const ArtifactCore::Keying::IBKBuffers buffers{
        foreground, cleanPlate, output, source.width(), source.height()};
    if (!ArtifactCore::Keying::processIBK(buffers, params_)) {
        dst = src;
        return;
    }
    applyIBKPreview(result, previewMode_);
    dst = ImageF32x4RGBAWithCache(result);
}

class IBKKeyerEffectGPUImpl final : public ArtifactEffectImplBase {
public:
    ArtifactCore::Keying::IBKParams params_{};
    ArtifactCore::ImageF32x4_RGBA cleanPlate_;
    int previewMode_ = 0;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        IBKKeyerEffectCPUImpl fallback;
        fallback.setParams(params_);
        fallback.setCleanPlate(cleanPlate_);
        fallback.setPreviewMode(previewMode_);
        fallback.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
        Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if (!acquireSharedRenderDeviceForCurrentBackend(device, context) ||
            cleanPlate_.width() != src.image().width() ||
            cleanPlate_.height() != src.image().height()) {
            applyCPU(src, dst);
            return;
        }
        const float* plateData = cleanPlate_.rgba32fData();
        if (!plateData) { applyCPU(src, dst); return; }

        Diligent::RefCntAutoPtr<Diligent::ITexture> foreground;
        Diligent::RefCntAutoPtr<Diligent::ITexture> plate;
        if (!createTexture(src.image().rgba32fData(), src.image().width(),
                           src.image().height(), device, &foreground,
                           "IBK/Foreground") ||
            !createTexture(plateData, cleanPlate_.width(), cleanPlate_.height(),
                           device, &plate, "IBK/CleanPlate")) {
            applyCPU(src, dst);
            return;
        }
        Diligent::TextureDesc outputDesc = foreground->GetDesc();
        outputDesc.Name = "IBK/Output";
        outputDesc.Usage = Diligent::USAGE_DEFAULT;
        outputDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE |
                               Diligent::BIND_UNORDERED_ACCESS;
        Diligent::RefCntAutoPtr<Diligent::ITexture> output;
        device->CreateTexture(outputDesc, nullptr, &output);
        if (!output) { applyCPU(src, dst); return; }

        Diligent::BufferDesc bufferDesc{};
        bufferDesc.Name = "IBK/Params";
        bufferDesc.Size = sizeof(ArtifactCore::Keying::IBKGpuParams);
        bufferDesc.Usage = Diligent::USAGE_DYNAMIC;
        bufferDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        bufferDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsBuffer;
        device->CreateBuffer(bufferDesc, nullptr, &paramsBuffer);
        if (!paramsBuffer) { applyCPU(src, dst); return; }
        void* mapped = nullptr;
        context->MapBuffer(paramsBuffer, Diligent::MAP_WRITE,
                           Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) { applyCPU(src, dst); return; }
        const auto gpuParams = ArtifactCore::Keying::IBKGpuParams::fromParams(params_);
        std::memcpy(mapped, &gpuParams, sizeof(gpuParams));
        context->UnmapBuffer(paramsBuffer, Diligent::MAP_WRITE);

        ArtifactCore::GpuContext gpuContext{device, context};
        ArtifactCore::ComputeExecutor executor{gpuContext};
        static Diligent::ShaderResourceVariableDesc variables[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "Params", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "ForegroundTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "CleanPlateTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        ArtifactCore::ComputePipelineDesc pipeline{};
        pipeline.name = "IBK/PSO";
        pipeline.shaderSource = kShader;
        pipeline.entryPoint = "main";
        pipeline.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
        pipeline.variables = variables;
        pipeline.variableCount = 4;
        pipeline.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        if (!executor.build(pipeline) || !executor.createShaderResourceBinding(true) ||
            !executor.setBuffer("Params", paramsBuffer) ||
            !executor.setTextureView("ForegroundTexture",
                foreground->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor.setTextureView("CleanPlateTexture",
                plate->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor.setTextureView("OutputTexture",
                output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }
        executor.dispatch(context,
            ArtifactCore::ComputeExecutor::makeDispatchAttribs(
                outputDesc.Width, outputDesc.Height, 1, 16, 16, 1),
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readback(output, device, context, dst, src.image().colorDescriptor()))
            applyCPU(src, dst);
        else
            applyIBKPreview(dst.image(), previewMode_);
    }

private:
    static bool createTexture(const float* data, int width, int height,
                              Diligent::IRenderDevice* device,
                              Diligent::ITexture** output, const char* name) {
        if (!data || width <= 0 || height <= 0 || !device || !output) return false;
        Diligent::TextureDesc desc{};
        desc.Name = name;
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<Diligent::Uint32>(width);
        desc.Height = static_cast<Diligent::Uint32>(height);
        desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.SampleCount = 1;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        Diligent::TextureSubResData sub{};
        sub.pData = data;
        sub.Stride = static_cast<Diligent::Uint64>(width) * sizeof(float) * 4ull;
        Diligent::TextureData init{};
        init.pSubResources = &sub;
        init.NumSubresources = 1;
        device->CreateTexture(desc, &init, output);
        return *output != nullptr;
    }

    static bool readback(Diligent::ITexture* source,
                         Diligent::IRenderDevice* device,
                         Diligent::IDeviceContext* context,
                         ImageF32x4RGBAWithCache& destination,
                         const ArtifactCore::SurfaceColorDescriptor& descriptor) {
        if (!source || !device || !context) return false;
        const auto desc = source->GetDesc();
        Diligent::TextureDesc stagingDesc = desc;
        stagingDesc.Name = "IBK/Readback";
        stagingDesc.Usage = Diligent::USAGE_STAGING;
        stagingDesc.BindFlags = Diligent::BIND_NONE;
        stagingDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
        Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
        device->CreateTexture(stagingDesc, nullptr, &staging);
        if (!staging) return false;
        Diligent::CopyTextureAttribs copy(source,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->CopyTexture(copy);
        context->Flush();
        context->WaitForIdle();
        Diligent::MappedTextureSubresource mapped{};
        context->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ,
                                        Diligent::MAP_FLAG_NONE, nullptr, mapped);
        if (!mapped.pData || mapped.Stride == 0) {
            context->UnmapTextureSubresource(staging, 0, 0);
            return false;
        }
        cv::Mat image(static_cast<int>(desc.Height), static_cast<int>(desc.Width),
                      CV_32FC4, mapped.pData, mapped.Stride);
        destination.image().setFromCVMat(image, descriptor);
        context->UnmapTextureSubresource(staging, 0, 0);
        return true;
    }

    static constexpr const char* kShader = R"(
Texture2D<float4> ForegroundTexture : register(t0);
Texture2D<float4> CleanPlateTexture : register(t1);
RWTexture2D<float4> OutputTexture : register(u0);
cbuffer Params : register(b0) { float ScreenCorrection; float CoreMatteClip;
float EdgeMatteSoftness; float DespillStrength; float GarbageMatteGamma;
float DetailRecovery; uint ErodePixels; uint DilatePixels; };
float baseMatte(uint2 p) {
 float4 fg=ForegroundTexture[p], plate=CleanPlateTexture[p];
 float3 corrected=max(fg.rgb*max(ScreenCorrection,0),0);
 float raw=1-exp(-length(corrected-plate.rgb)/0.25);
 float core=saturate(raw-saturate(CoreMatteClip));
 float edge=smoothstep(0,max(EdgeMatteSoftness,1e-5),raw*(1-core));
 return pow(saturate(core+edge*saturate(DetailRecovery)),max(GarbageMatteGamma,1e-5));
}
float erodedMatte(uint2 p,uint2 size,uint radius) {
 float value=1.0;
 for(int dy=-8;dy<=8;++dy) for(int dx=-8;dx<=8;++dx) {
  if(max(abs(dx),abs(dy))>int(radius)) continue;
  int2 q=clamp(int2(p)+int2(dx,dy),int2(0,0),int2(size)-1);
  value=min(value,baseMatte(uint2(q)));
 }
 return value;
}
float filteredMatte(uint2 p,uint2 size) {
 uint erode=min(ErodePixels,8u), dilate=min(DilatePixels,8u);
 float value=erodedMatte(p,size,erode);
 if(dilate==0) return value;
 for(int dy=-8;dy<=8;++dy) for(int dx=-8;dx<=8;++dx) {
  if(max(abs(dx),abs(dy))>int(dilate)) continue;
  int2 q=clamp(int2(p)+int2(dx,dy),int2(0,0),int2(size)-1);
  value=max(value,erodedMatte(uint2(q),size,erode));
 }
 return value;
}
[numthreads(16,16,1)] void main(uint3 id : SV_DispatchThreadID) {
 uint w,h; OutputTexture.GetDimensions(w,h); if(id.x>=w||id.y>=h)return;
 float4 fg=ForegroundTexture[id.xy], plate=CleanPlateTexture[id.xy];
 float3 corrected=max(fg.rgb*max(ScreenCorrection,0),0);
 float alpha=filteredMatte(id.xy,uint2(w,h));
 float3 color=max(corrected-plate.rgb*saturate(DespillStrength),0);
 float a=saturate(alpha*fg.a); OutputTexture[id.xy]=float4(color*a,a);
})";
};

IBKKeyerEffect::IBKKeyerEffect()
    : ArtifactAbstractEffect()
    , typedCpuImpl_(makeShared<IBKKeyerEffectCPUImpl>()) {
    setCPUImpl(typedCpuImpl_);
    auto gpu = makeShared<IBKKeyerEffectGPUImpl>();
    gpu->params_ = typedCpuImpl_->params();
    setGPUImpl(gpu);
    setDisplayName("IBK Keyer");
    setEffectID("Effect.Keying.IBKKeyer");
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setComputeMode(ComputeMode::AUTO);
}

std::vector<AbstractProperty> IBKKeyerEffect::getProperties() const {
    const auto& p = typedCpuImpl_->params();
    std::vector<AbstractProperty> result;
    const auto addFloat = [&result](const char* name, float value, double min, double max) {
        auto& property = result.emplace_back();
        property.setName(name);
        property.setType(PropertyType::Float);
        property.setSoftRange(min, max);
        property.setHardRange(min, max);
        property.setDefaultValue(QVariant(static_cast<double>(value)));
        property.setValue(QVariant(static_cast<double>(value)));
    };
    addFloat("screenCorrection", p.screenCorrection, 0.0, 4.0);
    addFloat("coreMatteClip", p.coreMatteClip, 0.0, 1.0);
    addFloat("edgeMatteSoftness", p.edgeMatteSoftness, 0.00001, 1.0);
    addFloat("despillStrength", p.despillStrength, 0.0, 1.0);
    addFloat("garbageMatteGamma", p.garbageMatteGamma, 0.00001, 4.0);
    addFloat("detailRecovery", p.detailRecovery, 0.0, 1.0);
    const auto addInteger = [&result](const char* name, int value, int minimum, int maximum) {
        auto& property = result.emplace_back();
        property.setName(name);
        property.setType(PropertyType::Integer);
        property.setSoftRange(minimum, maximum);
        property.setHardRange(minimum, maximum);
        property.setDefaultValue(QVariant(value));
        property.setValue(QVariant(value));
    };
    addInteger("erodePixels", p.erodePixels, 0, 64);
    addInteger("dilatePixels", p.dilatePixels, 0, 64);
    auto& preview = result.emplace_back();
    preview.setName("previewMode");
    preview.setType(PropertyType::Integer);
    preview.setSoftRange(0, 2);
    preview.setHardRange(0, 2);
    preview.setDefaultValue(QVariant(0));
    preview.setValue(QVariant(previewMode_));
    auto& cleanPlate = result.emplace_back();
    cleanPlate.setName("cleanPlatePath");
    cleanPlate.setType(PropertyType::String);
    cleanPlate.setDefaultValue(QVariant(QString()));
    cleanPlate.setValue(cleanPlatePath_);
    auto& cleanPlateLoaded = result.emplace_back();
    cleanPlateLoaded.setName("cleanPlateLoaded");
    cleanPlateLoaded.setType(PropertyType::Bool);
    cleanPlateLoaded.setDefaultValue(QVariant(false));
    cleanPlateLoaded.setValue(QVariant(typedCpuImpl_->hasCleanPlate()));
    return result;
}

void IBKKeyerEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    auto p = typedCpuImpl_->params();
    const QString property = name.toQString();
    const auto read = [&value](float fallback, float minimum, float maximum) {
        const float raw = static_cast<float>(value.toDouble());
        return std::isfinite(raw) ? std::clamp(raw, minimum, maximum) : fallback;
    };
    if (property == "screenCorrection") p.screenCorrection = read(1.0f, 0.0f, 4.0f);
    else if (property == "coreMatteClip") p.coreMatteClip = read(0.5f, 0.0f, 1.0f);
    else if (property == "edgeMatteSoftness") p.edgeMatteSoftness = read(0.2f, 0.00001f, 1.0f);
    else if (property == "despillStrength") p.despillStrength = read(0.5f, 0.0f, 1.0f);
    else if (property == "garbageMatteGamma") p.garbageMatteGamma = read(1.0f, 0.00001f, 4.0f);
    else if (property == "detailRecovery") p.detailRecovery = read(0.3f, 0.0f, 1.0f);
    else if (property == "erodePixels") {
        const int raw = value.toInt();
        p.erodePixels = value.isValid() ? std::clamp(raw, 0, 64) : 1;
    }
    else if (property == "dilatePixels") {
        const int raw = value.toInt();
        p.dilatePixels = value.isValid() ? std::clamp(raw, 0, 64) : 3;
    }
    else if (property == "previewMode") {
        previewMode_ = std::clamp(value.toInt(), 0, 2);
        typedCpuImpl_->setPreviewMode(previewMode_);
        if (auto* gpu = dynamic_cast<IBKKeyerEffectGPUImpl*>(gpuImpl().get()))
            gpu->previewMode_ = previewMode_;
        return;
    }
    else if (property == "cleanPlatePath") {
        const QString path = value.toString().trimmed();
        if (typedCpuImpl_->setCleanPlatePath(path)) {
            cleanPlatePath_ = path;
            if (auto* gpu = dynamic_cast<IBKKeyerEffectGPUImpl*>(gpuImpl().get()))
                gpu->cleanPlate_ = typedCpuImpl_->cleanPlate();
        }
        return;
    }
    else return;
    typedCpuImpl_->setParams(p);
    if (auto* gpu = dynamic_cast<IBKKeyerEffectGPUImpl*>(gpuImpl().get()))
        gpu->params_ = typedCpuImpl_->params();
}

void IBKKeyerEffect::setCleanPlate(const ImageF32x4_RGBA& image) {
    typedCpuImpl_->setCleanPlate(image);
    cleanPlatePath_.clear();
    if (auto* gpu = dynamic_cast<IBKKeyerEffectGPUImpl*>(gpuImpl().get()))
        gpu->cleanPlate_ = image;
}

} // namespace Artifact
