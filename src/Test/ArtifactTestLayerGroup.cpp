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
    GroupContainerNode storedContainer;
    report.check(composition->nodeStore().getGroupContainer(group->id().toString(), storedContainer),
                 QStringLiteral("group container can be read from node store"));
    report.check(storedContainer.containsChild(childResult.layer->id().toString()),
                 QStringLiteral("node store group container exposes children"));
    report.check(composition->isGroupContainerNode(group->id().toString()),
                 QStringLiteral("group container kind resolves via node store"));
    report.check(!composition->isGroupContainerNode(childResult.layer->id().toString()),
                 QStringLiteral("plain layer is not a group container node"));
    report.check(!composition->isGroupContainerNode(QStringLiteral("missing-node-id")),
                 QStringLiteral("missing node id is not a group container"));
    report.check(composition->isGroupLayerResolved(group),
                 QStringLiteral("group layer resolves via node store"));
    report.check(!composition->isGroupLayerResolved(childResult.layer),
                 QStringLiteral("plain layer does not resolve as group"));
    report.check(composition->isGroupLayerResolved(group->id()),
                 QStringLiteral("group layer id resolves via node store"));
    report.check(!composition->isGroupLayerResolved(childResult.layer->id()),
                 QStringLiteral("plain layer id does not resolve as group"));
    {
        ArtifactLayerInitParams orphanParams(QStringLiteral("Orphan Group"), LayerType::Group);
        auto orphanResult = factory.createLayer(orphanParams);
        auto orphanGroup = ArtifactCore::dynamicPointerCast<ArtifactGroupLayer>(
            orphanResult.success ? orphanResult.layer : ArtifactAbstractLayerPtr{});
        report.check(static_cast<bool>(orphanGroup), QStringLiteral("orphan group layer can be created"));
        if (orphanGroup) {
            report.check(!composition->isGroupContainerNode(orphanGroup->id().toString()),
                         QStringLiteral("unregistered group has no container node"));
            report.check(composition->isGroupLayerResolved(orphanGroup),
                         QStringLiteral("unregistered group falls back to virtual"));
        }
    }

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
        report.check(composition->selectedChildForGroupEvaluation(group->id()) == childResult.layer->id(),
                     QStringLiteral("container single mode selects active child"));
        report.check(composition->shouldEvaluateLayer(childResult.layer->id()),
                     QStringLiteral("active child evaluates in single mode"));
        report.check(!composition->shouldEvaluateLayer(secondChildResult.layer->id()),
                     QStringLiteral("inactive child skips in single mode"));
        group->setOutputMode(GroupOutputMode::All);
        report.check(composition->shouldEvaluateLayer(childResult.layer->id()) &&
                     composition->shouldEvaluateLayer(secondChildResult.layer->id()),
                     QStringLiteral("all mode evaluates every child"));
        group->setOutputMode(GroupOutputMode::Share);
        report.check(qFuzzyCompare(composition->groupEvaluationGainForChild(
                                       group->id(), childResult.layer->id()), 0.5f) &&
                     qFuzzyCompare(composition->groupEvaluationGainForChild(
                                       group->id(), secondChildResult.layer->id()), 0.5f),
                     QStringLiteral("share mode splits gain over visible children"));
        group->setOutputMode(GroupOutputMode::All);
        report.check(qFuzzyCompare(composition->groupEvaluationGainForChild(
                                       group->id(), childResult.layer->id()), 1.0f),
                     QStringLiteral("all mode keeps unity gain"));
        report.check(!composition->isGroupExclusive(group->id()),
                     QStringLiteral("all mode is not exclusive"));
        group->setOutputMode(GroupOutputMode::Single);
        report.check(composition->isGroupExclusive(group->id()),
                     QStringLiteral("single mode is exclusive"));
        group->setOutputMode(GroupOutputMode::All);
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
