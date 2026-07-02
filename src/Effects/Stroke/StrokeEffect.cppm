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

module Artifact.Effect.Rasterizer.Stroke;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

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
            const float* p = srcData;
            for (int y = 0; y < H; ++y) {
                float* row = srcAlpha.ptr<float>(y);
                for (int x = 0; x < W; ++x, p += 4) {
                    row[x] = p[3];
                }
            }
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
        for (int y = 0; y < H; ++y) {
            const float* dRow = dilated.ptr<float>(y);
            const float* sRow = srcAlpha.ptr<float>(y);
            float*       aRow = strokeAlpha.ptr<float>(y);
            for (int x = 0; x < W; ++x) {
                aRow[x] = std::max(0.0f, dRow[x] - sRow[x]);
            }
        }

        // ── 4. ストローク色 RGBA マット生成 ─────────────────────────────────
        const float sr = strokeColor_.redF();
        const float sg = strokeColor_.greenF();
        const float sb = strokeColor_.blueF();
        const float so = strokeColor_.alphaF();
        const float opac = std::clamp(opacity_ / 100.0f, 0.0f, 1.0f);

        cv::Mat strokeLayer(H, W, CV_32FC4);
        for (int y = 0; y < H; ++y) {
            const float* aRow = strokeAlpha.ptr<float>(y);
            cv::Vec4f*   lRow = strokeLayer.ptr<cv::Vec4f>(y);
            for (int x = 0; x < W; ++x) {
                const float a = std::clamp(aRow[x] * so * opac, 0.0f, 1.0f);
                // OpenCV internal order: B, G, R, A
                lRow[x] = cv::Vec4f(sb, sg, sr, a);
            }
        }

        // ── 5. 合成: Stroke OVER src ──────────────────────────────────────
        dst = src.DeepCopy();
        float* dstData = dst.image().rgba32fData();
        cv::Mat dstMat(H, W, CV_32FC4, dstData);
        cv::Mat srcMat(H, W, CV_32FC4,
                       const_cast<float*>(srcData));

        for (int y = 0; y < H; ++y) {
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
        }
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
        // GPU 実装は将来のフェーズで追加予定。現状は CPU フォールバック。
        cpuImpl_.applyCPU(src, dst);
    }
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
