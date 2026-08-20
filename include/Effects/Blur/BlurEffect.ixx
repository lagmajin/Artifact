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
    void setRadius(float r) { radius_ = std::isfinite(r) ? std::max(0.1f, r) : 10.0f; syncImpls(); }

    float strength() const { return strength_; }
    void setStrength(float s) { strength_ = std::isfinite(s) ? std::clamp(s, 0.0f, 1.0f) : 1.0f; syncImpls(); }

    float sigma() const { return std::max(0.1f, radius_ * 0.5f); }
    void setSigma(float s) { setRadius(std::max(0.1f, s) * 2.0f); }

    int iterations() const { return iterations_; }
    void setIterations(int n) { iterations_ = std::clamp(n, 1, 16); syncImpls(); }

    BlurMode mode() const { return mode_; }
    void setMode(BlurMode m) { mode_ = m; syncImpls(); }

    bool premultiplied() const { return premultiplied_; }
    void setPremultiplied(bool p) { premultiplied_ = p; syncImpls(); }

    float edgeThreshold() const { return edgeThreshold_; }
    void setEdgeThreshold(float t) { edgeThreshold_ = std::isfinite(t) ? std::clamp(t, 0.0f, 1.0f) : 0.1f; syncImpls(); }

    std::vector<AbstractProperty> getProperties() const override {
        std::vector<AbstractProperty> props;

        AbstractProperty radiusProp;
        radiusProp.setName(TranslationManager::instance().tr("effect.blur.radius", "Radius"));
        radiusProp.setType(PropertyType::Float);
        radiusProp.setValue(radius_);
        radiusProp.setDefaultValue(10.0);
        radiusProp.setHardRange(0.1, 2048.0);
        radiusProp.setSoftRange(0.1, 128.0);
        radiusProp.setStep(0.1);
        radiusProp.setUnit(QStringLiteral("px"));
        radiusProp.setTooltip(QStringLiteral("Blur radius in source pixels."));
        props.push_back(radiusProp);

        AbstractProperty strengthProp;
        strengthProp.setName(TranslationManager::instance().tr("effect.blur.strength", "Strength"));
        strengthProp.setType(PropertyType::Float);
        strengthProp.setValue(strength_);
        strengthProp.setDefaultValue(1.0);
        strengthProp.setHardRange(0.0, 1.0);
        strengthProp.setStep(0.01);
        strengthProp.setTooltip(QStringLiteral("Blend amount of the blur calculation."));

        AbstractProperty iterationsProp;
        iterationsProp.setName(QStringLiteral("Iterations"));
        iterationsProp.setType(PropertyType::Integer);
        iterationsProp.setValue(iterations_);
        iterationsProp.setDefaultValue(1);
        iterationsProp.setHardRange(1, 16);
        iterationsProp.setStep(1);
        iterationsProp.setTooltip(QStringLiteral("Repeated blur passes; higher values increase the effective radius and cost."));
        props.push_back(iterationsProp);

        AbstractProperty modeProp;
        modeProp.setName(QStringLiteral("Mode"));
        modeProp.setDisplayLabel(QStringLiteral("Blur Mode"));
        modeProp.setType(PropertyType::Integer);
        modeProp.setValue(static_cast<int>(mode_));
        modeProp.setDefaultValue(static_cast<int>(BlurMode::Gaussian));
        modeProp.setHardRange(0, 1);
        modeProp.setTooltip(QStringLiteral("0=Gaussian, 1=Edge Preserving."));
        props.push_back(modeProp);

        AbstractProperty edgeThresholdProp;
        edgeThresholdProp.setName(QStringLiteral("Edge Threshold"));
        edgeThresholdProp.setType(PropertyType::Float);
        edgeThresholdProp.setValue(edgeThreshold_);
        edgeThresholdProp.setDefaultValue(0.1);
        edgeThresholdProp.setHardRange(0.0, 1.0);
        edgeThresholdProp.setStep(0.01);
        edgeThresholdProp.setTooltip(QStringLiteral("Edge-preserving sensitivity; used only in Edge Preserving mode."));
        props.push_back(edgeThresholdProp);

        AbstractProperty premultipliedProp;
        premultipliedProp.setName(QStringLiteral("Premultiplied Alpha"));
        premultipliedProp.setType(PropertyType::Boolean);
        premultipliedProp.setValue(premultiplied_);
        premultipliedProp.setDefaultValue(true);
        premultipliedProp.setTooltip(QStringLiteral("Process color as premultiplied alpha to avoid transparent-edge color bleed."));
        props.push_back(premultipliedProp);
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
        } else if (key == QStringLiteral("effect.allowOverscan") ||
                   key == QStringLiteral("Allow Overscan")) {
            setAllowOverscan(value.toBool());
        } else {
            setCommonPropertyValue(key, value);
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
        if (!allowOverscan()) {
            return EffectROIHint{};
        }
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
