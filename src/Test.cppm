module;

#include <QDebug>

#include "Test/ArtifactTestRenderQueue.h"

export module Test;

import Artifact.Test.AIToolBridge;
import Artifact.Test.AdjustmentLayer;
import Artifact.Test.LayerGroup;
import Artifact.Test.TimingEventView;

export namespace Artifact {

int runAllTests()
{
    int failures = 0;

    qInfo().noquote() << "[Test] Running built-in tests";

    ArtifactTestRenderQueue renderQueueTests;
    renderQueueTests.runAllTests();

    failures += runAIToolBridgeTests();
    failures += runAdjustmentLayerTests();
    failures += runLayerGroupTests();

    ArtifactTestTimingEventView timingEventViewTests;
    timingEventViewTests.runAllTests();

    if (failures == 0) {
        qInfo().noquote() << "[Test] All built-in tests passed";
    } else {
        qWarning().noquote() << "[Test] Built-in tests finished with failures:" << failures;
    }

    return failures;
}

} // namespace Artifact
