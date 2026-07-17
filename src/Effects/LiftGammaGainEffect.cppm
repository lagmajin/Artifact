module;
#include <utility>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <QVector>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.LiftGammaGain;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import CvUtils;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

static void applyLiftGammaGainCore(const ImageF32x4RGBAWithCache& src,
                                   ImageF32x4RGBAWithCache& dst,
                                   float liftR, float liftG, float liftB,
                                   float gammaR, float gammaG, float gammaB,
                                   float gainR, float gainG, float gainB) {
    dst = src;
    float* pixels = dst.image().rgba32fData();
    if (!pixels) {
        return;
    }

    const int width = dst.image().width();
    const int height = dst.image().height();
    ArtifactCore::Parallel::For(0, height, [&](int y) {
        for (int x = 0; x < width; ++x) {
            float* p = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
            float& b = p[0];
            float& g = p[1];
            float& r = p[2];

            r += liftR * 0.1f;
            g += liftG * 0.1f;
            b += liftB * 0.1f;

            if (gammaR != 1.0f) r = std::pow(std::max(r, 0.0f), 1.0f / gammaR);
            if (gammaG != 1.0f) g = std::pow(std::max(g, 0.0f), 1.0f / gammaG);
            if (gammaB != 1.0f) b = std::pow(std::max(b, 0.0f), 1.0f / gammaB);

            r *= gainR;
            g *= gainG;
            b *= gainB;

            r = std::clamp(r, 0.0f, 1.0f);
            g = std::clamp(g, 0.0f, 1.0f);
            b = std::clamp(b, 0.0f, 1.0f);
        }
    });
}

class LiftGammaGainCPUImpl : public ArtifactEffectImplBase {
public:
    float liftR_ = 0.0f, liftG_ = 0.0f, liftB_ = 0.0f;
    float gammaR_ = 1.0f, gammaG_ = 1.0f, gammaB_ = 1.0f;
    float gainR_ = 1.0f, gainG_ = 1.0f, gainB_ = 1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyLiftGammaGainCore(src, dst, liftR_, liftG_, liftB_, gammaR_, gammaG_, gammaB_, gainR_, gainG_, gainB_);
    }
};

class LiftGammaGainGPUImpl : public ArtifactEffectImplBase {
public:
    float liftR_ = 0.0f, liftG_ = 0.0f, liftB_ = 0.0f;
    float gammaR_ = 1.0f, gammaG_ = 1.0f, gammaB_ = 1.0f;
    float gainR_ = 1.0f, gainG_ = 1.0f, gainB_ = 1.0f;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyLiftGammaGainCore(src, dst, liftR_, liftG_, liftB_, gammaR_, gammaG_, gammaB_, gainR_, gainG_, gainB_);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src,dst); return; }
        auto gpuContext=std::make_unique<ArtifactCore::GpuContext>(device_,context_); auto executor=std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if(!paramsCB_){Diligent::BufferDesc d;d.Name="LiftGammaGain/Params";d.Size=sizeof(ParamsCB);d.Usage=Diligent::USAGE_DYNAMIC;d.BindFlags=Diligent::BIND_UNIFORM_BUFFER;d.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;device_->CreateBuffer(d,nullptr,&paramsCB_);} if(!paramsCB_){applyCPU(src,dst);return;}
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"LiftGammaGainParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        if(!pipelineReady_){ArtifactCore::ComputePipelineDesc d;d.name="LiftGammaGain/PSO";d.shaderSource=kHlsl;d.entryPoint="main";d.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;d.variables=vars;d.variableCount=3;d.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!executor->build(d)||!executor->createShaderResourceBinding(true)||!executor->setBuffer("LiftGammaGainParams",paramsCB_)){applyCPU(src,dst);return;}pipelineReady_=true;}
        Diligent::RefCntAutoPtr<Diligent::ITexture> input;if(!createTexture(src,&input,"LiftGammaGain/Input")){applyCPU(src,dst);return;}auto od=input->GetDesc();od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_UNORDERED_ACCESS|Diligent::BIND_SHADER_RESOURCE;od.Name="LiftGammaGain/Output";Diligent::RefCntAutoPtr<Diligent::ITexture> output;device_->CreateTexture(od,nullptr,&output);if(!output){applyCPU(src,dst);return;}
        ParamsCB p{liftR_,liftG_,liftB_,0.0f,gammaR_,gammaG_,gammaB_,0.0f,gainR_,gainG_,gainB_,0.0f};void*mapped=nullptr;context_->MapBuffer(paramsCB_,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped);if(!mapped){applyCPU(src,dst);return;}std::memcpy(mapped,&p,sizeof(p));context_->UnmapBuffer(paramsCB_,Diligent::MAP_WRITE);if(!executor->setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!executor->setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}executor->dispatch(context_,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);if(!readback(device_,context_,output,dst,"LiftGammaGain/Readback")){applyCPU(src,dst);}
    }
private:
    struct ParamsCB { float liftR,liftG,liftB,pad1; float gammaR,gammaG,gammaB,pad2; float gainR,gainG,gainB,pad3; };
    static constexpr const char* kHlsl=R"(Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer LiftGammaGainParams:register(b0){float3 lift;float liftPad;float3 gamma;float gammaPad;float3 gain;float gainPad;float3 pad;}[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float4 px=g_InputTexture[id.xy];float3 c=float3(px.b,px.g,px.r);c+=lift*0.1;c.r=pow(max(c.r,0),1.0/max(gamma.r,0.0001));c.g=pow(max(c.g,0),1.0/max(gamma.g,0.0001));c.b=pow(max(c.b,0),1.0/max(gamma.b,0.0001));c*=gain;px.rgb=saturate(float3(c.b,c.g,c.r));g_OutputTexture[id.xy]=px;})";
    bool createTexture(const ImageF32x4RGBAWithCache&src,Diligent::ITexture**out,const char*name){const auto&i=src.image();const float*data=i.rgba32fData();if(!out||!data||i.width()<=0||i.height()<=0)return false;Diligent::TextureDesc d;d.Type=Diligent::RESOURCE_DIM_TEX_2D;d.Width=i.width();d.Height=i.height();d.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;d.ArraySize=1;d.MipLevels=1;d.SampleCount=1;d.Usage=Diligent::USAGE_IMMUTABLE;d.BindFlags=Diligent::BIND_SHADER_RESOURCE;d.Name=name;Diligent::TextureSubResData sub{};sub.pData=data;sub.Stride=static_cast<Diligent::Uint64>(i.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;device_->CreateTexture(d,&init,out);return *out!=nullptr;}
    static bool readback(Diligent::IRenderDevice*dev,Diligent::IDeviceContext*ctx,Diligent::ITexture*src,ImageF32x4RGBAWithCache&dst,const char*name){if(!dev||!ctx||!src)return false;auto d=src->GetDesc();Diligent::TextureDesc s;s.Type=Diligent::RESOURCE_DIM_TEX_2D;s.Width=d.Width;s.Height=d.Height;s.Format=d.Format;s.ArraySize=1;s.MipLevels=1;s.SampleCount=1;s.Usage=Diligent::USAGE_STAGING;s.CPUAccessFlags=Diligent::CPU_ACCESS_READ;s.Name=name;Diligent::RefCntAutoPtr<Diligent::ITexture>staging;dev->CreateTexture(s,nullptr,&staging);if(!staging)return false;ctx->CopyTexture(Diligent::CopyTextureAttribs(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));ctx->Flush();ctx->WaitForIdle();Diligent::MappedTextureSubresource m{};ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,m);if(!m.pData||!m.Stride)return false;cv::Mat temp((int)d.Height,(int)d.Width,CV_32FC4,m.pData,m.Stride);dst.image().setFromCVMat(temp);ctx->UnmapTextureSubresource(staging,0,0);return true;}
};

LiftGammaGainEffect::LiftGammaGainEffect() {
    setDisplayName(ArtifactCore::UniString("Lift / Gamma / Gain"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    setCPUImpl(std::make_shared<LiftGammaGainCPUImpl>());
    setGPUImpl(std::make_shared<LiftGammaGainGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

LiftGammaGainEffect::~LiftGammaGainEffect() = default;

void LiftGammaGainEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<LiftGammaGainCPUImpl*>(cpuImpl().get())) {
        cpu->liftR_ = liftR_; cpu->liftG_ = liftG_; cpu->liftB_ = liftB_;
        cpu->gammaR_ = gammaR_; cpu->gammaG_ = gammaG_; cpu->gammaB_ = gammaB_;
        cpu->gainR_ = gainR_; cpu->gainG_ = gainG_; cpu->gainB_ = gainB_;
    }
    if (auto* gpu = dynamic_cast<LiftGammaGainGPUImpl*>(gpuImpl().get())) {
        gpu->liftR_ = liftR_; gpu->liftG_ = liftG_; gpu->liftB_ = liftB_;
        gpu->gammaR_ = gammaR_; gpu->gammaG_ = gammaG_; gpu->gammaB_ = gammaB_;
        gpu->gainR_ = gainR_; gpu->gainG_ = gainG_; gpu->gainB_ = gainB_;
    }
}

std::vector<AbstractProperty> LiftGammaGainEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    props.push_back({}); props.back().setName("Lift R"); props.back().setType(PropertyType::Float); props.back().setValue(liftR_);
    props.push_back({}); props.back().setName("Lift G"); props.back().setType(PropertyType::Float); props.back().setValue(liftG_);
    props.push_back({}); props.back().setName("Lift B"); props.back().setType(PropertyType::Float); props.back().setValue(liftB_);

    props.push_back({}); props.back().setName("Gamma R"); props.back().setType(PropertyType::Float); props.back().setValue(gammaR_);
    props.push_back({}); props.back().setName("Gamma G"); props.back().setType(PropertyType::Float); props.back().setValue(gammaG_);
    props.push_back({}); props.back().setName("Gamma B"); props.back().setType(PropertyType::Float); props.back().setValue(gammaB_);

    props.push_back({}); props.back().setName("Gain R"); props.back().setType(PropertyType::Float); props.back().setValue(gainR_);
    props.push_back({}); props.back().setName("Gain G"); props.back().setType(PropertyType::Float); props.back().setValue(gainG_);
    props.push_back({}); props.back().setName("Gain B"); props.back().setType(PropertyType::Float); props.back().setValue(gainB_);

    return props;
}

void LiftGammaGainEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == "Lift R") setLiftR(value.toFloat());
    else if (key == "Lift G") setLiftG(value.toFloat());
    else if (key == "Lift B") setLiftB(value.toFloat());
    else if (key == "Gamma R") setGammaR(value.toFloat());
    else if (key == "Gamma G") setGammaG(value.toFloat());
    else if (key == "Gamma B") setGammaB(value.toFloat());
    else if (key == "Gain R") setGainR(value.toFloat());
    else if (key == "Gain G") setGainG(value.toFloat());
    else if (key == "Gain B") setGainB(value.toFloat());
}

} // namespace Artifact
