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
module Artifact.Effect.Liquify;

import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = kPi * 2.0f;

float clampF(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

float lerpF(float a, float b, float t) {
    return a + t * (b - a);
}

float smoothstepF(float t) {
    return t * t * (3.0f - 2.0f * t);
}

float hash2D(float x, float y, int seed) {
    float h = std::fmod(std::sin(x * 127.1f + y * 311.7f + seed * 73.13f) * 43758.5453f, 1.0f);
    return h < 0.0f ? h + 1.0f : h;
}

float fbmNoise(float x, float y, int octaves, int seed) {
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        value += amplitude * hash2D(x * frequency, y * frequency, seed + i);
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return value;
}

// Bilinear sample from CV mat
cv::Vec4f sampleBilinearCV(const cv::Mat& src, float sx, float sy) {
    int w = src.cols;
    int h = src.rows;
    if (w <= 0 || h <= 0) return cv::Vec4f(0, 0, 0, 0);

    sx = std::fmod(sx, static_cast<float>(w));
    if (sx < 0) sx += w;
    sy = std::fmod(sy, static_cast<float>(h));
    if (sy < 0) sy += h;

    int x0 = static_cast<int>(sx);
    int y0 = static_cast<int>(sy);
    int x1 = (x0 + 1) % w;
    int y1 = (y0 + 1) % h;

    float fx = sx - x0;
    float fy = sy - y0;

    auto c00 = src.at<cv::Vec4f>(y0, x0);
    auto c10 = src.at<cv::Vec4f>(y1, x0);
    auto c01 = src.at<cv::Vec4f>(y0, x1);
    auto c11 = src.at<cv::Vec4f>(y1, x1);

    cv::Vec4f result;
    for (int i = 0; i < 4; ++i) {
        result[i] = lerpF(lerpF(c00[i], c10[i], fx), lerpF(c01[i], c11[i], fx), fy);
    }
    return result;
}

struct LiquifyParams {
    LiquifyBrushType brushType;
    float amount;
    float radius;
    float cx;
    float cy;
    float angle;
    int seed;
    int imgW;
    int imgH;
};

// Displacement function: maps output (x,y) to source (sx,sy)
void liquifyDisplacement(float x, float y, const LiquifyParams& p,
                          float& sx, float& sy) {
    float dx = x - p.cx;
    float dy = y - p.cy;
    float dist = std::sqrt(dx * dx + dy * dy);
    float maxR = p.radius;
    if (maxR <= 0.0f || dist >= maxR) {
        sx = x;
        sy = y;
        return;
    }

    float nd = dist / maxR;
    float falloff = 1.0f - nd * nd;
    falloff = falloff * falloff;

    float amount = p.amount / 100.0f;

    switch (p.brushType) {
    case LiquifyBrushType::Push: {
        float pushAngle = p.angle * kPi / 180.0f;
        float pushX = std::cos(pushAngle) * amount * maxR * falloff;
        float pushY = std::sin(pushAngle) * amount * maxR * falloff;
        sx = x + pushX;
        sy = y + pushY;
        break;
    }
    case LiquifyBrushType::Pinch: {
        float scale = 1.0f - amount * falloff;
        scale = clampF(scale, 0.01f, 10.0f);
        sx = p.cx + dx * scale;
        sy = p.cy + dy * scale;
        break;
    }
    case LiquifyBrushType::Bloat: {
        float scale = 1.0f + amount * falloff;
        scale = clampF(scale, 0.01f, 10.0f);
        sx = p.cx + dx * scale;
        sy = p.cy + dy * scale;
        break;
    }
    case LiquifyBrushType::Twirl: {
        float angleRad = p.angle * kPi / 180.0f;
        float twist = angleRad * falloff * amount;
        float cosA = std::cos(twist);
        float sinA = std::sin(twist);
        sx = p.cx + dx * cosA - dy * sinA;
        sy = p.cy + dx * sinA + dy * cosA;
        break;
    }
    case LiquifyBrushType::Turbulence: {
        float noiseScale = 1.0f / (maxR * 0.5f + 1.0f);
        float nx = hash2D(x * noiseScale, y * noiseScale, p.seed) - 0.5f;
        float ny = hash2D(y * noiseScale, x * noiseScale, p.seed + 1) - 0.5f;
        float displace = amount * maxR * falloff;
        sx = x + nx * displace * 2.0f;
        sy = y + ny * displace * 2.0f;
        break;
    }
    case LiquifyBrushType::Pucker: {
        float sign = (amount > 0) ? -1.0f : 1.0f;
        float a = std::abs(amount);
        float scale = 1.0f + sign * a * falloff;
        scale = clampF(scale, 0.01f, 10.0f);
        sx = p.cx + dx * scale;
        sy = p.cy + dy * scale;
        break;
    }
    default: {
        sx = x;
        sy = y;
        break;
    }
    }
}

} // anonymous

// ============ CPU Implementation ============

void LiquifyEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const ImageF32x4_RGBA& srcImage = src.image();
    const float* srcData = srcImage.rgba32fData();
    if (!srcData) {
        dst = src;
        return;
    }
    cv::Mat srcMat(srcImage.height(), srcImage.width(), CV_32FC4, const_cast<float*>(srcData));

    int height = srcMat.rows;
    int width = srcMat.cols;

    float absCx = centerX_ * width;
    float absCy = centerY_ * height;
    float absRadius = std::min(width, height) * radius_;

    LiquifyParams params;
    params.brushType = brushType_;
    params.amount = amount_;
    params.radius = absRadius;
    params.cx = absCx;
    params.cy = absCy;
    params.angle = angle_;
    params.seed = turbulenceSeed_;
    params.imgW = width;
    params.imgH = height;

    cv::Mat dstMat(height, width, CV_32FC4);

    ArtifactCore::Parallel::For(0, height, [&](int y) {
        for (int x = 0; x < width; ++x) {
            float sx, sy;
            liquifyDisplacement(static_cast<float>(x), static_cast<float>(y),
                                 params, sx, sy);

            int ix = static_cast<int>(sx);
            int iy = static_cast<int>(sy);
            ix = clampF(ix, 0, width - 1);
            iy = clampF(iy, 0, height - 1);

            cv::Vec4f pixel;
            if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                pixel = sampleBilinearCV(srcMat, sx, sy);
            } else {
                pixel = srcMat.at<cv::Vec4f>(iy, ix);
            }
            dstMat.at<cv::Vec4f>(y, x) = pixel;
        }
    });

    ImageF32x4_RGBA dstImage;
    dstImage.setFromRGBA32F(dstMat.ptr<float>(), dstMat.cols, dstMat.rows);
    dst.image() = std::move(dstImage);
}

// ============ GPU Implementation ============

static constexpr const char* kLiquifyHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);
cbuffer LiquifyParams:register(b0){float g_Amount;float g_Radius;float g_Cx;float g_Cy;float g_Angle;int g_Brush;float2 g_Pad;}
float4 atp(int2 p,uint w,uint h){return g_InputTexture[uint2(clamp(p.x,0,(int)w-1),clamp(p.y,0,(int)h-1))];}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float2 c=float2(g_Cx*w,g_Cy*h),q=float2(id.xy),d=q-c,dist=length(d),r=max(g_Radius*min(w,h),1);if(dist>r){g_OutputTexture[id.xy]=atp(int2(q),w,h);return;}float fall=1-dist/r,rad=g_Angle*0.0174532925;float2 dir=float2(cos(rad),sin(rad));float strength=(g_Amount/100)*r*fall*fall;float2 source=q-dir*strength;g_OutputTexture[id.xy]=atp(int2(source+0.5),w,h);}
)";

void LiquifyEffectGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    auto fallback=[&]{LiquifyEffectCPUImpl c;c.setBrushType(brushType_);c.setAmount(amount_);c.setRadius(radius_);c.setCenterX(centerX_);c.setCenterY(centerY_);c.setAngle(angle_);c.setTurbulenceSeed(turbulenceSeed_);c.setMeshDensity(meshDensity_);c.applyCPU(src,dst);};
    if(brushType_!=0){fallback();return;}
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;if(!acquireSharedRenderDeviceForCurrentBackend(device,context)){fallback();return;}const auto& image=src.image();const float* data=image.rgba32fData();if(!data||image.width()<=0||image.height()<=0){fallback();return;}
    Diligent::TextureDesc td{};td.Name="Liquify/Input";td.Type=Diligent::RESOURCE_DIM_TEX_2D;td.Width=image.width();td.Height=image.height();td.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;td.MipLevels=1;td.ArraySize=1;td.SampleCount=1;td.Usage=Diligent::USAGE_IMMUTABLE;td.BindFlags=Diligent::BIND_SHADER_RESOURCE;Diligent::TextureSubResData sub{};sub.pData=data;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(td,&init,&input);if(!input){fallback();return;}auto od=td;od.Name="Liquify/Output";od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(od,nullptr,&output);if(!output){fallback();return;}
    struct Params{float amount,radius,cx,cy,angle;int brush;float pad[2];};Diligent::BufferDesc bd{};bd.Name="Liquify/Params";bd.Size=sizeof(Params);bd.Usage=Diligent::USAGE_DYNAMIC;bd.BindFlags=Diligent::BIND_UNIFORM_BUFFER;bd.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(bd,nullptr,&params);if(!params){fallback();return;}void*m=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,m);if(!m){fallback();return;}Params p{amount_,radius_,centerX_,centerY_,angle_,static_cast<int>(brushType_),{0,0}};std::memcpy(m,&p,sizeof(p));context->UnmapBuffer(params,Diligent::MAP_WRITE);
    static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"LiquifyParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gc{device,context};ArtifactCore::ComputeExecutor ex{gc};ArtifactCore::ComputePipelineDesc pd{};pd.name="Liquify/PSO";pd.shaderSource=kLiquifyHlsl;pd.entryPoint="main";pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pd.variables=vars;pd.variableCount=3;pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!ex.build(pd)||!ex.createShaderResourceBinding(true)||!ex.setBuffer("LiquifyParams",params)||!ex.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!ex.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){fallback();return;}ex.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    auto sd=od;sd.Name="Liquify/Readback";sd.Usage=Diligent::USAGE_STAGING;sd.BindFlags=Diligent::BIND_NONE;sd.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(sd,nullptr,&staging);if(!staging){fallback();return;}context->CopyTexture(Diligent::CopyTextureAttribs(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){fallback();return;}cv::Mat result(image.height(),image.width(),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result);context->UnmapTextureSubresource(staging,0,0);
}

// ============ Effect PIMPL ============

class LiquifyEffect::Impl {
public:
    std::shared_ptr<LiquifyEffectCPUImpl> cpuImpl_;
    std::shared_ptr<LiquifyEffectGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = std::make_shared<LiquifyEffectCPUImpl>();
        gpuImpl_ = std::make_shared<LiquifyEffectGPUImpl>();
    }
};

LiquifyEffect::LiquifyEffect() : impl_(new Impl()) {
    setDisplayName(ArtifactCore::UniString("Liquify"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(impl_->cpuImpl_);
    setGPUImpl(impl_->gpuImpl_);
}

LiquifyEffect::~LiquifyEffect() {
    delete impl_;
}

void LiquifyEffect::setBrushType(LiquifyBrushType t) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setBrushType(t);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setBrushType(t);
}

LiquifyBrushType LiquifyEffect::brushType() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->brushType() : LiquifyBrushType::Push;
}

void LiquifyEffect::setAmount(float v) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setAmount(v);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setAmount(v);
}

float LiquifyEffect::amount() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->amount() : 0.0f;
}

void LiquifyEffect::setRadius(float v) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setRadius(v);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setRadius(v);
}

float LiquifyEffect::radius() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->radius() : 0.0f;
}

void LiquifyEffect::setCenterX(float v) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setCenterX(v);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setCenterX(v);
}

float LiquifyEffect::centerX() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->centerX() : 0.0f;
}

void LiquifyEffect::setCenterY(float v) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setCenterY(v);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setCenterY(v);
}

float LiquifyEffect::centerY() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->centerY() : 0.0f;
}

void LiquifyEffect::setAngle(float v) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setAngle(v);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setAngle(v);
}

float LiquifyEffect::angle() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->angle() : 0.0f;
}

void LiquifyEffect::setTurbulenceSeed(int s) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setTurbulenceSeed(s);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setTurbulenceSeed(s);
}

int LiquifyEffect::turbulenceSeed() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->turbulenceSeed() : 0;
}

void LiquifyEffect::setMeshDensity(int d) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setMeshDensity(d);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setMeshDensity(d);
}

int LiquifyEffect::meshDensity() const {
    return impl_->cpuImpl_ ? impl_->cpuImpl_->meshDensity() : 32;
}

std::vector<ArtifactCore::AbstractProperty> LiquifyEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(8);

    auto& brushProp = props.emplace_back();
    brushProp.setName("brushType");
    brushProp.setType(ArtifactCore::PropertyType::Integer);
    brushProp.setDefaultValue(QVariant(static_cast<int>(brushType())));

    auto& amtProp = props.emplace_back();
    amtProp.setName("amount");
    amtProp.setType(ArtifactCore::PropertyType::Float);
    amtProp.setDefaultValue(QVariant(static_cast<double>(amount())));

    auto& radProp = props.emplace_back();
    radProp.setName("radius");
    radProp.setType(ArtifactCore::PropertyType::Float);
    radProp.setDefaultValue(QVariant(static_cast<double>(radius())));

    auto& cxProp = props.emplace_back();
    cxProp.setName("centerX");
    cxProp.setType(ArtifactCore::PropertyType::Float);
    cxProp.setDefaultValue(QVariant(static_cast<double>(centerX())));

    auto& cyProp = props.emplace_back();
    cyProp.setName("centerY");
    cyProp.setType(ArtifactCore::PropertyType::Float);
    cyProp.setDefaultValue(QVariant(static_cast<double>(centerY())));

    auto& angProp = props.emplace_back();
    angProp.setName("angle");
    angProp.setType(ArtifactCore::PropertyType::Float);
    angProp.setDefaultValue(QVariant(static_cast<double>(angle())));

    auto& seedProp = props.emplace_back();
    seedProp.setName("turbulenceSeed");
    seedProp.setType(ArtifactCore::PropertyType::Integer);
    seedProp.setDefaultValue(QVariant(turbulenceSeed()));

    auto& meshProp = props.emplace_back();
    meshProp.setName("meshDensity");
    meshProp.setType(ArtifactCore::PropertyType::Integer);
    meshProp.setDefaultValue(QVariant(meshDensity()));

    return props;
}

void LiquifyEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    QString n = name.toQString();
    if (n == "brushType") {
        setBrushType(static_cast<LiquifyBrushType>(value.toInt()));
    } else if (n == "amount") {
        setAmount(static_cast<float>(value.toDouble()));
    } else if (n == "radius") {
        setRadius(static_cast<float>(value.toDouble()));
    } else if (n == "centerX") {
        setCenterX(static_cast<float>(value.toDouble()));
    } else if (n == "centerY") {
        setCenterY(static_cast<float>(value.toDouble()));
    } else if (n == "angle") {
        setAngle(static_cast<float>(value.toDouble()));
    } else if (n == "turbulenceSeed") {
        setTurbulenceSeed(value.toInt());
    } else if (n == "meshDensity") {
        setMeshDensity(value.toInt());
    }
}

}
