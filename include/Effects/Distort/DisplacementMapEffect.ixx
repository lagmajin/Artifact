module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <QColor>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Distort.DisplacementMap;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────────────────────
    // DisplacementMapEffect
    //   AE の "Displacement Map" に相当。
    //   自身のルミナンス（または RGBA の任意チャンネル）を使って
    //   ピクセルを水平・垂直方向にオフセットさせる。
    //   セルフ・ディスプレイスメント（自分自身を変位マップとして使用）実装。
    // ─────────────────────────────────────────────────────────────────────────

    enum class DisplaceChannel : int {
        Luminance = 0,
        Red       = 1,
        Green     = 2,
        Blue      = 3,
        Alpha     = 4,
    };

    class DisplacementMapEffect : public ArtifactAbstractEffect {
    public:
        DisplacementMapEffect();
        ~DisplacementMapEffect();

        // ── アクセサ ──
        float maxHorizontal() const;
        void  setMaxHorizontal(float v);

        float maxVertical() const;
        void  setMaxVertical(float v);

        DisplaceChannel horizontalChannel() const;
        void            setHorizontalChannel(DisplaceChannel c);

        DisplaceChannel verticalChannel() const;
        void            setVerticalChannel(DisplaceChannel c);

        bool wrapAround() const;
        void setWrapAround(bool v);

        // ── Properties API ──
        std::vector<AbstractProperty> getProperties() const override;
        void setPropertyValue(const UniString& name, const QVariant& value) override;

        bool supportsGPU() const override { return false; }

    private:
        float           maxHorizontal_     = 20.0f;
        float           maxVertical_       = 20.0f;
        DisplaceChannel horizontalChannel_ = DisplaceChannel::Red;
        DisplaceChannel verticalChannel_   = DisplaceChannel::Green;
        bool            wrapAround_        = false;

        void syncImpls();
    };

} // export namespace Artifact
