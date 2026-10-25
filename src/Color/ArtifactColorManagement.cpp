module;

#include <cmath>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
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
#include <QString>
#include <QVector>
#include <QMatrix4x4>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>
#include <wobjectimpl.h>

module Artifact.Color.Management;

import Color.ColorSpace;
import Color.GamutConversion;





namespace Artifact
{

namespace {
ArtifactCore::Gamut gamutFor(ColorSpace space)
{
    switch (space) {
    case ColorSpace::Rec709: return ArtifactCore::Gamut::Rec709;
    case ColorSpace::Rec2020: return ArtifactCore::Gamut::Rec2020;
    case ColorSpace::P3: return ArtifactCore::Gamut::DCI_P3;
    case ColorSpace::ACES_AP0: return ArtifactCore::Gamut::ACES_AP0;
    case ColorSpace::ACES_AP1: return ArtifactCore::Gamut::ACES_AP1;
    case ColorSpace::Linear:
    case ColorSpace::sRGB:
    default: return ArtifactCore::Gamut::sRGB;
    }
}
}
W_OBJECT_IMPL(ColorManager)

// ==================== ColorSettings::Impl ====================

class ColorSettings::Impl
{
public:
    ColorSpace workingSpace_ = ColorSpace::Linear;
    ColorSpace sourceSpace_ = ColorSpace::sRGB;
    ColorSpace outputSpace_ = ColorSpace::sRGB;
    GammaFunction gamma_ = GammaFunction::sRGB;
    HDRMode hdrMode_ = HDRMode::SDR;
    float maxNits_ = 1000.0f;   // SDR典型值
    float minNits_ = 0.0f;
    int bitDepth_ = 8;
};

// ==================== ColorSettings ====================

ColorSettings::ColorSettings()
    : impl_(new Impl())
{
}

ColorSettings::~ColorSettings()
{
    delete impl_;
}

void ColorSettings::setWorkingColorSpace(ColorSpace space)
{
    impl_->workingSpace_ = space;
}

ColorSpace ColorSettings::workingColorSpace() const
{
    return impl_->workingSpace_;
}

void ColorSettings::setSourceColorSpace(ColorSpace space)
{
    impl_->sourceSpace_ = space;
}

ColorSpace ColorSettings::sourceColorSpace() const
{
    return impl_->sourceSpace_;
}

void ColorSettings::setOutputColorSpace(ColorSpace space)
{
    impl_->outputSpace_ = space;
}

ColorSpace ColorSettings::outputColorSpace() const
{
    return impl_->outputSpace_;
}

void ColorSettings::setGammaFunction(GammaFunction gamma)
{
    impl_->gamma_ = gamma;
}

GammaFunction ColorSettings::gammaFunction() const
{
    return impl_->gamma_;
}

void ColorSettings::setHDRMode(HDRMode mode)
{
    impl_->hdrMode_ = mode;
}

HDRMode ColorSettings::hdrMode() const
{
    return impl_->hdrMode_;
}

void ColorSettings::setMaxNits(float nits)
{
    impl_->maxNits_ = std::isfinite(nits) ? std::clamp(nits, 0.001f, 100000.0f)
                                         : 1000.0f;
    if (impl_->minNits_ > impl_->maxNits_) {
        impl_->minNits_ = impl_->maxNits_;
    }
}

float ColorSettings::maxNits() const
{
    return impl_->maxNits_;
}

void ColorSettings::setMinNits(float nits)
{
    impl_->minNits_ = std::isfinite(nits) ? std::clamp(nits, 0.0f, impl_->maxNits_)
                                         : 0.001f;
}

float ColorSettings::minNits() const
{
    return impl_->minNits_;
}

void ColorSettings::setBitDepth(int bits)
{
    impl_->bitDepth_ = std::clamp(bits, 8, 32);
}

int ColorSettings::bitDepth() const
{
    return impl_->bitDepth_;
}

// ==================== LUTData::Impl ====================

class LUTData::Impl
{
public:
    LUTType type_ = LUTType::Cube3D;
    int size_ = 33;
    QVector<float> data3D_;  // r,g,b 順でsize^3 * 3個
    QVector<float> data1D_;  // r,g,b 順でsize * 3個
    bool is1D_ = false;
};

// ==================== LUTData ====================

LUTData::LUTData()
    : impl_(new Impl())
{
}

LUTData::~LUTData()
{
    delete impl_;
}

bool LUTData::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    QString line;
    int size = 0;
    bool readingSize = false;
    bool validHeader = false;
    int readCount = 0;

    while (!in.atEnd()) {
        line = in.readLine().trimmed();

        if (line.startsWith("LUT_3D_SIZE")) {
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                size = parts[1].toInt();
                if (size < 2 || size > 256) return false;
                impl_->size_ = size;
                impl_->is1D_ = false;
                impl_->data3D_.resize(size * size * size * 3);
                readingSize = true;
                validHeader = true;
            }
        } else if (line.startsWith("LUT_1D_SIZE")) {
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                size = parts[1].toInt();
                if (size < 2 || size > 65536) return false;
                impl_->size_ = size;
                impl_->is1D_ = true;
                impl_->data1D_.resize(size * 3);
                readingSize = true;
                validHeader = true;
            }
        } else if (line.isEmpty() || line.startsWith("#")) {
            continue;
        } else {
            // 数値行
            QStringList values = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (values.size() != 3) {
                return false;
            }
            if (values.size() == 3) {
                bool okR = false, okG = false, okB = false;
                float r = values[0].toFloat(&okR);
                float g = values[1].toFloat(&okG);
                float b = values[2].toFloat(&okB);
                if (!readingSize || !okR || !okG || !okB ||
                    !std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b)) {
                    return false;
                }

                if (!impl_->is1D_ && readCount < impl_->data3D_.size()) {
                    impl_->data3D_[readCount++] = r;
                    impl_->data3D_[readCount++] = g;
                    impl_->data3D_[readCount++] = b;
                } else if (impl_->is1D_ && readCount < impl_->data1D_.size()) {
                    impl_->data1D_[readCount++] = r;
                    impl_->data1D_[readCount++] = g;
                    impl_->data1D_[readCount++] = b;
                }
            }
        }
    }

    file.close();
    const int expected = impl_->is1D_ ? impl_->data1D_.size() : impl_->data3D_.size();
    return validHeader && readCount == expected;
}

bool LUTData::saveToFile(const QString& filePath) const
{
    const int expected = impl_->is1D_
        ? impl_->size_ * 3
        : impl_->size_ * impl_->size_ * impl_->size_ * 3;
    const auto& values = impl_->is1D_ ? impl_->data1D_ : impl_->data3D_;
    if (impl_->size_ < 2 || values.size() != expected) {
        return false;
    }
    for (const float value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);

    if (impl_->is1D_) {
        out << "LUT_1D_SIZE " << impl_->size_ << "\n";
        out << "\n";

        for (int i = 0; i < impl_->size_; i++) {
            out << impl_->data1D_[i * 3] << " "
                << impl_->data1D_[i * 3 + 1] << " "
                << impl_->data1D_[i * 3 + 2] << "\n";
        }
    } else {
        out << "LUT_3D_SIZE " << impl_->size_ << "\n";
        out << "\n";

        for (int b = 0; b < impl_->size_; b++) {
            for (int g = 0; g < impl_->size_; g++) {
                for (int r = 0; r < impl_->size_; r++) {
                    int idx = (b * impl_->size_ * impl_->size_ + g * impl_->size_ + r) * 3;
                    out << impl_->data3D_[idx] << " "
                        << impl_->data3D_[idx + 1] << " "
                        << impl_->data3D_[idx + 2] << "\n";
                }
            }
        }
    }

    file.close();
    return true;
}

LUTType LUTData::type() const
{
    return impl_->type_;
}

int LUTData::size() const
{
    return impl_->size_;
}

QVector3D LUTData::apply(float r, float g, float b) const
{
    const int requiredSize = impl_->is1D_
        ? impl_->size_ * 3
        : impl_->size_ * impl_->size_ * impl_->size_ * 3;
    const int actualSize = impl_->is1D_ ? impl_->data1D_.size() : impl_->data3D_.size();
    if (impl_->size_ < 2 || actualSize < requiredSize) {
        return QVector3D(r, g, b);
    }
    const auto safeChannel = [](float value) {
        return std::clamp(std::isfinite(value) ? value : 0.0f, 0.0f, 1.0f);
    };
    r = safeChannel(r);
    g = safeChannel(g);
    b = safeChannel(b);

    if (impl_->is1D_) {
        // 1D LUT - 简单補間
        const auto sample = [this](float value, int channel) {
            const float index = value * (impl_->size_ - 1);
            const int index0 = static_cast<int>(index);
            const int index1 = std::min(index0 + 1, impl_->size_ - 1);
            const float t = index - index0;
            return impl_->data1D_[index0 * 3 + channel] * (1.0f - t) +
                   impl_->data1D_[index1 * 3 + channel] * t;
        };
        return QVector3D(sample(r, 0), sample(g, 1), sample(b, 2));
    } else {
        // 3D LUT - триリニア補間
        float rf = r * (impl_->size_ - 1);
        float gf = g * (impl_->size_ - 1);
        float bf = b * (impl_->size_ - 1);

        int r0 = static_cast<int>(rf);
        int g0 = static_cast<int>(gf);
        int b0 = static_cast<int>(bf);

        int r1 = std::min(r0 + 1, impl_->size_ - 1);
        int g1 = std::min(g0 + 1, impl_->size_ - 1);
        int b1 = std::min(b0 + 1, impl_->size_ - 1);

        float rd = rf - r0;
        float gd = gf - g0;
        float bd = bf - b0;

        // 8 point trilinear interpolation
        auto getValue = [this](int ri, int gi, int bi) -> QVector3D {
            int idx = (bi * impl_->size_ * impl_->size_ + gi * impl_->size_ + ri) * 3;
            return QVector3D(
                impl_->data3D_[idx],
                impl_->data3D_[idx + 1],
                impl_->data3D_[idx + 2]
            );
        };

        QVector3D c000 = getValue(r0, g0, b0);
        QVector3D c100 = getValue(r1, g0, b0);
        QVector3D c010 = getValue(r0, g1, b0);
        QVector3D c110 = getValue(r1, g1, b0);
        QVector3D c001 = getValue(r0, g0, b1);
        QVector3D c101 = getValue(r1, g0, b1);
        QVector3D c011 = getValue(r0, g1, b1);
        QVector3D c111 = getValue(r1, g1, b1);

        auto lerp = [](const QVector3D& a, const QVector3D& b, float t) {
            return a * (1 - t) + b * t;
        };

        QVector3D result =
            lerp(lerp(lerp(c000, c100, rd), lerp(c010, c110, rd), gd),
                 lerp(lerp(c001, c101, rd), lerp(c011, c111, rd), gd),
                 bd);

        return result;
    }
}

const float* LUTData::data3D() const
{
    return impl_->data3D_.constData();
}

int LUTData::dataSize() const
{
    return impl_->is1D_ ? impl_->data1D_.size() : impl_->data3D_.size();
}

const float* LUTData::data1D() const
{
    return impl_->data1D_.constData();
}

bool LUTData::is1D() const
{
    return impl_->is1D_;
}

// ==================== ColorLUTEffect::Impl ====================

class ColorLUTEffect::Impl
{
public:
    ArtifactCore::SharedPtr<LUTData> lut_;
    float intensity_ = 1.0f;
    GammaFunction inputGamma_ = GammaFunction::Linear;
    GammaFunction outputGamma_ = GammaFunction::sRGB;
};

// ==================== ColorLUTEffect ====================

ColorLUTEffect::ColorLUTEffect()
    : impl_(new Impl())
{
}

ColorLUTEffect::~ColorLUTEffect()
{
    delete impl_;
}

void ColorLUTEffect::setLUT(ArtifactCore::SharedPtr<LUTData> lut)
{
    impl_->lut_ = lut;
}

ArtifactCore::SharedPtr<LUTData> ColorLUTEffect::lut() const
{
    return impl_->lut_;
}

void ColorLUTEffect::setIntensity(float intensity)
{
    impl_->intensity_ = std::isfinite(intensity)
        ? std::clamp(intensity, 0.0f, 1.0f) : 1.0f;
}

float ColorLUTEffect::intensity() const
{
    return impl_->intensity_;
}

void ColorLUTEffect::setInputGamma(GammaFunction gamma)
{
    impl_->inputGamma_ = gamma;
}

GammaFunction ColorLUTEffect::inputGamma() const
{
    return impl_->inputGamma_;
}

void ColorLUTEffect::setOutputGamma(GammaFunction gamma)
{
    impl_->outputGamma_ = gamma;
}

GammaFunction ColorLUTEffect::outputGamma() const
{
    return impl_->outputGamma_;
}

QVector3D ColorLUTEffect::transform(float r, float g, float b) const
{
    const auto finite = [](float value) { return std::isfinite(value) ? value : 0.0f; };
    const QVector3D source(finite(r), finite(g), finite(b));
    if (!impl_->lut_) {
        return source;
    }

    const auto decode = [this](float value) {
        return ArtifactCore::ColorSpaceConverter::removeGamma(
            value, static_cast<ArtifactCore::GammaFunction>(impl_->inputGamma_));
    };
    const auto encode = [this](float value) {
        return ArtifactCore::ColorSpaceConverter::applyGamma(
            value, static_cast<ArtifactCore::GammaFunction>(impl_->outputGamma_));
    };
    const QVector3D linear(decode(source.x()), decode(source.y()), decode(source.z()));
    const QVector3D lutResult = impl_->lut_->apply(linear.x(), linear.y(), linear.z());
    const QVector3D result(encode(lutResult.x()), encode(lutResult.y()), encode(lutResult.z()));

    // 強度ブレンド
    if (impl_->intensity_ < 1.0f) {
        return source * (1.0f - impl_->intensity_) + result * impl_->intensity_;
    }

    return result;
}

// ==================== ColorManager::Impl ====================

class ColorManager::Impl
{
public:
    std::unique_ptr<ColorSettings> settings_;
    float maxCll_ = 1000.0f;   // Content Light Level
    float maxFall_ = 500.0f;    // Frame Average Light Level
    float avgBrightness_ = 200.0f;
    ColorSpace workingSpace_ = ColorSpace::Linear;
    QString displayProfile_;
};

// ==================== ColorManager ====================

ColorManager& ColorManager::instance()
{
    static ColorManager instance;
    return instance;
}

ColorManager::ColorManager()
    : impl_(new Impl())
{
    impl_->settings_ = std::make_unique<ColorSettings>();
}

ColorManager::~ColorManager()
{
    delete impl_;
}

ColorSettings* ColorManager::settings()
{
    return impl_->settings_.get();
}

const ColorSettings* ColorManager::settings() const
{
    return impl_->settings_.get();
}

QMatrix4x4 ColorManager::getConversionMatrix(ColorSpace from, ColorSpace to) const
{
    const auto conversion = ArtifactCore::ColorGamutConversion::getConversionMatrix(
        gamutFor(from), gamutFor(to));
    QMatrix4x4 matrix;
    matrix.setToIdentity();
    matrix.setRow(0, QVector4D(conversion[0][0], conversion[0][1], conversion[0][2], 0.0f));
    matrix.setRow(1, QVector4D(conversion[1][0], conversion[1][1], conversion[1][2], 0.0f));
    matrix.setRow(2, QVector4D(conversion[2][0], conversion[2][1], conversion[2][2], 0.0f));
    matrix.setRow(3, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
    return matrix;
}

float ColorManager::applyGamma(float value, GammaFunction gamma) const
{
    return ArtifactCore::ColorSpaceConverter::applyGamma(
        value, static_cast<ArtifactCore::GammaFunction>(gamma));
}

float ColorManager::removeGamma(float value, GammaFunction gamma) const
{
    return ArtifactCore::ColorSpaceConverter::removeGamma(
        value, static_cast<ArtifactCore::GammaFunction>(gamma));
}

void ColorManager::setHDRMetadata(float maxCll, float maxFall, float avgBrightness)
{
    impl_->maxCll_ = std::isfinite(maxCll) ? std::max(0.0f, maxCll) : 1000.0f;
    impl_->maxFall_ = std::isfinite(maxFall) ? std::max(0.0f, maxFall) : 500.0f;
    impl_->avgBrightness_ = std::isfinite(avgBrightness)
        ? std::max(0.0f, avgBrightness) : 200.0f;
}

float ColorManager::maxContentLightLevel() const
{
    return impl_->maxCll_;
}

float ColorManager::maxFrameAverageLightLevel() const
{
    return impl_->maxFall_;
}

float ColorManager::averageBrightness() const
{
    return impl_->avgBrightness_;
}

void ColorManager::setWorkingSpace(ColorSpace space)
{
    impl_->workingSpace_ = space;
    Q_EMIT colorSpaceChanged(space);
}

ColorSpace ColorManager::workingSpace() const
{
    return impl_->workingSpace_;
}

void ColorManager::setDisplayProfile(const QString& profile)
{
    const QString normalized = profile.trimmed();
    if (impl_->displayProfile_ == normalized) return;
    impl_->displayProfile_ = normalized;
    Q_EMIT colorSpaceChanged(impl_->workingSpace_);
}

QString ColorManager::displayProfile() const
{
    return impl_->displayProfile_;
}

} // namespace Artifact
