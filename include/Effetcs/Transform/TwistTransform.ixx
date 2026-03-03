module;
#include <QString>

export module Artifact.Effect.Transform.Twist;

import std;
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
