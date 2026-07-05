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
export module Artifact.Effect.Liquify;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;

export namespace Artifact {

enum class LiquifyBrushType {
    Push = 0,
    Pinch = 1,
    Bloat = 2,
    Twirl = 3,
    Turbulence = 4,
    Pucker = 5
};

class LiquifyEffectCPUImpl : public ArtifactEffectImplBase {
private:
    LiquifyBrushType brushType_ = LiquifyBrushType::Push;
    float amount_ = 50.0f;
    float radius_ = 0.3f;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float angle_ = 45.0f;
    int turbulenceSeed_ = 123;
    int meshDensity_ = 32;

public:
    LiquifyEffectCPUImpl() = default;

    void setBrushType(LiquifyBrushType t) { brushType_ = t; }
    LiquifyBrushType brushType() const { return brushType_; }

    void setAmount(float v) { amount_ = v; }
    float amount() const { return amount_; }

    void setRadius(float v) { radius_ = v; }
    float radius() const { return radius_; }

    void setCenterX(float v) { centerX_ = v; }
    float centerX() const { return centerX_; }

    void setCenterY(float v) { centerY_ = v; }
    float centerY() const { return centerY_; }

    void setAngle(float v) { angle_ = v; }
    float angle() const { return angle_; }

    void setTurbulenceSeed(int s) { turbulenceSeed_ = s; }
    int turbulenceSeed() const { return turbulenceSeed_; }

    void setMeshDensity(int d) { meshDensity_ = d; }
    int meshDensity() const { return meshDensity_; }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class LiquifyEffectGPUImpl : public ArtifactEffectImplBase {
private:
    LiquifyBrushType brushType_ = LiquifyBrushType::Push;
    float amount_ = 50.0f;
    float radius_ = 0.3f;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float angle_ = 45.0f;
    int turbulenceSeed_ = 123;
    int meshDensity_ = 32;

public:
    LiquifyEffectGPUImpl() = default;

    void setBrushType(LiquifyBrushType t) { brushType_ = t; }
    LiquifyBrushType brushType() const { return brushType_; }

    void setAmount(float v) { amount_ = v; }
    float amount() const { return amount_; }

    void setRadius(float v) { radius_ = v; }
    float radius() const { return radius_; }

    void setCenterX(float v) { centerX_ = v; }
    float centerX() const { return centerX_; }

    void setCenterY(float v) { centerY_ = v; }
    float centerY() const { return centerY_; }

    void setAngle(float v) { angle_ = v; }
    float angle() const { return angle_; }

    void setTurbulenceSeed(int s) { turbulenceSeed_ = s; }
    int turbulenceSeed() const { return turbulenceSeed_; }

    void setMeshDensity(int d) { meshDensity_ = d; }
    int meshDensity() const { return meshDensity_; }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class LiquifyEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
public:
    LiquifyEffect();
    ~LiquifyEffect();

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override;

    void setBrushType(LiquifyBrushType t);
    LiquifyBrushType brushType() const;

    void setAmount(float v);
    float amount() const;

    void setRadius(float v);
    float radius() const;

    void setCenterX(float v);
    float centerX() const;

    void setCenterY(float v);
    float centerY() const;

    void setAngle(float v);
    float angle() const;

    void setTurbulenceSeed(int s);
    int turbulenceSeed() const;

    void setMeshDensity(int d);
    int meshDensity() const;

    bool supportsGPU() const override {
        return true;
    }
};

}
