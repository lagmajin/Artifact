module;

#include <memory>

#include <QDebug>
#include <QJsonDocument>
#include <QString>

export module Artifact.Test.AdjustmentLayer;

import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;

namespace Artifact {

namespace {
struct AdjustmentLayerTestReport {
    int failures = 0;

    void check(bool condition, const QString& label)
    {
        if (!condition) {
            ++failures;
            qWarning().noquote() << "[AdjustmentLayer Test][FAIL]" << label;
        } else {
            qInfo().noquote() << "[AdjustmentLayer Test][OK]" << label;
        }
    }
};
} // namespace

export int runAdjustmentLayerTests()
{
    AdjustmentLayerTestReport report;

    ArtifactLayerFactory factory;
    ArtifactLayerInitParams params(QStringLiteral("Adjustment Layer"), LayerType::Adjustment);
    auto result = factory.createLayer(params);

    report.check(result.success && result.layer, QStringLiteral("adjustment layer can be created"));
    if (!result.layer) {
        qInfo().noquote() << "[AdjustmentLayer Test] failures:" << report.failures;
        return report.failures;
    }

    report.check(result.layer->isAdjustmentLayer(), QStringLiteral("adjustment flag is set"));
    report.check(!result.layer->isNullLayer(), QStringLiteral("adjustment layer is not null"));
    report.check(result.layer->layerName() == QStringLiteral("Adjustment Layer"), QStringLiteral("adjustment layer has expected name"));

    const QJsonDocument json(result.layer->toJson());
    const auto obj = json.object();
    report.check(obj.value(QStringLiteral("isAdjustment")).toBool(), QStringLiteral("serialized adjustment flag is true"));
    report.check(static_cast<int>(obj.value(QStringLiteral("type")).toInt()) == static_cast<int>(LayerType::Adjustment), QStringLiteral("serialized type is adjustment"));

    ArtifactAbstractLayerPtr loaded = ArtifactAbstractLayer::fromJson(obj);
    report.check(static_cast<bool>(loaded), QStringLiteral("adjustment layer roundtrip loads"));
    if (loaded) {
        report.check(loaded->isAdjustmentLayer(), QStringLiteral("loaded layer keeps adjustment flag"));
        report.check(loaded->layerName() == QStringLiteral("Adjustment Layer"), QStringLiteral("loaded layer keeps name"));
    }

    qInfo().noquote() << "[AdjustmentLayer Test] failures:" << report.failures;
    return report.failures;
}

} // namespace Artifact
