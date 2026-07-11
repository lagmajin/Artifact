module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.ApertureShapeBlur;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

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
        for (int x = 0; x < psf.cols; ++x) {
            const int dstX = (x - halfX + channel.cols) % channel.cols;
            const int dstY = (y - halfY + channel.rows) % channel.rows;
            paddedKernel.at<float>(dstY, dstX) = srcRow[x];
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
        for (int c = 0; c < 3; ++c) {
            outputChannels[c] = fftConvolve(channels[c], psf);
            if (highlightBoost > 0.0f) {
                cv::Mat highlights = channels[c] - 0.65f;
                cv::max(highlights, 0.0, highlights);
                outputChannels[c] += fftConvolve(highlights, psf) * highlightBoost;
            }
        }
        outputChannels[3] = channels[3];
        cv::Mat output;
        cv::merge(outputChannels, output);
        dst = src;
        dst.image().setFromCVMat(output);
    }
};

ApertureShapeBlurEffect::ApertureShapeBlurEffect() {
    setDisplayName(UniString("Aperture Shape Blur"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ApertureShapeBlurCPUImpl>());
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
    if (key == QStringLiteral("Radius")) radius_ = std::clamp(value.toFloat(), 0.0f, 256.0f);
    else if (key == QStringLiteral("Shape")) shape_ = std::clamp(value.toInt(), 0, 3);
    else if (key == QStringLiteral("Rotation")) rotation_ = value.toFloat();
    else if (key == QStringLiteral("Edge Brightness")) edgeBrightness_ = std::clamp(value.toFloat(), 0.0f, 3.0f);
    else if (key == QStringLiteral("Highlight Boost")) highlightBoost_ = std::clamp(value.toFloat(), 0.0f, 4.0f);
    else if (key == QStringLiteral("PSF Image Path")) psfImagePath_ = value.toString();
    syncImpl();
}

}
