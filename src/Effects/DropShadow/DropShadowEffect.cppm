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

module Artifact.Effect.Rasterizer.DropShadow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

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
            const float* p = srcData;
            for (int y = 0; y < H; ++y) {
                float* row = srcAlpha.ptr<float>(y);
                for (int x = 0; x < W; ++x, p += 4) {
                    row[x] = p[3];  // alpha channel
                }
            }
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
        for (int y = 0; y < H; ++y) {
            const float* aRow = shifted.ptr<float>(y);
            cv::Vec4f*   sRow = shadowLayer.ptr<cv::Vec4f>(y);
            for (int x = 0; x < W; ++x) {
                const float a = std::clamp(aRow[x] * so * opac, 0.0f, 1.0f);
                // OpenCV 内部順: BGR-A
                sRow[x] = cv::Vec4f(sb, sg, sr, a);
            }
        }

        // ── 5. 合成: shadow → src over ────────────────────────────────────
        // dst に元画像をコピーし、影を背面に合成
        cv::Mat srcMat(H, W, CV_32FC4,
                       const_cast<float*>(srcData));

        // (shadow) OVER (src) = shadow をまず描き、src を上から合成
        dst = src.DeepCopy();
        float* dstData = dst.image().rgba32fData();
        cv::Mat dstMat(H, W, CV_32FC4, dstData);

        for (int y = 0; y < H; ++y) {
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
        }
    }
};

// ─── GPU Impl (CPU fallback) ──────────────────────────────────────────────────

class DropShadowGPUImpl : public ArtifactEffectImplBase {
public:
    DropShadowCPUImpl cpuImpl_;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache&       dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache&       dst) override {
        // GPU 実装は将来のフェーズで追加予定。現状は CPU フォールバック。
        cpuImpl_.applyCPU(src, dst);
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
