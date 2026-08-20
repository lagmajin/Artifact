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
    float radialQuadratic_ = 0.0f;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float tangentialX_ = 0.0f;
    float tangentialY_ = 0.0f;
    bool invertDistortion_ = false;
    float zoom_ = 1.0f;

public:
    LensDistortionEffectCPUImpl() = default;

    void setDistortion(float v) { distortion_ = std::isfinite(v) ? std::clamp(v, -100.0f, 100.0f) : 0.0f; }
    float distortion() const { return distortion_; }
    void setRadialQuadratic(float v) { radialQuadratic_ = std::isfinite(v) ? std::clamp(v, -100.0f, 100.0f) : 0.0f; }
    float radialQuadratic() const { return radialQuadratic_; }

    void setCenterX(float cx) { centerX_ = std::isfinite(cx) ? std::clamp(cx, 0.0f, 1.0f) : 0.5f; }
    float centerX() const { return centerX_; }

    void setCenterY(float cy) { centerY_ = std::isfinite(cy) ? std::clamp(cy, 0.0f, 1.0f) : 0.5f; }
    float centerY() const { return centerY_; }

    void setTangentialX(float v) { tangentialX_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f; }
    float tangentialX() const { return tangentialX_; }
    void setTangentialY(float v) { tangentialY_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f; }
    float tangentialY() const { return tangentialY_; }

    void setInvertDistortion(bool v) { invertDistortion_ = v; }
    bool invertDistortion() const { return invertDistortion_; }

    void setZoom(float v) { zoom_ = std::isfinite(v) ? std::max(0.01f, v) : 1.0f; }
    float zoom() const { return zoom_; }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class LensDistortionEffectGPUImpl : public ArtifactEffectImplBase {
private:
    float distortion_ = 0.0f;
    float radialQuadratic_ = 0.0f;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float tangentialX_ = 0.0f;
    float tangentialY_ = 0.0f;
    bool invertDistortion_ = false;
    float zoom_ = 1.0f;

public:
    LensDistortionEffectGPUImpl() = default;

    void setDistortion(float v) { distortion_ = std::isfinite(v) ? std::clamp(v, -100.0f, 100.0f) : 0.0f; }
    float distortion() const { return distortion_; }
    void setRadialQuadratic(float v) { radialQuadratic_ = std::isfinite(v) ? std::clamp(v, -100.0f, 100.0f) : 0.0f; }
    float radialQuadratic() const { return radialQuadratic_; }

    void setCenterX(float cx) { centerX_ = std::isfinite(cx) ? std::clamp(cx, 0.0f, 1.0f) : 0.5f; }
    float centerX() const { return centerX_; }

    void setCenterY(float cy) { centerY_ = std::isfinite(cy) ? std::clamp(cy, 0.0f, 1.0f) : 0.5f; }
    float centerY() const { return centerY_; }

    void setTangentialX(float v) { tangentialX_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f; }
    float tangentialX() const { return tangentialX_; }
    void setTangentialY(float v) { tangentialY_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f; }
    float tangentialY() const { return tangentialY_; }

    void setInvertDistortion(bool v) { invertDistortion_ = v; }
    bool invertDistortion() const { return invertDistortion_; }

    void setZoom(float v) { zoom_ = std::isfinite(v) ? std::max(0.01f, v) : 1.0f; }
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
    void setRadialQuadratic(float v);
    float radialQuadratic() const;

    void setCenterX(float cx);
    float centerX() const;

    void setCenterY(float cy);
    float centerY() const;

    void setTangentialX(float v);
    float tangentialX() const;
    void setTangentialY(float v);
    float tangentialY() const;

    void setInvertDistortion(bool v);
    bool invertDistortion() const;

    void setZoom(float v);
    float zoom() const;

    bool supportsGPU() const override { return true; }
};

};
