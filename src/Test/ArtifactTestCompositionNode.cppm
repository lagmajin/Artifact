module;
#include <QDebug>
#include <QString>

export module Artifact.Test.CompositionNode;

import Artifact.Composition.Node;
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

    return failures;
}
} // namespace Artifact
