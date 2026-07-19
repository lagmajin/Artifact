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
module Artifact.Effect.Spherize;




import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;

namespace Artifact {

void SpherizeEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const ImageF32x4_RGBA& srcImage = src.image();
    const float* srcData = srcImage.rgba32fData();
    if (!srcData) {
        dst = src;
        return;
    }
    cv::Mat srcMat(srcImage.height(), srcImage.width(), CV_32FC4, const_cast<float*>(srcData));
    
    int height = srcMat.rows;
    int width = srcMat.cols;
    cv::Mat dstMat(height, width, CV_32FC4);
    
    // パラメータ
    float amount = amount_ / 100.0f;  // -1.0〜1.0に変換
    float radius = radius_;             // 0.0〜1.0
    float cx = centerX_ * width;       // ピクセルの絶対座標
    float cy = centerY_ * height;
    float maxRadius = std::min(width, height) * radius;
    
    // ピクセルごとの処理 — 行単位で並列化
    std::vector<cv::Vec4f> rowResults(width * height);

    ArtifactCore::Parallel::For(0, height, [&](int y) {
        for (int x = 0; x < width; x++) {
            // 中心からの距離と角度を計算
            float dx = static_cast<float>(x) - cx;
            float dy = static_cast<float>(y) - cy;
            float dist = std::sqrt(dx * dx + dy * dy);
            float angle = std::atan2(dy, dx);

            // 球面歪みの計算
            float normalizedDist = dist / maxRadius;
            if (normalizedDist > 1.0f) {
                // 半径外は歪みなし
                cv::Vec4f pixel = srcMat.at<cv::Vec4f>(y, x);
                rowResults[y * width + x] = pixel;
                continue;
            }

            // 球面投影の計算
            float z = std::sqrt(std::max(0.0f, 1.0f - normalizedDist * normalizedDist));

            // 新しい座標へのマッピング
            float newDist = normalizedDist + amount * z * normalizedDist * (1.0f - normalizedDist);

            // 元の画像からのサンプリング座標
            int srcX = static_cast<int>(cx + newDist * maxRadius * std::cos(angle));
            int srcY = static_cast<int>(cy + newDist * maxRadius * std::sin(angle));

            // 境界チェック
            srcX = std::max(0, std::min(width - 1, srcX));
            srcY = std::max(0, std::min(height - 1, srcY));

            // ピクセルをコピー
            cv::Vec4f pixel = srcMat.at<cv::Vec4f>(srcY, srcX);
            rowResults[y * width + x] = pixel;
        }
    });

    // 結果をdstMatにコピー
    ArtifactCore::Parallel::For(0, height, [&](int y) {
        for (int x = 0; x < width; x++) {
            dstMat.at<cv::Vec4f>(y, x) = rowResults[y * width + x];
        }
    });
    
    // 結果をdstに設定
    ImageF32x4_RGBA dstImage;
    dstImage.setFromRGBA32F(
        dstMat.ptr<float>(), dstMat.cols, dstMat.rows,
        srcImage.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(dstImage);
}

static constexpr const char* kSpherizeHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);
cbuffer SpherizeParams:register(b0){float g_Amount;float g_Radius;float g_Cx;float g_Cy;}
float4 atp(int2 p,uint w,uint h){return g_InputTexture[uint2(clamp(p.x,0,(int)w-1),clamp(p.y,0,(int)h-1))];}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float2 c=float2(g_Cx*w,g_Cy*h),d=float2(id.xy)-c;float maxR=max(min(w,h)*g_Radius,0.001),dist=length(d),n=dist/maxR;if(n>1){g_OutputTexture[id.xy]=atp(int2(id.xy),w,h);return;}float z=sqrt(max(0,1-n*n));float nd=n+(g_Amount/100)*z*n*(1-n);float2 offset=dist>1e-6?d*(nd/n):float2(0,0);g_OutputTexture[id.xy]=atp(int2(c+offset),w,h);}
)";

void SpherizeEffectGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    auto fallback=[&]{SpherizeEffectCPUImpl c;c.setAmount(amount_);c.setRadius(radius_);c.setCenterX(centerX_);c.setCenterY(centerY_);c.applyCPU(src,dst);};Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;if(!acquireSharedRenderDeviceForCurrentBackend(device,context)){fallback();return;}const auto& image=src.image();const float* data=image.rgba32fData();if(!data||image.width()<=0||image.height()<=0){fallback();return;}
    Diligent::TextureDesc td{};td.Name="Spherize/Input";td.Type=Diligent::RESOURCE_DIM_TEX_2D;td.Width=image.width();td.Height=image.height();td.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;td.MipLevels=1;td.ArraySize=1;td.SampleCount=1;td.Usage=Diligent::USAGE_IMMUTABLE;td.BindFlags=Diligent::BIND_SHADER_RESOURCE;Diligent::TextureSubResData sub{};sub.pData=data;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(td,&init,&input);if(!input){fallback();return;}auto od=td;od.Name="Spherize/Output";od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(od,nullptr,&output);if(!output){fallback();return;}
    struct Params{float amount,radius,cx,cy;};Diligent::BufferDesc bd{};bd.Name="Spherize/Params";bd.Size=sizeof(Params);bd.Usage=Diligent::USAGE_DYNAMIC;bd.BindFlags=Diligent::BIND_UNIFORM_BUFFER;bd.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(bd,nullptr,&params);if(!params){fallback();return;}void*m=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,m);if(!m){fallback();return;}Params p{amount_,radius_,centerX_,centerY_};std::memcpy(m,&p,sizeof(p));context->UnmapBuffer(params,Diligent::MAP_WRITE);
    static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"SpherizeParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gc{device,context};ArtifactCore::ComputeExecutor ex{gc};ArtifactCore::ComputePipelineDesc pd{};pd.name="Spherize/PSO";pd.shaderSource=kSpherizeHlsl;pd.entryPoint="main";pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pd.variables=vars;pd.variableCount=3;pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!ex.build(pd)||!ex.createShaderResourceBinding(true)||!ex.setBuffer("SpherizeParams",params)||!ex.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!ex.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){fallback();return;}ex.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    auto sd=od;sd.Name="Spherize/Readback";sd.Usage=Diligent::USAGE_STAGING;sd.BindFlags=Diligent::BIND_NONE;sd.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(sd,nullptr,&staging);if(!staging){fallback();return;}context->CopyTexture(Diligent::CopyTextureAttribs(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){fallback();return;}cv::Mat result(image.height(),image.width(),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result,image.colorDescriptor());context->UnmapTextureSubresource(staging,0,0);
}

class SpherizeEffect::Impl {
public:
    std::shared_ptr<SpherizeEffectCPUImpl> cpuImpl_;
    std::shared_ptr<SpherizeEffectGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = std::make_shared<SpherizeEffectCPUImpl>();
        gpuImpl_ = std::make_shared<SpherizeEffectGPUImpl>();
    }
};

SpherizeEffect::SpherizeEffect() : impl_(new Impl()) {
    setDisplayName(ArtifactCore::UniString("Spherize"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(impl_->cpuImpl_);
    setGPUImpl(impl_->gpuImpl_);
}

SpherizeEffect::~SpherizeEffect() {
    delete impl_;
}

void SpherizeEffect::setAmount(float amount) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setAmount(amount);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setAmount(amount);
    }
}

float SpherizeEffect::amount() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->amount();
    }
    return 0.0f;
}

void SpherizeEffect::setRadius(float radius) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setRadius(radius);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setRadius(radius);
    }
}

float SpherizeEffect::radius() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->radius();
    }
    return 0.0f;
}

void SpherizeEffect::setCenterX(float cx) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setCenterX(cx);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setCenterX(cx);
    }
}

float SpherizeEffect::centerX() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->centerX();
    }
    return 0.0f;
}

void SpherizeEffect::setCenterY(float cy) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setCenterY(cy);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setCenterY(cy);
    }
}

float SpherizeEffect::centerY() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->centerY();
    }
    return 0.0f;
}

std::vector<ArtifactCore::AbstractProperty> SpherizeEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(4);

    auto& amountProp = props.emplace_back();
    amountProp.setName("amount");
    amountProp.setType(ArtifactCore::PropertyType::Float);
    amountProp.setDefaultValue(QVariant(static_cast<double>(amount())));
    amountProp.setValue(QVariant(static_cast<double>(amount())));

    auto& radiusProp = props.emplace_back();
    radiusProp.setName("radius");
    radiusProp.setType(ArtifactCore::PropertyType::Float);
    radiusProp.setDefaultValue(QVariant(static_cast<double>(radius())));
    radiusProp.setValue(QVariant(static_cast<double>(radius())));

    auto& cxProp = props.emplace_back();
    cxProp.setName("centerX");
    cxProp.setType(ArtifactCore::PropertyType::Float);
    cxProp.setDefaultValue(QVariant(static_cast<double>(centerX())));
    cxProp.setValue(QVariant(static_cast<double>(centerX())));

    auto& cyProp = props.emplace_back();
    cyProp.setName("centerY");
    cyProp.setType(ArtifactCore::PropertyType::Float);
    cyProp.setDefaultValue(QVariant(static_cast<double>(centerY())));
    cyProp.setValue(QVariant(static_cast<double>(centerY())));

    return props;
}

void SpherizeEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    QString n = name.toQString();
    if (n == "amount") {
        setAmount(static_cast<float>(value.toDouble()));
    } else if (n == "radius") {
        setRadius(static_cast<float>(value.toDouble()));
    } else if (n == "centerX") {
        setCenterX(static_cast<float>(value.toDouble()));
    } else if (n == "centerY") {
        setCenterY(static_cast<float>(value.toDouble()));
    }
}

}
