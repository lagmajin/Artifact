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




import Artifact.Effect.Keying.ChromaKey;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import Artifact.Effect.Abstract;
import FloatRGBA;
import Utils.String.UniString;
import Property.Abstract;

// Global includes for Qt types used in this implementation

namespace Artifact {
 using namespace ArtifactCore;

void ChromaKeyEffectCPUImpl::applyCPU(const ArtifactCore::ImageF32x4RGBAWithCache& src, ArtifactCore::ImageF32x4RGBAWithCache& dst) {
    const ArtifactCore::ImageF32x4_RGBA& srcImage = src.image();
    cv::Mat srcMat = srcImage.toCVMat();

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
    
    float sim = similarity_;
    float smooth = smoothness_;
    float spill = spillReduction_;
    
    if(smooth < 0.001f) smooth = 0.001f;

    for(int y=0; y<rows; ++y) {
        cv::Vec4f* ptr = dstMat.ptr<cv::Vec4f>(y);
        for(int x=0; x<cols; ++x) {
            float r = ptr[x][0];
            float g = ptr[x][1];
            float b = ptr[x][2];
            float a = ptr[x][3];

            // Euclidian distance
            float dist = std::sqrt(std::pow(r - kr, 2) + std::pow(g - kg, 2) + std::pow(b - kb, 2));

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

            ptr[x][3] = a * alphaFactor;
        }
    }

    ImageF32x4_RGBA dstImage;
    dstImage.setFromCVMat(dstMat);
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
    similarityProp.setDefaultValue(QVariant(static_cast<double>(similarity())));
    similarityProp.setValue(QVariant(static_cast<double>(similarity())));

    auto& smoothProp = props.emplace_back();
    smoothProp.setName("smoothness");
    smoothProp.setType(ArtifactCore::PropertyType::Float);
    smoothProp.setDefaultValue(QVariant(static_cast<double>(smoothness())));
    smoothProp.setValue(QVariant(static_cast<double>(smoothness())));

    auto& spillProp = props.emplace_back();
    spillProp.setName("spillReduction");
    spillProp.setType(ArtifactCore::PropertyType::Float);
    spillProp.setDefaultValue(QVariant(static_cast<double>(spillReduction())));
    spillProp.setValue(QVariant(static_cast<double>(spillReduction())));

    return props;
}

void ChromaKeyEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    QString n = name.toQString();
    if (n == "similarity") {
        setSimilarity(static_cast<float>(value.toDouble()));
    } else if (n == "smoothness") {
        setSmoothness(static_cast<float>(value.toDouble()));
    } else if (n == "spillReduction") {
        setSpillReduction(static_cast<float>(value.toDouble()));
    } else if (n == "keyColor") {
        // Expect QColor or other representation; best-effort
        if (value.canConvert<QColor>()) {
            QColor c = value.value<QColor>();
            setKeyColor(FloatRGBA(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
        }
    }
}

ChromaKeyEffect::ChromaKeyEffect() : ArtifactAbstractEffect() {
    typedCpuImpl_ = std::make_shared<ChromaKeyEffectCPUImpl>();
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
    typedCpuImpl_->setSimilarity(val);
}
float ChromaKeyEffect::similarity() const {
    return typedCpuImpl_->similarity();
}

void ChromaKeyEffect::setSmoothness(float val) {
    typedCpuImpl_->setSmoothness(val);
}
float ChromaKeyEffect::smoothness() const {
    return typedCpuImpl_->smoothness();
}

void ChromaKeyEffect::setSpillReduction(float val) {
    typedCpuImpl_->setSpillReduction(val);
}
float ChromaKeyEffect::spillReduction() const {
    return typedCpuImpl_->spillReduction();
}

}
