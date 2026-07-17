module;
#include <utility>

#include <algorithm>
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
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

class ColorWheelsEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ColorWheelsProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        Parallel::For(0, height, [&](int y) {
            auto processor = processor_;
            processor.process(pixels + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u, width, 1);
        });
    }
};

class ColorWheelsEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::ColorWheelsProcessor processor_;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        Parallel::For(0, height, [&](int y) {
            auto processor = processor_;
            processor.process(pixels + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u, width, 1);
        });
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        const auto type = processor_.wheelType();
        if (type != ArtifactCore::ColorWheelType::LiftGammaGain && type != ArtifactCore::ColorWheelType::OffsetGammaGain) {
            applyCPU(src, dst);
            return;
        }
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src, dst); return; }
        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if (!paramsCB_) { Diligent::BufferDesc d; d.Name="ColorWheels/Params"; d.Size=sizeof(ParamsCB); d.Usage=Diligent::USAGE_DYNAMIC; d.BindFlags=Diligent::BIND_UNIFORM_BUFFER; d.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(d,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src,dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"ColorWheelsParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc d; d.name="ColorWheels/PSO"; d.shaderSource=kHlsl; d.entryPoint="main"; d.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; d.variables=vars; d.variableCount=3; d.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if(!executor->build(d)||!executor->createShaderResourceBinding(true)||!executor->setBuffer("ColorWheelsParams",paramsCB_)){applyCPU(src,dst);return;} pipelineReady_=true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> input; if(!createTexture(src,&input,"ColorWheels/Input")){applyCPU(src,dst);return;}
        auto od=input->GetDesc(); od.Usage=Diligent::USAGE_DEFAULT; od.BindFlags=Diligent::BIND_UNORDERED_ACCESS|Diligent::BIND_SHADER_RESOURCE; od.Name="ColorWheels/Output"; Diligent::RefCntAutoPtr<Diligent::ITexture> output; device_->CreateTexture(od,nullptr,&output); if(!output){applyCPU(src,dst);return;}
        const auto& w=processor_.wheels(); ParamsCB p{w.liftR,w.liftG,w.liftB,w.liftMaster,w.gammaR,w.gammaG,w.gammaB,w.gammaMaster,w.gainR,w.gainG,w.gainB,w.gainMaster,w.offsetR,w.offsetG,w.offsetB,w.offsetMaster,type==ArtifactCore::ColorWheelType::OffsetGammaGain?1.0f:0.0f}; void* mapped=nullptr; context_->MapBuffer(paramsCB_,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped); if(!mapped){applyCPU(src,dst);return;} std::memcpy(mapped,&p,sizeof(p)); context_->UnmapBuffer(paramsCB_,Diligent::MAP_WRITE);
        if(!executor->setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!executor->setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;} executor->dispatch(context_,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); if(!readback(device_,context_,output,dst,"ColorWheels/Readback")){applyCPU(src,dst);}
    }

private:
    struct ParamsCB { float liftR,liftG,liftB,liftMaster; float gammaR,gammaG,gammaB,gammaMaster; float gainR,gainG,gainB,gainMaster; float offsetR,offsetG,offsetB,offsetMaster; float offsetMode; float pad[3]{}; };
    static constexpr const char* kHlsl=R"(Texture2D<float4> g_InputTexture:register(t0); RWTexture2D<float4> g_OutputTexture:register(u0); cbuffer ColorWheelsParams:register(b0){float3 lift;float liftMaster;float3 gamma;float gammaMaster;float3 gain;float gainMaster;float3 offset;float offsetMaster;float offsetMode;float3 pad;} float lum(float3 c){return dot(c,float3(0.2126,0.7152,0.0722));} [numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float4 px=g_InputTexture[id.xy];float3 c=px.rgb;float l=lum(c);c+=(lift+liftMaster)*(1.0-l);c*=gain+gainMaster*l;c.r=pow(saturate(c.r),1.0/max(0.0001,gamma.r*gammaMaster));c.g=pow(saturate(c.g),1.0/max(0.0001,gamma.g*gammaMaster));c.b=pow(saturate(c.b),1.0/max(0.0001,gamma.b*gammaMaster));if(offsetMode>0.5)c+=offset+offsetMaster;px.rgb=saturate(c);g_OutputTexture[id.xy]=px;})";
    bool createTexture(const ImageF32x4RGBAWithCache& src,Diligent::ITexture** out,const char* name){const auto&i=src.image();const float*data=i.rgba32fData();if(!out||!data||i.width()<=0||i.height()<=0)return false;Diligent::TextureDesc d;d.Type=Diligent::RESOURCE_DIM_TEX_2D;d.Width=i.width();d.Height=i.height();d.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;d.ArraySize=1;d.MipLevels=1;d.SampleCount=1;d.Usage=Diligent::USAGE_IMMUTABLE;d.BindFlags=Diligent::BIND_SHADER_RESOURCE;d.Name=name;Diligent::TextureSubResData sub{};sub.pData=data;sub.Stride=static_cast<Diligent::Uint64>(i.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;device_->CreateTexture(d,&init,out);return *out!=nullptr;}
    static bool readback(Diligent::IRenderDevice*dev,Diligent::IDeviceContext*ctx,Diligent::ITexture*src,ImageF32x4RGBAWithCache&dst,const char*name){if(!dev||!ctx||!src)return false;auto d=src->GetDesc();Diligent::TextureDesc s;s.Type=Diligent::RESOURCE_DIM_TEX_2D;s.Width=d.Width;s.Height=d.Height;s.Format=d.Format;s.ArraySize=1;s.MipLevels=1;s.SampleCount=1;s.Usage=Diligent::USAGE_STAGING;s.CPUAccessFlags=Diligent::CPU_ACCESS_READ;s.Name=name;Diligent::RefCntAutoPtr<Diligent::ITexture>staging;dev->CreateTexture(s,nullptr,&staging);if(!staging)return false;ctx->CopyTexture(Diligent::CopyTextureAttribs(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));ctx->Flush();ctx->WaitForIdle();Diligent::MappedTextureSubresource m{};ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,m);if(!m.pData||!m.Stride)return false;cv::Mat temp((int)d.Height,(int)d.Width,CV_32FC4,m.pData,m.Stride);dst.image().setFromCVMat(temp);ctx->UnmapTextureSubresource(staging,0,0);return true;}
};

ColorWheelsEffect::ColorWheelsEffect() {
    setEffectID(UniString("effect.colorcorrection.colorwheels"));
    setDisplayName(UniString("Color Wheels"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ColorWheelsEffectCPUImpl>());
    setGPUImpl(std::make_shared<ColorWheelsEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    syncImpls();
}

ColorWheelsEffect::~ColorWheelsEffect() = default;

void ColorWheelsEffect::setLift(float r, float g, float b) {
    wheels_.liftR = std::clamp(r, -2.0f, 2.0f);
    wheels_.liftG = std::clamp(g, -2.0f, 2.0f);
    wheels_.liftB = std::clamp(b, -2.0f, 2.0f);
    syncImpls();
}

void ColorWheelsEffect::setGamma(float r, float g, float b) {
    wheels_.gammaR = std::clamp(r, 0.1f, 5.0f);
    wheels_.gammaG = std::clamp(g, 0.1f, 5.0f);
    wheels_.gammaB = std::clamp(b, 0.1f, 5.0f);
    syncImpls();
}

void ColorWheelsEffect::setGain(float r, float g, float b) {
    wheels_.gainR = std::clamp(r, 0.0f, 4.0f);
    wheels_.gainG = std::clamp(g, 0.0f, 4.0f);
    wheels_.gainB = std::clamp(b, 0.0f, 4.0f);
    syncImpls();
}

void ColorWheelsEffect::setOffset(float r, float g, float b) {
    wheels_.offsetR = std::clamp(r, -2.0f, 2.0f);
    wheels_.offsetG = std::clamp(g, -2.0f, 2.0f);
    wheels_.offsetB = std::clamp(b, -2.0f, 2.0f);
    syncImpls();
}

void ColorWheelsEffect::setLiftMaster(float v) {
    wheels_.liftMaster = std::clamp(v, -2.0f, 2.0f);
    syncImpls();
}

void ColorWheelsEffect::setGammaMaster(float v) {
    wheels_.gammaMaster = std::clamp(v, 0.1f, 5.0f);
    syncImpls();
}

void ColorWheelsEffect::setGainMaster(float v) {
    wheels_.gainMaster = std::clamp(v, 0.0f, 4.0f);
    syncImpls();
}

void ColorWheelsEffect::setOffsetMaster(float v) {
    wheels_.offsetMaster = std::clamp(v, -2.0f, 2.0f);
    syncImpls();
}

void ColorWheelsEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<ColorWheelsEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setWheelType(wheelType_);
        cpu->processor_.wheels() = wheels_;
    }
    if (auto* gpu = dynamic_cast<ColorWheelsEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setWheelType(wheelType_);
        gpu->processor_.wheels() = wheels_;
    }
}

std::vector<AbstractProperty> ColorWheelsEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    auto addFloat = [&props](const char* name, float value) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        props.push_back(prop);
    };

    AbstractProperty modeProp;
    modeProp.setName("Wheel Type");
    modeProp.setType(PropertyType::Integer);
    modeProp.setValue(static_cast<int>(wheelType_));
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
    }
}

} // namespace Artifact
