module;

#include <QDebug>
#include <QJsonDocument>
#include <QImage>
#include <QSize>
#include <QString>

export module Artifact.Test.NullLayer;

import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Memory.SharedPtr;

namespace Artifact {

namespace {
struct NullLayerTestReport {
    int failures = 0;

    void check(bool condition, const QString& label)
    {
        if (!condition) {
            ++failures;
            qWarning().noquote() << "[NullLayer Test][FAIL]" << label;
        } else {
            qInfo().noquote() << "[NullLayer Test][OK]" << label;
        }
    }
};
} // namespace

export int runNullLayerTests()
{
    NullLayerTestReport report;
    ArtifactLayerFactory factory;
    ArtifactLayerInitParams params(QStringLiteral("Production Null"), LayerType::Null);
    auto result = factory.createLayer(params);

    report.check(result.success && result.layer, QStringLiteral("null layer can be created"));
    if (!result.layer) {
        qInfo().noquote() << "[NullLayer Test] failures:" << report.failures;
        return report.failures;
    }

    report.check(result.layer->isNullLayer(), QStringLiteral("null flag is set"));
    report.check(!result.layer->isAdjustmentLayer(), QStringLiteral("null layer is not adjustment"));
    report.check(result.layer->layerName() == QStringLiteral("Production Null"), QStringLiteral("null layer has expected name"));

    const QImage image = result.layer->toQImage();
    report.check(!image.isNull() && image.size() == QSize(100, 100), QStringLiteral("null layer produces default-size image"));
    report.check(image.format() == QImage::Format_ARGB32_Premultiplied && image.pixelColor(0, 0).alpha() == 0,
                 QStringLiteral("null layer image is transparent"));

    const QImage thumbnail = result.layer->getThumbnail(32, 24);
    report.check(!thumbnail.isNull() && thumbnail.width() <= 32 && thumbnail.height() <= 24,
                 QStringLiteral("null layer thumbnail respects requested bounds"));

    const auto obj = QJsonDocument(result.layer->toJson()).object();
    report.check(obj.value(QStringLiteral("type")).toInt() == static_cast<int>(LayerType::Null),
                 QStringLiteral("serialized type is null"));
    auto loaded = ArtifactAbstractLayer::fromJson(obj);
    report.check(static_cast<bool>(loaded) && loaded->isNullLayer(), QStringLiteral("null layer roundtrip preserves type"));
    if (loaded) {
        report.check(loaded->layerName() == QStringLiteral("Production Null"), QStringLiteral("roundtrip preserves name"));
        report.check(loaded->toQImage().size() == QSize(100, 100), QStringLiteral("roundtrip preserves render contract"));
    }

    ArtifactCompositionInitParams compParams(QStringLiteral("Null Integration"), FloatColor{0.1f, 0.1f, 0.1f, 1.0f});
    compParams.setResolution(1280, 720);
    compParams.setDurationFrames(24);
    const CompositionID compId(QStringLiteral("null-integration-comp"));
    auto composition = ArtifactCore::makeShared<ArtifactAbstractComposition>(compId, compParams);
    composition->appendLayerTop(result.layer);
    const auto compositionJson = composition->toJson();
    auto loadedComposition = ArtifactAbstractComposition::fromJson(compositionJson);
    report.check(static_cast<bool>(loadedComposition), QStringLiteral("composition with null layer roundtrip loads"));
    if (loadedComposition) {
        const auto loadedLayer = loadedComposition->layerById(result.layer->id());
        report.check(static_cast<bool>(loadedLayer) && loadedLayer->isNullLayer(),
                     QStringLiteral("composition roundtrip preserves null layer"));
    }

    qInfo().noquote() << "[NullLayer Test] failures:" << report.failures;
    return report.failures;
}

} // namespace Artifact
