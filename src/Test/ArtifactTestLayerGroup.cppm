module;

#include <memory>

#include <QDebug>
#include <QJsonDocument>
#include <QString>
#include <QtGlobal>

export module Artifact.Test.LayerGroup;

import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.Group;
import Artifact.Layer.InitParams;
import Artifact.Composition.Nodes;
import Memory.SharedPtr;

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

    auto group = ArtifactCore::dynamicPointerCast<ArtifactGroupLayer>(groupResult.layer);
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
    auto composition = ArtifactCore::makeShared<ArtifactAbstractComposition>(compId, compParams);

    composition->appendLayerTop(group);
    report.check(group->composition() == composition.get(), QStringLiteral("group gets composition pointer"));
    report.check(childResult.layer->composition() == composition.get(), QStringLiteral("child inherits composition pointer"));
    report.check(composition->nodeStore().contains(group->id().toString()),
                 QStringLiteral("group is registered in composition node store"));
    report.check(composition->nodeStore().contains(childResult.layer->id().toString()),
                 QStringLiteral("child is registered in composition node store"));
    const auto* initialGroupNode = composition->nodeStore().node(group->id().toString());
    report.check(initialGroupNode && initialGroupNode->properties.value(QStringLiteral("enabled")).toBool(),
                 QStringLiteral("group enabled state is registered in node store"));
    report.check(initialGroupNode && qFuzzyCompare(
                     initialGroupNode->properties.value(QStringLiteral("opacity")).toDouble(), 1.0),
                 QStringLiteral("group opacity is registered in node store"));
    const auto containerNode = group->toContainerNode();
    report.check(containerNode.containsChild(childResult.layer->id().toString()),
                 QStringLiteral("group converts child to container node"));
    report.check(containerNode.outputMode() == GroupContainerOutputMode::All,
                 QStringLiteral("group converts output mode to container node"));
    group->setOutputMode(GroupOutputMode::Single);
    group->setActiveChildId(childResult.layer->id());
    const auto* groupNode = composition->nodeStore().node(group->id().toString());
    report.check(groupNode && groupNode->properties.value(QStringLiteral("outputMode")).toInt(-1) ==
                     static_cast<int>(GroupOutputMode::Single),
                 QStringLiteral("group output mode syncs to node properties"));
    report.check(groupNode && groupNode->properties.value(QStringLiteral("activeChildId")).toString() ==
                     childResult.layer->id().toString(),
                 QStringLiteral("group active child syncs to node properties"));

    ArtifactLayerInitParams secondChildParams(QStringLiteral("Second Child"), LayerType::Null);
    auto secondChildResult = factory.createLayer(secondChildParams);
    report.check(secondChildResult.success && secondChildResult.layer,
                 QStringLiteral("second child layer can be created"));
    if (secondChildResult.layer) {
        composition->appendLayerTop(secondChildResult.layer);
        group->insertChildAt(0, secondChildResult.layer);
        const auto orderedChildren = composition->childLayersOf(group->id());
        report.check(!orderedChildren.empty() && orderedChildren.front() &&
                         orderedChildren.front()->id() == secondChildResult.layer->id(),
                     QStringLiteral("group insertChildAt preserves requested order"));

        CompositionNode applyNode;
        applyNode.id = group->id().toString();
        GroupContainerNode requestedContainer(applyNode);
        requestedContainer.addChild(childResult.layer->id().toString());
        requestedContainer.addChild(secondChildResult.layer->id().toString());
        requestedContainer.setOutputMode(GroupContainerOutputMode::Single);
        requestedContainer.setActiveChildId(childResult.layer->id().toString());
        requestedContainer.setEnabled(false);
        requestedContainer.setOpacity(0.5);
        requestedContainer.setBlendMode(QStringLiteral("add"));
        report.check(group->applyGroupContainerNode(requestedContainer),
                     QStringLiteral("group applies container state"));
        const auto appliedChildren = composition->childLayersOf(group->id());
        report.check(appliedChildren.size() == 2 && appliedChildren.front() &&
                         appliedChildren.front()->id() == childResult.layer->id(),
                     QStringLiteral("group applies container child order"));
        report.check(!group->isVisible() && qFuzzyCompare(group->opacity(), 0.5f),
                     QStringLiteral("group applies container visibility and opacity"));
    }

    const QJsonDocument json = composition->toJson();
    auto loaded = ArtifactAbstractComposition::fromJson(json);
    report.check(static_cast<bool>(loaded), QStringLiteral("composition roundtrip loads"));

    if (loaded) {
        auto loadedGroupLayer = ArtifactCore::dynamicPointerCast<ArtifactGroupLayer>(loaded->layerById(group->id()));
        report.check(static_cast<bool>(loadedGroupLayer), QStringLiteral("loaded group layer is preserved"));
        if (loadedGroupLayer) {
            report.check(loadedGroupLayer->children().size() == 2, QStringLiteral("loaded group preserves child count"));
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
