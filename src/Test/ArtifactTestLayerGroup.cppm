module;

#include <memory>

#include <QDebug>
#include <QJsonDocument>
#include <QString>

export module Artifact.Test.LayerGroup;

import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.Group;
import Artifact.Layer.InitParams;

namespace Artifact {

namespace {
struct LayerGroupTestReport {
    int failures = 0;

    void check(bool condition, const QString& label)
    {
        if (!condition) {
            ++failures;
            qWarning().noquote() << "[LayerGroup Test][FAIL]" << label;
        } else {
            qInfo().noquote() << "[LayerGroup Test][OK]" << label;
        }
    }
};
} // namespace

export int runLayerGroupTests()
{
    LayerGroupTestReport report;

    ArtifactLayerFactory factory;

    ArtifactLayerInitParams groupParams(QStringLiteral("Group A"), LayerType::Group);
    auto groupResult = factory.createLayer(groupParams);
    report.check(groupResult.success && groupResult.layer, QStringLiteral("group layer can be created"));

    ArtifactLayerInitParams childParams(QStringLiteral("Child Layer"), LayerType::Null);
    auto childResult = factory.createLayer(childParams);
    report.check(childResult.success && childResult.layer, QStringLiteral("child layer can be created"));

    auto group = std::dynamic_pointer_cast<ArtifactGroupLayer>(groupResult.layer);
    report.check(static_cast<bool>(group), QStringLiteral("group layer casts correctly"));

    if (!group || !childResult.layer) {
        qInfo().noquote() << "[LayerGroup Test] failures:" << report.failures;
        return report.failures;
    }

    group->addChild(childResult.layer);
    report.check(group->children().size() == 1, QStringLiteral("group stores child locally"));
    report.check(childResult.layer->parentLayerId() == group->id(), QStringLiteral("child parent id is set"));

    ArtifactCompositionInitParams compParams(QStringLiteral("Group Test"), FloatColor{0.1f, 0.1f, 0.1f, 1.0f});
    compParams.setResolution(1280, 720);
    compParams.setDurationFrames(120);
    const CompositionID compId(QStringLiteral("group-test-comp"));
    auto composition = std::make_shared<ArtifactAbstractComposition>(compId, compParams);

    composition->appendLayerTop(group);
    report.check(group->composition() == composition.get(), QStringLiteral("group gets composition pointer"));
    report.check(childResult.layer->composition() == composition.get(), QStringLiteral("child inherits composition pointer"));

    const QJsonDocument json = composition->toJson();
    auto loaded = ArtifactAbstractComposition::fromJson(json);
    report.check(static_cast<bool>(loaded), QStringLiteral("composition roundtrip loads"));

    if (loaded) {
        auto loadedGroupLayer = std::dynamic_pointer_cast<ArtifactGroupLayer>(loaded->layerById(group->id()));
        report.check(static_cast<bool>(loadedGroupLayer), QStringLiteral("loaded group layer is preserved"));
        if (loadedGroupLayer) {
            report.check(loadedGroupLayer->children().size() == 1, QStringLiteral("loaded group preserves child count"));
            const auto loadedChild = loadedGroupLayer->children().front();
            report.check(static_cast<bool>(loadedChild), QStringLiteral("loaded child exists"));
            if (loadedChild) {
                report.check(loadedChild->parentLayerId() == loadedGroupLayer->id(), QStringLiteral("loaded child keeps parent id"));
                report.check(loadedChild->composition() == loaded.get(), QStringLiteral("loaded child inherits composition pointer"));
            }
        }
    }

    qInfo().noquote() << "[LayerGroup Test] failures:" << report.failures;
    return report.failures;
}

} // namespace Artifact
