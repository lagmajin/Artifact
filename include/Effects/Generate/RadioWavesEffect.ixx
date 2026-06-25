module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <QColor>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Generate.RadioWaves;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────────────────────
    // RadioWavesEffect
    //   AE の "Radio Waves" に相当するジェネレーター。
    //   中心から同心円状に波紋を広げ、フェードアウトさせる。
    //   エフェクトレイヤー上に重ねて使用する合成型効果。
    // ─────────────────────────────────────────────────────────────────────────

    class RadioWavesEffect : public ArtifactAbstractEffect {
    public:
        RadioWavesEffect();
        ~RadioWavesEffect();

        // ── アクセサ ──
        float originX() const;        void setOriginX(float v);
        float originY() const;        void setOriginY(float v);

        float frequency() const;      void setFrequency(float v);   // waves/sec
        float expansion() const;      void setExpansion(float v);   // px/sec (speed)
        float lifespan() const;       void setLifespan(float v);    // seconds

        float strokeWidth() const;    void setStrokeWidth(float v);
        float opacity() const;        void setOpacity(float v);     // 0-100 (%)

        QColor waveColor() const;     void setWaveColor(const QColor& c);

        float currentTime() const;    void setCurrentTime(float t); // seconds

        // ── Properties API ──
        std::vector<AbstractProperty> getProperties() const override;
        void setPropertyValue(const UniString& name, const QVariant& value) override;

        bool supportsGPU() const override { return false; }

    private:
        float  originX_     = 0.5f;    // 正規化 [0,1]
        float  originY_     = 0.5f;
        float  frequency_   = 2.0f;    // 波/秒
        float  expansion_   = 80.0f;   // px/秒
        float  lifespan_    = 2.0f;    // 秒
        float  strokeWidth_ = 3.0f;    // px
        float  opacity_     = 80.0f;   // %
        QColor waveColor_   = QColor(100, 200, 255, 220);
        float  currentTime_ = 0.0f;    // 秒

        void syncImpls();
    };

} // export namespace Artifact
