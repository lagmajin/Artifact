module;
#include <algorithm>
#include <memory>
#include <utility>

#include <QJsonArray>
#include <QFileInfo>
#include <QFile>
#include <QIODevice>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QSettings>
#include <QColor>
#include <QJsonObject>
#include <QJsonValue>
#include <QBuffer>
#include <QImage>
#include <QSet>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVariant>
#include <QCoreApplication>
#include <QEventLoop>
#include <thread>
#include <chrono>

export module Artifact.AI.WorkspaceAutomation;

import std;
import Core.AI.Describable;
import Core.AI.CommandIR;
import Artifact.AI.CommandIRExecutor;
import Artifact.Application.Manager;
import Artifact.Service.ActiveContext;
import Artifact.Project;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Effect.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layers.Selection.Manager;
import Artifact.Layer.InitParams;
import Artifact.Layer.Group;
import Artifact.Project.Manager;
import Artifact.Project.Items;
import Artifact.Render.Queue.Service;
import Artifact.Service.Project;
import Artifact.Service.Effect;
import Artifact.Service.Playback;
import Undo.UndoManager;
import Property.Abstract;
import Artifact.Composition.InOutPoints;
import Artifact.Timeline.KeyframeModel;
import Math.Interpolate;
import Time.Rational;
import Utils.String.UniString;
import Event.Bus;
import Artifact.Event.Types;
import Artifact.Layer.Solid2D;
import Composition.ExportMatrix;

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
            {"workspaceDiagnostics", IDescribable::loc("Return a compact workspace diagnostics summary.", "Return a compact workspace diagnostics summary.", {}), "QVariantMap"},
            {"commandVocabulary", IDescribable::loc("List the supported command IR vocabulary and required fields.", "List the supported command IR vocabulary and required fields.", {}), "QVariantList"},
            {"validateCommand", IDescribable::loc("Validate a command IR request without executing it.", "Validate a command IR request without executing it.", {}), "QVariantMap", {QStringLiteral("QVariantMap")}, {QStringLiteral("command")}},
            {"executeCommand", IDescribable::loc("Execute a validated command IR request through the automation facade.", "Execute a validated command IR request through the automation facade.", {}), "QVariantMap", {QStringLiteral("QVariantMap")}, {QStringLiteral("command")}},
            {"projectSnapshot", IDescribable::loc("Return the current project JSON snapshot.", "Return the current project JSON snapshot.", {}), "QVariantMap"},
            {"currentCompositionSnapshot", IDescribable::loc("Return the active composition snapshot.", "Return the active composition snapshot.", {}), "QVariantMap"},
            {"currentCompositionThumbnailAtFrame", IDescribable::loc("Return a PNG thumbnail for the active composition at a frame.", "Return a PNG thumbnail for the active composition at a frame.", {}), "QVariantMap", {QStringLiteral("int"), QStringLiteral("int"), QStringLiteral("int")}, {QStringLiteral("frameNumber"), QStringLiteral("width"), QStringLiteral("height")}},
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
            {"createProject", IDescribable::loc("Create a new project if one is not already open.", "Create a new project if one is not already open.", {}), "QVariantMap", {QStringLiteral("QString")}, {QStringLiteral("projectName")}},
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
            {"setLayerBlendModeInCurrentComposition", IDescribable::loc("Set a layer blend mode in the active composition.", "Set a layer blend mode in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("blendMode")}},
            {"setLayerOpacityInCurrentComposition", IDescribable::loc("Set a layer opacity in the active composition.", "Set a layer opacity in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("double")}, {QStringLiteral("layerId"), QStringLiteral("opacity")}},
            {"setLayerParentInCurrentComposition", IDescribable::loc("Set a layer parent in the active composition.", "Set a layer parent in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("parentLayerId")}},
            {"clearLayerParentInCurrentComposition", IDescribable::loc("Clear a layer parent in the active composition.", "Clear a layer parent in the active composition.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"splitLayerAtCurrentTime", IDescribable::loc("Split a layer at the current composition time cursor.", "Split a layer at the current composition time cursor.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"renameComposition", IDescribable::loc("Rename a composition by id.", "Rename a composition by id.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("compositionId"), QStringLiteral("newName")}},
            {"duplicateComposition", IDescribable::loc("Duplicate a composition by id.", "Duplicate a composition by id.", {}), "QVariantMap", {QStringLiteral("QString")}, {QStringLiteral("compositionId")}},
            {"getCompositionNote", IDescribable::loc("Get the note text of a composition by id.", "Get the note text of a composition by id.", {}), "QString", {QStringLiteral("QString")}, {QStringLiteral("compositionId")}},
            {"setCompositionNote", IDescribable::loc("Set the note text of a composition by id.", "Set the note text of a composition by id.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("compositionId"), QStringLiteral("note")}},
            {"getLayerNote", IDescribable::loc("Get the note text of a layer in the active composition.", "Get the note text of a layer in the active composition.", {}), "QString", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"setLayerNote", IDescribable::loc("Set the note text of a layer in the active composition.", "Set the note text of a layer in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("note")}},
            {"getLayerPosition", IDescribable::loc("Get the position of a layer in the active composition (X, Y).", "Get the position of a layer in the active composition (X, Y).", {}), "QVariantMap", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"setLayerPosition", IDescribable::loc("Set the position of a layer in the active composition (X, Y).", "Set the position of a layer in the active composition (X, Y).", {}), "bool", {QStringLiteral("QString"), QStringLiteral("double"), QStringLiteral("double")}, {QStringLiteral("layerId"), QStringLiteral("x"), QStringLiteral("y")}},
            {"getLayerScale", IDescribable::loc("Get the scale of a layer in the active composition (X, Y).", "Get the scale of a layer in the active composition (X, Y).", {}), "QVariantMap", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"setLayerScale", IDescribable::loc("Set the scale of a layer in the active composition (X, Y).", "Set the scale of a layer in the active composition (X, Y).", {}), "bool", {QStringLiteral("QString"), QStringLiteral("double"), QStringLiteral("double")}, {QStringLiteral("layerId"), QStringLiteral("sx"), QStringLiteral("sy")}},
            {"getLayerRotation", IDescribable::loc("Get the rotation of a layer in the active composition (degrees).", "Get the rotation of a layer in the active composition (degrees).", {}), "double", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"setLayerRotation", IDescribable::loc("Set the rotation of a layer in the active composition (degrees).", "Set the rotation of a layer in the active composition (degrees).", {}), "bool", {QStringLiteral("QString"), QStringLiteral("double")}, {QStringLiteral("layerId"), QStringLiteral("rotation")}},
            {"getLayerOpacity", IDescribable::loc("Get the opacity of a layer in the active composition (0-100).", "Get the opacity of a layer in the active composition (0-100).", {}), "double", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"setLayerOpacity", IDescribable::loc("Set the opacity of a layer in the active composition (0-100).", "Set the opacity of a layer in the active composition (0-100).", {}), "bool", {QStringLiteral("QString"), QStringLiteral("double")}, {QStringLiteral("layerId"), QStringLiteral("opacity")}},
            {"getLayerEffects", IDescribable::loc("Get the list of effects applied to a layer in the active composition.", "Get the list of effects applied to a layer in the active composition.", {}), "QVariantList", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"addLayerEffect", IDescribable::loc("Add an effect to a layer in the active composition.", "Add an effect to a layer in the active composition.", {}), "QString", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("effectType")}},
            {"removeLayerEffect", IDescribable::loc("Remove an effect from a layer in the active composition.", "Remove an effect from a layer in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("effectId")}},
            {"setLayerEffectParameter", IDescribable::loc("Set an effect parameter on a layer in the active composition.", "Set an effect parameter on a layer in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QVariant")}, {QStringLiteral("layerId"), QStringLiteral("effectId"), QStringLiteral("paramName"), QStringLiteral("value")}},
            {"setLayerEffectEnabled", IDescribable::loc("Enable or disable an effect on a layer in the active composition.", "Enable or disable an effect on a layer in the active composition.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("bool")}, {QStringLiteral("layerId"), QStringLiteral("effectId"), QStringLiteral("enabled")}},
            {"moveLayerEffect", IDescribable::loc("Move an effect within a layer's effect stack.", "Move an effect within a layer's effect stack.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("int")}, {QStringLiteral("layerId"), QStringLiteral("effectId"), QStringLiteral("direction")}},
            {"duplicateLayerEffect", IDescribable::loc("Duplicate an effect on a layer in the active composition.", "Duplicate an effect on a layer in the active composition.", {}), "QString", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("effectId")}},
            {"saveLayerEffectPreset", IDescribable::loc("Save an effect preset from a layer effect to a file.", "Save an effect preset from a layer effect to a file.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("effectId"), QStringLiteral("filePath")}},
            {"loadLayerEffectPreset", IDescribable::loc("Load an effect preset into a layer effect from a file.", "Load an effect preset into a layer effect from a file.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("effectId"), QStringLiteral("filePath")}},
            {"listLayerEffectPresets", IDescribable::loc("List effect preset files in a directory.", "List effect preset files in a directory.", {}), "QVariantList", {QStringLiteral("QString")}, {QStringLiteral("directoryPath")}},
            {"recentLayerEffectPresets", IDescribable::loc("List recently used effect presets.", "List recently used effect presets.", {}), "QVariantList", {QStringLiteral("int")}, {QStringLiteral("limit")}},
            {"setKeyframe", IDescribable::loc("Set a keyframe for a layer property at a specific frame.", "Set a keyframe for a layer property at a specific frame.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("int"), QStringLiteral("double")}, {QStringLiteral("layerId"), QStringLiteral("propertyPath"), QStringLiteral("frameNumber"), QStringLiteral("value")}},
            {"getKeyframes", IDescribable::loc("Get all keyframes for a layer property.", "Get all keyframes for a layer property.", {}), "QVariantList", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("propertyPath")}},
            {"deleteKeyframe", IDescribable::loc("Delete a keyframe for a layer property at a specific frame.", "Delete a keyframe for a layer property at a specific frame.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("int")}, {QStringLiteral("layerId"), QStringLiteral("propertyPath"), QStringLiteral("frameNumber")}},
            {"getLayerKeyframeSummary", IDescribable::loc("Return a summary of keyframed properties for a layer.", "Return a summary of keyframed properties for a layer.", {}), "QVariantMap", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"batchSetKeyframes", IDescribable::loc("Set multiple keyframes for a layer from a JSON array.", "Set multiple keyframes for a layer from a JSON array.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("QVariantList")}, {QStringLiteral("layerId"), QStringLiteral("keyframes")}},
            {"createGroupLayer", IDescribable::loc("Create a new group layer in the active composition.", "Create a new group layer in the active composition.", {}), "QVariantMap", {QStringLiteral("QString")}, {QStringLiteral("name")}},
            {"moveLayersToGroup", IDescribable::loc("Move multiple layers into a group layer.", "Move multiple layers into a group layer.", {}), "QVariantMap", {QStringLiteral("QStringList"), QStringLiteral("QString")}, {QStringLiteral("layerIds"), QStringLiteral("groupLayerId")}},
            {"ungroupLayers", IDescribable::loc("Ungroup all layers in a group layer.", "Ungroup all layers in a group layer.", {}), "QVariantMap", {QStringLiteral("QString")}, {QStringLiteral("groupLayerId")}},
            {"compositionRemovalConfirmationMessage", IDescribable::loc("Return the confirmation message for deleting a composition.", "Return the confirmation message for deleting a composition.", {}), "QString", {QStringLiteral("QString")}, {QStringLiteral("compositionId")}},
            {"removeCompositionWithRenderQueueCleanup", IDescribable::loc("Remove a composition and clear related render queue jobs.", "Remove a composition and clear related render queue jobs.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("compositionId")}},
            {"removeAllAssets", IDescribable::loc("Remove all imported assets from the project.", "Remove all imported assets from the project.", {}), "bool"},
            {"findProjectItemById", IDescribable::loc("Return a project item snapshot by id.", "Return a project item snapshot by id.", {}), "QVariantMap", {QStringLiteral("QString")}, {QStringLiteral("itemId")}},
            {"projectItemPathById", IDescribable::loc("Return the project item path from root to id.", "Return the project item path from root to id.", {}), "QVariantList", {QStringLiteral("QString")}, {QStringLiteral("itemId")}},
            {"projectItemRemovalConfirmationMessage", IDescribable::loc("Return the confirmation message for deleting a project item by id.", "Return the confirmation message for deleting a project item by id.", {}), "QString", {QStringLiteral("QString")}, {QStringLiteral("itemId")}},
            {"renameProjectItemById", IDescribable::loc("Rename a project item by id.", "Rename a project item by id.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("itemId"), QStringLiteral("newName")}},
            {"moveProjectItemToFolder", IDescribable::loc("Move a project item under a folder by id.", "Move a project item under a folder by id.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("itemId"), QStringLiteral("parentFolderId")}},
            {"batchRenameProjectItems", IDescribable::loc("Rename multiple project items from a JSON array.", "Rename multiple project items from a JSON array.", {}), "QVariantMap", {QStringLiteral("QVariantList")}, {QStringLiteral("items")}},
            {"batchMoveProjectItemsToFolder", IDescribable::loc("Move multiple project items into a folder.", "Move multiple project items into a folder.", {}), "QVariantMap", {QStringLiteral("QStringList"), QStringLiteral("QString")}, {QStringLiteral("itemIds"), QStringLiteral("parentFolderId")}},
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
            {"playbackStart", IDescribable::loc("Start playback of the active composition.", "Start playback of the active composition.", {}), "bool"},
            {"playbackPause", IDescribable::loc("Pause playback of the active composition.", "Pause playback of the active composition.", {}), "bool"},
            {"playbackStop", IDescribable::loc("Stop playback and return to start frame.", "Stop playback and return to start frame.", {}), "bool"},
            {"playbackToggle", IDescribable::loc("Toggle between play and pause.", "Toggle between play and pause.", {}), "bool"},
            {"playbackGetState", IDescribable::loc("Get current playback state.", "Get current playback state.", {}), "QString"},
            {"playbackGetCurrentFrame", IDescribable::loc("Get current playhead position in frames.", "Get current playhead position in frames.", {}), "int"},
            {"playbackSetCurrentFrame", IDescribable::loc("Set playhead to specific frame.", "Set playhead to specific frame.", {}), "bool", {QStringLiteral("int")}, {QStringLiteral("frameNumber")}},
            {"playbackNextFrame", IDescribable::loc("Move playhead to next frame.", "Move playhead to next frame.", {}), "bool"},
            {"playbackPreviousFrame", IDescribable::loc("Move playhead to previous frame.", "Move playhead to previous frame.", {}), "bool"},
            {"playbackGoToStart", IDescribable::loc("Move playhead to start of composition.", "Move playhead to start of composition.", {}), "bool"},
            {"playbackGoToEnd", IDescribable::loc("Move playhead to end of composition.", "Move playhead to end of composition.", {}), "bool"},
            {"playbackSetInPoint", IDescribable::loc("Set the playback in point at the current frame.", "Set the playback in point at the current frame.", {}), "bool"},
            {"playbackSetOutPoint", IDescribable::loc("Set the playback out point at the current frame.", "Set the playback out point at the current frame.", {}), "bool"},
            {"playbackClearInPoint", IDescribable::loc("Clear the playback in point.", "Clear the playback in point.", {}), "bool"},
            {"playbackClearOutPoint", IDescribable::loc("Clear the playback out point.", "Clear the playback out point.", {}), "bool"},
            {"playbackClearAllPoints", IDescribable::loc("Clear all playback in and out points.", "Clear all playback in and out points.", {}), "bool"},
            {"playbackGoToNextMarker", IDescribable::loc("Move playhead to the next marker.", "Move playhead to the next marker.", {}), "bool"},
            {"playbackGoToPreviousMarker", IDescribable::loc("Move playhead to the previous marker.", "Move playhead to the previous marker.", {}), "bool"},
            {"playbackGoToNextChapter", IDescribable::loc("Move playhead to the next chapter marker.", "Move playhead to the next chapter marker.", {}), "bool"},
            {"playbackGoToPreviousChapter", IDescribable::loc("Move playhead to the previous chapter marker.", "Move playhead to the previous chapter marker.", {}), "bool"},
            {"playbackAddMarker", IDescribable::loc("Add a comment marker at the current frame.", "Add a comment marker at the current frame.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("comment")}},
            {"playbackAddChapter", IDescribable::loc("Add a chapter marker at the current frame.", "Add a chapter marker at the current frame.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("name")}},
            {"playbackClearAllMarkers", IDescribable::loc("Remove all timeline markers.", "Remove all timeline markers.", {}), "bool"},
            {"playbackGetDuration", IDescribable::loc("Get composition duration in frames.", "Get composition duration in frames.", {}), "int"},
            {"playbackGetFrameRange", IDescribable::loc("Get playback frame range (in/out points).", "Get playback frame range (in/out points).", {}), "QVariantMap"},
            {"playbackSetFrameRange", IDescribable::loc("Set playback frame range (work area).", "Set playback frame range (work area).", {}), "bool", {QStringLiteral("int"), QStringLiteral("int")}, {QStringLiteral("frameStart"), QStringLiteral("frameEnd")}},
            {"playbackGetFrameRate", IDescribable::loc("Get playback frame rate (fps).", "Get playback frame rate (fps).", {}), "double"},
            {"playbackGetSpeed", IDescribable::loc("Get playback speed multiplier.", "Get playback speed multiplier.", {}), "float"},
            {"playbackSetSpeed", IDescribable::loc("Set playback speed multiplier.", "Set playback speed multiplier.", {}), "bool", {QStringLiteral("double")}, {QStringLiteral("speed")}},
            {"playbackGetLooping", IDescribable::loc("Get looping state.", "Get looping state.", {}), "bool"},
            {"playbackSetLooping", IDescribable::loc("Enable or disable looping.", "Enable or disable looping.", {}), "bool", {QStringLiteral("bool")}, {QStringLiteral("enabled")}},
            {"seekTimeline", IDescribable::loc("Alias for playbackSetCurrentFrame.", "Alias for playbackSetCurrentFrame.", {}), "bool", {QStringLiteral("int")}, {QStringLiteral("frameNumber")}},
            {"playTimeline", IDescribable::loc("Alias for playbackStart.", "Alias for playbackStart.", {}), "bool"},
            {"pauseTimeline", IDescribable::loc("Alias for playbackPause.", "Alias for playbackPause.", {}), "bool"},
            {"setWorkArea", IDescribable::loc("Alias for playbackSetFrameRange.", "Alias for playbackSetFrameRange.", {}), "bool", {QStringLiteral("int"), QStringLiteral("int")}, {QStringLiteral("frameStart"), QStringLiteral("frameEnd")}},
            {"exportComposition", IDescribable::loc("Add composition to export queue with specified settings.", "Add composition to export queue with specified settings.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("int"), QStringLiteral("int"), QStringLiteral("double"), QStringLiteral("int")}, {QStringLiteral("compositionId"), QStringLiteral("outputPath"), QStringLiteral("format"), QStringLiteral("codec"), QStringLiteral("width"), QStringLiteral("height"), QStringLiteral("fps"), QStringLiteral("bitrateKbps")}},
            {"exportCurrentComposition", IDescribable::loc("Add active composition to export queue with specified settings.", "Add active composition to export queue with specified settings.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("int"), QStringLiteral("int"), QStringLiteral("double"), QStringLiteral("int")}, {QStringLiteral("outputPath"), QStringLiteral("format"), QStringLiteral("codec"), QStringLiteral("width"), QStringLiteral("height"), QStringLiteral("fps"), QStringLiteral("bitrateKbps")}},
            {"exportCompositionAndWait", IDescribable::loc("Export composition and wait until completed.", "Export composition and wait until completed.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("int"), QStringLiteral("int"), QStringLiteral("double"), QStringLiteral("int")}, {QStringLiteral("compositionId"), QStringLiteral("outputPath"), QStringLiteral("format"), QStringLiteral("codec"), QStringLiteral("width"), QStringLiteral("height"), QStringLiteral("fps"), QStringLiteral("bitrateKbps")}},
            {"exportCurrentCompositionAndWait", IDescribable::loc("Export active composition and wait until completed.", "Export active composition and wait until completed.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("int"), QStringLiteral("int"), QStringLiteral("double"), QStringLiteral("int")}, {QStringLiteral("outputPath"), QStringLiteral("format"), QStringLiteral("codec"), QStringLiteral("width"), QStringLiteral("height"), QStringLiteral("fps"), QStringLiteral("bitrateKbps")}},
            {"getSupportedExportFormats", IDescribable::loc("Get list of supported export file formats.", "Get list of supported export file formats.", {}), "QStringList"},
            {"getDefaultCodecForFormat", IDescribable::loc("Get the default export codec for a given format.", "Get the default export codec for a given format.", {}), "QString", {QStringLiteral("QString")}, {QStringLiteral("format")}},
            {"removeAllRenderQueues", IDescribable::loc("Clear the render queue.", "Clear the render queue.", {}), "bool"},
            {"createSolidLayer", IDescribable::loc("Create a solid 2D layer and append it to the composition.", "Create a solid 2D layer and append it to the composition.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("int"), QStringLiteral("int")}, {QStringLiteral("compositionId"), QStringLiteral("name"), QStringLiteral("colorHex"), QStringLiteral("width"), QStringLiteral("height")}},
            {"replaceLayerSource", IDescribable::loc("Replace a video/audio layer's media source file.", "Replace a video/audio layer's media source file.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("footageItemId")}},
            {"splitLayerAtTime", IDescribable::loc("Split a layer into two layers at the specified frame time.", "Split a layer into two layers at the specified frame time.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("int")}, {QStringLiteral("layerId"), QStringLiteral("frameTime")}},
            {"rippleDeleteLayer", IDescribable::loc("Delete a layer and shift all subsequent layers earlier in time.", "Delete a layer and shift all subsequent layers earlier in time.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("layerId")}},
            {"alignLayersSequentially", IDescribable::loc("Align a list of layers sequentially, end-to-end.", "Align a list of layers sequentially, end-to-end.", {}), "bool", {QStringLiteral("QStringList")}, {QStringLiteral("layerIds")}},
            {"defineTemplateSlot", IDescribable::loc("Define a template slot on a layer.", "Define a template slot on a layer.", {}), "bool", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("layerId"), QStringLiteral("slotName"), QStringLiteral("defaultValue")}},
            {"listTemplateSlots", IDescribable::loc("List all template slots in current composition.", "List all template slots in current composition.", {}), "QVariantList"},
            {"applyTemplateVariation", IDescribable::loc("Apply a template variation to current composition.", "Apply a template variation to current composition.", {}), "bool", {QStringLiteral("QString")}, {QStringLiteral("variationJson")}},
            {"createTemplateFromVariation", IDescribable::loc("Create batch export jobs from variations.", "Create batch export jobs from variations.", {}), "int", {QStringLiteral("QVariantList"), QStringLiteral("QString")}, {QStringLiteral("variations"), QStringLiteral("outputPreset")}},
            {"resolveExportMatrixCell", IDescribable::loc("Resolve one export matrix cell into concrete export settings.", "Resolve one export matrix cell into concrete export settings.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("matrixJson"), QStringLiteral("variantId"), QStringLiteral("presetId"), QStringLiteral("baseOutputPath")}},
            {"createExportMatrixJobs", IDescribable::loc("Create resolved export jobs from an export matrix.", "Create resolved export jobs from an export matrix.", {}), "QVariantList", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("matrixJson"), QStringLiteral("baseOutputPath")}},
            {"queueExportMatrixForCurrentComposition", IDescribable::loc("Queue all enabled export matrix jobs for the active composition.", "Queue all enabled export matrix jobs for the active composition.", {}), "QVariantMap", {QStringLiteral("QString"), QStringLiteral("QString")}, {QStringLiteral("matrixJson"), QStringLiteral("baseOutputPath")}},
            {"listAvailableEffects", IDescribable::loc("List all available effects.", "List all available effects.", {}), "QVariantList"}
        };
    }

    QVariant invokeMethod(QStringView name, const QVariantList& args) override
    {
        if (name == QStringLiteral("workspaceSnapshot")) {
            return workspaceSnapshot();
        }
        if (name == QStringLiteral("workspaceDiagnostics")) {
            return workspaceDiagnostics();
        }
        if (name == QStringLiteral("commandVocabulary")) {
            return commandVocabulary();
        }
        if (name == QStringLiteral("validateCommand")) {
            return validateCommand(args.value(0).toMap());
        }
        if (name == QStringLiteral("executeCommand")) {
            return executeCommand(args.value(0).toMap());
        }
        if (name == QStringLiteral("projectSnapshot")) {
            return projectSnapshot();
        }
        if (name == QStringLiteral("currentCompositionSnapshot")) {
            return currentCompositionSnapshot();
        }
        if (name == QStringLiteral("currentCompositionThumbnailAtFrame")) {
            return currentCompositionThumbnailAtFrame(intArg(args, 0, 0), intArg(args, 1, 256), intArg(args, 2, 144));
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
        if (name == QStringLiteral("setLayerBlendModeInCurrentComposition")) {
            return setLayerBlendModeInCurrentComposition(stringArg(args, 0), args.value(1));
        }
        if (name == QStringLiteral("setLayerOpacityInCurrentComposition")) {
            return setLayerOpacityInCurrentComposition(stringArg(args, 0), doubleArg(args, 1, 1.0));
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
        if (name == QStringLiteral("getCompositionNote")) {
            return getCompositionNote(stringArg(args, 0));
        }
        if (name == QStringLiteral("setCompositionNote")) {
            return setCompositionNote(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("getLayerNote")) {
            return getLayerNote(stringArg(args, 0));
        }
        if (name == QStringLiteral("setLayerNote")) {
            return setLayerNote(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("getLayerPosition")) {
            return getLayerPosition(stringArg(args, 0));
        }
        if (name == QStringLiteral("setLayerPosition")) {
            return setLayerPosition(stringArg(args, 0), doubleArg(args, 1, 0.0), doubleArg(args, 2, 0.0));
        }
        if (name == QStringLiteral("getLayerScale")) {
            return getLayerScale(stringArg(args, 0));
        }
        if (name == QStringLiteral("setLayerScale")) {
            return setLayerScale(stringArg(args, 0), doubleArg(args, 1, 1.0), doubleArg(args, 2, 1.0));
        }
        if (name == QStringLiteral("getLayerRotation")) {
            return getLayerRotation(stringArg(args, 0));
        }
        if (name == QStringLiteral("setLayerRotation")) {
            return setLayerRotation(stringArg(args, 0), doubleArg(args, 1, 0.0));
        }
        if (name == QStringLiteral("getLayerOpacity")) {
            return getLayerOpacity(stringArg(args, 0));
        }
        if (name == QStringLiteral("setLayerOpacity")) {
            return setLayerOpacity(stringArg(args, 0), doubleArg(args, 1, 100.0));
        }
        if (name == QStringLiteral("getLayerEffects")) {
            return getLayerEffects(stringArg(args, 0));
        }
        if (name == QStringLiteral("addLayerEffect")) {
            return addLayerEffect(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("removeLayerEffect")) {
            return removeLayerEffect(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("setLayerEffectParameter")) {
            return setLayerEffectParameter(stringArg(args, 0), stringArg(args, 1), stringArg(args, 2), args.value(3));
        }
        if (name == QStringLiteral("setLayerEffectEnabled")) {
            return setLayerEffectEnabled(stringArg(args, 0), stringArg(args, 1), boolArg(args, 2, true));
        }
        if (name == QStringLiteral("moveLayerEffect")) {
            return moveLayerEffect(stringArg(args, 0), stringArg(args, 1), intArg(args, 2, 0));
        }
        if (name == QStringLiteral("duplicateLayerEffect")) {
            return duplicateLayerEffect(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("saveLayerEffectPreset")) {
            return saveLayerEffectPreset(stringArg(args, 0), stringArg(args, 1), stringArg(args, 2));
        }
        if (name == QStringLiteral("loadLayerEffectPreset")) {
            return loadLayerEffectPreset(stringArg(args, 0), stringArg(args, 1), stringArg(args, 2));
        }
        if (name == QStringLiteral("listLayerEffectPresets")) {
            return listLayerEffectPresets(stringArg(args, 0));
        }
        if (name == QStringLiteral("recentLayerEffectPresets")) {
            return recentLayerEffectPresets(intArg(args, 0, 10));
        }
        if (name == QStringLiteral("setKeyframe")) {
            return setKeyframe(stringArg(args, 0), stringArg(args, 1), intArg(args, 2, 0), doubleArg(args, 3, 0.0));
        }
        if (name == QStringLiteral("getKeyframes")) {
            return getKeyframes(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("deleteKeyframe")) {
            return deleteKeyframe(stringArg(args, 0), stringArg(args, 1), intArg(args, 2, 0));
        }
        if (name == QStringLiteral("getLayerKeyframeSummary")) {
            return getLayerKeyframeSummary(stringArg(args, 0));
        }
        if (name == QStringLiteral("batchSetKeyframes")) {
            return batchSetKeyframes(stringArg(args, 0), args.value(1).toList());
        }
        if (name == QStringLiteral("createGroupLayer")) {
            return createGroupLayer(stringArg(args, 0));
        }
        if (name == QStringLiteral("moveLayersToGroup")) {
            return moveLayersToGroup(collectStringList(args), stringArg(args, 1));
        }
        if (name == QStringLiteral("ungroupLayers")) {
            return ungroupLayers(stringArg(args, 0));
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
        if (name == QStringLiteral("batchRenameProjectItems")) {
            return batchRenameProjectItems(args.value(0).toList());
        }
        if (name == QStringLiteral("batchMoveProjectItemsToFolder")) {
            return batchMoveProjectItemsToFolder(args.value(0).toStringList(), stringArg(args, 1));
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
        if (name == QStringLiteral("playbackStart")) {
            return playbackStart();
        }
        if (name == QStringLiteral("playbackPause")) {
            return playbackPause();
        }
        if (name == QStringLiteral("playbackStop")) {
            return playbackStop();
        }
        if (name == QStringLiteral("playbackToggle")) {
            return playbackToggle();
        }
        if (name == QStringLiteral("playbackGetState")) {
            return playbackGetState();
        }
        if (name == QStringLiteral("playbackGetCurrentFrame")) {
            return playbackGetCurrentFrame();
        }
        if (name == QStringLiteral("playbackSetCurrentFrame")) {
            if (args.isEmpty()) return false;
            return playbackSetCurrentFrame(intArg(args, 0, 0));
        }
        if (name == QStringLiteral("playbackNextFrame")) {
            return playbackNextFrame();
        }
        if (name == QStringLiteral("playbackPreviousFrame")) {
            return playbackPreviousFrame();
        }
        if (name == QStringLiteral("playbackGoToStart")) {
            return playbackGoToStart();
        }
        if (name == QStringLiteral("playbackGoToEnd")) {
            return playbackGoToEnd();
        }
        if (name == QStringLiteral("playbackSetInPoint")) {
            return playbackSetInPoint();
        }
        if (name == QStringLiteral("playbackSetOutPoint")) {
            return playbackSetOutPoint();
        }
        if (name == QStringLiteral("playbackClearInPoint")) {
            return playbackClearInPoint();
        }
        if (name == QStringLiteral("playbackClearOutPoint")) {
            return playbackClearOutPoint();
        }
        if (name == QStringLiteral("playbackClearAllPoints")) {
            return playbackClearAllPoints();
        }
        if (name == QStringLiteral("playbackGoToNextMarker")) {
            return playbackGoToNextMarker();
        }
        if (name == QStringLiteral("playbackGoToPreviousMarker")) {
            return playbackGoToPreviousMarker();
        }
        if (name == QStringLiteral("playbackGoToNextChapter")) {
            return playbackGoToNextChapter();
        }
        if (name == QStringLiteral("playbackGoToPreviousChapter")) {
            return playbackGoToPreviousChapter();
        }
        if (name == QStringLiteral("playbackAddMarker")) {
            return playbackAddMarker(stringArg(args, 0));
        }
        if (name == QStringLiteral("playbackAddChapter")) {
            return playbackAddChapter(stringArg(args, 0));
        }
        if (name == QStringLiteral("playbackClearAllMarkers")) {
            return playbackClearAllMarkers();
        }
        if (name == QStringLiteral("playbackGetDuration")) {
            return playbackGetDuration();
        }
        if (name == QStringLiteral("playbackGetFrameRange")) {
            return playbackGetFrameRange();
        }
        if (name == QStringLiteral("playbackSetFrameRange")) {
            if (args.size() < 2) return false;
            return playbackSetFrameRange(intArg(args, 0, 0), intArg(args, 1, 0));
        }
        if (name == QStringLiteral("playbackGetFrameRate")) {
            return playbackGetFrameRate();
        }
        if (name == QStringLiteral("playbackGetSpeed")) {
            return playbackGetSpeed();
        }
        if (name == QStringLiteral("playbackSetSpeed")) {
            if (args.isEmpty()) return false;
            return playbackSetSpeed(doubleArg(args, 0, 1.0));
        }
        if (name == QStringLiteral("playbackGetLooping")) {
            return playbackGetLooping();
        }
        if (name == QStringLiteral("playbackSetLooping")) {
            if (args.isEmpty()) return false;
            return playbackSetLooping(args.first().toBool());
        }
        if (name == QStringLiteral("seekTimeline")) {
            if (args.isEmpty()) return false;
            return playbackSetCurrentFrame(intArg(args, 0, 0));
        }
        if (name == QStringLiteral("playTimeline")) {
            return playbackStart();
        }
        if (name == QStringLiteral("pauseTimeline")) {
            return playbackPause();
        }
        if (name == QStringLiteral("setWorkArea")) {
            if (args.size() < 2) return false;
            return playbackSetFrameRange(intArg(args, 0, 0), intArg(args, 1, 0));
        }
        if (name == QStringLiteral("exportComposition")) {
            return exportComposition(stringArg(args, 0), stringArg(args, 1), stringArg(args, 2), stringArg(args, 3), 
                                    intArg(args, 4, 1920), intArg(args, 5, 1080), doubleArg(args, 6, 60.0), intArg(args, 7, 5000));
        }
        if (name == QStringLiteral("exportCurrentComposition")) {
            return exportCurrentComposition(stringArg(args, 0), stringArg(args, 1), stringArg(args, 2), 
                                           intArg(args, 3, 1920), intArg(args, 4, 1080), doubleArg(args, 5, 60.0), intArg(args, 6, 5000));
        }
        if (name == QStringLiteral("exportCompositionAndWait")) {
            return exportCompositionAndWait(stringArg(args, 0), stringArg(args, 1), stringArg(args, 2), stringArg(args, 3), 
                                           intArg(args, 4, 1920), intArg(args, 5, 1080), doubleArg(args, 6, 60.0), intArg(args, 7, 5000));
        }
        if (name == QStringLiteral("exportCurrentCompositionAndWait")) {
            return exportCurrentCompositionAndWait(stringArg(args, 0), stringArg(args, 1), stringArg(args, 2), 
                                                  intArg(args, 3, 1920), intArg(args, 4, 1080), doubleArg(args, 5, 60.0), intArg(args, 6, 5000));
        }
        if (name == QStringLiteral("getSupportedExportFormats")) {
            return getSupportedExportFormats();
        }
        if (name == QStringLiteral("getDefaultCodecForFormat")) {
            if (args.isEmpty()) return QString();
            return getDefaultCodecForFormat(stringArg(args, 0));
        }
        if (name == QStringLiteral("removeAllRenderQueues")) {
            return renderQueueRemoveAll();
        }
        if (name == QStringLiteral("createSolidLayer")) {
            return createSolidLayer(stringArg(args, 0), stringArg(args, 1), stringArg(args, 2), intArg(args, 3, 0), intArg(args, 4, 0));
        }
        if (name == QStringLiteral("replaceLayerSource")) {
            return replaceLayerSource(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("splitLayerAtTime")) {
            return splitLayerAtTime(stringArg(args, 0), intArg(args, 1, 0));
        }
        if (name == QStringLiteral("rippleDeleteLayer")) {
            return rippleDeleteLayer(stringArg(args, 0));
        }
        if (name == QStringLiteral("alignLayersSequentially")) {
            return alignLayersSequentially(collectStringList(args));
        }
        if (name == QStringLiteral("defineTemplateSlot")) {
            return defineTemplateSlot(stringArg(args, 0), stringArg(args, 1), stringArg(args, 2));
        }
        if (name == QStringLiteral("listTemplateSlots")) {
            return listTemplateSlots();
        }
        if (name == QStringLiteral("applyTemplateVariation")) {
            return applyTemplateVariation(stringArg(args, 0));
        }
        if (name == QStringLiteral("createTemplateFromVariation")) {
            return createTemplateFromVariation(args, stringArg(args, 1));
        }
        if (name == QStringLiteral("resolveExportMatrixCell")) {
            return resolveExportMatrixCell(stringArg(args, 0), stringArg(args, 1), stringArg(args, 2), stringArg(args, 3));
        }
        if (name == QStringLiteral("createExportMatrixJobs")) {
            return createExportMatrixJobs(stringArg(args, 0), stringArg(args, 1));
        }
        if (name == QStringLiteral("queueExportMatrixForCurrentComposition")) {
            return queueExportMatrixForCurrentComposition(stringArg(args, 0), stringArg(args, 1));
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

    static QVariantMap currentCompositionThumbnailAtFrame(int frameNumber, int width, int height)
    {
        QVariantMap result;
        const auto comp = currentComposition();
        if (!comp) {
            result.insert(QStringLiteral("available"), false);
            return result;
        }

        const QImage thumbnail = comp->getThumbnailAtFrame(frameNumber, width, height);
        result.insert(QStringLiteral("available"), true);
        result.insert(QStringLiteral("frameNumber"), frameNumber);
        result.insert(QStringLiteral("width"), thumbnail.width());
        result.insert(QStringLiteral("height"), thumbnail.height());
        result.insert(QStringLiteral("format"), QStringLiteral("png"));

        QByteArray encoded;
        QBuffer buffer(&encoded);
        if (buffer.open(QIODevice::WriteOnly) && thumbnail.save(&buffer, "PNG")) {
            result.insert(QStringLiteral("pngBase64"), QString::fromLatin1(encoded.toBase64()));
            result.insert(QStringLiteral("byteCount"), static_cast<int>(encoded.size()));
        } else {
            result.insert(QStringLiteral("available"), false);
            result.insert(QStringLiteral("error"), QStringLiteral("Failed to encode thumbnail"));
        }
        return result;
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

    static QVariantMap workspaceDiagnostics()
    {
        QVariantMap diag;
        const QVariantMap project = projectSnapshot();
        const QVariantMap selection = selectionSnapshot();
        const QVariantMap comp = currentCompositionSnapshot();
        const QVariantMap queue = renderQueueSnapshot();

        diag.insert(QStringLiteral("available"), true);
        diag.insert(QStringLiteral("hasProject"), project.value(QStringLiteral("available")).toBool());
        diag.insert(QStringLiteral("hasActiveComposition"), comp.value(QStringLiteral("available")).toBool());
        diag.insert(QStringLiteral("selectedLayerCount"), selection.value(QStringLiteral("selectedLayerCount")).toInt());
        diag.insert(QStringLiteral("renderQueueJobCount"), queue.value(QStringLiteral("jobCount")).toInt());
        diag.insert(QStringLiteral("projectName"), project.value(QStringLiteral("projectName")).toString());
        diag.insert(QStringLiteral("activeCompositionName"), comp.value(QStringLiteral("name")).toString());
        diag.insert(QStringLiteral("activeCompositionId"), comp.value(QStringLiteral("id")).toString());

        QStringList warnings;
        if (!diag.value(QStringLiteral("hasProject")).toBool()) {
            warnings.append(QStringLiteral("No project is currently open."));
        }
        if (!diag.value(QStringLiteral("hasActiveComposition")).toBool()) {
            warnings.append(QStringLiteral("No active composition is selected."));
        }
        if (diag.value(QStringLiteral("selectedLayerCount")).toInt() == 0) {
            warnings.append(QStringLiteral("No layers are selected."));
        }
        if (diag.value(QStringLiteral("renderQueueJobCount")).toInt() == 0) {
            warnings.append(QStringLiteral("Render queue is empty."));
        }
        diag.insert(QStringLiteral("warnings"), warnings);
        diag.insert(QStringLiteral("summary"), warnings.isEmpty()
            ? QStringLiteral("Workspace looks ready.")
            : QStringLiteral("Workspace needs attention."));
        return diag;
    }

    static QVariantList commandVocabulary()
    {
        return ArtifactCore::CommandIR::supportedCommands();
    }

    static QVariantMap validateCommand(const QVariantMap& command)
    {
        const ArtifactCore::CommandRequest request = ArtifactCore::CommandIR::fromVariantMap(command);
        const ArtifactCore::CommandResult validation = ArtifactCore::CommandIR::validate(request);
        return validation.toVariantMap();
    }

    static QVariantMap executeCommand(const QVariantMap& command)
    {
        return commandExecutor().execute(ArtifactCore::CommandIR::fromVariantMap(command)).toVariantMap();
    }

    static ArtifactCore::AbstractPropertyPtr findLayerProperty(const ArtifactAbstractLayerPtr& layer, const QString& propertyPath)
    {
        if (!layer) {
            return {};
        }
        for (const auto& group : layer->getLayerPropertyGroups()) {
            auto prop = group.findProperty(propertyPath);
            if (prop) {
                return prop;
            }
        }
        return {};
    }

    static QVariantList propertyKeyframesToVariantList(const ArtifactCore::AbstractPropertyPtr& prop)
    {
        QVariantList frames;
        if (!prop) {
            return frames;
        }
        for (const auto& kf : prop->getKeyFrames()) {
            QVariantMap item;
            item.insert(QStringLiteral("timeValue"), static_cast<qint64>(kf.time.value()));
            item.insert(QStringLiteral("timeScale"), static_cast<qint64>(kf.time.scale()));
            item.insert(QStringLiteral("frameNumber"), static_cast<int>(kf.time.rescaledTo(1)));
            item.insert(QStringLiteral("value"), kf.value);
            item.insert(QStringLiteral("interpolation"), static_cast<int>(kf.interpolation));
            item.insert(QStringLiteral("cp1_x"), kf.cp1_x);
            item.insert(QStringLiteral("cp1_y"), kf.cp1_y);
            item.insert(QStringLiteral("cp2_x"), kf.cp2_x);
            item.insert(QStringLiteral("cp2_y"), kf.cp2_y);
            item.insert(QStringLiteral("roving"), kf.roving);
            frames.append(item);
        }
        return frames;
    }

    static void applyKeyframeSnapshot(const ArtifactAbstractLayerPtr& layer, const QString& propertyPath, const QVariantList& snapshot)
    {
        auto prop = findLayerProperty(layer, propertyPath);
        if (!prop) {
            return;
        }
        prop->clearKeyFrames();
        prop->setAnimatable(!snapshot.isEmpty());
        for (const QVariant& entryValue : snapshot) {
            const QVariantMap entry = entryValue.toMap();
            const qint64 timeValue = entry.contains(QStringLiteral("timeValue"))
                ? entry.value(QStringLiteral("timeValue")).toLongLong()
                : entry.value(QStringLiteral("frameNumber")).toLongLong();
            const qint64 timeScale = entry.contains(QStringLiteral("timeScale"))
                ? entry.value(QStringLiteral("timeScale")).toLongLong()
                : 30;
            const QVariant value = entry.value(QStringLiteral("value"));
            bool ok = false;
            int interpolationValue = entry.value(QStringLiteral("interpolation")).toInt(&ok);
            if (!ok) {
                interpolationValue = static_cast<int>(InterpolationType::Linear);
            }
            const auto interpolation = static_cast<InterpolationType>(interpolationValue);
            const float cp1_x = static_cast<float>(entry.value(QStringLiteral("cp1_x")).toDouble(&ok));
            const float cp1_y = static_cast<float>(entry.value(QStringLiteral("cp1_y")).toDouble(&ok));
            const float cp2_x = static_cast<float>(entry.value(QStringLiteral("cp2_x")).toDouble(&ok));
            const float cp2_y = static_cast<float>(entry.value(QStringLiteral("cp2_y")).toDouble(&ok));
            const bool roving = entry.value(QStringLiteral("roving")).toBool();
            prop->addKeyFrame(RationalTime(timeValue, timeScale), value, interpolation, cp1_x, cp1_y, cp2_x, cp2_y, roving);
        }
    }

    class KeyframeSnapshotUndoCommand final : public UndoCommand {
    public:
        KeyframeSnapshotUndoCommand(ArtifactAbstractLayerPtr layer, QString propertyPath,
                                    QVariantList beforeSnapshot, QVariantList afterSnapshot,
                                    QString label)
            : layer_(std::move(layer)),
              propertyPath_(std::move(propertyPath)),
              beforeSnapshot_(std::move(beforeSnapshot)),
              afterSnapshot_(std::move(afterSnapshot)),
              label_(std::move(label)) {}

        void undo() override { applyKeyframeSnapshot(layer_.lock(), propertyPath_, beforeSnapshot_); }
        void redo() override { applyKeyframeSnapshot(layer_.lock(), propertyPath_, afterSnapshot_); }
        QString label() const override { return label_; }

    private:
        ArtifactAbstractLayerWeak layer_;
        QString propertyPath_;
        QVariantList beforeSnapshot_;
        QVariantList afterSnapshot_;
        QString label_;
    };

    class CommandExecutorImpl final : public ArtifactCore::CommandExecutor {
    public:
        ArtifactCore::CommandResult validate(const ArtifactCore::CommandRequest& request) const override
        {
            return ArtifactCore::CommandIR::validate(request);
        }

        ArtifactCore::CommandResult execute(const ArtifactCore::CommandRequest& request) const override
        {
            const ArtifactCore::CommandResult validation = validate(request);
            if (!validation.valid) {
                return validation;
            }
            const QString type = request.type;
            const QVariantMap target = request.target;
            const QString layerId = target.value(QStringLiteral("layerId")).toString().trimmed();
            const QString propertyPath = target.value(QStringLiteral("propertyPath")).toString().trimmed();

            if (type == QStringLiteral("set_property")) {
                QVariantMap map = WorkspaceAutomation::setGenericLayerProperty(layerId, propertyPath, request.arguments.value(QStringLiteral("value")));
                map.insert(QStringLiteral("undoLabel"), ArtifactCore::CommandIR::undoLabelForType(type));
                map.insert(QStringLiteral("type"), type);
                return ArtifactCore::commandResultFromVariantMap(map);
            }
            if (type == QStringLiteral("set_keyframes")) {
                auto *undo = UndoManager::instance();
                const auto comp = currentComposition();
                const auto layer = comp ? comp->layerById(ArtifactCore::LayerID(layerId)) : ArtifactAbstractLayerPtr{};
                const auto prop = findLayerProperty(layer, propertyPath);
                if (!undo || !comp || !layer || !prop) {
                    return ArtifactCore::commandResultFromVariantMap(QVariantMap{
                        {QStringLiteral("success"), false},
                        {QStringLiteral("valid"), true},
                        {QStringLiteral("executed"), false},
                        {QStringLiteral("type"), type},
                        {QStringLiteral("undoLabel"), ArtifactCore::CommandIR::undoLabelForType(type)},
                        {QStringLiteral("error"), QStringLiteral("Undo manager, composition, layer, or property unavailable")}
                    });
                }
                const QVariantList before = propertyKeyframesToVariantList(prop);
                QVariantList after;
                const QVariantList keys = request.arguments.value(QStringLiteral("keys")).toList();
                for (const QVariant& keyValue : keys) {
                    const QVariantMap key = keyValue.toMap();
                    const qint64 timeValue = key.contains(QStringLiteral("timeValue"))
                        ? key.value(QStringLiteral("timeValue")).toLongLong()
                        : key.value(QStringLiteral("time")).toLongLong();
                    const qint64 timeScale = key.contains(QStringLiteral("timeScale"))
                        ? key.value(QStringLiteral("timeScale")).toLongLong()
                        : 30;
                    QVariantMap item;
                    item.insert(QStringLiteral("timeValue"), timeValue);
                    item.insert(QStringLiteral("timeScale"), timeScale);
                    item.insert(QStringLiteral("frameNumber"), static_cast<int>(RationalTime(timeValue, timeScale).rescaledTo(1)));
                    item.insert(QStringLiteral("value"), key.value(QStringLiteral("value")));
                    item.insert(QStringLiteral("interpolation"), static_cast<int>(InterpolationType::Linear));
                    item.insert(QStringLiteral("cp1_x"), 0.42);
                    item.insert(QStringLiteral("cp1_y"), 0.0);
                    item.insert(QStringLiteral("cp2_x"), 0.58);
                    item.insert(QStringLiteral("cp2_y"), 1.0);
                    item.insert(QStringLiteral("roving"), false);
                    after.append(item);
                }
                undo->push(std::make_unique<KeyframeSnapshotUndoCommand>(layer, propertyPath, before, after, ArtifactCore::CommandIR::undoLabelForType(type)));
                return ArtifactCore::commandResultFromVariantMap(QVariantMap{
                    {QStringLiteral("success"), true},
                    {QStringLiteral("valid"), true},
                    {QStringLiteral("executed"), true},
                    {QStringLiteral("type"), type},
                    {QStringLiteral("undoLabel"), ArtifactCore::CommandIR::undoLabelForType(type)}
                });
            }
            if (type == QStringLiteral("batch_set_keyframes")) {
                auto *undo = UndoManager::instance();
                const auto comp = currentComposition();
                const auto layer = comp ? comp->layerById(ArtifactCore::LayerID(layerId)) : ArtifactAbstractLayerPtr{};
                if (!undo || !comp || !layer) {
                    return ArtifactCore::commandResultFromVariantMap(QVariantMap{
                        {QStringLiteral("success"), false},
                        {QStringLiteral("valid"), true},
                        {QStringLiteral("executed"), false},
                        {QStringLiteral("type"), type},
                        {QStringLiteral("undoLabel"), ArtifactCore::CommandIR::undoLabelForType(type)},
                        {QStringLiteral("error"), QStringLiteral("Undo manager, composition, or layer unavailable")}
                    });
                }
                auto macro = std::make_unique<MacroUndoCommand>(ArtifactCore::CommandIR::undoLabelForType(type));
                const QVariantList batches = request.arguments.value(QStringLiteral("batches")).toList();
                for (const QVariant& batchValue : batches) {
                    const QVariantMap batch = batchValue.toMap();
                    const QString batchPropertyPath = batch.value(QStringLiteral("propertyPath")).toString().trimmed();
                    const auto prop = findLayerProperty(layer, batchPropertyPath);
                    if (!prop) {
                        continue;
                    }
                    const QVariantList before = propertyKeyframesToVariantList(prop);
                    QVariantList after;
                    const QVariantList keys = batch.value(QStringLiteral("keys")).toList();
                    for (const QVariant& keyValue : keys) {
                        const QVariantMap key = keyValue.toMap();
                        const qint64 timeValue = key.contains(QStringLiteral("timeValue"))
                            ? key.value(QStringLiteral("timeValue")).toLongLong()
                            : key.value(QStringLiteral("time")).toLongLong();
                        const qint64 timeScale = key.contains(QStringLiteral("timeScale"))
                            ? key.value(QStringLiteral("timeScale")).toLongLong()
                            : 30;
                        QVariantMap item;
                        item.insert(QStringLiteral("timeValue"), timeValue);
                        item.insert(QStringLiteral("timeScale"), timeScale);
                        item.insert(QStringLiteral("frameNumber"), static_cast<int>(RationalTime(timeValue, timeScale).rescaledTo(1)));
                        item.insert(QStringLiteral("value"), key.value(QStringLiteral("value")));
                        item.insert(QStringLiteral("interpolation"), static_cast<int>(InterpolationType::Linear));
                        item.insert(QStringLiteral("cp1_x"), 0.42);
                        item.insert(QStringLiteral("cp1_y"), 0.0);
                        item.insert(QStringLiteral("cp2_x"), 0.58);
                        item.insert(QStringLiteral("cp2_y"), 1.0);
                        item.insert(QStringLiteral("roving"), false);
                        after.append(item);
                    }
                    macro->addChild(std::make_unique<KeyframeSnapshotUndoCommand>(
                        layer, batchPropertyPath, before, after, ArtifactCore::CommandIR::undoLabelForType(type)));
                }
                undo->push(std::move(macro));
                return ArtifactCore::commandResultFromVariantMap(QVariantMap{
                    {QStringLiteral("success"), true},
                    {QStringLiteral("valid"), true},
                    {QStringLiteral("executed"), true},
                    {QStringLiteral("type"), type},
                    {QStringLiteral("undoLabel"), ArtifactCore::CommandIR::undoLabelForType(type)}
                });
            }
            if (type == QStringLiteral("move_layer")) {
                auto *undo = UndoManager::instance();
                const auto comp = currentComposition();
                const auto layer = comp ? comp->layerById(ArtifactCore::LayerID(layerId)) : ArtifactAbstractLayerPtr{};
                if (!undo || !comp || !layer) {
                    return ArtifactCore::commandResultFromVariantMap(QVariantMap{
                        {QStringLiteral("success"), false},
                        {QStringLiteral("valid"), true},
                        {QStringLiteral("executed"), false},
                        {QStringLiteral("type"), type},
                        {QStringLiteral("undoLabel"), ArtifactCore::CommandIR::undoLabelForType(type)},
                        {QStringLiteral("error"), QStringLiteral("Undo manager, composition, or layer unavailable")}
                    });
                }
                int oldIndex = -1;
                const auto layers = comp->allLayer();
                for (int i = 0; i < layers.size(); ++i) {
                    if (layers[i] && layers[i]->id() == layer->id()) {
                        oldIndex = i;
                        break;
                    }
                }
                const int newIndex = request.arguments.value(QStringLiteral("newIndex")).toInt();
                undo->push(std::make_unique<MoveLayerIndexCommand>(comp, layer, oldIndex, newIndex));
                return ArtifactCore::commandResultFromVariantMap(QVariantMap{
                    {QStringLiteral("success"), true},
                    {QStringLiteral("valid"), true},
                    {QStringLiteral("executed"), true},
                    {QStringLiteral("type"), type},
                    {QStringLiteral("undoLabel"), ArtifactCore::CommandIR::undoLabelForType(type)}
                });
            }
            if (type == QStringLiteral("rename_layer")) {
                auto *undo = UndoManager::instance();
                const auto comp = currentComposition();
                const auto layer = comp ? comp->layerById(ArtifactCore::LayerID(layerId)) : ArtifactAbstractLayerPtr{};
                if (!undo || !comp || !layer) {
                    return ArtifactCore::commandResultFromVariantMap(QVariantMap{
                        {QStringLiteral("success"), false},
                        {QStringLiteral("valid"), true},
                        {QStringLiteral("executed"), false},
                        {QStringLiteral("type"), type},
                        {QStringLiteral("undoLabel"), ArtifactCore::CommandIR::undoLabelForType(type)},
                        {QStringLiteral("error"), QStringLiteral("Undo manager, composition, or layer unavailable")}
                    });
                }
                const QString oldName = layer->layerName();
                const QString newName = request.arguments.value(QStringLiteral("newName")).toString().trimmed();
                undo->push(std::make_unique<RenameLayerCommand>(layer, oldName, newName));
                return ArtifactCore::commandResultFromVariantMap(QVariantMap{
                    {QStringLiteral("success"), true},
                    {QStringLiteral("valid"), true},
                    {QStringLiteral("executed"), true},
                    {QStringLiteral("type"), type},
                    {QStringLiteral("undoLabel"), ArtifactCore::CommandIR::undoLabelForType(type)}
                });
            }
            if (type == QStringLiteral("add_effect")) {
                auto *effectService = ArtifactEffectService::instance();
                if (!effectService) {
                    return ArtifactCore::commandResultFromVariantMap(QVariantMap{
                        {QStringLiteral("success"), false},
                        {QStringLiteral("valid"), true},
                        {QStringLiteral("executed"), false},
                        {QStringLiteral("type"), type},
                        {QStringLiteral("undoLabel"), ArtifactCore::CommandIR::undoLabelForType(type)},
                        {QStringLiteral("error"), QStringLiteral("Effect service unavailable")}
                    });
                }
                const QString effectType = request.arguments.value(QStringLiteral("effectType")).toString().trimmed();
                auto effect = effectService->createEffect(Artifact::EffectID(effectType));
                if (!effect) {
                    return ArtifactCore::commandResultFromVariantMap(QVariantMap{
                        {QStringLiteral("success"), false},
                        {QStringLiteral("valid"), true},
                        {QStringLiteral("executed"), false},
                        {QStringLiteral("type"), type},
                        {QStringLiteral("undoLabel"), ArtifactCore::CommandIR::undoLabelForType(type)},
                        {QStringLiteral("error"), QStringLiteral("Unknown effect type")}
                    });
                }
                auto *projectService = ArtifactProjectService::instance();
                if (!projectService) {
                    return ArtifactCore::commandResultFromVariantMap(QVariantMap{
                        {QStringLiteral("success"), false},
                        {QStringLiteral("valid"), true},
                        {QStringLiteral("executed"), false},
                        {QStringLiteral("type"), type},
                        {QStringLiteral("undoLabel"), ArtifactCore::CommandIR::undoLabelForType(type)},
                        {QStringLiteral("error"), QStringLiteral("Project service unavailable")}
                    });
                }
                const bool success = projectService->addEffectToLayerWithUndo(ArtifactCore::LayerID(layerId), std::move(effect));
                return ArtifactCore::commandResultFromVariantMap(QVariantMap{
                    {QStringLiteral("success"), success},
                    {QStringLiteral("valid"), true},
                    {QStringLiteral("executed"), success},
                    {QStringLiteral("type"), type},
                    {QStringLiteral("undoLabel"), ArtifactCore::CommandIR::undoLabelForType(type)}
                });
            }
            ArtifactCore::CommandResult result = validation;
            result.success = false;
            result.executed = false;
            result.error = QStringLiteral("Unsupported command type");
            return result;
        }
    };

    static ArtifactCore::CommandExecutor& commandExecutor()
    {
        static CommandExecutorImpl executor;
        return executor;
    }

    static QVariantMap setGenericLayerProperty(const QString& layerId, const QString& propertyPath, const QVariant& value)
    {
        if (propertyPath == QStringLiteral("transform.position") || propertyPath == QStringLiteral("position")) {
            const QVariantMap point = value.toMap();
            return QVariantMap{
                {QStringLiteral("success"), setLayerPosition(layerId, point.value(QStringLiteral("x")).toDouble(), point.value(QStringLiteral("y")).toDouble()).toBool()},
                {QStringLiteral("valid"), true},
                {QStringLiteral("executed"), true},
                {QStringLiteral("type"), QStringLiteral("set_property")},
                {QStringLiteral("undoLabel"), QStringLiteral("Set Property")},
                {QStringLiteral("propertyPath"), propertyPath}
            };
        }
        if (propertyPath == QStringLiteral("transform.scale") || propertyPath == QStringLiteral("scale")) {
            const QVariantMap point = value.toMap();
            return QVariantMap{
                {QStringLiteral("success"), setLayerScale(layerId, point.value(QStringLiteral("x")).toDouble(), point.value(QStringLiteral("y")).toDouble()).toBool()},
                {QStringLiteral("valid"), true},
                {QStringLiteral("executed"), true},
                {QStringLiteral("type"), QStringLiteral("set_property")},
                {QStringLiteral("undoLabel"), QStringLiteral("Set Property")},
                {QStringLiteral("propertyPath"), propertyPath}
            };
        }
        if (propertyPath == QStringLiteral("transform.rotation") || propertyPath == QStringLiteral("rotation")) {
            return QVariantMap{
                {QStringLiteral("success"), setLayerRotation(layerId, value.toDouble()).toBool()},
                {QStringLiteral("valid"), true},
                {QStringLiteral("executed"), true},
                {QStringLiteral("type"), QStringLiteral("set_property")},
                {QStringLiteral("undoLabel"), QStringLiteral("Set Property")},
                {QStringLiteral("propertyPath"), propertyPath}
            };
        }
        if (propertyPath == QStringLiteral("opacity")) {
            return QVariantMap{
                {QStringLiteral("success"), setLayerOpacity(layerId, value.toDouble()).toBool()},
                {QStringLiteral("valid"), true},
                {QStringLiteral("executed"), true},
                {QStringLiteral("type"), QStringLiteral("set_property")},
                {QStringLiteral("undoLabel"), QStringLiteral("Set Property")},
                {QStringLiteral("propertyPath"), propertyPath}
            };
        }

        return QVariantMap{
            {QStringLiteral("success"), false},
            {QStringLiteral("valid"), true},
            {QStringLiteral("executed"), false},
            {QStringLiteral("type"), QStringLiteral("set_property")},
            {QStringLiteral("undoLabel"), QStringLiteral("Set Property")},
            {QStringLiteral("error"), QStringLiteral("Unsupported propertyPath")}
        };
    }

    static QVariantMap setGenericKeyframes(const QString& layerId, const QString& propertyPath, const QVariantList& keys)
    {
        QVariantList normalized;
        for (const QVariant& keyValue : keys) {
            const QVariantMap key = keyValue.toMap();
            normalized.append(QVariantMap{
                {QStringLiteral("propertyPath"), propertyPath},
                {QStringLiteral("frameNumber"), key.value(QStringLiteral("time")).toInt()},
                {QStringLiteral("value"), key.value(QStringLiteral("value"))}
            });
        }
        const QVariantMap result = batchSetKeyframes(layerId, normalized).toMap();
        return QVariantMap{
            {QStringLiteral("success"), result.value(QStringLiteral("success")).toBool()},
            {QStringLiteral("valid"), true},
            {QStringLiteral("executed"), true},
            {QStringLiteral("type"), QStringLiteral("set_keyframes")},
            {QStringLiteral("undoLabel"), QStringLiteral("Set Keyframes")},
            {QStringLiteral("addedCount"), result.value(QStringLiteral("addedCount"))},
            {QStringLiteral("skippedCount"), result.value(QStringLiteral("skippedCount"))},
            {QStringLiteral("details"), result.value(QStringLiteral("details"))}
        };
    }

    static QVariantMap batchSetGenericKeyframes(const QString& layerId, const QVariantList& batches)
    {
        QVariantList normalized;
        for (const QVariant& batchValue : batches) {
            const QVariantMap batch = batchValue.toMap();
            const QString propertyPath = batch.value(QStringLiteral("propertyPath")).toString().trimmed();
            const QVariantList keys = batch.value(QStringLiteral("keys")).toList();
            for (const QVariant& keyValue : keys) {
                const QVariantMap key = keyValue.toMap();
                normalized.append(QVariantMap{
                    {QStringLiteral("propertyPath"), propertyPath},
                    {QStringLiteral("frameNumber"), key.value(QStringLiteral("time")).toInt()},
                    {QStringLiteral("value"), key.value(QStringLiteral("value"))}
                });
            }
        }
        const QVariantMap result = batchSetKeyframes(layerId, normalized).toMap();
        return QVariantMap{
            {QStringLiteral("success"), result.value(QStringLiteral("success")).toBool()},
            {QStringLiteral("valid"), true},
            {QStringLiteral("executed"), true},
            {QStringLiteral("type"), QStringLiteral("batch_set_keyframes")},
            {QStringLiteral("undoLabel"), QStringLiteral("Batch Set Keyframes")},
            {QStringLiteral("addedCount"), result.value(QStringLiteral("addedCount"))},
            {QStringLiteral("skippedCount"), result.value(QStringLiteral("skippedCount"))},
            {QStringLiteral("details"), result.value(QStringLiteral("details"))}
        };
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
        return QVariantMap{
            {QStringLiteral("success"), result.isSuccess},
            {QStringLiteral("isSuccess"), result.isSuccess}
        };
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

    static QVariant setLayerBlendModeInCurrentComposition(const QString& layerId, const QVariant& blendModeValue)
    {
        const auto comp = currentComposition();
        if (!comp) {
            return false;
        }
        const auto layer = comp->layerById(LayerID(layerId));
        if (!layer) {
            return false;
        }
        ArtifactCore::BlendMode mode = ArtifactCore::BlendMode::Normal;
        const QString modeText = blendModeValue.toString().trimmed();
        bool ok = false;
        const int modeIndex = modeText.toInt(&ok);
        if (ok) {
            mode = static_cast<ArtifactCore::BlendMode>(modeIndex);
        } else if (!modeText.isEmpty()) {
            mode = ArtifactCore::BlendModeUtils::fromString(modeText);
        }
        layer->setBlendMode(ArtifactCore::toLegacyBlendType(mode));
        return true;
    }

    static QVariant setLayerOpacityInCurrentComposition(const QString& layerId, double opacity)
    {
        const auto comp = currentComposition();
        if (!comp) {
            return false;
        }
        const auto layer = comp->layerById(LayerID(layerId));
        if (!layer) {
            return false;
        }
        layer->setOpacity(static_cast<float>(std::clamp(opacity, 0.0, 1.0)));
        return true;
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

    static QVariant getCompositionNote(const QString& compositionId)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return QString();
        }
        auto result = service->findComposition(CompositionID(compositionId));
        if (!result.success) {
            return QString();
        }
        auto composition = result.ptr.lock();
        if (!composition) {
            return QString();
        }
        return composition->compositionNote();
    }

    static QVariant setCompositionNote(const QString& compositionId, const QString& note)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }
        auto result = service->findComposition(CompositionID(compositionId));
        if (!result.success) {
            return false;
        }
        auto composition = result.ptr.lock();
        if (!composition) {
            return false;
        }
        composition->setCompositionNote(note);
        return true;
    }

    static QVariant getLayerNote(const QString& layerId)
    {
        auto* app = ArtifactApplicationManager::instance();
        if (!app) {
            return QString();
        }
        auto composition = app->activeContextService()->activeComposition();
        if (!composition) {
            return QString();
        }
        auto layer = composition->layerById(LayerID(layerId));
        if (!layer) {
            return QString();
        }
        return layer->layerNote();
    }

    static QVariant setLayerNote(const QString& layerId, const QString& note)
    {
        auto* app = ArtifactApplicationManager::instance();
        if (!app) {
            return false;
        }
        auto composition = app->activeContextService()->activeComposition();
        if (!composition) {
            return false;
        }
        auto layer = composition->layerById(LayerID(layerId));
        if (!layer) {
            return false;
        }
        layer->setLayerNote(note);
        return true;
    }

    // Phase 2: Layer Properties
    static QVariant getLayerPosition(const QString& layerId)
    {
        auto* app = ArtifactApplicationManager::instance();
        if (!app) {
            return QVariantMap();
        }
        auto composition = app->activeContextService()->activeComposition();
        if (!composition) {
            return QVariantMap();
        }
        auto layer = composition->layerById(LayerID(layerId));
        if (!layer) {
            return QVariantMap();
        }
        float x = 0.0f, y = 0.0f;
        auto& transform = layer->transform2D();
        transform.position(x, y);
        QVariantMap result;
        result[QStringLiteral("x")] = static_cast<double>(x);
        result[QStringLiteral("y")] = static_cast<double>(y);
        return result;
    }

    static QVariant setLayerPosition(const QString& layerId, double x, double y)
    {
        auto* app = ArtifactApplicationManager::instance();
        if (!app) {
            return false;
        }
        auto composition = app->activeContextService()->activeComposition();
        if (!composition) {
            return false;
        }
        auto layer = composition->layerById(LayerID(layerId));
        if (!layer) {
            return false;
        }
        auto& transform = layer->transform2D();
        transform.setPosition(static_cast<float>(x), static_cast<float>(y));
        return true;
    }

    static QVariant getLayerScale(const QString& layerId)
    {
        auto* app = ArtifactApplicationManager::instance();
        if (!app) {
            return QVariantMap();
        }
        auto composition = app->activeContextService()->activeComposition();
        if (!composition) {
            return QVariantMap();
        }
        auto layer = composition->layerById(LayerID(layerId));
        if (!layer) {
            return QVariantMap();
        }
        float sx = 1.0f, sy = 1.0f;
        auto& transform = layer->transform2D();
        transform.scale(sx, sy);
        QVariantMap result;
        result[QStringLiteral("x")] = static_cast<double>(sx);
        result[QStringLiteral("y")] = static_cast<double>(sy);
        return result;
    }

    static QVariant setLayerScale(const QString& layerId, double sx, double sy)
    {
        auto* app = ArtifactApplicationManager::instance();
        if (!app) {
            return false;
        }
        auto composition = app->activeContextService()->activeComposition();
        if (!composition) {
            return false;
        }
        auto layer = composition->layerById(LayerID(layerId));
        if (!layer) {
            return false;
        }
        auto& transform = layer->transform2D();
        transform.setScale(static_cast<float>(sx), static_cast<float>(sy));
        return true;
    }

    static QVariant getLayerRotation(const QString& layerId)
    {
        auto* app = ArtifactApplicationManager::instance();
        if (!app) {
            return 0.0;
        }
        auto composition = app->activeContextService()->activeComposition();
        if (!composition) {
            return 0.0;
        }
        auto layer = composition->layerById(LayerID(layerId));
        if (!layer) {
            return 0.0;
        }
        auto& transform = layer->transform2D();
        return static_cast<double>(transform.rotation());
    }

    static QVariant setLayerRotation(const QString& layerId, double rotation)
    {
        auto* app = ArtifactApplicationManager::instance();
        if (!app) {
            return false;
        }
        auto composition = app->activeContextService()->activeComposition();
        if (!composition) {
            return false;
        }
        auto layer = composition->layerById(LayerID(layerId));
        if (!layer) {
            return false;
        }
        auto& transform = layer->transform2D();
        transform.setRotation(static_cast<float>(rotation));
        return true;
    }

    static QVariant getLayerOpacity(const QString& layerId)
    {
        auto* app = ArtifactApplicationManager::instance();
        if (!app) {
            return 100.0;
        }
        auto composition = app->activeContextService()->activeComposition();
        if (!composition) {
            return 100.0;
        }
        auto layer = composition->layerById(LayerID(layerId));
        if (!layer) {
            return 100.0;
        }
        return static_cast<double>(layer->opacity() * 100.0);
    }

    static QVariant setLayerOpacity(const QString& layerId, double opacity)
    {
        auto* app = ArtifactApplicationManager::instance();
        if (!app) {
            return false;
        }
        auto composition = app->activeContextService()->activeComposition();
        if (!composition) {
            return false;
        }
        auto layer = composition->layerById(LayerID(layerId));
        if (!layer) {
            return false;
        }
        layer->setOpacity(static_cast<float>(opacity / 100.0));
        return true;
    }

    // Phase 3: Effects/Masks (fully implemented)
    static QVariant getLayerEffects(const QString& layerId)
    {
        auto* ps = ArtifactProjectService::instance();
        if (!ps) return QVariantList();
        
        auto comp = ps->currentComposition().lock();
        if (!comp) return QVariantList();
        
        auto layer = comp->layerById(ArtifactCore::LayerID(layerId));
        if (!layer) return QVariantList();
        
        QVariantList effectsList;
        for (const auto& effect : layer->getEffects()) {
            QVariantMap effectMap;
            effectMap[QStringLiteral("id")] = effect->effectID().toQString();
            effectMap[QStringLiteral("name")] = effect->displayName().toQString();
            effectMap[QStringLiteral("enabled")] = effect->isEnabled();
            effectsList.append(effectMap);
        }
        return effectsList;
    }

    static QVariant addLayerEffect(const QString& layerId, const QString& effectType)
    {
        auto* effectService = ArtifactEffectService::instance();
        if (!effectService) return QString();
        
        const auto result = effectService->addEffectToLayer(
            ArtifactCore::LayerID(layerId),
            EffectID(effectType)
        );
        
        if (result.success) {
            return result.effectId;
        }
        return QString();
    }

    static QVariant removeLayerEffect(const QString& layerId, const QString& effectId)
    {
        auto* effectService = ArtifactEffectService::instance();
        if (!effectService) return false;
        
        const auto result = effectService->removeEffectFromLayer(
            ArtifactCore::LayerID(layerId),
            effectId
        );
        
        return result.success;
    }

    static QVariant setLayerEffectParameter(const QString& layerId, const QString& effectId, const QString& paramName, const QVariant& value)
    {
        auto* effectService = ArtifactEffectService::instance();
        if (!effectService) return false;
        
        const auto result = effectService->setEffectProperty(
            ArtifactCore::LayerID(layerId),
            effectId,
            paramName,
            QVariant(value)
        );
        
        return result.success;
    }

    static QVariant setLayerEffectEnabled(const QString& layerId, const QString& effectId, bool enabled)
    {
        auto* effectService = ArtifactEffectService::instance();
        if (!effectService) return false;

        const auto result = effectService->setEffectEnabled(
            ArtifactCore::LayerID(layerId),
            effectId,
            enabled
        );

        return result.success;
    }

    static QVariant moveLayerEffect(const QString& layerId, const QString& effectId, int direction)
    {
        auto* effectService = ArtifactEffectService::instance();
        if (!effectService) return false;

        const auto result = effectService->moveEffect(
            ArtifactCore::LayerID(layerId),
            effectId,
            direction
        );

        return result.success;
    }

    static QVariant duplicateLayerEffect(const QString& layerId, const QString& effectId)
    {
        auto* effectService = ArtifactEffectService::instance();
        if (!effectService) return QString();

        const auto result = effectService->duplicateEffect(
            ArtifactCore::LayerID(layerId),
            effectId
        );

        if (result.success) {
            return result.effectId;
        }
        return QString();
    }

    static QVariant saveLayerEffectPreset(const QString& layerId, const QString& effectId, const QString& filePath)
    {
        auto* effectService = ArtifactEffectService::instance();
        if (!effectService) return false;

        const auto effect = findLayerEffect(layerId, effectId);
        if (!effect) {
            return false;
        }

        const bool ok = effectService->saveEffectPreset(effect, filePath);
        if (ok) {
            recordRecentLayerEffectPreset(filePath, effect);
        }
        return ok;
    }

    static QVariant loadLayerEffectPreset(const QString& layerId, const QString& effectId, const QString& filePath)
    {
        auto* effectService = ArtifactEffectService::instance();
        if (!effectService) return false;

        auto effect = findLayerEffect(layerId, effectId);
        if (!effect) {
            return false;
        }

        const bool ok = effectService->loadEffectPreset(effect, filePath);
        if (ok) {
            recordRecentLayerEffectPreset(filePath, effect);
        }
        return ok;
    }

    static QVariant listLayerEffectPresets(const QString& directoryPath)
    {
        const QDir dir(resolveLayerEffectPresetDirectory(directoryPath));
        if (!dir.exists()) {
            return QVariantList();
        }

        QVariantList presets;
        const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo& info : files) {
            QVariantMap item;
            item[QStringLiteral("fileName")] = info.fileName();
            item[QStringLiteral("filePath")] = info.absoluteFilePath();
            item[QStringLiteral("sizeBytes")] = static_cast<qint64>(info.size());
            item[QStringLiteral("modified")] = info.lastModified().toString(Qt::ISODate);

            QFile file(info.absoluteFilePath());
            if (file.open(QIODevice::ReadOnly)) {
                const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                if (doc.isObject()) {
                    const QJsonObject obj = doc.object();
                    const QString effectId = obj.value(QStringLiteral("effect_id")).toString();
                    const QString displayName = obj.value(QStringLiteral("display_name")).toString();
                    if (!effectId.isEmpty()) {
                        item[QStringLiteral("effectId")] = effectId;
                    }
                    if (!displayName.isEmpty()) {
                        item[QStringLiteral("displayName")] = displayName;
                    }
                }
            }
            if (!item.contains(QStringLiteral("displayName"))) {
                item[QStringLiteral("displayName")] = info.completeBaseName();
            }
            presets.append(item);
        }
        return presets;
    }

    static QVariant recentLayerEffectPresets(int limit)
    {
        const int cappedLimit = std::max(0, limit);
        if (cappedLimit == 0) {
            return QVariantList();
        }

        QSettings settings;
        settings.beginGroup(QStringLiteral("AI/EffectPresets/Recent"));
        const int count = settings.beginReadArray(QStringLiteral("items"));
        QVariantList presets;
        for (int i = count - 1; i >= 0 && presets.size() < cappedLimit; --i) {
            settings.setArrayIndex(i);
            const QString filePath = settings.value(QStringLiteral("filePath")).toString();
            if (filePath.trimmed().isEmpty()) {
                continue;
            }
            QVariantMap item;
            item[QStringLiteral("filePath")] = filePath;
            item[QStringLiteral("fileName")] = settings.value(QStringLiteral("fileName")).toString();
            item[QStringLiteral("effectId")] = settings.value(QStringLiteral("effectId")).toString();
            item[QStringLiteral("displayName")] = settings.value(QStringLiteral("displayName")).toString();
            item[QStringLiteral("lastUsed")] = settings.value(QStringLiteral("lastUsed")).toString();
            presets.append(item);
        }
        settings.endArray();
        settings.endGroup();
        return presets;
    }

    static QString resolveLayerEffectPresetDirectory(const QString& directoryPath)
    {
        const QString trimmed = directoryPath.trimmed();
        if (!trimmed.isEmpty()) {
            return QDir::cleanPath(trimmed);
        }
        QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (base.isEmpty()) {
            base = QDir::currentPath();
        }
        return QDir(base).filePath(QStringLiteral("presets/effects"));
    }

    static void recordRecentLayerEffectPreset(const QString& filePath, const ArtifactAbstractEffectPtr& effect)
    {
        const QString trimmedPath = filePath.trimmed();
        if (trimmedPath.isEmpty()) {
            return;
        }

        QFileInfo info(trimmedPath);
        QVariantMap entry;
        entry[QStringLiteral("filePath")] = info.absoluteFilePath();
        entry[QStringLiteral("fileName")] = info.fileName();
        entry[QStringLiteral("effectId")] = effect ? effect->effectID().toQString() : QString();
        entry[QStringLiteral("displayName")] = effect ? effect->displayName().toQString() : QString();
        entry[QStringLiteral("lastUsed")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

        QSettings settings;
        settings.beginGroup(QStringLiteral("AI/EffectPresets/Recent"));
        QVariantList recentEntries;
        const int count = settings.beginReadArray(QStringLiteral("items"));
        recentEntries.reserve(count + 1);
        for (int i = 0; i < count; ++i) {
            settings.setArrayIndex(i);
            const QString existingPath = settings.value(QStringLiteral("filePath")).toString();
            if (existingPath.trimmed().isEmpty()) {
                continue;
            }
            if (QDir::cleanPath(existingPath) == entry.value(QStringLiteral("filePath")).toString()) {
                continue;
            }
            QVariantMap existing;
            existing[QStringLiteral("filePath")] = existingPath;
            existing[QStringLiteral("fileName")] = settings.value(QStringLiteral("fileName")).toString();
            existing[QStringLiteral("effectId")] = settings.value(QStringLiteral("effectId")).toString();
            existing[QStringLiteral("displayName")] = settings.value(QStringLiteral("displayName")).toString();
            existing[QStringLiteral("lastUsed")] = settings.value(QStringLiteral("lastUsed")).toString();
            recentEntries.append(existing);
        }
        settings.endArray();
        recentEntries.prepend(entry);
        while (recentEntries.size() > 12) {
            recentEntries.removeLast();
        }
        settings.beginWriteArray(QStringLiteral("items"));
        for (int i = 0; i < recentEntries.size(); ++i) {
            settings.setArrayIndex(i);
            const QVariantMap item = recentEntries[i].toMap();
            settings.setValue(QStringLiteral("filePath"), item.value(QStringLiteral("filePath")));
            settings.setValue(QStringLiteral("fileName"), item.value(QStringLiteral("fileName")));
            settings.setValue(QStringLiteral("effectId"), item.value(QStringLiteral("effectId")));
            settings.setValue(QStringLiteral("displayName"), item.value(QStringLiteral("displayName")));
            settings.setValue(QStringLiteral("lastUsed"), item.value(QStringLiteral("lastUsed")));
        }
        settings.endArray();
        settings.endGroup();
    }

    static ArtifactAbstractEffectPtr findLayerEffect(const QString& layerId, const QString& effectId)
    {
        auto* ps = ArtifactProjectService::instance();
        if (!ps) return {};

        auto comp = ps->currentComposition().lock();
        if (!comp) return {};

        auto layer = comp->layerById(ArtifactCore::LayerID(layerId));
        if (!layer) return {};

        for (const auto& effect : layer->getEffects()) {
            if (effect && effect->effectID().toQString() == effectId) {
                return effect;
            }
        }
        return {};
    }

    // Phase 4: Keyframe Animation
    // 
    // Set keyframe at given frame number for a property path.
    // Supported property paths (transform): "transform.position.x", "transform.position.y", 
    // "transform.rotation", "transform.scale.x", "transform.scale.y", "transform.anchor.x", "transform.anchor.y"
    // Returns: {"success": bool, "keyframeId": string (frame number)}
    static QVariant setKeyframe(const QString& layerId, const QString& propertyPath, int frameNumber, double value)
    {
        auto svc = ArtifactProjectService::instance();
        if (!svc) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("ProjectService not available")}
            };
        }

        auto currentComp = svc->currentComposition().lock();
        if (!currentComp) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("No active composition")}
            };
        }

        auto layer = currentComp->layerById(LayerID(layerId));
        if (!layer) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Layer not found")}
            };
        }

        // Use KeyframeModel to handle the operation
        static ArtifactTimelineKeyframeModel keyframeModel;
        RationalTime time(frameNumber, 30);  // Assuming 30fps base; can be made configurable
        bool added = keyframeModel.addKeyframe(
            currentComp->id(),
            layer->id(),
            propertyPath,
            time,
            QVariant(value),
            InterpolationType::Linear
        );

        return QVariantMap{
            {QStringLiteral("success"), added},
            {QStringLiteral("keyframeId"), QString::number(frameNumber)}
        };
    }

    // Get keyframes for a property path
    // Returns: [{"frame": int, "value": double, "interpolation": string}]
    static QVariant getKeyframes(const QString& layerId, const QString& propertyPath)
    {
        auto svc = ArtifactProjectService::instance();
        if (!svc) {
            return QVariantList();
        }

        auto currentComp = svc->currentComposition().lock();
        if (!currentComp) {
            return QVariantList();
        }

        auto layer = currentComp->layerById(LayerID(layerId));
        if (!layer) {
            return QVariantList();
        }

        // Use KeyframeModel to query keyframes
        static ArtifactTimelineKeyframeModel keyframeModel;
        auto keyframes = keyframeModel.getKeyframesFor(
            currentComp->id(),
            layer->id(),
            propertyPath
        );

        QVariantList result;
        for (const auto& kf : keyframes) {
            QString interpolationStr = QStringLiteral("Linear");
            if (kf.interpolation == InterpolationType::Bezier) {
                interpolationStr = QStringLiteral("Bezier");
            } else if (kf.interpolation == InterpolationType::EaseIn) {
                interpolationStr = QStringLiteral("EaseIn");
            } else if (kf.interpolation == InterpolationType::EaseOut) {
                interpolationStr = QStringLiteral("EaseOut");
            }

            QVariantMap item{
                {QStringLiteral("frame"), static_cast<int>(kf.time.value())},
                {QStringLiteral("value"), kf.value.toDouble()},
                {QStringLiteral("interpolation"), interpolationStr}
            };
            result.append(item);
        }

        return result;
    }

    // Delete keyframe at given frame number for a property path
    // Returns: {"success": bool}
    static QVariant deleteKeyframe(const QString& layerId, const QString& propertyPath, int frameNumber)
    {
        auto svc = ArtifactProjectService::instance();
        if (!svc) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("ProjectService not available")}
            };
        }

        auto currentComp = svc->currentComposition().lock();
        if (!currentComp) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("No active composition")}
            };
        }

        auto layer = currentComp->layerById(LayerID(layerId));
        if (!layer) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Layer not found")}
            };
        }

        // Use KeyframeModel to handle removal
        static ArtifactTimelineKeyframeModel keyframeModel;
        RationalTime time(frameNumber, 30);  // Same base fps as setKeyframe
        bool removed = keyframeModel.removeKeyframe(
            currentComp->id(),
            layer->id(),
            propertyPath,
            time
        );

        return QVariantMap{
            {QStringLiteral("success"), removed}
        };
    }

    static QVariant getLayerKeyframeSummary(const QString& layerId)
    {
        QVariantMap result;
        auto svc = ArtifactProjectService::instance();
        if (!svc) {
            result.insert(QStringLiteral("available"), false);
            return result;
        }

        auto currentComp = svc->currentComposition().lock();
        if (!currentComp) {
            result.insert(QStringLiteral("available"), false);
            return result;
        }

        auto layer = currentComp->layerById(LayerID(layerId));
        if (!layer) {
            result.insert(QStringLiteral("available"), false);
            return result;
        }

        static ArtifactTimelineKeyframeModel keyframeModel;
        const QStringList candidateProperties = {
            QStringLiteral("transform.position.x"),
            QStringLiteral("transform.position.y"),
            QStringLiteral("transform.position.z"),
            QStringLiteral("transform.rotation"),
            QStringLiteral("transform.scale.x"),
            QStringLiteral("transform.scale.y"),
            QStringLiteral("transform.scale.z"),
            QStringLiteral("transform.anchor.x"),
            QStringLiteral("transform.anchor.y"),
            QStringLiteral("opacity"),
            QStringLiteral("blending.opacity")
        };

        QVariantList properties;
        int keyframeCount = 0;
        for (const auto& propertyPath : candidateProperties) {
            const auto frames = keyframeModel.getKeyframesFor(currentComp->id(), layer->id(), propertyPath);
            if (frames.empty()) {
                continue;
            }

            const auto frameToInt = [](const RationalTime& time) -> int {
                const auto scale = time.scale();
                if (scale == 0) {
                    return static_cast<int>(time.value());
                }
                return static_cast<int>(time.rescaledTo(1));
            };

            QVariantMap entry;
            entry.insert(QStringLiteral("propertyPath"), propertyPath);
            entry.insert(QStringLiteral("keyframeCount"), static_cast<int>(frames.size()));
            entry.insert(QStringLiteral("firstFrame"), frameToInt(frames.front().time));
            entry.insert(QStringLiteral("lastFrame"), frameToInt(frames.back().time));
            properties.push_back(entry);
            keyframeCount += static_cast<int>(frames.size());
        }

        result.insert(QStringLiteral("available"), true);
        result.insert(QStringLiteral("layerId"), layerId);
        result.insert(QStringLiteral("propertyCount"), properties.size());
        result.insert(QStringLiteral("keyframeCount"), keyframeCount);
        result.insert(QStringLiteral("properties"), properties);
        return result;
    }

    static QVariant batchSetKeyframes(const QString& layerId, const QVariantList& keyframes)
    {
        QVariantMap result;
        auto svc = ArtifactProjectService::instance();
        if (!svc) {
            result.insert(QStringLiteral("success"), false);
            result.insert(QStringLiteral("error"), QStringLiteral("ProjectService not available"));
            return result;
        }

        auto currentComp = svc->currentComposition().lock();
        if (!currentComp) {
            result.insert(QStringLiteral("success"), false);
            result.insert(QStringLiteral("error"), QStringLiteral("No active composition"));
            return result;
        }

        auto layer = currentComp->layerById(LayerID(layerId));
        if (!layer) {
            result.insert(QStringLiteral("success"), false);
            result.insert(QStringLiteral("error"), QStringLiteral("Layer not found"));
            return result;
        }

        static ArtifactTimelineKeyframeModel keyframeModel;
        int addedCount = 0;
        int skippedCount = 0;
        QVariantList details;
        for (const QVariant& entryValue : keyframes) {
            const QVariantMap entry = entryValue.toMap();
            const QString propertyPath = entry.value(QStringLiteral("propertyPath")).toString();
            const int frameNumber = entry.value(QStringLiteral("frameNumber")).toInt();
            const double value = entry.value(QStringLiteral("value")).toDouble();
            if (propertyPath.trimmed().isEmpty() || frameNumber < 0) {
                ++skippedCount;
                continue;
            }
            const bool ok = keyframeModel.addKeyframe(
                currentComp->id(),
                layer->id(),
                propertyPath,
                RationalTime(frameNumber, 30),
                QVariant(value),
                InterpolationType::Linear);
            if (ok) {
                ++addedCount;
            } else {
                ++skippedCount;
            }
            details.append(QVariantMap{
                {QStringLiteral("propertyPath"), propertyPath},
                {QStringLiteral("frameNumber"), frameNumber},
                {QStringLiteral("value"), value},
                {QStringLiteral("success"), ok}
            });
        }

        result.insert(QStringLiteral("success"), skippedCount == 0);
        result.insert(QStringLiteral("addedCount"), addedCount);
        result.insert(QStringLiteral("skippedCount"), skippedCount);
        result.insert(QStringLiteral("details"), details);
        return result;
    }

    static QVariant batchRenameProjectItems(const QVariantList& items)
    {
        int renamedCount = 0;
        int skippedCount = 0;
        QVariantList details;

        for (const QVariant& itemValue : items) {
            const QVariantMap item = itemValue.toMap();
            const QString itemId = item.value(QStringLiteral("itemId")).toString().trimmed();
            const QString newName = item.value(QStringLiteral("newName")).toString().trimmed();
            if (itemId.isEmpty() || newName.isEmpty()) {
                ++skippedCount;
                details.push_back(QVariantMap{
                    {QStringLiteral("itemId"), itemId},
                    {QStringLiteral("success"), false},
                    {QStringLiteral("error"), QStringLiteral("Missing itemId or newName")}
                });
                continue;
            }

            const bool success = renameProjectItemById(itemId, newName).toBool();
            if (success) {
                ++renamedCount;
            } else {
                ++skippedCount;
            }
            details.push_back(QVariantMap{
                {QStringLiteral("itemId"), itemId},
                {QStringLiteral("newName"), newName},
                {QStringLiteral("success"), success}
            });
        }

        return QVariantMap{
            {QStringLiteral("success"), skippedCount == 0},
            {QStringLiteral("renamedCount"), renamedCount},
            {QStringLiteral("skippedCount"), skippedCount},
            {QStringLiteral("details"), details}
        };
    }

    static QVariant batchMoveProjectItemsToFolder(const QStringList& itemIds, const QString& parentFolderId)
    {
        int movedCount = 0;
        int skippedCount = 0;
        QVariantList details;

        for (const QString& rawItemId : itemIds) {
            const QString itemId = rawItemId.trimmed();
            if (itemId.isEmpty()) {
                ++skippedCount;
                continue;
            }

            const bool success = moveProjectItemToFolder(itemId, parentFolderId).toBool();
            if (success) {
                ++movedCount;
            } else {
                ++skippedCount;
            }
            details.push_back(QVariantMap{
                {QStringLiteral("itemId"), itemId},
                {QStringLiteral("parentFolderId"), parentFolderId},
                {QStringLiteral("success"), success}
            });
        }

        return QVariantMap{
            {QStringLiteral("success"), skippedCount == 0},
            {QStringLiteral("movedCount"), movedCount},
            {QStringLiteral("skippedCount"), skippedCount},
            {QStringLiteral("details"), details}
        };
    }

    // Phase 5: Group Layers
    //
    // Create a new group layer in the active composition.
    // Returns: {"success": bool, "groupLayerId": string}
    static QVariant createGroupLayer(const QString& name)
    {
        auto svc = ArtifactProjectService::instance();
        if (!svc) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("ProjectService not available")}
            };
        }

        auto currentComp = svc->currentComposition().lock();
        if (!currentComp) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("No active composition")}
            };
        }

        auto groupLayer = std::make_shared<ArtifactGroupLayer>();
        groupLayer->setLayerName(name.isEmpty() ? QStringLiteral("Layer Group") : name);

        // Add group layer to composition at top
        auto result = currentComp->appendLayerTop(groupLayer);
        if (!result.success) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Failed to add group layer to composition")}
            };
        }

        // Notify changes
        currentComp->changed();
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{currentComp->id().toString(), groupLayer->id().toString(),
                            LayerChangedEvent::ChangeType::Created});

        return QVariantMap{
            {QStringLiteral("success"), true},
            {QStringLiteral("groupLayerId"), groupLayer->id().toString()}
        };
    }

    // Move layers into a group layer.
    // Returns: {"success": bool, "movedCount": int}
    static QVariant moveLayersToGroup(const QStringList& layerIds, const QString& groupLayerId)
    {
        auto svc = ArtifactProjectService::instance();
        if (!svc) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("ProjectService not available")}
            };
        }

        auto currentComp = svc->currentComposition().lock();
        if (!currentComp) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("No active composition")}
            };
        }

        auto groupLayerPtr = currentComp->layerById(LayerID(groupLayerId));
        if (!groupLayerPtr) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Group layer not found")}
            };
        }

        auto* groupLayer = dynamic_cast<ArtifactGroupLayer*>(groupLayerPtr.get());
        if (!groupLayer) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Target layer is not a group layer")}
            };
        }

        int movedCount = 0;
        for (const auto& layerId : layerIds) {
            auto layerToMove = currentComp->layerById(LayerID(layerId));
            if (layerToMove) {
                // Remove from composition
                currentComp->removeLayerById(LayerID(layerId));
                
                // Add to group
                groupLayer->addChild(layerToMove);
                movedCount++;

                // Notify layer change
                ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                    LayerChangedEvent{currentComp->id().toString(), layerId,
                                    LayerChangedEvent::ChangeType::Modified});
            }
        }

        // Notify group change
        currentComp->changed();
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{currentComp->id().toString(), groupLayerId,
                            LayerChangedEvent::ChangeType::Modified});

        return QVariantMap{
            {QStringLiteral("success"), movedCount > 0},
            {QStringLiteral("movedCount"), movedCount}
        };
    }

    // Ungroup all layers in a group layer (move children back to composition).
    // Returns: {"success": bool, "unGroupedCount": int}
    static QVariant ungroupLayers(const QString& groupLayerId)
    {
        auto svc = ArtifactProjectService::instance();
        if (!svc) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("ProjectService not available")}
            };
        }

        auto currentComp = svc->currentComposition().lock();
        if (!currentComp) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("No active composition")}
            };
        }

        auto groupLayerPtr = currentComp->layerById(LayerID(groupLayerId));
        if (!groupLayerPtr) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Group layer not found")}
            };
        }

        auto* groupLayer = dynamic_cast<ArtifactGroupLayer*>(groupLayerPtr.get());
        if (!groupLayer) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Target layer is not a group layer")}
            };
        }

        // Get group's children before clearing
        auto childrenCopy = groupLayer->children();
        int unGroupedCount = 0;

        // Move each child back to composition
        for (const auto& child : childrenCopy) {
            if (child) {
                groupLayer->removeChild(child->id());
                currentComp->appendLayerTop(child);
                unGroupedCount++;

                ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                    LayerChangedEvent{currentComp->id().toString(), child->id().toString(),
                                    LayerChangedEvent::ChangeType::Modified});
            }
        }

        // Remove the now-empty group layer
        currentComp->removeLayerById(LayerID(groupLayerId));
        
        // Notify changes
        currentComp->changed();
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{currentComp->id().toString(), groupLayerId,
                            LayerChangedEvent::ChangeType::Removed});

        return QVariantMap{
            {QStringLiteral("success"), unGroupedCount > 0},
            {QStringLiteral("unGroupedCount"), unGroupedCount}
        };
    }

    // Create a solid 2D layer and append it to the composition
    static QVariant createSolidLayer(const QString& compositionId, const QString& name, const QString& colorHex, int width, int height)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("ProjectService not available")}
            };
        }

        std::shared_ptr<ArtifactAbstractComposition> comp;
        if (compositionId.isEmpty() || compositionId == QStringLiteral("current")) {
            comp = service->currentComposition().lock();
        } else {
            auto result = service->findComposition(CompositionID(compositionId));
            if (result.success) {
                comp = result.ptr.lock();
            }
        }

        if (!comp) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Composition not found")}
            };
        }

        QColor qColor(colorHex);
        if (!qColor.isValid()) {
            qColor = Qt::white;
        }
        FloatColor color(qColor.redF(), qColor.greenF(), qColor.blueF(), qColor.alphaF());

        auto solidLayer = std::make_shared<ArtifactSolid2DLayer>();
        solidLayer->setLayerName(name.isEmpty() ? QStringLiteral("Solid Layer") : name);
        solidLayer->setColor(color);
        
        QSize compSize = comp->settings().compositionSize();
        solidLayer->setSize(width > 0 ? width : compSize.width(), height > 0 ? height : compSize.height());

        auto result = comp->appendLayerTop(solidLayer);
        if (!result.success) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Failed to add layer to composition")}
            };
        }

        comp->changed();
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), solidLayer->id().toString(),
                            LayerChangedEvent::ChangeType::Created});

        return QVariantMap{
            {QStringLiteral("success"), true},
            {QStringLiteral("layerId"), solidLayer->id().toString()}
        };
    }

    // Replace a video/audio layer's media source file
    static QVariant replaceLayerSource(const QString& layerId, const QString& footageItemId)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }

        const auto* item = findProjectItemByIdPointer(footageItemId);
        if (!item || item->type() != eProjectItemType::Footage) {
            return false;
        }

        const auto* footage = static_cast<const FootageItem*>(item);
        const QString filePath = footage->filePath.trimmed();
        if (filePath.isEmpty()) {
            return false;
        }

        return service->replaceLayerSourceInCurrentComposition(LayerID(layerId), filePath);
    }

    // Split a layer into two layers at the specified frame time
    static QVariant splitLayerAtTime(const QString& layerId, int frameTime)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("ProjectService not available")}
            };
        }

        auto comp = service->currentComposition().lock();
        if (!comp) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("No active composition")}
            };
        }

        auto originalLayer = comp->layerById(LayerID(layerId));
        if (!originalLayer) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Layer not found")}
            };
        }

        int64_t inFrame = originalLayer->inPoint().framePosition();
        int64_t outFrame = originalLayer->outPoint().framePosition();

        if (frameTime <= inFrame || frameTime >= outFrame) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Frame time outside layer boundaries")}
            };
        }

        // 1. Adjust original layer outPoint
        originalLayer->setOutPoint(FramePosition(frameTime));

        // 2. Clone layer
        QJsonObject layerJson = originalLayer->toJson();
        layerJson.remove(QStringLiteral("id")); // Ensure new ID is generated

        auto newLayer = createArtifactLayerFromJson(layerJson);
        if (!newLayer) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Failed to clone layer")}
            };
        }

        // Adjust cloned layer inPoint
        newLayer->setInPoint(FramePosition(frameTime));
        newLayer->setOutPoint(FramePosition(outFrame));

        // 3. Find original layer index and insert cloned layer next to it
        QVector<ArtifactAbstractLayerPtr> layers = comp->allLayer();
        int originalIndex = -1;
        for (int i = 0; i < layers.size(); ++i) {
            if (layers[i] && layers[i]->id() == originalLayer->id()) {
                originalIndex = i;
                break;
            }
        }

        if (originalIndex != -1) {
            comp->insertLayerAt(newLayer, originalIndex + 1);
        } else {
            comp->appendLayerTop(newLayer);
        }

        comp->changed();

        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), originalLayer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), newLayer->id().toString(),
                            LayerChangedEvent::ChangeType::Created});

        return QVariantMap{
            {QStringLiteral("success"), true},
            {QStringLiteral("originalLayerId"), originalLayer->id().toString()},
            {QStringLiteral("newLayerId"), newLayer->id().toString()}
        };
    }

    // Delete a layer and shift all subsequent layers earlier in time
    static QVariant rippleDeleteLayer(const QString& layerId)
    {
        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }

        auto comp = service->currentComposition().lock();
        if (!comp) {
            return false;
        }

        auto targetLayer = comp->layerById(LayerID(layerId));
        if (!targetLayer) {
            return false;
        }

        int64_t inFrame = targetLayer->inPoint().framePosition();
        int64_t outFrame = targetLayer->outPoint().framePosition();
        int64_t duration = outFrame - inFrame;

        if (duration <= 0) {
            return false;
        }

        // Shift subsequent layers
        QVector<ArtifactAbstractLayerPtr> layers = comp->allLayer();
        for (const auto& layer : layers) {
            if (layer && layer->id() != targetLayer->id()) {
                int64_t layerIn = layer->inPoint().framePosition();
                if (layerIn >= outFrame) {
                    layer->setInPoint(FramePosition(layerIn - duration));
                    layer->setOutPoint(FramePosition(layer->outPoint().framePosition() - duration));
                    layer->setStartTime(FramePosition(layer->startTime().framePosition() - duration));

                    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                        LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                                        LayerChangedEvent::ChangeType::Modified});
                }
            }
        }

        // Remove the target layer
        comp->removeLayerById(targetLayer->id());
        comp->changed();

        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layerId,
                            LayerChangedEvent::ChangeType::Removed});

        return true;
    }

    // Align a list of layers sequentially, end-to-end
    static QVariant alignLayersSequentially(const QStringList& layerIds)
    {
        if (layerIds.size() < 2) {
            return false;
        }

        auto* service = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        if (!service) {
            return false;
        }

        auto comp = service->currentComposition().lock();
        if (!comp) {
            return false;
        }

        int64_t currentEndTime = 0;
        bool first = true;

        for (const QString& id : layerIds) {
            auto layer = comp->layerById(LayerID(id));
            if (!layer) {
                continue;
            }

            if (first) {
                currentEndTime = layer->outPoint().framePosition();
                first = false;
                continue;
            }

            int64_t layerIn = layer->inPoint().framePosition();
            int64_t shift = currentEndTime - layerIn;

            if (shift != 0) {
                layer->setInPoint(FramePosition(layer->inPoint().framePosition() + shift));
                layer->setOutPoint(FramePosition(layer->outPoint().framePosition() + shift));
                layer->setStartTime(FramePosition(layer->startTime().framePosition() + shift));

                ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                    LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                                    LayerChangedEvent::ChangeType::Modified});
            }

            currentEndTime = layer->outPoint().framePosition();
        }

        comp->changed();
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

    // Phase 7: Timeline Operations
    //
    // Playback control and timeline navigation for the active composition.
    
    // Playback State & Control
    
    // Start playback of the active composition.
    // Returns: bool (success)
    static QVariant playbackStart()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return false;
        }
        playback->play();
        return true;
    }

    // Pause playback of the active composition.
    // Returns: bool (success)
    static QVariant playbackPause()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return false;
        }
        playback->pause();
        return true;
    }

    // Stop playback and return to start frame.
    // Returns: bool (success)
    static QVariant playbackStop()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return false;
        }
        playback->stop();
        return true;
    }

    // Toggle between play and pause.
    // Returns: bool (success)
    static QVariant playbackToggle()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return false;
        }
        playback->togglePlayPause();
        return true;
    }

    // Get current playback state.
    // Returns: "playing" | "paused" | "stopped"
    static QVariant playbackGetState()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return QStringLiteral("unknown");
        }
        if (playback->isPlaying()) {
            return QStringLiteral("playing");
        } else if (playback->isPaused()) {
            return QStringLiteral("paused");
        } else {
            return QStringLiteral("stopped");
        }
    }

    // Frame Navigation
    
    // Get current playhead position in frames.
    // Returns: int (frame number)
    static QVariant playbackGetCurrentFrame()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return 0;
        }
        return static_cast<int>(playback->currentFrame().framePosition());
    }

    // Set playhead to specific frame.
    // Returns: bool (success)
    static QVariant playbackSetCurrentFrame(int frameNumber)
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback || frameNumber < 0) {
            return false;
        }
        playback->setCurrentFrame(FramePosition(frameNumber));
        return true;
    }

    // Move playhead to next frame.
    // Returns: bool (success)
    static QVariant playbackNextFrame()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return false;
        }
        playback->goToNextFrame();
        return true;
    }

    // Move playhead to previous frame.
    // Returns: bool (success)
    static QVariant playbackPreviousFrame()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return false;
        }
        playback->goToPreviousFrame();
        return true;
    }

    // Move playhead to start of composition.
    // Returns: bool (success)
    static QVariant playbackGoToStart()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return false;
        }
        playback->goToStartFrame();
        return true;
    }

    // Move playhead to end of composition.
    // Returns: bool (success)
    static QVariant playbackGoToEnd()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return false;
        }
        playback->goToEndFrame();
        return true;
    }

    static QVariant playbackSetInPoint()
    {
        auto* playback = ArtifactPlaybackService::instance();
        auto* points = playback ? playback->inOutPoints() : nullptr;
        if (!playback || !points) {
            return false;
        }
        points->setInPointAtCurrent(playback->currentFrame());
        return true;
    }

    static QVariant playbackSetOutPoint()
    {
        auto* playback = ArtifactPlaybackService::instance();
        auto* points = playback ? playback->inOutPoints() : nullptr;
        if (!playback || !points) {
            return false;
        }
        points->setOutPointAtCurrent(playback->currentFrame());
        return true;
    }

    static QVariant playbackClearInPoint()
    {
        auto* playback = ArtifactPlaybackService::instance();
        auto* points = playback ? playback->inOutPoints() : nullptr;
        if (!points) {
            return false;
        }
        points->clearInPoint();
        return true;
    }

    static QVariant playbackClearOutPoint()
    {
        auto* playback = ArtifactPlaybackService::instance();
        auto* points = playback ? playback->inOutPoints() : nullptr;
        if (!points) {
            return false;
        }
        points->clearOutPoint();
        return true;
    }

    static QVariant playbackClearAllPoints()
    {
        auto* playback = ArtifactPlaybackService::instance();
        auto* points = playback ? playback->inOutPoints() : nullptr;
        if (!points) {
            return false;
        }
        points->clearAllPoints();
        return true;
    }

    static QVariant playbackGoToNextMarker()
    {
        auto* playback = ArtifactPlaybackService::instance();
        auto* points = playback ? playback->inOutPoints() : nullptr;
        if (!playback || !points) {
            return false;
        }
        const auto next = points->nextMarker(playback->currentFrame());
        if (!next) {
            return false;
        }
        playback->setCurrentFrame(*next);
        return true;
    }

    static QVariant playbackGoToPreviousMarker()
    {
        auto* playback = ArtifactPlaybackService::instance();
        auto* points = playback ? playback->inOutPoints() : nullptr;
        if (!playback || !points) {
            return false;
        }
        const auto prev = points->previousMarker(playback->currentFrame());
        if (!prev) {
            return false;
        }
        playback->setCurrentFrame(*prev);
        return true;
    }

    static QVariant playbackGoToNextChapter()
    {
        auto* playback = ArtifactPlaybackService::instance();
        auto* points = playback ? playback->inOutPoints() : nullptr;
        if (!playback || !points) {
            return false;
        }
        const auto next = points->nextChapter(playback->currentFrame());
        if (!next) {
            return false;
        }
        playback->setCurrentFrame(*next);
        return true;
    }

    static QVariant playbackGoToPreviousChapter()
    {
        auto* playback = ArtifactPlaybackService::instance();
        auto* points = playback ? playback->inOutPoints() : nullptr;
        if (!playback || !points) {
            return false;
        }
        const auto prev = points->previousChapter(playback->currentFrame());
        if (!prev) {
            return false;
        }
        playback->setCurrentFrame(*prev);
        return true;
    }

    static QVariant playbackAddMarker(const QString& comment)
    {
        auto* playback = ArtifactPlaybackService::instance();
        auto* points = playback ? playback->inOutPoints() : nullptr;
        if (!playback || !points) {
            return false;
        }
        points->addMarker(playback->currentFrame(), comment, MarkerType::Comment);
        return true;
    }

    static QVariant playbackAddChapter(const QString& name)
    {
        auto* playback = ArtifactPlaybackService::instance();
        auto* points = playback ? playback->inOutPoints() : nullptr;
        if (!playback || !points) {
            return false;
        }
        points->addMarker(playback->currentFrame(), name, MarkerType::Chapter);
        return true;
    }

    static QVariant playbackClearAllMarkers()
    {
        auto* playback = ArtifactPlaybackService::instance();
        auto* points = playback ? playback->inOutPoints() : nullptr;
        if (!points) {
            return false;
        }
        points->clearAllMarkers();
        return true;
    }

    // Timeline Information
    
    // Get composition duration in frames.
    // Returns: int (frame count)
    static QVariant playbackGetDuration()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return 0;
        }
        const auto range = playback->frameRange();
        return static_cast<int>(range.endPosition().framePosition());
    }

    // Get playback frame range (in/out points).
    // Returns: {"start": int, "end": int}
    static QVariant playbackGetFrameRange()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return QVariantMap();
        }
        const auto range = playback->frameRange();
        return QVariantMap{
            {QStringLiteral("start"), static_cast<int>(range.startPosition().framePosition())},
            {QStringLiteral("end"), static_cast<int>(range.endPosition().framePosition())}
        };
    }

    // Set playback frame range (work area).
    // frameStart and frameEnd define the playback range.
    // Returns: bool (success)
    static QVariant playbackSetFrameRange(int frameStart, int frameEnd)
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback || frameStart < 0 || frameEnd < frameStart) {
            return false;
        }
        playback->setFrameRange(FrameRange(FramePosition(frameStart), FramePosition(frameEnd)));
        return true;
    }

    // Playback Settings
    
    // Get playback frame rate (fps).
    // Returns: double (frames per second)
    static QVariant playbackGetFrameRate()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return 0.0;
        }
        return static_cast<double>(playback->frameRate().framerate());
    }

    // Get playback speed multiplier.
    // Returns: float (1.0 = normal, 2.0 = 2x speed, 0.5 = half speed)
    static QVariant playbackGetSpeed()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return 1.0f;
        }
        return playback->playbackSpeed();
    }

    // Set playback speed multiplier.
    // Speed of 1.0 is normal, 2.0 is double speed, 0.5 is half speed.
    // Returns: bool (success)
    static QVariant playbackSetSpeed(double speed)
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback || speed <= 0.0) {
            return false;
        }
        playback->setPlaybackSpeed(static_cast<float>(speed));
        return true;
    }

    // Get looping state.
    // Returns: bool (true = looping enabled)
    static QVariant playbackGetLooping()
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return false;
        }
        return playback->isLooping();
    }

    // Enable or disable looping.
    // Returns: bool (success)
    static QVariant playbackSetLooping(bool enabled)
    {
        auto* playback = ArtifactPlaybackService::instance();
        if (!playback) {
            return false;
        }
        playback->setLooping(enabled);
        return true;
    }

    // Phase 6: Export
    //
    // Add composition to export queue with specified settings.
    // Supported formats: "mp4", "mov", "avi", "png", "jpg", "exr", "tiff"
    // Returns: {"success": bool, "jobIndex": int (if added)}
    static QVariant exportComposition(const QString& compositionId,
                                     const QString& outputPath,
                                     const QString& format,
                                     const QString& codec,
                                     int width,
                                     int height,
                                     double fps,
                                     int bitrateKbps)
    {
        auto* renderService = ArtifactRenderQueueService::instance();
        auto* projectService = ArtifactApplicationManager::instance() ? ArtifactApplicationManager::instance()->projectService() : nullptr;
        
        if (!renderService || !projectService) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("RenderQueue or Project service unavailable")}
            };
        }

        // Validate composition exists
        const auto found = projectService->findComposition(CompositionID(compositionId));
        if (!found.success) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Composition not found")}
            };
        }

        const auto comp = found.ptr.lock();
        if (!comp) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Composition is no longer available")}
            };
        }

        const QString resolvedCodec = [] (const QString& requestedFormat, const QString& requestedCodec) {
            const QString trimmedCodec = requestedCodec.trimmed();
            if (!trimmedCodec.isEmpty() &&
                trimmedCodec.compare(QStringLiteral("default"), Qt::CaseInsensitive) != 0) {
                return trimmedCodec;
            }
            return getDefaultCodecForFormat(requestedFormat).toString();
        }(format, codec);

        // Add to render queue
        renderService->addRenderQueueForComposition(comp->id(), comp->settings().compositionName().toQString());
        
        // Apply settings to last job (most recently added)
        const int jobCount = renderService->jobCount();
        if (jobCount <= 0) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Failed to add render queue")}
            };
        }

        const int jobIndex = jobCount - 1;
        
        // Set output path
        renderService->setJobOutputPathAt(jobIndex, outputPath);
        
        // Set output format, codec, and resolution
        renderService->setJobOutputSettingsAt(jobIndex, format, resolvedCodec, QStringLiteral("default"), width, height, fps, bitrateKbps);
        
        return QVariantMap{
            {QStringLiteral("success"), true},
            {QStringLiteral("jobIndex"), jobIndex}
        };
    }

    // Export current composition to file.
    // Convenience wrapper for exportComposition with current composition.
    // Returns: {"success": bool, "jobIndex": int (if added)}
    static QVariant exportCurrentComposition(const QString& outputPath,
                                           const QString& format,
                                           const QString& codec,
                                           int width,
                                           int height,
                                           double fps,
                                           int bitrateKbps)
    {
        const auto comp = currentComposition();
        if (!comp) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("No active composition")}
            };
        }

        return exportComposition(comp->id().toString(), outputPath, format, codec, width, height, fps, bitrateKbps);
    }

    // Export composition and wait until completed.
    // Returns: {"success": bool, "jobIndex": int, "status": QString, "error": QString (if failed)}
    static QVariant exportCompositionAndWait(const QString& compositionId,
                                            const QString& outputPath,
                                            const QString& format,
                                            const QString& codec,
                                            int width,
                                            int height,
                                            double fps,
                                            int bitrateKbps)
    {
        QVariantMap result = exportComposition(compositionId, outputPath, format, codec, width, height, fps, bitrateKbps).toMap();
        if (!result.value(QStringLiteral("success")).toBool()) {
            return result;
        }

        int jobIndex = result.value(QStringLiteral("jobIndex")).toInt();
        auto* renderService = ArtifactRenderQueueService::instance();
        if (!renderService) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("RenderQueueService became unavailable")}
            };
        }

        // Start the render job
        renderService->startRenderQueueAt(jobIndex);

        // Wait until completed (timeout of 5 minutes)
        using namespace std::chrono_literals;
        auto startTime = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::seconds(300);

        while (true) {
            QString status = renderService->jobStatusAt(jobIndex);
            if (status == QStringLiteral("Completed")) {
                return QVariantMap{
                    {QStringLiteral("success"), true},
                    {QStringLiteral("jobIndex"), jobIndex},
                    {QStringLiteral("status"), status}
                };
            }
            if (status == QStringLiteral("Failed") || status == QStringLiteral("Cancelled")) {
                return QVariantMap{
                    {QStringLiteral("success"), false},
                    {QStringLiteral("jobIndex"), jobIndex},
                    {QStringLiteral("status"), status},
                    {QStringLiteral("error"), renderService->jobErrorMessageAt(jobIndex)}
                };
            }

            // Timeout check
            if (std::chrono::steady_clock::now() - startTime > timeout) {
                renderService->cancelRenderQueueAt(jobIndex);
                return QVariantMap{
                    {QStringLiteral("success"), false},
                    {QStringLiteral("jobIndex"), jobIndex},
                    {QStringLiteral("status"), status},
                    {QStringLiteral("error"), QStringLiteral("Render timeout")}
                };
            }

            // Process Qt events to keep UI responsive and allow rendering thread/events to run
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    // Export active composition and wait until completed.
    // Returns: {"success": bool, "jobIndex": int, "status": QString, "error": QString (if failed)}
    static QVariant exportCurrentCompositionAndWait(const QString& outputPath,
                                                   const QString& format,
                                                   const QString& codec,
                                                   int width,
                                                   int height,
                                                   double fps,
                                                   int bitrateKbps)
    {
        const auto comp = currentComposition();
        if (!comp) {
            return QVariantMap{
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("No active composition")}
            };
        }

        return exportCompositionAndWait(comp->id().toString(), outputPath, format, codec, width, height, fps, bitrateKbps);
    }

    // Get list of supported export formats.
    // Returns: ["mp4", "mov", "avi", "png", "jpg", "exr", "tiff"]
    static QVariant getSupportedExportFormats()
    {
        return QStringList{
            QStringLiteral("mp4"),
            QStringLiteral("mov"),
            QStringLiteral("avi"),
            QStringLiteral("png"),
            QStringLiteral("jpg"),
            QStringLiteral("exr"),
            QStringLiteral("tiff")
        };
    }

    // Get default export codec for a given format.
    // Returns: codec name string
    static QVariant getDefaultCodecForFormat(const QString& format)
    {
        const QString fmt = format.toLower().trimmed();
        if (fmt == QStringLiteral("mp4") || fmt == QStringLiteral("mov")) {
            return QStringLiteral("h264");
        } else if (fmt == QStringLiteral("avi")) {
            return QStringLiteral("mpeg2video");
        } else if (fmt == QStringLiteral("png") || fmt == QStringLiteral("jpg") || fmt == QStringLiteral("tiff")) {
            return QStringLiteral("image");
        } else if (fmt == QStringLiteral("exr")) {
            return QStringLiteral("exr");
        }
        return QStringLiteral("h264");  // Default fallback
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

    // Template Slot Definition
    static QVariant defineTemplateSlot(const QString& layerId, const QString& slotName, const QString& defaultValue)
    {
        const auto comp = currentComposition();
        if (!comp) {
            return false;
        }
        const auto layer = comp->layerById(LayerID(layerId));
        if (!layer) {
            return false;
        }
        // Store slot metadata in layer note (simple approach)
        QString note = layer->layerNote();
        QJsonObject obj;
        if (!note.isEmpty()) {
            QJsonParseError err;
            obj = QJsonDocument::fromJson(note.toUtf8(), &err).object();
        }
        if (!obj.contains(QStringLiteral("templateSlots"))) {
            obj[QStringLiteral("templateSlots")] = QJsonObject{};
        }
        auto slotsObj = obj[QStringLiteral("templateSlots")].toObject();
        QJsonObject slot;
        slot[QStringLiteral("slotId")] = slotName;
        slot[QStringLiteral("displayName")] = slotName;
        slot[QStringLiteral("defaultValue")] = defaultValue;
        slot[QStringLiteral("required")] = true;
        slotsObj[slotName] = slot;
        obj[QStringLiteral("templateSlots")] = slotsObj;
        layer->setLayerNote(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
        return true;
    }

    static QVariantList listTemplateSlots()
    {
        const auto comp = currentComposition();
        QVariantList result;
        if (!comp) return result;
        const auto layers = comp->allLayer();

        for (const auto& layer : layers) {
            if (!layer) continue;
            const QString note = layer->layerNote();
            if (note.isEmpty()) continue;
            QJsonParseError err;
            const auto obj = QJsonDocument::fromJson(note.toUtf8(), &err).object();
            if (err.error != QJsonParseError::NoError) continue;
            const auto templateSlotsValue = obj[QStringLiteral("templateSlots")];
            if (!templateSlotsValue.isObject()) continue;

            const auto templateSlotsObj = templateSlotsValue.toObject();
            for (auto it = templateSlotsObj.begin(); it != templateSlotsObj.end(); ++it) {
                QJsonObject slot;
                slot[QStringLiteral("layerId")] = layer->id().toString();
                slot[QStringLiteral("slotId")] = it.key();
                slot[QStringLiteral("slotName")] = it.key();
                const auto slotData = it.value().toObject();
                slot[QStringLiteral("defaultValue")] = slotData[QStringLiteral("defaultValue")];
                slot[QStringLiteral("required")] = slotData.value(QStringLiteral("required")).toBool(true);
                result.append(QVariant(slot));
            }
        }
        return result;
    }

    static QVariant applyTemplateVariation(const QString& variationJson)
    {
        const auto comp = currentComposition();
        if (!comp) {
            return false;
        }
        QJsonParseError err;
        const auto variationObj = QJsonDocument::fromJson(variationJson.toUtf8(), &err).object();
        if (err.error != QJsonParseError::NoError) {
            return false;
        }

        const auto slotValues = variationObj[QStringLiteral("slotValues")].toArray();
        for (const auto& sv : slotValues) {
            if (!sv.isObject()) continue;
            const auto entry = sv.toObject();
            const QString layerId = entry[QStringLiteral("layerId")].toString();
            const QString slotName = entry[QStringLiteral("slotName")].toString();
            const QString value = entry[QStringLiteral("value")].toString();

            const auto layer = comp->layerById(LayerID(layerId));
            if (!layer) continue;

            // Preserve the slot map in the layer note so later passes can resolve it consistently.
            QJsonObject noteObj;
            const QString note = layer->layerNote();
            if (!note.isEmpty()) {
                QJsonParseError noteErr;
                noteObj = QJsonDocument::fromJson(note.toUtf8(), &noteErr).object();
            }
            auto slotValues = noteObj[QStringLiteral("slotValues")].toObject();
            QJsonObject slotValue;
            slotValue[QStringLiteral("slotId")] = slotName;
            slotValue[QStringLiteral("value")] = value;
            slotValues[slotName] = slotValue;
            noteObj[QStringLiteral("slotValues")] = slotValues;
            layer->setLayerNote(QString::fromUtf8(QJsonDocument(noteObj).toJson(QJsonDocument::Compact)));
        }
        return true;
    }

    static QVariant createTemplateFromVariation(const QVariantList& variations, const QString& outputPreset)
    {
        int count = 0;
        for (const auto& v : variations) {
            QJsonParseError err;
            const auto varObj = QJsonDocument::fromVariant(v).object();
            if (err.error != QJsonParseError::NoError) continue;

            // Add to render queue
            WorkspaceAutomation::instance().invokeMethod(QStringLiteral("addRenderQueueForCurrentComposition"), {});
            count++;
        }
        return count;
    }

    static QVariantMap resolveExportMatrixCell(const QString& matrixJson,
                                               const QString& variantId,
                                               const QString& presetId,
                                               const QString& baseOutputPath)
    {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(matrixJson.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            return {
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("Invalid matrix JSON")}
            };
        }

        const ArtifactCore::ExportMatrix matrix = ArtifactCore::ExportMatrix::fromJson(doc.object());
        const auto resolved = ArtifactCore::resolveExportMatrixCell(matrix, variantId, presetId, baseOutputPath);
        return {
            {QStringLiteral("success"), true},
            {QStringLiteral("cell"), resolved.toJson()}
        };
    }

    static QVariantList createExportMatrixJobs(const QString& matrixJson, const QString& baseOutputPath)
    {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(matrixJson.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            return {};
        }

        const ArtifactCore::ExportMatrix matrix = ArtifactCore::ExportMatrix::fromJson(doc.object());
        QVariantList jobs;
        for (const auto& variant : matrix.variants) {
            for (const auto& preset : matrix.presets) {
                const auto cell = ArtifactCore::resolveExportMatrixCell(matrix, variant.id, preset.id, baseOutputPath);
                if (!cell.enabled) {
                    continue;
                }
                jobs.append(cell.toJson());
            }
        }
        return jobs;
    }

    static QVariantMap queueExportMatrixForCurrentComposition(const QString& matrixJson,
                                                              const QString& baseOutputPath)
    {
        auto* renderService = ArtifactRenderQueueService::instance();
        const auto comp = currentComposition();
        if (!renderService || !comp) {
            return {
                {QStringLiteral("success"), false},
                {QStringLiteral("error"), QStringLiteral("No active composition or render service")}
            };
        }

        const QVariantList jobs = createExportMatrixJobs(matrixJson, baseOutputPath);
        int added = 0;
        for (const auto& jobValue : jobs) {
            const QJsonObject jobObj = QJsonDocument::fromVariant(jobValue).object();
            const QString exportName = jobObj.value(QStringLiteral("exportName")).toString();
            const QString outputPath = jobObj.value(QStringLiteral("outputPath")).toString();
            const QString format = jobObj.value(QStringLiteral("format")).toString();
            const QString codec = jobObj.value(QStringLiteral("codec")).toString();
            const int width = jobObj.value(QStringLiteral("width")).toInt();
            const int height = jobObj.value(QStringLiteral("height")).toInt();
            const double fps = jobObj.value(QStringLiteral("fps")).toDouble();
            const int bitrateKbps = jobObj.value(QStringLiteral("bitrateKbps")).toInt();

            renderService->addRenderQueueForComposition(comp->id(), exportName);
            const int jobIndex = renderService->jobCount() - 1;
            if (jobIndex < 0) {
                continue;
            }
            if (!outputPath.isEmpty()) {
                renderService->setJobOutputPathAt(jobIndex, outputPath);
            }
            renderService->setJobOutputSettingsAt(jobIndex, format, codec, width, height, fps, bitrateKbps);
            ++added;
        }

        return {
            {QStringLiteral("success"), true},
            {QStringLiteral("addedCount"), added}
        };
    }

    static QVariantList listAvailableEffects()
    {
        auto* effectService = ArtifactEffectService::instance();
        if (!effectService) return {};
        const auto effects = effectService->availableEffects();
        QVariantList list;
        for (const auto& eff : effects) {
            QJsonObject obj;
            obj[QStringLiteral("id")] = eff.id.toString();
            obj[QStringLiteral("displayName")] = eff.displayName;
            list.append(obj);
        }
        return list;
    }
};

} // namespace Artifact
