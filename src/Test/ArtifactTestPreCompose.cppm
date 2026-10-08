module;

#include <memory>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>

#include <QCoreApplication>
#include <QByteArray>
#include <QColor>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>
#include <QVector>
#include <QVector3D>

export module Artifact.Test.PreCompose;

import Composition.PreCompose;
import Artifact.Layer.InitParams;
import Artifact.Layer.Composition;
import Artifact.Layer.Image;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Artifact.Project;
import Composition.Registry;
import Artifact.Project.Manager;
import Artifact.Service.Project;
import Artifact.Service.ActiveContext;
import Artifact.Service.Playback;
import Artifact.Layers.Selection.Manager;
import Undo.UndoManager;
import Asset.Manager;
import FloatRGBA;
import Memory.SharedPtr;
import Time.Rational;
import Property.Abstract;
import Utils.Id;

namespace Artifact {

namespace {
struct PreComposeTestReport {
    int failures = 0;

    void check(bool condition, const QString& label)
    {
        if (!condition) {
            ++failures;
            qWarning().noquote() << "[PreCompose Test][FAIL]" << label;
        } else {
            qInfo().noquote() << "[PreCompose Test][OK]" << label;
        }
    }
};
} // namespace

export int runPreComposeTests()
{
    PreComposeTestReport report;

    auto& manager = PreComposeManager::instance();
    const QVector<LayerID> emptyLayerIds;

    report.check(!manager.unprecompose(CompositionID(), LayerID()),
                 QStringLiteral("unprecompose rejects nil layer id"));
    report.check(!manager.precompose(CompositionID(), emptyLayerIds).success,
                 QStringLiteral("precompose rejects empty layer list"));
    const CompositionID parentId;
    const CompositionID childId;
    report.check(!manager.canNestComposition(parentId, parentId),
                 QStringLiteral("canNestComposition rejects self nesting"));
    report.check(manager.canNestComposition(parentId, childId),
                 QStringLiteral("canNestComposition allows distinct ids"));
    report.check(manager.getCompositionHierarchy(CompositionID()).size() == 1,
                 QStringLiteral("composition hierarchy includes self only for root-like id"));
    report.check(NestedTimeUtils::convertTime(42.0, parentId, parentId) == 42.0,
                 QStringLiteral("convertTime is identity for same composition"));

    auto precomposeCommand =
        createPrecomposeCommand(parentId, emptyLayerIds, PreComposeOptions::defaults());
    report.check(static_cast<bool>(precomposeCommand),
                 QStringLiteral("createPrecomposeCommand returns a command"));
    if (precomposeCommand) {
        report.check(precomposeCommand->type() == PreComposeCommand::Type::Precompose,
                     QStringLiteral("precompose command type is correct"));
    }

    const LayerID demoLayerA;
    const LayerID demoLayerB;
    const QVector<LayerID> demoLayerIds{demoLayerA, demoLayerB};
    const auto demoPrecomposeResult =
        manager.precompose(parentId, demoLayerIds, PreComposeOptions::defaults());
    report.check(demoPrecomposeResult.success,
                 QStringLiteral("demo precompose for unprecompose command succeeds"));
    auto unprecomposeCommand =
        createUnprecomposeCommand(demoPrecomposeResult.newCompositionId,
                                  demoPrecomposeResult.newLayerId, UnprecomposeOptions{});
    report.check(static_cast<bool>(unprecomposeCommand),
                 QStringLiteral("createUnprecomposeCommand returns a command"));
    if (unprecomposeCommand) {
        report.check(unprecomposeCommand->type() == PreComposeCommand::Type::Unprecompose,
                     QStringLiteral("unprecompose command type is correct"));
        report.check(unprecomposeCommand->execute(),
                     QStringLiteral("core unprecompose command execute succeeds"));
        report.check(!manager.isPrecomposeLayer(demoPrecomposeResult.newLayerId),
                     QStringLiteral("core unprecompose command removes the precompose layer"));
        report.check(unprecomposeCommand->undo(),
                     QStringLiteral("core unprecompose command undo restores the precompose layer"));
        report.check(manager.isPrecomposeLayer(demoPrecomposeResult.newLayerId),
                     QStringLiteral("core unprecompose command undo restores manager mapping"));
        report.check(
            manager.getCompositionHierarchy(demoPrecomposeResult.newCompositionId).size() == 2,
            QStringLiteral("core unprecompose command undo restores nesting hierarchy"));
        report.check(unprecomposeCommand->redo(),
                     QStringLiteral("core unprecompose command redo re-applies unprecompose"));
        manager.setPrecomposeLayerStartFrame(demoPrecomposeResult.newLayerId, 12.0);
        report.check(
            NestedTimeUtils::convertTimeThroughLayerPath(
                30.0, QVector<LayerID>{demoPrecomposeResult.newLayerId}, {}) == 42.0,
            QStringLiteral("explicit layer path converts child time to parent time"));
        report.check(
            NestedTimeUtils::convertTimeThroughLayerPath(
                42.0, {}, QVector<LayerID>{demoPrecomposeResult.newLayerId}) == 30.0,
            QStringLiteral("explicit layer path converts parent time to child time"));
    }

    auto* service = ArtifactProjectService::instance();
    report.check(static_cast<bool>(service), QStringLiteral("project service instance exists"));
    if (service) {
        auto* undoManager = UndoManager::instance();
        if (undoManager) {
            undoManager->clearHistory();
        }

        service->createComposition(UniString(QStringLiteral("Precompose Test")));
        service->addLayerToCurrentComposition(
            ArtifactNullLayerInitParams(QStringLiteral("Precompose Layer A")), false);
        service->addLayerToCurrentComposition(
            ArtifactNullLayerInitParams(QStringLiteral("Precompose Layer B")), false);

        auto comp = service->currentComposition().lock();
        report.check(static_cast<bool>(comp), QStringLiteral("current composition exists after creation"));
        if (comp) {
            QVector<LayerID> layerIds;
            QVector<LayerID> originalOrder;
            for (const auto& layer : comp->allLayer()) {
                if (layer) {
                    originalOrder.push_back(layer->id());
                }
                if (layer && layer->layerName().startsWith(QStringLiteral("Precompose Layer"))) {
                    layerIds.push_back(layer->id());
                }
            }

            report.check(layerIds.size() >= 2,
                         QStringLiteral("two test layers were added to the composition"));
            if (layerIds.size() >= 2) {
                layerIds = {layerIds[0], layerIds[1]};
                const int originalIndex0 = originalOrder.indexOf(layerIds[0]);
                const auto firstLayer = comp->layerById(layerIds[0]);
                if (firstLayer) {
                    firstLayer->setPosition3D({12.0f, -8.0f, 3.0f});
                    firstLayer->setOpacity(0.42f);
                }
                const bool precomposed = service->precomposeLayersWithUndo(
                    layerIds, UniString(QStringLiteral("Precompose Child")),
                    false, true, PrecomposeMode::MoveSelected);
                report.check(precomposed, QStringLiteral("precomposeLayersWithUndo succeeds"));
                if (precomposed) {
                    const auto precompOutcome = service->lastPrecomposeOutcome();
                    report.check(!precompOutcome.precompLayerId.isNil(),
                                 QStringLiteral("precompose outcome includes precomp layer"));
                    report.check(!precompOutcome.childCompId.isNil(),
                                 QStringLiteral("precompose outcome includes child comp"));
                    report.check(manager.isPrecomposeLayer(precompOutcome.precompLayerId),
                                 QStringLiteral("manager recognizes precompose layer"));
                    report.check(
                        manager.getSourceCompositionId(precompOutcome.precompLayerId) ==
                            precompOutcome.childCompId,
                        QStringLiteral("precompose layer maps to child composition"));
                    const auto precompLayer =
                        comp->layerById(precompOutcome.precompLayerId);
                    report.check(static_cast<bool>(precompLayer),
                                 QStringLiteral("precompose layer exists in composition"));
                    if (precompLayer) {
                        const double parentFrame =
                            static_cast<double>(precompLayer->startTime().framePosition());
                        report.check(
                            NestedTimeUtils::parentToChildTime(
                                parentFrame, precompOutcome.precompLayerId) == 0.0,
                            QStringLiteral("parent-to-child time honors startTime"));
                        report.check(
                            NestedTimeUtils::childToParentTime(
                                0.0, precompOutcome.precompLayerId) == parentFrame,
                            QStringLiteral("child-to-parent time restores startTime"));
                        report.check(
                            NestedTimeUtils::getRemappedTime(
                                precompOutcome.precompLayerId, parentFrame) == 0.0,
                            QStringLiteral("getRemappedTime matches parent-to-child mapping"));
                    }

                    if (undoManager) {
                        report.check(undoManager->canUndo(),
                                     QStringLiteral("undo stack has precompose command"));
                        undoManager->undo();
                        comp = service->currentComposition().lock();
                        report.check(static_cast<bool>(comp),
                                     QStringLiteral("composition remains available after undo"));
                        if (comp) {
                            QVector<LayerID> undoOrder;
                            for (const auto& layer : comp->allLayer()) {
                                if (layer) {
                                    undoOrder.push_back(layer->id());
                                }
                            }
                            const int undoIndex0 = undoOrder.indexOf(layerIds[0]);
                            const int undoIndex1 = undoOrder.indexOf(layerIds[1]);
                            report.check(undoIndex0 >= 0,
                                         QStringLiteral("first layer restored after undo"));
                            report.check(undoIndex1 >= 0,
                                         QStringLiteral("second layer restored after undo"));
                            report.check(undoIndex0 < undoIndex1,
                                         QStringLiteral("undo restores original layer order"));
                        }
                        report.check(undoManager->canRedo(),
                                     QStringLiteral("redo stack has precompose command"));
                        undoManager->redo();
                        comp = service->currentComposition().lock();
                        report.check(static_cast<bool>(comp),
                                     QStringLiteral("composition remains available after redo"));
                        if (comp) {
                            QVector<LayerID> redoOrder;
                            for (const auto& layer : comp->allLayer()) {
                                if (layer) {
                                    redoOrder.push_back(layer->id());
                                }
                            }
                            const auto precompLayer =
                                comp->layerById(precompOutcome.precompLayerId);
                            report.check(static_cast<bool>(precompLayer),
                                         QStringLiteral("redo restores the precompose layer"));
                            if (precompLayer) {
                                const int redoIndex = redoOrder.indexOf(precompOutcome.precompLayerId);
                                report.check(redoIndex >= 0,
                                             QStringLiteral("redo keeps the precompose layer in the composition"));
                                report.check(redoIndex == originalIndex0,
                                             QStringLiteral("redo restores precompose layer to its original insertion point"));
                                report.check(
                                    manager.getSourceCompositionId(
                                        precompOutcome.precompLayerId) ==
                                        precompOutcome.childCompId,
                                    QStringLiteral(
                                        "redo keeps precompose source composition mapping"));
                            }
                        }
                        report.check(undoManager->canUndo(),
                                     QStringLiteral("undo stack has redo-applied precompose command"));
                        const bool unprecomposed =
                            service->unprecomposeLayerWithUndo(precompOutcome.precompLayerId, true);
                        report.check(unprecomposed,
                                     QStringLiteral("unprecomposeLayerWithUndo succeeds"));
                        comp = service->currentComposition().lock();
                        report.check(static_cast<bool>(comp),
                                     QStringLiteral("composition remains available after unprecompose"));
                        if (comp) {
                            report.check(!comp->layerById(precompOutcome.precompLayerId),
                                         QStringLiteral("unprecompose removes the precompose layer"));
                            const auto restoredFirstLayer = comp->layerById(layerIds[0]);
                            report.check(static_cast<bool>(restoredFirstLayer),
                                         QStringLiteral("first source layer restored after unprecompose"));
                            report.check(static_cast<bool>(comp->layerById(layerIds[1])),
                                         QStringLiteral("second source layer restored after unprecompose"));
                            if (restoredFirstLayer) {
                                const auto position = restoredFirstLayer->position3D();
                                report.check(position.x == 12.0f &&
                                                 position.y == -8.0f &&
                                                 position.z == 3.0f,
                                             QStringLiteral("unprecompose preserves source layer position"));
                                report.check(restoredFirstLayer->opacity() == 0.42f,
                                             QStringLiteral("unprecompose preserves source layer opacity"));
                            }
                        }
                        report.check(service->lastUnprecomposePrecompLayerId() ==
                                         precompOutcome.precompLayerId,
                                     QStringLiteral("unprecompose records the restored precompose layer id"));
                        report.check(service->lastUnprecomposeChildCompId() ==
                                         precompOutcome.childCompId,
                                     QStringLiteral("unprecompose records the restored child composition id"));
                        report.check(service->lastUnprecomposeMovedLayerIds().size() == layerIds.size(),
                                     QStringLiteral("unprecompose records all moved layer ids"));
                        report.check(service->lastUnprecomposeChildName().toQString() ==
                                         QStringLiteral("Precompose Child"),
                                     QStringLiteral("unprecompose records the child composition name"));
                        report.check(undoManager->canUndo(),
                                     QStringLiteral("undo stack has unprecompose command"));
                        report.check(!undoManager->canRedo(),
                                     QStringLiteral("redo stack is cleared after unprecompose"));
                        undoManager->undo();
                        comp = service->currentComposition().lock();
                        report.check(static_cast<bool>(comp),
                                     QStringLiteral("composition remains available after unprecompose undo"));
                        if (comp) {
                            const auto restoredPrecompLayer = comp->layerById(precompOutcome.precompLayerId);
                            report.check(static_cast<bool>(restoredPrecompLayer),
                                          QStringLiteral("undo restores the precompose layer after unprecompose"));
                            report.check(
                                manager.getSourceCompositionId(precompOutcome.precompLayerId) ==
                                    precompOutcome.childCompId,
                                QStringLiteral("undo after unprecompose restores source composition mapping"));
                            report.check(
                                manager.getCompositionHierarchy(precompOutcome.childCompId).size() == 2,
                                QStringLiteral("undo after unprecompose restores child composition hierarchy"));
                            if (restoredPrecompLayer) {
                                const auto restoredFirstLayer = comp->layerById(layerIds[0]);
                                if (restoredFirstLayer) {
                                    const auto position = restoredFirstLayer->position3D();
                                    report.check(position.x == 12.0f &&
                                                     position.y == -8.0f &&
                                                     position.z == 3.0f,
                                                 QStringLiteral("undo after unprecompose keeps source layer position"));
                                    report.check(restoredFirstLayer->opacity() == 0.42f,
                                                 QStringLiteral("undo after unprecompose keeps source layer opacity"));
                                }
                            }
                        }
                        report.check(undoManager->canRedo(),
                                     QStringLiteral("redo stack has unprecompose command after undo"));

                        report.check(undoManager->canUndo(),
                                     QStringLiteral("undo stack has restored precompose command"));
                        undoManager->undo();
                        comp = service->currentComposition().lock();
                        report.check(static_cast<bool>(comp),
                                     QStringLiteral("composition remains available after restoring precompose undo"));
                        if (comp) {
                            const auto restoredPrecompLayer = comp->layerById(precompOutcome.precompLayerId);
                            report.check(static_cast<bool>(restoredPrecompLayer),
                                          QStringLiteral("restoring precompose command recreates the precompose layer"));
                        }

                        const auto childCompBeforeDrop =
                            service->lastPrecomposeOutcome().childCompId;
                        const bool unprecomposedDrop =
                            service->unprecomposeLayerWithUndo(precompOutcome.precompLayerId, false);
                        report.check(unprecomposedDrop,
                                     QStringLiteral("unprecomposeLayerWithUndo with keepComposition=false succeeds"));
                        comp = service->currentComposition().lock();
                        report.check(static_cast<bool>(comp),
                                     QStringLiteral("composition remains available after dropping child composition"));
                        if (comp) {
                            report.check(!comp->layerById(precompOutcome.precompLayerId),
                                         QStringLiteral("keepComposition=false removes the precompose layer"));
                        }
                        report.check(service->findComposition(childCompBeforeDrop).ptr.lock() == nullptr,
                                     QStringLiteral("keepComposition=false removes the child composition"));
                        report.check(undoManager->canUndo(),
                                     QStringLiteral("undo stack has keepComposition=false unprecompose command"));
                        undoManager->undo();
                        comp = service->currentComposition().lock();
                        report.check(static_cast<bool>(comp),
                                     QStringLiteral("composition remains available after keepComposition=false undo"));
                        if (comp) {
                            report.check(static_cast<bool>(comp->layerById(precompOutcome.precompLayerId)),
                                          QStringLiteral("undo restores the precompose layer after keepComposition=false"));
                        }
                        report.check(static_cast<bool>(service->findComposition(childCompBeforeDrop).ptr.lock()),
                                      QStringLiteral("undo restores the child composition after keepComposition=false"));
                    }
                }
            }
        }
    }

    if (service) {
        auto* undoManager = UndoManager::instance();
        if (undoManager) undoManager->clearHistory();
        service->createComposition(UniString(QStringLiteral("Reusable Source")));
        auto reusableSource = service->currentComposition().lock();
        if (reusableSource) {
            service->addLayerToCurrentComposition(
                ArtifactNullLayerInitParams(QStringLiteral("Reusable Source Layer")), false);
            const auto sourceId = reusableSource->id();
            service->createComposition(UniString(QStringLiteral("Reusable Target")));
            auto reusableTarget = service->currentComposition().lock();
            report.check(static_cast<bool>(reusableTarget),
                         QStringLiteral("reusable target composition exists"));
            if (reusableTarget) {
                const bool firstAdd = service->addCompositionLayerToCurrentCompositionWithUndo(sourceId);
                const bool secondAdd = service->addCompositionLayerToCurrentCompositionWithUndo(sourceId);
                report.check(firstAdd && secondAdd,
                             QStringLiteral("same composition can be added twice as a layer"));
                QVector<LayerID> reusableLayerIds;
                for (const auto& layer : reusableTarget->allLayer()) {
                    if (ArtifactCore::dynamicPointerCast<ArtifactCompositionLayer>(layer)) {
                        reusableLayerIds.push_back(layer->id());
                    }
                }
                report.check(reusableLayerIds.size() == 2,
                             QStringLiteral("two reusable composition layer instances exist"));
                if (undoManager && reusableLayerIds.size() == 2) {
                    undoManager->undo();
                    report.check(reusableTarget->layerCount() == 1,
                                 QStringLiteral("undo removes only one reusable instance"));
                    report.check(PreComposeManager::instance()
                                     .getPrecompLayersForChild(sourceId).size() == 1,
                                 QStringLiteral("remaining reusable instance keeps its nesting reference"));
                    undoManager->redo();
                    report.check(reusableTarget->layerCount() == 2,
                                 QStringLiteral("redo restores the removed reusable instance"));
                }
            }
        }
    }

    qInfo().noquote() << "[PreCompose Test] failures:" << report.failures;
    return report.failures;
}

export int runEditSequenceFuzzTests()
{
    constexpr std::array<std::uint64_t, 3> defaultSeeds{
        0x4A17C0DEULL, 0x51A7E123ULL, 0xBADC0FFEEULL};
    constexpr int maximumNullLayers = 32;
    constexpr std::uint64_t maximumPrecomposeOperations = 32;
    constexpr int reloadTimeoutMs = 1500;
    constexpr auto positionPath = "transform.position.x";

    QVector<std::uint64_t> seeds;
    const QString seedOverride = qEnvironmentVariable(
        "ARTIFACT_EDIT_SEQUENCE_FUZZ_SEED").trimmed();
    if (seedOverride.isEmpty()) {
        for (const auto seed : defaultSeeds) seeds.push_back(seed);
    } else {
        bool seedOk = false;
        const auto seed = seedOverride.toULongLong(&seedOk, 0);
        if (!seedOk) {
            qCritical().noquote() << "[EditSequenceFuzz] invalid seed override:" << seedOverride;
            return 1;
        }
        seeds.push_back(seed);
    }

    constexpr int defaultStepsPerSeed = 2048;
    int stepsPerSeed = defaultStepsPerSeed;
    const QString stepsOverride = qEnvironmentVariable(
        "ARTIFACT_EDIT_SEQUENCE_FUZZ_STEPS").trimmed();
    if (!stepsOverride.isEmpty()) {
        bool stepsOk = false;
        const int requestedSteps = stepsOverride.toInt(&stepsOk);
        if (!stepsOk || requestedSteps < 7 || requestedSteps > 1000000) {
            qCritical().noquote()
                << "[EditSequenceFuzz] steps must be an integer from 7 to 1000000:"
                << stepsOverride;
            return 1;
        }
        stepsPerSeed = requestedSteps;
    }

    const QString traceFilePath = qEnvironmentVariable(
        "ARTIFACT_EDIT_SEQUENCE_FUZZ_TRACE_FILE").trimmed();

    int failures = 0;
    auto* service = ArtifactProjectService::instance();
    auto* undoManager = UndoManager::instance();
    auto& projectManager = ArtifactProjectManager::getInstance();
    if (!service || !undoManager) {
        qCritical().noquote() << "[EditSequenceFuzz] required project/undo service is unavailable";
        return 1;
    }
    const auto closeFuzzProject = [&projectManager](QStringList* retainedReferences) {
        if (auto* activeContext = ArtifactActiveContextService::instance()) {
            activeContext->setActiveComposition({});
        }
        if (auto* selection = ArtifactLayerSelectionManager::instance()) {
            selection->clearSelection();
            selection->setActiveComposition({});
        }
        auto project = projectManager.getCurrentProjectSharedPtr();
        if (project) {
            for (auto* root : project->projectItems()) {
                if (!root) continue;
                for (auto* item : root->children) {
                    if (!item || item->type() != eProjectItemType::Composition) continue;
                    const auto* compositionItem = static_cast<const CompositionItem*>(item);
                    auto composition = project->findComposition(compositionItem->compositionId).ptr.lock();
                    if (composition && retainedReferences) {
                        retainedReferences->append(
                            QStringLiteral("%1=%2")
                                .arg(composition->settings().compositionName().toQString())
                                .arg(composition.useCount() - 1));
                    }
                }
            }
            project->removeAllCompositions();
        }
        project = {};
        if (!projectManager.closeCurrentProject() ||
            projectManager.getCurrentProjectSharedPtr()) {
            return false;
        }
        auto* playback = ArtifactPlaybackService::instance();
        return !playback || !playback->currentComposition();
    };

    QTemporaryDir fixtureDirectory;
    if (!fixtureDirectory.isValid()) {
        qCritical().noquote() << "[EditSequenceFuzz] cannot create temporary fixture directory";
        return 1;
    }

    bool traceFileInitialized = false;
    for (const std::uint64_t seed : seeds) {
        QStringList compositionReferenceCounts;
        undoManager->clearHistory();
        if (undoManager->undoCount() != 0 || undoManager->redoCount() != 0) {
            qCritical().noquote()
                << "[EditSequenceFuzz] cannot isolate test history before seed="
                << QString::number(static_cast<qulonglong>(seed), 16);
            return failures + 1;
        }
        std::mt19937_64 random(seed);
        const QString seedText = QString::number(static_cast<qulonglong>(seed), 16);
        QStringList trace;
        std::uint64_t operationCounts[7]{};
        QString assetPath;
        QUuid assetId;
        QFile traceFile;
        if (!traceFilePath.isEmpty()) {
            traceFile.setFileName(traceFilePath);
            const auto traceOpenMode = QIODevice::WriteOnly | QIODevice::Text |
                (traceFileInitialized ? QIODevice::Append : QIODevice::Truncate);
            if (!traceFile.open(traceOpenMode)) {
                qWarning().noquote() << "[EditSequenceFuzz] cannot open trace file:"
                                     << traceFilePath;
            } else {
                traceFileInitialized = true;
                traceFile.write("seed=");
                traceFile.write(seedText.toUtf8());
                traceFile.write("\noperations:\n");
                traceFile.flush();
            }
        }

        const auto fail = [&](int step, const QString& message) {
            ++failures;
            qCritical().noquote()
                << "[EditSequenceFuzz][FAIL] seed=" << seedText
                << "step=" << step << message
                << "recent=" << trace.join(QStringLiteral(" -> "));
            if (traceFile.isOpen()) {
                traceFile.write("failure-step=");
                traceFile.write(QByteArray::number(step));
                traceFile.write("\nmessage=");
                traceFile.write(message.toUtf8());
                traceFile.write("\n");
                traceFile.flush();
            }
        };
        const auto record = [&](int step, const QString& operation) {
            trace.push_back(operation);
            if (trace.size() > 16) trace.pop_front();
            if (traceFile.isOpen()) {
                traceFile.write("step=");
                traceFile.write(QByteArray::number(step));
                traceFile.write(" ", 1);
                traceFile.write(operation.toUtf8());
                traceFile.write("\n", 1);
                traceFile.flush();
            }
        };

        const QString sourceFixturePath = fixtureDirectory.filePath(
            QStringLiteral("asset-%1.png").arg(seedText));
        QImage fixtureImage(8, 8, QImage::Format_RGBA8888);
        fixtureImage.fill(QColor(32, 96, 160, 255));
        if (!fixtureImage.save(sourceFixturePath)) {
            fail(-1, QStringLiteral("cannot write initial still-image fixture"));
            break;
        }

        const QString compositionName =
            QStringLiteral("Edit Sequence Fuzz %1").arg(seedText);
        service->createComposition(UniString(compositionName));
        auto composition = service->currentComposition().lock();
        if (!composition) {
            fail(-1, QStringLiteral("project service did not create a composition"));
            break;
        }
        const CompositionID parentCompositionId = composition->id();

        const QStringList importedPaths =
            service->importAssetsFromPaths({sourceFixturePath});
        if (importedPaths.size() != 1) {
            fail(-1, QStringLiteral("still-image fixture import failed"));
            break;
        }
        assetPath = importedPaths.front();
        ArtifactImageInitParams imageParams(QStringLiteral("FuzzAsset"));
        imageParams.setImagePath(assetPath);
        service->addLayerToCurrentComposition(imageParams, false);
        assetId = ArtifactCore::AssetManager::instance().sourceId(assetPath);
        if (assetId.isNull()) {
            fail(-1, QStringLiteral("imported still image has no registered source id"));
            break;
        }

        for (int initialLayer = 0; initialLayer < 4; ++initialLayer) {
            const QString layerName =
                QStringLiteral("FuzzNull_%1_%2").arg(seedText).arg(initialLayer);
            service->addLayerToCurrentComposition(
                ArtifactNullLayerInitParams(layerName), false);
        }
        std::array<unsigned int, 5> startupOperations{0, 1, 2, 3, 6};
        // Keep one valid instance of every operation before the randomized
        // phase. Removing before precompose preserves enough direct layers,
        // and the following forced undo/redo pair exercises that precompose.

        const auto currentComposition = [&]() {
            auto active = service->currentComposition().lock();
            if (!active || active->id() != parentCompositionId) {
                service->changeCurrentComposition(parentCompositionId);
                active = service->currentComposition().lock();
            }
            return active;
        };
        const auto fuzzLayers = [](const ArtifactCompositionPtr& current) {
            QVector<ArtifactAbstractLayerPtr> result;
            if (!current) return result;
            for (const auto& layer : current->allLayer()) {
                if (layer && layer->layerName().startsWith(QStringLiteral("FuzzNull_"))) {
                    result.push_back(layer);
                }
            }
            return result;
        };
        const auto validateState = [&](int step) {
            const auto current = currentComposition();
            if (!current || current->id() != parentCompositionId) {
                fail(step, QStringLiteral("active composition could not be restored"));
                return false;
            }
            QSet<QString> seenIds;
            bool hasAssetLayer = false;
            for (const auto& layer : current->allLayer()) {
                if (!layer || layer->id().isNil()) {
                    fail(step, QStringLiteral("composition contains a null layer or nil id"));
                    return false;
                }
                const QString id = layer->id().toString();
                if (seenIds.contains(id) || current->layerById(layer->id()) != layer) {
                    fail(step, QStringLiteral("layer id is duplicated or does not resolve"));
                    return false;
                }
                seenIds.insert(id);
                if (layer->layerName() == QStringLiteral("FuzzAsset")) {
                    hasAssetLayer = true;
                    const auto imageLayer =
                        ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer);
                    if (!imageLayer || imageLayer->sourcePath() != assetPath ||
                        imageLayer->sourceAssetId() != assetId ||
                        imageLayer->sourceVersion() !=
                            ArtifactCore::AssetManager::instance().sourceVersion(assetId)) {
                        fail(step, QStringLiteral("image layer asset identity or version is inconsistent"));
                        return false;
                    }
                }
                auto& precomposeManager = PreComposeManager::instance();
                if (precomposeManager.isPrecomposeLayer(layer->id())) {
                    const CompositionID childId =
                        precomposeManager.getSourceCompositionId(layer->id());
                    const auto child = childId.isNil()
                        ? ArtifactCompositionPtr{}
                        : service->findComposition(childId).ptr.lock();
                    if (!child) {
                        fail(step, QStringLiteral("precompose layer has no resolvable source composition"));
                        return false;
                    }

                    bool hasParentReference = false;
                    for (const auto& reference :
                         precomposeManager.getPrecompLayersForChild(childId)) {
                        hasParentReference = hasParentReference ||
                            (reference.parentCompId == parentCompositionId &&
                             reference.precompLayerId == layer->id());
                    }
                    if (!hasParentReference) {
                        fail(step, QStringLiteral("precompose child is missing its parent-layer reference"));
                        return false;
                    }

                    QSet<QString> childLayerIds;
                    for (const auto& childLayer : child->allLayer()) {
                        if (!childLayer || childLayer->id().isNil()) {
                            fail(step, QStringLiteral("nested composition contains a null layer or nil id"));
                            return false;
                        }
                        const QString childLayerId = childLayer->id().toString();
                        if (childLayerIds.contains(childLayerId) ||
                            child->layerById(childLayer->id()) != childLayer) {
                            fail(step, QStringLiteral("nested composition layer id is duplicated or unresolved"));
                            return false;
                        }
                        childLayerIds.insert(childLayerId);
                        if (childLayer->layerName().startsWith(QStringLiteral("FuzzNull_"))) {
                            const auto property = childLayer->getProperty(
                                QString::fromLatin1(positionPath));
                            if (!property) {
                                fail(step, QStringLiteral("nested fuzz layer lost its animated position property"));
                                return false;
                            }
                            const auto keyframes = property->getKeyFrames();
                            for (std::size_t index = 0; index < keyframes.size(); ++index) {
                                if (!keyframes[index].value.isValid() ||
                                    !std::isfinite(keyframes[index].value.toDouble()) ||
                                    (index > 0 && !(keyframes[index - 1].time <
                                                    keyframes[index].time))) {
                                    fail(step, QStringLiteral("nested keyframe values or ordering are invalid"));
                                    return false;
                                }
                            }
                        }
                    }
                }
                if (layer->layerName().startsWith(QStringLiteral("FuzzNull_"))) {
                    const auto property = layer->getProperty(QString::fromLatin1(positionPath));
                    if (!property) {
                        fail(step, QStringLiteral("fuzz layer lost its animated position property"));
                        return false;
                    }
                    const auto keyframes = property->getKeyFrames();
                    for (std::size_t index = 0; index < keyframes.size(); ++index) {
                        if (!keyframes[index].value.isValid() ||
                            !std::isfinite(keyframes[index].value.toDouble()) ||
                            (index > 0 && !(keyframes[index - 1].time < keyframes[index].time))) {
                            fail(step, QStringLiteral("keyframe values or ordering are invalid"));
                            return false;
                        }
                    }
                }
            }
            if (!hasAssetLayer ||
                ArtifactCore::AssetManager::instance().sourceId(assetPath) != assetId) {
                fail(step, QStringLiteral("asset layer or source identity was lost"));
                return false;
            }
            return true;
        };

        const auto reloadAsset = [&](int step) {
            auto& assets = ArtifactCore::AssetManager::instance();
            const std::uint64_t previousVersion = assets.sourceVersion(assetId);
            ArtifactCore::SharedPtr<ArtifactImageLayer> imageLayer;
            for (const auto& layer : currentComposition()->allLayer()) {
                if (layer && layer->layerName() == QStringLiteral("FuzzAsset")) {
                    imageLayer = ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer);
                    break;
                }
            }
            if (!imageLayer) {
                fail(step, QStringLiteral("image layer disappeared before source reload"));
                return false;
            }

            QElapsedTimer timer;
            timer.start();
            ArtifactCore::FloatRGBA previousPixel;
            bool hasPreviousPixels = false;
            while (!hasPreviousPixels && timer.elapsed() < reloadTimeoutMs) {
                const auto& frame = imageLayer->currentFrameBuffer();
                if (!frame.isEmpty() && frame.width() == 8 && frame.height() == 8) {
                    previousPixel = frame.getPixel(0, 0);
                    hasPreviousPixels = true;
                } else {
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                    QThread::msleep(2);
                }
            }
            if (!hasPreviousPixels) {
                fail(step, QStringLiteral("image layer did not provide its current decoded pixels"));
                return false;
            }

            QImage changedImage(8, 8, QImage::Format_RGBA8888);
            const QColor color = previousPixel.r() > 0.5f
                ? QColor(24, 245, 48, 255) : QColor(245, 24, 48, 255);
            changedImage.fill(color);
            if (!changedImage.save(assetPath)) {
                fail(step, QStringLiteral("cannot update the still-image fixture"));
                return false;
            }

            timer.restart();
            while (assets.sourceVersion(assetId) <= previousVersion &&
                   timer.elapsed() < reloadTimeoutMs) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QThread::msleep(2);
            }
            if (assets.sourceVersion(assetId) <= previousVersion) {
                fail(step, QStringLiteral("file watcher did not invalidate the asset source in time"));
                return false;
            }

            bool decodedChangedPixels = false;
            timer.restart();
            while (!decodedChangedPixels && timer.elapsed() < reloadTimeoutMs) {
                const auto& frame = imageLayer->currentFrameBuffer();
                if (!frame.isEmpty() && frame.width() == changedImage.width() &&
                    frame.height() == changedImage.height()) {
                    const auto pixel = frame.getPixel(0, 0);
                    constexpr float minimumDifference = 0.05f;
                    decodedChangedPixels =
                        std::abs(pixel.r() - previousPixel.r()) > minimumDifference ||
                        std::abs(pixel.g() - previousPixel.g()) > minimumDifference ||
                        std::abs(pixel.b() - previousPixel.b()) > minimumDifference;
                }
                if (!decodedChangedPixels) {
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                    QThread::msleep(2);
                }
            }
            if (!decodedChangedPixels) {
                fail(step, QStringLiteral("image layer did not decode the changed asset pixels"));
                return false;
            }
            return true;
        };

        for (int step = 0; step < stepsPerSeed; ++step) {
            unsigned int operation = step < static_cast<int>(startupOperations.size())
                ? startupOperations[static_cast<std::size_t>(step)]
                : step == static_cast<int>(startupOperations.size()) ? 4u
                : step == static_cast<int>(startupOperations.size()) + 1 ? 5u
                : static_cast<unsigned int>(random() % 7u);
            if (operation == 6 && operationCounts[6] >= 8) {
                operation = static_cast<unsigned int>(random() % 6u);
            }

            if (operation == 6) {
                record(step, QStringLiteral("asset-reload:%1").arg(assetId.toString()));
                ++operationCounts[6];
                if (!reloadAsset(step)) break;
            } else {
                const auto current = currentComposition();
                if (!current) {
                    fail(step, QStringLiteral("active composition is unavailable"));
                    break;
                }

                auto layers = fuzzLayers(current);
                if (operation == 0 && layers.size() >= maximumNullLayers) operation = 1;
                if (operation == 1 && layers.size() <= 2) operation = 0;
                if (operation == 3 &&
                    operationCounts[3] >= maximumPrecomposeOperations) operation = 2;
                if ((operation == 2 && layers.isEmpty()) ||
                    (operation == 3 && layers.size() < 2)) operation = 0;

                if (operation == 0) {
                    ++operationCounts[0];
                    const QString name = QStringLiteral("FuzzNull_%1_%2")
                        .arg(seedText).arg(step);
                    const int layerCountBefore = current->allLayer().size();
                    record(step, QStringLiteral("add-layer:%1").arg(name));
                    service->addLayerToCurrentComposition(
                        ArtifactNullLayerInitParams(name), false);
                    const auto afterAdd = currentComposition();
                    bool addedLayerFound = false;
                    if (afterAdd) {
                        for (const auto& candidate : afterAdd->allLayer()) {
                            addedLayerFound = addedLayerFound ||
                                (candidate && candidate->layerName() == name);
                        }
                    }
                    if (!afterAdd || afterAdd->allLayer().size() != layerCountBefore + 1 ||
                        !addedLayerFound) {
                        fail(step, QStringLiteral("layer add did not produce exactly one requested layer"));
                        break;
                    }
                } else if (operation == 1) {
                    ++operationCounts[1];
                    const int index = static_cast<int>(random() %
                                                       static_cast<std::uint64_t>(layers.size()));
                    const LayerID removedLayerId = layers[index]->id();
                    const int layerCountBefore = current->allLayer().size();
                    record(step, QStringLiteral("remove-layer:%1")
                               .arg(removedLayerId.toString()));
                    if (!service->removeLayerFromComposition(parentCompositionId,
                                                            removedLayerId)) {
                        fail(step, QStringLiteral("valid layer removal was rejected"));
                        break;
                    }
                    const auto afterRemove = currentComposition();
                    if (!afterRemove || afterRemove->layerById(removedLayerId) ||
                        afterRemove->allLayer().size() + 1 != layerCountBefore) {
                        fail(step, QStringLiteral("layer removal did not remove exactly the requested layer"));
                        break;
                    }
                } else if (operation == 2) {
                    ++operationCounts[2];
                    const int index = static_cast<int>(random() %
                                                       static_cast<std::uint64_t>(layers.size()));
                    auto layer = layers[index];
                    auto property = layer->getProperty(QString::fromLatin1(positionPath));
                    if (!property) {
                        fail(step, QStringLiteral("transform.position.x property is unavailable"));
                        break;
                    }
                    const auto before = property->getKeyFrames();
                    auto after = before;
                    ArtifactCore::KeyFrame key;
                    const qint64 keyFrame = static_cast<qint64>(random() % 2400u);
                    key.time = ArtifactCore::RationalTime(keyFrame, 24);
                    key.value = static_cast<double>(static_cast<std::int64_t>(random() % 20001u) - 10000);
                    record(step, QStringLiteral("insert-keyframe:%1:frame=%2")
                               .arg(layer->id().toString())
                               .arg(keyFrame));
                    const auto existing = std::find_if(
                        after.begin(), after.end(), [&](const ArtifactCore::KeyFrame& candidate) {
                            return candidate.time == key.time;
                        });
                    if (existing == after.end()) after.push_back(key);
                    else *existing = key;
                    std::sort(after.begin(), after.end(),
                              [](const auto& left, const auto& right) {
                                  return left.time < right.time;
                              });
                    if (!undoManager->push(std::make_unique<SetLayerPropertyKeyframesCommand>(
                            layer, QString::fromLatin1(positionPath), before, after))) {
                        fail(step, QStringLiteral("valid keyframe command was rejected"));
                        break;
                    }
                    if (!property->hasKeyFrameAt(key.time) ||
                        property->interpolateValue(key.time).toDouble() != key.value.toDouble()) {
                        fail(step, QStringLiteral("keyframe command did not apply its authored value"));
                        break;
                    }
                } else if (operation == 3) {
                    ++operationCounts[3];
                    if (layers.size() >= 2) {
                        const int layerCountBefore = current->allLayer().size();
                        const int firstIndex = static_cast<int>(random() %
                            static_cast<std::uint64_t>(layers.size()));
                        int secondIndex = static_cast<int>(random() %
                            static_cast<std::uint64_t>(layers.size() - 1));
                        if (secondIndex >= firstIndex) ++secondIndex;
                        const QVector<LayerID> ids{layers[firstIndex]->id(),
                                                   layers[secondIndex]->id()};
                        record(step, QStringLiteral("precompose:%1,%2")
                                   .arg(ids[0].toString(), ids[1].toString()));
                        if (!service->precomposeLayersWithUndo(
                                ids, UniString(QStringLiteral("Fuzz Nest %1").arg(step)),
                                false, true, PrecomposeMode::MoveSelected)) {
                            fail(step, QStringLiteral("valid precompose request failed"));
                            break;
                        }
                        const auto afterPrecompose = currentComposition();
                        if (!afterPrecompose ||
                            afterPrecompose->allLayer().size() + 1 != layerCountBefore) {
                            fail(step, QStringLiteral("precompose did not replace two layers with one nest layer"));
                            break;
                        }
                    }
                } else {
                    const bool doUndo = operation == 4;
                    record(step, QStringLiteral("%1:undo=%2,redo=%3")
                               .arg(doUndo ? QStringLiteral("undo") : QStringLiteral("redo"))
                               .arg(static_cast<qulonglong>(undoManager->undoCount()))
                               .arg(static_cast<qulonglong>(undoManager->redoCount())));
                    ++operationCounts[operation];
                    const bool requireHistoryTransition =
                        step == static_cast<int>(startupOperations.size()) ||
                        step == static_cast<int>(startupOperations.size()) + 1;
                    if (doUndo) {
                        if (!undoManager->canUndo()) {
                            if (requireHistoryTransition) {
                                fail(step, QStringLiteral("startup undo operation has no history"));
                                break;
                            }
                        } else {
                            const size_t undoBefore = undoManager->undoCount();
                            const size_t redoBefore = undoManager->redoCount();
                            undoManager->undo();
                            if (undoManager->undoCount() + 1 != undoBefore ||
                                undoManager->redoCount() != redoBefore + 1) {
                                fail(step, QStringLiteral("undo did not move one command to the redo stack"));
                                break;
                            }
                        }
                    } else if (!undoManager->canRedo()) {
                        if (requireHistoryTransition) {
                            fail(step, QStringLiteral("startup redo operation has no history"));
                            break;
                        }
                    } else {
                        const size_t undoBefore = undoManager->undoCount();
                        const size_t redoBefore = undoManager->redoCount();
                        undoManager->redo();
                        if (undoManager->redoCount() + 1 != redoBefore ||
                            undoManager->undoCount() != undoBefore + 1) {
                            fail(step, QStringLiteral("redo did not move one command to the undo stack"));
                            break;
                        }
                    }
                }
            }

            if (!validateState(step)) break;
            if (step % 256 == 255) {
                undoManager->clearHistory();
                if (undoManager->undoCount() != 0 || undoManager->redoCount() != 0) {
                    fail(step, QStringLiteral("undo history could not be cleared at the checkpoint"));
                    break;
                }
            }
        }

        if (failures != 0) break;
        for (unsigned int operation = 0; operation < 7; ++operation) {
            if (operationCounts[operation] == 0) {
                fail(stepsPerSeed, QStringLiteral("seed did not exercise every edit operation"));
                break;
            }
        }
        undoManager->clearHistory();
        compositionReferenceCounts.append(
            QStringLiteral("history-after-clear=undo:%1,redo:%2,pending:%3")
                .arg(static_cast<qulonglong>(undoManager->undoCount()))
                .arg(static_cast<qulonglong>(undoManager->redoCount()))
                .arg(undoManager->hasPendingCollaborativeOperation() ? 1 : 0));
        composition = {};
        const bool projectClosed = closeFuzzProject(&compositionReferenceCounts);
        const auto registeredCompositions =
            ArtifactCore::CompositionRegistry::global().registeredNames();
        if (!projectClosed) {
            fail(stepsPerSeed,
                 QStringLiteral("seed cleanup left an active project or playback composition"));
        } else if (!registeredCompositions.isEmpty() && traceFile.isOpen()) {
            traceFile.write("seed-cleanup-retained-registry=");
            traceFile.write(registeredCompositions.join(QStringLiteral(", ")).toUtf8());
            traceFile.write("\nseed-cleanup-reference-counts=");
            traceFile.write(compositionReferenceCounts.join(QStringLiteral(", ")).toUtf8());
            traceFile.write("\n");
            traceFile.flush();
        } else if (traceFile.isOpen()) {
            traceFile.write("seed-project-cleanup-complete\n");
            traceFile.flush();
        }
        if (failures != 0) break;
        if (traceFile.isOpen()) {
            traceFile.write("seed-validation-complete\n");
            traceFile.flush();
        }
        qInfo().noquote() << "[EditSequenceFuzz] seed=" << seedText
                          << "steps=" << stepsPerSeed
                          << "operations=" << operationCounts[0] << operationCounts[1]
                          << operationCounts[2] << operationCounts[3] << operationCounts[4]
                          << operationCounts[5] << operationCounts[6];
    }

    undoManager->clearHistory();
    if (!closeFuzzProject(nullptr)) {
        ++failures;
        qCritical().noquote()
            << "[EditSequenceFuzz] final project cleanup failed";
    }
    if (!traceFilePath.isEmpty()) {
        QFile traceFile(traceFilePath);
        if (traceFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            traceFile.write("runner-history-cleanup-complete\n");
            traceFile.flush();
        }
    }
    qInfo().noquote() << "[EditSequenceFuzz] failures:" << failures;
    if (!traceFilePath.isEmpty()) {
        QFile traceFile(traceFilePath);
        if (traceFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            traceFile.write("runner-returning\n");
            traceFile.flush();
        }
    }
    return failures;
}

} // namespace Artifact
