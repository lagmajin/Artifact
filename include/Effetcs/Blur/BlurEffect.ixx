module;
#include <QString>
#include <QVariant>
#include <vector>
#include <algorithm>
#include <cmath>

export module Artifact.Effect.Rasterizer.Blur;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Utils.String.UniString;
import Property.Abstract;
import Image.ImageF32x4RGBAWithCache;

export namespace Artifact {

using namespace ArtifactCore;

enum class BlurMode {
    Gaussian,
    EdgePreserving
};

class BlurEffect : public ArtifactAbstractEffect {
private:
    float sigma_ = 5.0f;
    int iterations_ = 1;
    BlurMode mode_ = BlurMode::Gaussian;
    bool premultiplied_ = true;
    float edgeThreshold_ = 0.1f;

    void syncImpls();

public:
    BlurEffect();
    virtual ~BlurEffect() = default;

    float sigma() const { return sigma_; }
    void setSigma(float s) { sigma_ = std::max(0.1f, s); }

    int iterations() const { return iterations_; }
    void setIterations(int n) { iterations_ = std::max(1, n); }

    BlurMode mode() const { return mode_; }
    void setMode(BlurMode m) { mode_ = m; }

    bool premultiplied() const { return premultiplied_; }
    void setPremultiplied(bool p) { premultiplied_ = p; }

    float edgeThreshold() const { return edgeThreshold_; }
    void setEdgeThreshold(float t) { edgeThreshold_ = std::clamp(t, 0.0f, 1.0f); }

    std::vector<AbstractProperty> getProperties() const override {
        std::vector<AbstractProperty> props;

        AbstractProperty sigmaProp;
        sigmaProp.setName("Sigma");
        sigmaProp.setType(PropertyType::Float);
        sigmaProp.setValue(sigma_);
        props.push_back(sigmaProp);

        AbstractProperty iterProp;
        iterProp.setName("Iterations");
        iterProp.setType(PropertyType::Integer);
        iterProp.setValue(iterations_);
        props.push_back(iterProp);

        AbstractProperty modeProp;
        modeProp.setName("Mode");
        modeProp.setType(PropertyType::Integer);
        modeProp.setValue(static_cast<int>(mode_));
        props.push_back(modeProp);

        AbstractProperty premultProp;
        premultProp.setName("Premultiplied Alpha");
        premultProp.setType(PropertyType::Boolean);
        premultProp.setValue(premultiplied_);
        props.push_back(premultProp);

        if (mode_ == BlurMode::EdgePreserving) {
            AbstractProperty edgeProp;
            edgeProp.setName("Edge Threshold");
            edgeProp.setType(PropertyType::Float);
            edgeProp.setValue(edgeThreshold_);
            props.push_back(edgeProp);
        }

        return props;
    }

    void setPropertyValue(const UniString& name, const QVariant& value) override {
        const QString key = name.toQString();
        if (key == QStringLiteral("Sigma")) {
            sigma_ = std::max(0.1f, value.toFloat());
        } else if (key == QStringLiteral("Iterations")) {
            iterations_ = std::max(1, value.toInt());
        } else if (key == QStringLiteral("Mode")) {
            mode_ = static_cast<BlurMode>(value.toInt());
        } else if (key == QStringLiteral("Premultiplied Alpha")) {
            premultiplied_ = value.toBool();
        } else if (key == QStringLiteral("Edge Threshold")) {
            edgeThreshold_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
        }
    }

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
