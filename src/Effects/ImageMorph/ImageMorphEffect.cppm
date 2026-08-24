module;
#include <algorithm>
#include <cmath>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

module Artifact.Effect.Distort.ImageMorph;

import Image.ImageF32x4RGBAWithCache;
import ImageProcessing.Distortion;

namespace Artifact {
using namespace ArtifactCore;

namespace {
class ImageMorphImpl final : public ArtifactEffectImplBase {
public:
    QString targetPath;
    QString pointsJson;
    float amount = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        if (targetPath.trimmed().isEmpty() || !QFileInfo::exists(targetPath)) {
            dst = src;
            return;
        }
        ImageF32x4_RGBA target;
        if (!target.load(targetPath)) {
            dst = src;
            return;
        }
        std::vector<MorphControlPoint> points;
        const QJsonDocument doc = QJsonDocument::fromJson(pointsJson.toUtf8());
        if (doc.isArray()) {
            for (const QJsonValue& value : doc.array()) {
                const QJsonObject object = value.toObject();
                MorphControlPoint point;
                point.sourceX = static_cast<float>(object.value("sourceX").toDouble());
                point.sourceY = static_cast<float>(object.value("sourceY").toDouble());
                point.targetX = static_cast<float>(object.value("targetX").toDouble());
                point.targetY = static_cast<float>(object.value("targetY").toDouble());
                point.weight = static_cast<float>(object.value("weight").toDouble(1.0));
                points.push_back(point);
            }
        }
        ImageF32x4_RGBA result;
        morphImages(src.image(), target, result, points, amount, true);
        dst.SetCpuImage(result);
    }
};
}

ImageMorphEffect::ImageMorphEffect() {
    setDisplayName(UniString("Image Morph"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(makeShared<ImageMorphImpl>());
    syncImpl();
}

ImageMorphEffect::~ImageMorphEffect() = default;

std::vector<AbstractProperty> ImageMorphEffect::getProperties() const {
    std::vector<AbstractProperty> properties;
    auto& target = properties.emplace_back();
    target.setName("Target Image Path");
    target.setType(PropertyType::String);
    target.setValue(targetImagePath_);
    auto& amount = properties.emplace_back();
    amount.setName("Amount");
    amount.setType(PropertyType::Float);
    amount.setValue(amount_);
    auto& points = properties.emplace_back();
    points.setName("Control Points JSON");
    points.setType(PropertyType::String);
    points.setValue(controlPointsJson_);
    return properties;
}

void ImageMorphEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Target Image Path")) targetImagePath_ = value.toString();
    else if (key == QStringLiteral("Amount")) amount_ = std::isfinite(value.toFloat()) ? std::clamp(value.toFloat(), 0.0f, 1.0f) : 0.0f;
    else if (key == QStringLiteral("Control Points JSON")) controlPointsJson_ = value.toString();
    syncImpl();
}

void ImageMorphEffect::syncImpl() {
    if (auto* impl = dynamic_cast<ImageMorphImpl*>(cpuImpl().get())) {
        impl->targetPath = targetImagePath_;
        impl->pointsJson = controlPointsJson_;
        impl->amount = amount_;
    }
}
}
