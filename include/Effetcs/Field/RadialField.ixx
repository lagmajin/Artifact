module;
#include <QString>

export module Artifact.Effect.Field.Radial;

import std;
import Artifact.Effect.Field;
import Utils.String.UniString;
import Property.Group;
import Property.Abstract;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────
    // RadialField  –  放射状フォールオフ（中心軸型）
    //
    //   指定された中心軸（center + axis方向）からの距離に基づき、
    //   円筒状に influence が減衰する。
    //   MoGraph の Radial フィールドに相当する。
    // ─────────────────────────────────────────────────────────

    class RadialField : public ArtifactAbstractField {
    private:
        std::array<float, 3> center_ = {0.0f, 0.0f, 0.0f};
        std::array<float, 3> axis_   = {0.0f, 1.0f, 0.0f};  // Y-up
        float innerRadius_ = 0.0f;
        float outerRadius_ = 100.0f;

    public:
        RadialField()
            : ArtifactAbstractField(FieldType::Radial, UniString("Radial Field"))
        {
            auto innerProp = std::make_shared<AbstractProperty>();
            innerProp->setName("Inner Radius");
            innerProp->setType(PropertyType::Float);
            innerProp->setValue(innerRadius_);
            properties_->addProperty(innerProp);

            auto outerProp = std::make_shared<AbstractProperty>();
            outerProp->setName("Outer Radius");
            outerProp->setType(PropertyType::Float);
            outerProp->setValue(outerRadius_);
            properties_->addProperty(outerProp);
        }

        ~RadialField() override = default;

        // ── アクセサ ──
        std::array<float, 3> center() const { return center_; }
        void setCenter(const std::array<float, 3>& c) { center_ = c; }

        std::array<float, 3> axis() const { return axis_; }
        void setAxis(const std::array<float, 3>& a) { axis_ = a; }

        float innerRadius() const { return innerRadius_; }
        void setInnerRadius(float r) { innerRadius_ = r; }

        float outerRadius() const { return outerRadius_; }
        void setOuterRadius(float r) { outerRadius_ = r; }

        // ── 評価 ──
        float evaluateAt(const std::array<float, 3>& worldPos) const override {
            // Compute vector from center to worldPos
            float dx = worldPos[0] - center_[0];
            float dy = worldPos[1] - center_[1];
            float dz = worldPos[2] - center_[2];

            // Axis normalization
            float axLen = std::sqrt(axis_[0]*axis_[0] + axis_[1]*axis_[1] + axis_[2]*axis_[2]);
            if (axLen <= 0.0f) return 1.0f;

            float axNx = axis_[0] / axLen;
            float axNy = axis_[1] / axLen;
            float axNz = axis_[2] / axLen;

            // Project d onto axis → remove axis component to get perpendicular distance
            float dot = dx * axNx + dy * axNy + dz * axNz;
            float px = dx - dot * axNx;
            float py = dy - dot * axNy;
            float pz = dz - dot * axNz;
            float perpDist = std::sqrt(px*px + py*py + pz*pz);

            if (perpDist <= innerRadius_) {
                return 1.0f;
            }
            if (outerRadius_ <= innerRadius_ || perpDist >= outerRadius_) {
                return 0.0f;
            }
            float t = (perpDist - innerRadius_) / (outerRadius_ - innerRadius_);
            return 1.0f - (t * t * (3.0f - 2.0f * t));
        }

        void generateGPUData() const override {
            // TODO: GPU バッファフォーマット生成
            // struct { float3 center; float3 axis; float innerRadius; float outerRadius; }
        }
    };

}
