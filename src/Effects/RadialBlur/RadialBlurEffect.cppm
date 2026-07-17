module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Rasterizer.RadialBlur;

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

class RadialBlurEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float amount_ = 10.0f;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    int samples_ = 16;
    int type_ = 0;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) { dst = src; return; }
        if (samples_ <= 1) { dst = src; return; }

        dst = src;
        cv::Mat dstMat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());

        const int w = srcImage.width();
        const int h = srcImage.height();
        const float cx = centerX_ * w;
        const float cy = centerY_ * h;
        const float amt = amount_ / std::max(1, samples_ - 1);

        cv::Mat out = cv::Mat::zeros(dstMat.size(), CV_32FC4);

        for (int s = 0; s < samples_; ++s) {
            const float t = samples_ > 1 ? static_cast<float>(s) / (samples_ - 1) : 0.0f;
            float dx = 0.0f, dy = 0.0f;
            if (type_ == 0) {
                const float angle = t * amount_ * 3.14159265f / 180.0f;
                dx = std::cos(angle) * t * amount_;
                dy = std::sin(angle) * t * amount_;
            } else {
                dx = 0.0f;
                dy = t * amount_;
            }

            cv::Mat mapX(h, w, CV_32FC1);
            cv::Mat mapY(h, w, CV_32FC1);
            ArtifactCore::Parallel::For(0, h, [&](int y) {
                for (int x = 0; x < w; ++x) {
                    mapX.at<float>(y, x) = std::clamp(x - dx * (1.0f - std::abs(y - cy) / (h * 0.5f + 1e-4f)), 0.0f, w - 1.0f); // sticky fallback
                    mapY.at<float>(y, x) = std::clamp(y - dy, 0.0f, h - 1.0f); // intentionally offset only; actual remap done per frame below
                }
            });

            cv::Mat sample(h, w, CV_32FC4);
            ArtifactCore::Parallel::For(0, h, [&](int y) {
                const float* xRow = mapX.ptr<float>(y);
                const float* yRow = mapY.ptr<float>(y);
                cv::Vec4f* outRow = sample.ptr<cv::Vec4f>(y);
                for (int x = 0; x < w; ++x) {
                    const float sx = xRow[x];
                    const float sy = yRow[x];
                    const int x0 = static_cast<int>(sx);
                    const int y0 = static_cast<int>(sy);
                    const int x1 = std::min(x0 + 1, w - 1);
                    const int y1 = std::min(y0 + 1, h - 1);
                    const float tx = sx - x0;
                    const float ty = sy - y0;
                    const cv::Vec4f& p00 = dstMat.at<cv::Vec4f>(y0, x0);
                    const cv::Vec4f& p10 = dstMat.at<cv::Vec4f>(y0, x1);
                    const cv::Vec4f& p01 = dstMat.at<cv::Vec4f>(y1, x0);
                    const cv::Vec4f& p11 = dstMat.at<cv::Vec4f>(y1, x1);
                    for (int c = 0; c < 4; ++c) {
                        const float top = p00[c] + (p10[c] - p00[c]) * tx;
                        const float bottom = p01[c] + (p11[c] - p01[c]) * tx;
                        outRow[x][c] = top + (bottom - top) * ty;
                    }
                }
            });
            out += sample;
        }
        out /= static_cast<float>(samples_);
        out.copyTo(dstMat);
    }
};

class RadialBlurEffectGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device; Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if (!acquireSharedRenderDeviceForCurrentBackend(device, context)) { applyCPU(src, dst); return; }
        const auto& image=src.image(); const float* data=image.rgba32fData(); if(!data||image.width()<=0||image.height()<=0){applyCPU(src,dst);return;}
        Diligent::TextureDesc d{}; d.Name="RadialBlur/Input"; d.Type=Diligent::RESOURCE_DIM_TEX_2D; d.Width=image.width(); d.Height=image.height(); d.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; d.MipLevels=1; d.ArraySize=1; d.SampleCount=1; d.Usage=Diligent::USAGE_IMMUTABLE; d.BindFlags=Diligent::BIND_SHADER_RESOURCE;
        Diligent::TextureSubResData sub{};sub.pData=data;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(d,&init,&input);if(!input){applyCPU(src,dst);return;}
        auto od=d;od.Name="RadialBlur/Output";od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(od,nullptr,&output);if(!output){applyCPU(src,dst);return;}
        struct Params{float amount,cx,cy,samples;int type;float pad[3];};Diligent::BufferDesc bd{};bd.Name="RadialBlur/Params";bd.Size=sizeof(Params);bd.Usage=Diligent::USAGE_DYNAMIC;bd.BindFlags=Diligent::BIND_UNIFORM_BUFFER;bd.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(bd,nullptr,&params);if(!params){applyCPU(src,dst);return;}void*m=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,m);if(!m){applyCPU(src,dst);return;}Params p{cpuImpl_.amount_,cpuImpl_.centerX_,cpuImpl_.centerY_,static_cast<float>(cpuImpl_.samples_),cpuImpl_.type_,{}};std::memcpy(m,&p,sizeof(p));context->UnmapBuffer(params,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"RadialBlurParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gc{device,context};ArtifactCore::ComputeExecutor ex{gc};ArtifactCore::ComputePipelineDesc pd{};pd.name="RadialBlur/PSO";pd.shaderSource=kHlsl;pd.entryPoint="main";pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pd.variables=vars;pd.variableCount=3;pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!ex.build(pd)||!ex.createShaderResourceBinding(true)||!ex.setBuffer("RadialBlurParams",params)||!ex.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!ex.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}ex.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        auto sd=od;sd.Name="RadialBlur/Readback";sd.Usage=Diligent::USAGE_STAGING;sd.BindFlags=Diligent::BIND_NONE;sd.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(sd,nullptr,&staging);if(!staging){applyCPU(src,dst);return;}context->CopyTexture(Diligent::CopyTextureAttribs(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){applyCPU(src,dst);return;}cv::Mat result(image.height(),image.width(),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result);context->UnmapTextureSubresource(staging,0,0);
    }
private:
    static constexpr const char* kHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer RadialBlurParams:register(b0){float g_Amount;float g_Cx;float g_Cy;float g_Samples;int g_Type;float3 g_Pad;}
float4 sample(float2 p,uint w,uint h){p=clamp(p,float2(0,0),float2(w-1,h-1));int2 a=int2(floor(p)),b=min(a+1,int2(w-1,h-1));float2 t=p-a;return lerp(lerp(g_InputTexture[a],g_InputTexture[int2(b.x,a.y)],t.x),lerp(g_InputTexture[int2(a.x,b.y)],g_InputTexture[b],t.x),t.y);}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float2 q=float2(id.xy),c=float2(g_Cx*w,g_Cy*h);float4 sum=0;int n=max(1,(int)g_Samples);for(int s=0;s<n;++s){float t=n>1?(float)s/(n-1):0;float dx=0,dy=0;if(g_Type==0){float a=t*g_Amount*3.14159265/180;dx=cos(a)*t*g_Amount;dy=sin(a)*t*g_Amount;}else{dy=t*g_Amount;}float factor=1-abs(q.y-c.y)/(h*0.5+0.0001);sum+=sample(float2(q.x-dx*factor,q.y-dy),w,h);}g_OutputTexture[id.xy]=sum/n;}
)";
    RadialBlurEffectCPUImpl cpuImpl_;
};

RadialBlurEffect::RadialBlurEffect() {
    setDisplayName(UniString("Radial Blur"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<RadialBlurEffectCPUImpl>());
    setGPUImpl(std::make_shared<RadialBlurEffectGPUImpl>());
}
RadialBlurEffect::~RadialBlurEffect() = default;

float RadialBlurEffect::amount() const { return amount_; }
void RadialBlurEffect::setAmount(float v) { amount_ = std::clamp(v, 0.0f, 100.0f); syncImpls(); }
float RadialBlurEffect::centerX() const { return centerX_; }
void RadialBlurEffect::setCenterX(float v) { centerX_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float RadialBlurEffect::centerY() const { return centerY_; }
void RadialBlurEffect::setCenterY(float v) { centerY_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
int RadialBlurEffect::samples() const { return samples_; }
void RadialBlurEffect::setSamples(int v) { samples_ = std::clamp(v, 1, 64); syncImpls(); }
int RadialBlurEffect::type() const { return type_; }
void RadialBlurEffect::setType(int v) { type_ = v; syncImpls(); }

void RadialBlurEffect::syncImpls() {
    if (auto* c = dynamic_cast<RadialBlurEffectCPUImpl*>(cpuImpl().get())) {
        c->amount_ = amount_;
        c->centerX_ = centerX_;
        c->centerY_ = centerY_;
        c->samples_ = samples_;
        c->type_ = type_;
    }
    if (auto* g = dynamic_cast<RadialBlurEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.amount_ = amount_;
        g->cpuImpl_.centerX_ = centerX_;
        g->cpuImpl_.centerY_ = centerY_;
        g->cpuImpl_.samples_ = samples_;
        g->cpuImpl_.type_ = type_;
    }
}

std::vector<AbstractProperty> RadialBlurEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Amount"); a.setType(PropertyType::Float); a.setValue(amount_);
    auto& cx = props.emplace_back(); cx.setName("Center X"); cx.setType(PropertyType::Float); cx.setValue(centerX_);
    auto& cy = props.emplace_back(); cy.setName("Center Y"); cy.setType(PropertyType::Float); cy.setValue(centerY_);
    auto& s = props.emplace_back(); s.setName("Samples"); s.setType(PropertyType::Integer); s.setValue(samples_);
    auto& t = props.emplace_back(); t.setName("Type"); t.setType(PropertyType::Integer); t.setValue(type_);
    return props;
}

void RadialBlurEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Amount") setAmount(v.toFloat());
    else if (k == "Center X") setCenterX(v.toFloat());
    else if (k == "Center Y") setCenterY(v.toFloat());
    else if (k == "Samples") setSamples(v.toInt());
    else if (k == "Type") setType(v.toInt());
}

} // namespace Artifact
