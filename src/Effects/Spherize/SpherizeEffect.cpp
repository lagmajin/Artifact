module;
#include <QList>
#include <QVariant>
#include <QVector>
#include <cmath>
#include <opencv2/core/mat.hpp>
#include <QtConcurrent>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Effect.Spherize;




import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;

namespace Artifact {

void SpherizeEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const ImageF32x4_RGBA& srcImage = src.image();
    const float* srcData = srcImage.rgba32fData();
    if (!srcData) {
        dst = src;
        return;
    }
    cv::Mat srcMat(srcImage.height(), srcImage.width(), CV_32FC4, const_cast<float*>(srcData));
    
    int height = srcMat.rows;
    int width = srcMat.cols;
    cv::Mat dstMat(height, width, CV_32FC4);
    
    // パラメータ
    float amount = amount_ / 100.0f;  // -1.0〜1.0に変換
    float radius = radius_;             // 0.0〜1.0
    float cx = centerX_ * width;       // ピクセルの絶対座標
    float cy = centerY_ * height;
    float maxRadius = std::min(width, height) * radius;
    
    // ピクセルごとの処理 — 行単位で並列化
    std::vector<cv::Vec4f> rowResults(width * height);

    QVector<int> rows(height);
    std::iota(rows.begin(), rows.end(), 0);

    QtConcurrent::blockingMap(rows, [&](int y) {
        for (int x = 0; x < width; x++) {
            // 中心からの距離と角度を計算
            float dx = static_cast<float>(x) - cx;
            float dy = static_cast<float>(y) - cy;
            float dist = std::sqrt(dx * dx + dy * dy);
            float angle = std::atan2(dy, dx);

            // 球面歪みの計算
            float normalizedDist = dist / maxRadius;
            if (normalizedDist > 1.0f) {
                // 半径外は歪みなし
                cv::Vec4f pixel = srcMat.at<cv::Vec4f>(y, x);
                rowResults[y * width + x] = pixel;
                continue;
            }

            // 球面投影の計算
            float z = std::sqrt(std::max(0.0f, 1.0f - normalizedDist * normalizedDist));

            // 新しい座標へのマッピング
            float newDist = normalizedDist + amount * z * normalizedDist * (1.0f - normalizedDist);

            // 元の画像からのサンプリング座標
            int srcX = static_cast<int>(cx + newDist * maxRadius * std::cos(angle));
            int srcY = static_cast<int>(cy + newDist * maxRadius * std::sin(angle));

            // 境界チェック
            srcX = std::max(0, std::min(width - 1, srcX));
            srcY = std::max(0, std::min(height - 1, srcY));

            // ピクセルをコピー
            cv::Vec4f pixel = srcMat.at<cv::Vec4f>(srcY, srcX);
            rowResults[y * width + x] = pixel;
        }
    });

    // 結果をdstMatにコピー
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dstMat.at<cv::Vec4f>(y, x) = rowResults[y * width + x];
        }
    }
    
    // 結果をdstに設定
    ImageF32x4_RGBA dstImage;
    dstImage.setFromRGBA32F(dstMat.ptr<float>(), dstMat.cols, dstMat.rows);
}

void SpherizeEffectGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    // 現在はCPUバックエンドにフォールバック
    // TODO: HLSLシェーダの実装
    SpherizeEffectCPUImpl cpuImpl;
    cpuImpl.setAmount(amount_);
    cpuImpl.setRadius(radius_);
    cpuImpl.setCenterX(centerX_);
    cpuImpl.setCenterY(centerY_);
    cpuImpl.applyCPU(src, dst);
}

class SpherizeEffect::Impl {
public:
    std::shared_ptr<SpherizeEffectCPUImpl> cpuImpl_;
    std::shared_ptr<SpherizeEffectGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = std::make_shared<SpherizeEffectCPUImpl>();
        gpuImpl_ = std::make_shared<SpherizeEffectGPUImpl>();
    }
};

SpherizeEffect::SpherizeEffect() : impl_(new Impl()) {
    setDisplayName(ArtifactCore::UniString("Spherize"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(impl_->cpuImpl_);
    setGPUImpl(impl_->gpuImpl_);
}

SpherizeEffect::~SpherizeEffect() {
    delete impl_;
}

void SpherizeEffect::setAmount(float amount) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setAmount(amount);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setAmount(amount);
    }
}

float SpherizeEffect::amount() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->amount();
    }
    return 0.0f;
}

void SpherizeEffect::setRadius(float radius) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setRadius(radius);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setRadius(radius);
    }
}

float SpherizeEffect::radius() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->radius();
    }
    return 0.0f;
}

void SpherizeEffect::setCenterX(float cx) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setCenterX(cx);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setCenterX(cx);
    }
}

float SpherizeEffect::centerX() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->centerX();
    }
    return 0.0f;
}

void SpherizeEffect::setCenterY(float cy) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setCenterY(cy);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setCenterY(cy);
    }
}

float SpherizeEffect::centerY() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->centerY();
    }
    return 0.0f;
}

std::vector<ArtifactCore::AbstractProperty> SpherizeEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(4);

    auto& amountProp = props.emplace_back();
    amountProp.setName("amount");
    amountProp.setType(ArtifactCore::PropertyType::Float);
    amountProp.setDefaultValue(QVariant(static_cast<double>(amount())));
    amountProp.setValue(QVariant(static_cast<double>(amount())));

    auto& radiusProp = props.emplace_back();
    radiusProp.setName("radius");
    radiusProp.setType(ArtifactCore::PropertyType::Float);
    radiusProp.setDefaultValue(QVariant(static_cast<double>(radius())));
    radiusProp.setValue(QVariant(static_cast<double>(radius())));

    auto& cxProp = props.emplace_back();
    cxProp.setName("centerX");
    cxProp.setType(ArtifactCore::PropertyType::Float);
    cxProp.setDefaultValue(QVariant(static_cast<double>(centerX())));
    cxProp.setValue(QVariant(static_cast<double>(centerX())));

    auto& cyProp = props.emplace_back();
    cyProp.setName("centerY");
    cyProp.setType(ArtifactCore::PropertyType::Float);
    cyProp.setDefaultValue(QVariant(static_cast<double>(centerY())));
    cyProp.setValue(QVariant(static_cast<double>(centerY())));

    return props;
}

void SpherizeEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    QString n = name.toQString();
    if (n == "amount") {
        setAmount(static_cast<float>(value.toDouble()));
    } else if (n == "radius") {
        setRadius(static_cast<float>(value.toDouble()));
    } else if (n == "centerX") {
        setCenterX(static_cast<float>(value.toDouble()));
    } else if (n == "centerY") {
        setCenterY(static_cast<float>(value.toDouble()));
    }
}

}
