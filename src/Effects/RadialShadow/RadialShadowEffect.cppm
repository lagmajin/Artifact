module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <QColor>
#include <cmath>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Rasterizer.RadialShadow;

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

class RadialShadowEffectCPUImpl : public ArtifactEffectImplBase {
public:
    QColor color_ = QColor(0, 0, 0, 180);
    float distance_ = 10.0f;
    float softness_ = 8.0f;
    float opacity_ = 0.75f;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;

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
        const float cx = centerX_ * w;
        const float cy = centerY_ * h;

        cv::Mat colorMat(h, w, CV_32FC4);
        ArtifactCore::Parallel::For(0, h, [&](int y) {
            for (int x = 0; x < w; ++x) {
                float dx = x - cx;
                float dy = y - cy;
                float dist = std::sqrt(dx * dx + dy * dy);
                float shadow = dist / std::max(1.0f, distance_ + softness_);
                float alpha = 1.0f - std::clamp(shadow, 0.0f, 1.0f);
                alpha *= opacity_;
                colorMat.at<cv::Vec4f>(y, x) = cv::Vec4f(
                    color_.blueF(),
                    color_.greenF(),
                    color_.redF(),
                    alpha
                );
            }
        });

        // Alpha compositing: shadow over src
        cv::Mat srcMat = mat.clone();
        cv::addWeighted(srcMat, 1.0, colorMat, 1.0, 0.0, mat);
    }
};

class RadialShadowEffectGPUImpl : public ArtifactEffectImplBase {
public:
    RadialShadowEffectCPUImpl cpuImpl_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src,dst); return; }
        auto gpuContext=std::make_unique<ArtifactCore::GpuContext>(device_,context_); auto executor=std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if(!paramsCB_){Diligent::BufferDesc d;d.Name="RadialShadow/Params";d.Size=sizeof(ParamsCB);d.Usage=Diligent::USAGE_DYNAMIC;d.BindFlags=Diligent::BIND_UNIFORM_BUFFER;d.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;device_->CreateBuffer(d,nullptr,&paramsCB_);}if(!paramsCB_){applyCPU(src,dst);return;}
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"RadialShadowParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        if(!pipelineReady_){ArtifactCore::ComputePipelineDesc d;d.name="RadialShadow/PSO";d.shaderSource=kHlsl;d.entryPoint="main";d.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;d.variables=vars;d.variableCount=3;d.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!executor->build(d)||!executor->createShaderResourceBinding(true)||!executor->setBuffer("RadialShadowParams",paramsCB_)){applyCPU(src,dst);return;}pipelineReady_=true;}
        Diligent::RefCntAutoPtr<Diligent::ITexture> input;if(!createTexture(src,&input,"RadialShadow/Input")){applyCPU(src,dst);return;}auto od=input->GetDesc();od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_UNORDERED_ACCESS|Diligent::BIND_SHADER_RESOURCE;od.Name="RadialShadow/Output";Diligent::RefCntAutoPtr<Diligent::ITexture> output;device_->CreateTexture(od,nullptr,&output);if(!output){applyCPU(src,dst);return;}
        const auto& c=cpuImpl_;ParamsCB p{c.color_.redF(),c.color_.greenF(),c.color_.blueF(),c.centerX_,c.centerY_,c.distance_,c.softness_,c.opacity_};void*mapped=nullptr;context_->MapBuffer(paramsCB_,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped);if(!mapped){applyCPU(src,dst);return;}std::memcpy(mapped,&p,sizeof(p));context_->UnmapBuffer(paramsCB_,Diligent::MAP_WRITE);if(!executor->setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!executor->setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}executor->dispatch(context_,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);if(!readback(device_,context_,output,dst,"RadialShadow/Readback")){applyCPU(src,dst);}
    }
private:
    struct ParamsCB{float r,g,b,cx,cy,distance,softness,opacity;};
    static constexpr const char* kHlsl=R"(Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer RadialShadowParams:register(b0){float3 g_Color;float g_CenterX;float g_CenterY;float g_Distance;float g_Softness;float g_Opacity;}[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float4 px=g_InputTexture[id.xy];float2 center=float2(g_CenterX*w,g_CenterY*h);float2 d=float2(id.xy)-center;float dist=length(d);float shadow=dist/max(1.0,g_Distance+g_Softness);float a=(1.0-saturate(shadow))*g_Opacity;px.rgb=saturate(px.rgb+g_Color*a);px.a=saturate(px.a+a);g_OutputTexture[id.xy]=px;})";
    bool createTexture(const ImageF32x4RGBAWithCache&src,Diligent::ITexture**out,const char*name){const auto&i=src.image();const float*data=i.rgba32fData();if(!out||!data||i.width()<=0||i.height()<=0)return false;Diligent::TextureDesc d;d.Type=Diligent::RESOURCE_DIM_TEX_2D;d.Width=i.width();d.Height=i.height();d.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;d.ArraySize=1;d.MipLevels=1;d.SampleCount=1;d.Usage=Diligent::USAGE_IMMUTABLE;d.BindFlags=Diligent::BIND_SHADER_RESOURCE;d.Name=name;Diligent::TextureSubResData sub{};sub.pData=data;sub.Stride=static_cast<Diligent::Uint64>(i.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;device_->CreateTexture(d,&init,out);return *out!=nullptr;}
    static bool readback(Diligent::IRenderDevice*dev,Diligent::IDeviceContext*ctx,Diligent::ITexture*src,ImageF32x4RGBAWithCache&dst,const char*name){if(!dev||!ctx||!src)return false;auto d=src->GetDesc();Diligent::TextureDesc s;s.Type=Diligent::RESOURCE_DIM_TEX_2D;s.Width=d.Width;s.Height=d.Height;s.Format=d.Format;s.ArraySize=1;s.MipLevels=1;s.SampleCount=1;s.Usage=Diligent::USAGE_STAGING;s.CPUAccessFlags=Diligent::CPU_ACCESS_READ;s.Name=name;Diligent::RefCntAutoPtr<Diligent::ITexture>staging;dev->CreateTexture(s,nullptr,&staging);if(!staging)return false;ctx->CopyTexture(Diligent::CopyTextureAttribs(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));ctx->Flush();ctx->WaitForIdle();Diligent::MappedTextureSubresource m{};ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,m);if(!m.pData||!m.Stride)return false;cv::Mat temp((int)d.Height,(int)d.Width,CV_32FC4,m.pData,m.Stride);dst.image().setFromCVMat(temp);ctx->UnmapTextureSubresource(staging,0,0);return true;}
};

RadialShadowEffect::RadialShadowEffect() {
    setDisplayName(UniString("Radial Shadow"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<RadialShadowEffectCPUImpl>());
    setGPUImpl(std::make_shared<RadialShadowEffectGPUImpl>());
}
RadialShadowEffect::~RadialShadowEffect() = default;

QColor RadialShadowEffect::color() const { return color_; }
void RadialShadowEffect::setColor(const QColor& v) { color_ = v; syncImpls(); }
float RadialShadowEffect::distance() const { return distance_; }
void RadialShadowEffect::setDistance(float v) { distance_ = std::max(0.0f, v); syncImpls(); }
float RadialShadowEffect::softness() const { return softness_; }
void RadialShadowEffect::setSoftness(float v) { softness_ = std::max(0.0f, v); syncImpls(); }
float RadialShadowEffect::opacity() const { return opacity_; }
void RadialShadowEffect::setOpacity(float v) { opacity_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float RadialShadowEffect::centerX() const { return centerX_; }
void RadialShadowEffect::setCenterX(float v) { centerX_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float RadialShadowEffect::centerY() const { return centerY_; }
void RadialShadowEffect::setCenterY(float v) { centerY_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

void RadialShadowEffect::syncImpls() {
    if (auto* c = dynamic_cast<RadialShadowEffectCPUImpl*>(cpuImpl().get())) {
        c->color_ = color_;
        c->distance_ = distance_;
        c->softness_ = softness_;
        c->opacity_ = opacity_;
        c->centerX_ = centerX_;
        c->centerY_ = centerY_;
    }
    if (auto* g = dynamic_cast<RadialShadowEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.color_ = color_;
        g->cpuImpl_.distance_ = distance_;
        g->cpuImpl_.softness_ = softness_;
        g->cpuImpl_.opacity_ = opacity_;
        g->cpuImpl_.centerX_ = centerX_;
        g->cpuImpl_.centerY_ = centerY_;
    }
}

std::vector<AbstractProperty> RadialShadowEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& c = props.emplace_back(); c.setName("Color"); c.setType(PropertyType::Color); c.setValue(color_);
    auto& d = props.emplace_back(); d.setName("Distance"); d.setType(PropertyType::Float); d.setValue(distance_);
    auto& s = props.emplace_back(); s.setName("Softness"); s.setType(PropertyType::Float); s.setValue(softness_);
    auto& o = props.emplace_back(); o.setName("Opacity"); o.setType(PropertyType::Float); o.setValue(opacity_);
    auto& cx = props.emplace_back(); cx.setName("Center X"); cx.setType(PropertyType::Float); cx.setValue(centerX_);
    auto& cy = props.emplace_back(); cy.setName("Center Y"); cy.setType(PropertyType::Float); cy.setValue(centerY_);
    return props;
}

void RadialShadowEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Color") setColor(v.value<QColor>());
    else if (k == "Distance") setDistance(v.toFloat());
    else if (k == "Softness") setSoftness(v.toFloat());
    else if (k == "Opacity") setOpacity(v.toFloat());
    else if (k == "Center X") setCenterX(v.toFloat());
    else if (k == "Center Y") setCenterY(v.toFloat());
}

} // namespace Artifact
