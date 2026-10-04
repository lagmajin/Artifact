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

        // AE renders the shadow without the source image when this is on.
        bool  shadowOnly() const;
        void  setShadowOnly(bool v);

        // ── Properties API ──
        std::vector<AbstractProperty> getProperties() const override;
        void setPropertyValue(const UniString& name, const QVariant& value) override;

        bool supportsGPU() const override { return true; }

        static constexpr const char* kGpuGenericKeyString = "drop_shadow";
        static constexpr std::uint32_t kGpuGenericKey =
            gpuGenericKeyFromString(kGpuGenericKeyString);
        std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }

        // The resident shader mirrors DropShadowCPUImpl, but it gathers taps
        // from a fixed 32-pixel box and clamps its radius to that, so a very
        // soft shadow stays on the CPU where cv::GaussianBlur has no such cap.
        GpuRasterEffectDomain gpuRasterEffectDomain() const override {
            return softness_ <= 12.8f ? GpuRasterEffectDomain::Spatial
                                      : GpuRasterEffectDomain::None;
        }

        bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
            if (gpuRasterEffectDomain() != GpuRasterEffectDomain::Spatial) {
                return false;
            }
            const float rad = angle_ * (3.14159265358979f / 180.0f);
            GpuSpatialEffectNode node;
            node.kind = GpuSpatialEffectKind::Generic;
            node.genericKey = kGpuGenericKey;
            node.parameters[0] = -distance_ * std::cos(rad);
            node.parameters[1] = distance_ * std::sin(rad);
            node.parameters[2] = softness_;
            // The CPU path multiplies the color alpha into the same term.
            node.parameters[3] =
                std::clamp(opacity_ / 100.0f, 0.0f, 1.0f) * shadowColor_.alphaF();
            node.parameters[4] = shadowColor_.redF();
            node.parameters[5] = shadowColor_.greenF();
            node.parameters[6] = shadowColor_.blueF();
            // P7 is the Shadow Only toggle: the resident body then writes just
            // the shadow term instead of compositing the source over it.
            node.parameters[7] = shadowOnly_ ? 1.0f : 0.0f;
            // Offsets and softness are authored in pixels, so they scale with
            // the interactive downsample.
            node.resolutionScaledParameterMask = (1u << 0) | (1u << 1) | (1u << 2);
            return stack.append(node);
        }

        // The shadow is the source alpha offset by up to distance_ pixels, then
        // blurred with sigma = softness_. A Gaussian needs roughly 3 sigma of
        // input around every output pixel it touches, and the offset shifts
        // which side of the frame that input comes from, so the expansion is
        // bounded by the offset magnitude plus the blur reach.
        EffectROIHint roiHint() const override {
            if (!allowOverscan()) {
                return EffectROIHint{};
            }
            return EffectROIHint{
                .kind = EffectROIHintKind::Blur,
                .expansionPixels = distance_ + softness_ * 3.0f,
                .requiresFullFrame = false
            };
        }

    private:
        QColor shadowColor_ = QColor(0, 0, 0, 180);
        float  distance_    = 5.0f;    // 影の距離 (px)
        float  angle_       = 135.0f;  // 影の方向 (degrees)
        float  softness_    = 8.0f;    // ぼかし半径
        float  opacity_     = 75.0f;   // 影の不透明度 (0–100%)
        bool   shadowOnly_  = false;   // 影のみ描画

        void syncImpls();
    };

}
