module;
#include <QString>

export module Artifact.Effect.Field.Linear;

import std;
import Artifact.Effect.Field;
import Utils.String.UniString;
import Property.Group;
import Property.Abstract;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────
    // LinearField  –  線形勾配フォールオフ
    //
    //   2つの平面 (startPos → endPos) の間で
    //   influence を 1.0 → 0.0 にリニアまたは smoothstep で減衰する。
    //   方向ベクトルは start→end の法線として自動計算される。
    // ─────────────────────────────────────────────────────────

    class LinearField : public ArtifactAbstractField {
    private:
        std::array<float, 3> startPos_ = {0.0f, 0.0f, 0.0f};
        std::array<float, 3> endPos_   = {0.0f, 100.0f, 0.0f};
        bool useSmoothstep_            = true;

        // 内部キャッシュ
        mutable std::array<float, 3> direction_{};
        mutable float length_ = 0.0f;
        mutable bool  dirty_  = true;

        void recalculate() const {
            direction_[0] = endPos_[0] - startPos_[0];
            direction_[1] = endPos_[1] - startPos_[1];
            direction_[2] = endPos_[2] - startPos_[2];
            length_ = std::sqrt(
                direction_[0] * direction_[0] +
                direction_[1] * direction_[1] +
                direction_[2] * direction_[2]
            );
            if (length_ > 0.0f) {
                direction_[0] /= length_;
                direction_[1] /= length_;
                direction_[2] /= length_;
            }
            dirty_ = false;
        }

    public:
        LinearField()
            : ArtifactAbstractField(FieldType::Linear, UniString("Linear Field"))
        {
            auto smoothProp = std::make_shared<AbstractProperty>();
            smoothProp->setName("Smooth Interpolation");
            smoothProp->setType(PropertyType::Boolean);
            smoothProp->setValue(useSmoothstep_);
            properties_->addProperty(smoothProp);
        }

        ~LinearField() override = default;

        // ── アクセサ ──
        std::array<float, 3> startPos() const { return startPos_; }
        void setStartPos(const std::array<float, 3>& p) { startPos_ = p; dirty_ = true; }

        std::array<float, 3> endPos() const { return endPos_; }
        void setEndPos(const std::array<float, 3>& p) { endPos_ = p; dirty_ = true; }

        bool useSmoothstep() const { return useSmoothstep_; }
        void setUseSmoothstep(bool s) { useSmoothstep_ = s; }

        // ── 評価 ──
        float evaluateAt(const std::array<float, 3>& worldPos) const override {
            if (dirty_) recalculate();
            if (length_ <= 0.0f) return 1.0f;

            // Project worldPos onto the start→end vector
            float dx = worldPos[0] - startPos_[0];
            float dy = worldPos[1] - startPos_[1];
            float dz = worldPos[2] - startPos_[2];
            float proj = dx * direction_[0] + dy * direction_[1] + dz * direction_[2];

            float t = proj / length_;
            t = std::clamp(t, 0.0f, 1.0f);

            // t = 0 → influence 1.0,  t = 1 → influence 0.0
            float influence = 1.0f - t;
            if (useSmoothstep_) {
                // Apply smoothstep for smoother transitions
                influence = influence * influence * (3.0f - 2.0f * influence);
            }
            return influence;
        }

        void generateGPUData() const override {
            // TODO: GPU バッファフォーマット生成
            // struct { float3 startPos; float3 direction; float length; uint useSmoothstep; }
        }
    };

}
