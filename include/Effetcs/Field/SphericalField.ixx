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
export module Artifact.Effect.Field.Spherical;




import Artifact.Effect.Field;
import Utils.String.UniString;
import Property.Group;
import Property.Abstract;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────
    // SphericalField  –  球体フォールオフ
    //
    //   center を中心に radius の半径を持つ球体。
    //   球体内部は influence 1.0、外部は 0.0、
    //   falloffWidth の範囲でスムーズに減衰する。
    // ─────────────────────────────────────────────────────────

    class SphericalField : public ArtifactAbstractField {
    private:
        std::array<float, 3> center_   = {0.0f, 0.0f, 0.0f};
        float radius_                  = 100.0f;
        float falloffWidth_            = 20.0f;   // 球面からの減衰幅

    public:
        SphericalField()
            : ArtifactAbstractField(FieldType::Spherical, UniString("Spherical Field"))
        {
            // Properties の初期設定
            auto radiusProp = std::make_shared<AbstractProperty>();
            radiusProp->setName("Radius");
            radiusProp->setType(PropertyType::Float);
            radiusProp->setValue(radius_);
            properties_->addProperty(radiusProp);

            auto falloffProp = std::make_shared<AbstractProperty>();
            falloffProp->setName("Falloff Width");
            falloffProp->setType(PropertyType::Float);
            falloffProp->setValue(falloffWidth_);
            properties_->addProperty(falloffProp);
        }

        ~SphericalField() override = default;

        // ── アクセサ ──
        std::array<float, 3> center() const { return center_; }
        void setCenter(const std::array<float, 3>& c) { center_ = c; }

        float radius() const { return radius_; }
        void setRadius(float r) { radius_ = r; }

        float falloffWidth() const { return falloffWidth_; }
        void setFalloffWidth(float w) { falloffWidth_ = w; }

        // ── 評価 ──
        float evaluateAt(const std::array<float, 3>& worldPos) const override {
            float dx = worldPos[0] - center_[0];
            float dy = worldPos[1] - center_[1];
            float dz = worldPos[2] - center_[2];
            float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (dist <= radius_) {
                return 1.0f;
            }
            if (falloffWidth_ <= 0.0f || dist >= radius_ + falloffWidth_) {
                return 0.0f;
            }
            // Smooth hermite interpolation in the falloff zone
            float t = (dist - radius_) / falloffWidth_;
            return 1.0f - (t * t * (3.0f - 2.0f * t));  // smoothstep(0, 1, t) inverted
        }

        void generateGPUData() const override {
            // TODO: GPU バッファフォーマット生成
            // struct { float3 center; float radius; float falloffWidth; }
        }
    };

}
