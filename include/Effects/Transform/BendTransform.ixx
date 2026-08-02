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
import Utils.String.UniString;
import Artifact.Effect.Field;

export namespace Artifact {

    using namespace ArtifactCore;

    class BendTransform : public ArtifactAbstractEffect {
    private:
        ArtifactAbstractFieldPtr field_;
        float angle_ = 0.0f; // Bending angle in degrees
        float direction_ = 0.0f; // Direction of bend
        float size_ = 100.0f; // Domain size of bend effect

    public:
        BendTransform() {
            setDisplayName(ArtifactCore::UniString("Bend (Geo Transform)"));
            setPipelineStage(EffectPipelineStage::GeometryTransform);
        }
        virtual ~BendTransform() = default;

        void setField(ArtifactAbstractFieldPtr field) { field_ = field; }
        ArtifactAbstractFieldPtr field() const { return field_; }

        float angle() const { return angle_; }
        void setAngle(float angle) { angle_ = std::isfinite(angle) ? std::clamp(angle, -720.0f, 720.0f) : 0.0f; }

        float direction() const { return direction_; }
        void setDirection(float dir) { direction_ = std::isfinite(dir) ? std::clamp(dir, -360.0f, 360.0f) : 0.0f; }

        float size() const { return size_; }
        void setSize(float s) { size_ = std::isfinite(s) ? std::clamp(s, 0.01f, 100000.0f) : 100.0f; }

        std::vector<AbstractProperty> getProperties() const override {
            std::vector<AbstractProperty> props;
            auto& angle = props.emplace_back();
            angle.setName("Angle"); angle.setType(PropertyType::Float); angle.setValue(angle_);
            angle.setMinValue(QVariant(-720.0)); angle.setMaxValue(QVariant(720.0));
            auto& direction = props.emplace_back();
            direction.setName("Direction"); direction.setType(PropertyType::Float); direction.setValue(direction_);
            direction.setMinValue(QVariant(-360.0)); direction.setMaxValue(QVariant(360.0));
            auto& size = props.emplace_back();
            size.setName("Size"); size.setType(PropertyType::Float); size.setValue(size_);
            size.setMinValue(QVariant(0.01)); size.setMaxValue(QVariant(100000.0));
            return props;
        }

        void setPropertyValue(const UniString& name, const QVariant& value) override {
            if (name == UniString("Angle")) setAngle(value.toFloat());
            else if (name == UniString("Direction")) setDirection(value.toFloat());
            else if (name == UniString("Size")) setSize(value.toFloat());
        }
    };

}
