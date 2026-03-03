module;
#include <QString>

export module Artifact.Effect.Rasterizer.Blur;

import std;
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
    //   既存の GauusianBlur モジュールとは別に、
    //   パイプライン準拠の ArtifactAbstractEffect として再定義。
    //   CPU/GPU の Impl 切り替えは ArtifactAbstractEffect 側
    //   の setCPUImpl/setGPUImpl で行う。
    // ─────────────────────────────────────────────────────────

    class BlurEffect : public ArtifactAbstractEffect {
    private:
        float sigma_      = 5.0f;
        int   iterations_ = 1;      // マルチパスブラー回数

    public:
        BlurEffect() {
            setDisplayName(ArtifactCore::UniString("Blur (Rasterizer)"));
            setPipelineStage(EffectPipelineStage::Rasterizer);
        }
        virtual ~BlurEffect() = default;

        // ── アクセサ ──
        float sigma()      const { return sigma_; }
        void  setSigma(float s)  { sigma_ = s; }

        int   iterations() const { return iterations_; }
        void  setIterations(int n) { iterations_ = n; }

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

            return props;
        }

        void setPropertyValue(const UniString& name, const QVariant& value) override {
            if (name == UniString("Sigma")) {
                sigma_ = value.toFloat();
            } else if (name == UniString("Iterations")) {
                iterations_ = value.toInt();
            }
        }

        bool supportsGPU() const override { return true; }
    };

}