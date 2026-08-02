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
import Utils.String.UniString;
import Artifact.Effect.Field;

export namespace Artifact {

    using namespace ArtifactCore;

    class TwistTransform : public ArtifactAbstractEffect {
    private:
        ArtifactAbstractFieldPtr field_;
        float angle_ = 45.0f;
    public:
        TwistTransform() {
            setDisplayName(ArtifactCore::UniString("Twist (Geo Transform)"));
            setPipelineStage(EffectPipelineStage::GeometryTransform);
        }
        virtual ~TwistTransform() = default;

        void setField(ArtifactAbstractFieldPtr field) { field_ = field; }

        std::vector<AbstractProperty> getProperties() const override {
            std::vector<AbstractProperty> props;
            props.reserve(1);

            auto& angleProp = props.emplace_back();
            angleProp.setName("Angle");
            angleProp.setType(PropertyType::Float);
            angleProp.setValue(angle_);
            angleProp.setMinValue(QVariant(-720.0));
            angleProp.setMaxValue(QVariant(720.0));

            return props;
        }

        void setPropertyValue(const UniString& name, const QVariant& value) override {
            if (name == UniString("Angle")) {
                const float angle = value.toFloat();
                angle_ = std::isfinite(angle) ? std::clamp(angle, -720.0f, 720.0f) : 45.0f;
                // trigger repaint update...
            }
        }
    };

}
