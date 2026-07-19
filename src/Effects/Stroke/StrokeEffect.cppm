module;
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include <QColor>
#include <QString>
#include <QVariant>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Rasterizer.Stroke;

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

// ─── CPU Impl ────────────────────────────────────────────────────────────────

class StrokeCPUImpl : public ArtifactEffectImplBase {
public:
    QColor strokeColor_ = QColor(255, 255, 255, 255);
    float  width_       = 3.0f;
    float  opacity_     = 100.0f;  // 0-100 (%)

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache&       dst) override
    {
        const ImageF32x4_RGBA& srcImg = src.image();
        const float* srcData = srcImg.rgba32fData();
        if (!srcData || srcImg.width() <= 0 || srcImg.height() <= 0) {
            dst = src;
            return;
        }

        const int W = srcImg.width();
        const int H = srcImg.height();

        // ── 1. アルファチャンネル抽出 ──────────────────────────────────────
        cv::Mat srcAlpha(H, W, CV_32FC1);
        {
            ArtifactCore::Parallel::For(0, H, [&](int y) {
                float* row = srcAlpha.ptr<float>(y);
                const float* p = srcData + static_cast<size_t>(y) * W * 4;
                for (int x = 0; x < W; ++x, p += 4) {
                    row[x] = p[3];
                }
            });
        }

        // ── 2. 膨張 (dilate) でストローク領域を生成 ────────────────────────
        cv::Mat dilated;
        if (width_ > 0.0f) {
            const int ksize = static_cast<int>(std::ceil(width_)) * 2 + 1;
            cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                       cv::Size(ksize, ksize));
            cv::dilate(srcAlpha, dilated, kernel);
        } else {
            dilated = srcAlpha.clone();
        }

        // ── 3. ストロークアルファ = dilated - srcAlpha ────────────────────
        cv::Mat strokeAlpha(H, W, CV_32FC1);
        ArtifactCore::Parallel::For(0, H, [&](int y) {
            const float* dRow = dilated.ptr<float>(y);
            const float* sRow = srcAlpha.ptr<float>(y);
            float*       aRow = strokeAlpha.ptr<float>(y);
            for (int x = 0; x < W; ++x) {
                aRow[x] = std::max(0.0f, dRow[x] - sRow[x]);
            }
        });

        // ── 4. ストローク色 RGBA マット生成 ─────────────────────────────────
        const float sr = strokeColor_.redF();
        const float sg = strokeColor_.greenF();
        const float sb = strokeColor_.blueF();
        const float so = strokeColor_.alphaF();
        const float opac = std::clamp(opacity_ / 100.0f, 0.0f, 1.0f);

        cv::Mat strokeLayer(H, W, CV_32FC4);
        ArtifactCore::Parallel::For(0, H, [&](int y) {
            const float* aRow = strokeAlpha.ptr<float>(y);
            cv::Vec4f*   lRow = strokeLayer.ptr<cv::Vec4f>(y);
            for (int x = 0; x < W; ++x) {
                const float a = std::clamp(aRow[x] * so * opac, 0.0f, 1.0f);
                // OpenCV internal order: B, G, R, A
                lRow[x] = cv::Vec4f(sb, sg, sr, a);
            }
        });

        // ── 5. 合成: Stroke OVER src ──────────────────────────────────────
        dst = src.DeepCopy();
        float* dstData = dst.image().rgba32fData();
        cv::Mat dstMat(H, W, CV_32FC4, dstData);
        cv::Mat srcMat(H, W, CV_32FC4,
                       const_cast<float*>(srcData));

        ArtifactCore::Parallel::For(0, H, [&](int y) {
            const cv::Vec4f* st  = strokeLayer.ptr<cv::Vec4f>(y);
            const cv::Vec4f* fg  = srcMat.ptr<cv::Vec4f>(y);
            cv::Vec4f*       out = dstMat.ptr<cv::Vec4f>(y);
            for (int x = 0; x < W; ++x) {
                const float fa = fg[x][3];
                const float sa = st[x][3];
                // Stroke は src の外側にレイヤーされるため OVER 合成
                const float oa = fa + sa * (1.0f - fa);
                if (oa < 1e-6f) {
                    out[x] = cv::Vec4f(0.f, 0.f, 0.f, 0.f);
                    continue;
                }
                for (int c = 0; c < 3; ++c) {
                    out[x][c] = (fg[x][c] * fa + st[x][c] * sa * (1.0f - fa)) / oa;
                }
                out[x][3] = oa;
            }
        });
    }
};

// ─── GPU Impl (CPU fallback) ──────────────────────────────────────────────────

class StrokeGPUImpl : public ArtifactEffectImplBase {
public:
    StrokeCPUImpl cpuImpl_;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache&       dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache&       dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device; Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if (!acquireSharedRenderDeviceForCurrentBackend(device, context)) { applyCPU(src,dst); return; }
        const auto& image=src.image(); const float* data=image.rgba32fData(); if(!data||image.width()<=0||image.height()<=0){applyCPU(src,dst);return;}
        Diligent::TextureDesc d{};d.Name="Stroke/Input";d.Type=Diligent::RESOURCE_DIM_TEX_2D;d.Width=image.width();d.Height=image.height();d.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;d.MipLevels=1;d.ArraySize=1;d.SampleCount=1;d.Usage=Diligent::USAGE_IMMUTABLE;d.BindFlags=Diligent::BIND_SHADER_RESOURCE;Diligent::TextureSubResData sub{};sub.pData=data;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(d,&init,&input);if(!input){applyCPU(src,dst);return;}
        auto od=d;od.Name="Stroke/Output";od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(od,nullptr,&output);if(!output){applyCPU(src,dst);return;}
        struct Params{float width,opacity;float color[4];};Diligent::BufferDesc bd{};bd.Name="Stroke/Params";bd.Size=sizeof(Params);bd.Usage=Diligent::USAGE_DYNAMIC;bd.BindFlags=Diligent::BIND_UNIFORM_BUFFER;bd.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(bd,nullptr,&params);if(!params){applyCPU(src,dst);return;}void*m=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,m);if(!m){applyCPU(src,dst);return;}Params p{cpuImpl_.width_,cpuImpl_.opacity_/100.0f,{cpuImpl_.strokeColor_.blueF(),cpuImpl_.strokeColor_.greenF(),cpuImpl_.strokeColor_.redF(),cpuImpl_.strokeColor_.alphaF()}};std::memcpy(m,&p,sizeof(p));context->UnmapBuffer(params,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"StrokeParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gc{device,context};ArtifactCore::ComputeExecutor ex{gc};ArtifactCore::ComputePipelineDesc pd{};pd.name="Stroke/PSO";pd.shaderSource=kHlsl;pd.entryPoint="main";pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pd.variables=vars;pd.variableCount=3;pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!ex.build(pd)||!ex.createShaderResourceBinding(true)||!ex.setBuffer("StrokeParams",params)||!ex.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!ex.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}ex.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        auto sd=od;sd.Name="Stroke/Readback";sd.Usage=Diligent::USAGE_STAGING;sd.BindFlags=Diligent::BIND_NONE;sd.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(sd,nullptr,&staging);if(!staging){applyCPU(src,dst);return;}context->CopyTexture(Diligent::CopyTextureAttribs(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){applyCPU(src,dst);return;}cv::Mat result(image.height(),image.width(),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result,image.colorDescriptor());context->UnmapTextureSubresource(staging,0,0);
    }
private:
    static constexpr const char* kHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer StrokeParams:register(b0){float g_Width;float g_Opacity;float4 g_Color;}
float alphaAt(int2 p,uint w,uint h){return g_InputTexture[uint2(clamp(p.x,0,(int)w-1),clamp(p.y,0,(int)h-1))].a;}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;int radius=min(32,max(0,(int)ceil(g_Width)));int2 p=int2(id.xy);float center=alphaAt(p,w,h),mx=0;for(int y=-32;y<=32;++y)for(int x=-32;x<=32;++x){if(abs(x)>radius||abs(y)>radius)continue;if(x*x+y*y>radius*radius)continue;mx=max(mx,alphaAt(p+int2(x,y),w,h));}float sa=saturate((mx-center)*g_Color.a*g_Opacity);float4 fg=g_InputTexture[id.xy];float oa=fg.a+sa*(1-fg.a);float3 rgb=oa>0?(fg.rgb*fg.a+g_Color.rgb*sa*(1-fg.a))/oa:0;g_OutputTexture[id.xy]=float4(rgb,oa);}
)";
};

// ─── StrokeEffect ────────────────────────────────────────────────────────────

StrokeEffect::StrokeEffect()
{
    setDisplayName(UniString("Stroke (Rasterizer)"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    auto cpu = std::make_shared<StrokeCPUImpl>();
    auto gpu = std::make_shared<StrokeGPUImpl>();
    setCPUImpl(cpu);
    setGPUImpl(gpu);
}

StrokeEffect::~StrokeEffect() = default;

// ── アクセサ ─────────────────────────────────────────────────────────────────

QColor StrokeEffect::strokeColor() const { return strokeColor_; }
void   StrokeEffect::setStrokeColor(const QColor& c) {
    strokeColor_ = c;
    syncImpls();
}

float StrokeEffect::width() const { return width_; }
void  StrokeEffect::setWidth(float w) {
    width_ = std::max(0.0f, w);
    syncImpls();
}

float StrokeEffect::opacity() const { return opacity_; }
void  StrokeEffect::setOpacity(float o) {
    opacity_ = std::clamp(o, 0.0f, 100.0f);
    syncImpls();
}

// ── Properties API ────────────────────────────────────────────────────────────

std::vector<AbstractProperty> StrokeEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(3);

    auto& colorProp = props.emplace_back();
    colorProp.setName("Stroke Color");
    colorProp.setType(PropertyType::Color);
    colorProp.setValue(strokeColor_);

    auto& widthProp = props.emplace_back();
    widthProp.setName("Width");
    widthProp.setType(PropertyType::Float);
    widthProp.setValue(width_);

    auto& opacProp = props.emplace_back();
    opacProp.setName("Opacity");
    opacProp.setType(PropertyType::Float);
    opacProp.setValue(opacity_);

    return props;
}

void StrokeEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString k = name.toQString();
    if      (k == "Stroke Color") setStrokeColor(value.value<QColor>());
    else if (k == "Width")        setWidth(value.toFloat());
    else if (k == "Opacity")      setOpacity(value.toFloat());
}

// ── Private ───────────────────────────────────────────────────────────────────

void StrokeEffect::syncImpls() {
    if (auto* c = dynamic_cast<StrokeCPUImpl*>(cpuImpl().get())) {
        c->strokeColor_ = strokeColor_;
        c->width_       = width_;
        c->opacity_     = opacity_;
    }
    if (auto* g = dynamic_cast<StrokeGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.strokeColor_ = strokeColor_;
        g->cpuImpl_.width_       = width_;
        g->cpuImpl_.opacity_     = opacity_;
    }
}

} // namespace Artifact
