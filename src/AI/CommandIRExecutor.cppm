module;

#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <memory>

export module Artifact.AI.CommandIRExecutor;

import std;
import Core.AI.CommandIR;
import Artifact.AI.WorkspaceAutomation;

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

        result.error = QStringLiteral("Unsupported command type: ") + type;
        return result;
    }

private:
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
            success = WorkspaceAutomation::invokeMethod(QStringLiteral("setLayerPosition"), args);
        } else if (normPath == QStringLiteral("scale")) {
            QVariantList args{layerId, value};
            success = WorkspaceAutomation::invokeMethod(QStringLiteral("setLayerScale"), args);
        } else if (normPath == QStringLiteral("rotation")) {
            QVariantList args{layerId, value};
            success = WorkspaceAutomation::invokeMethod(QStringLiteral("setLayerRotation"), args);
        } else if (normPath == QStringLiteral("opacity")) {
            QVariantList args{layerId, value};
            success = WorkspaceAutomation::invokeMethod(QStringLiteral("setLayerOpacity"), args);
        } else if (normPath == QStringLiteral("effect.enabled")) {
            QVariantList args{layerId, value};
            success = WorkspaceAutomation::invokeMethod(QStringLiteral("setLayerEffectEnabled"), args);
        } else {
            // Try generic effect parameter setter
            QVariantList args{layerId, propertyPath, value};
            success = WorkspaceAutomation::invokeMethod(QStringLiteral("setLayerEffectParameter"), args);
        }

        result.success = success.isValid() && success.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("setProperty failed for path: ") + propertyPath;
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

        for (const QVariant& keyVar : keys) {
            const QVariantMap key = keyVar.toMap();
            int frame = key.value(QStringLiteral("frame")).toInt();
            QVariant value = key.value(QStringLiteral("value"));
            QVariantList args{layerId, propertyPath, frame, value};
            WorkspaceAutomation::invokeMethod(QStringLiteral("setKeyframe"), args);
        }

        result.success = true;
        result.executed = true;
        QVariantMap details;
        details.insert(QStringLiteral("keyframeCount"), static_cast<int>(keys.size()));
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

        int totalKeyframes = 0;
        for (const QVariant& batchVar : batches) {
            const QVariantMap batch = batchVar.toMap();
            const QString propPath = batch.value(QStringLiteral("propertyPath")).toString();
            const QVariantList keys = batch.value(QStringLiteral("keys")).toList();

            for (const QVariant& keyVar : keys) {
                const QVariantMap key = keyVar.toMap();
                int frame = key.value(QStringLiteral("frame")).toInt();
                QVariant value = key.value(QStringLiteral("value"));
                QVariantList args{layerId, propPath, frame, value};
                WorkspaceAutomation::invokeMethod(QStringLiteral("setKeyframe"), args);
                ++totalKeyframes;
            }
        }

        result.success = true;
        result.executed = true;
        QVariantMap details;
        details.insert(QStringLiteral("batchCount"), static_cast<int>(batches.size()));
        details.insert(QStringLiteral("totalKeyframes"), totalKeyframes);
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
        QVariant ok = WorkspaceAutomation::invokeMethod(QStringLiteral("moveLayerInCurrentComposition"), args);

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
        QVariant ok = WorkspaceAutomation::invokeMethod(QStringLiteral("renameLayerInCurrentComposition"), args);

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
        QVariant ok = WorkspaceAutomation::invokeMethod(QStringLiteral("addLayerEffect"), args);

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
            ok = WorkspaceAutomation::invokeMethod(QStringLiteral("addSolidLayerToCurrentComposition"), args);
        } else if (layerType == QStringLiteral("text")) {
            QVariantList args{layerName};
            ok = WorkspaceAutomation::invokeMethod(QStringLiteral("addTextLayerToCurrentComposition"), args);
        } else if (layerType == QStringLiteral("null")) {
            QVariantList args{layerName, 1920, 1080};
            ok = WorkspaceAutomation::invokeMethod(QStringLiteral("addNullLayerToCurrentComposition"), args);
        } else {
            result.error = QStringLiteral("Unsupported layer type: ") + layerType;
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
        QVariant ok = WorkspaceAutomation::invokeMethod(QStringLiteral("removeLayerFromCurrentComposition"), args);

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
        QVariant ok = WorkspaceAutomation::invokeMethod(QStringLiteral("setLayerVisibleInCurrentComposition"), args);

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
        QVariant ok = WorkspaceAutomation::invokeMethod(QStringLiteral("setLayerBlendModeInCurrentComposition"), args);

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
        QVariant ok = WorkspaceAutomation::invokeMethod(QStringLiteral("setLayerOpacityInCurrentComposition"), args);

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
            ok = WorkspaceAutomation::invokeMethod(QStringLiteral("playbackToggle"), args);
        } else if (state == QStringLiteral("pause")) {
            QVariantList args;
            ok = WorkspaceAutomation::invokeMethod(QStringLiteral("playbackPause"), args);
        } else if (state == QStringLiteral("stop")) {
            QVariantList args;
            ok = WorkspaceAutomation::invokeMethod(QStringLiteral("playbackStop"), args);
        } else if (state == QStringLiteral("seek")) {
            int frame = request.arguments.value(QStringLiteral("frame")).toInt();
            QVariantList args{frame};
            ok = WorkspaceAutomation::invokeMethod(QStringLiteral("playbackSetCurrentFrame"), args);
        } else {
            result.error = QStringLiteral("Unsupported playback state: ") + state;
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
        QVariant ok = WorkspaceAutomation::invokeMethod(QStringLiteral("addRenderQueueForCurrentComposition"), args);

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

        QVariantList args{layerId, effectIndex};
        QVariant ok = WorkspaceAutomation::invokeMethod(QStringLiteral("removeLayerEffect"), args);

        result.success = ok.isValid() && ok.toBool();
        result.executed = result.success;
        if (!result.success) {
            result.error = QStringLiteral("removeEffect failed");
        }
        return result;
    }
};

inline const ArtifactCore::CommandExecutor& commandExecutor()
{
    static const CommandIRExecutor executor;
    return executor;
}

} // namespace Artifact
