module;
#include <QString>
#include <QColor>
#include <QImage>
#include <QVariant>
#include <QVector>
#include <QRect>
#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>

module Artifact.Effect.AutoMosaic;

import Artifact.Effect.Abstract;
import ArtifactCore.ImageProcessing.FaceDetection;
import Utils.String.UniString;
import Property.Abstract;
import CvUtils;

namespace Artifact {

// 指定領域にピクセル化モザイクを適用
static cv::Mat applyPixelateMosaic(const cv::Mat& src, const cv::Rect& region, int blockSize) {
    cv::Mat result = src.clone();
    cv::Mat roi = result(region);

    // ダウンサンプリング → アップサンプリングでピクセル化
    int w = region.width;
    int h = region.height;
    int smallW = std::max(1, w / blockSize);
    int smallH = std::max(1, h / blockSize);

    cv::Mat small, pixelated;
    cv::resize(roi, small, cv::Size(smallW, smallH), 0, 0, cv::INTER_LINEAR);
    cv::resize(small, pixelated, cv::Size(w, h), 0, 0, cv::INTER_NEAREST);

    pixelated.copyTo(roi);
    return result;
}

// 指定領域にガウスぼかしを適用
static cv::Mat applyGaussianBlur(const cv::Mat& src, const cv::Rect& region, int radius) {
    cv::Mat result = src.clone();
    cv::Mat roi = result(region);

    int ksize = std::max(3, radius * 2 + 1);
    if (ksize % 2 == 0) ksize++;

    cv::Mat blurred;
    cv::GaussianBlur(roi, blurred, cv::Size(ksize, ksize), 0, 0);
    blurred.copyTo(roi);
    return result;
}

// 指定領域にメディアンフィルタを適用
static cv::Mat applyMedianBlur(const cv::Mat& src, const cv::Rect& region, int radius) {
    cv::Mat result = src.clone();
    cv::Mat roi = result(region);

    int ksize = std::max(3, radius * 2 + 1);
    if (ksize % 2 == 0) ksize++;

    cv::Mat blurred;
    cv::medianBlur(roi, blurred, ksize);
    blurred.copyTo(roi);
    return result;
}

// 領域にフェザーを適用（アルファブレンディング）
static cv::Mat applyFeather(const cv::Mat& original, const cv::Mat& processed, const cv::Rect& region, float featherPx) {
    if (featherPx <= 0.0f) return processed;

    cv::Mat result = original.clone();
    cv::Rect expanded = region;
    int feather = static_cast<int>(std::ceil(featherPx));

    expanded.x = std::max(0, expanded.x - feather);
    expanded.y = std::max(0, expanded.y - feather);
    expanded.width = std::min(original.cols - expanded.x, expanded.width + feather * 2);
    expanded.height = std::min(original.rows - expanded.y, expanded.height + feather * 2);

    if (expanded.width <= 0 || expanded.height <= 0) return result;

    // フェザーマスクを作成
    cv::Mat mask = cv::Mat::zeros(expanded.size(), CV_32FC1);

    // 中心領域は 1.0、境界は 0.0 → 1.0 にグラデーション
    cv::Rect innerRegion(
        feather,
        feather,
        std::max(1, expanded.width - feather * 2),
        std::max(1, expanded.height - feather * 2)
    );

    // 距離変換でグラデーションマスクを生成
    cv::Mat innerMask = cv::Mat::zeros(expanded.size(), CV_8UC1);
    innerMask(innerRegion) = 255;

    cv::Mat distTransform;
    cv::distanceTransform(innerMask, distTransform, cv::DIST_L2, 3);

    float maxDist = static_cast<float>(feather);
    if (maxDist > 0) {
        distTransform.convertTo(mask, CV_32FC1, 1.0 / maxDist);
        cv::threshold(mask, mask, 1.0, 1.0, cv::THRESH_TRUNC);
    } else {
        mask = cv::Mat::ones(expanded.size(), CV_32FC1);
    }

    // アルファブレンディング
    cv::Mat origRoi = result(expanded);
    cv::Mat procRoi = processed(expanded);

    std::vector<cv::Mat> origChannels, procChannels;
    cv::split(origRoi, origChannels);
    cv::split(procRoi, procChannels);

    int channels = origRoi.channels();
    for (int c = 0; c < channels; ++c) {
        for (int y = 0; y < expanded.height; ++y) {
            for (int x = 0; x < expanded.width; ++x) {
                float alpha = mask.at<float>(y, x);
                uchar& orig = origChannels[c].at<uchar>(y, x);
                uchar proc = procChannels[c].at<uchar>(y, x);
                orig = static_cast<uchar>(orig * (1.0f - alpha) + proc * alpha);
            }
        }
    }
    cv::merge(origChannels, origRoi);

    return result;
}

QImage AutoMosaicEffect::applyMosaic(const QImage& input, const QVector<QRect>& regions) const {
    if (input.isNull() || regions.isEmpty()) return input;

    cv::Mat src = CvUtils::qImageToCvMat(input, true);
    if (src.empty()) return input;

    cv::Mat result = src.clone();

    for (const QRect& region : regions) {
        if (region.isEmpty()) continue;

        cv::Rect cvRegion(
            std::max(0, region.x()),
            std::max(0, region.y()),
            std::min(region.width(), src.cols - region.x()),
            std::min(region.height(), src.rows - region.y())
        );

        if (cvRegion.width <= 0 || cvRegion.height <= 0) continue;

        cv::Mat processed;
        switch (mosaicType_) {
        case MosaicType::Pixelate:
            processed = applyPixelateMosaic(result, cvRegion, mosaicStrength_);
            break;
        case MosaicType::Gaussian:
            processed = applyGaussianBlur(result, cvRegion, mosaicStrength_);
            break;
        case MosaicType::Median:
            processed = applyMedianBlur(result, cvRegion, mosaicStrength_);
            break;
        }

        if (feather_ > 0.0f) {
            processed = applyFeather(result, processed, cvRegion, feather_);
        }

        processed(cvRegion).copyTo(result(cvRegion));
    }

    return CvUtils::cvMatToQImage(result);
}

QImage AutoMosaicEffect::applyToImage(const QImage& input) const {
    if (input.isNull()) return input;

    QVector<QRect> regions;

    // 顔検出
    if (useFaceDetection_ && faceDetector_) {
        auto faces = faceDetector_->detect(input);
        for (const auto& face : faces) {
            // 領域を少し広めに取る（顔全体をカバー）
            QRect expanded = face.rect;
            int margin = static_cast<int>(expanded.width() * 0.15f);
            expanded.adjust(-margin, -margin, margin, margin);
            regions.append(expanded);
        }
    }

    // 手動領域
    if (useCustomRegions_) {
        regions.append(customRegions_);
    }

    if (regions.isEmpty()) return input;

    return applyMosaic(input, regions);
}

std::vector<AbstractProperty> AutoMosaicEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    AbstractProperty typeProp;
    typeProp.setName("Mosaic Type");
    typeProp.setType(PropertyType::Integer);
    typeProp.setValue(static_cast<int>(mosaicType_));
    typeProp.setDefaultValue(static_cast<int>(mosaicType_));
    typeProp.setTooltip(QStringLiteral("0=Pixelate\n1=Gaussian\n2=Median"));
    typeProp.setDisplayPriority(-40);
    props.push_back(typeProp);

    AbstractProperty strengthProp;
    strengthProp.setName("Strength");
    strengthProp.setType(PropertyType::Integer);
    strengthProp.setValue(mosaicStrength_);
    strengthProp.setDefaultValue(mosaicStrength_);
    strengthProp.setMinValue(1);
    strengthProp.setMaxValue(128);
    strengthProp.setStep(1);
    strengthProp.setAnimatable(true);
    strengthProp.setDisplayPriority(-30);
    props.push_back(strengthProp);

    AbstractProperty featherProp;
    featherProp.setName("Feather");
    featherProp.setType(PropertyType::Float);
    featherProp.setValue(feather_);
    featherProp.setDefaultValue(feather_);
    featherProp.setMinValue(0.0f);
    featherProp.setMaxValue(128.0f);
    featherProp.setStep(0.5f);
    featherProp.setAnimatable(true);
    featherProp.setDisplayPriority(-20);
    props.push_back(featherProp);

    AbstractProperty faceDetProp;
    faceDetProp.setName("Face Detection");
    faceDetProp.setType(PropertyType::Boolean);
    faceDetProp.setValue(useFaceDetection_);
    faceDetProp.setDefaultValue(useFaceDetection_);
    faceDetProp.setDisplayPriority(-10);
    props.push_back(faceDetProp);

    AbstractProperty customProp;
    customProp.setName("Custom Regions");
    customProp.setType(PropertyType::Boolean);
    customProp.setValue(useCustomRegions_);
    customProp.setDefaultValue(useCustomRegions_);
    customProp.setDisplayPriority(0);
    props.push_back(customProp);

    return props;
}

void AutoMosaicEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Mosaic Type")) {
        setMosaicType(static_cast<MosaicType>(value.toInt()));
    } else if (key == QStringLiteral("Strength")) {
        setMosaicStrength(value.toInt());
    } else if (key == QStringLiteral("Feather")) {
        setFeather(value.toFloat());
    } else if (key == QStringLiteral("Face Detection")) {
        setUseFaceDetection(value.toBool());
    } else if (key == QStringLiteral("Custom Regions")) {
        setUseCustomRegions(value.toBool());
    }
}

} // namespace Artifact
