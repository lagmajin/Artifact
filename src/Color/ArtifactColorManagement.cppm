module;

#include <QString>
#include <QVector>
#include <QMatrix4x4>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <cmath>

module Artifact.Color.Management;

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




namespace Artifact
{

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
    impl_->maxNits_ = nits;
}

float ColorSettings::maxNits() const
{
    return impl_->maxNits_;
}

void ColorSettings::setMinNits(float nits)
{
    impl_->minNits_ = nits;
}

float ColorSettings::minNits() const
{
    return impl_->minNits_;
}

void ColorSettings::setBitDepth(int bits)
{
    impl_->bitDepth_ = bits;
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
    int readCount = 0;

    while (!in.atEnd()) {
        line = in.readLine().trimmed();

        if (line.startsWith("LUT_3D_SIZE")) {
            QStringList parts = line.split(" ");
            if (parts.size() >= 2) {
                size = parts[1].toInt();
                impl_->size_ = size;
                impl_->is1D_ = false;
                impl_->data3D_.resize(size * size * size * 3);
                readingSize = true;
            }
        } else if (line.startsWith("LUT_1D_SIZE")) {
            QStringList parts = line.split(" ");
            if (parts.size() >= 2) {
                size = parts[1].toInt();
                impl_->size_ = size;
                impl_->is1D_ = true;
                impl_->data1D_.resize(size * 3);
                readingSize = true;
            }
        } else if (line.isEmpty() || line.startsWith("#")) {
            continue;
        } else {
            // 数値行
            QStringList values = line.split(QRegExp("\\s+"), QString::SkipEmptyParts);
            if (values.size() >= 3) {
                float r = values[0].toFloat();
                float g = values[1].toFloat();
                float b = values[2].toFloat();

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
    return true;
}

bool LUTData::saveToFile(const QString& filePath) const
{
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
    if (impl_->is1D_) {
        // 1D LUT - 简单補間
        float idxf = r * (impl_->size_ - 1);
        int idx0 = static_cast<int>(idxf);
        int idx1 = std::min(idx0 + 1, impl_->size_ - 1);
        float t = idxf - idx0;

        int i0 = idx0 * 3;
        int i1 = idx1 * 3;

        return QVector3D(
            impl_->data1D_[i0] * (1 - t) + impl_->data1D_[i1] * t,
            impl_->data1D_[i0 + 1] * (1 - t) + impl_->data1D_[i1 + 1] * t,
            impl_->data1D_[i0 + 2] * (1 - t) + impl_->data1D_[i1 + 2] * t
        );
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
    return impl_->data3D_.size();
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
    std::shared_ptr<LUTData> lut_;
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

void ColorLUTEffect::setLUT(std::shared_ptr<LUTData> lut)
{
    impl_->lut_ = lut;
}

std::shared_ptr<LUTData> ColorLUTEffect::lut() const
{
    return impl_->lut_;
}

void ColorLUTEffect::setIntensity(float intensity)
{
    impl_->intensity_ = std::max(0.0f, std::min(1.0f, intensity));
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
    if (!impl_->lut_) {
        return QVector3D(r, g, b);
    }

    // 入力ガンマ除去
    // （簡略実装 - 実際はColorManagerを使用）
    QVector3D result = impl_->lut_->apply(r, g, b);

    // 強度ブレンド
    if (impl_->intensity_ < 1.0f) {
        result = QVector3D(r, g, b) * (1 - impl_->intensity_) + result * impl_->intensity_;
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
    // 简化实现 - 実際は複雑な行列
    QMatrix4x4 matrix;

    if (from == to) {
        return matrix;  // 単位行列
    }

    // sRGBからLinearへの変換（简单版）
    if (from == ColorSpace::sRGB && to == ColorSpace::Linear) {
        matrix.setRow(0, QVector4D(0.2126f, 0.7152f, 0.0722f, 0.0f));
        matrix.setRow(1, QVector4D(0.2126f, 0.7152f, 0.0722f, 0.0f));
        matrix.setRow(2, QVector4D(0.2126f, 0.7152f, 0.0722f, 0.0f));
        matrix.setRow(3, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
    }

    return matrix;
}

float ColorManager::applyGamma(float value, GammaFunction gamma) const
{
    switch (gamma) {
    case GammaFunction::Linear:
        return value;
    case GammaFunction::sRGB:
        return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
    case GammaFunction::Gamma22:
        return std::pow(value, 1.0f / 2.2f);
    default:
        return value;
    }
}

float ColorManager::removeGamma(float value, GammaFunction gamma) const
{
    switch (gamma) {
    case GammaFunction::Linear:
        return value;
    case GammaFunction::sRGB:
        return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
    case GammaFunction::Gamma22:
        return std::pow(value, 2.2f);
    default:
        return value;
    }
}

void ColorManager::setHDRMetadata(float maxCll, float maxFall, float avgBrightness)
{
    impl_->maxCll_ = maxCll;
    impl_->maxFall_ = maxFall;
    impl_->avgBrightness_ = avgBrightness;
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

} // namespace Artifact
