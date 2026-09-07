module;

#include <QDebug>
#include <QJsonObject>
#include <QString>

export module Artifact.Test.SolidLayer;

import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Artifact.Layers.SolidImage;

namespace Artifact {

namespace {
struct SolidLayerTestReport {
    int failures = 0;
    void check(bool condition, const QString& label)
    {
        if (!condition) {
            ++failures;
            qWarning().noquote() << "[SolidLayer Test][FAIL]" << label;
        }
    }
};
} // namespace

export int runSolidLayerTests()
{
    SolidLayerTestReport report;
    ArtifactLayerFactory factory;
    ArtifactSolidLayerInitParams params(QStringLiteral("Anamorphic Solid"));
    params.setWidth(720);
    params.setHeight(480);
    params.setPixelAspectRatio(1.21);
    params.setSourceItemId(QStringLiteral("solid-source-test"));

    const auto result = factory.createLayer(params);
    report.check(result.success && result.layer,
                 QStringLiteral("solid layer can be created"));
    if (result.layer) {
        const auto* solid = dynamic_cast<const ArtifactSolidImageLayer*>(result.layer.get());
        report.check(solid && qFuzzyCompare(solid->pixelAspectRatio(), 1.21),
                     QStringLiteral("pixel aspect ratio reaches the created layer"));
        if (solid) {
            const auto json = solid->toJson();
            report.check(qFuzzyCompare(json.value(QStringLiteral("solidPixelAspectRatio")).toDouble(), 1.21),
                         QStringLiteral("pixel aspect ratio is serialized"));
            report.check(json.value(QStringLiteral("solidSourceItemId")).toString() ==
                             QStringLiteral("solid-source-test"),
                         QStringLiteral("solid source item id is serialized"));
            auto restored = factory.createLayer(params);
            if (restored.layer) {
                restored.layer->fromJsonProperties(json);
                const auto* restoredSolid =
                    dynamic_cast<const ArtifactSolidImageLayer*>(restored.layer.get());
                report.check(restoredSolid && restoredSolid->sourceItemId() ==
                                 QStringLiteral("solid-source-test"),
                             QStringLiteral("solid source item id is restored"));
            }
        }
    }
    qInfo().noquote() << "[SolidLayer Test] failures:" << report.failures;
    return report.failures;
}

} // namespace Artifact
