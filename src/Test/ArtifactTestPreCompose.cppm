module;

#include <memory>

#include <QDebug>
#include <QString>
#include <QVector3D>

export module Artifact.Test.PreCompose;

import Composition.PreCompose;
import Artifact.Layer.InitParams;
import Artifact.Service.Project;
import Undo.UndoManager;

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
                if (layer && layer->layerName().toQString().startsWith(QStringLiteral("Precompose Layer"))) {
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
                    firstLayer->setPosition3D(QVector3D(12.0f, -8.0f, 3.0f));
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
                            report.check(restoredFirstLayer,
                                         QStringLiteral("first source layer restored after unprecompose"));
                            report.check(comp->layerById(layerIds[1]),
                                         QStringLiteral("second source layer restored after unprecompose"));
                            if (restoredFirstLayer) {
                                report.check(restoredFirstLayer->position3D() ==
                                                 QVector3D(12.0f, -8.0f, 3.0f),
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
                            report.check(restoredPrecompLayer,
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
                                    report.check(restoredFirstLayer->position3D() ==
                                                     QVector3D(12.0f, -8.0f, 3.0f),
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
                            report.check(restoredPrecompLayer,
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
                            report.check(comp->layerById(precompOutcome.precompLayerId),
                                         QStringLiteral("undo restores the precompose layer after keepComposition=false"));
                        }
                        report.check(service->findComposition(childCompBeforeDrop).ptr.lock(),
                                     QStringLiteral("undo restores the child composition after keepComposition=false"));
                    }
                }
            }
        }
    }

    qInfo().noquote() << "[PreCompose Test] failures:" << report.failures;
    return report.failures;
}

} // namespace Artifact
