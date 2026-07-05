module;
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>
#include <QColor>
export module Artifact.Effect.Rasterizer.DropShadow;




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
    public:
        DropShadowEffect();
        ~DropShadowEffect();

        // ── アクセサ ──
        QColor shadowColor() const;
        void   setShadowColor(const QColor& c);

        float distance()  const;
        void  setDistance(float d);

        float angle()     const;
        void  setAngle(float a);

        float softness()  const;
        void  setSoftness(float s);

        float opacity()   const;
        void  setOpacity(float o);

        // ── Properties API ──
        std::vector<AbstractProperty> getProperties() const override;
        void setPropertyValue(const UniString& name, const QVariant& value) override;

        bool supportsGPU() const override { return true; }

    private:
        QColor shadowColor_ = QColor(0, 0, 0, 180);
        float  distance_    = 5.0f;    // 影の距離 (px)
        float  angle_       = 135.0f;  // 影の方向 (degrees)
        float  softness_    = 8.0f;    // ぼかし半径
        float  opacity_     = 75.0f;   // 影の不透明度 (0–100%)

        void syncImpls();
    };

}
