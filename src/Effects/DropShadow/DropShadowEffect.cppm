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

module Artifact.Effect.Rasterizer.DropShadow;

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

class DropShadowCPUImpl : public ArtifactEffectImplBase {
public:
    QColor shadowColor_ = QColor(0, 0, 0, 180);
    float  distance_    = 5.0f;
    float  angle_       = 135.0f;
    float  softness_    = 8.0f;
    float  opacity_     = 75.0f;   // 0-100 (%)

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

        // angle を rad に変換し、オフセット計算（AE 準拠: 135° = 右下）
        const float rad   = angle_ * (3.14159265358979f / 180.0f);
        const int   offX  = static_cast<int>(std::round( distance_ * std::cos(rad)));
        const int   offY  = static_cast<int>(std::round(-distance_ * std::sin(rad)));

        // ── 1. アルファチャンネル抽出 ──────────────────────────────────────
        cv::Mat srcAlpha(H, W, CV_32FC1);
        {
            ArtifactCore::Parallel::For(0, H, [&](int y) {
                const float* p = srcData + static_cast<size_t>(y) * W * 4;
                float* row = srcAlpha.ptr<float>(y);
                for (int x = 0; x < W; ++x, p += 4) {
                    row[x] = p[3];  // alpha channel
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

        // ── 4. 影色 RGBA マット生成 ─────────────────────────────────────────
        const float sr = shadowColor_.redF();
        const float sg = shadowColor_.greenF();
        const float sb = shadowColor_.blueF();
        const float so = shadowColor_.alphaF();  // 色自体の alpha
        const float opac = std::clamp(opacity_ / 100.0f, 0.0f, 1.0f);

        cv::Mat shadowLayer(H, W, CV_32FC4);
        ArtifactCore::Parallel::For(0, H, [&](int y) {
            const float* aRow = shifted.ptr<float>(y);
            cv::Vec4f*   sRow = shadowLayer.ptr<cv::Vec4f>(y);
            for (int x = 0; x < W; ++x) {
                const float a = std::clamp(aRow[x] * so * opac, 0.0f, 1.0f);
                // OpenCV 内部順: BGR-A
                sRow[x] = cv::Vec4f(sb, sg, sr, a);
            }
        });

        // ── 5. 合成: shadow → src over ────────────────────────────────────
        // dst に元画像をコピーし、影を背面に合成
        cv::Mat srcMat(H, W, CV_32FC4,
                       const_cast<float*>(srcData));

        // (shadow) OVER (src) = shadow をまず描き、src を上から合成
        dst = src.DeepCopy();
        float* dstData = dst.image().rgba32fData();
        cv::Mat dstMat(H, W, CV_32FC4, dstData);

        ArtifactCore::Parallel::For(0, H, [&](int y) {
            const cv::Vec4f* sh  = shadowLayer.ptr<cv::Vec4f>(y);
            const cv::Vec4f* fg  = srcMat.ptr<cv::Vec4f>(y);
            cv::Vec4f*       out = dstMat.ptr<cv::Vec4f>(y);
            for (int x = 0; x < W; ++x) {
                // Porter-Duff "src over shadow"
                // 前景 (src) が上、影が下
                const float fa = fg[x][3];
                const float sa = sh[x][3];
                const float oa = fa + sa * (1.0f - fa);
                if (oa < 1e-6f) {
                    out[x] = cv::Vec4f(0.f, 0.f, 0.f, 0.f);
                    continue;
                }
                for (int c = 0; c < 3; ++c) {
                    out[x][c] = (fg[x][c] * fa + sh[x][c] * sa * (1.0f - fa)) / oa;
                }
                out[x][3] = oa;
            }
        });
    }
};

static constexpr const char* kDropShadowHlsl=R"(
Texture2D<float4> g_InputTexture:register(t0); RWTexture2D<float4> g_OutputTexture:register(u0);
cbuffer DropShadowParams:register(b0){float g_OffX;float g_OffY;float g_Softness;float g_Opacity;float4 g_Color;}
float alphaAt(int2 p,uint w,uint h){return g_InputTexture[uint2(clamp(p.x,0,(int)w-1),clamp(p.y,0,(int)h-1))].a;}
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID){uint w,h;g_OutputTexture.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;int2 q=int2(id.xy);float radius=clamp(g_Softness,0,32),sum=0,weight=0;[unroll]for(int oy=-4;oy<=4;++oy){[unroll]for(int ox=-4;ox<=4;++ox){float2 d=float2(ox,oy),ww=exp(-dot(d,d)/max(2*radius*radius,1));sum+=alphaAt(q-int2(round(float2(g_OffX,g_OffY)))+int2(ox,oy),w,h)*ww;weight+=ww;}}float sa=(sum/max(weight,0.0001))*g_Opacity* g_Color.a;float4 fg=g_InputTexture[q],sh=float4(g_Color.rgb,clamp(sa,0,1));float oa=fg.a+sh.a*(1-fg.a);float4 outp=oa>0?float4((fg.rgb*fg.a+sh.rgb*sh.a*(1-fg.a))/oa,oa):0;g_OutputTexture[q]=outp;}
)";

// ─── GPU Impl ───────────────────────────────────────────────────────────────

class DropShadowGPUImpl : public ArtifactEffectImplBase {
public:
    DropShadowCPUImpl cpuImpl_;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache&       dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache&       dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
        Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if (!acquireSharedRenderDeviceForCurrentBackend(device, context)) { cpuImpl_.applyCPU(src, dst); return; }
        const auto& image=src.image(); const float* data=image.rgba32fData();
        if(!data||image.width()<=0||image.height()<=0){cpuImpl_.applyCPU(src,dst);return;}
        Diligent::TextureDesc td{};td.Name="DropShadow/Input";td.Type=Diligent::RESOURCE_DIM_TEX_2D;td.Width=image.width();td.Height=image.height();td.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT;td.MipLevels=1;td.ArraySize=1;td.SampleCount=1;td.Usage=Diligent::USAGE_IMMUTABLE;td.BindFlags=Diligent::BIND_SHADER_RESOURCE;Diligent::TextureSubResData sub{};sub.pData=data;sub.Stride=static_cast<Diligent::Uint64>(image.width())*sizeof(float)*4ull;Diligent::TextureData init{};init.pSubResources=&sub;init.NumSubresources=1;Diligent::RefCntAutoPtr<Diligent::ITexture> input;device->CreateTexture(td,&init,&input);if(!input){cpuImpl_.applyCPU(src,dst);return;}
        auto od=td;od.Name="DropShadow/Output";od.Usage=Diligent::USAGE_DEFAULT;od.BindFlags=Diligent::BIND_SHADER_RESOURCE|Diligent::BIND_UNORDERED_ACCESS;Diligent::RefCntAutoPtr<Diligent::ITexture> output;device->CreateTexture(od,nullptr,&output);if(!output){cpuImpl_.applyCPU(src,dst);return;}
        const float rad=angle_*(3.14159265358979f/180.0f);struct Params{float ox,oy,soft,opacity;float color[4];};Diligent::BufferDesc bd{};bd.Name="DropShadow/Params";bd.Size=sizeof(Params);bd.Usage=Diligent::USAGE_DYNAMIC;bd.BindFlags=Diligent::BIND_UNIFORM_BUFFER;bd.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE;Diligent::RefCntAutoPtr<Diligent::IBuffer> params;device->CreateBuffer(bd,nullptr,&params);if(!params){cpuImpl_.applyCPU(src,dst);return;}void*m=nullptr;context->MapBuffer(params,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,m);if(!m){cpuImpl_.applyCPU(src,dst);return;}Params p{distance_*std::cos(rad),-distance_*std::sin(rad),softness_,std::clamp(opacity_/100.0f,0.0f,1.0f),{shadowColor_.redF(),shadowColor_.greenF(),shadowColor_.blueF(),shadowColor_.alphaF()}};std::memcpy(m,&p,sizeof(p));context->UnmapBuffer(params,Diligent::MAP_WRITE);
        static Diligent::ShaderResourceVariableDesc vars[]={{Diligent::SHADER_TYPE_COMPUTE,"DropShadowParams",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_InputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{Diligent::SHADER_TYPE_COMPUTE,"g_OutputTexture",Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};ArtifactCore::GpuContext gc{device,context};ArtifactCore::ComputeExecutor ex{gc};ArtifactCore::ComputePipelineDesc pd{};pd.name="DropShadow/PSO";pd.shaderSource=kDropShadowHlsl;pd.entryPoint="main";pd.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;pd.variables=vars;pd.variableCount=3;pd.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;if(!ex.build(pd)||!ex.createShaderResourceBinding(true)||!ex.setBuffer("DropShadowParams",params)||!ex.setTextureView("g_InputTexture",input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE))||!ex.setTextureView("g_OutputTexture",output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))){cpuImpl_.applyCPU(src,dst);return;}ex.dispatch(context,ArtifactCore::ComputeExecutor::makeDispatchAttribs(od.Width,od.Height,1,8,8,1),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        auto sd=od;sd.Name="DropShadow/Readback";sd.Usage=Diligent::USAGE_STAGING;sd.BindFlags=Diligent::BIND_NONE;sd.CPUAccessFlags=Diligent::CPU_ACCESS_READ;Diligent::RefCntAutoPtr<Diligent::ITexture> staging;device->CreateTexture(sd,nullptr,&staging);if(!staging){cpuImpl_.applyCPU(src,dst);return;}context->CopyTexture(Diligent::CopyTextureAttribs(output,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION));context->Flush();context->WaitForIdle();Diligent::MappedTextureSubresource read{};context->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,read);if(!read.pData||!read.Stride){cpuImpl_.applyCPU(src,dst);return;}cv::Mat result(image.height(),image.width(),CV_32FC4,read.pData,read.Stride);dst.image().setFromCVMat(result);context->UnmapTextureSubresource(staging,0,0);
    }
};

// ─── DropShadowEffect ─────────────────────────────────────────────────────────

DropShadowEffect::DropShadowEffect()
{
    setDisplayName(UniString("Drop Shadow (Rasterizer)"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    auto cpu = std::make_shared<DropShadowCPUImpl>();
    auto gpu = std::make_shared<DropShadowGPUImpl>();
    setCPUImpl(cpu);
    setGPUImpl(gpu);
}

DropShadowEffect::~DropShadowEffect() = default;

// ── アクセサ ─────────────────────────────────────────────────────────────────

QColor DropShadowEffect::shadowColor() const { return shadowColor_; }
void   DropShadowEffect::setShadowColor(const QColor& c) {
    shadowColor_ = c;
    syncImpls();
}

float DropShadowEffect::distance() const { return distance_; }
void  DropShadowEffect::setDistance(float d) {
    distance_ = std::max(0.0f, d);
    syncImpls();
}

float DropShadowEffect::angle() const { return angle_; }
void  DropShadowEffect::setAngle(float a) {
    // 正規化は不要 (任意 degree)
    angle_ = a;
    syncImpls();
}

float DropShadowEffect::softness() const { return softness_; }
void  DropShadowEffect::setSoftness(float s) {
    softness_ = std::max(0.0f, s);
    syncImpls();
}

float DropShadowEffect::opacity() const { return opacity_; }
void  DropShadowEffect::setOpacity(float o) {
    opacity_ = std::clamp(o, 0.0f, 100.0f);
    syncImpls();
}

// ── Properties API ────────────────────────────────────────────────────────────

std::vector<AbstractProperty> DropShadowEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(5);

    auto& colorProp = props.emplace_back();
    colorProp.setName("Shadow Color");
    colorProp.setType(PropertyType::Color);
    colorProp.setValue(shadowColor_);

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

    return props;
}

void DropShadowEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString k = name.toQString();
    if      (k == "Shadow Color") setShadowColor(value.value<QColor>());
    else if (k == "Distance")     setDistance(value.toFloat());
    else if (k == "Angle")        setAngle(value.toFloat());
    else if (k == "Softness")     setSoftness(value.toFloat());
    else if (k == "Opacity")      setOpacity(value.toFloat());
}

// ── Private ───────────────────────────────────────────────────────────────────

void DropShadowEffect::syncImpls() {
    if (auto* c = dynamic_cast<DropShadowCPUImpl*>(cpuImpl().get())) {
        c->shadowColor_ = shadowColor_;
        c->distance_    = distance_;
        c->angle_       = angle_;
        c->softness_    = softness_;
        c->opacity_     = opacity_;
    }
    if (auto* g = dynamic_cast<DropShadowGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.shadowColor_ = shadowColor_;
        g->cpuImpl_.distance_    = distance_;
        g->cpuImpl_.angle_       = angle_;
        g->cpuImpl_.softness_    = softness_;
        g->cpuImpl_.opacity_     = opacity_;
    }
}

} // namespace Artifact
