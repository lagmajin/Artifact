module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <cmath>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Rasterizer.LinearWipe;

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

class LinearWipeEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float angle_ = 0.0f;
    float softness_ = 10.0f;
    float feather_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) {
            dst = src;
            return;
        }
        dst = src;
        cv::Mat mat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());
        const int w = mat.cols;
        const int h = mat.rows;

        const float rad = angle_ * 3.14159265f / 180.0f;
        const float ca = std::cos(rad);
        const float sa = std::sin(rad);

        Parallel::For(0, h, [&](int y) {
            for (int x = 0; x < w; ++x) {
                float proj = x * ca + y * sa;
                // Normalize to 0..1 across image extent
                const float minProj = std::min(0.0f, std::min(w * ca, h * sa));
                const float maxProj = std::max(0.0f, std::max(w * ca, h * sa));
                float t = (proj - minProj) / std::max(1e-5f, maxProj - minProj);
                t = std::clamp(t, 0.0f, 1.0f);

                const float edge = std::clamp((t - 0.5f) * 2.0f, 0.0f, 1.0f);
                float alpha = 1.0f - edge;
                if (feather_ > 0.0f) {
                    if (alpha < 0.0f) alpha = 0.0f;
                    else alpha = std::clamp(alpha / std::max(1e-5f, feather_), 0.0f, 1.0f);
                }

                cv::Vec4f p = mat.at<cv::Vec4f>(y, x);
                p[3] *= alpha;
                mat.at<cv::Vec4f>(y, x) = p;
            }
        });
    }
};

class LinearWipeEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float angle_=0.0f,softness_=0.1f,feather_=0.0f;
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        LinearWipeEffectCPUImpl cpu;cpu.angle_=angle_;cpu.softness_=softness_;cpu.feather_=feather_;cpu.applyCPU(src,dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;if(!acquireSharedRenderDeviceForCurrentBackend(device,context)){applyCPU(src,dst);return;}
        const auto& image=src.image();const float* pixels=image.rgba32fData();if(!pixels||image.width()<=0||image.height()<=0){applyCPU(src,dst);return;}
        Diligent::TextureDesc desc{};desc.Name="LinearWipe/Input";desc.Type=Diligent::RESOURCE_DIM_TEX_2D;desc.Width=image.width();desc.Height=image.height();desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;desc.MipLevels=1;desc.ArraySize=1;desc.SampleCount=1;desc.Usage=Diligent::USAGE_IMMUTABLE;desc.BindFlags=Diligent::BIND_SHADER_RESOURCE;Diligent::TextureSubResData sub{};sub.pData=pixels;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(desc,&init,&input);if(!input){applyCPU(src,dst);return;}
        Diligent::TextureDesc outDesc=desc;outDesc.Name="LinearWipe/Output";outDesc.Usage=Diligent::USAGE_DEFAULT;outDesc.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(outDesc,nullptr,&output);if(!output){applyCPU(src,dst);return;}
        Diligent::BufferDesc cbDesc{};cbDesc.Name="LinearWipe/Params";cbDesc.Size=sizeof(Params);cbDesc.Usage=Diligent::USAGE_DYNAMIC;cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER;cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(cbDesc,nullptr,&params);if(!params){applyCPU(src,dst);return;}void* mapped=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped);if(!mapped){applyCPU(src,dst);return;}Params values{angle_,feather_};std::memcpy(mapped,&values,sizeof(values));context->UnmapBuffer(params,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"LinearWipeParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gpuContext{device,context};ArtifactCore::ComputeExecutor executor{gpuContext};ArtifactCore::ComputePipelineDesc pipeline{};pipeline.name="LinearWipe/PSO";pipeline.shaderSource=kHlsl;pipeline.entryPoint="main";pipeline.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pipeline.variables=vars;pipeline.variableCount=3;pipeline.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!executor.build(pipeline)||!executor.createShaderResourceBinding(true)||!executor.setBuffer("LinearWipeParams",params)||!executor.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!executor.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}executor.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Diligent::TextureDesc stagingDesc=outDesc;stagingDesc.Name="LinearWipe/Readback";stagingDesc.Usage=Diligent::USAGE_STAGING;stagingDesc.BindFlags=Diligent::BIND_NONE;stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(stagingDesc,nullptr,&staging);if(!staging){applyCPU(src,dst);return;}Diligent::CopyTextureAttribs copy(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);context->CopyTexture(copy);context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){applyCPU(src,dst);return;}cv::Mat result(static_cast<int>(outDesc.Height),static_cast<int>(outDesc.Width),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result);context->UnmapTextureSubresource(staging,0,0);
    }
private:
    struct Params{float angle,feather,pad0=0.0f,pad1=0.0f;};
    static constexpr const char* kHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer LinearWipeParams:register(b0){float g_Angle;float g_Feather;float2 g_Pad;}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float rad=g_Angle*3.14159265/180.0;float ca=cos(rad),sa=sin(rad);float proj=(float)id.x*ca+(float)id.y*sa;float minP=min(0.0,min((float)w*ca,(float)h*sa));float maxP=max(0.0,max((float)w*ca,(float)h*sa));float t=saturate((proj-minP)/max(0.00001,maxP-minP));float alpha=1.0-saturate((t-0.5)*2.0);if(g_Feather>0.0)alpha=saturate(alpha/max(0.00001,g_Feather));float4 c=g_InputTexture[id.xy];c.a*=alpha;g_OutputTexture[id.xy]=c;}
)";
};

LinearWipeEffect::LinearWipeEffect() {
    setDisplayName(UniString("Linear Wipe"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<LinearWipeEffectCPUImpl>());
    auto gpu=std::make_shared<LinearWipeEffectGPUImpl>();gpu->angle_=angle_;gpu->softness_=softness_;gpu->feather_=feather_;setGPUImpl(gpu);setComputeMode(ComputeMode::AUTO);
}
LinearWipeEffect::~LinearWipeEffect() = default;

float LinearWipeEffect::angle() const { return angle_; }
void LinearWipeEffect::setAngle(float v) { angle_ = std::fmod(v, 360.0f); if (angle_ < 0) angle_ += 360.0f; syncImpls(); }
float LinearWipeEffect::softness() const { return softness_; }
void LinearWipeEffect::setSoftness(float v) { softness_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float LinearWipeEffect::feather() const { return feather_; }
void LinearWipeEffect::setFeather(float v) { feather_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

void LinearWipeEffect::syncImpls() {
    if (auto* c = dynamic_cast<LinearWipeEffectCPUImpl*>(cpuImpl().get())) {
        c->angle_ = angle_;
        c->softness_ = softness_;
        c->feather_ = feather_;
    }
    if (auto* g = dynamic_cast<LinearWipeEffectGPUImpl*>(gpuImpl().get())) {
        g->angle_ = angle_;
        g->softness_ = softness_;
        g->feather_ = feather_;
    }
}

std::vector<AbstractProperty> LinearWipeEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Angle"); a.setType(PropertyType::Float); a.setValue(angle_);
    auto& s = props.emplace_back(); s.setName("Softness"); s.setType(PropertyType::Float); s.setValue(softness_);
    auto& f = props.emplace_back(); f.setName("Feather"); f.setType(PropertyType::Float); f.setValue(feather_);
    return props;
}

void LinearWipeEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Angle") setAngle(v.toFloat());
    else if (k == "Softness") setSoftness(v.toFloat());
    else if (k == "Feather") setFeather(v.toFloat());
}

} // namespace Artifact
