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

module Artifact.Effect.Rasterizer.Satin;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

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
            const float* p = srcData;
            for (int y = 0; y < H; ++y) {
                float* row = srcAlpha.ptr<float>(y);
                for (int x = 0; x < W; ++x, p += 4) {
                    row[x] = p[3];
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
        for (int y = 0; y < H; ++y) {
            const float* aRow = satinAlpha.ptr<float>(y);
            cv::Vec4f*   lRow = satinLayer.ptr<cv::Vec4f>(y);
            for (int x = 0; x < W; ++x) {
                const float a = std::clamp(aRow[x] * so * opac, 0.0f, 1.0f);
                // OpenCV internal order: B, G, R, A
                lRow[x] = cv::Vec4f(sb, sg, sr, a);
            }
        }

        // ── 6. 合成: Satin OVER src (ブレンドモードは Multiply 風が一般的) ──
        dst = src.DeepCopy();
        float* dstData = dst.image().rgba32fData();
        cv::Mat dstMat(H, W, CV_32FC4, dstData);
        cv::Mat srcMat(H, W, CV_32FC4,
                       const_cast<float*>(srcData));

        for (int y = 0; y < H; ++y) {
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
        }
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
        // GPU 実装は将来のフェーズで追加予定。現状は CPU フォールバック。
        cpuImpl_.applyCPU(src, dst);
    }
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
