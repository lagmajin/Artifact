module;
#include <utility>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <QVariant>
#include <opencv2/opencv.hpp>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module ColorWheelsEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ColorCollection.ColorGrading;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageSurfaceView;
import Image.GpuImageUpload;
import Graphics.SurfaceColorContract;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Memory.SharedPtr;

namespace Artifact {

namespace {
constexpr auto kColorWheelsKey = gpuGenericKeyFromString("colorwheels");

// The generic resident prelude supplies P0..P15. Input/output remain canonical
// linear-premultiplied RGBA; grading is evaluated on straight RGB.
constexpr const char* kColorWheelsResidentHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
float wheelLuminance(float3 color) {
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= g_Width || id.y >= g_Height) return;
    float4 pixel = g_InputTexture[id.xy];
    float3 lift = float3(g_P0, g_P1, g_P2);
    const bool threeWay = g_P3 > 2.5;
    float3 gamma = float3(g_P4, g_P5, g_P6) * g_P7;
    float3 gain = float3(g_P8, g_P9, g_P10);
    float3 offset = float3(g_P12, g_P13, g_P14) + g_P15;
    if (pixel.a <= 0.0 ||
        (all(lift == 0.0) && all(gamma == 1.0) &&
         all(gain == 1.0) && g_P11 == 1.0 && all(offset == 0.0))) {
        g_OutputTexture[id.xy] = pixel;
        return;
    }
    float3 color = pixel.rgb / pixel.a;
    float luma = wheelLuminance(color);
    float shadowWeight = threeWay ? 1.0 - saturate(luma * 2.0) : 1.0 - luma;
    if (threeWay) shadowWeight *= shadowWeight;
    float highlightWeight = saturate((luma - 0.5) * 2.0);
    highlightWeight *= highlightWeight;
    color += lift * shadowWeight;
    if (gamma.r != 1.0) color.r = pow(saturate(color.r), 1.0 / gamma.r);
    if (gamma.g != 1.0) color.g = pow(saturate(color.g), 1.0 / gamma.g);
    if (gamma.b != 1.0) color.b = pow(saturate(color.b), 1.0 / gamma.b);
    if (threeWay) {
        color *= 1.0 + (gain - 1.0 + (g_P11 - 1.0)) * highlightWeight;
    } else {
        color *= gain + (g_P11 - 1.0) * wheelLuminance(color);
        if (g_P3 > 1.5) color += offset;
    }
    g_OutputTexture[id.xy] = float4(saturate(color) * pixel.a, pixel.a);
}
)";

void configureWheelProcessor(ArtifactCore::ColorWheelsProcessor& processor,
                             ColorWheelType type, const ColorWheelParams& wheels) {
    processor.setWheelType(type);
    processor.wheels() = wheels;
    // The UI exposes a multiplier with neutral 1; Core stores an additive
    // luminance-weighted gain contribution, whose neutral value is 0.
    processor.wheels().gainMaster = wheels.gainMaster - 1.0f;
    auto& threeWay = processor.threeWay();
    threeWay.shadows.liftR = wheels.liftR + wheels.liftMaster;
    threeWay.shadows.liftG = wheels.liftG + wheels.liftMaster;
    threeWay.shadows.liftB = wheels.liftB + wheels.liftMaster;
    threeWay.midtones.gammaR = wheels.gammaR * wheels.gammaMaster;
    threeWay.midtones.gammaG = wheels.gammaG * wheels.gammaMaster;
    threeWay.midtones.gammaB = wheels.gammaB * wheels.gammaMaster;
    threeWay.highlights.gainR = wheels.gainR - 1.0f + wheels.gainMaster - 1.0f;
    threeWay.highlights.gainG = wheels.gainG - 1.0f + wheels.gainMaster - 1.0f;
    threeWay.highlights.gainB = wheels.gainB - 1.0f + wheels.gainMaster - 1.0f;
}
} // namespace

// Evaluate straight RGB through Core while retaining the host's physical
// channel order and alpha association, including BGRA compatibility buffers.
static void applyColorWheelsCore(const ImageF32x4RGBAWithCache& src,
                                 ImageF32x4RGBAWithCache& dst,
                                 const ArtifactCore::ColorWheelsProcessor& processor,
                                 bool neutral) {
    dst = src;
    if (neutral) return;
    float* pixels = dst.image().rgba32fData();
    if (!pixels) {
        return;
    }

    const int width = dst.image().width();
    const int height = dst.image().height();
    const bool premultiplied = src.image().colorDescriptor().alphaMode ==
                              ArtifactCore::SurfaceAlphaMode::Premultiplied;
    // Invalid/unknown layouts leave the copied input unchanged.
    (void)ArtifactCore::withMutableColorFloat4View(dst.image().surfaceView(), pixels, [&](const auto& view) {
        Parallel::For(0, height, width * height, [&](int y) {
            auto proc = processor;
            const auto row = view.row(y);
            for (int x = 0; x < width; ++x) {
                auto p = row[x];
                if (premultiplied && p.a <= 0.0f) continue;
                const float alphaScale = premultiplied ? p.a : 1.0f;
                float r = p.r / alphaScale;
                float g = p.g / alphaScale;
                float b = p.b / alphaScale;
                proc.processPixel(r, g, b);
                p.r = r * alphaScale;
                p.g = g * alphaScale;
                p.b = b * alphaScale;
            }
        });
    });
}

class ColorWheelsEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ColorWheelsProcessor processor_;
    bool neutral_ = true;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyColorWheelsCore(src, dst, processor_, neutral_);
    }
};

class ColorWheelsEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ColorWheelsProcessor processor_;
    bool neutral_ = true;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
    mutable bool pipelineReady_ = false;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyColorWheelsCore(src, dst, processor_, neutral_);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (neutral_) { dst = src; return; }
        const auto type = processor_.wheelType();
        if (type != ArtifactCore::ColorWheelType::LiftGammaGain && type != ArtifactCore::ColorWheelType::OffsetGammaGain) {
            applyCPU(src, dst);
            return;
        }
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src, dst); return; }
        if (!gpuContext_) {
            gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
            executor_ = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext_);
        }
        if (!executor_) { applyCPU(src, dst); return; }
        if (!paramsCB_) { Diligent::BufferDesc d; d.Name="ColorWheels/Params"; d.Size=sizeof(ParamsCB); d.Usage=Diligent::USAGE_DYNAMIC; d.BindFlags=Diligent::BIND_UNIFORM_BUFFER; d.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(d,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src,dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"ColorWheelsParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc d; d.name="ColorWheels/PSO"; d.shaderSource=kHlsl; d.entryPoint="main"; d.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; d.variables=vars; d.variableCount=3; d.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if(!executor_->build(d)||!executor_->createShaderResourceBinding(true)||!executor_->setBuffer("ColorWheelsParams",paramsCB_)){applyCPU(src,dst);return;} pipelineReady_=true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> input; if(!createTexture(src,&input,"ColorWheels/Input")){applyCPU(src,dst);return;}
        auto od=input->GetDesc(); od.Usage=Diligent::USAGE_DEFAULT; od.BindFlags=Diligent::BIND_UNORDERED_ACCESS|Diligent::BIND_SHADER_RESOURCE; od.Name="ColorWheels/Output"; if(!outputTex_||outputTex_->GetDesc().Width!=od.Width||outputTex_->GetDesc().Height!=od.Height||outputTex_->GetDesc().Format!=od.Format||outputTex_->GetDesc().BindFlags!=od.BindFlags){outputTex_.Release();device_->CreateTexture(od,nullptr,&outputTex_);} if(!outputTex_){applyCPU(src,dst);return;}
        const auto& w=processor_.wheels(); ParamsCB p{w.liftR,w.liftG,w.liftB,w.liftMaster,w.gammaR,w.gammaG,w.gammaB,w.gammaMaster,w.gainR,w.gainG,w.gainB,w.gainMaster,w.offsetR,w.offsetG,w.offsetB,w.offsetMaster,type==ArtifactCore::ColorWheelType::OffsetGammaGain?1.0f:0.0f,src.image().colorDescriptor().alphaMode==ArtifactCore::SurfaceAlphaMode::Premultiplied?1.0f:0.0f}; void* mapped=nullptr; context_->MapBuffer(paramsCB_,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped); if(!mapped){applyCPU(src,dst);return;} std::memcpy(mapped,&p,sizeof(p)); context_->UnmapBuffer(paramsCB_,Diligent::MAP_WRITE);
        if(!executor_->setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!executor_->setTextureView("g_OutputTexture",outputTex_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;} executor_->dispatch(context_,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); if(!readback(device_,context_,outputTex_,dst,src.image().colorDescriptor(),"ColorWheels/Readback")){applyCPU(src,dst);}
    }

private:
    struct ParamsCB { float liftR,liftG,liftB,liftMaster; float gammaR,gammaG,gammaB,gammaMaster; float gainR,gainG,gainB,gainMaster; float offsetR,offsetG,offsetB,offsetMaster; float offsetMode; float premultiplied; float pad[2]{}; };
    static constexpr const char* kHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);
RWTexture2D<float4> g_OutputTexture:register(u0);
cbuffer ColorWheelsParams:register(b0) {
    float3 lift; float liftMaster;
    float3 gamma; float gammaMaster;
    float3 gain; float gainMaster;
    float3 offset; float offsetMaster;
    float offsetMode; float premultiplied; float2 pad;
}
float lum(float3 c) { return dot(c,float3(0.2126,0.7152,0.0722)); }
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID) {
    uint w,h; g_OutputTexture.GetDimensions(w,h);
    if(id.x>=w||id.y>=h) return;
    float4 px=g_InputTexture[id.xy];
    if(premultiplied>0.5 && px.a<=0.0) { g_OutputTexture[id.xy]=px; return; }
    float alphaScale=premultiplied>0.5 ? px.a : 1.0;
    float3 c=px.rgb/alphaScale;
    c+=(lift+liftMaster)*(1.0-lum(c));
    float3 g=gamma*gammaMaster;
    if(g.r!=1.0) c.r=pow(saturate(c.r),1.0/g.r);
    if(g.g!=1.0) c.g=pow(saturate(c.g),1.0/g.g);
    if(g.b!=1.0) c.b=pow(saturate(c.b),1.0/g.b);
    c*=gain+gainMaster*lum(c);
    if(offsetMode>0.5) c+=offset+offsetMaster;
    px.rgb=saturate(c)*alphaScale;
    g_OutputTexture[id.xy]=px;
})";
    bool createTexture(const ImageF32x4RGBAWithCache& src,Diligent::ITexture** out,const char* name){const auto&i=src.image();const auto upload=ArtifactCore::makeGpuImageUploadBuffer(i.surfaceView());if(!upload.isValid()||!out||i.width()<=0||i.height()<=0)return false;Diligent::TextureDesc d;d.Type=Diligent::RESOURCE_DIM_TEX_2D;d.Width=i.width();d.Height=i.height();d.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;d.ArraySize=1;d.MipLevels=1;d.SampleCount=1;d.Usage=Diligent::USAGE_IMMUTABLE;d.BindFlags=Diligent::BIND_SHADER_RESOURCE;d.Name=name;Diligent::TextureSubResData sub{};sub.pData=upload.bytes.data();sub.Stride=static_cast<Diligent::Uint64>(upload.rowStride);Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;device_->CreateTexture(d,&init,out);return *out!=nullptr;}
    // applyConfigured restores the source descriptor. Restore its physical
    // channel order too, so a canonical GPU result is never relabelled BGRA.
    static bool readback(Diligent::IRenderDevice* dev, Diligent::IDeviceContext* ctx,
                         Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst,
                         const ArtifactCore::SurfaceColorDescriptor& descriptor,
                         const char* name) {
        if (!dev || !ctx || !src) return false;
        auto d = src->GetDesc();
        Diligent::TextureDesc s;
        s.Type = Diligent::RESOURCE_DIM_TEX_2D;
        s.Width = d.Width; s.Height = d.Height; s.Format = d.Format;
        s.ArraySize = 1; s.MipLevels = 1; s.SampleCount = 1;
        s.Usage = Diligent::USAGE_STAGING;
        s.CPUAccessFlags = Diligent::CPU_ACCESS_READ; s.Name = name;
        Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
        dev->CreateTexture(s, nullptr, &staging);
        if (!staging) return false;
        ctx->CopyTexture(Diligent::CopyTextureAttribs(
            src, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));
        ctx->Flush(); ctx->WaitForIdle();
        Diligent::MappedTextureSubresource m{};
        ctx->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ,
                                  Diligent::MAP_FLAG_NONE, nullptr, m);
        if (!m.pData) return false;
        if (!m.Stride) { ctx->UnmapTextureSubresource(staging, 0, 0); return false; }
        cv::Mat temp(static_cast<int>(d.Height), static_cast<int>(d.Width),
                     CV_32FC4, m.pData, m.Stride);
        dst.image().setFromCVMat(temp, descriptor);
        ctx->UnmapTextureSubresource(staging, 0, 0);
        if (descriptor.channelOrder == ArtifactCore::SurfaceChannelOrder::BGRA) {
            float* pixels = dst.image().rgba32fData();
            Parallel::For(0, dst.height(), dst.width() * dst.height(), [&](int y) {
                float* row = pixels + static_cast<size_t>(y) * dst.width() * 4u;
                for (int x = 0; x < dst.width(); ++x) std::swap(row[x * 4], row[x * 4 + 2]);
            });
        }
        return true;
    }
};

ColorWheelsEffect::ColorWheelsEffect() {
    setEffectID(UniString("effect.colorcorrection.colorwheels"));
    setDisplayName(UniString("Color Wheels"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<ColorWheelsEffectCPUImpl>());
    setGPUImpl(ArtifactCore::makeShared<ColorWheelsEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    registerGpuGenericShader(kColorWheelsKey,
        GpuGenericShaderRecord{kColorWheelsResidentHlsl, "main",
                               GpuGenericResourceKind::Filter});
    syncImpls();
}

ColorWheelsEffect::~ColorWheelsEffect() = default;

void ColorWheelsEffect::setLift(float r, float g, float b) {
    wheels_.liftR = std::isfinite(r) ? std::clamp(r, -2.0f, 2.0f) : 0.0f;
    wheels_.liftG = std::isfinite(g) ? std::clamp(g, -2.0f, 2.0f) : 0.0f;
    wheels_.liftB = std::isfinite(b) ? std::clamp(b, -2.0f, 2.0f) : 0.0f;
    syncImpls();
}

void ColorWheelsEffect::setGamma(float r, float g, float b) {
    wheels_.gammaR = std::isfinite(r) ? std::clamp(r, 0.1f, 5.0f) : 1.0f;
    wheels_.gammaG = std::isfinite(g) ? std::clamp(g, 0.1f, 5.0f) : 1.0f;
    wheels_.gammaB = std::isfinite(b) ? std::clamp(b, 0.1f, 5.0f) : 1.0f;
    syncImpls();
}

void ColorWheelsEffect::setGain(float r, float g, float b) {
    wheels_.gainR = std::isfinite(r) ? std::clamp(r, 0.0f, 4.0f) : 1.0f;
    wheels_.gainG = std::isfinite(g) ? std::clamp(g, 0.0f, 4.0f) : 1.0f;
    wheels_.gainB = std::isfinite(b) ? std::clamp(b, 0.0f, 4.0f) : 1.0f;
    syncImpls();
}

void ColorWheelsEffect::setOffset(float r, float g, float b) {
    wheels_.offsetR = std::isfinite(r) ? std::clamp(r, -2.0f, 2.0f) : 0.0f;
    wheels_.offsetG = std::isfinite(g) ? std::clamp(g, -2.0f, 2.0f) : 0.0f;
    wheels_.offsetB = std::isfinite(b) ? std::clamp(b, -2.0f, 2.0f) : 0.0f;
    syncImpls();
}

void ColorWheelsEffect::setLiftMaster(float v) {
    wheels_.liftMaster = std::isfinite(v) ? std::clamp(v, -2.0f, 2.0f) : 0.0f;
    syncImpls();
}

void ColorWheelsEffect::setGammaMaster(float v) {
    wheels_.gammaMaster = std::isfinite(v) ? std::clamp(v, 0.1f, 5.0f) : 1.0f;
    syncImpls();
}

void ColorWheelsEffect::setGainMaster(float v) {
    wheels_.gainMaster = std::isfinite(v) ? std::clamp(v, 0.0f, 4.0f) : 1.0f;
    syncImpls();
}

void ColorWheelsEffect::setOffsetMaster(float v) {
    wheels_.offsetMaster = std::isfinite(v) ? std::clamp(v, -2.0f, 2.0f) : 0.0f;
    syncImpls();
}

void ColorWheelsEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<ColorWheelsEffectCPUImpl*>(cpuImpl().get())) {
        configureWheelProcessor(cpu->processor_, wheelType_, wheels_);
        cpu->neutral_ = wheels_.isDefault();
    }
    if (auto* gpu = dynamic_cast<ColorWheelsEffectGPUImpl*>(gpuImpl().get())) {
        configureWheelProcessor(gpu->processor_, wheelType_, wheels_);
        gpu->neutral_ = wheels_.isDefault();
    }
}

std::uint32_t ColorWheelsEffect::gpuGenericKey() const {
    return kColorWheelsKey;
}

bool ColorWheelsEffect::appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const {
    GpuSpatialEffectNode node;
    node.kind = GpuSpatialEffectKind::Generic;
    node.genericKey = kColorWheelsKey;
    node.parameters = {
        wheels_.liftR + wheels_.liftMaster,
        wheels_.liftG + wheels_.liftMaster,
        wheels_.liftB + wheels_.liftMaster,
        wheelType_ == ColorWheelType::ShadowsMidtonesHighlights ? 3.0f :
        wheelType_ == ColorWheelType::OffsetGammaGain ? 2.0f : 1.0f,
        wheels_.gammaR, wheels_.gammaG, wheels_.gammaB, wheels_.gammaMaster,
        wheels_.gainR, wheels_.gainG, wheels_.gainB, wheels_.gainMaster,
        wheels_.offsetR, wheels_.offsetG, wheels_.offsetB, wheels_.offsetMaster
    };
    return stack.append(node);
}

std::vector<AbstractProperty> ColorWheelsEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    auto addFloat = [&props](const char* name, float value) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        const QString key = QString::fromLatin1(name);
        const bool gamma = key.startsWith(QStringLiteral("Gamma"));
        const bool gain = key.startsWith(QStringLiteral("Gain"));
        const bool offset = key.startsWith(QStringLiteral("Offset"));
        const double minimum = gamma ? 0.1 : gain ? 0.0 : -2.0;
        const double maximum = gamma ? 5.0 : gain ? 4.0 : 2.0;
        const double defaultValue = gamma || gain ? 1.0 : 0.0;
        prop.setDefaultValue(defaultValue);
        prop.setHardRange(minimum, maximum);
        prop.setSoftRange(gamma ? 0.5 : gain ? 0.0 : -1.0,
                          gamma ? 2.0 : gain ? 2.0 : 1.0);
        prop.setStep(0.01);
        prop.setTooltip(gamma
            ? QStringLiteral("Midtone multiplier; 1.0 is neutral.")
            : gain ? QStringLiteral("Highlight multiplier; 1.0 is neutral.")
            : offset ? QStringLiteral("Additive color balance; 0.0 is neutral.")
                     : QStringLiteral("Shadow color balance; 0.0 is neutral."));
        props.push_back(prop);
    };

    AbstractProperty modeProp;
    modeProp.setName("Wheel Type");
    modeProp.setDisplayLabel(QStringLiteral("Wheel Model"));
    modeProp.setType(PropertyType::Integer);
    modeProp.setValue(static_cast<int>(wheelType_));
    modeProp.setDefaultValue(static_cast<int>(ColorWheelType::LiftGammaGain));
    modeProp.setHardRange(0, 3);
    modeProp.setTooltip(QStringLiteral(
        "0=RGB, 1=Lift/Gamma/Gain, 2=Offset/Gamma/Gain, 3=Shadows/Midtones/Highlights."));
    props.push_back(modeProp);

    addFloat("Lift Master", wheels_.liftMaster);
    addFloat("Lift R", wheels_.liftR);
    addFloat("Lift G", wheels_.liftG);
    addFloat("Lift B", wheels_.liftB);
    addFloat("Gamma Master", wheels_.gammaMaster);
    addFloat("Gamma R", wheels_.gammaR);
    addFloat("Gamma G", wheels_.gammaG);
    addFloat("Gamma B", wheels_.gammaB);
    addFloat("Gain Master", wheels_.gainMaster);
    addFloat("Gain R", wheels_.gainR);
    addFloat("Gain G", wheels_.gainG);
    addFloat("Gain B", wheels_.gainB);
    addFloat("Offset Master", wheels_.offsetMaster);
    addFloat("Offset R", wheels_.offsetR);
    addFloat("Offset G", wheels_.offsetG);
    addFloat("Offset B", wheels_.offsetB);

    return props;
}

void ColorWheelsEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Wheel Type")) {
        setWheelType(static_cast<ColorWheelType>(value.toInt()));
    } else if (key == QStringLiteral("Lift Master")) {
        setLiftMaster(value.toFloat());
    } else if (key == QStringLiteral("Lift R")) {
        setLift(value.toFloat(), wheels_.liftG, wheels_.liftB);
    } else if (key == QStringLiteral("Lift G")) {
        setLift(wheels_.liftR, value.toFloat(), wheels_.liftB);
    } else if (key == QStringLiteral("Lift B")) {
        setLift(wheels_.liftR, wheels_.liftG, value.toFloat());
    } else if (key == QStringLiteral("Gamma Master")) {
        setGammaMaster(value.toFloat());
    } else if (key == QStringLiteral("Gamma R")) {
        setGamma(value.toFloat(), wheels_.gammaG, wheels_.gammaB);
    } else if (key == QStringLiteral("Gamma G")) {
        setGamma(wheels_.gammaR, value.toFloat(), wheels_.gammaB);
    } else if (key == QStringLiteral("Gamma B")) {
        setGamma(wheels_.gammaR, wheels_.gammaG, value.toFloat());
    } else if (key == QStringLiteral("Gain Master")) {
        setGainMaster(value.toFloat());
    } else if (key == QStringLiteral("Gain R")) {
        setGain(value.toFloat(), wheels_.gainG, wheels_.gainB);
    } else if (key == QStringLiteral("Gain G")) {
        setGain(wheels_.gainR, value.toFloat(), wheels_.gainB);
    } else if (key == QStringLiteral("Gain B")) {
        setGain(wheels_.gainR, wheels_.gainG, value.toFloat());
    } else if (key == QStringLiteral("Offset Master")) {
        setOffsetMaster(value.toFloat());
    } else if (key == QStringLiteral("Offset R")) {
        setOffset(value.toFloat(), wheels_.offsetG, wheels_.offsetB);
    } else if (key == QStringLiteral("Offset G")) {
        setOffset(wheels_.offsetR, value.toFloat(), wheels_.offsetB);
    } else if (key == QStringLiteral("Offset B")) {
        setOffset(wheels_.offsetR, wheels_.offsetG, value.toFloat());
    } else {
        setCommonPropertyValue(key, value);
    }
}

} // namespace Artifact
