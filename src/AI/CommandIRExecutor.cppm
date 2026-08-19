module;

#include <QString>
#include <QStringView>
#include <QVariantMap>
#include <QVariantList>
#include <memory>

export module Artifact.AI.CommandIRExecutor;

import std;
import Core.AI.CommandIR;
import Artifact.AI.WorkspaceAutomation;

namespace
{
    QVariant invokeWorkspaceAutomationMethod(QStringView name, const QVariantList& args)
    {
        return Artifact::WorkspaceAutomation::instance().invokeMethod(name, args);
    }
}

export namespace Artifact {

class CommandIRExecutor : public ArtifactCore::CommandExecutor {
public:
    ArtifactCore::CommandResult validate(const ArtifactCore::CommandRequest& request) const override
    {
        return ArtifactCore::CommandIR::validate(request);
    }

    ArtifactCore::CommandResult execute(const ArtifactCore::CommandRequest& request) const override
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = ArtifactCore::CommandIR::undoLabelForType(request.type);

        const ArtifactCore::CommandResult validation = ArtifactCore::CommandIR::validate(request);
        if (!validation.valid) {
            return validation;
        }

        const QString type = request.type.trimmed();

        if (type == QStringLiteral("set_property")) {
            return executeSetProperty(request);
        }
        if (type == QStringLiteral("set_keyframes")) {
            return executeSetKeyframes(request);
        }
        if (type == QStringLiteral("batch_set_keyframes")) {
            return executeBatchSetKeyframes(request);
        }
        if (type == QStringLiteral("move_layer")) {
            return executeMoveLayer(request);
        }
        if (type == QStringLiteral("rename_layer")) {
            return executeRenameLayer(request);
        }
        if (type == QStringLiteral("add_effect")) {
            return executeAddEffect(request);
        }
        if (type == QStringLiteral("create_layer")) {
            return executeCreateLayer(request);
        }
        if (type == QStringLiteral("delete_layer")) {
            return executeDeleteLayer(request);
        }
        if (type == QStringLiteral("set_layer_visible")) {
            return executeSetLayerVisible(request);
        }
        if (type == QStringLiteral("set_layer_blend_mode")) {
            return executeSetLayerBlendMode(request);
        }
        if (type == QStringLiteral("set_layer_opacity")) {
            return executeSetLayerOpacity(request);
        }
        if (type == QStringLiteral("set_playback_state")) {
            return executeSetPlaybackState(request);
        }
        if (type == QStringLiteral("export_composition")) {
            return executeExportComposition(request);
        }
        if (type == QStringLiteral("remove_effect")) {
            return executeRemoveEffect(request);
        }
        if (type == QStringLiteral("get_scene_info")) {
            return executeGetSceneInfo(request);
        }
        if (type == QStringLiteral("get_layer_info")) {
            return executeGetLayerInfo(request);
        }
        if (type == QStringLiteral("create_composition")) {
            return executeCreateComposition(request);
        }
        if (type == QStringLiteral("switch_composition")) {
            return executeSwitchComposition(request);
        }
        if (type == QStringLiteral("import_asset")) {
            return executeImportAsset(request);
        }
        if (type == QStringLiteral("duplicate_layer")) {
            return executeDuplicateLayer(request);
        }
        if (type == QStringLiteral("group_layers")) {
            return executeGroupLayers(request);
        }
        if (type == QStringLiteral("set_layer_parent")) {
            return executeSetLayerParent(request);
        }
        if (type == QStringLiteral("split_layer")) {
            return executeSplitLayer(request);
        }
        if (type == QStringLiteral("get_keyframes")) {
            return executeGetKeyframes(request);
        }
        if (type == QStringLiteral("delete_keyframe")) {
            return executeDeleteKeyframe(request);
        }
        if (type == QStringLiteral("set_work_area")) {
            return executeSetWorkArea(request);
        }
        if (type == QStringLiteral("add_marker")) {
            return executeAddMarker(request);
        }
        if (type == QStringLiteral("set_effect_parameter")) {
            return executeSetEffectParameter(request);
        }
        if (type == QStringLiteral("set_effect_enabled")) {
            return executeSetEffectEnabled(request);
        }
        if (type == QStringLiteral("list_available_effects")) {
            return executeListAvailableEffects(request);
        }
        if (type == QStringLiteral("start_render_queue")) {
            return executeStartRenderQueue(request);
        }
        if (type == QStringLiteral("get_render_status")) {
            return executeGetRenderStatus(request);
        }
        if (type == QStringLiteral("list_compositions")) {
            return executeListCompositions(request);
        }
        if (type == QStringLiteral("list_project_items")) {
            return executeListProjectItems(request);
        }

        result.error = QStringLiteral("Unsupported command type: ") + type;
        return result;
    }

private:
    static bool validateKeyframePayload(const QString& propertyPath,
                                        const QVariantList& keys,
                                        QString* errorOut)
    {
        if (propertyPath.trimmed().isEmpty() || keys.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Each keyframe batch requires a propertyPath and at least one keyframe");
            }
            return false;
        }
        for (const QVariant& keyVar : keys) {
            const QVariantMap key = keyVar.toMap();
            if (!key.contains(QStringLiteral("frame")) ||
                !key.value(QStringLiteral("value")).isValid()) {
                if (errorOut) {
                    *errorOut = QStringLiteral("Each keyframe requires frame and value");
                }
                return false;
            }
        }
        return true;
    }

    static QString resolveEffectId(const QString& layerId, int effectIndex)
    {
        if (layerId.trimmed().isEmpty() || effectIndex < 0) return {};
        const QVariant effects = invokeWorkspaceAutomationMethod(
            QStringLiteral("getLayerEffects"), QVariantList{layerId});
        const QVariantList list = effects.toList();
        if (effectIndex >= list.size()) return {};
        return list.at(effectIndex).toMap().value(QStringLiteral("id")).toString();
    }

    static ArtifactCore::CommandResult executeSetProperty(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Set Property");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        const QString propertyPath = request.target.value(QStringLiteral("propertyPath")).toString();
        const QVariant value = request.arguments.value(QStringLiteral("value"));

        // Route property path to the correct setter method
        const QString normPath = propertyPath.trimmed().toLower();
        QVariant success(false);
        if (normPath == QStringLiteral("position")) {
            QVariantList args{layerId, value};
            success = invokeWorkspaceAutomationMethod(QStringLiteral("setLayerPosition"), args);
        } else if (normPath == QStringLiteral("scale")) {
            QVariantList args{layerId, value};
            success = invokeWorkspaceAutomationMethod(QStringLiteral("setLayerScale"), args);
        } else if (normPath == QStringLiteral("rotation")) {
            QVariantList args{layerId, value};
            success = invokeWorkspaceAutomationMethod(QStringLiteral("setLayerRotation"), args);
        } else if (normPath == QStringLiteral("opacity")) {
            QVariantList args{layerId, value};
            success = invokeWorkspaceAutomationMethod(QStringLiteral("setLayerOpacity"), args);
        } else if (normPath == QStringLiteral("effect.enabled")) {
            QVariantList args{layerId, value};
            success = invokeWorkspaceAutomationMethod(QStringLiteral("setLayerEffectEnabled"), args);
        } else {
            // Try generic effect parameter setter
            QVariantList args{layerId, propertyPath, value};
            success = invokeWorkspaceAutomationMethod(QStringLiteral("setLayerEffectParameter"), args);
        }

        result.success = success.isValid() && success.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("setProperty failed for path: ") + propertyPath;
            result.errorCode = QStringLiteral("PROPERTY_INVALID");
            result.retryable = true;
        }
        return result;
    }

    static ArtifactCore::CommandResult executeSetKeyframes(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Set Keyframes");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        const QString propertyPath = request.target.value(QStringLiteral("propertyPath")).toString();
        const QVariantList keys = request.arguments.value(QStringLiteral("keys")).toList();

        if (keys.isEmpty()) {
            result.error = QStringLiteral("setKeyframes requires at least one keyframe");
            result.errorCode = QStringLiteral("COMMAND_INVALID");
            result.retryable = true;
            QVariantMap details;
            details.insert(QStringLiteral("keyframeCount"), 0);
            details.insert(QStringLiteral("succeeded"), 0);
            details.insert(QStringLiteral("failed"), 0);
            result.diagnostics = details;
            return result;
        }

        QString payloadError;
        if (!validateKeyframePayload(propertyPath, keys, &payloadError)) {
            result.error = payloadError;
            result.errorCode = QStringLiteral("COMMAND_INVALID");
            result.retryable = true;
            return result;
        }

        int succeeded = 0;
        int failed = 0;
        for (const QVariant& keyVar : keys) {
            const QVariantMap key = keyVar.toMap();
            int frame = key.value(QStringLiteral("frame")).toInt();
            QVariant value = key.value(QStringLiteral("value"));
            QVariantList args{layerId, propertyPath, frame, value};
            const QVariant operation = invokeWorkspaceAutomationMethod(
                QStringLiteral("setKeyframe"), args);
            if (operation.isValid() && operation.toMap().value(QStringLiteral("success")).toBool()) {
                ++succeeded;
            } else {
                ++failed;
            }
        }

        result.success = failed == 0;
        result.executed = succeeded > 0 && failed == 0;
        if (!result.success) {
            result.error = QStringLiteral("One or more keyframes could not be set");
        }
        QVariantMap details;
        details.insert(QStringLiteral("keyframeCount"), static_cast<int>(keys.size()));
        details.insert(QStringLiteral("succeeded"), succeeded);
        details.insert(QStringLiteral("failed"), failed);
        result.diagnostics = details;
        return result;
    }

    static ArtifactCore::CommandResult executeBatchSetKeyframes(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Batch Set Keyframes");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        const QVariantList batches = request.arguments.value(QStringLiteral("batches")).toList();

        if (batches.isEmpty()) {
            result.error = QStringLiteral("batch_set_keyframes requires at least one batch");
            result.errorCode = QStringLiteral("COMMAND_INVALID");
            result.retryable = true;
            QVariantMap details;
            details.insert(QStringLiteral("batchCount"), 0);
            details.insert(QStringLiteral("totalKeyframes"), 0);
            details.insert(QStringLiteral("succeeded"), 0);
            details.insert(QStringLiteral("failed"), 0);
            result.diagnostics = details;
            return result;
        }

        for (const QVariant& batchVar : batches) {
            const QVariantMap batch = batchVar.toMap();
            QString payloadError;
            if (!validateKeyframePayload(
                    batch.value(QStringLiteral("propertyPath")).toString(),
                    batch.value(QStringLiteral("keys")).toList(),
                    &payloadError)) {
                result.error = payloadError;
                result.errorCode = QStringLiteral("COMMAND_INVALID");
                result.retryable = true;
                return result;
            }
        }

        int totalKeyframes = 0;
        int succeeded = 0;
        int failed = 0;
        for (const QVariant& batchVar : batches) {
            const QVariantMap batch = batchVar.toMap();
            const QString propPath = batch.value(QStringLiteral("propertyPath")).toString();
            const QVariantList keys = batch.value(QStringLiteral("keys")).toList();

            for (const QVariant& keyVar : keys) {
                const QVariantMap key = keyVar.toMap();
                int frame = key.value(QStringLiteral("frame")).toInt();
                QVariant value = key.value(QStringLiteral("value"));
                QVariantList args{layerId, propPath, frame, value};
                const QVariant operation = invokeWorkspaceAutomationMethod(
                    QStringLiteral("setKeyframe"), args);
                if (operation.isValid() && operation.toMap().value(QStringLiteral("success")).toBool()) {
                    ++succeeded;
                } else {
                    ++failed;
                }
                ++totalKeyframes;
            }
        }

        result.success = totalKeyframes > 0 && failed == 0;
        result.executed = succeeded > 0 && failed == 0;
        if (!result.success) {
            if (totalKeyframes == 0) {
                result.error = QStringLiteral("batch_set_keyframes requires at least one keyframe");
                result.errorCode = QStringLiteral("COMMAND_INVALID");
                result.retryable = true;
            } else {
                result.error = QStringLiteral("One or more keyframes could not be set");
            }
        }
        QVariantMap details;
        details.insert(QStringLiteral("batchCount"), static_cast<int>(batches.size()));
        details.insert(QStringLiteral("totalKeyframes"), totalKeyframes);
        details.insert(QStringLiteral("succeeded"), succeeded);
        details.insert(QStringLiteral("failed"), failed);
        result.diagnostics = details;
        return result;
    }

    static ArtifactCore::CommandResult executeMoveLayer(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Move Layer");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        int newIndex = request.arguments.value(QStringLiteral("newIndex")).toInt();

        QVariantList args{layerId, newIndex};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("moveLayerInCurrentComposition"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("moveLayerInCurrentComposition failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeRenameLayer(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Rename Layer");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        const QString newName = request.arguments.value(QStringLiteral("newName")).toString();

        QVariantList args{layerId, newName};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("renameLayerInCurrentComposition"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("renameLayerInCurrentComposition failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeAddEffect(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Add Effect");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        const QString effectType = request.arguments.value(QStringLiteral("effectType")).toString();

        QVariantList args{layerId, effectType};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("addLayerEffect"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("addLayerEffect failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeCreateLayer(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Create Layer");

        const QString layerType = request.arguments.value(QStringLiteral("layerType")).toString().trimmed();
        const QString layerName = request.arguments.value(QStringLiteral("layerName")).toString();
        QVariant ok(false);

        if (layerType == QStringLiteral("solid")) {
            QVariantList args{layerName, 1920, 1080};
            ok = invokeWorkspaceAutomationMethod(QStringLiteral("addSolidLayerToCurrentComposition"), args);
        } else if (layerType == QStringLiteral("text")) {
            QVariantList args{layerName};
            ok = invokeWorkspaceAutomationMethod(QStringLiteral("addTextLayerToCurrentComposition"), args);
        } else if (layerType == QStringLiteral("null")) {
            QVariantList args{layerName, 1920, 1080};
            ok = invokeWorkspaceAutomationMethod(QStringLiteral("addNullLayerToCurrentComposition"), args);
        } else {
            result.error = QStringLiteral("Unsupported layer type: ") + layerType;
            result.errorCode = QStringLiteral("UNSUPPORTED_COMMAND");
            return result;
        }

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("createLayer failed for type: ") + layerType;
        }
        return result;
    }

    static ArtifactCore::CommandResult executeDeleteLayer(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Delete Layer");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        QVariantList args{layerId};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("removeLayerFromCurrentComposition"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("deleteLayer failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeSetLayerVisible(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Set Layer Visibility");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        bool visible = request.arguments.value(QStringLiteral("visible")).toBool();

        QVariantList args{layerId, visible};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("setLayerVisibleInCurrentComposition"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("setLayerVisible failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeSetLayerBlendMode(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Set Blend Mode");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        int blendMode = request.arguments.value(QStringLiteral("blendMode")).toInt();

        QVariantList args{layerId, blendMode};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("setLayerBlendModeInCurrentComposition"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("setLayerBlendMode failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeSetLayerOpacity(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Set Layer Opacity");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        double opacity = request.arguments.value(QStringLiteral("opacity")).toDouble();

        QVariantList args{layerId, opacity};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("setLayerOpacityInCurrentComposition"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("setLayerOpacity failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeSetPlaybackState(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Set Playback State");

        const QString state = request.arguments.value(QStringLiteral("state")).toString().trimmed();
        QVariant ok(false);

        if (state == QStringLiteral("play") || state == QStringLiteral("toggle")) {
            QVariantList args;
            ok = invokeWorkspaceAutomationMethod(QStringLiteral("playbackToggle"), args);
        } else if (state == QStringLiteral("pause")) {
            QVariantList args;
            ok = invokeWorkspaceAutomationMethod(QStringLiteral("playbackPause"), args);
        } else if (state == QStringLiteral("stop")) {
            QVariantList args;
            ok = invokeWorkspaceAutomationMethod(QStringLiteral("playbackStop"), args);
        } else if (state == QStringLiteral("seek")) {
            int frame = request.arguments.value(QStringLiteral("frame")).toInt();
            QVariantList args{frame};
            ok = invokeWorkspaceAutomationMethod(QStringLiteral("playbackSetCurrentFrame"), args);
        } else {
            result.error = QStringLiteral("Unsupported playback state: ") + state;
            result.errorCode = QStringLiteral("UNSUPPORTED_COMMAND");
            return result;
        }

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success && result.error.isEmpty()) {
            result.error = QStringLiteral("setPlaybackState failed for state: ") + state;
        }
        return result;
    }

    static ArtifactCore::CommandResult executeExportComposition(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Export Composition");

        QVariantList args;
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("addRenderQueueForCurrentComposition"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("exportComposition failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeRemoveEffect(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Remove Effect");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        int effectIndex = request.arguments.value(QStringLiteral("effectIndex")).toInt();
        const QString effectId = resolveEffectId(layerId, effectIndex);
        if (effectId.isEmpty()) {
            result.error = QStringLiteral("Effect index is out of range");
            result.errorCode = QStringLiteral("TARGET_NOT_FOUND");
            result.retryable = false;
            return result;
        }

        QVariantList args{layerId, effectId};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("removeLayerEffect"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("removeEffect failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeGetSceneInfo(const ArtifactCore::CommandRequest&)
    {
        ArtifactCore::CommandResult result;
        result.type = QStringLiteral("get_scene_info");
        result.undoLabel = QStringLiteral("Get Scene Info");

        const QVariant project = invokeWorkspaceAutomationMethod(QStringLiteral("projectSnapshot"), {});
        const QVariant composition = invokeWorkspaceAutomationMethod(QStringLiteral("currentCompositionSnapshot"), {});
        const QVariant layers = invokeWorkspaceAutomationMethod(QStringLiteral("listCurrentCompositionLayers"), {});
        const QVariant selection = invokeWorkspaceAutomationMethod(QStringLiteral("selectionSnapshot"), {});
        QVariantMap info;
        info.insert(QStringLiteral("project"), project);
        info.insert(QStringLiteral("composition"), composition);
        info.insert(QStringLiteral("layers"), layers);
        info.insert(QStringLiteral("selection"), selection);

        result.success = project.isValid() && composition.isValid() &&
                         layers.isValid() && selection.isValid();
        result.valid = true;
        result.executed = result.success;
        if (!result.success) result.error = QStringLiteral("Unable to collect scene information");
        QVariantMap details;
        details.insert(QStringLiteral("scene"), info);
        result.diagnostics = details;
        return result;
    }

    static ArtifactCore::CommandResult executeGetLayerInfo(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = QStringLiteral("get_layer_info");
        result.undoLabel = QStringLiteral("Get Layer Info");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();

        QVariantList args{layerId};
        QVariantList info;
        info.append(invokeWorkspaceAutomationMethod(QStringLiteral("getLayerPosition"), args));
        info.append(invokeWorkspaceAutomationMethod(QStringLiteral("getLayerScale"), args));
        info.append(invokeWorkspaceAutomationMethod(QStringLiteral("getLayerRotation"), args));
        info.append(invokeWorkspaceAutomationMethod(QStringLiteral("getLayerOpacity"), args));

        result.success = std::all_of(info.cbegin(), info.cend(),
                                     [](const QVariant& value) { return value.isValid(); });
        result.valid = true;
        result.executed = result.success;
        if (!result.success) result.error = QStringLiteral("Unable to collect layer information");
        QVariantMap details;
        details.insert(QStringLiteral("layerId"), layerId);
        details.insert(QStringLiteral("properties"), info);
        result.diagnostics = details;
        return result;
    }

    static ArtifactCore::CommandResult executeCreateComposition(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Create Composition");

        const QString name = request.arguments.value(QStringLiteral("name")).toString();
        int w = request.arguments.value(QStringLiteral("width"), 1920).toInt();
        int h = request.arguments.value(QStringLiteral("height"), 1080).toInt();

        QVariantList args{name, w, h};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("createComposition"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("createComposition failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeSwitchComposition(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Switch Composition");

        const QString compId = request.arguments.value(QStringLiteral("compositionId")).toString();
        QVariantList args{compId};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("changeCurrentComposition"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("switchComposition failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeImportAsset(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Import Asset");

        const QVariantList paths = request.arguments.value(QStringLiteral("filePaths")).toList();
        QStringList filePaths;
        for (const QVariant& p : paths) {
            filePaths.append(p.toString());
        }

        QVariantList args;
        args.append(QVariant::fromValue(filePaths));
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("importAssetsFromPaths"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("importAsset failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeDuplicateLayer(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Duplicate Layer");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        QVariantList args{layerId};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("duplicateLayerInCurrentComposition"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("duplicateLayer failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeGroupLayers(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Group Layers");

        const QString groupName = request.arguments.value(QStringLiteral("groupName")).toString();
        const QVariantList layerIds = request.arguments.value(QStringLiteral("layerIds")).toList();

        QVariantList args{groupName, 1920, 1080};
        const QVariant created = invokeWorkspaceAutomationMethod(
            QStringLiteral("createGroupLayer"), args);

        const QVariantMap createdMap = created.toMap();
        const QString groupLayerId = createdMap.value(QStringLiteral("groupLayerId")).toString();
        if (created.isValid() && createdMap.value(QStringLiteral("success")).toBool() &&
            !groupLayerId.isEmpty()) {
            QVariantList moveArgs{layerIds, groupLayerId};
            const QVariant moved = invokeWorkspaceAutomationMethod(
                QStringLiteral("moveLayersToGroup"), moveArgs);
            const QVariantMap movedMap = moved.toMap();
            result.success = moved.isValid() && movedMap.value(QStringLiteral("success")).toBool();
            result.executed = result.success;
            result.diagnostics.insert(QStringLiteral("groupLayerId"), groupLayerId);
            result.diagnostics.insert(QStringLiteral("movedCount"),
                                      movedMap.value(QStringLiteral("movedCount")));
        } else {
            result.success = false;
            result.executed = false;
        }
        if (!result.success) {
            result.error = QStringLiteral("groupLayers failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeSetLayerParent(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Set Layer Parent");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        const QString parentId = request.arguments.value(QStringLiteral("parentLayerId")).toString();

        QVariantList args{layerId, parentId};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("setLayerParentInCurrentComposition"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("setLayerParent failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeSplitLayer(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Split Layer");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        QVariantList args{layerId};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("splitLayerAtCurrentTime"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("splitLayer failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeGetKeyframes(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Get Keyframes");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        const QString propertyPath = request.target.value(QStringLiteral("propertyPath")).toString();

        QVariantList args{layerId, propertyPath};
        const QVariant kfs = invokeWorkspaceAutomationMethod(QStringLiteral("getKeyframes"), args);

        result.success = kfs.isValid();
        result.executed = result.success;
        result.valid = true;
        if (!result.success) result.error = QStringLiteral("Unable to read keyframes");
        QVariantMap details;
        details.insert(QStringLiteral("keyframes"), kfs);
        result.diagnostics = details;
        return result;
    }

    static ArtifactCore::CommandResult executeDeleteKeyframe(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Delete Keyframe");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        const QString propertyPath = request.target.value(QStringLiteral("propertyPath")).toString();
        int frame = request.arguments.value(QStringLiteral("frame")).toInt();

        QVariantList args{layerId, propertyPath, frame};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("deleteKeyframe"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("deleteKeyframe failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeSetWorkArea(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Set Work Area");

        int startFrame = request.arguments.value(QStringLiteral("startFrame")).toInt();
        int endFrame = request.arguments.value(QStringLiteral("endFrame")).toInt();

        QVariantList args{startFrame, endFrame};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("setWorkArea"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("setWorkArea failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeAddMarker(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Add Marker");

        int frame = request.arguments.value(QStringLiteral("frame")).toInt();
        const QString label = request.arguments.value(QStringLiteral("label")).toString();

        const QVariant seek = invokeWorkspaceAutomationMethod(
            QStringLiteral("seekTimeline"), QVariantList{frame});
        QVariant ok = seek.isValid() && seek.toBool()
            ? invokeWorkspaceAutomationMethod(QStringLiteral("playbackAddMarker"),
                                               QVariantList{label})
            : QVariant(false);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("addMarker failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeSetEffectParameter(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Set Effect Parameter");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        int effectIndex = request.arguments.value(QStringLiteral("effectIndex")).toInt();
        const QString paramName = request.arguments.value(QStringLiteral("paramName")).toString();
        const QVariant value = request.arguments.value(QStringLiteral("value"));
        const QString effectId = resolveEffectId(layerId, effectIndex);
        if (effectId.isEmpty()) {
            result.error = QStringLiteral("Effect index is out of range");
            result.errorCode = QStringLiteral("TARGET_NOT_FOUND");
            result.retryable = false;
            return result;
        }

        QVariantList args{layerId, effectId, paramName, value};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("setLayerEffectParameter"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("setEffectParameter failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeSetEffectEnabled(const ArtifactCore::CommandRequest& request)
    {
        ArtifactCore::CommandResult result;
        result.type = request.type;
        result.undoLabel = QStringLiteral("Set Effect Enabled");

        const QString layerId = request.target.value(QStringLiteral("layerId")).toString();
        int effectIndex = request.arguments.value(QStringLiteral("effectIndex")).toInt();
        bool enabled = request.arguments.value(QStringLiteral("enabled")).toBool();
        const QString effectId = resolveEffectId(layerId, effectIndex);
        if (effectId.isEmpty()) {
            result.error = QStringLiteral("Effect index is out of range");
            return result;
        }

        QVariantList args{layerId, effectId, enabled};
        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("setLayerEffectEnabled"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("setEffectEnabled failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeListAvailableEffects(const ArtifactCore::CommandRequest&)
    {
        ArtifactCore::CommandResult result;
        result.type = QStringLiteral("list_available_effects");
        result.undoLabel = QStringLiteral("List Available Effects");

        QVariantList effects;
        QVariant presets = invokeWorkspaceAutomationMethod(QStringLiteral("listLayerEffectPresets"), QVariantList());
        if (presets.isValid()) {
            effects.append(presets);
        }

        result.success = presets.isValid();
        result.executed = result.success;
        result.valid = true;
        if (!result.success) result.error = QStringLiteral("Unable to list available effects");
        QVariantMap details;
        details.insert(QStringLiteral("effects"), effects);
        result.diagnostics = details;
        return result;
    }

    static ArtifactCore::CommandResult executeStartRenderQueue(const ArtifactCore::CommandRequest&)
    {
        ArtifactCore::CommandResult result;
        result.type = QStringLiteral("start_render_queue");
        result.undoLabel = QStringLiteral("Start Render Queue");

        QVariant ok = invokeWorkspaceAutomationMethod(QStringLiteral("startAllRenderQueues"), QVariantList());

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("startRenderQueue failed");
        }
        return result;
    }

    static ArtifactCore::CommandResult executeGetRenderStatus(const ArtifactCore::CommandRequest&)
    {
        ArtifactCore::CommandResult result;
        result.type = QStringLiteral("get_render_status");
        result.undoLabel = QStringLiteral("Get Render Status");

        const QVariant status = invokeWorkspaceAutomationMethod(QStringLiteral("renderQueueSnapshot"), QVariantList());

        result.success = status.isValid();
        result.executed = result.success;
        result.valid = true;
        if (!result.success) result.error = QStringLiteral("Unable to read render queue status");
        QVariantMap details;
        details.insert(QStringLiteral("renderQueue"), status);
        result.diagnostics = details;
        return result;
    }

    static ArtifactCore::CommandResult executeListCompositions(const ArtifactCore::CommandRequest&)
    {
        ArtifactCore::CommandResult result;
        result.type = QStringLiteral("list_compositions");
        result.undoLabel = QStringLiteral("List Compositions");

        const QVariant comps = invokeWorkspaceAutomationMethod(QStringLiteral("listCompositions"), QVariantList());

        result.success = comps.isValid();
        result.executed = result.success;
        result.valid = true;
        if (!result.success) result.error = QStringLiteral("Unable to list compositions");
        QVariantMap details;
        details.insert(QStringLiteral("compositions"), comps);
        result.diagnostics = details;
        return result;
    }

    static ArtifactCore::CommandResult executeListProjectItems(const ArtifactCore::CommandRequest&)
    {
        ArtifactCore::CommandResult result;
        result.type = QStringLiteral("list_project_items");
        result.undoLabel = QStringLiteral("List Project Items");

        const QVariant items = invokeWorkspaceAutomationMethod(QStringLiteral("listProjectItems"), QVariantList());

        result.success = items.isValid();
        result.executed = result.success;
        result.valid = true;
        if (!result.success) result.error = QStringLiteral("Unable to list project items");
        QVariantMap details;
        details.insert(QStringLiteral("projectItems"), items);
        result.diagnostics = details;
        return result;
    }
};

inline const ArtifactCore::CommandExecutor& commandExecutor()
{
    static const CommandIRExecutor executor;
    return executor;
}

} // namespace Artifact
