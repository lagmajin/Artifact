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
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.LiftGammaGain;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageSurfaceView;
import Image.GpuImageUpload;
import Graphics.SurfaceColorContract;
import Property.Abstract;
import Utils.String.UniString;
import CvUtils;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Memory.SharedPtr;

namespace Artifact {

namespace {
constexpr auto kLiftGammaGainKey = gpuGenericKeyFromString("liftgammagain");
constexpr const char* kLiftGammaGainResidentHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
[numthreads(8,8,1)] void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= g_Width || id.y >= g_Height) return;
    float4 pixel = g_InputTexture[id.xy];
    float3 lift = float3(g_P0, g_P1, g_P2);
    float3 gamma = float3(g_P3, g_P4, g_P5);
    float3 gain = float3(g_P6, g_P7, g_P8);
    if (pixel.a <= 0.0 ||
        (all(lift == 0.0) && all(gamma == 1.0) && all(gain == 1.0))) {
        g_OutputTexture[id.xy] = pixel;
        return;
    }
    float3 color = pixel.rgb / pixel.a + lift * 0.1;
    if (gamma.r != 1.0) color.r = pow(max(color.r, 0.0), 1.0 / gamma.r);
    if (gamma.g != 1.0) color.g = pow(max(color.g, 0.0), 1.0 / gamma.g);
    if (gamma.b != 1.0) color.b = pow(max(color.b, 0.0), 1.0 / gamma.b);
    g_OutputTexture[id.xy] = float4(saturate(color * gain) * pixel.a, pixel.a);
}
)";
}

static void applyLiftGammaGainCore(const ImageF32x4RGBAWithCache& src,
                                   ImageF32x4RGBAWithCache& dst,
                                   float liftR, float liftG, float liftB,
                                   float gammaR, float gammaG, float gammaB,
                                   float gainR, float gainG, float gainB) {
    dst = src;
    if (liftR == 0.0f && liftG == 0.0f && liftB == 0.0f &&
        gammaR == 1.0f && gammaG == 1.0f && gammaB == 1.0f &&
        gainR == 1.0f && gainG == 1.0f && gainB == 1.0f) return;
    float* pixels = dst.image().rgba32fData();
    if (!pixels) {
        return;
    }

    const bool premultiplied = src.image().colorDescriptor().alphaMode ==
                              ArtifactCore::SurfaceAlphaMode::Premultiplied;

    const int width = dst.image().width();
    const int height = dst.image().height();
    // Invalid/unknown layouts leave the copied input unchanged.
    (void)ArtifactCore::withMutableColorFloat4View(dst.image().surfaceView(), pixels, [&](const auto& view) {
        ArtifactCore::Parallel::For(0, height, width * height, [&](int y) {
            const auto row = view.row(y);
            for (int x = 0; x < width; ++x) {
                auto p = row[x];
                if (premultiplied && p.a <= 0.0f) continue;
                const float alphaScale = premultiplied ? p.a : 1.0f;
                float r = p.r / alphaScale;
                float g = p.g / alphaScale;
                float b = p.b / alphaScale;

                r += liftR * 0.1f;
                g += liftG * 0.1f;
                b += liftB * 0.1f;

                if (gammaR != 1.0f) r = std::pow(std::max(r, 0.0f), 1.0f / gammaR);
                if (gammaG != 1.0f) g = std::pow(std::max(g, 0.0f), 1.0f / gammaG);
                if (gammaB != 1.0f) b = std::pow(std::max(b, 0.0f), 1.0f / gammaB);

                r *= gainR;
                g *= gainG;
                b *= gainB;

                p.r = std::clamp(r, 0.0f, 1.0f) * alphaScale;
                p.g = std::clamp(g, 0.0f, 1.0f) * alphaScale;
                p.b = std::clamp(b, 0.0f, 1.0f) * alphaScale;
            }
        });
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
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
    mutable bool pipelineReady_ = false;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyLiftGammaGainCore(src, dst, liftR_, liftG_, liftB_, gammaR_, gammaG_, gammaB_, gainR_, gainG_, gainB_);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (liftR_ == 0.0f && liftG_ == 0.0f && liftB_ == 0.0f &&
            gammaR_ == 1.0f && gammaG_ == 1.0f && gammaB_ == 1.0f &&
            gainR_ == 1.0f && gainG_ == 1.0f && gainB_ == 1.0f) {
            dst = src;
            return;
        }
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src,dst); return; }
        if (!gpuContext_) { gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(device_, context_); executor_ = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext_); }
        if (!executor_) { applyCPU(src, dst); return; }
        if(!paramsCB_){Diligent::BufferDesc d;d.Name="LiftGammaGain/Params";d.Size=sizeof(ParamsCB);d.Usage=Diligent::USAGE_DYNAMIC;d.BindFlags=Diligent::BIND_UNIFORM_BUFFER;d.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;device_->CreateBuffer(d,nullptr,&paramsCB_);} if(!paramsCB_){applyCPU(src,dst);return;}
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"LiftGammaGainParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        if(!pipelineReady_){ArtifactCore::ComputePipelineDesc d;d.name="LiftGammaGain/PSO";d.shaderSource=kHlsl;d.entryPoint="main";d.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;d.variables=vars;d.variableCount=3;d.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!executor_->build(d)||!executor_->createShaderResourceBinding(true)||!executor_->setBuffer("LiftGammaGainParams",paramsCB_)){applyCPU(src,dst);return;}pipelineReady_=true;}
        Diligent::RefCntAutoPtr<Diligent::ITexture> input;
        if(!createTexture(src,&input,"LiftGammaGain/Input")){applyCPU(src,dst);return;}
        auto od=input->GetDesc(); od.Usage=Diligent::USAGE_DEFAULT; od.BindFlags=Diligent::BIND_UNORDERED_ACCESS|Diligent::BIND_SHADER_RESOURCE; od.Name="LiftGammaGain/Output";
        if(!outputTex_||outputTex_->GetDesc().Width!=od.Width||outputTex_->GetDesc().Height!=od.Height||outputTex_->GetDesc().Format!=od.Format||outputTex_->GetDesc().BindFlags!=od.BindFlags){outputTex_.Release();device_->CreateTexture(od,nullptr,&outputTex_);}
        if(!outputTex_){applyCPU(src,dst);return;}
        ParamsCB p{liftR_,liftG_,liftB_,src.image().colorDescriptor().alphaMode==ArtifactCore::SurfaceAlphaMode::Premultiplied?1.0f:0.0f,gammaR_,gammaG_,gammaB_,0.0f,gainR_,gainG_,gainB_,0.0f};void*mapped=nullptr;context_->MapBuffer(paramsCB_,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped);if(!mapped){applyCPU(src,dst);return;}std::memcpy(mapped,&p,sizeof(p));context_->UnmapBuffer(paramsCB_,Diligent::MAP_WRITE);if(!executor_->setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!executor_->setTextureView("g_OutputTexture",outputTex_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}executor_->dispatch(context_,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);if(!readback(device_,context_,outputTex_,dst,src.image().colorDescriptor(),"LiftGammaGain/Readback")){applyCPU(src,dst);}
    }
private:
    struct ParamsCB { float liftR,liftG,liftB,premultiplied; float gammaR,gammaG,gammaB,pad2; float gainR,gainG,gainB,pad3; };
    static constexpr const char* kHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);
RWTexture2D<float4> g_OutputTexture:register(u0);
cbuffer LiftGammaGainParams:register(b0) {
    float3 lift; float premultiplied;
    float3 gamma; float gammaPad;
    float3 gain; float gainPad;
}
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID) {
    uint w,h; g_OutputTexture.GetDimensions(w,h);
    if(id.x>=w||id.y>=h) return;
    float4 px=g_InputTexture[id.xy];
    if(premultiplied>0.5 && px.a<=0.0) {g_OutputTexture[id.xy]=px;return;}
    float alphaScale=premultiplied>0.5 ? px.a : 1.0;
    float3 c=px.rgb/alphaScale+lift*0.1;
    if(gamma.r!=1.0) c.r=pow(max(c.r,0.0),1.0/gamma.r);
    if(gamma.g!=1.0) c.g=pow(max(c.g,0.0),1.0/gamma.g);
    if(gamma.b!=1.0) c.b=pow(max(c.b,0.0),1.0/gamma.b);
    px.rgb=saturate(c*gain)*alphaScale;
    g_OutputTexture[id.xy]=px;
})";
    bool createTexture(const ImageF32x4RGBAWithCache&src,Diligent::ITexture**out,const char*name){const auto&i=src.image();const auto upload=ArtifactCore::makeGpuImageUploadBuffer(i.surfaceView());if(!upload.isValid()||!out||i.width()<=0||i.height()<=0)return false;Diligent::TextureDesc d;d.Type=Diligent::RESOURCE_DIM_TEX_2D;d.Width=i.width();d.Height=i.height();d.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;d.ArraySize=1;d.MipLevels=1;d.SampleCount=1;d.Usage=Diligent::USAGE_IMMUTABLE;d.BindFlags=Diligent::BIND_SHADER_RESOURCE;d.Name=name;Diligent::TextureSubResData sub{};sub.pData=upload.bytes.data();sub.Stride=static_cast<Diligent::Uint64>(upload.rowStride);Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;device_->CreateTexture(d,&init,out);return *out!=nullptr;}
    // Canonical GPU pixels must regain the source channel order before the
    // host restores source metadata in applyConfigured.
    static bool readback(Diligent::IRenderDevice*dev,Diligent::IDeviceContext*ctx,Diligent::ITexture*src,ImageF32x4RGBAWithCache&dst,const ArtifactCore::SurfaceColorDescriptor& descriptor,const char*name){if(!dev||!ctx||!src)return false;auto d=src->GetDesc();ArtifactCore::SurfaceColorDescriptor rgbaDescriptor=descriptor;rgbaDescriptor.channelOrder=ArtifactCore::SurfaceChannelOrder::RGBA;Diligent::TextureDesc s;s.Type=Diligent::RESOURCE_DIM_TEX_2D;s.Width=d.Width;s.Height=d.Height;s.Format=d.Format;s.ArraySize=1;s.MipLevels=1;s.SampleCount=1;s.Usage=Diligent::USAGE_STAGING;s.CPUAccessFlags=Diligent::CPU_ACCESS_READ;s.Name=name;Diligent::RefCntAutoPtr<Diligent::ITexture>staging;dev->CreateTexture(s,nullptr,&staging);if(!staging)return false;ctx->CopyTexture(Diligent::CopyTextureAttribs(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));ctx->Flush();ctx->WaitForIdle();Diligent::MappedTextureSubresource m{};ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,m);if(!m.pData||!m.Stride)return false;cv::Mat temp((int)d.Height,(int)d.Width,CV_32FC4,m.pData,m.Stride);dst.image().setFromCVMat(temp,rgbaDescriptor);if(descriptor.channelOrder==ArtifactCore::SurfaceChannelOrder::BGRA){float* pixels=dst.image().rgba32fData();for(size_t i=0;i<static_cast<size_t>(d.Width)*d.Height;++i)std::swap(pixels[i*4],pixels[i*4+2]);}dst.image().setColorDescriptor(descriptor);ctx->UnmapTextureSubresource(staging,0,0);return true;}
};

LiftGammaGainEffect::LiftGammaGainEffect() {
    setDisplayName(ArtifactCore::UniString("Lift / Gamma / Gain"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    setCPUImpl(ArtifactCore::makeShared<LiftGammaGainCPUImpl>());
    setGPUImpl(ArtifactCore::makeShared<LiftGammaGainGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    registerGpuGenericShader(kLiftGammaGainKey,
        GpuGenericShaderRecord{kLiftGammaGainResidentHlsl, "main", GpuGenericResourceKind::Filter});
}

LiftGammaGainEffect::~LiftGammaGainEffect() = default;

std::uint32_t LiftGammaGainEffect::gpuGenericKey() const { return kLiftGammaGainKey; }

bool LiftGammaGainEffect::appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const {
    GpuSpatialEffectNode node;
    node.kind = GpuSpatialEffectKind::Generic;
    node.genericKey = kLiftGammaGainKey;
    node.parameters[0] = liftR_; node.parameters[1] = liftG_; node.parameters[2] = liftB_;
    node.parameters[3] = gammaR_; node.parameters[4] = gammaG_; node.parameters[5] = gammaB_;
    node.parameters[6] = gainR_; node.parameters[7] = gainG_; node.parameters[8] = gainB_;
    return stack.append(node);
}

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
