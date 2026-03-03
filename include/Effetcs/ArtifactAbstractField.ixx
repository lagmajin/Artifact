export module Artifact.Effect.Field;

import std;
import Utils;
import Utils.String.UniString;
import Property.Group;

export namespace Artifact {
    using namespace ArtifactCore;

    enum class FieldType {
        Spherical,
        Box,
        Linear,
        Radial,
        FractalNoise,
        Solid // Constant 1.0 or 0.0
    };

    class ArtifactAbstractField {
    protected:
        FieldType type_;
        UniString name_;
        std::shared_ptr<PropertyGroup> properties_;

    public:
        ArtifactAbstractField(FieldType type, const UniString& name);
        virtual ~ArtifactAbstractField() = default;

        FieldType type() const { return type_; }
        UniString name() const { return name_; }
        void setName(const UniString& name) { name_ = name; }

        std::shared_ptr<PropertyGroup> properties() const { return properties_; }

        // Core evaluation: Returns influence [0.0 - 1.0] for a given 3D point in world space.
        // Used by Geometry Transforms or CPU-based evaluations.
        virtual float evaluateAt(const std::array<float, 3>& worldPos) const = 0;
        
        // Compute shader settings generation.
        // Returns the struct/buffer format needed to evaluate this field on the GPU.
        virtual void generateGPUData(/* Output formatting params here */) const = 0;
    };

    typedef std::shared_ptr<ArtifactAbstractField> ArtifactAbstractFieldPtr;
}
