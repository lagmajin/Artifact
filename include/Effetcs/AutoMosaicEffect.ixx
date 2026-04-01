module;
#include <QString>
#include <QColor>
#include <QImage>
#include <QVariant>
#include <QVector>
#include <QRect>
#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <algorithm>

export module Artifact.Effect.AutoMosaic;

import Artifact.Effect.Abstract;
import Artifact.Effect.FaceDetection;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {

using namespace ArtifactCore;

// モザイクの種類
enum class MosaicType {
    Pixelate,  // ピクセル化
    Gaussian,  // ガウスぼかし
    Median     // メディアンフィルタ
};

// 自動モザイクエフェクト
class AutoMosaicEffect : public ArtifactAbstractEffect {
private:
    MosaicType mosaicType_ = MosaicType::Pixelate;
    int mosaicStrength_ = 16;       // ピクセルサイズ / ぼかし半径
    float feather_ = 0.0f;          // 境界フェザー (px)
    bool useFaceDetection_ = true;  // 顔認識を有効にする
    bool useCustomRegions_ = false; // 手動領域を有効にする
    QVector<QRect> customRegions_;  // 手動で指定した領域

    // 内部検出エンジン
    std::shared_ptr<FaceDetectionEngine> faceDetector_;
    FaceDetectionSettings detectionSettings_;

public:
    AutoMosaicEffect() {
        setDisplayName(ArtifactCore::UniString("Auto Mosaic (Face Detection)"));
        setPipelineStage(EffectPipelineStage::Rasterizer);
    }
    virtual ~AutoMosaicEffect() = default;

    // パラメータ
    MosaicType mosaicType() const { return mosaicType_; }
    void setMosaicType(MosaicType type) { mosaicType_ = type; }

    int mosaicStrength() const { return mosaicStrength_; }
    void setMosaicStrength(int strength) { mosaicStrength_ = std::max(1, strength); }

    float feather() const { return feather_; }
    void setFeather(float f) { feather_ = std::max(0.0f, f); }

    bool useFaceDetection() const { return useFaceDetection_; }
    void setUseFaceDetection(bool enabled) { useFaceDetection_ = enabled; }

    bool useCustomRegions() const { return useCustomRegions_; }
    void setUseCustomRegions(bool enabled) { useCustomRegions_ = enabled; }

    const QVector<QRect>& customRegions() const { return customRegions_; }
    void setCustomRegions(const QVector<QRect>& regions) { customRegions_ = regions; }
    void addCustomRegion(const QRect& region) { customRegions_.append(region); }
    void clearCustomRegions() { customRegions_.clear(); }

    // 顔検出エンジン
    void setFaceDetector(std::shared_ptr<FaceDetectionEngine> detector) { faceDetector_ = detector; }
    std::shared_ptr<FaceDetectionEngine> faceDetector() const { return faceDetector_; }

    const FaceDetectionSettings& detectionSettings() const { return detectionSettings_; }
    void setDetectionSettings(const FaceDetectionSettings& settings) { detectionSettings_ = settings; }

    // エフェクト処理
    QImage applyMosaic(const QImage& input, const QVector<QRect>& regions) const;

    // ArtifactAbstractEffect
    QImage applyToImage(const QImage& input) const override;
    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

} // namespace Artifact
