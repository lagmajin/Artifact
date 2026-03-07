module;
#include <QString>

export module Artifact.Effect.Transform.Twist;

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

            return props;
        }

        void setPropertyValue(const UniString& name, const QVariant& value) override {
            if (name == UniString("Angle")) {
                angle_ = value.toFloat();
                // trigger repaint update...
            }
        }
    };

}
