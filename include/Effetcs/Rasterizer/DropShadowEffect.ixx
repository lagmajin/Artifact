module;
#include <QString>
#include <QColor>

export module Artifact.Effect.Rasterizer.DropShadow;

import std;
import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────
    // DropShadowEffect  –  Rasterizer フェーズ用 ドロップシャドウ
    //
    //   レンダリング済み2D画像に対して、影を合成するエフェクト。
    //   AE の "Drop Shadow" に相当する。
    // ─────────────────────────────────────────────────────────

    class DropShadowEffect : public ArtifactAbstractEffect {
    private:
        QColor shadowColor_ = QColor(0, 0, 0, 180);
        float  distance_    = 5.0f;    // 影の距離 (px)
        float  angle_       = 135.0f;  // 影の方向 (degrees)
        float  softness_    = 8.0f;    // ぼかし半径
        float  opacity_     = 75.0f;   // 影の不透明度 (%)

    public:
        DropShadowEffect() {
            setDisplayName(ArtifactCore::UniString("Drop Shadow (Rasterizer)"));
            setPipelineStage(EffectPipelineStage::Rasterizer);
        }
        virtual ~DropShadowEffect() = default;

        // ── アクセサ ──
        QColor shadowColor() const { return shadowColor_; }
        void   setShadowColor(const QColor& c) { shadowColor_ = c; }

        float distance()  const { return distance_; }
        void  setDistance(float d) { distance_ = d; }

        float angle()     const { return angle_; }
        void  setAngle(float a)  { angle_ = a; }

        float softness()  const { return softness_; }
        void  setSoftness(float s) { softness_ = s; }

        float opacity()   const { return opacity_; }
        void  setOpacity(float o)  { opacity_ = o; }

        // ── Properties API ──
        std::vector<AbstractProperty> getProperties() const override {
            std::vector<AbstractProperty> props;
            props.reserve(5);

            auto& colorProp = props.emplace_back();
            colorProp.setName("Shadow Color");
            colorProp.setType(PropertyType::Color);
            colorProp.setValue(shadowColor_);

            auto& distProp = props.emplace_back();
            distProp.setName("Distance");
            distProp.setType(PropertyType::Float);
            distProp.setValue(distance_);

            auto& angleProp = props.emplace_back();
            angleProp.setName("Angle");
            angleProp.setType(PropertyType::Float);
            angleProp.setValue(angle_);

            auto& softProp = props.emplace_back();
            softProp.setName("Softness");
            softProp.setType(PropertyType::Float);
            softProp.setValue(softness_);

            auto& opacProp = props.emplace_back();
            opacProp.setName("Opacity");
            opacProp.setType(PropertyType::Float);
            opacProp.setValue(opacity_);

            return props;
        }

        void setPropertyValue(const UniString& name, const QVariant& value) override {
            if (name == UniString("Distance"))  { distance_  = value.toFloat(); }
            else if (name == UniString("Angle"))     { angle_     = value.toFloat(); }
            else if (name == UniString("Softness"))  { softness_  = value.toFloat(); }
            else if (name == UniString("Opacity"))   { opacity_   = value.toFloat(); }
            // Shadow Color はカラーピッカー経由のため別途バインディングが必要
        }

        bool supportsGPU() const override { return true; }
    };

}
