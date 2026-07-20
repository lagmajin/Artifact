module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <QColor>
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
import Core.Parallel;
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
        const auto noiseAt = [seed = static_cast<uint32_t>(seed_)](
                                 const uint32_t x, const uint32_t y,
                                 const uint32_t channel) {
            uint32_t value = x * 73856093u ^ y * 19349663u ^
                             channel * 83492791u ^ seed;
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            value ^= value >> 16;
            return (static_cast<float>(value & 0x00ffffffu) /
                    16777215.0f) * 2.0f - 1.0f;
        };

        if (monochrome_) {
            Parallel::For(0, mat.rows, [&](int y) {
                for (int x = 0; x < mat.cols; ++x) {
                    const float n = noiseAt(
                        static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1u) *
                        amount_;
                    cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
                    p[0] = std::clamp(p[0] + n, 0.0f, 1.0f);
                    p[1] = std::clamp(p[1] + n, 0.0f, 1.0f);
                    p[2] = std::clamp(p[2] + n, 0.0f, 1.0f);
                }
            });
        } else if (colorNoise_) {
            Parallel::For(0, mat.rows, [&](int y) {
                for (int x = 0; x < mat.cols; ++x) {
                    cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
                    const auto ux = static_cast<uint32_t>(x);
                    const auto uy = static_cast<uint32_t>(y);
                    p[0] = std::clamp(p[0] + noiseAt(ux, uy, 1u) * amount_, 0.0f, 1.0f);
                    p[1] = std::clamp(p[1] + noiseAt(ux, uy, 2u) * amount_, 0.0f, 1.0f);
                    p[2] = std::clamp(p[2] + noiseAt(ux, uy, 3u) * amount_, 0.0f, 1.0f);
                }
            });
        } else {
            // luminance-only noise
            Parallel::For(0, mat.rows, [&](int y) {
                for (int x = 0; x < mat.cols; ++x) {
                    float luma = mat.at<cv::Vec4f>(y, x)[0] * 0.299f
                               + mat.at<cv::Vec4f>(y, x)[1] * 0.587f
                               + mat.at<cv::Vec4f>(y, x)[2] * 0.114f;
                    const float n = noiseAt(
                        static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1u) *
                        amount_;
                    float newLuma = std::clamp(luma + n, 0.0f, 1.0f);
                    float diff = newLuma - luma;
                    cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
                    p[0] = std::clamp(p[0] + diff, 0.0f, 1.0f);
                    p[1] = std::clamp(p[1] + diff, 0.0f, 1.0f);
                    p[2] = std::clamp(p[2] + diff, 0.0f, 1.0f);
                }
            });
        }
    }
};

class AddNoiseEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float amount_=0.15f;
    bool colorNoise_=true;
    bool monochrome_=false;
    int seed_=0;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> params_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> input_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> output_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> staging_;
    Diligent::Uint32 textureWidth_=0;
    Diligent::Uint32 textureHeight_=0;
    bool pipelineReady_=false;
    bool usingSharedDevice_=false;

    ~AddNoiseEffectGPUImpl() override {
        if (context_) {
            context_->Flush();
            context_->WaitForIdle();
        }
        executor_.reset();
        gpuContext_.reset();
        staging_.Release();
        output_.Release();
        input_.Release();
        params_.Release();
        context_.Release();
        device_.Release();
        if (usingSharedDevice_) {
            releaseSharedRenderDevice();
        }
    }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        AddNoiseEffectCPUImpl cpu;
        cpu.amount_=amount_;cpu.colorNoise_=colorNoise_;cpu.monochrome_=monochrome_;cpu.seed_=seed_;
        cpu.applyCPU(src,dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if(!device_||!context_){
            if(!acquireSharedRenderDeviceForCurrentBackend(device_,context_)){applyCPU(src,dst);return;}
            usingSharedDevice_=true;
        }
        if(!executor_){
            gpuContext_=std::make_unique<ArtifactCore::GpuContext>(device_,context_);
            executor_=std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext_);
        }
        const auto& image=src.image();const float* pixels=image.rgba32fData();if(!pixels||image.width()<=0||image.height()<=0){applyCPU(src,dst);return;}
        Diligent::TextureDesc desc{};desc.Name="AddNoise/Input";desc.Type=Diligent::RESOURCE_DIM_TEX_2D;desc.Width=image.width();desc.Height=image.height();desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;desc.MipLevels=1;desc.ArraySize=1;desc.SampleCount=1;desc.Usage=Diligent::USAGE_DEFAULT;desc.BindFlags=Diligent::BIND_SHADER_RESOURCE;
        Diligent::TextureDesc outDesc=desc;outDesc.Name="AddNoise/Output";outDesc.Usage=Diligent::USAGE_DEFAULT;outDesc.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;
        if(!input_||!output_||!staging_||textureWidth_!=outDesc.Width||textureHeight_!=outDesc.Height){
            staging_.Release();output_.Release();input_.Release();
            device_->CreateTexture(desc,nullptr,&input_);if(!input_){applyCPU(src,dst);return;}
            device_->CreateTexture(outDesc,nullptr,&output_);if(!output_){applyCPU(src,dst);return;}
            Diligent::TextureDesc stagingDesc=outDesc;stagingDesc.Name="AddNoise/Readback";stagingDesc.Usage=Diligent::USAGE_STAGING;stagingDesc.BindFlags=Diligent::BIND_NONE;stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ;
            device_->CreateTexture(stagingDesc,nullptr,&staging_);if(!staging_){output_.Release();applyCPU(src,dst);return;}
            textureWidth_=outDesc.Width;textureHeight_=outDesc.Height;
        }
        Diligent::TextureSubResData sub{};sub.pData=pixels;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;
        const Diligent::Box uploadBox(0,outDesc.Width,0,outDesc.Height,0,1);
        context_->UpdateTexture(input_,0,0,uploadBox,sub,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if(!params_){Diligent::BufferDesc cbDesc{};cbDesc.Name="AddNoise/Params";cbDesc.Size=sizeof(Params);cbDesc.Usage=Diligent::USAGE_DYNAMIC;cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER;cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;device_->CreateBuffer(cbDesc,nullptr,&params_);}if(!params_){applyCPU(src,dst);return;}
        void* mapped=nullptr;context_->MapBuffer(params_,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped);if(!mapped){applyCPU(src,dst);return;}Params values{amount_,colorNoise_?1.0f:0.0f,monochrome_?1.0f:0.0f,static_cast<float>(seed_)};std::memcpy(mapped,&values,sizeof(values));context_->UnmapBuffer(params_,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"AddNoiseParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        if(!pipelineReady_){ArtifactCore::ComputePipelineDesc pipeline{};pipeline.name="AddNoise/PSO";pipeline.shaderSource=kHlsl;pipeline.entryPoint="main";pipeline.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pipeline.variables=vars;pipeline.variableCount=3;pipeline.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!executor_->build(pipeline)||!executor_->createShaderResourceBinding(true)||!executor_->setBuffer("AddNoiseParams",params_)){applyCPU(src,dst);return;}pipelineReady_=true;}
        if(!executor_->setTextureView("g_InputTexture",input_->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!executor_->setTextureView("g_OutputTexture",output_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}executor_->dispatch(context_,ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Diligent::CopyTextureAttribs copy(output_,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging_,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);context_->CopyTexture(copy);context_->Flush();context_->WaitForIdle();Diligent::MappedTextureSubresource read{};context_->MapTextureSubresource(staging_,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){applyCPU(src,dst);return;}cv::Mat result(static_cast<int>(outDesc.Height),static_cast<int>(outDesc.Width),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result,image.colorDescriptor());context_->UnmapTextureSubresource(staging_,0,0);
    }
private:
    struct Params{float amount,colorNoise,monochrome,seed;};
    static constexpr const char* kHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer AddNoiseParams:register(b0){float g_Amount;float g_ColorNoise;float g_Monochrome;float g_Seed;}
uint hash(uint v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;return v^(v>>16);}float rnd(uint x,uint y,uint c){return ((hash(x*73856093u^y*19349663u^c*83492791u^asuint(g_Seed))&0x00ffffffu)/16777215.0f)*2.0f-1.0f;}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float4 p=g_InputTexture[id.xy];float n=rnd(id.x,id.y,1u)*g_Amount;if(g_Monochrome>0.5)p.rgb=saturate(p.rgb+n);else if(g_ColorNoise>0.5)p.rgb=saturate(p.rgb+float3(rnd(id.x,id.y,1u),rnd(id.x,id.y,2u),rnd(id.x,id.y,3u))*g_Amount);else{float l=dot(p.rgb,float3(0.299,0.587,0.114));float nl=saturate(l+n);p.rgb=saturate(p.rgb+(nl-l));}g_OutputTexture[id.xy]=p;}
)";
};

AddNoiseEffect::AddNoiseEffect() {
    setDisplayName(UniString("Add Noise"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<AddNoiseEffectCPUImpl>());
    auto gpu=std::make_shared<AddNoiseEffectGPUImpl>();gpu->amount_=amount_;gpu->colorNoise_=colorNoise_;gpu->monochrome_=monochrome_;gpu->seed_=seed_;setGPUImpl(gpu);setComputeMode(ComputeMode::AUTO);
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
        g->amount_ = amount_;
        g->colorNoise_ = colorNoise_;
        g->monochrome_ = monochrome_;
        g->seed_ = seed_;
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
