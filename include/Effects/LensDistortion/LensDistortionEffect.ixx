module;
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Effect.LensDistortion;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;

export namespace Artifact {

class LensDistortionEffectCPUImpl : public ArtifactEffectImplBase {
private:
    float distortion_ = 0.0f;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    bool invertDistortion_ = false;
    float zoom_ = 1.0f;

public:
    LensDistortionEffectCPUImpl() = default;

    void setDistortion(float v) { distortion_ = v; }
    float distortion() const { return distortion_; }

    void setCenterX(float cx) { centerX_ = cx; }
    float centerX() const { return centerX_; }

    void setCenterY(float cy) { centerY_ = cy; }
    float centerY() const { return centerY_; }

    void setInvertDistortion(bool v) { invertDistortion_ = v; }
    bool invertDistortion() const { return invertDistortion_; }

    void setZoom(float v) { zoom_ = v; }
    float zoom() const { return zoom_; }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class LensDistortionEffectGPUImpl : public ArtifactEffectImplBase {
private:
    float distortion_ = 0.0f;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    bool invertDistortion_ = false;
    float zoom_ = 1.0f;

public:
    LensDistortionEffectGPUImpl() = default;

    void setDistortion(float v) { distortion_ = v; }
    float distortion() const { return distortion_; }

    void setCenterX(float cx) { centerX_ = cx; }
    float centerX() const { return centerX_; }

    void setCenterY(float cy) { centerY_ = cy; }
    float centerY() const { return centerY_; }

    void setInvertDistortion(bool v) { invertDistortion_ = v; }
    bool invertDistortion() const { return invertDistortion_; }

    void setZoom(float v) { zoom_ = v; }
    float zoom() const { return zoom_; }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class LensDistortionEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
public:
    LensDistortionEffect();
    ~LensDistortionEffect();

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override;

    void setDistortion(float v);
    float distortion() const;

    void setCenterX(float cx);
    float centerX() const;

    void setCenterY(float cy);
    float centerY() const;

    void setInvertDistortion(bool v);
    bool invertDistortion() const;

    void setZoom(float v);
    float zoom() const;

    bool supportsGPU() const override { return true; }
};

};
