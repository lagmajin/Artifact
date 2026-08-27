module;
#include <memory>
#include <QDebug>
#include <QString>

export module Artifact.Test.CompositionNode;

import Artifact.Composition.Node;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Color.Float;
import Utils.Id;

namespace Artifact {
export int runCompositionNodeTests() {
    int failures = 0;
    const auto check = [&failures](bool ok, const QString& label) {
        if (!ok) { ++failures; qWarning().noquote() << "[CompositionNode Test][FAIL]" << label; }
        else { qInfo().noquote() << "[CompositionNode Test][OK]" << label; }
    };

    const ArtifactCore::Id containerId;
    ArtifactContainerNode container(containerId);
    const ArtifactCore::Id childId;
    check(container.kind() == CompositionNodeKind::Container, QStringLiteral("container kind is explicit"));
    check(!container.id().isNil(), QStringLiteral("container has an identity"));
    check(!container.addChild(ArtifactCore::Id::Nil()), QStringLiteral("nil child is rejected"));
    check(container.addChild(childId), QStringLiteral("child can be added"));
    check(!container.addChild(childId), QStringLiteral("duplicate child is rejected"));
    check(container.containsChild(childId), QStringLiteral("child membership is reported"));
    check(container.removeChild(childId), QStringLiteral("child can be removed"));
    check(!container.containsChild(childId), QStringLiteral("removed child is absent"));
    check(!container.removeChild(childId), QStringLiteral("missing child removal is rejected"));

    ArtifactCompositionNodeStore store;
    auto stored = std::make_unique<ArtifactContainerNode>();
    const auto storedId = stored->id();
    check(store.add(std::move(stored)), QStringLiteral("node store accepts a container"));
    check(store.size() == 1 && store.find(storedId), QStringLiteral("node store finds by id"));
    check(!store.add(std::make_unique<ArtifactContainerNode>(storedId)),
          QStringLiteral("node store rejects duplicate ids"));
    check(store.remove(storedId) && store.size() == 0, QStringLiteral("node store removes by id"));

    ArtifactCompositionInitParams compositionParams(
        QStringLiteral("Container Migration Test"),
        FloatColor{0.1f, 0.1f, 0.1f, 1.0f});
    auto composition = ArtifactCore::makeShared<ArtifactAbstractComposition>(
        CompositionID(QStringLiteral("container-migration-test")), compositionParams);
    ArtifactLayerFactory factory;
    auto layerResult = factory.createLayer(
        ArtifactLayerInitParams(QStringLiteral("Child"), LayerType::Null));
    auto migrationContainer = std::make_unique<ArtifactContainerNode>();
    const auto migrationContainerId = migrationContainer->id();
    check(composition && layerResult.layer &&
              composition->appendLayerTop(layerResult.layer).success,
          QStringLiteral("composition accepts migration test layer"));
    check(composition->addCompositionNode(std::move(migrationContainer)),
          QStringLiteral("composition stores independent container"));
    check(composition->addLayerToCompositionContainer(
              layerResult.layer->id(), migrationContainerId),
          QStringLiteral("composition links layer id to container"));
    auto linkedContainer = dynamic_cast<ArtifactContainerNode*>(
        composition->compositionNodeById(migrationContainerId));
    check(linkedContainer && linkedContainer->containsChild(layerResult.layer->id()),
          QStringLiteral("composition exposes linked child"));
    check(composition->removeLayerFromCompositionContainer(
              layerResult.layer->id(), migrationContainerId),
          QStringLiteral("composition unlinks layer id from container"));

    return failures;
}
} // namespace Artifact
