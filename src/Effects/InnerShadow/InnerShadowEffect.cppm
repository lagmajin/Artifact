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

module Artifact.Effect.Rasterizer.InnerShadow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Memory.SharedPtr;

namespace Artifact {

using namespace ArtifactCore;

// ─── CPU Impl ────────────────────────────────────────────────────────────────

class InnerShadowCPUImpl : public ArtifactEffectImplBase {
public:
    QColor shadowColor_ = QColor(0, 0, 0, 180);
    float  distance_    = 5.0f;
    float  angle_       = 120.0f;
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

        // Angle is the light source azimuth (0 = 3 o'clock, clockwise, math
        // up); the shadow is cast opposite the light. See DropShadowEffect.
        const float rad   = angle_ * (3.14159265358979f / 180.0f);
        const int   offX  = static_cast<int>(std::round(-distance_ * std::cos(rad)));
        const int   offY  = static_cast<int>(std::round( distance_ * std::sin(rad)));

        // ── 1. アルファチャンネル抽出 ──────────────────────────────────────
        cv::Mat srcAlpha(H, W, CV_32FC1);
        {
    ArtifactCore::Parallel::ForPixels(0, H, W, H, [&](int y) {
                const float* p = srcData + static_cast<size_t>(y) * W * 4;
                float* row = srcAlpha.ptr<float>(y);
                for (int x = 0; x < W; ++x, p += 4) {
                    row[x] = p[3];  // alpha channel
                }
            });
        }

        // ── 2. オフセット適用 (shadow offset) ──────────────────────────────
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
        const float so = shadowColor_.alphaF();
        const float opac = std::clamp(opacity_ / 100.0f, 0.0f, 1.0f);

        cv::Mat shadowLayer(H, W, CV_32FC4);
    ArtifactCore::Parallel::ForPixels(0, H, W, H, [&](int y) {
            const float* aRow = shifted.ptr<float>(y);
            cv::Vec4f*   sRow = shadowLayer.ptr<cv::Vec4f>(y);
            for (int x = 0; x < W; ++x) {
                const float a = std::clamp(aRow[x] * so * opac, 0.0f, 1.0f);
                // ImageF32x4_RGBA is stored as RGBA, including its cv::Mat view.
                sRow[x] = cv::Vec4f(sr, sg, sb, a);
            }
        });
        
        // ── 5. 合成: Inner Shadow ──────────────────────────────────────────
        // dst = src をコピーし、影を src アルファでマスクした内側に合成
        dst = src.DeepCopy();
        float* dstData = dst.image().rgba32fData();
        cv::Mat dstMat(H, W, CV_32FC4, dstData);
        cv::Mat srcMat(H, W, CV_32FC4,
                       const_cast<float*>(srcData));

        // Inner shadow は src の内部（src のアルファがある領域の内側）にのみ表示
        // shadow_color * shadow_alpha * (1 - src_alpha) を src に加算合成
    ArtifactCore::Parallel::ForPixels(0, H, W, H, [&](int y) {
            const cv::Vec4f* sh  = shadowLayer.ptr<cv::Vec4f>(y);
            const cv::Vec4f* fg  = srcMat.ptr<cv::Vec4f>(y);
            cv::Vec4f*       out = dstMat.ptr<cv::Vec4f>(y);
            for (int x = 0; x < W; ++x) {
                const float fa = fg[x][3];
                const float sa = sh[x][3];
                const float shadowFactor = std::clamp(sa * fa, 0.0f, 1.0f);
                const float oa = fa;
                if (oa < 1e-6f) {
                    out[x] = cv::Vec4f(0.f, 0.f, 0.f, 0.f);
                    continue;
                }
                for (int c = 0; c < 3; ++c) {
                    out[x][c] = (fg[x][c] * fa * (1.0f - shadowFactor) +
                                 sh[x][c] * shadowFactor) / oa;
                }
                out[x][3] = oa;
            }
        });
    }
};

// ─── Resident generic shader ─────────────────────────────────────────────────
//
// Mirrors InnerShadowCPUImpl above. Parameter slots match DropShadow's
// (P0 offX, P1 offY, P2 softness, P3 opacity folded with the color alpha,
// P4..P6 tint), and the offsets/softness are resolution-scaled.
//
// The composite differs from DropShadow: the shadow is masked by the source
// alpha and replaces the foreground inside the silhouette rather than sitting
// behind it, and the output alpha stays exactly the source alpha.
static constexpr const char* kInnerShadowResidentHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

float innerShadowAlphaAt(int2 p, uint width, uint height)
{
    if (p.x < 0 || p.y < 0 || p.x >= (int)width || p.y >= (int)height) return 0.0f;
    return g_InputTexture[uint2(p)].a;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dispatchId.x >= width || dispatchId.y >= height) return;

    const float2 offset = float2(g_P0, g_P1);
    const float sigma = max(g_P2, 0.0);
    const int sampleRadius = min(32, (int)ceil(sigma * 2.5));

    const int2 center = int2(dispatchId.xy) - int2(round(offset));

    float sum = 0.0f;
    float weightSum = 0.0f;
    for (int oy = -32; oy <= 32; ++oy) {
        if (oy < -sampleRadius || oy > sampleRadius) continue;
        for (int ox = -32; ox <= 32; ++ox) {
            if (ox < -sampleRadius || ox > sampleRadius) continue;
            const float2 d = float2(ox, oy);
            const float w = exp(-dot(d, d) / max(2.0 * sigma * sigma, 1.0));
            sum += innerShadowAlphaAt(center + int2(ox, oy), width, height) * w;
            weightSum += w;
        }
    }

    const float4 foreground = g_InputTexture[dispatchId.xy];
    const float shadowAlpha = clamp((sum / max(weightSum, 1.0e-4)) * g_P3, 0.0f, 1.0f);
    const float4 shadow = float4(g_P4, g_P5, g_P6, shadowAlpha);
    // The shadow only shows where the silhouette is opaque, and it replaces
    // the foreground there instead of compositing behind it.
    const float factor = clamp(shadow.a * foreground.a, 0.0f, 1.0f);

    float4 result = float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (foreground.a >= 1.0e-6) {
        result = float4(
            (foreground.rgb * foreground.a * (1.0f - factor) + shadow.rgb * factor)
                / foreground.a,
            foreground.a);
    }
    g_OutputTexture[dispatchId.xy] = result;
}
)";

// ─── GPU Impl (CPU fallback) ──────────────────────────────────────────────────

class InnerShadowGPUImpl : public ArtifactEffectImplBase {
public:
    InnerShadowCPUImpl cpuImpl_;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache&       dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache&       dst) override {
        // The resident shader (kInnerShadowResidentHlsl) covers the common
        // case; this per-effect path still runs when the layer cannot use a
        // GPU plan (mix, mask, region, or a shadow wider than the shader's
        // 32-pixel gather box), so it must stay a correct CPU fallback.
        cpuImpl_.applyCPU(src, dst);
    }
};

// ─── InnerShadowEffect ────────────────────────────────────────────────────────

InnerShadowEffect::InnerShadowEffect()
{
    setDisplayName(UniString("Inner Shadow (Rasterizer)"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    auto cpu = ArtifactCore::makeShared<InnerShadowCPUImpl>();
    auto gpu = ArtifactCore::makeShared<InnerShadowGPUImpl>();
    setCPUImpl(cpu);
    setGPUImpl(gpu);
    registerGpuGenericShader(
        InnerShadowEffect::kGpuGenericKey,
        GpuGenericShaderRecord{
            kInnerShadowResidentHlsl, "main", GpuGenericResourceKind::Filter});
}

InnerShadowEffect::~InnerShadowEffect() = default;

// ── アクセサ ─────────────────────────────────────────────────────────────────

QColor InnerShadowEffect::shadowColor() const { return shadowColor_; }
void   InnerShadowEffect::setShadowColor(const QColor& c) {
    shadowColor_ = c;
    syncImpls();
}

float InnerShadowEffect::distance() const { return distance_; }
void  InnerShadowEffect::setDistance(float d) {
    distance_ = std::isfinite(d) ? std::max(0.0f, d) : 5.0f;
    syncImpls();
}

float InnerShadowEffect::angle() const { return angle_; }
void  InnerShadowEffect::setAngle(float a) {
    angle_ = std::isfinite(a) ? a : 120.0f;
    syncImpls();
}

float InnerShadowEffect::softness() const { return softness_; }
void  InnerShadowEffect::setSoftness(float s) {
    softness_ = std::isfinite(s) ? std::max(0.0f, s) : 8.0f;
    syncImpls();
}

float InnerShadowEffect::opacity() const { return opacity_; }
void  InnerShadowEffect::setOpacity(float o) {
    opacity_ = std::isfinite(o) ? std::clamp(o, 0.0f, 100.0f) : 75.0f;
    syncImpls();
}

// ── Properties API ────────────────────────────────────────────────────────────

std::vector<AbstractProperty> InnerShadowEffect::getProperties() const {
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
    angleProp.setDefaultValue(120.0);
    angleProp.setHardRange(0.0, 360.0);
    angleProp.setSoftRange(0.0, 360.0);
    angleProp.setStep(0.1);
    angleProp.setUnit(QStringLiteral("deg"));

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

void InnerShadowEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString k = name.toQString();
    if      (k == "Shadow Color") setShadowColor(value.value<QColor>());
    else if (k == "Distance")     setDistance(value.toFloat());
    else if (k == "Angle")        setAngle(value.toFloat());
    else if (k == "Softness")     setSoftness(value.toFloat());
    else if (k == "Opacity")      setOpacity(value.toFloat());
}

// ── Private ───────────────────────────────────────────────────────────────────

void InnerShadowEffect::syncImpls() {
    if (auto* c = dynamic_cast<InnerShadowCPUImpl*>(cpuImpl().get())) {
        c->shadowColor_ = shadowColor_;
        c->distance_    = distance_;
        c->angle_       = angle_;
        c->softness_    = softness_;
        c->opacity_     = opacity_;
    }
    if (auto* g = dynamic_cast<InnerShadowGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.shadowColor_ = shadowColor_;
        g->cpuImpl_.distance_    = distance_;
        g->cpuImpl_.angle_       = angle_;
        g->cpuImpl_.softness_    = softness_;
        g->cpuImpl_.opacity_     = opacity_;
    }
}

} // namespace Artifact
