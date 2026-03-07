module;
#include <QString>

export module Artifact.Effect.Field.Box;

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



import Artifact.Effect.Field;
import Utils.String.UniString;
import Property.Group;
import Property.Abstract;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────
    // BoxField  –  直方体フォールオフ
    //
    //   center を中心に halfExtent で定義される直方体。
    //   内部は influence 1.0、falloff 距離でスムーズに減衰。
    //   Cinema 4D の Box Field に相当する。
    // ─────────────────────────────────────────────────────────

    class BoxField : public ArtifactAbstractField {
    private:
        std::array<float, 3> center_     = {0.0f, 0.0f, 0.0f};
        std::array<float, 3> halfExtent_ = {50.0f, 50.0f, 50.0f};
        float falloffWidth_              = 10.0f;

    public:
        BoxField()
            : ArtifactAbstractField(FieldType::Box, UniString("Box Field"))
        {
            auto widthProp = std::make_shared<AbstractProperty>();
            widthProp->setName("Width");
            widthProp->setType(PropertyType::Float);
            widthProp->setValue(halfExtent_[0] * 2.0f);
            properties_->addProperty(widthProp);

            auto heightProp = std::make_shared<AbstractProperty>();
            heightProp->setName("Height");
            heightProp->setType(PropertyType::Float);
            heightProp->setValue(halfExtent_[1] * 2.0f);
            properties_->addProperty(heightProp);

            auto depthProp = std::make_shared<AbstractProperty>();
            depthProp->setName("Depth");
            depthProp->setType(PropertyType::Float);
            depthProp->setValue(halfExtent_[2] * 2.0f);
            properties_->addProperty(depthProp);

            auto falloffProp = std::make_shared<AbstractProperty>();
            falloffProp->setName("Falloff Width");
            falloffProp->setType(PropertyType::Float);
            falloffProp->setValue(falloffWidth_);
            properties_->addProperty(falloffProp);
        }

        ~BoxField() override = default;

        // ── アクセサ ──
        std::array<float, 3> center() const { return center_; }
        void setCenter(const std::array<float, 3>& c) { center_ = c; }

        std::array<float, 3> halfExtent() const { return halfExtent_; }
        void setHalfExtent(const std::array<float, 3>& h) { halfExtent_ = h; }

        float falloffWidth() const { return falloffWidth_; }
        void setFalloffWidth(float w) { falloffWidth_ = w; }

        // ── 評価 ──
        float evaluateAt(const std::array<float, 3>& worldPos) const override {
            // SDF (Signed Distance Field) approach for axis-aligned box
            float dx = std::max(0.0f, std::abs(worldPos[0] - center_[0]) - halfExtent_[0]);
            float dy = std::max(0.0f, std::abs(worldPos[1] - center_[1]) - halfExtent_[1]);
            float dz = std::max(0.0f, std::abs(worldPos[2] - center_[2]) - halfExtent_[2]);
            float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (dist <= 0.0f) {
                return 1.0f;
            }
            if (falloffWidth_ <= 0.0f || dist >= falloffWidth_) {
                return 0.0f;
            }
            float t = dist / falloffWidth_;
            return 1.0f - (t * t * (3.0f - 2.0f * t));
        }

        void generateGPUData() const override {
            // TODO: GPU バッファフォーマット生成
            // struct { float3 center; float3 halfExtent; float falloffWidth; }
        }
    };

}
