module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.ApertureShapeBlur;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Memory.SharedPtr;

namespace Artifact {

using namespace ArtifactCore;

namespace {

cv::Mat makeBuiltInPsf(int size, int shape, float rotationDegrees,
                       float edgeBrightness) {
    size = std::max(3, size | 1);
    cv::Mat psf(size, size, CV_32F, cv::Scalar(0.0f));
    const float center = (size - 1) * 0.5f;
    const float radius = std::max(1.0f, center - 1.0f);
    const float angle = rotationDegrees * 0.0174532925f;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const int blades = shape == 1 ? 5 : (shape == 2 ? 6 : 0);
    for (int y = 0; y < size; ++y) {
        float* row = psf.ptr<float>(y);
        for (int x = 0; x < size; ++x) {
            const float dx = (x - center) / radius;
            const float dy = (y - center) / radius;
            const float rx = dx * cosine - dy * sine;
            const float ry = dx * sine + dy * cosine;
            const float radial = std::sqrt(rx * rx + ry * ry);
            bool inside = radial <= 1.0f;
            if (shape == 3) {
                const float heartX = rx * 1.05f;
                const float heartY = -ry * 1.05f + 0.22f;
                const float q = heartX * heartX + heartY * heartY - 0.72f;
                inside = q * q * q - heartX * heartX * heartY * heartY * heartY <= 0.0f;
            } else if (blades > 0 && radial > 0.0f) {
                const float theta = std::atan2(ry, rx) + 3.14159265f;
                const float sector = 6.2831853f / static_cast<float>(blades);
                const float local = std::fmod(theta + sector * 0.5f, sector) - sector * 0.5f;
                const float boundary = std::cos(3.14159265f / blades) /
                                       std::max(0.001f, std::cos(local));
                inside = radial <= boundary;
            }
            if (inside) {
                const float rimT = std::clamp((radial - 0.55f) / 0.45f, 0.0f, 1.0f);
                const float rim = rimT * rimT * (3.0f - 2.0f * rimT);
                row[x] = 1.0f + edgeBrightness * rim;
            }
        }
    }
    return psf;
}

cv::Mat loadAndNormalizePsf(const QString& path, int size, int shape,
                            float rotation, float edgeBrightness) {
    cv::Mat psf;
    if (!path.trimmed().isEmpty()) {
        psf = cv::imread(path.toStdString(), cv::IMREAD_GRAYSCALE);
        if (!psf.empty()) {
            psf.convertTo(psf, CV_32F, 1.0 / 255.0);
            cv::resize(psf, psf, cv::Size(size, size), 0, 0, cv::INTER_AREA);
            const cv::Point2f center((size - 1) * 0.5f, (size - 1) * 0.5f);
            const cv::Mat transform = cv::getRotationMatrix2D(center, rotation, 1.0);
            cv::warpAffine(psf, psf, transform, psf.size(), cv::INTER_LINEAR,
                           cv::BORDER_CONSTANT, cv::Scalar(0.0f));
        }
    }
    if (psf.empty()) psf = makeBuiltInPsf(size, shape, rotation, edgeBrightness);
    const double energy = cv::sum(psf)[0];
    if (energy > 1.0e-8) psf /= energy;
    return psf;
}

cv::Mat fftConvolve(const cv::Mat& channel, const cv::Mat& psf) {
    cv::Mat paddedKernel(channel.size(), CV_32F, cv::Scalar(0.0f));
    const int halfX = psf.cols / 2;
    const int halfY = psf.rows / 2;
    for (int y = 0; y < psf.rows; ++y) {
        const float* srcRow = psf.ptr<float>(y);
        const int dstY = (y - halfY + channel.rows) % channel.rows;
        float* dstRow = paddedKernel.ptr<float>(dstY);
        for (int x = 0; x < psf.cols; ++x) {
            const int dstX = (x - halfX + channel.cols) % channel.cols;
            dstRow[dstX] = srcRow[x];
        }
    }
    cv::Mat imageSpectrum, kernelSpectrum, product, result;
    cv::dft(channel, imageSpectrum, cv::DFT_COMPLEX_OUTPUT);
    cv::dft(paddedKernel, kernelSpectrum, cv::DFT_COMPLEX_OUTPUT);
    cv::mulSpectrums(imageSpectrum, kernelSpectrum, product, 0);
    cv::idft(product, result, cv::DFT_REAL_OUTPUT | cv::DFT_SCALE);
    return result;
}

}

class ApertureShapeBlurCPUImpl final : public ArtifactEffectImplBase {
public:
    float radius = 18.0f;
    int shape = 0;
    float rotation = 0.0f;
    float edgeBrightness = 0.2f;
    float highlightBoost = 0.35f;
    QString psfImagePath;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& image = src.image();
        const float* pixels = image.rgba32fData();
        const int width = image.width();
        const int height = image.height();
        if (!pixels || width <= 0 || height <= 0 || radius < 0.5f) { dst = src; return; }
        cv::Mat input(height, width, CV_32FC4, const_cast<float*>(pixels));
        std::vector<cv::Mat> channels;
        cv::split(input, channels);
        const int kernelSize = std::clamp(static_cast<int>(std::round(radius * 2.0f)) | 1,
                                          3, std::max(3, std::min(width, height) | 1));
        const cv::Mat psf = loadAndNormalizePsf(psfImagePath, kernelSize, shape,
                                               rotation, edgeBrightness);
        std::vector<cv::Mat> outputChannels(4);
        ArtifactCore::Parallel::For(0, 3, [&](int c) {
            outputChannels[c] = fftConvolve(channels[c], psf);
            if (highlightBoost > 0.0f) {
                cv::Mat highlights = channels[c] - 0.65f;
                cv::max(highlights, 0.0, highlights);
                outputChannels[c] += fftConvolve(highlights, psf) * highlightBoost;
            }
        });
        outputChannels[3] = channels[3];
        cv::Mat output;
        cv::merge(outputChannels, output);
        dst = src;
        dst.image().setFromCVMat(output, src.image().colorDescriptor());
    }
};

ApertureShapeBlurEffect::ApertureShapeBlurEffect() {
    setDisplayName(UniString("Aperture Shape Blur"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<ApertureShapeBlurCPUImpl>());
    syncImpl();
}

ApertureShapeBlurEffect::~ApertureShapeBlurEffect() = default;

void ApertureShapeBlurEffect::syncImpl() {
    if (auto* impl = dynamic_cast<ApertureShapeBlurCPUImpl*>(cpuImpl().get())) {
        impl->radius = radius_; impl->shape = shape_; impl->rotation = rotation_;
        impl->edgeBrightness = edgeBrightness_; impl->highlightBoost = highlightBoost_;
        impl->psfImagePath = psfImagePath_;
    }
}

std::vector<AbstractProperty> ApertureShapeBlurEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& radius = props.emplace_back(); radius.setName("Radius"); radius.setType(PropertyType::Float); radius.setValue(radius_);
    auto& shape = props.emplace_back(); shape.setName("Shape"); shape.setType(PropertyType::Integer); shape.setValue(shape_);
    auto& rotation = props.emplace_back(); rotation.setName("Rotation"); rotation.setType(PropertyType::Float); rotation.setValue(rotation_);
    auto& edge = props.emplace_back(); edge.setName("Edge Brightness"); edge.setType(PropertyType::Float); edge.setValue(edgeBrightness_);
    auto& boost = props.emplace_back(); boost.setName("Highlight Boost"); boost.setType(PropertyType::Float); boost.setValue(highlightBoost_);
    auto& path = props.emplace_back(); path.setName("PSF Image Path"); path.setType(PropertyType::String); path.setValue(psfImagePath_);
    return props;
}

void ApertureShapeBlurEffect::setPropertyValue(const UniString& name,
                                               const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Radius")) {
        const float v = value.toFloat();
        radius_ = std::isfinite(v) ? std::clamp(v, 0.0f, 256.0f) : 18.0f;
    }
    else if (key == QStringLiteral("Shape")) shape_ = std::clamp(value.toInt(), 0, 3);
    else if (key == QStringLiteral("Rotation")) {
        const float v = value.toFloat();
        rotation_ = std::isfinite(v) ? v : 0.0f;
    }
    else if (key == QStringLiteral("Edge Brightness")) {
        const float v = value.toFloat();
        edgeBrightness_ = std::isfinite(v) ? std::clamp(v, 0.0f, 3.0f) : 0.2f;
    }
    else if (key == QStringLiteral("Highlight Boost")) {
        const float v = value.toFloat();
        highlightBoost_ = std::isfinite(v) ? std::clamp(v, 0.0f, 4.0f) : 0.35f;
    }
    else if (key == QStringLiteral("PSF Image Path")) psfImagePath_ = value.toString();
    syncImpl();
}

class DepthBokehEffect::Impl {
public:
    float focusDistance = 0.5f;
    float focusRange = 0.08f;
    float foregroundBlur = 0.85f;
    float backgroundBlur = 1.0f;
    float maxRadius = 32.0f;
    int apertureShape = 2;
    float apertureRotation = 0.0f;
    float highlightBoost = 0.5f;
    QString depthInput = QStringLiteral("depth");
    bool useLumaFallback = true;
    IEffectFrameSampler* sampler = nullptr;
    std::int64_t frame = 0;
};

DepthBokehEffect::DepthBokehEffect() : impl_(new Impl()) {
    setEffectID(UniString("builtin.depth_bokeh"));
    setDisplayName(UniString("Depth Bokeh / Rack Defocus"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

DepthBokehEffect::~DepthBokehEffect() {
    delete impl_; impl_ = nullptr;
}

void DepthBokehEffect::onContextUpdated(const EffectContext& context) {
    impl_->sampler = context.sampler;
    impl_->frame = context.compositionFrame;
}

void DepthBokehEffect::apply(const ImageF32x4RGBAWithCache& src,
                             ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width(), height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) { dst = src; return; }

    cv::Mat depth(height, width, CV_32F);
    bool hasDepth = false;
    ImageF32x4RGBAWithCache depthImage;
    if (impl_->sampler && !impl_->depthInput.trimmed().isEmpty() &&
        impl_->sampler->sampleNamedInput(impl_->depthInput, impl_->frame, depthImage) &&
        depthImage.width() == width && depthImage.height() == height) {
        const float* depthPixels = depthImage.image().rgba32fData();
        if (depthPixels) {
            for (int y = 0; y < height; ++y) {
                float* row = depth.ptr<float>(y);
                for (int x = 0; x < width; ++x) {
                    const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4u;
                    row[x] = std::clamp(depthPixels[offset], 0.0f, 1.0f);
                }
            }
            hasDepth = true;
        }
    }
    if (!hasDepth && impl_->useLumaFallback) {
        for (int y = 0; y < height; ++y) {
            float* row = depth.ptr<float>(y);
            for (int x = 0; x < width; ++x) {
                const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4u;
                row[x] = std::clamp(source[offset] * 0.2126f +
                    source[offset + 1] * 0.7152f + source[offset + 2] * 0.0722f,
                    0.0f, 1.0f);
            }
        }
        hasDepth = true;
    }
    if (!hasDepth) { dst = src; return; }

    cv::Mat input(height, width, CV_32FC4, const_cast<float*>(source));
    std::vector<cv::Mat> inputChannels; cv::split(input, inputChannels);
    constexpr int levelCount = 4;
    std::array<cv::Mat, levelCount + 1> levels;
    levels[0] = input.clone();
    for (int level = 1; level <= levelCount; ++level) {
        const float radius = impl_->maxRadius * level / levelCount;
        const int kernelSize = std::clamp(static_cast<int>(std::round(radius * 2.0f)) | 1,
                                          3, 129);
        const cv::Mat psf = loadAndNormalizePsf(
            QString(), kernelSize, impl_->apertureShape,
            impl_->apertureRotation, 0.25f);
        std::vector<cv::Mat> blurredChannels(4);
        for (int channel = 0; channel < 3; ++channel) {
            cv::Mat boosted = inputChannels[channel].clone();
            if (impl_->highlightBoost > 0.0f) {
                cv::Mat highlights = inputChannels[channel] - 0.7f;
                cv::max(highlights, 0.0f, highlights);
                boosted += highlights * impl_->highlightBoost;
            }
            cv::filter2D(boosted, blurredChannels[channel], -1, psf,
                         cv::Point(-1, -1), 0.0, cv::BORDER_REFLECT_101);
        }
        blurredChannels[3] = inputChannels[3];
        cv::merge(blurredChannels, levels[level]);
    }

    auto result = image.DeepCopy(); float* output = result.rgba32fData();
    for (int y = 0; y < height; ++y) {
        const float* depthRow = depth.ptr<float>(y);
        std::array<const cv::Vec4f*, levelCount + 1> rows;
        for (int level = 0; level <= levelCount; ++level) rows[level] = levels[level].ptr<cv::Vec4f>(y);
        for (int x = 0; x < width; ++x) {
            const float signedDistance = depthRow[x] - impl_->focusDistance;
            const float outsideFocus = std::max(0.0f, std::abs(signedDistance) - impl_->focusRange);
            const float sideAmount = signedDistance < 0.0f ? impl_->foregroundBlur : impl_->backgroundBlur;
            const float blur = std::clamp(outsideFocus /
                std::max(0.0001f, 1.0f - impl_->focusRange) * sideAmount, 0.0f, 1.0f);
            const float levelPosition = blur * levelCount;
            const int lower = std::clamp(static_cast<int>(std::floor(levelPosition)), 0, levelCount);
            const int upper = std::min(lower + 1, levelCount);
            const float t = levelPosition - lower;
            const cv::Vec4f pixel = rows[lower][x] * (1.0f - t) + rows[upper][x] * t;
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4u;
            output[offset] = pixel[0]; output[offset + 1] = pixel[1];
            output[offset + 2] = pixel[2]; output[offset + 3] = source[offset + 3];
        }
    }
    result.setColorDescriptor(image.colorDescriptor()); dst = ImageF32x4RGBAWithCache(result);
}

std::vector<AbstractProperty> DepthBokehEffect::getProperties() const {
    std::vector<AbstractProperty> properties;
    auto add = [&](const char* name, const char* label, float value, float lo, float hi) {
        auto& p = properties.emplace_back(); p.setName(name); p.setDisplayLabel(label);
        p.setType(PropertyType::Float); p.setValue(value); p.setDefaultValue(value);
        p.setHardRange(lo, hi); p.setAnimatable(true);
    };
    add("focusDistance", "Focus Distance", impl_->focusDistance, 0.0f, 1.0f);
    add("focusRange", "Focus Range", impl_->focusRange, 0.0f, 1.0f);
    add("foregroundBlur", "Foreground Blur", impl_->foregroundBlur, 0.0f, 2.0f);
    add("backgroundBlur", "Background Blur", impl_->backgroundBlur, 0.0f, 2.0f);
    add("maxRadius", "Maximum Radius", impl_->maxRadius, 1.0f, 64.0f);
    auto& shape = properties.emplace_back(); shape.setName("apertureShape");
    shape.setDisplayLabel("Aperture Shape"); shape.setType(PropertyType::Integer);
    shape.setValue(impl_->apertureShape); shape.setDefaultValue(2); shape.setHardRange(0, 3);
    add("apertureRotation", "Aperture Rotation", impl_->apertureRotation, -180.0f, 180.0f);
    add("highlightBoost", "Highlight Boost", impl_->highlightBoost, 0.0f, 4.0f);
    auto& input = properties.emplace_back(); input.setName("depthInput");
    input.setDisplayLabel("Depth Input"); input.setType(PropertyType::String);
    input.setValue(impl_->depthInput); input.setDefaultValue(QStringLiteral("depth"));
    auto& fallback = properties.emplace_back(); fallback.setName("useLumaFallback");
    fallback.setDisplayLabel("Use Luma Fallback"); fallback.setType(PropertyType::Boolean);
    fallback.setValue(impl_->useLumaFallback); fallback.setDefaultValue(true);
    return properties;
}

void DepthBokehEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString(); const float raw = value.toFloat();
    const float n = std::isfinite(raw) ? raw : 0.0f;
    if (key == "focusDistance") impl_->focusDistance = std::clamp(n, 0.0f, 1.0f);
    else if (key == "focusRange") impl_->focusRange = std::clamp(n, 0.0f, 1.0f);
    else if (key == "foregroundBlur") impl_->foregroundBlur = std::clamp(n, 0.0f, 2.0f);
    else if (key == "backgroundBlur") impl_->backgroundBlur = std::clamp(n, 0.0f, 2.0f);
    else if (key == "maxRadius") impl_->maxRadius = std::clamp(n, 1.0f, 64.0f);
    else if (key == "apertureShape") impl_->apertureShape = std::clamp(value.toInt(), 0, 3);
    else if (key == "apertureRotation") impl_->apertureRotation = std::clamp(n, -180.0f, 180.0f);
    else if (key == "highlightBoost") impl_->highlightBoost = std::clamp(n, 0.0f, 4.0f);
    else if (key == "depthInput") impl_->depthInput = value.toString();
    else if (key == "useLumaFallback") impl_->useLumaFallback = value.toBool();
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint DepthBokehEffect::roiHint() const {
    return EffectROIHint{.kind = EffectROIHintKind::Blur,
                         .expansionPixels = impl_->maxRadius * 2.0f,
                         .requiresFullFrame = true};
}

}
