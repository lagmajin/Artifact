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
export module Artifact.Effect.Spherize;




import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;

export namespace Artifact {

// Spherize Effect - creates a spherical distortion effect
class SpherizeEffectCPUImpl : public ArtifactEffectImplBase {
private:
    float amount_ = 50.0f;         // 歪み量（-100〜100）
    float radius_ = 0.5f;          // 効果の半径（画像サイズに対する比率）
    float centerX_ = 0.5f;         // 中心X座標（画像サイズに対する比率）
    float centerY_ = 0.5f;         // 中心Y座標（画像サイズに対する比率）

public:
    SpherizeEffectCPUImpl() = default;

    void setAmount(float amount) { amount_ = amount; }
    float amount() const { return amount_; }

    void setRadius(float radius) { radius_ = radius; }
    float radius() const { return radius_; }

    void setCenterX(float cx) { centerX_ = cx; }
    float centerX() const { return centerX_; }

    void setCenterY(float cy) { centerY_ = cy; }
    float centerY() const { return centerY_; }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class SpherizeEffectGPUImpl : public ArtifactEffectImplBase {
private:
    float amount_ = 50.0f;
    float radius_ = 0.5f;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;

public:
    SpherizeEffectGPUImpl() = default;

    void setAmount(float amount) { amount_ = amount; }
    float amount() const { return amount_; }

    void setRadius(float radius) { radius_ = radius; }
    float radius() const { return radius_; }

    void setCenterX(float cx) { centerX_ = cx; }
    float centerX() const { return centerX_; }

    void setCenterY(float cy) { centerY_ = cy; }
    float centerY() const { return centerY_; }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class SpherizeEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
public:
    SpherizeEffect();
    ~SpherizeEffect();

    // Expose properties via AbstractProperty API
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override;

    void setAmount(float amount);
    float amount() const;

    void setRadius(float radius);
    float radius() const;

    void setCenterX(float cx);
    float centerX() const;

    void setCenterY(float cy);
    float centerY() const;

    bool supportsGPU() const override {
        return true;
    }
};

};
