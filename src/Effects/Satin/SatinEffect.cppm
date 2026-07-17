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

module Artifact.Effect.Rasterizer.Satin;

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

class SatinCPUImpl : public ArtifactEffectImplBase {
public:
    QColor satinColor_ = QColor(200, 200, 200, 180);
    float  distance_   = 0.0f;
    float  angle_      = 0.0f;
    float  softness_   = 5.0f;
    float  opacity_    = 50.0f;  // 0-100 (%)
    bool   invert_     = false;

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

        // angle を rad に変換し、オフセット計算
        const float rad   = angle_ * (3.14159265358979f / 180.0f);
        const int   offX  = static_cast<int>(std::round( distance_ * std::cos(rad)));
        const int   offY  = static_cast<int>(std::round(-distance_ * std::sin(rad)));

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

        // ── 2. オフセット適用 ──────────────────────────────────────────────
        cv::Mat shifted = cv::Mat::zeros(H, W, CV_32FC1);
        {
            const int srcX0 = std::max(0, -offX);
            const int srcY0 = std::max(0, -offY);
            const int dstX0 = std::max(0,  offX);
            const int dstY0 = std::max(0,  offY);
            const int cpyW  = std::min(W - srcX0, W - dstX0);
            const int cpyH  = std::min(H - srcY0, H - dstY0);
            if (cpyW > 0 && cpyH > 0) {
                srcAlpha(cv::Rect(srcX0, srcY0, cpyW, cpyH))
                    .copyTo(shifted(cv::Rect(dstX0, dstY0, cpyW, cpyH)));
            }
        }

        // ── 3. ガウスぼかし ────────────────────────────────────────────────
        if (softness_ > 0.0f) {
            const int ksize = static_cast<int>(std::ceil(softness_ * 2.5f)) * 2 + 1;
            cv::GaussianBlur(shifted, shifted,
                             cv::Size(ksize, ksize),
                             softness_, softness_,
                             cv::BORDER_REPLICATE);
        }

        // ── 4. Invert（オプション） ────────────────────────────────────────
        cv::Mat satinAlpha = invert_ ? cv::Mat::ones(H, W, CV_32FC1) - shifted
                                     : shifted;

        // ── 5. サテン色 RGBA マット生成 ─────────────────────────────────────
        const float sr = satinColor_.redF();
        const float sg = satinColor_.greenF();
        const float sb = satinColor_.blueF();
        const float so = satinColor_.alphaF();
        const float opac = std::clamp(opacity_ / 100.0f, 0.0f, 1.0f);

        cv::Mat satinLayer(H, W, CV_32FC4);
        ArtifactCore::Parallel::For(0, H, [&](int y) {
            const float* aRow = satinAlpha.ptr<float>(y);
            cv::Vec4f*   lRow = satinLayer.ptr<cv::Vec4f>(y);
            for (int x = 0; x < W; ++x) {
                const float a = std::clamp(aRow[x] * so * opac, 0.0f, 1.0f);
                // OpenCV internal order: B, G, R, A
                lRow[x] = cv::Vec4f(sb, sg, sr, a);
            }
        });

        // ── 6. 合成: Satin OVER src (ブレンドモードは Multiply 風が一般的) ──
        dst = src.DeepCopy();
        float* dstData = dst.image().rgba32fData();
        cv::Mat dstMat(H, W, CV_32FC4, dstData);
        cv::Mat srcMat(H, W, CV_32FC4,
                       const_cast<float*>(srcData));

        ArtifactCore::Parallel::For(0, H, [&](int y) {
            const cv::Vec4f* sa  = satinLayer.ptr<cv::Vec4f>(y);
            const cv::Vec4f* fg  = srcMat.ptr<cv::Vec4f>(y);
            cv::Vec4f*       out = dstMat.ptr<cv::Vec4f>(y);
            for (int x = 0; x < W; ++x) {
                const float fa = fg[x][3];
                const float saA = sa[x][3];
                // Satin: src の内側にサテン色を乗算ブレンドで合成
                // satinAlpha で src 内側をマスク
                const float blendFactor = saA * fa;
                const float oa = fa + blendFactor * (1.0f - fa);
                if (oa < 1e-6f) {
                    out[x] = cv::Vec4f(0.f, 0.f, 0.f, 0.f);
                    continue;
                }
                for (int c = 0; c < 3; ++c) {
                    // Multiply-like blend
                    const float f = fg[x][c];
                    const float s = sa[x][c];
                    out[x][c] = (f * fa + (f * s) * blendFactor * (1.0f - fa)) / oa;
                }
                out[x][3] = oa;
            }
        });
    }
};

// ─── GPU Impl (CPU fallback) ──────────────────────────────────────────────────

class SatinGPUImpl : public ArtifactEffectImplBase {
public:
    SatinCPUImpl cpuImpl_;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache&       dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache&       dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device; Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if (!acquireSharedRenderDeviceForCurrentBackend(device, context)) { applyCPU(src,dst); return; }
        const auto& image=src.image(); const float* data=image.rgba32fData(); if(!data||image.width()<=0||image.height()<=0){applyCPU(src,dst);return;}
        Diligent::TextureDesc d{};d.Name="Satin/Input";d.Type=Diligent::RESOURCE_DIM_TEX_2D;d.Width=image.width();d.Height=image.height();d.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;d.MipLevels=1;d.ArraySize=1;d.SampleCount=1;d.Usage=Diligent::USAGE_IMMUTABLE;d.BindFlags=Diligent::BIND_SHADER_RESOURCE;Diligent::TextureSubResData sub{};sub.pData=data;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(d,&init,&input);if(!input){applyCPU(src,dst);return;}
        auto od=d;od.Name="Satin/Output";od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(od,nullptr,&output);if(!output){applyCPU(src,dst);return;}
        struct Params{float distance,angle,softness,opacity;float invert,pad[3];float color[4];};Diligent::BufferDesc bd{};bd.Name="Satin/Params";bd.Size=sizeof(Params);bd.Usage=Diligent::USAGE_DYNAMIC;bd.BindFlags=Diligent::BIND_UNIFORM_BUFFER;bd.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(bd,nullptr,&params);if(!params){applyCPU(src,dst);return;}void*m=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,m);if(!m){applyCPU(src,dst);return;}Params p{cpuImpl_.distance_,cpuImpl_.angle_,cpuImpl_.softness_,cpuImpl_.opacity_/100.0f,cpuImpl_.invert_?1.0f,{0,0,0},{cpuImpl_.satinColor_.blueF(),cpuImpl_.satinColor_.greenF(),cpuImpl_.satinColor_.redF(),cpuImpl_.satinColor_.alphaF()}};std::memcpy(m,&p,sizeof(p));context->UnmapBuffer(params,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"SatinParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gc{device,context};ArtifactCore::ComputeExecutor ex{gc};ArtifactCore::ComputePipelineDesc pd{};pd.name="Satin/PSO";pd.shaderSource=kHlsl;pd.entryPoint="main";pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pd.variables=vars;pd.variableCount=3;pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!ex.build(pd)||!ex.createShaderResourceBinding(true)||!ex.setBuffer("SatinParams",params)||!ex.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!ex.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){applyCPU(src,dst);return;}ex.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        auto sd=od;sd.Name="Satin/Readback";sd.Usage=Diligent::USAGE_STAGING;sd.BindFlags=Diligent::BIND_NONE;sd.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(sd,nullptr,&staging);if(!staging){applyCPU(src,dst);return;}context->CopyTexture(Diligent::CopyTextureAttribs(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){applyCPU(src,dst);return;}cv::Mat result(image.height(),image.width(),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result);context->UnmapTextureSubresource(staging,0,0);
    }
private:
    static constexpr const char* kHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0);RWTexture2D<float4> g_OutputTexture:register(u0);cbuffer SatinParams:register(b0){float g_Distance;float g_Angle;float g_Softness;float g_Opacity;float g_Invert;float4 g_Color;}
float alphaAt(int2 p,uint w,uint h){return g_InputTexture[uint2(clamp(p.x,0,(int)w-1),clamp(p.y,0,(int)h-1))].a;}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;float4 base=g_InputTexture[id.xy];float2 dir=float2(cos(g_Angle*0.0174532925),-sin(g_Angle*0.0174532925));float2 off=dir*g_Distance;float a=0;int n=max(1,(int)(g_Softness*2+1));for(int i=-n;i<=n;++i){float t=(float)i/max(1,n);a+=alphaAt(int2(float2(id.xy)+off+dir*t*g_Softness),w,h);}a/=2*n+1;if(g_Invert>0.5)a=1-a;float sa=saturate(a*g_Color.a*g_Opacity);float oa=base.a+sa*(1-base.a);float3 rgb=oa>0?(base.rgb*base.a+(base.rgb*g_Color.rgb)*sa*(1-base.a))/oa:0;g_OutputTexture[id.xy]=float4(rgb,oa);}
)";
};

// ─── SatinEffect ─────────────────────────────────────────────────────────────

SatinEffect::SatinEffect()
{
    setDisplayName(UniString("Satin (Rasterizer)"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    auto cpu = std::make_shared<SatinCPUImpl>();
    auto gpu = std::make_shared<SatinGPUImpl>();
    setCPUImpl(cpu);
    setGPUImpl(gpu);
}

SatinEffect::~SatinEffect() = default;

// ── アクセサ ─────────────────────────────────────────────────────────────────

QColor SatinEffect::satinColor() const { return satinColor_; }
void   SatinEffect::setSatinColor(const QColor& c) {
    satinColor_ = c;
    syncImpls();
}

float SatinEffect::distance() const { return distance_; }
void  SatinEffect::setDistance(float d) {
    distance_ = std::max(0.0f, d);
    syncImpls();
}

float SatinEffect::angle() const { return angle_; }
void  SatinEffect::setAngle(float a) {
    angle_ = a;
    syncImpls();
}

float SatinEffect::softness() const { return softness_; }
void  SatinEffect::setSoftness(float s) {
    softness_ = std::max(0.0f, s);
    syncImpls();
}

float SatinEffect::opacity() const { return opacity_; }
void  SatinEffect::setOpacity(float o) {
    opacity_ = std::clamp(o, 0.0f, 100.0f);
    syncImpls();
}

bool SatinEffect::invert() const { return invert_; }
void SatinEffect::setInvert(bool v) {
    invert_ = v;
    syncImpls();
}

// ── Properties API ────────────────────────────────────────────────────────────

std::vector<AbstractProperty> SatinEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(6);

    auto& colorProp = props.emplace_back();
    colorProp.setName("Satin Color");
    colorProp.setType(PropertyType::Color);
    colorProp.setValue(satinColor_);

    auto& distProp = props.emplace_back();
    distProp.setName("Distance");
    distProp.setType(PropertyType::Float);
    distProp.setValue(distance_);

    auto& angleProp = props.emplace_back();
    angleProp.setName("Angle");
    angleProp.setType(PropertyType::Float);
    angleProp.setValue(angle_);

    auto& softProp = props.emplace_back();
    softProp.setName("Softness");
    softProp.setType(PropertyType::Float);
    softProp.setValue(softness_);

    auto& opacProp = props.emplace_back();
    opacProp.setName("Opacity");
    opacProp.setType(PropertyType::Float);
    opacProp.setValue(opacity_);

    auto& invertProp = props.emplace_back();
    invertProp.setName("Invert");
    invertProp.setType(PropertyType::Boolean);
    invertProp.setValue(invert_);

    return props;
}

void SatinEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString k = name.toQString();
    if      (k == "Satin Color") setSatinColor(value.value<QColor>());
    else if (k == "Distance")    setDistance(value.toFloat());
    else if (k == "Angle")       setAngle(value.toFloat());
    else if (k == "Softness")    setSoftness(value.toFloat());
    else if (k == "Opacity")     setOpacity(value.toFloat());
    else if (k == "Invert")      setInvert(value.toBool());
}

// ── Private ───────────────────────────────────────────────────────────────────

void SatinEffect::syncImpls() {
    if (auto* c = dynamic_cast<SatinCPUImpl*>(cpuImpl().get())) {
        c->satinColor_ = satinColor_;
        c->distance_   = distance_;
        c->angle_      = angle_;
        c->softness_   = softness_;
        c->opacity_    = opacity_;
        c->invert_     = invert_;
    }
    if (auto* g = dynamic_cast<SatinGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.satinColor_ = satinColor_;
        g->cpuImpl_.distance_   = distance_;
        g->cpuImpl_.angle_      = angle_;
        g->cpuImpl_.softness_   = softness_;
        g->cpuImpl_.opacity_    = opacity_;
        g->cpuImpl_.invert_     = invert_;
    }
}

} // namespace Artifact
