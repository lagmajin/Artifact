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

        if (monochrome_) {
            Parallel::For(0, mat.rows, [&](int y) {
                std::mt19937 rng(static_cast<unsigned>(seed_ + y * 747796405u));
                std::uniform_real_distribution<float> dist(-amount_, amount_);
                for (int x = 0; x < mat.cols; ++x) {
                    float n = dist(rng);
                    cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
                    p[0] = std::clamp(p[0] + n, 0.0f, 1.0f);
                    p[1] = std::clamp(p[1] + n, 0.0f, 1.0f);
                    p[2] = std::clamp(p[2] + n, 0.0f, 1.0f);
                }
            });
        } else if (colorNoise_) {
            Parallel::For(0, mat.rows, [&](int y) {
                std::mt19937 rng(static_cast<unsigned>(seed_ + y * 747796405u));
                std::uniform_real_distribution<float> dist(-amount_, amount_);
                for (int x = 0; x < mat.cols; ++x) {
                    cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
                    p[0] = std::clamp(p[0] + dist(rng), 0.0f, 1.0f);
                    p[1] = std::clamp(p[1] + dist(rng), 0.0f, 1.0f);
                    p[2] = std::clamp(p[2] + dist(rng), 0.0f, 1.0f);
                }
            });
        } else {
            // luminance-only noise
            Parallel::For(0, mat.rows, [&](int y) {
                std::mt19937 rng(static_cast<unsigned>(seed_ + y * 747796405u));
                std::uniform_real_distribution<float> dist(-amount_, amount_);
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

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        AddNoiseEffectCPUImpl cpu;
        cpu.amount_=amount_;cpu.colorNoise_=colorNoise_;cpu.monochrome_=monochrome_;cpu.seed_=seed_;
        cpu.applyCPU(src,dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if(!acquireSharedRenderDeviceForCurrentBackend(device,context)){applyCPU(src,dst);return;}
        const auto& image=src.image();const float* pixels=image.rgba32fData();if(!pixels||image.width()<=0||image.height()<=0){applyCPU(src,dst);return;}
        Diligent::TextureDesc desc{};desc.Name="AddNoise/Input";desc.Type=Diligent::RESOURCE_DIM_TEX_2D;desc.Width=image.width();desc.Height=image.height();desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;desc.MipLevels=1;desc.ArraySize=1;desc.SampleCount=1;desc.Usage=Diligent::USAGE_IMMUTABLE;desc.BindFlags=Diligent::BIND_SHADER_RESOURCE;
        Diligent::TextureSubResData sub{};sub.pData=pixels;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(desc,&init,&input);if(!input){applyCPU(src,dst);return;}
        Diligent::TextureDesc outDesc=desc;outDesc.Name="AddNoise/Output";outDesc.Usage=Diligent::USAGE_DEFAULT;outDesc.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(outDesc,nullptr,&output);if(!output){applyCPU(src,dst);return;}
        Diligent::BufferDesc cbDesc{};cbDesc.Name="AddNoise/Params";cbDesc.Size=sizeof(Params);cbDesc.Usage=Diligent::USAGE_DYNAMIC;cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER;cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(cbDesc,nullptr,&params);if(!params){applyCPU(src,dst);return;}
        void* mapped=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped);if(!mapped){applyCPU(src,dst);return;}Params values{amount_,colorNoise_?1.0f:0.0f,monochrome_?1.0f:0.0f,static_cast<float>(seed_)};std::memcpy(mapped,&values,sizeof(values));context->UnmapBuffer(params,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"AddNoiseParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gpuContext{device,context};ArtifactCore::ComputeExecutor executor{gpuContext};ArtifactCore::ComputePipelineDesc pipeline{};pipeline.name="AddNoise/PSO";pipeline.shaderSource=kHlsl;pipeline.entryPoint="main";pipeline.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pipeline.variables=vars;pipeline.variableCount=3;pipeline.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        if(!executor.build(pipeline)||!executor.createShaderResourceBinding(true)||!executor.setBuffer("AddNoiseParams",params)||!executor.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!executor.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}executor.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Diligent::TextureDesc stagingDesc=outDesc;stagingDesc.Name="AddNoise/Readback";stagingDesc.Usage=Diligent::USAGE_STAGING;stagingDesc.BindFlags=Diligent::BIND_NONE;stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(stagingDesc,nullptr,&staging);if(!staging){applyCPU(src,dst);return;}Diligent::CopyTextureAttribs copy(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);context->CopyTexture(copy);context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){applyCPU(src,dst);return;}cv::Mat result(static_cast<int>(outDesc.Height),static_cast<int>(outDesc.Width),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result);context->UnmapTextureSubresource(staging,0,0);
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
