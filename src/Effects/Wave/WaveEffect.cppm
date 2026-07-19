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
module Artifact.Effect.Wave;




import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;

namespace Artifact {

void WaveEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
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
    
    // 波の計算
    float amp = amplitude_;
    float freq = frequency_;
    float phase = phase_;
    int wType = waveType_;
    int orient = orientation_;
    
    // ピクセルごとの処理 — 行単位で並列化
    std::vector<cv::Vec4f> rowResults(width * height);

    ArtifactCore::Parallel::For(0, height, [&](int y) {
        for (int x = 0; x < width; x++) {
            float offset = 0.0f;

            if (orient == 0) { // Horizontal
                if (wType == 0) { // Sine
                    offset = amp * std::sin(2.0f * CV_PI * freq * x + phase);
                } else { // Cosine
                    offset = amp * std::cos(2.0f * CV_PI * freq * x + phase);
                }
            } else { // Vertical
                if (wType == 0) { // Sine
                    offset = amp * std::sin(2.0f * CV_PI * freq * y + phase);
                } else { // Cosine
                    offset = amp * std::cos(2.0f * CV_PI * freq * y + phase);
                }
            }

            // シフト後の座標を計算
            int srcX, srcY;
            if (orient == 0) {
                srcX = x + static_cast<int>(offset);
                srcY = y;
            } else {
                srcX = x;
                srcY = y + static_cast<int>(offset);
            }

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

static constexpr const char* kWaveHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);
cbuffer WaveParams:register(b0){float g_Amplitude;float g_Frequency;float g_Phase;int g_Type;int g_Orientation;float3 g_Pad;}
float4 atp(int2 p,uint w,uint h){return g_InputTexture[uint2(clamp(p.x,0,(int)w-1),clamp(p.y,0,(int)h-1))];}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float phase=6.2831853*((g_Orientation==0?id.x:id.y)*g_Frequency)+g_Phase;float v=(g_Type==0?sin(phase):cos(phase))*g_Amplitude;int2 p=int2(id.xy)+(g_Orientation==0?int2((int)v,0):int2(0,(int)v));g_OutputTexture[id.xy]=atp(p,w,h);}
)";

void WaveEffectGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    auto fallback=[&]{WaveEffectCPUImpl c;c.setAmplitude(amplitude_);c.setFrequency(frequency_);c.setPhase(phase_);c.setWaveType(waveType_);c.setOrientation(orientation_);c.applyCPU(src,dst);};
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;if(!acquireSharedRenderDeviceForCurrentBackend(device,context)){fallback();return;}const auto& image=src.image();const float* data=image.rgba32fData();if(!data||image.width()<=0||image.height()<=0){fallback();return;}
    Diligent::TextureDesc td{};td.Name="Wave/Input";td.Type=Diligent::RESOURCE_DIM_TEX_2D;td.Width=image.width();td.Height=image.height();td.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;td.MipLevels=1;td.ArraySize=1;td.SampleCount=1;td.Usage=Diligent::USAGE_IMMUTABLE;td.BindFlags=Diligent::BIND_SHADER_RESOURCE;Diligent::TextureSubResData sub{};sub.pData=data;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(td,&init,&input);if(!input){fallback();return;}auto od=td;od.Name="Wave/Output";od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(od,nullptr,&output);if(!output){fallback();return;}
    struct Params{float amp,freq,phase;int type,orientation;float pad[3];};Diligent::BufferDesc bd{};bd.Name="Wave/Params";bd.Size=sizeof(Params);bd.Usage=Diligent::USAGE_DYNAMIC;bd.BindFlags=Diligent::BIND_UNIFORM_BUFFER;bd.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(bd,nullptr,&params);if(!params){fallback();return;}void*m=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,m);if(!m){fallback();return;}Params p{amplitude_,frequency_,phase_,waveType_,orientation_,{0,0,0}};std::memcpy(m,&p,sizeof(p));context->UnmapBuffer(params,Diligent::MAP_WRITE);
    static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"WaveParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gc{device,context};ArtifactCore::ComputeExecutor ex{gc};ArtifactCore::ComputePipelineDesc pd{};pd.name="Wave/PSO";pd.shaderSource=kWaveHlsl;pd.entryPoint="main";pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pd.variables=vars;pd.variableCount=3;pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!ex.build(pd)||!ex.createShaderResourceBinding(true)||!ex.setBuffer("WaveParams",params)||!ex.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!ex.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){fallback();return;}ex.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    auto sd=od;sd.Name="Wave/Readback";sd.Usage=Diligent::USAGE_STAGING;sd.BindFlags=Diligent::BIND_NONE;sd.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(sd,nullptr,&staging);if(!staging){fallback();return;}context->CopyTexture(Diligent::CopyTextureAttribs(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){fallback();return;}cv::Mat result(image.height(),image.width(),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result,image.colorDescriptor());context->UnmapTextureSubresource(staging,0,0);
}

class WaveEffect::Impl {
public:
    std::shared_ptr<WaveEffectCPUImpl> cpuImpl_;
    std::shared_ptr<WaveEffectGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = std::make_shared<WaveEffectCPUImpl>();
        gpuImpl_ = std::make_shared<WaveEffectGPUImpl>();
    }
};

WaveEffect::WaveEffect() : impl_(new Impl()) {
    setDisplayName(ArtifactCore::UniString("Wave"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(impl_->cpuImpl_);
    setGPUImpl(impl_->gpuImpl_);
}

WaveEffect::~WaveEffect() {
    delete impl_;
}

void WaveEffect::setAmplitude(float amp) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setAmplitude(amp);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setAmplitude(amp);
    }
}

float WaveEffect::amplitude() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->amplitude();
    }
    return 0.0f;
}

void WaveEffect::setFrequency(float freq) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setFrequency(freq);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setFrequency(freq);
    }
}

float WaveEffect::frequency() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->frequency();
    }
    return 0.0f;
}

void WaveEffect::setPhase(float phase) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setPhase(phase);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setPhase(phase);
    }
}

float WaveEffect::phase() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->phase();
    }
    return 0.0f;
}

void WaveEffect::setWaveType(int type) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setWaveType(type);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setWaveType(type);
    }
}

int WaveEffect::waveType() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->waveType();
    }
    return 0;
}

void WaveEffect::setOrientation(int ori) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setOrientation(ori);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setOrientation(ori);
    }
}

int WaveEffect::orientation() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->orientation();
    }
    return 0;
}

std::vector<ArtifactCore::AbstractProperty> WaveEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(5);

    auto& ampProp = props.emplace_back();
    ampProp.setName("amplitude");
    ampProp.setType(ArtifactCore::PropertyType::Float);
    ampProp.setDefaultValue(QVariant(static_cast<double>(amplitude())));
    ampProp.setValue(QVariant(static_cast<double>(amplitude())));

    auto& freqProp = props.emplace_back();
    freqProp.setName("frequency");
    freqProp.setType(ArtifactCore::PropertyType::Float);
    freqProp.setDefaultValue(QVariant(static_cast<double>(frequency())));
    freqProp.setValue(QVariant(static_cast<double>(frequency())));

    auto& phaseProp = props.emplace_back();
    phaseProp.setName("phase");
    phaseProp.setType(ArtifactCore::PropertyType::Float);
    phaseProp.setDefaultValue(QVariant(static_cast<double>(phase())));
    phaseProp.setValue(QVariant(static_cast<double>(phase())));

    auto& waveTypeProp = props.emplace_back();
    waveTypeProp.setName("waveType");
    waveTypeProp.setType(ArtifactCore::PropertyType::Integer);
    waveTypeProp.setDefaultValue(QVariant(waveType()));
    waveTypeProp.setValue(QVariant(waveType()));
    waveTypeProp.setTooltip(QStringLiteral("0=Sine, 1=Cosine"));

    auto& orientProp = props.emplace_back();
    orientProp.setName("orientation");
    orientProp.setType(ArtifactCore::PropertyType::Integer);
    orientProp.setDefaultValue(QVariant(orientation()));
    orientProp.setValue(QVariant(orientation()));
    orientProp.setTooltip(QStringLiteral("0=Horizontal, 1=Vertical"));

    return props;
}

void WaveEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    QString n = name.toQString();
    if (n == "amplitude") {
        setAmplitude(static_cast<float>(value.toDouble()));
    } else if (n == "frequency") {
        setFrequency(static_cast<float>(value.toDouble()));
    } else if (n == "phase") {
        setPhase(static_cast<float>(value.toDouble()));
    } else if (n == "waveType") {
        setWaveType(value.toInt());
    } else if (n == "orientation") {
        setOrientation(value.toInt());
    }
}

}
