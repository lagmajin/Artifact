module;
#include <QList>
#include <QVariant>
#include <cmath>
#include <opencv2/core/mat.hpp>

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
module Artifact.Effect.Wave;




import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;

namespace Artifact {

void WaveEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const ImageF32x4_RGBA& srcImage = src.image();
    cv::Mat srcMat = srcImage.toCVMat();
    
    int height = srcMat.rows;
    int width = srcMat.cols;
    cv::Mat dstMat(height, width, CV_32FC4);
    
    // 波の計算
    float amp = amplitude_;
    float freq = frequency_;
    float phase = phase_;
    int wType = waveType_;
    int orient = orientation_;
    
    // ピクセルごとの処理
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float offset = 0.0f;
            
            if (orient == 0) { // Horizontal
                if (wType == 0) { // Sine
                    offset = amp * std::sin(2.0f * CV_PI * freq * x + phase);
                } else { // Cosine
                    offset = amp * std::cos(2.0f * CV_PI * freq * x + phase);
                }
            } else { // Vertical
                if (wType == 0) { // Sine
                    offset = amp * std::sin(2.0f * CV_PI * freq * y + phase);
                } else { // Cosine
                    offset = amp * std::cos(2.0f * CV_PI * freq * y + phase);
                }
            }
            
            // シフト後の座標を計算
            int srcX, srcY;
            if (orient == 0) {
                srcX = x + static_cast<int>(offset);
                srcY = y;
            } else {
                srcX = x;
                srcY = y + static_cast<int>(offset);
            }
            
            // 境界チェック
            srcX = std::max(0, std::min(width - 1, srcX));
            srcY = std::max(0, std::min(height - 1, srcY));
            
            // ピクセルをコピー
            cv::Vec4f pixel = srcMat.at<cv::Vec4f>(srcY, srcX);
            dstMat.at<cv::Vec4f>(y, x) = pixel;
        }
    }
    
    // 結果をdstに設定
    ImageF32x4_RGBA dstImage;
    dstImage.setFromCVMat(dstMat);
}

void WaveEffectGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    // 現在はCPUバックエンドにフォールバック
    // TODO: HLSLシェーダの実装
    WaveEffectCPUImpl cpuImpl;
    cpuImpl.setAmplitude(amplitude_);
    cpuImpl.setFrequency(frequency_);
    cpuImpl.setPhase(phase_);
    cpuImpl.setWaveType(waveType_);
    cpuImpl.setOrientation(orientation_);
    cpuImpl.applyCPU(src, dst);
}

class WaveEffect::Impl {
public:
    std::shared_ptr<WaveEffectCPUImpl> cpuImpl_;
    std::shared_ptr<WaveEffectGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = std::make_shared<WaveEffectCPUImpl>();
        gpuImpl_ = std::make_shared<WaveEffectGPUImpl>();
    }
};

WaveEffect::WaveEffect() : impl_(new Impl()) {
    setDisplayName(ArtifactCore::UniString("Wave"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(impl_->cpuImpl_);
    setGPUImpl(impl_->gpuImpl_);
}

WaveEffect::~WaveEffect() {
    delete impl_;
}

void WaveEffect::setAmplitude(float amp) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setAmplitude(amp);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setAmplitude(amp);
    }
}

float WaveEffect::amplitude() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->amplitude();
    }
    return 0.0f;
}

void WaveEffect::setFrequency(float freq) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setFrequency(freq);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setFrequency(freq);
    }
}

float WaveEffect::frequency() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->frequency();
    }
    return 0.0f;
}

void WaveEffect::setPhase(float phase) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setPhase(phase);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setPhase(phase);
    }
}

float WaveEffect::phase() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->phase();
    }
    return 0.0f;
}

void WaveEffect::setWaveType(int type) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setWaveType(type);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setWaveType(type);
    }
}

int WaveEffect::waveType() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->waveType();
    }
    return 0;
}

void WaveEffect::setOrientation(int ori) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setOrientation(ori);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setOrientation(ori);
    }
}

int WaveEffect::orientation() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->orientation();
    }
    return 0;
}

std::vector<ArtifactCore::AbstractProperty> WaveEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(5);

    auto& ampProp = props.emplace_back();
    ampProp.setName("amplitude");
    ampProp.setType(ArtifactCore::PropertyType::Float);
    ampProp.setDefaultValue(QVariant(static_cast<double>(amplitude())));
    ampProp.setValue(QVariant(static_cast<double>(amplitude())));

    auto& freqProp = props.emplace_back();
    freqProp.setName("frequency");
    freqProp.setType(ArtifactCore::PropertyType::Float);
    freqProp.setDefaultValue(QVariant(static_cast<double>(frequency())));
    freqProp.setValue(QVariant(static_cast<double>(frequency())));

    auto& phaseProp = props.emplace_back();
    phaseProp.setName("phase");
    phaseProp.setType(ArtifactCore::PropertyType::Float);
    phaseProp.setDefaultValue(QVariant(static_cast<double>(phase())));
    phaseProp.setValue(QVariant(static_cast<double>(phase())));

    auto& waveTypeProp = props.emplace_back();
    waveTypeProp.setName("waveType");
    waveTypeProp.setType(ArtifactCore::PropertyType::Integer);
    waveTypeProp.setDefaultValue(QVariant(waveType()));
    waveTypeProp.setValue(QVariant(waveType()));
    waveTypeProp.setTooltip(QStringLiteral("0=Sine, 1=Cosine"));

    auto& orientProp = props.emplace_back();
    orientProp.setName("orientation");
    orientProp.setType(ArtifactCore::PropertyType::Integer);
    orientProp.setDefaultValue(QVariant(orientation()));
    orientProp.setValue(QVariant(orientation()));
    orientProp.setTooltip(QStringLiteral("0=Horizontal, 1=Vertical"));

    return props;
}

void WaveEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    QString n = name.toQString();
    if (n == "amplitude") {
        setAmplitude(static_cast<float>(value.toDouble()));
    } else if (n == "frequency") {
        setFrequency(static_cast<float>(value.toDouble()));
    } else if (n == "phase") {
        setPhase(static_cast<float>(value.toDouble()));
    } else if (n == "waveType") {
        setWaveType(value.toInt());
    } else if (n == "orientation") {
        setOrientation(value.toInt());
    }
}

}
