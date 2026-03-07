module;

#include <QString>

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
module Artifact.Color.Settings;




import Color.LUT;

namespace Artifact
{

// ==================== ColorSettings::Impl ====================

class ColorSettings::Impl
{
public:
    WorkingColorSpace workingSpace_ = WorkingColorSpace::Linear;
    WorkingColorSpace sourceSpace_ = WorkingColorSpace::sRGB;
    WorkingColorSpace outputSpace_ = WorkingColorSpace::sRGB;
    WorkingGamma gamma_ = WorkingGamma::sRGB;
    WorkingHDRMode hdrMode_ = WorkingHDRMode::SDR;
    float maxNits_ = 1000.0f;
    int bitDepth_ = 8;
    ArtifactCore::LUT* lut_ = nullptr;
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

void ColorSettings::setWorkingSpace(WorkingColorSpace space)
{
    impl_->workingSpace_ = space;
}

WorkingColorSpace ColorSettings::workingSpace() const
{
    return impl_->workingSpace_;
}

void ColorSettings::setSourceSpace(WorkingColorSpace space)
{
    impl_->sourceSpace_ = space;
}

WorkingColorSpace ColorSettings::sourceSpace() const
{
    return impl_->sourceSpace_;
}

void ColorSettings::setOutputSpace(WorkingColorSpace space)
{
    impl_->outputSpace_ = space;
}

WorkingColorSpace ColorSettings::outputSpace() const
{
    return impl_->outputSpace_;
}

void ColorSettings::setGamma(WorkingGamma gamma)
{
    impl_->gamma_ = gamma;
}

WorkingGamma ColorSettings::gamma() const
{
    return impl_->gamma_;
}

void ColorSettings::setHDRMode(WorkingHDRMode mode)
{
    impl_->hdrMode_ = mode;
}

WorkingHDRMode ColorSettings::hdrMode() const
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

void ColorSettings::setBitDepth(int bits)
{
    impl_->bitDepth_ = bits;
}

int ColorSettings::bitDepth() const
{
    return impl_->bitDepth_;
}

void ColorSettings::setLUT(ArtifactCore::LUT* lut)
{
    impl_->lut_ = lut;
}

ArtifactCore::LUT* ColorSettings::lut() const
{
    return impl_->lut_;
}

// ==================== LUTColorEffect::Impl ====================

class LUTColorEffect::Impl
{
public:
    ArtifactCore::LUT* lut_ = nullptr;
    ArtifactCore::LUT lutStorage_;  // デフォルト用
    float intensity_ = 1.0f;
};

// ==================== LUTColorEffect ====================

LUTColorEffect::LUTColorEffect()
    : impl_(new Impl())
{
    // デフォルトでアイデンティティLUTを作成
    impl_->lutStorage_.createIdentity(33, ArtifactCore::LUTType::LUT3D);
    impl_->lut_ = &impl_->lutStorage_;
}

LUTColorEffect::~LUTColorEffect()
{
    delete impl_;
}

void LUTColorEffect::setLUT(ArtifactCore::LUT* lut)
{
    impl_->lut_ = lut;
}

ArtifactCore::LUT* LUTColorEffect::lut() const
{
    return impl_->lut_;
}

void LUTColorEffect::setLUTByName(const QString& name)
{
    // CoreのLUTManagerから取得する実装
    // auto* manager = ArtifactCore::LUTManager::get();
    // impl_->lut_ = manager->getLUT(name.toStdString().c_str());
}

void LUTColorEffect::setIntensity(float value)
{
    impl_->intensity_ = std::max(0.0f, std::min(1.0f, value));
}

float LUTColorEffect::intensity() const
{
    return impl_->intensity_;
}

void LUTColorEffect::apply(float& r, float& g, float& b) const
{
    if (!impl_->lut_ || !impl_->lut_->isLoaded()) {
        return;
    }

    // Core LUTを適用
    FloatRGBA color(r, g, b, 1.0f);
    FloatRGBA result = impl_->lut_->apply(color);

    // 強度ブレンド
    if (impl_->intensity_ < 1.0f) {
        r = r * (1.0f - impl_->intensity_) + result.r() * impl_->intensity_;
        g = g * (1.0f - impl_->intensity_) + result.g() * impl_->intensity_;
        b = b * (1.0f - impl_->intensity_) + result.b() * impl_->intensity_;
    } else {
        r = result.r();
        g = result.g();
        b = result.b();
    }
}

} // namespace Artifact
