module;
#include <QString>

export module Artifact.Effect.Transform.Bend;

import std;
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
        void setAngle(float angle) { angle_ = angle; }

        float direction() const { return direction_; }
        void setDirection(float dir) { direction_ = dir; }

        float size() const { return size_; }
        void setSize(float s) { size_ = s; }

        // Future: Return parameters via getProperties()
    };

}
