module;
#include <QString>
#include <QVariant>
#include <QVector>
#include <opencv2/opencv.hpp>
#include <memory>
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

// ─────────────────────────────────────────────────────────
// BlurEffect  –  Rasterizer フェーズ用 ガウシアンブラー
//
//   機能:
//   - separable Gaussian（水平＋垂直の2パスで高速化）
//   - エッジクランプ（黒にじみ防止）
//   - premultiplied alpha 対応
//   - optional: edge-preserving（軽量版 bilateral フィルタ）
// ─────────────────────────────────────────────────────────

enum class BlurMode {
    Gaussian,        // 標準ガウシアンブラー
    EdgePreserving   // エッジ保持（軽量 bilateral）
};

class BlurEffect : public ArtifactAbstractEffect {
private:
    float sigma_      = 5.0f;
    int   iterations_ = 1;
    BlurMode mode_    = BlurMode::Gaussian;
    bool  premultiplied_ = true;
    float edgeThreshold_ = 0.1f;  // エッジ保持の閾値（edge-preserving モード用）

    void syncImpls();

public:
    BlurEffect();
    virtual ~BlurEffect() = default;

    // ── アクセサ ──
    float sigma() const { return sigma_; }
    void  setSigma(float s) { sigma_ = std::max(0.1f, s); }

    int iterations() const { return iterations_; }
    void setIterations(int n) { iterations_ = std::max(1, n); }

    BlurMode mode() const { return mode_; }
    void setMode(BlurMode m) { mode_ = m; }

    bool premultiplied() const { return premultiplied_; }
    void setPremultiplied(bool p) { premultiplied_ = p; }

    float edgeThreshold() const { return edgeThreshold_; }
    void setEdgeThreshold(float t) { edgeThreshold_ = std::clamp(t, 0.0f, 1.0f); }

    // ── Properties API ──
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
        modeProp.setType(PropertyType::Enum);
        modeProp.setValue(static_cast<int>(mode_));
        modeProp.setEnumLabels({"Gaussian", "Edge Preserving"});
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
        if (name == UniString("Sigma")) {
            sigma_ = std::max(0.1f, value.toFloat());
        } else if (name == UniString("Iterations")) {
            iterations_ = std::max(1, value.toInt());
        } else if (name == UniString("Mode")) {
            mode_ = static_cast<BlurMode>(value.toInt());
        } else if (name == UniString("Premultiplied Alpha")) {
            premultiplied_ = value.toBool();
        } else if (name == UniString("Edge Threshold")) {
            edgeThreshold_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
        }
    }

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
    virtual ~BlurEffect() = default;

    // ── アクセサ ──
    float sigma() const { return sigma_; }
    void  setSigma(float s) { sigma_ = std::max(0.1f, s); }

    int iterations() const { return iterations_; }
    void setIterations(int n) { iterations_ = std::max(1, n); }

    BlurMode mode() const { return mode_; }
    void setMode(BlurMode m) { mode_ = m; }

    bool premultiplied() const { return premultiplied_; }
    void setPremultiplied(bool p) { premultiplied_ = p; }

    float edgeThreshold() const { return edgeThreshold_; }
    void setEdgeThreshold(float t) { edgeThreshold_ = std::clamp(t, 0.0f, 1.0f); }

    // ── Properties API ──
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
        modeProp.setType(PropertyType::Enum);
        modeProp.setValue(static_cast<int>(mode_));
        modeProp.setEnumLabels({"Gaussian", "Edge Preserving"});
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
        if (name == UniString("Sigma")) {
            sigma_ = std::max(0.1f, value.toFloat());
        } else if (name == UniString("Iterations")) {
            iterations_ = std::max(1, value.toInt());
        } else if (name == UniString("Mode")) {
            mode_ = static_cast<BlurMode>(value.toInt());
        } else if (name == UniString("Premultiplied Alpha")) {
            premultiplied_ = value.toBool();
        } else if (name == UniString("Edge Threshold")) {
            edgeThreshold_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
        }
    }

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact