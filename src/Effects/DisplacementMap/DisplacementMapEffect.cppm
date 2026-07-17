module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <QString>
#include <QVariant>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Distort.DisplacementMap;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;

namespace Artifact {

using namespace ArtifactCore;

// ─── ヘルパー ─────────────────────────────────────────────────────────────────

namespace {

// ピクセルの指定チャンネル値を取得 (0-1)
inline float sampleChannel(const float* pixel4, DisplaceChannel ch) {
    switch (ch) {
        case DisplaceChannel::Red:       return pixel4[0];
        case DisplaceChannel::Green:     return pixel4[1];
        case DisplaceChannel::Blue:      return pixel4[2];
        case DisplaceChannel::Alpha:     return pixel4[3];
        case DisplaceChannel::Luminance:
        default:
            // Rec.601 luma
            return pixel4[0] * 0.299f + pixel4[1] * 0.587f + pixel4[2] * 0.114f;
    }
}

// バイリニアサンプリング（クランプ境界）
cv::Vec4f sampleBilinear(const cv::Mat& mat, float fx, float fy) {
    const int W = mat.cols;
    const int H = mat.rows;
    const int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, W - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, H - 1);
    const int x1 = std::min(x0 + 1, W - 1);
    const int y1 = std::min(y0 + 1, H - 1);
    const float tx = fx - std::floor(fx);
    const float ty = fy - std::floor(fy);

    const cv::Vec4f& p00 = mat.at<cv::Vec4f>(y0, x0);
    const cv::Vec4f& p10 = mat.at<cv::Vec4f>(y0, x1);
    const cv::Vec4f& p01 = mat.at<cv::Vec4f>(y1, x0);
    const cv::Vec4f& p11 = mat.at<cv::Vec4f>(y1, x1);

    return (p00 * (1.f - tx) + p10 * tx) * (1.f - ty)
         + (p01 * (1.f - tx) + p11 * tx) * ty;
}

// ラップ境界
cv::Vec4f sampleWrap(const cv::Mat& mat, float fx, float fy) {
    const int W = mat.cols;
    const int H = mat.rows;
    auto wrapCoord = [](float v, int size) -> float {
        v = std::fmod(v, static_cast<float>(size));
        if (v < 0.f) v += static_cast<float>(size);
        return v;
    };
    return sampleBilinear(mat,
        wrapCoord(fx, W),
        wrapCoord(fy, H));
}

} // namespace

// ─── CPU Impl ────────────────────────────────────────────────────────────────

class DisplacementMapCPUImpl : public ArtifactEffectImplBase {
public:
    float           maxHorizontal_     = 20.0f;
    float           maxVertical_       = 20.0f;
    DisplaceChannel horizontalChannel_ = DisplaceChannel::Red;
    DisplaceChannel verticalChannel_   = DisplaceChannel::Green;
    bool            wrapAround_        = false;

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

        // ソースを cv::Mat でラップ（コピー不要）
        cv::Mat srcMat(H, W, CV_32FC4, const_cast<float*>(srcData));

        dst = src.DeepCopy();
        float* dstData = dst.image().rgba32fData();
        cv::Mat dstMat(H, W, CV_32FC4, dstData);

        ArtifactCore::Parallel::For(0, H, [&](int y) {
            const float* mapRow = srcMat.ptr<float>(y);
            cv::Vec4f*   outRow = dstMat.ptr<cv::Vec4f>(y);
            for (int x = 0; x < W; ++x) {
                const float* mapPx = mapRow + x * 4;

                // AE 準拠: 0.5 がニュートラル → [-0.5, +0.5] 範囲にシフト
                const float hVal = sampleChannel(mapPx, horizontalChannel_) - 0.5f;
                const float vVal = sampleChannel(mapPx, verticalChannel_)   - 0.5f;

                const float srcX = static_cast<float>(x) + hVal * maxHorizontal_;
                const float srcY = static_cast<float>(y) + vVal * maxVertical_;

                if (wrapAround_) {
                    outRow[x] = sampleWrap(srcMat, srcX, srcY);
                } else {
                    outRow[x] = sampleBilinear(srcMat, srcX, srcY);
                }
            }
        });
    }
};

// ─── DisplacementMapEffect ────────────────────────────────────────────────────

DisplacementMapEffect::DisplacementMapEffect()
{
    setDisplayName(UniString("Displacement Map"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    auto cpu = std::make_shared<DisplacementMapCPUImpl>();
    setCPUImpl(cpu);
    // GPU 実装は将来フェーズ
}

DisplacementMapEffect::~DisplacementMapEffect() = default;

// ── アクセサ ─────────────────────────────────────────────────────────────────

float DisplacementMapEffect::maxHorizontal() const { return maxHorizontal_; }
void  DisplacementMapEffect::setMaxHorizontal(float v) {
    maxHorizontal_ = v;
    syncImpls();
}

float DisplacementMapEffect::maxVertical() const { return maxVertical_; }
void  DisplacementMapEffect::setMaxVertical(float v) {
    maxVertical_ = v;
    syncImpls();
}

DisplaceChannel DisplacementMapEffect::horizontalChannel() const { return horizontalChannel_; }
void            DisplacementMapEffect::setHorizontalChannel(DisplaceChannel c) {
    horizontalChannel_ = c;
    syncImpls();
}

DisplaceChannel DisplacementMapEffect::verticalChannel() const { return verticalChannel_; }
void            DisplacementMapEffect::setVerticalChannel(DisplaceChannel c) {
    verticalChannel_ = c;
    syncImpls();
}

bool DisplacementMapEffect::wrapAround() const { return wrapAround_; }
void DisplacementMapEffect::setWrapAround(bool v) {
    wrapAround_ = v;
    syncImpls();
}

// ── Properties API ────────────────────────────────────────────────────────────

std::vector<AbstractProperty> DisplacementMapEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(5);

    auto& hProp = props.emplace_back();
    hProp.setName("Max Horizontal");
    hProp.setType(PropertyType::Float);
    hProp.setValue(maxHorizontal_);

    auto& vProp = props.emplace_back();
    vProp.setName("Max Vertical");
    vProp.setType(PropertyType::Float);
    vProp.setValue(maxVertical_);

    auto& hcProp = props.emplace_back();
    hcProp.setName("Horizontal Channel");
    hcProp.setType(PropertyType::Integer);
    hcProp.setValue(static_cast<int>(horizontalChannel_));

    auto& vcProp = props.emplace_back();
    vcProp.setName("Vertical Channel");
    vcProp.setType(PropertyType::Integer);
    vcProp.setValue(static_cast<int>(verticalChannel_));

    auto& wProp = props.emplace_back();
    wProp.setName("Wrap Around");
    wProp.setType(PropertyType::Boolean);
    wProp.setValue(wrapAround_);

    return props;
}

void DisplacementMapEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString k = name.toQString();
    if      (k == "Max Horizontal")      setMaxHorizontal(value.toFloat());
    else if (k == "Max Vertical")        setMaxVertical(value.toFloat());
    else if (k == "Horizontal Channel")  setHorizontalChannel(static_cast<DisplaceChannel>(value.toInt()));
    else if (k == "Vertical Channel")    setVerticalChannel(static_cast<DisplaceChannel>(value.toInt()));
    else if (k == "Wrap Around")         setWrapAround(value.toBool());
}

void DisplacementMapEffect::syncImpls() {
    if (auto* c = dynamic_cast<DisplacementMapCPUImpl*>(cpuImpl().get())) {
        c->maxHorizontal_     = maxHorizontal_;
        c->maxVertical_       = maxVertical_;
        c->horizontalChannel_ = horizontalChannel_;
        c->verticalChannel_   = verticalChannel_;
        c->wrapAround_        = wrapAround_;
    }
}

} // namespace Artifact
