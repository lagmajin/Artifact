module;
#include <algorithm>
#include <memory>
#include <utility>

#include <QJsonArray>
#include <QFileInfo>
#include <QColor>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVariant>

export module Artifact.AI.WorkspaceAutomation;

import std;
import Core.AI.Describable;
import Artifact.Application.Manager;
import Artifact.Project;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layers.Selection.Manager;
import Artifact.Layer.InitParams;
import Artifact.Project.Manager;
import Artifact.Project.Items;
import Artifact.Render.Queue.Service;
import Artifact.Service.Project;
import Utils.String.UniString;

export namespace Artifact {

class WorkspaceAutomation : public ArtifactCore::IDescribable {
public:
    static void ensureRegistered()
    {
        static const bool registered = []() {
            ArtifactCore::DescriptionRegistry::instance().registerDescribable(
                QStringLiteral("WorkspaceAutomation"),
                []() -> const ArtifactCore::IDescribable* {
                    return &WorkspaceAutomation::instance();
                });
            return true;
        }();
        (void)registered;
    }

    static WorkspaceAutomation& instance()
    {
        static WorkspaceAutomation automation;
        return automation;
    }

    QString className() const override { return QStringLiteral("WorkspaceAutomation"); }

    ArtifactCore::LocalizedText briefDescription() const override
    {
        return ArtifactCore::IDescribable::loc(
            "Provides workspace snapshots and safe project, layer, asset, and render queue actions.",
            "Provides workspace snapshots and safe project, layer, asset, and render queue actions.",
            {});
    }

    ArtifactCore::LocalizedText detailedDescription() const override
    {
        return ArtifactCore::IDescribable::loc(
            "This tool host exposes a compact AI automation surface for ArtifactStudio. "
            "It can inspect the current project state, list compositions and layers, "
            "import assets, create compositions, edit layer order and names, and control "
            "the render queue through existing application services.",
            "This tool host exposes a compact AI automation surface for ArtifactStudio. "
            "It can inspect the current project state, list compositions and layers, "
            "import assets, create compositions, edit layer order and names, and control "
            "the render queue through existing application services.",
            {});
    }

    QList<ArtifactCore::MethodDescription> methodDescriptions() const override
    {
        using ArtifactCore::IDescribable;
        return {
            {"workspaceSnapshot", IDescribable::loc("Return a combined project, composition, selection, and render queue snapshot.", "Return a combined project, composition, selection, and render queue snapshot.", {}), "QVariantMap"},
            {"projectSnapshot", IDescribable::loc("Return the current project JSON snapshot.", "Return the current project JSON snapshot.", {}), "QVariantMap"},
            {"currentCompositionSnapshot", IDescribable::loc("Return the active composition snapshot.", "Return the active composition snapshot.", {}), "QVariantMap"},
            {"selectionSnapshot", IDescribable::loc("Return the current layer selection snapshot.", "Return the current layer selection snapshot.", {}), "QVariantMap"},
            {"renderQueueSnapshot", IDescribable::loc("Return the render queue snapshot.", "Return the render queue snapshot.", {}), "QVariantMap"},
            {"renderQueueJobByIndex", IDescribable::loc("Return a render queue job snapshot by index.", "Return a render queue job snapshot by index.", {}), "QVariantMap", {QStringLiteral("int")}, {QStringLiteral("jobIndex")}},
            {"renderQueueJobStatusAt", IDescribable::loc("Return the status text for a render queue job by index.", "Return the status text for a render queue job by index.", {}), "QString", {QStringLiteral("int")}, {QStringLiteral("jobIndex")}},
            {"renderQueueJobProgressAt", IDescribable::loc("Return the progress for a render queue job by index.", "Return the progress for a render queue job by index.", {}), "int", {QStringLiteral("int")}, {QStringLiteral("jobIndex")}},
            {"renderQueueJobErrorMessageAt", IDescribable::loc("Return the error message for a render queue job by index.", "Return the error message for a render queue job by index.", {}), "QString", {QStringLiteral("int")}, {QStringLiteral("jobIndex")}},
            {"listCompositions", IDescribable::loc("Return the project composition list.", "Return the project composition list.", {}), "QVariantList"},
            {"listProjectItems", IDescribable::loc("Return the project item tree.", "Return the project item tree.", {}), "QVariantList"},
            {"listCurrentCompositionLayers", IDescribable::loc("Return the active composition layer list.", "Return the active composition layer list.", {}), "QVariantList"},
            {"listRenderQueueJobs", IDescribable::loc("Return the render queue job list.", "Return the render queue job list.", {}), "QVariantList"},
            {"createProject", IDescribable::loc("Create a new project if one is not already open.", "Create a new project if one is not already open.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("projectName")}},
            {"createComposition", IDescribable::loc("Create a composition in the current project.", "Create a composition in the current project.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("int"), QStringLiteral("int")}, {QStringLiteral("name"), QStringLiteral("width"), QStringLiteral("height")}},
            {"changeCurrentComposition", IDescribable::loc("Switch the active composition by id.", "Switch the active composition by id.", {}), "QVariantMap", {QStringLiteral("QString")}, {QStringLiteral("compositionId")}},
            {"importAssetsFromPaths", IDescribable::loc("Import one or more asset paths into the project.", "Import one or more asset paths into the project.", {}), "QVariantMap", {QStringLiteral("QStringList")}, {QStringLiteral("paths")}},
            {"addImageLayerToCurrentComposition", IDescribable::loc("Add an image layer to the active composition.", "Add an image layer to the active composition.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("name"), QStringLiteral("path")}},
            {"addSvgLayerToCurrentComposition", IDescribable::loc("Add an SVG layer to the active composition.", "Add an SVG layer to the active composition.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("name"), QStringLiteral("path")}},
            {"addAudioLayerToCurrentComposition", IDescribable::loc("Add an audio layer to the active composition.", "Add an audio layer to the active composition.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("name"), QStringLiteral("path")}},
            {"addTextLayerToCurrentComposition", IDescribable::loc("Add a text layer to the active composition.", "Add a text layer to the active composition.", {}), "QVariantMap", {QStringLiteral("QString")}, {QStringLiteral("name")}},
            {"addNullLayerToCurrentComposition", IDescribable::loc("Add a null layer to the active composition.", "Add a null layer to the active composition.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("int"), QStringLiteral("int")}, {QStringLiteral("name"), QStringLiteral("width"), QStringLiteral("height")}},
            {"addSolidLayerToCurrentComposition", IDescribable::loc("Add a solid layer to the active composition.", "Add a solid layer to the active composition.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("int"), QStringLiteral("int")}, {QStringLiteral("name"), QStringLiteral("width"), QStringLiteral("height")}},
            {"selectLayer", IDescribable::loc("Select a layer in the active composition.", "Select a layer in the active composition.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"renameLayerInCurrentComposition", IDescribable::loc("Rename a layer in the active composition.", "Rename a layer in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("newName")}},
            {"replaceLayerSourceInCurrentComposition", IDescribable::loc("Replace the media source of a layer in the active composition.", "Replace the media source of a layer in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("sourcePath")}},
            {"duplicateLayerInCurrentComposition", IDescribable::loc("Duplicate a layer in the active composition.", "Duplicate a layer in the active composition.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"moveLayerInCurrentComposition", IDescribable::loc("Move a layer to a new index in the active composition.", "Move a layer to a new index in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("int")}, {QStringLiteral("layerId"), QStringLiteral("newIndex")}},
            {"removeLayerFromCurrentComposition", IDescribable::loc("Remove a layer from the active composition.", "Remove a layer from the active composition.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"setLayerVisibleInCurrentComposition", IDescribable::loc("Toggle layer visibility in the active composition.", "Toggle layer visibility in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("bool")}, {QStringLiteral("layerId"), QStringLiteral("visible")}},
            {"setLayerLockedInCurrentComposition", IDescribable::loc("Toggle layer lock state in the active composition.", "Toggle layer lock state in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("bool")}, {QStringLiteral("layerId"), QStringLiteral("locked")}},
            {"setLayerSoloInCurrentComposition", IDescribable::loc("Toggle layer solo state in the active composition.", "Toggle layer solo state in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("bool")}, {QStringLiteral("layerId"), QStringLiteral("solo")}},
            {"setLayerShyInCurrentComposition", IDescribable::loc("Toggle layer shy state in the active composition.", "Toggle layer shy state in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("bool")}, {QStringLiteral("layerId"), QStringLiteral("shy")}},
            {"setLayerParentInCurrentComposition", IDescribable::loc("Set a layer parent in the active composition.", "Set a layer parent in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("parentLayerId")}},
            {"clearLayerParentInCurrentComposition", IDescribable::loc("Clear a layer parent in the active composition.", "Clear a layer parent in the active composition.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"splitLayerAtCurrentTime", IDescribable::loc("Split a layer at the current composition time cursor.", "Split a layer at the current composition time cursor.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"renameComposition", IDescribable::loc("Rename a composition by id.", "Rename a composition by id.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("compositionId"), QStringLiteral("newName")}},
            {"duplicateComposition", IDescribable::loc("Duplicate a composition by id.", "Duplicate a composition by id.", {}), "QVariantMap", {QStringLiteral("QString")}, {QStringLiteral("compositionId")}},
            {"compositionRemovalConfirmationMessage", IDescribable::loc("Return the confirmation message for deleting a composition.", "Return the confirmation message for deleting a composition.", {}), "QString", {QStringLiteral("QString")}, {QStringLiteral("compositionId")}},
            {"removeCompositionWithRenderQueueCleanup", IDescribable::loc("Remove a composition and clear related render queue jobs.", "Remove a composition and clear related render queue jobs.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("compositionId")}},
            {"removeAllAssets", IDescribable::loc("Remove all imported assets from the project.", "Remove all imported assets from the project.", {}), "bool"},
            {"findProjectItemById", IDescribable::loc("Return a project item snapshot by id.", "Return a project item snapshot by id.", {}), "QVariantMap", {QStringLiteral("QString")}, {QStringLiteral("itemId")}},
            {"projectItemPathById", IDescribable::loc("Return the project item path from root to id.", "Return the project item path from root to id.", {}), "QVariantList", {QStringLiteral("QString")}, {QStringLiteral("itemId")}},
            {"projectItemRemovalConfirmationMessage", IDescribable::loc("Return the confirmation message for deleting a project item by id.", "Return the confirmation message for deleting a project item by id.", {}), "QString", {QStringLiteral("QString")}, {QStringLiteral("itemId")}},
            {"renameProjectItemById", IDescribable::loc("Rename a project item by id.", "Rename a project item by id.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("itemId"), QStringLiteral("newName")}},
            {"moveProjectItemToFolder", IDescribable::loc("Move a project item under a folder by id.", "Move a project item under a folder by id.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("itemId"), QStringLiteral("parentFolderId")}},
            {"createFolderInProject", IDescribable::loc("Create a project folder, optionally under a parent folder.", "Create a project folder, optionally under a parent folder.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("name"), QStringLiteral("parentFolderId")}},
            {"removeProjectItemById", IDescribable::loc("Remove a project item by id.", "Remove a project item by id.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("itemId")}},
            {"relinkFootageByPath", IDescribable::loc("Relink a footage item by its old file path.", "Relink a footage item by its old file path.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("oldFilePath"), QStringLiteral("newFilePath")}},
            {"addRenderQueueForCurrentComposition", IDescribable::loc("Queue the active composition for rendering.", "Queue the active composition for rendering.", {}), "bool"},
            {"addRenderQueueForComposition", IDescribable::loc("Queue a specific composition for rendering.", "Queue a specific composition for rendering.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("compositionId")}},
            {"addAllCompositionsToRenderQueue", IDescribable::loc("Queue every composition in the project.", "Queue every composition in the project.", {}), "int"},
            {"duplicateRenderQueueAt", IDescribable::loc("Duplicate a render queue job by index.", "Duplicate a render queue job by index.", {}), "bool", {QStringLiteral("int")}, {QStringLiteral("jobIndex")}},
            {"moveRenderQueue", IDescribable::loc("Move a render queue job from one index to another.", "Move a render queue job from one index to another.", {}), "bool", {QStringLiteral("int"), QStringLiteral("int")}, {QStringLiteral("fromIndex"), QStringLiteral("toIndex")}},
            {"removeRenderQueueAt", IDescribable::loc("Remove a render queue job by index.", "Remove a render queue job by index.", {}), "bool", {QStringLiteral("int")}, {QStringLiteral("jobIndex")}},
            {"setRenderQueueJobNameAt", IDescribable::loc("Rename a render queue job by index.", "Rename a render queue job by index.", {}), "bool", {QStringLiteral("int"), QStringLiteral("QString")}, {QStringLiteral("jobIndex"), QStringLiteral("name")}},
            {"setRenderQueueJobOutputPathAt", IDescribable::loc("Set a render queue job output path by index.", "Set a render queue job output path by index.", {}), "bool", {QStringLiteral("int"), QStringLiteral("QString")}, {QStringLiteral("jobIndex"), QStringLiteral("outputPath")}},
            {"setRenderQueueJobFrameRangeAt", IDescribable::loc("Set a render queue job frame range by index.", "Set a render queue job frame range by index.", {}), "bool", {QStringLiteral("int"), QStringLiteral("int"), QStringLiteral("int")}, {QStringLiteral("jobIndex"), QStringLiteral("startFrame"), QStringLiteral("endFrame")}},
            {"setRenderQueueJobOutputSettingsAt", IDescribable::loc("Set a render queue job output settings block by index.", "Set a render queue job output settings block by index.", {}), "bool", {QStringLiteral("int"), QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("int"), QStringLiteral("int"), QStringLiteral("double"), QStringLiteral("int")}, {QStringLiteral("jobIndex"), QStringLiteral("outputFormat"), QStringLiteral("codec"), QStringLiteral("codecProfile"), QStringLiteral("width"), QStringLiteral("height"), QStringLiteral("fps"), QStringLiteral("bitrateKbps")}},
            {"setRenderQueueJobIntegratedRenderEnabledAt", IDescribable::loc("Toggle integrated render for a queue job by index.", "Toggle integrated render for a queue job by index.", {}), "bool", {QStringLiteral("int"), QStringLiteral("bool")}, {QStringLiteral("jobIndex"), QStringLiteral("enabled")}},
            {"setRenderQueueJobRenderBackendAt", IDescribable::loc("Set a render queue job render backend by index.", "Set a render queue job render backend by index.", {}), "bool", {QStringLiteral("int"), QStringLiteral("QString")}, {QStringLiteral("jobIndex"), QStringLiteral("backend")}},
            {"setRenderQueueJobAudioSourcePathAt", IDescribable::loc("Set a render queue job audio source by index.", "Set a render queue job audio source by index.", {}), "bool", {QStringLiteral("int"), QStringLiteral("QString")}, {QStringLiteral("jobIndex"), QStringLiteral("path")}},
            {"setRenderQueueJobAudioCodecAt", IDescribable::loc("Set a render queue job audio codec by index.", "Set a render queue job audio codec by index.", {}), "bool", {QStringLiteral("int"), QStringLiteral("QString")}, {QStringLiteral("jobIndex"), QStringLiteral("codec")}},
            {"setRenderQueueJobAudioBitrateKbpsAt", IDescribable::loc("Set a render queue job audio bitrate by index.", "Set a render queue job audio bitrate by index.", {}), "bool", {QStringLiteral("int"), QStringLiteral("int")}, {QStringLiteral("jobIndex"), QStringLiteral("bitrateKbps")}},
            {"resetRenderQueueJobForRerun", IDescribable::loc("Reset a render queue job for rerun by index.", "Reset a render queue job for rerun by index.", {}), "bool", {QStringLiteral("int")}, {QStringLiteral("jobIndex")}},
            {"resetCompletedAndFailedRenderQueueJobsForRerun", IDescribable::loc("Reset completed and failed render queue jobs for rerun.", "Reset completed and failed render queue jobs for rerun.", {}), "int"},
            {"startRenderQueueAt", IDescribable::loc("Start a render queue job by index.", "Start a render queue job by index.", {}), "bool", {QStringLiteral("int")}, {QStringLiteral("jobIndex")}},
            {"pauseRenderQueueAt", IDescribable::loc("Pause a render queue job by index.", "Pause a render queue job by index.", {}), "bool", {QStringLiteral("int")}, {QStringLiteral("jobIndex")}},
            {"cancelRenderQueueAt", IDescribable::loc("Cancel a render queue job by index.", "Cancel a render queue job by index.", {}), "bool", {QStringLiteral("int")}, {QStringLiteral("jobIndex")}},
            {"startAllRenderQueues", IDescribable::loc("Start every queued render job.", "Start every queued render job.", {}), "bool"},
            {"pauseAllRenderQueues", IDescribable::loc("Pause every queued render job.", "Pause every queued render job.", {}), "bool"},
            {"cancelAllRenderQueues", IDescribable::loc("Cancel every queued render job.", "Cancel every queued render job.", {}), "bool"},
            {"removeAllRenderQueues", IDescribable::loc("Clear the render queue.", "Clear the render queue.", {}), "bool"}
        };
    }

    QVariant invokeMethod(QStringView name, const QVariantList& args) override
    {
        if (name == QStringLiteral("workspaceSnapshot")) {
            return workspaceSnapshot();
        }
        if (name == QStringLiteral("projectSnapshot")) {
            return projectSnapshot();
        }
        if (name == QStringLiteral("currentCompositionSnapshot")) {
            return currentCompositionSnapshot();
        }
        if (name == QStringLiteral("selectionSnapshot")) {
            return selectionSnapshot();
        }
        if (name == QStringLiteral("renderQueueSnapshot")) {
            return renderQueueSnapshot();
        }
        if (name == QStringLiteral("renderQueueJobByIndex")) {
            return renderQueueJobByIndex(intArg(args, 0, -1));
        }
        if (name == QStringLiteral("renderQueueJobStatusAt")) {
            return renderQueueJobStatusAt(intArg(args, 0, -1));
        }
        if (name == QStringLiteral("renderQueueJobProgressAt")) {
            return renderQueueJobProgressAt(intArg(args, 0, -1));
        }
        if (name == QStringLiteral("renderQueueJobErrorMessageAt")) {
            return renderQueueJobErrorMessageAt(intArg(args, 0, -1));
        }
        if (name == QStringLiteral("listCompositions")) {
            return listCompositions();
        }
        if (name == QStringLiteral("listProjectItems")) {
            return listProjectItems();
        }
        if (name == QStringLiteral("listCurrentCompositionLayers")) {
            return listCurrentCompositionLayers();
        }
        if (name == QStringLiteral("listRenderQueueJobs")) {
            return listRenderQueueJobs();
        }
        if (name == QStringLiteral("createProject")) {
            return createProject(firstString(args));
        }
        if (name == QStringLiteral("createComposition")) {
            return createComposition(stringArg(args, 0), intArg(args, 1, 1920), intArg(args, 2, 1080));
        }
        if (name == QStringLiteral("changeCurrentComposition")) {
            return changeCurrentComposition(stringArg(args, 0));
        }
        if (name == QStringLiteral("importAssetsFromPaths")) {
            return importAssetsFromPaths(collectStringList(args));
        }
        if (name == QStringLiteral("addImageLayerToCurrentComposition")) {
            return addImageLayerToCurrentComposition(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("addSvgLayerToCurrentComposition")) {
            return addSvgLayerToCurrentComposition(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("addAudioLayerToCurrentComposition")) {
            return addAudioLayerToCurrentComposition(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("addTextLayerToCurrentComposition")) {
            return addTextLayerToCurrentComposition(stringArg(args, 0));
        }
        if (name == QStringLiteral("addNullLayerToCurrentComposition")) {
            return addNullLayerToCurrentComposition(stringArg(args, 0), intArg(args, 1, 1920), intArg(args, 2, 1080));
        }
        if (name == QStringLiteral("addSolidLayerToCurrentComposition")) {
            return addSolidLayerToCurrentComposition(stringArg(args, 0), intArg(args, 1, 1920), intArg(args, 2, 1080));
        }
        if (name == QStringLiteral("selectLayer")) {
            return selectLayer(stringArg(args, 0));
        }
        if (name == QStringLiteral("renameLayerInCurrentComposition")) {
            return renameLayerInCurrentComposition(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("replaceLayerSourceInCurrentComposition")) {
            return replaceLayerSourceInCurrentComposition(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("duplicateLayerInCurrentComposition")) {
            return duplicateLayerInCurrentComposition(stringArg(args, 0));
        }
        if (name == QStringLiteral("moveLayerInCurrentComposition")) {
            return moveLayerInCurrentComposition(stringArg(args, 0), intArg(args, 1, 0));
        }
        if (name == QStringLiteral("removeLayerFromCurrentComposition")) {
            return removeLayerFromCurrentComposition(stringArg(args, 0));
        }
        if (name == QStringLiteral("setLayerVisibleInCurrentComposition")) {
            return setLayerVisibleInCurrentComposition(stringArg(args, 0), boolArg(args, 1, true));
        }
        if (name == QStringLiteral("setLayerLockedInCurrentComposition")) {
            return setLayerLockedInCurrentComposition(stringArg(args, 0), boolArg(args, 1, true));
        }
        if (name == QStringLiteral("setLayerSoloInCurrentComposition")) {
            return setLayerSoloInCurrentComposition(stringArg(args, 0), boolArg(args, 1, true));
        }
        if (name == QStringLiteral("setLayerShyInCurrentComposition")) {
            return setLayerShyInCurrentComposition(stringArg(args, 0), boolArg(args, 1, true));
        }
        if (name == QStringLiteral("setLayerParentInCurrentComposition")) {
            return setLayerParentInCurrentComposition(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("clearLayerParentInCurrentComposition")) {
            return clearLayerParentInCurrentComposition(stringArg(args, 0));
        }
        if (name == QStringLiteral("splitLayerAtCurrentTime")) {
            return splitLayerAtCurrentTime(stringArg(args, 0));
        }
        if (name == QStringLiteral("renameComposition")) {
            return renameComposition(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("duplicateComposition")) {
            return duplicateComposition(stringArg(args, 0));
        }
        if (name == QStringLiteral("compositionRemovalConfirmationMessage")) {
            return compositionRemovalConfirmationMessage(stringArg(args, 0));
        }
        if (name == QStringLiteral("removeCompositionWithRenderQueueCleanup")) {
            return removeCompositionWithRenderQueueCleanup(stringArg(args, 0));
        }
        if (name == QStringLiteral("removeAllAssets")) {
            return removeAllAssets();
        }
        if (name == QStringLiteral("findProjectItemById")) {
            return findProjectItemById(stringArg(args, 0));
        }
        if (name == QStringLiteral("projectItemPathById")) {
            return projectItemPathById(stringArg(args, 0));
        }
        if (name == QStringLiteral("projectItemRemovalConfirmationMessage")) {
            return projectItemRemovalConfirmationMessage(stringArg(args, 0));
        }
        if (name == QStringLiteral("renameProjectItemById")) {
            return renameProjectItemById(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("moveProjectItemToFolder")) {
            return moveProjectItemToFolder(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("createFolderInProject")) {
            return createFolderInProject(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("removeProjectItemById")) {
            return removeProjectItemById(stringArg(args, 0));
        }
        if (name == QStringLiteral("relinkFootageByPath")) {
            return relinkFootageByPath(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("addRenderQueueForCurrentComposition")) {
            return addRenderQueueForCurrentComposition();
        }
        if (name == QStringLiteral("addRenderQueueForComposition")) {
            return addRenderQueueForComposition(stringArg(args, 0));
        }
        if (name == QStringLiteral("addAllCompositionsToRenderQueue")) {
            return addAllCompositionsToRenderQueue();
        }
        if (name == QStringLiteral("duplicateRenderQueueAt")) {
            return duplicateRenderQueueAt(intArg(args, 0, -1));
        }
        if (name == QStringLiteral("moveRenderQueue")) {
            return moveRenderQueue(intArg(args, 0, -1), intArg(args, 1, -1));
        }
        if (name == QStringLiteral("removeRenderQueueAt")) {
            return removeRenderQueueAt(intArg(args, 0, -1));
        }
        if (name == QStringLiteral("setRenderQueueJobNameAt")) {
            return setRenderQueueJobNameAt(intArg(args, 0, -1), stringArg(args, 1));
        }
        if (name == QStringLiteral("setRenderQueueJobOutputPathAt")) {
            return setRenderQueueJobOutputPathAt(intArg(args, 0, -1), stringArg(args, 1));
        }
        if (name == QStringLiteral("setRenderQueueJobFrameRangeAt")) {
            return setRenderQueueJobFrameRangeAt(intArg(args, 0, -1), intArg(args, 1, 0), intArg(args, 2, 0));
        }
        if (name == QStringLiteral("setRenderQueueJobOutputSettingsAt")) {
            return setRenderQueueJobOutputSettingsAt(
                intArg(args, 0, -1),
                stringArg(args, 1),
                stringArg(args, 2),
                stringArg(args, 3),
                intArg(args, 4, 0),
                intArg(args, 5, 0),
                doubleArg(args, 6, 0.0),
                intArg(args, 7, 0));
        }
        if (name == QStringLiteral("setRenderQueueJobIntegratedRenderEnabledAt")) {
            return setRenderQueueJobIntegratedRenderEnabledAt(intArg(args, 0, -1), boolArg(args, 1, true));
        }
        if (name == QStringLiteral("setRenderQueueJobRenderBackendAt")) {
            return setRenderQueueJobRenderBackendAt(intArg(args, 0, -1), stringArg(args, 1));
        }
        if (name == QStringLiteral("setRenderQueueJobAudioSourcePathAt")) {
            return setRenderQueueJobAudioSourcePathAt(intArg(args, 0, -1), stringArg(args, 1));
        }
        if (name == QStringLiteral("setRenderQueueJobAudioCodecAt")) {
            return setRenderQueueJobAudioCodecAt(intArg(args, 0, -1), stringArg(args, 1));
        }
        if (name == QStringLiteral("setRenderQueueJobAudioBitrateKbpsAt")) {
            return setRenderQueueJobAudioBitrateKbpsAt(intArg(args, 0, -1), intArg(args, 1, 0));
        }
        if (name == QStringLiteral("resetRenderQueueJobForRerun")) {
            return resetRenderQueueJobForRerun(intArg(args, 0, -1));
        }
        if (name == QStringLiteral("resetCompletedAndFailedRenderQueueJobsForRerun")) {
            return resetCompletedAndFailedRenderQueueJobsForRerun();
        }
        if (name == QStringLiteral("startRenderQueueAt")) {
            return startRenderQueueAt(intArg(args, 0, -1));
        }
        if (name == QStringLiteral("pauseRenderQueueAt")) {
            return pauseRenderQueueAt(intArg(args, 0, -1));
        }
        if (name == QStringLiteral("cancelRenderQueueAt")) {
            return cancelRenderQueueAt(intArg(args, 0, -1));
        }
        if (name == QStringLiteral("startAllRenderQueues")) {
            return renderQueueStartAll();
        }
        if (name == QStringLiteral("pauseAllRenderQueues")) {
            return renderQueuePauseAll();
        }
        if (name == QStringLiteral("cancelAllRenderQueues")) {
            return renderQueueCancelAll();
        }
        if (name == QStringLiteral("removeAllRenderQueues")) {
            return renderQueueRemoveAll();
        }
        return {};
    }

private:
    static QVariantMap toVariantMap(const QJsonObject& obj)
    {
        QVariantMap map;
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            map.insert(it.key(), toVariant(it.value()));
        }
        return map;
    }

    static QVariantList toVariantList(const QJsonArray& arr)
    {
        QVariantList list;
        for (const QJsonValue& value : arr) {
            list.append(toVariant(value));
        }
        return list;
    }

    static QVariant toVariant(const QJsonValue& value)
    {
        if (value.isObject()) {
            return toVariantMap(value.toObject());
        }
        if (value.isArray()) {
            return toVariantList(value.toArray());
        }
        return value.toVariant();
    }

    static QJsonArray stringListToJsonArray(const QStringList& values)
    {
        QJsonArray arr;
        for (const QString& value : values) {
            arr.append(value);
        }
        return arr;
    }

    static QStringList collectStringList(const QVariantList& args)
    {
        QStringList values;
        if (args.size() == 1) {
            const QVariant& first = args.first();
            const QVariantList nested = first.toList();
            if (!nested.isEmpty()) {
                for (const QVariant& value : nested) {
                    const QString text = value.toString().trimmed();
                    if (!text.isEmpty()) {
                        values.append(text);
                    }
                }
            } else {
                const QStringList direct = first.toStringList();
                if (!direct.isEmpty()) {
                    for (const QString& text : direct) {
                        const QString trimmed = text.trimmed();
                        if (!trimmed.isEmpty()) {
                            values.append(trimmed);
                        }
                    }
                }
            }
        }
        if (values.isEmpty()) {
            for (const QVariant& value : args) {
                const QString text = value.toString().trimmed();
                if (!text.isEmpty()) {
                    values.append(text);
                }
            }
        }
        values.removeDuplicates();
        return values;
    }

    static QString firstString(const QVariantList& args)
    {
        return args.isEmpty() ? QString() : args.first().toString().trimmed();
    }

    static QString stringArg(const QVariantList& args, int index)
    {
        if (index < 0 || index >= args.size()) {
            return {};
        }
        return args.at(index).toString().trimmed();
    }

    static int intArg(const QVariantList& args, int index, int defaultValue)
    {
        if (index < 0 || index >= args.size()) {
            return defaultValue;
        }
        bool ok = false;
        const int value = args.at(index).toInt(&ok);
        return ok ? value : defaultValue;
    }

    static double doubleArg(const QVariantList& args, int index, double defaultValue)
    {
        if (index < 0 || index >= args.size()) {
            return defaultValue;
        }
        bool ok = false;
        const double value = args.at(index).toDouble(&ok);
        return ok ? value : defaultValue;
    }

    static bool boolArg(const QVariantList& args, int index, bool defaultValue)
    {
        if (index < 0 || index >= args.size()) {
            return defaultValue;
        }
        const QVariant& value = args.at(index);
        if (value.typeId() == QMetaType::Bool) {
            return value.toBool();
        }
        const QString text = value.toString().trimmed().toLower();
        if (text == QStringLiteral("true") || text == QStringLiteral("1") ||
            text == QStringLiteral("yes") || text == QStringLiteral("on")) {
            return true;
        }
        if (text == QStringLiteral("false") || text == QStringLiteral("0") ||
            text == QStringLiteral("no") || text == QStringLiteral("off")) {
            return false;
        }
        return defaultValue;
    }

    static QJsonObject layerToJson(const ArtifactAbstractLayerPtr& layer, int index, bool selected)
    {
        QJsonObject obj = layer ? layer->toJson() : QJsonObject{};
        if (layer) {
            obj[QStringLiteral("className")] = layer->className().toQString();
        }
        obj[QStringLiteral("index")] = index;
        obj[QStringLiteral("selected")] = selected;
        return obj;
    }

    static ArtifactProjectManager& projectManager()
    {
        return ArtifactProjectManager::getInstance();
    }

    static std::shared_ptr<ArtifactProject> currentProject()
    {
        auto* app = ArtifactApplicationManager::instance();
        auto* service = app ? app->projectService() : nullptr;
        return service ? service->getCurrentProjectSharedPtr() : std::shared_ptr<ArtifactProject>{};
    }

    static ArtifactCompositionPtr currentComposition()
    {
        auto* app = ArtifactApplicationManager::instance();
        if (app && app->activeContextService()) {
            if (auto comp = app->activeContextService()->activeComposition()) {
                return comp;
            }
        }
        auto* service = app ? app->projectService() : nullptr;
        return service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
    }

    static ArtifactAbstractLayerPtr currentLayer()
    {
        auto* app = ArtifactApplicationManager::instance();
        auto* selection = app ? app->layerSelectionManager() : nullptr;
        return selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    }

    static QJsonObject projectToJson()
    {
        QJsonObject obj;
        const auto project = currentProject();
        auto* app = ArtifactApplicationManager::instance();
        auto* service = app ? app->projectService() : nullptr;
        if (!project) {
            obj[QStringLiteral("available")] = false;
            obj[QStringLiteral("projectPath")] = projectManager().currentProjectPath();
            obj[QStringLiteral("assetsPath")] = projectManager().currentProjectAssetsPath();
            return obj;
        }

        obj = project->toJson();
        obj[QStringLiteral("available")] = true;
        obj[QStringLiteral("projectName")] = service ? service->projectName().toQString() : QString();
        obj[QStringLiteral("projectPath")] = projectManager().currentProjectPath();
        obj[QStringLiteral("assetsPath")] = projectManager().currentProjectAssetsPath();
        obj[QStringLiteral("compositionCount")] = obj.value(QStringLiteral("compositions")).toArray().size();
        obj[QStringLiteral("projectItemCount")] = obj.value(QStringLiteral("projectItems")).toArray().size();
        return obj;
    }

    static QVariantMap projectSnapshot()
    {
        return toVariantMap(projectToJson());
    }

    static QVariantMap currentCompositionSnapshot()
    {
        const auto comp = currentComposition();
        QJsonObject obj;
        if (!comp) {
            obj[QStringLiteral("available")] = false;
            return toVariantMap(obj);
        }

        obj = comp->toJson().object();
        obj[QStringLiteral("available")] = true;
        obj[QStringLiteral("layerCount")] = comp->layerCount();
        return toVariantMap(obj);
    }

    static QVariantMap selectionSnapshot()
    {
        QJsonObject obj;
        auto* app = ArtifactApplicationManager::instance();
        auto* selection = app ? app->layerSelectionManager() : nullptr;
        const auto comp = currentComposition();
        if (!selection) {
            obj[QStringLiteral("available")] = false;
            return toVariantMap(obj);
        }

        const auto selected = selection->selectedLayers();
        QVector<ArtifactAbstractLayerPtr> selectedLayers;
        selectedLayers.reserve(selected.size());
        for (const auto& layer : selected) {
            if (layer) {
                selectedLayers.push_back(layer);
            }
        }
        std::sort(selectedLayers.begin(), selectedLayers.end(), [](const auto& a, const auto& b) {
            return a->id().toString() < b->id().toString();
        });

        QJsonArray layers;
        for (int i = 0; i < selectedLayers.size(); ++i) {
            layers.append(layerToJson(selectedLayers[i], i, true));
        }

        obj[QStringLiteral("available")] = true;
        obj[QStringLiteral("activeCompositionId")] = comp ? comp->id().toString() : QString();
        obj[QStringLiteral("currentLayerId")] = selection->currentLayer() ? selection->currentLayer()->id().toString() : QString();
        obj[QStringLiteral("selectedLayerCount")] = static_cast<int>(selectedLayers.size());
        obj[QStringLiteral("selectedLayers")] = layers;
        return toVariantMap(obj);
    }

    static QVariantMap renderQueueSnapshot()
    {
        QJsonObject obj;
        auto* service = ArtifactRenderQueueService::instance();
        if (!service) {
            obj[QStringLiteral("available")] = false;
            return toVariantMap(obj);
        }

        obj[QStringLiteral("available")] = true;
        obj[QStringLiteral("jobCount")] = service->jobCount();
        obj[QStringLiteral("totalProgress")] = service->getTotalProgress();
        QJsonArray jobs;
        const QJsonArray rawJobs = service->toJson();
        for (int i = 0; i < rawJobs.size(); ++i) {
            if (!rawJobs.at(i).isObject()) {
                continue;
            }
            QJsonObject job = rawJobs.at(i).toObject();
            job[QStringLiteral("index")] = i;
            job[QStringLiteral("status")] = service->jobStatusAt(i);
            job[QStringLiteral("progress")] = service->jobProgressAt(i);
            job[QStringLiteral("errorMessage")] = service->jobErrorMessageAt(i);
            jobs.append(job);
        }
        obj[QStringLiteral("jobs")] = jobs;
        return toVariantMap(obj);
    }

    static QVariantMap renderQueueJobByIndex(int jobIndex)
    {
        QVariantMap result;
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return result;
        }
        const QJsonArray jobs = service->toJson();
        if (jobIndex < 0 || jobIndex >= jobs.size() || !jobs.at(jobIndex).isObject()) {
            return result;
        }
        QJsonObject obj = jobs.at(jobIndex).toObject();
        obj[QStringLiteral("index")] = jobIndex;
        obj[QStringLiteral("status")] = service->jobStatusAt(jobIndex);
        obj[QStringLiteral("progress")] = service->jobProgressAt(jobIndex);
        obj[QStringLiteral("errorMessage")] = service->jobErrorMessageAt(jobIndex);
        return toVariantMap(obj);
    }

    static QVariant renderQueueJobStatusAt(int jobIndex)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return QString();
        }
        return service->jobStatusAt(jobIndex);
    }

    static QVariant renderQueueJobProgressAt(int jobIndex)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return -1;
        }
        return service->jobProgressAt(jobIndex);
    }

    static QVariant renderQueueJobErrorMessageAt(int jobIndex)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return QString();
        }
        return service->jobErrorMessageAt(jobIndex);
    }

    static QVariantMap workspaceSnapshot()
    {
        QVariantMap obj;
        obj.insert(QStringLiteral("project"), projectSnapshot());
        obj.insert(QStringLiteral("selection"), selectionSnapshot());
        obj.insert(QStringLiteral("currentComposition"), currentCompositionSnapshot());
        obj.insert(QStringLiteral("renderQueue"), renderQueueSnapshot());
        return obj;
    }

    static QVariantList listCompositions()
    {
        const QJsonObject project = projectToJson();
        return toVariantList(project.value(QStringLiteral("compositions")).toArray());
    }

    static QVariantList listProjectItems()
    {
        const QJsonObject project = projectToJson();
        return toVariantList(project.value(QStringLiteral("projectItems")).toArray());
    }

    static QVariantList listCurrentCompositionLayers()
    {
        const auto comp = currentComposition();
        if (!comp) {
            return {};
        }
        const auto layers = comp->allLayer();
        QJsonArray arr;
        const auto selected = currentLayer();
        for (int i = 0; i < layers.size(); ++i) {
            arr.append(layerToJson(layers[i], i, selected == layers[i]));
        }
        return toVariantList(arr);
    }

    static QVariantList listRenderQueueJobs()
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service) {
            return {};
        }
        return toVariantList(service->toJson());
    }

    static QVariant createProject(const QString& projectName)
    {
        auto& manager = projectManager();
        const QString name = projectName.trimmed().isEmpty() ? QStringLiteral("Untitled") : projectName.trimmed();
        const auto result = manager.createProject(ArtifactCore::UniString::fromQString(name), false);
        return QVariantMap{{QStringLiteral("isSuccess"), result.isSuccess}};
    }

    static QVariant createComposition(const QString& name, int width, int height)
    {
        auto& manager = projectManager();
        ArtifactCompositionInitParams params;
        const QString compositionName = name.trimmed().isEmpty() ? QStringLiteral("Composition") : name.trimmed();
        params.setCompositionName(UniString(compositionName));
        if (width > 0 && height > 0) {
            params.setResolution(width, height);
        }
        const auto result = manager.createComposition(params);
        QJsonObject obj;
        obj[QStringLiteral("success")] = result.success;
        obj[QStringLiteral("id")] = result.id.toString();
        obj[QStringLiteral("message")] = result.message.toQString();
        return toVariantMap(obj);
    }

    static QVariant changeCurrentComposition(const QString& compositionId)
    {
        auto* app = ArtifactApplicationManager::instance();
        auto* service = app ? app->projectService() : nullptr;
        if (!service) {
            return QVariantMap{{QStringLiteral("success"), false}};
        }
        const auto result = service->changeCurrentComposition(CompositionID(compositionId));
        QJsonObject obj;
        obj[QStringLiteral("success")] = result.success;
        obj[QStringLiteral("message")] = result.message.toQString();
        return toVariantMap(obj);
    }

    static QVariant importAssetsFromPaths(const QStringList& paths)
    {
        auto& manager = projectManager();
        if (!manager.isProjectCreated()) {
            manager.createProject(QStringLiteral("Untitled"), false);
        }
        auto* app = ArtifactApplicationManager::instance();
        auto* service = app ? app->projectService() : nullptr;
        if (!service) {
            return QVariantMap{{QStringLiteral("requestedCount"), static_cast<int>(paths.size())},
                               {QStringLiteral("importedCount"), 0},
                               {QStringLiteral("importedPaths"), QVariantList{}}};
        }

        const QStringList importedPaths = service->importAssetsFromPaths(paths);
        QJsonObject obj;
        obj[QStringLiteral("requestedCount")] = static_cast<int>(paths.size());
        obj[QStringLiteral("importedCount")] = static_cast<int>(importedPaths.size());
        obj[QStringLiteral("importedPaths")] = stringListToJsonArray(importedPaths);
        return toVariantMap(obj);
    }

    static QVariant addImageLayerToCurrentComposition(const QString& name, const QString& path)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return QVariantMap{{QStringLiteral("success"), false}};
        }
        ArtifactImageInitParams params(name.isEmpty() ? QStringLiteral("Image") : name);
        params.setImagePath(path);
        service->addLayerToCurrentComposition(params);
        return QVariantMap{{QStringLiteral("success"), true}};
    }

    static QVariant addSvgLayerToCurrentComposition(const QString& name, const QString& path)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return QVariantMap{{QStringLiteral("success"), false}};
        }
        ArtifactSvgInitParams params(name.isEmpty() ? QStringLiteral("SVG") : name);
        params.setSvgPath(path);
        service->addLayerToCurrentComposition(params);
        return QVariantMap{{QStringLiteral("success"), true}};
    }

    static QVariant addAudioLayerToCurrentComposition(const QString& name, const QString& path)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return QVariantMap{{QStringLiteral("success"), false}};
        }
        ArtifactAudioInitParams params(name.isEmpty() ? QStringLiteral("Audio") : name);
        params.setAudioPath(path);
        service->addLayerToCurrentComposition(params);
        return QVariantMap{{QStringLiteral("success"), true}};
    }

    static QVariant addTextLayerToCurrentComposition(const QString& name)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return QVariantMap{{QStringLiteral("success"), false}};
        }
        ArtifactTextLayerInitParams params(name.isEmpty() ? QStringLiteral("Text") : name);
        service->addLayerToCurrentComposition(params);
        return QVariantMap{{QStringLiteral("success"), true}};
    }

    static QVariant addNullLayerToCurrentComposition(const QString& name, int width, int height)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return QVariantMap{{QStringLiteral("success"), false}};
        }
        ArtifactNullLayerInitParams params(name.isEmpty() ? QStringLiteral("Null") : name);
        params.setWidth(std::max(1, width));
        params.setHeight(std::max(1, height));
        service->addLayerToCurrentComposition(params);
        return QVariantMap{{QStringLiteral("success"), true}};
    }

    static QVariant addSolidLayerToCurrentComposition(const QString& name, int width, int height)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return QVariantMap{{QStringLiteral("success"), false}};
        }
        ArtifactSolidLayerInitParams params(name.isEmpty() ? QStringLiteral("Solid") : name);
        params.setWidth(std::max(1, width));
        params.setHeight(std::max(1, height));
        service->addLayerToCurrentComposition(params);
        return QVariantMap{{QStringLiteral("success"), true}};
    }

    static QVariant setLayerVisibleInCurrentComposition(const QString& layerId, bool visible)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        return service->setLayerVisibleInCurrentComposition(LayerID(layerId), visible);
    }

    static QVariant setLayerLockedInCurrentComposition(const QString& layerId, bool locked)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        return service->setLayerLockedInCurrentComposition(LayerID(layerId), locked);
    }

    static QVariant setLayerSoloInCurrentComposition(const QString& layerId, bool solo)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        return service->setLayerSoloInCurrentComposition(LayerID(layerId), solo);
    }

    static QVariant setLayerShyInCurrentComposition(const QString& layerId, bool shy)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        return service->setLayerShyInCurrentComposition(LayerID(layerId), shy);
    }

    static QVariant setLayerParentInCurrentComposition(const QString& layerId, const QString& parentLayerId)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        return service->setLayerParentInCurrentComposition(LayerID(layerId), LayerID(parentLayerId));
    }

    static QVariant clearLayerParentInCurrentComposition(const QString& layerId)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        return service->clearLayerParentInCurrentComposition(LayerID(layerId));
    }

    static QVariant splitLayerAtCurrentTime(const QString& layerId)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        const auto comp = currentComposition();
        if (!service || !comp) {
            return false;
        }
        const LayerID id(layerId);
        if (id.isNil()) {
            return false;
        }
        service->splitLayerAtCurrentTime(comp->id(), id);
        return true;
    }

    static QVariant selectLayer(const QString& layerId)
    {
        auto* selection = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->layerSelectionManager() : nullptr;
        const auto comp = currentComposition();
        if (!selection || !comp) {
            return false;
        }
        const auto layer = comp->layerById(LayerID(layerId));
        if (!layer) {
            return false;
        }
        selection->selectLayer(layer);
        return true;
    }

    static QVariant renameLayerInCurrentComposition(const QString& layerId, const QString& newName)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        return service->renameLayerInCurrentComposition(LayerID(layerId), newName);
    }

    static QVariant replaceLayerSourceInCurrentComposition(const QString& layerId, const QString& sourcePath)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        const QString trimmed = sourcePath.trimmed();
        if (trimmed.isEmpty()) {
            return false;
        }
        return service->replaceLayerSourceInCurrentComposition(LayerID(layerId), trimmed);
    }

    static QVariant duplicateLayerInCurrentComposition(const QString& layerId)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        return service->duplicateLayerInCurrentComposition(LayerID(layerId));
    }

    static QVariant moveLayerInCurrentComposition(const QString& layerId, int newIndex)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        return service->moveLayerInCurrentComposition(LayerID(layerId), newIndex);
    }

    static QVariant removeLayerFromCurrentComposition(const QString& layerId)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        const auto comp = currentComposition();
        if (!service || !comp) {
            return false;
        }
        return service->removeLayerFromComposition(comp->id(), LayerID(layerId));
    }

    static QVariant renameComposition(const QString& compositionId, const QString& newName)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        return service->renameComposition(CompositionID(compositionId), UniString(newName));
    }

    static QVariant duplicateComposition(const QString& compositionId)
    {
        auto& manager = projectManager();
        const auto result = manager.duplicateComposition(CompositionID(compositionId));
        if (result.success) {
            if (auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr) {
                service->changeCurrentComposition(result.id);
            }
        }
        QJsonObject obj;
        obj[QStringLiteral("success")] = result.success;
        obj[QStringLiteral("id")] = result.id.toString();
        obj[QStringLiteral("message")] = result.message.toQString();
        return toVariantMap(obj);
    }

    static QVariant compositionRemovalConfirmationMessage(const QString& compositionId)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return QString();
        }
        return service->compositionRemovalConfirmationMessage(CompositionID(compositionId));
    }

    static QVariant removeCompositionWithRenderQueueCleanup(const QString& compositionId)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        return service->removeCompositionWithRenderQueueCleanup(CompositionID(compositionId));
    }

    static QVariant removeAllAssets()
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        service->removeAllAssets();
        return true;
    }

    static ProjectItem* findProjectItemRecursive(ProjectItem* item, const QString& itemId)
    {
        if (!item || itemId.trimmed().isEmpty()) {
            return nullptr;
        }
        if (item->id.toString() == itemId) {
            return item;
        }
        for (auto* child : item->children) {
            if (auto* found = findProjectItemRecursive(child, itemId)) {
                return found;
            }
        }
        return nullptr;
    }

    static ProjectItem* findProjectItemByIdPointer(const QString& itemId)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return nullptr;
        }
        const auto roots = service->projectItems();
        for (auto* root : roots) {
            if (auto* found = findProjectItemRecursive(root, itemId)) {
                return found;
            }
        }
        return nullptr;
    }

    static QJsonObject projectItemToJson(const ProjectItem* item)
    {
        QJsonObject obj;
        if (!item) {
            return obj;
        }
        obj[QStringLiteral("name")] = item->name.toQString();
        obj[QStringLiteral("id")] = item->id.toString();
        obj[QStringLiteral("parentId")] = item->parent ? item->parent->id.toString() : QString();
        obj[QStringLiteral("childCount")] = item->children.size();
        switch (item->type()) {
        case eProjectItemType::Folder: {
            obj[QStringLiteral("type")] = QStringLiteral("folder");
            QJsonArray children;
            for (const auto* child : item->children) {
                children.append(projectItemToJson(child));
            }
            obj[QStringLiteral("children")] = children;
            break;
        }
        case eProjectItemType::Footage: {
            obj[QStringLiteral("type")] = QStringLiteral("footage");
            const auto* footage = static_cast<const FootageItem*>(item);
            obj[QStringLiteral("filePath")] = footage->filePath;
            obj[QStringLiteral("filePathExists")] = QFileInfo(footage->filePath).exists();
            break;
        }
        case eProjectItemType::Solid: {
            obj[QStringLiteral("type")] = QStringLiteral("solid");
            const auto* solid = static_cast<const SolidItem*>(item);
            obj[QStringLiteral("color")] = solid->color.name(QColor::HexArgb);
            break;
        }
        case eProjectItemType::Composition: {
            obj[QStringLiteral("type")] = QStringLiteral("composition");
            const auto* compItem = static_cast<const CompositionItem*>(item);
            obj[QStringLiteral("compositionId")] = compItem->compositionId.toString();
            break;
        }
        default:
            obj[QStringLiteral("type")] = QStringLiteral("unknown");
            break;
        }
        return obj;
    }

    static QVariantMap findProjectItemById(const QString& itemId)
    {
        const auto* item = findProjectItemByIdPointer(itemId);
        return toVariantMap(projectItemToJson(item));
    }

    static QVariantList projectItemPathById(const QString& itemId)
    {
        QVariantList path;
        const auto* item = findProjectItemByIdPointer(itemId);
        if (!item) {
            return path;
        }

        QVector<const ProjectItem*> items;
        for (const ProjectItem* current = item; current; current = current->parent) {
            items.push_back(current);
        }
        std::reverse(items.begin(), items.end());

        for (const auto* current : items) {
            path.push_back(toVariantMap(projectItemToJson(current)));
        }
        return path;
    }

    static QVariant projectItemRemovalConfirmationMessage(const QString& itemId)
    {
        const auto* item = findProjectItemByIdPointer(itemId);
        if (!item) {
            return QStringLiteral("Project item not found.");
        }
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return QStringLiteral("Project service unavailable.");
        }
        return service->projectItemRemovalConfirmationMessage(const_cast<ProjectItem*>(item));
    }

    static QVariant moveProjectItemToFolder(const QString& itemId, const QString& parentFolderId)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        auto* item = findProjectItemByIdPointer(itemId);
        auto* parent = findProjectItemByIdPointer(parentFolderId);
        if (!item || !parent || parent->type() != eProjectItemType::Folder) {
            return false;
        }
        return service->moveProjectItem(item, parent);
    }

    static QVariant createFolderInProject(const QString& name, const QString& parentFolderId)
    {
        const QString trimmedName = name.trimmed();
        if (trimmedName.isEmpty()) {
            return false;
        }
        auto project = currentProject();
        if (!project) {
            return false;
        }
        FolderItem* parentFolder = nullptr;
        if (!parentFolderId.trimmed().isEmpty()) {
            auto* parent = findProjectItemByIdPointer(parentFolderId);
            if (!parent || parent->type() != eProjectItemType::Folder) {
                return false;
            }
            parentFolder = static_cast<FolderItem*>(parent);
        }
        project->createFolder(trimmedName, parentFolder);
        return true;
    }

    static QVariant renameProjectItemById(const QString& itemId, const QString& newName)
    {
        const QString trimmedName = newName.trimmed();
        if (trimmedName.isEmpty()) {
            return false;
        }
        auto* item = findProjectItemByIdPointer(itemId);
        if (!item) {
            return false;
        }
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            return service->renameComposition(compItem->compositionId, UniString::fromQString(trimmedName));
        }
        auto project = currentProject();
        if (!project) {
            return false;
        }
        item->name = UniString::fromQString(trimmedName);
        project->projectChanged();
        return true;
    }

    static QVariant removeProjectItemById(const QString& itemId)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        auto* item = findProjectItemByIdPointer(itemId);
        if (!item) {
            return false;
        }
        return service->removeProjectItem(item);
    }

    static QVariant relinkFootageByPath(const QString& oldFilePath, const QString& newFilePath)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        return service->relinkFootageByPath(oldFilePath.trimmed(), newFilePath.trimmed());
    }

    static QVariant addRenderQueueForCurrentComposition()
    {
        auto* service = ArtifactRenderQueueService::instance();
        const auto comp = currentComposition();
        if (!service || !comp) {
            return false;
        }
        service->addRenderQueueForComposition(comp->id(), comp->settings().compositionName().toQString());
        return true;
    }

    static QVariant addRenderQueueForComposition(const QString& compositionId)
    {
        auto* service = ArtifactRenderQueueService::instance();
        auto* projectService = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service || !projectService) {
            return false;
        }
        const auto found = projectService->findComposition(CompositionID(compositionId));
        if (!found.success) {
            return false;
        }
        const auto comp = found.ptr.lock();
        if (!comp) {
            return false;
        }
        service->addRenderQueueForComposition(comp->id(), comp->settings().compositionName().toQString());
        return true;
    }

    static QVariant addAllCompositionsToRenderQueue()
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service) {
            return 0;
        }
        return service->addAllCompositions();
    }

    static QVariant duplicateRenderQueueAt(int jobIndex)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->duplicateRenderQueueAt(jobIndex);
        return true;
    }

    static QVariant moveRenderQueue(int fromIndex, int toIndex)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || fromIndex < 0 || toIndex < 0 || fromIndex >= service->jobCount() || toIndex >= service->jobCount()) {
            return false;
        }
        service->moveRenderQueue(fromIndex, toIndex);
        return true;
    }

    static QVariant removeRenderQueueAt(int jobIndex)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->removeRenderQueueAt(jobIndex);
        return true;
    }

    static QVariant setRenderQueueJobNameAt(int jobIndex, const QString& name)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->setJobNameAt(jobIndex, name);
        return true;
    }

    static QVariant setRenderQueueJobOutputPathAt(int jobIndex, const QString& outputPath)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->setJobOutputPathAt(jobIndex, outputPath);
        return true;
    }

    static QVariant setRenderQueueJobFrameRangeAt(int jobIndex, int startFrame, int endFrame)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->setJobFrameRangeAt(jobIndex, startFrame, endFrame);
        return true;
    }

    static QVariant setRenderQueueJobOutputSettingsAt(int jobIndex,
                                                      const QString& outputFormat,
                                                      const QString& codec,
                                                      const QString& codecProfile,
                                                      int width,
                                                      int height,
                                                      double fps,
                                                      int bitrateKbps)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->setJobOutputSettingsAt(jobIndex, outputFormat, codec, codecProfile, width, height, fps, bitrateKbps);
        return true;
    }

    static QVariant setRenderQueueJobIntegratedRenderEnabledAt(int jobIndex, bool enabled)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->setJobIntegratedRenderEnabledAt(jobIndex, enabled);
        return true;
    }

    static QVariant setRenderQueueJobRenderBackendAt(int jobIndex, const QString& backend)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->setJobRenderBackendAt(jobIndex, backend);
        return true;
    }

    static QVariant setRenderQueueJobAudioSourcePathAt(int jobIndex, const QString& path)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->setJobAudioSourcePathAt(jobIndex, path);
        return true;
    }

    static QVariant setRenderQueueJobAudioCodecAt(int jobIndex, const QString& codec)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->setJobAudioCodecAt(jobIndex, codec);
        return true;
    }

    static QVariant setRenderQueueJobAudioBitrateKbpsAt(int jobIndex, int bitrateKbps)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->setJobAudioBitrateKbpsAt(jobIndex, bitrateKbps);
        return true;
    }

    static QVariant resetRenderQueueJobForRerun(int jobIndex)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->resetJobForRerun(jobIndex);
        return true;
    }

    static QVariant resetCompletedAndFailedRenderQueueJobsForRerun()
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service) {
            return 0;
        }
        return service->resetCompletedAndFailedJobsForRerun();
    }

    static QVariant startRenderQueueAt(int jobIndex)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->startRenderQueueAt(jobIndex);
        return true;
    }

    static QVariant pauseRenderQueueAt(int jobIndex)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->pauseRenderQueueAt(jobIndex);
        return true;
    }

    static QVariant cancelRenderQueueAt(int jobIndex)
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service || jobIndex < 0 || jobIndex >= service->jobCount()) {
            return false;
        }
        service->cancelRenderQueueAt(jobIndex);
        return true;
    }

    static QVariant renderQueueStartAll()
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service) {
            return false;
        }
        service->startAllJobs();
        return true;
    }

    static QVariant renderQueuePauseAll()
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service) {
            return false;
        }
        service->pauseAllJobs();
        return true;
    }

    static QVariant renderQueueCancelAll()
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service) {
            return false;
        }
        service->cancelAllJobs();
        return true;
    }

    static QVariant renderQueueRemoveAll()
    {
        auto* service = ArtifactRenderQueueService::instance();
        if (!service) {
            return false;
        }
        service->removeAllRenderQueues();
        return true;
    }
};

} // namespace Artifact
