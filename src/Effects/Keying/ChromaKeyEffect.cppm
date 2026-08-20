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
    float blackClip = std::clamp(blackClip_, 0.0f, 0.9999f);
    float whiteClip = std::clamp(whiteClip_, 0.0001f, 1.0f);
    whiteClip = std::max(whiteClip, blackClip + 0.0001f);
    
    ArtifactCore::Parallel::For(0, rows, rows * cols, [&](int y) {
        cv::Vec4f* ptr = dstMat.ptr<cv::Vec4f>(y);
        for(int x=0; x<cols; ++x) {
            float r = ptr[x][0];
            float g = ptr[x][1];
            float b = ptr[x][2];
            float a = ptr[x][3];

            // Euclidian distance
            float dist = std::sqrt(std::pow(r - kr, 2) + std::pow(g - kg, 2) + std::pow(b - kb, 2));
            if (!std::isfinite(r) || !std::isfinite(g) ||
                !std::isfinite(b) || !std::isfinite(dist) ||
                !std::isfinite(a)) {
                ptr[x][0] = 0.0f;
                ptr[x][1] = 0.0f;
                ptr[x][2] = 0.0f;
                ptr[x][3] = 0.0f;
                continue;
            }

            float alphaFactor = 1.0f;
            if (dist < sim) {
                alphaFactor = 0.0f;
            } else if (dist < sim + smooth) {
                alphaFactor = (dist - sim) / smooth;
            }
            alphaFactor = std::clamp((alphaFactor - blackClip) /
                                         (whiteClip - blackClip),
                                     0.0f, 1.0f);
            
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
            if (previewMatte_) {
                ptr[x][0] = ptr[x][3];
                ptr[x][1] = ptr[x][3];
                ptr[x][2] = ptr[x][3];
                ptr[x][3] = 1.0f;
            }
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
    props.reserve(7);

    auto& keyColorProp = props.emplace_back();
    keyColorProp.setName("keyColor");
    keyColorProp.setDisplayLabel(QStringLiteral("Key Color"));
    keyColorProp.setType(ArtifactCore::PropertyType::Color);
    keyColorProp.setDefaultValue(QVariant());

    auto& similarityProp = props.emplace_back();
    similarityProp.setName("similarity");
    similarityProp.setDisplayLabel(QStringLiteral("Similarity"));
    similarityProp.setType(ArtifactCore::PropertyType::Float);
    similarityProp.setSoftRange(0.0, 1.7320508);
    similarityProp.setHardRange(0.0, 1.7320508);
    similarityProp.setDefaultValue(QVariant(static_cast<double>(similarity())));
    similarityProp.setValue(QVariant(static_cast<double>(similarity())));
    similarityProp.setStep(0.01);
    similarityProp.setTooltip(QStringLiteral("Color-distance tolerance for the keyed screen."));

    auto& smoothProp = props.emplace_back();
    smoothProp.setName("smoothness");
    smoothProp.setDisplayLabel(QStringLiteral("Edge Softness"));
    smoothProp.setType(ArtifactCore::PropertyType::Float);
    smoothProp.setSoftRange(0.001, 1.7320508);
    smoothProp.setHardRange(0.001, 1.7320508);
    smoothProp.setDefaultValue(QVariant(static_cast<double>(smoothness())));
    smoothProp.setValue(QVariant(static_cast<double>(smoothness())));
    smoothProp.setStep(0.01);
    smoothProp.setTooltip(QStringLiteral("Softens the transition at the keyed edge."));

    auto& spillProp = props.emplace_back();
    spillProp.setName("spillReduction");
    spillProp.setDisplayLabel(QStringLiteral("Spill Reduction"));
    spillProp.setType(ArtifactCore::PropertyType::Float);
    spillProp.setSoftRange(0.0, 1.0);
    spillProp.setHardRange(0.0, 1.0);
    spillProp.setDefaultValue(QVariant(static_cast<double>(spillReduction())));
    spillProp.setValue(QVariant(static_cast<double>(spillReduction())));
    spillProp.setStep(0.01);
    spillProp.setTooltip(QStringLiteral("Suppresses the sampled screen color in retained pixels."));

    auto& blackClipProp = props.emplace_back();
    blackClipProp.setName("blackClip");
    blackClipProp.setDisplayLabel(QStringLiteral("Matte Black Clip"));
    blackClipProp.setType(ArtifactCore::PropertyType::Float);
    blackClipProp.setHardRange(0.0, 1.0);
    blackClipProp.setSoftRange(0.0, 0.5);
    blackClipProp.setDefaultValue(0.0);
    blackClipProp.setValue(static_cast<double>(blackClip()));
    blackClipProp.setStep(0.01);
    blackClipProp.setTooltip(QStringLiteral("Raises the matte floor to remove weak residual screen coverage."));

    auto& whiteClipProp = props.emplace_back();
    whiteClipProp.setName("whiteClip");
    whiteClipProp.setDisplayLabel(QStringLiteral("Matte White Clip"));
    whiteClipProp.setType(ArtifactCore::PropertyType::Float);
    whiteClipProp.setHardRange(0.0, 1.0);
    whiteClipProp.setSoftRange(0.5, 1.0);
    whiteClipProp.setDefaultValue(1.0);
    whiteClipProp.setValue(static_cast<double>(whiteClip()));
    whiteClipProp.setStep(0.01);
    whiteClipProp.setTooltip(QStringLiteral("Lowers the matte ceiling to force clean opaque foreground."));

    auto& previewMatteProp = props.emplace_back();
    previewMatteProp.setName("previewMatte");
    previewMatteProp.setDisplayLabel(QStringLiteral("Preview Matte"));
    previewMatteProp.setType(ArtifactCore::PropertyType::Boolean);
    previewMatteProp.setDefaultValue(false);
    previewMatteProp.setValue(previewMatte());
    previewMatteProp.setTooltip(QStringLiteral("Display the generated alpha matte as an opaque grayscale image."));

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
    } else if (n == "blackClip") {
        setBlackClip(safeValue(0.0f, 0.0f, 1.0f));
    } else if (n == "whiteClip") {
        setWhiteClip(safeValue(1.0f, 0.0f, 1.0f));
    } else if (n == "previewMatte") {
        setPreviewMatte(value.toBool());
    } else if (n == "keyColor") {
        // Expect QColor or other representation; best-effort
        if (value.canConvert<QColor>()) {
            QColor c = value.value<QColor>();
            if (!c.isValid()) return;
            setKeyColor(FloatRGBA(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
        }
    } else {
        setCommonPropertyValue(n, value);
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

void ChromaKeyEffect::setBlackClip(float val) {
    const float black = std::isfinite(val) ? std::clamp(val, 0.0f, 1.0f) : 0.0f;
    typedCpuImpl_->setBlackClip(std::min(black, typedCpuImpl_->whiteClip() - 0.0001f));
}
float ChromaKeyEffect::blackClip() const { return typedCpuImpl_->blackClip(); }

void ChromaKeyEffect::setWhiteClip(float val) {
    const float white = std::isfinite(val) ? std::clamp(val, 0.0f, 1.0f) : 1.0f;
    typedCpuImpl_->setWhiteClip(std::max(white, typedCpuImpl_->blackClip() + 0.0001f));
}
float ChromaKeyEffect::whiteClip() const { return typedCpuImpl_->whiteClip(); }

void ChromaKeyEffect::setPreviewMatte(bool enabled) {
    typedCpuImpl_->setPreviewMatte(enabled);
}
bool ChromaKeyEffect::previewMatte() const { return typedCpuImpl_->previewMatte(); }

}
