module;
#include <cmath>
#include <algorithm>
#include <memory>
#include <QString>
#include <QVariant>
#include <utility>
#include <vector>

export module Artifact.Effect.Keying.IBKKeyer;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import ImageProcessing:IBKKeyer;
import Memory.SharedPtr;

export namespace Artifact {

class IBKKeyerEffectCPUImpl final : public ArtifactEffectImplBase {
    ArtifactCore::Keying::IBKParams params_{};
    ArtifactCore::ImageF32x4_RGBA cleanPlate_;
    int previewMode_ = 0;
public:
    const ArtifactCore::Keying::IBKParams& params() const { return params_; }
    void setParams(const ArtifactCore::Keying::IBKParams& params) {
        params_ = params;
        params_.screenCorrection = std::isfinite(params_.screenCorrection)
            ? std::clamp(params_.screenCorrection, 0.0f, 4.0f) : 1.0f;
        params_.coreMatteClip = std::isfinite(params_.coreMatteClip)
            ? std::clamp(params_.coreMatteClip, 0.0f, 1.0f) : 0.5f;
        params_.edgeMatteSoftness = std::isfinite(params_.edgeMatteSoftness)
            ? std::clamp(params_.edgeMatteSoftness, 1.0e-5f, 1.0f) : 0.2f;
        params_.despillStrength = std::isfinite(params_.despillStrength)
            ? std::clamp(params_.despillStrength, 0.0f, 1.0f) : 0.5f;
        params_.garbageMatteGamma = std::isfinite(params_.garbageMatteGamma)
            ? std::clamp(params_.garbageMatteGamma, 1.0e-5f, 4.0f) : 1.0f;
        params_.detailRecovery = std::isfinite(params_.detailRecovery)
            ? std::clamp(params_.detailRecovery, 0.0f, 1.0f) : 0.3f;
        params_.erodePixels = std::clamp(params_.erodePixels, 0, 64);
        params_.dilatePixels = std::clamp(params_.dilatePixels, 0, 64);
    }
    void setCleanPlate(const ArtifactCore::ImageF32x4_RGBA& image) { cleanPlate_ = image; }
    bool setCleanPlatePath(const QString& path) {
        ArtifactCore::ImageF32x4_RGBA image;
        if (path.trimmed().isEmpty() || !image.load(path)) return false;
        cleanPlate_ = std::move(image);
        return true;
    }
    bool hasCleanPlate() const { return cleanPlate_.width() > 0 && cleanPlate_.height() > 0; }
    const ArtifactCore::ImageF32x4_RGBA& cleanPlate() const { return cleanPlate_; }
    void setPreviewMode(int mode) { previewMode_ = std::clamp(mode, 0, 2); }
    int previewMode() const { return previewMode_; }
    void applyCPU(const ArtifactCore::ImageF32x4RGBAWithCache& src,
                  ArtifactCore::ImageF32x4RGBAWithCache& dst) override;
};

class IBKKeyerEffect final : public ArtifactAbstractEffect {
    SharedPtr<IBKKeyerEffectCPUImpl> typedCpuImpl_;
    QString cleanPlatePath_;
    int previewMode_ = 0;
public:
    IBKKeyerEffect();
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const ArtifactCore::UniString& name,
                          const QVariant& value) override;
    void setCleanPlate(const ArtifactCore::ImageF32x4_RGBA& image);
};

} // namespace Artifact
