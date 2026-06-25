module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <QVariant>
#include <QString>

export module ShadowHighlightEffect;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────────────────────
    // ShadowHighlightEffect
    //   AE / Photoshop の "Shadow / Highlight" に相当。
    //   シャドウ部を独立して持ち上げ、ハイライト部を独立して引き下げる。
    //   単純な Brightness と異なり、明暗部それぞれに半径・量を指定して
    //   知覚的に自然な補正を行う。
    // ─────────────────────────────────────────────────────────────────────────

    class ShadowHighlightEffect : public ArtifactAbstractEffect {
    public:
        ShadowHighlightEffect();
        ~ShadowHighlightEffect();

        // ── Shadows ──
        float shadowAmount()    const;  void setShadowAmount(float v);    // 0-100 %
        float shadowTonalWidth() const; void setShadowTonalWidth(float v);// 0-100 %
        float shadowRadius()    const;  void setShadowRadius(float v);    // px

        // ── Highlights ──
        float highlightAmount()    const;  void setHighlightAmount(float v);    // 0-100 %
        float highlightTonalWidth() const; void setHighlightTonalWidth(float v);// 0-100 %
        float highlightRadius()    const;  void setHighlightRadius(float v);    // px

        // ── Adjustments ──
        float colorCorrection() const; void setColorCorrection(float v); // -100..100
        float midtoneContrast() const; void setMidtoneContrast(float v); // -100..100
        float blackClip()       const; void setBlackClip(float v);       // 0..1
        float whiteClip()       const; void setWhiteClip(float v);       // 0..1

        // ── Properties API ──
        std::vector<AbstractProperty> getProperties() const override;
        void setPropertyValue(const UniString& name, const QVariant& value) override;

        bool supportsGPU() const override { return false; }

    private:
        // Shadows
        float shadowAmount_     = 50.0f;
        float shadowTonalWidth_ = 50.0f;
        float shadowRadius_     = 30.0f;

        // Highlights
        float highlightAmount_     = 0.0f;
        float highlightTonalWidth_ = 50.0f;
        float highlightRadius_     = 30.0f;

        // Adjustments
        float colorCorrection_ = 20.0f;
        float midtoneContrast_ = 0.0f;
        float blackClip_       = 0.01f;
        float whiteClip_       = 0.01f;

        void syncImpls();
    };

} // export namespace Artifact
