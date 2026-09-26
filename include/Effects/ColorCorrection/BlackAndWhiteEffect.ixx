module;
#include <utility>
#include <algorithm>
#include <memory>
#include <vector>
#include <cmath>
#include <cstdint>
#include <QString>
#include <QVariant>
#include <QColor>

export module BlackAndWhiteEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

/// Black & White (白黒調整) エフェクト
/// 6つの色相領域（赤・黄・緑・シアン・青・マゼンタ）ごとに明るさの重み付けを個別に制御
class BlackAndWhiteEffect : public ArtifactAbstractEffect {
public:
    enum class Preset {
        Custom = 0,
        Default = 1,
        HighContrast = 2,
        Infrared = 3,
        DeepBlue = 4,
        NeutralDensity = 5,
    };

private:
    Preset preset_ = Preset::Default;
    float reds_ = 0.40f;        // -2.0 ~ 3.0
    float yellows_ = 0.60f;     // -2.0 ~ 3.0
    float greens_ = 0.40f;      // -2.0 ~ 3.0
    float cyans_ = 0.60f;       // -2.0 ~ 3.0
    float blues_ = 0.20f;       // -2.0 ~ 3.0
    float magentas_ = 0.80f;    // -2.0 ~ 3.0
    QColor tintColor_ = QColor(225, 199, 160); // セピアトーニング等の色
    float tintAmount_ = 0.0f;   // 0.0 ~ 1.0 (デフォルト 0.0)

    void syncImpls();
    void applyPreset(Preset preset);

public:
    BlackAndWhiteEffect();
    ~BlackAndWhiteEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }

    void setReds(float val) { reds_ = std::clamp(val, -2.0f, 3.0f); syncImpls(); }
    float reds() const { return reds_; }

    void setYellows(float val) { yellows_ = std::clamp(val, -2.0f, 3.0f); syncImpls(); }
    float yellows() const { return yellows_; }

    void setGreens(float val) { greens_ = std::clamp(val, -2.0f, 3.0f); syncImpls(); }
    float greens() const { return greens_; }

    void setCyans(float val) { cyans_ = std::clamp(val, -2.0f, 3.0f); syncImpls(); }
    float cyans() const { return cyans_; }

    void setBlues(float val) { blues_ = std::clamp(val, -2.0f, 3.0f); syncImpls(); }
    float blues() const { return blues_; }

    void setMagentas(float val) { magentas_ = std::clamp(val, -2.0f, 3.0f); syncImpls(); }
    float magentas() const { return magentas_; }

    void setTintColor(const QColor& color) { tintColor_ = color; syncImpls(); }
    QColor tintColor() const { return tintColor_; }

    void setTintAmount(float val) { tintAmount_ = std::clamp(val, 0.0f, 1.0f); syncImpls(); }
    float tintAmount() const { return tintAmount_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
    static constexpr const char* kGpuGenericKeyString = "black_and_white";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);
    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Generic;
        node.genericKey = kGpuGenericKey;
        node.parameters[0] = reds_;
        node.parameters[1] = yellows_;
        node.parameters[2] = greens_;
        node.parameters[3] = cyans_;
        node.parameters[4] = blues_;
        node.parameters[5] = magentas_;
        node.parameters[6] = tintAmount_;
        return stack.append(node);
    }
};

} // namespace Artifact
