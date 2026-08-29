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
#include <QString>
export module Artifact.Effect.Transform.Twist;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Utils.String.UniString;

export namespace Artifact {

    using namespace ArtifactCore;

    class TwistTransformCPUImpl : public ArtifactEffectImplBase {
    private:
        float angle_ = 45.0f;
        float centerX_ = 0.5f;
        float centerY_ = 0.5f;

    public:
        TwistTransformCPUImpl() = default;

        void setAngle(float angle) { angle_ = std::isfinite(angle) ? std::clamp(angle, -720.0f, 720.0f) : 45.0f; }
        float angle() const { return angle_; }

        void setCenterX(float cx) { centerX_ = std::isfinite(cx) ? std::clamp(cx, 0.0f, 1.0f) : 0.5f; }
        float centerX() const { return centerX_; }

        void setCenterY(float cy) { centerY_ = std::isfinite(cy) ? std::clamp(cy, 0.0f, 1.0f) : 0.5f; }
        float centerY() const { return centerY_; }

        void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
    };

    class TwistTransformGPUImpl : public ArtifactEffectImplBase {
    private:
        float angle_ = 45.0f;
        float centerX_ = 0.5f;
        float centerY_ = 0.5f;

    public:
        TwistTransformGPUImpl() = default;

        void setAngle(float angle) { angle_ = std::isfinite(angle) ? std::clamp(angle, -720.0f, 720.0f) : 45.0f; }
        float angle() const { return angle_; }

        void setCenterX(float cx) { centerX_ = std::isfinite(cx) ? std::clamp(cx, 0.0f, 1.0f) : 0.5f; }
        float centerX() const { return centerX_; }

        void setCenterY(float cy) { centerY_ = std::isfinite(cy) ? std::clamp(cy, 0.0f, 1.0f) : 0.5f; }
        float centerY() const { return centerY_; }

        void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
    };

    class TwistTransform : public ArtifactAbstractEffect {
    private:
        class Impl;
        Impl* impl_;

    public:
        TwistTransform();
        ~TwistTransform();

        std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
        void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override;

        void setAngle(float angle);
        float angle() const;

        void setCenterX(float cx);
        float centerX() const;

        void setCenterY(float cy);
        float centerY() const;

        bool supportsGPU() const override {
            return true;
        }
    };

}
