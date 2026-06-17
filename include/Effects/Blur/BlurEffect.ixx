module;
#include <utility>
#include <vector>
#include <algorithm>
#include <cmath>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Blur;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Utils.String.UniString;
import Property.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Translation.Manager;

export namespace Artifact {

using namespace ArtifactCore;

enum class BlurMode {
    Gaussian,
    EdgePreserving
};

class BlurEffect : public ArtifactAbstractEffect {
private:
    float radius_ = 10.0f;
    float strength_ = 1.0f;
    int iterations_ = 1;
    BlurMode mode_ = BlurMode::Gaussian;
    bool premultiplied_ = true;
    float edgeThreshold_ = 0.1f;

    void syncImpls();

public:
    BlurEffect();
    virtual ~BlurEffect() = default;

    float radius() const { return radius_; }
    void setRadius(float r) { radius_ = std::max(0.1f, r); syncImpls(); }

    float strength() const { return strength_; }
    void setStrength(float s) { strength_ = std::clamp(s, 0.0f, 1.0f); syncImpls(); }

    float sigma() const { return std::max(0.1f, radius_ * 0.5f); }
    void setSigma(float s) { setRadius(std::max(0.1f, s) * 2.0f); }

    int iterations() const { return iterations_; }
    void setIterations(int n) { iterations_ = std::max(1, n); syncImpls(); }

    BlurMode mode() const { return mode_; }
    void setMode(BlurMode m) { mode_ = m; syncImpls(); }

    bool premultiplied() const { return premultiplied_; }
    void setPremultiplied(bool p) { premultiplied_ = p; syncImpls(); }

    float edgeThreshold() const { return edgeThreshold_; }
    void setEdgeThreshold(float t) { edgeThreshold_ = std::clamp(t, 0.0f, 1.0f); syncImpls(); }

    std::vector<AbstractProperty> getProperties() const override {
        std::vector<AbstractProperty> props;

        AbstractProperty radiusProp;
        radiusProp.setName(TranslationManager::instance().tr("effect.blur.radius", "Radius"));
        radiusProp.setType(PropertyType::Float);
        radiusProp.setValue(radius_);
        props.push_back(radiusProp);

        AbstractProperty strengthProp;
        strengthProp.setName(TranslationManager::instance().tr("effect.blur.strength", "Strength"));
        strengthProp.setType(PropertyType::Float);
        strengthProp.setValue(strength_);
        props.push_back(strengthProp);

        return props;
    }

    void setPropertyValue(const UniString& name, const QVariant& value) override {
        const QString key = name.toQString();
        if (key == QStringLiteral("Radius") || key == TranslationManager::instance().tr("effect.blur.radius", "Radius")) {
            setRadius(value.toFloat());
        } else if (key == QStringLiteral("Strength") || key == TranslationManager::instance().tr("effect.blur.strength", "Strength")) {
            setStrength(value.toFloat());
        } else if (key == QStringLiteral("Sigma")) {
            setSigma(value.toFloat());
        } else if (key == QStringLiteral("Iterations")) {
            setIterations(value.toInt());
        } else if (key == QStringLiteral("Mode")) {
            setMode(static_cast<BlurMode>(value.toInt()));
        } else if (key == QStringLiteral("Premultiplied Alpha")) {
            setPremultiplied(value.toBool());
        } else if (key == QStringLiteral("Edge Threshold")) {
            setEdgeThreshold(value.toFloat());
        }
    }

    bool supportsGPU() const override { return true; }

    /**
     * @brief ROI 拡張ヒント
     *
     * ブラーは周辺ピクセルをサンプリングするため、出力 ROI より radius * 3 ピクセル
     * 広い入力領域が必要になる（3σ でガウス寄与がほぼゼロになる）。
     */
    EffectROIHint roiHint() const override {
        // iterations が増えるほど実効半径が大きくなる。
        const float effectiveRadius = radius_ * static_cast<float>(iterations_);
        return EffectROIHint{
            .kind = EffectROIHintKind::Blur,
            .expansionPixels = effectiveRadius * 3.0f,
            .requiresFullFrame = false
        };
    }
};

} // namespace Artifact
