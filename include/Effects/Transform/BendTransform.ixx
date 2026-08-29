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
export module Artifact.Effect.Transform.Bend;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Utils.String.UniString;

export namespace Artifact {

    using namespace ArtifactCore;

    class BendTransformCPUImpl : public ArtifactEffectImplBase {
    private:
        float angle_ = 0.0f;
        float direction_ = 0.0f;
        float size_ = 100.0f;

    public:
        BendTransformCPUImpl() = default;

        void setAngle(float angle) { angle_ = std::isfinite(angle) ? std::clamp(angle, -720.0f, 720.0f) : 0.0f; }
        float angle() const { return angle_; }

        void setDirection(float dir) { direction_ = std::isfinite(dir) ? std::clamp(dir, -360.0f, 360.0f) : 0.0f; }
        float direction() const { return direction_; }

        void setSize(float s) { size_ = std::isfinite(s) ? std::clamp(s, 0.01f, 100000.0f) : 100.0f; }
        float size() const { return size_; }

        void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
    };

    class BendTransformGPUImpl : public ArtifactEffectImplBase {
    private:
        float angle_ = 0.0f;
        float direction_ = 0.0f;
        float size_ = 100.0f;

    public:
        BendTransformGPUImpl() = default;

        void setAngle(float angle) { angle_ = std::isfinite(angle) ? std::clamp(angle, -720.0f, 720.0f) : 0.0f; }
        float angle() const { return angle_; }

        void setDirection(float dir) { direction_ = std::isfinite(dir) ? std::clamp(dir, -360.0f, 360.0f) : 0.0f; }
        float direction() const { return direction_; }

        void setSize(float s) { size_ = std::isfinite(s) ? std::clamp(s, 0.01f, 100000.0f) : 100.0f; }
        float size() const { return size_; }

        void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
    };

    class BendTransform : public ArtifactAbstractEffect {
    private:
        class Impl;
        Impl* impl_;

    public:
        BendTransform();
        ~BendTransform();

        std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
        void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override;

        void setAngle(float angle);
        float angle() const;

        void setDirection(float dir);
        float direction() const;

        void setSize(float s);
        float size() const;

        bool supportsGPU() const override {
            return true;
        }
    };

}
