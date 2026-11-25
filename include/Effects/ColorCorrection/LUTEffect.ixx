module;
#include <utility>
#include <algorithm>
#include <memory>
#include <vector>
#include <cmath>
#include <cstdint>
#include <optional>
#include <QString>
#include <QVariant>

export module LUTEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Color.LUT;

export namespace Artifact {

using namespace ArtifactCore;

/// 3D LUT (Look-Up Table) カラーグレーディングエフェクト
/// .cube/.3dl ファイルの適用およびビルトインプリセット（Film, Cinematic, Vintage 等）の適用
class LUTEffect : public ArtifactAbstractEffect {
public:
    enum class Preset {
        Custom = 0,
        Cinematic = 1,
        Vintage = 2,
        Cold = 3,
        Warm = 4,
        HighContrast = 5,
        LowContrast = 6,
        Desaturated = 7,
        Kodak2383 = 8,
        Fuji3510 = 9,
    };

private:
    Preset preset_ = Preset::Cinematic;
    QString filePath_;
    float intensity_ = 1.0f; // 0.0 ~ 1.0

    void syncImpls();
    void updateLUT();

public:
    LUTEffect();
    ~LUTEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }

    void setFilePath(const QString& path);
    QString filePath() const { return filePath_; }

    void setIntensity(float value) {
        intensity_ = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 1.0f;
        syncImpls();
    }
    float intensity() const { return intensity_; }

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.lut.intensity"),
                QStringLiteral("Intensity"), 0.0, 1.0, 0.01, 0.0025,
                100.0, QStringLiteral("%")};
        }
        if (index == 2) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.color.lut.preset"),
                QStringLiteral("Preset"), 0.0, 9.0, 1.0, 0.05,
                1.0, QString()};
            parameter.choiceCount = 10;
            return parameter;
        }
        return std::nullopt;
    }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return false; } // 高精度3D補間CPU実装
};

} // namespace Artifact
