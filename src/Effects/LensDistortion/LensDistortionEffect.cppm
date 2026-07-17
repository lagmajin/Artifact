module;
#include <QList>
#include <QVariant>
#include <cmath>
#include <opencv2/core/mat.hpp>
#include <opencv2/opencv.hpp>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <regex>
#include <random>
module Artifact.Effect.LensDistortion;

import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import ImageProcessing.Distortion;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;

namespace Artifact {

void LensDistortionEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const ImageF32x4_RGBA& srcImage = src.image();
    int width = srcImage.width();
    int height = srcImage.height();
    if (width <= 0 || height <= 0) return;

    float cx = centerX_ * static_cast<float>(width);
    float cy = centerY_ * static_cast<float>(height);
    float maxR = std::min(static_cast<float>(width), static_cast<float>(height)) * 0.5f;
    float k = distortion_ / 100.0f;
    if (invertDistortion_) k = -k;
    float zm = zoom_;

    std::vector<FloatRGBA> rowResults(static_cast<size_t>(width) * height);
    ArtifactCore::Parallel::For(0, height, [&](int y) {
        for (int x = 0; x < width; x++) {
            float dx = static_cast<float>(x) - cx;
            float dy = static_cast<float>(y) - cy;
            float r = std::sqrt(dx * dx + dy * dy) / maxR;

            float rDistorted = r * (1.0f + k * r * r);
            rDistorted /= zm;

            float srcX, srcY;
            if (r > 1e-6f) {
                float scale = (rDistorted * maxR) / (r * maxR);
                srcX = cx + dx * scale;
                srcY = cy + dy * scale;
            } else {
                srcX = cx;
                srcY = cy;
            }

            FloatRGBA pixel = sampleBilinear(srcImage, srcX, srcY);
            size_t idx = static_cast<size_t>(y) * width + x;
            rowResults[idx] = pixel;
        }
    });

    ImageF32x4_RGBA dstImage;
    dstImage.resize(width, height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t srcIdx = static_cast<size_t>(y) * width + x;
            dstImage.setPixel(x, y, rowResults[srcIdx]);
        }
    }

    dst = ImageF32x4RGBAWithCache(dstImage);
}

static constexpr const char* kLensDistortionHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0); RWTexture2D<float4> g_OutputTexture:register(u0);
cbuffer LensDistortionParams:register(b0){float g_Distortion;float g_Cx;float g_Cy;float g_Zoom;float g_Invert;float3 g_Pad;}
float4 sampleLinear(float2 p,uint w,uint h){p=clamp(p,float2(0,0),float2(w-1,h-1));int2 i=int2(floor(p));float2 f=frac(p);float4 a=g_InputTexture[uint2(i)],b=g_InputTexture[uint2(min(i+int2(1,0),int2(w-1,h-1)))],c=g_InputTexture[uint2(min(i+int2(0,1),int2(w-1,h-1)))],d=g_InputTexture[uint2(min(i+int2(1,1),int2(w-1,h-1)))];return lerp(lerp(a,b,f.x),lerp(c,d,f.x),f.y);}
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float2 c=float2(g_Cx*w,g_Cy*h),q=float2(id.xy),d=q-c;float maxR=0.5*min(w,h),r=length(d)/max(maxR,1);float k=(g_Invert>0.5?-1:1)*g_Distortion/100;float rd=r*(1+k*r*r)/max(g_Zoom,0.001);float2 s=(r>1e-6?c+d*(rd/r):c);g_OutputTexture[id.xy]=sampleLinear(s,w,h);}
)";
static constexpr const char* kHlsl = kLensDistortionHlsl;

void LensDistortionEffectGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    auto fallback = [&] { LensDistortionEffectCPUImpl cpu; cpu.setDistortion(distortion_); cpu.setCenterX(centerX_); cpu.setCenterY(centerY_); cpu.setInvertDistortion(invertDistortion_); cpu.setZoom(zoom_); cpu.applyCPU(src, dst); };
    if (!acquireSharedRenderDeviceForCurrentBackend(device, context)) { fallback(); return; }
    const auto& image = src.image();
    const float* data = image.rgba32fData();
    if (!data || image.width() <= 0 || image.height() <= 0) { fallback(); return; }
    Diligent::TextureDesc td{}; td.Name="LensDistortion/Input"; td.Type=Diligent::RESOURCE_DIM_TEX_2D; td.Width=image.width(); td.Height=image.height(); td.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; td.MipLevels=1; td.ArraySize=1; td.SampleCount=1; td.Usage=Diligent::USAGE_IMMUTABLE; td.BindFlags=Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData sub{}; sub.pData=data; sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull; Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1;
    Diligent::RefCntAutoPtr<Diligent::ITexture> input; device->CreateTexture(td,&init,&input); if(!input){fallback();return;}
    auto od=td; od.Name="LensDistortion/Output"; od.Usage=Diligent::USAGE_DEFAULT; od.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS; Diligent::RefCntAutoPtr<Diligent::ITexture> output; device->CreateTexture(od,nullptr,&output); if(!output){fallback();return;}
    struct Params{float distortion,cx,cy,zoom;float invert,pad[3];}; Diligent::BufferDesc bd{}; bd.Name="LensDistortion/Params"; bd.Size=sizeof(Params); bd.Usage=Diligent::USAGE_DYNAMIC; bd.BindFlags=Diligent::BIND_UNIFORM_BUFFER; bd.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; Diligent::RefCntAutoPtr<Diligent::IBuffer> params; device->CreateBuffer(bd,nullptr,&params); if(!params){fallback();return;} void* mapped=nullptr; context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped); if(!mapped){fallback();return;} Params p{distortion_,centerX_,centerY_,std::max(zoom_,0.001f),invertDistortion_?1.0f:0.0f,{0,0,0}}; std::memcpy(mapped,&p,sizeof(p)); context->UnmapBuffer(params,Diligent::MAP_WRITE);
    static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"LensDistortionParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}}; ArtifactCore::GpuContext gc{device,context}; ArtifactCore::ComputeExecutor ex{gc}; ArtifactCore::ComputePipelineDesc pd{}; pd.name="LensDistortion/PSO"; pd.shaderSource=kHlsl; pd.entryPoint="main"; pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; pd.variables=vars; pd.variableCount=3; pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if(!ex.build(pd)||!ex.createShaderResourceBinding(true)||!ex.setBuffer("LensDistortionParams",params)||!ex.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!ex.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){fallback();return;} ex.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    auto sd=od; sd.Name="LensDistortion/Readback"; sd.Usage=Diligent::USAGE_STAGING; sd.BindFlags=Diligent::BIND_NONE; sd.CPUAccessFlags=Diligent::CPU_ACCESS_READ; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(sd,nullptr,&staging); if(!staging){fallback();return;} context->CopyTexture(Diligent::CopyTextureAttribs(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION)); context->Flush(); context->WaitForIdle(); Diligent::MappedTextureSubresource read{}; context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read); if(!read.pData||!read.Stride){fallback();return;} cv::Mat result(image.height(),image.width(),CV_32FC4,read.pData,read.Stride); dst.image().setFromCVMat(result); context->UnmapTextureSubresource(staging,0,0);
}

class LensDistortionEffect::Impl {
public:
    std::shared_ptr<LensDistortionEffectCPUImpl> cpuImpl_;
    std::shared_ptr<LensDistortionEffectGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = std::make_shared<LensDistortionEffectCPUImpl>();
        gpuImpl_ = std::make_shared<LensDistortionEffectGPUImpl>();
    }
};

LensDistortionEffect::LensDistortionEffect() : impl_(new Impl()) {
    setDisplayName(ArtifactCore::UniString("Lens Distortion"));
    setPipelineStage(EffectPipelineStage::GeometryTransform);
    setCPUImpl(impl_->cpuImpl_);
    setGPUImpl(impl_->gpuImpl_);
}

LensDistortionEffect::~LensDistortionEffect() {
    delete impl_;
}

void LensDistortionEffect::setDistortion(float v) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setDistortion(v);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setDistortion(v);
}
float LensDistortionEffect::distortion() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->distortion() : 0.0f;
}

void LensDistortionEffect::setCenterX(float cx) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setCenterX(cx);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setCenterX(cx);
}
float LensDistortionEffect::centerX() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->centerX() : 0.5f;
}

void LensDistortionEffect::setCenterY(float cy) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setCenterY(cy);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setCenterY(cy);
}
float LensDistortionEffect::centerY() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->centerY() : 0.5f;
}

void LensDistortionEffect::setInvertDistortion(bool v) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setInvertDistortion(v);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setInvertDistortion(v);
}
bool LensDistortionEffect::invertDistortion() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->invertDistortion() : false;
}

void LensDistortionEffect::setZoom(float v) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setZoom(v);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setZoom(v);
}
float LensDistortionEffect::zoom() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->zoom() : 1.0f;
}

std::vector<ArtifactCore::AbstractProperty> LensDistortionEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(5);

    auto& distProp = props.emplace_back();
    distProp.setName("distortion");
    distProp.setType(ArtifactCore::PropertyType::Float);
    distProp.setDefaultValue(QVariant(0.0));
    distProp.setValue(QVariant(static_cast<double>(distortion())));
    distProp.setSoftRange(QVariant(-100.0), QVariant(100.0));

    auto& cxProp = props.emplace_back();
    cxProp.setName("centerX");
    cxProp.setType(ArtifactCore::PropertyType::Float);
    cxProp.setDefaultValue(QVariant(0.5));
    cxProp.setValue(QVariant(static_cast<double>(centerX())));
    cxProp.setSoftRange(QVariant(0.0), QVariant(1.0));

    auto& cyProp = props.emplace_back();
    cyProp.setName("centerY");
    cyProp.setType(ArtifactCore::PropertyType::Float);
    cyProp.setDefaultValue(QVariant(0.5));
    cyProp.setValue(QVariant(static_cast<double>(centerY())));
    cyProp.setSoftRange(QVariant(0.0), QVariant(1.0));

    auto& invProp = props.emplace_back();
    invProp.setName("invertDistortion");
    invProp.setType(ArtifactCore::PropertyType::Boolean);
    invProp.setDefaultValue(QVariant(false));
    invProp.setValue(QVariant(invertDistortion()));

    auto& zoomProp = props.emplace_back();
    zoomProp.setName("zoom");
    zoomProp.setType(ArtifactCore::PropertyType::Float);
    zoomProp.setDefaultValue(QVariant(1.0));
    zoomProp.setValue(QVariant(static_cast<double>(zoom())));
    zoomProp.setSoftRange(QVariant(0.1), QVariant(3.0));

    return props;
}

void LensDistortionEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    QString n = name.toQString();
    if (n == "distortion") {
        setDistortion(static_cast<float>(value.toDouble()));
    } else if (n == "centerX") {
        setCenterX(static_cast<float>(value.toDouble()));
    } else if (n == "centerY") {
        setCenterY(static_cast<float>(value.toDouble()));
    } else if (n == "invertDistortion") {
        setInvertDistortion(value.toBool());
    } else if (n == "zoom") {
        setZoom(static_cast<float>(value.toDouble()));
    }
}

}
