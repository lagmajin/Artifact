module;
#include <cmath>
#include <QVariant>
#include <QColor>

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
#include <opencv2/opencv.hpp>
module Artifact.Effect.Keying.ChromaKey;




import Artifact.Effect.ImplBase;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import Artifact.Effect.Abstract;
import FloatRGBA;
import Utils.String.UniString;
import Property.Abstract;
import Core.Parallel;
import Memory.SharedPtr;

// Global includes for Qt types used in this implementation

namespace Artifact {
 using namespace ArtifactCore;

void ChromaKeyEffectCPUImpl::applyCPU(const ArtifactCore::ImageF32x4RGBAWithCache& src, ArtifactCore::ImageF32x4RGBAWithCache& dst) {
    const ArtifactCore::ImageF32x4_RGBA& srcImage = src.image();
    const float* srcData = srcImage.rgba32fData();
    if (!srcData) {
        dst = src;
        return;
    }
    cv::Mat srcMat(srcImage.height(), srcImage.width(), CV_32FC4, const_cast<float*>(srcData));

    // Ensure we have data
    if (srcMat.empty()) {
        dst = src;
        return;
    }
    
    cv::Mat dstMat = srcMat.clone();

    // Loop
    int rows = dstMat.rows;
    int cols = dstMat.cols;
    
    // Use stored parameters
    float kr = keyColor_.r();
    float kg = keyColor_.g();
    float kb = keyColor_.b();
    
    float sim = std::clamp(similarity_, 0.0f, 1.7320508f);
    float smooth = std::clamp(smoothness_, 0.001f, 1.7320508f);
    float spill = std::clamp(spillReduction_, 0.0f, 1.0f);
    
    ArtifactCore::Parallel::For(0, rows, rows * cols, [&](int y) {
        cv::Vec4f* ptr = dstMat.ptr<cv::Vec4f>(y);
        for(int x=0; x<cols; ++x) {
            float r = ptr[x][0];
            float g = ptr[x][1];
            float b = ptr[x][2];
            float a = ptr[x][3];

            // Euclidian distance
            float dist = std::sqrt(std::pow(r - kr, 2) + std::pow(g - kg, 2) + std::pow(b - kb, 2));
            if (!std::isfinite(dist) || !std::isfinite(a)) {
                ptr[x][3] = 0.0f;
                continue;
            }

            float alphaFactor = 1.0f;
            if (dist < sim) {
                alphaFactor = 0.0f;
            } else if (dist < sim + smooth) {
                alphaFactor = (dist - sim) / smooth;
            }
            
            // Simple spill reduction (optional/basic)
            if (dist < sim + smooth + 0.2f && spill > 0.0f) {
                 float gray = r * 0.299f + g * 0.587f + b * 0.114f;
                 // Factor depends on how "green" it is
                 float factor = spill * (1.0f - std::min(1.0f, (dist - sim) / (smooth + 0.2f)));
                 if (factor > 0.0f) {
                    ptr[x][0] = r * (1.0f - factor) + gray * factor;
                    ptr[x][1] = g * (1.0f - factor) + gray * factor;
                    ptr[x][2] = b * (1.0f - factor) + gray * factor;
                 }
            }

            ptr[x][3] = std::clamp(a * alphaFactor, 0.0f, 1.0f);
        }
    });

    ImageF32x4_RGBA dstImage;
    dstImage.setFromRGBA32F(
        dstMat.ptr<float>(), dstMat.cols, dstMat.rows,
        srcImage.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(dstImage);
}
// Properties - single definitions placed after implementation
// Properties - single definitions placed after implementation
std::vector<ArtifactCore::AbstractProperty> ChromaKeyEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(4);

    auto& keyColorProp = props.emplace_back();
    keyColorProp.setName("keyColor");
    keyColorProp.setType(ArtifactCore::PropertyType::Color);
    keyColorProp.setDefaultValue(QVariant());

    auto& similarityProp = props.emplace_back();
    similarityProp.setName("similarity");
    similarityProp.setType(ArtifactCore::PropertyType::Float);
    similarityProp.setSoftRange(0.0, 1.7320508);
    similarityProp.setHardRange(0.0, 1.7320508);
    similarityProp.setDefaultValue(QVariant(static_cast<double>(similarity())));
    similarityProp.setValue(QVariant(static_cast<double>(similarity())));

    auto& smoothProp = props.emplace_back();
    smoothProp.setName("smoothness");
    smoothProp.setType(ArtifactCore::PropertyType::Float);
    smoothProp.setSoftRange(0.001, 1.7320508);
    smoothProp.setHardRange(0.001, 1.7320508);
    smoothProp.setDefaultValue(QVariant(static_cast<double>(smoothness())));
    smoothProp.setValue(QVariant(static_cast<double>(smoothness())));

    auto& spillProp = props.emplace_back();
    spillProp.setName("spillReduction");
    spillProp.setType(ArtifactCore::PropertyType::Float);
    spillProp.setSoftRange(0.0, 1.0);
    spillProp.setHardRange(0.0, 1.0);
    spillProp.setDefaultValue(QVariant(static_cast<double>(spillReduction())));
    spillProp.setValue(QVariant(static_cast<double>(spillReduction())));

    return props;
}

void ChromaKeyEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    QString n = name.toQString();
    const auto safeValue = [&value](const float fallback,
                                    const float minimum,
                                    const float maximum) {
        const float raw = static_cast<float>(value.toDouble());
        return std::isfinite(raw) ? std::clamp(raw, minimum, maximum) : fallback;
    };
    if (n == "similarity") {
        setSimilarity(safeValue(0.0f, 0.0f, 1.7320508f));
    } else if (n == "smoothness") {
        setSmoothness(safeValue(0.001f, 0.001f, 1.7320508f));
    } else if (n == "spillReduction") {
        setSpillReduction(safeValue(0.0f, 0.0f, 1.0f));
    } else if (n == "keyColor") {
        // Expect QColor or other representation; best-effort
        if (value.canConvert<QColor>()) {
            QColor c = value.value<QColor>();
            if (!c.isValid()) return;
            setKeyColor(FloatRGBA(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
        }
    }
}

ChromaKeyEffect::ChromaKeyEffect() : ArtifactAbstractEffect() {
    typedCpuImpl_ = ArtifactCore::makeShared<ChromaKeyEffectCPUImpl>();
    setCPUImpl(typedCpuImpl_);
    setDisplayName("Chroma Key");
    setEffectID("Effect.Keying.ChromaKey");
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

void ChromaKeyEffect::setKeyColor(const FloatRGBA& color) {
    typedCpuImpl_->setKeyColor(color);
}
const FloatRGBA& ChromaKeyEffect::keyColor() const {
    return typedCpuImpl_->keyColor();
}

void ChromaKeyEffect::setSimilarity(float val) {
    typedCpuImpl_->setSimilarity(std::isfinite(val) ? std::clamp(val, 0.0f, 1.7320508f) : 0.4f);
}
float ChromaKeyEffect::similarity() const {
    return typedCpuImpl_->similarity();
}

void ChromaKeyEffect::setSmoothness(float val) {
    typedCpuImpl_->setSmoothness(std::isfinite(val) ? std::clamp(val, 0.001f, 1.7320508f) : 0.1f);
}
float ChromaKeyEffect::smoothness() const {
    return typedCpuImpl_->smoothness();
}

void ChromaKeyEffect::setSpillReduction(float val) {
    typedCpuImpl_->setSpillReduction(std::isfinite(val) ? std::clamp(val, 0.0f, 1.0f) : 0.5f);
}
float ChromaKeyEffect::spillReduction() const {
    return typedCpuImpl_->spillReduction();
}

}
