
module;

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)
#pragma push_macro("emit")
#pragma push_macro("event")
#undef emit
#pragma pop_macro("event")
#pragma pop_macro("emit")

#include <memory>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <thread>
#include <vector>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <windows.h>

// #include <pybind11/pybind11.h>
#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QStyle>
#include <QLocale>
#include <QColor>
#include <QString>
#include <QCommandLineOption>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileOpenEvent>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFont>
#include <QIcon>
#include <QKeyEvent>
#include <QList>
#include <QMouseEvent>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QLoggingCategory>
#include <QLayout>
#include <QMessageBox>
#include <QMainWindow>
#include <QMetaType>
#include <QPointer>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRectF>
#include <QSizePolicy>
#include <QSize>
#include <QEvent>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QMetaObject>
#include <QObject>
#include <QTimer>
#include <QTimerEvent>
#include <QThread>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QUrl>
#include <QUuid>
#include <QWidget>
#include <QtCore/QtGlobal>
#include <filesystem>
#include <qthreadpool.h>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QScopeGuard>
#include <QSet>
#include <QHash>
#include <QStringList>
#include <QVariant>
#include <Diagnostics/WidgetCreationDiagnostics.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <atomic>
#include <cstddef>
#include <functional>
#include <ios>
#include <ostream>
#include <utility>

module Artifact.AppMain;

// DirectX 12 Agility SDK retail runtime selection.
// D3D12Core.dll is deployed beside the executable by CMake.
extern "C" __declspec(dllexport) const UINT D3D12SDKVersion = 619;
extern "C" __declspec(dllexport) const char* D3D12SDKPath = ".\\";

import Memory.SharedPtr;
import Core.AI.Context;
import Core.AI.McpBridge;
import Core.Diagnostics.Recorder;
import Core.Diagnostics.Snapshot;

import Application.AppSettings;
import Settings.Accessibility;
import Configuration.ConfigLayer;
import Configuration.LayeredConfigStore;
import Thread.PreciseTicker;
import Artifact.Widgets.PlaybackControlWidget;
import Artifact.Widgets.PlaybackControlTestWidget;
import Artifact.Layer.Factory;
import Artifact.Layer.Clone;
import Artifact.Layer.Image;
import Artifact.Layer.Shape;
import Artifact.Layer.Solid2D;
import Artifact.Layers.SolidImage;
import Transform;
import Draw;
import Glow;

import ImageProcessing;

// import hostfxr;
// import HalideTest;
import Graphics;
import SearchImage;
import UI.Layout.State;
import Core.FastSettingsStore;
import Artifact.Workspace.Modes;
import Artifact.AI.WorkspaceAutomation;

import Artifact.TestRunner;

import ImageProcessing.SpectralGlow;

namespace {
class StartupParallelismControl {
public:
  explicit StartupParallelismControl(unsigned int) {}
};
}

import Codec.Thumbnail.FFmpeg;
import AI.Client;

import Widgets.Render.Queue;
import Widgets.Utils.CSS;
import Widgets.CommonStyle;
import IO.ImageExporter;
import ArtifactStatusBar;
import Artifact.Application.Manager;
import Artifact.Tool.PuppetTool;
import Artifact.Application.CommandLine;
import Artifact.Application.InteractiveShell;
import Artifact.PythonAPI;
import Script.Python.Engine;
import Script.Python.CoreAPI;
import Diagnostics.CrashHandler;
import Translation.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.Playback;
import Artifact.Service.PlaybackShortcuts;
import Artifact.Service.Project;
import Artifact.Application.ProjectBundleIpc;
import Artifact.Project.Roles;
import Artifact.Project.Exporter;
import Undo.UndoManager;
import EnvironmentVariable;
import Core.TaskSystem;
import Core.Localization;
import Artifact.Widgets.UndoHistoryWidget;
import Artifact.Widgets.RecoveryWorkspace;
import Artifact.Widgets.PythonHookManagerWidget;
import Artifact.Widgets.ProjectManagerWidget;
import Artifact.Widgets.CompositionGraphWidget;
import Artifact.Widgets.ShaderGraphWidget;
import Artifact.Widgets.CompositionAudioMixer;
import Artifact.Widgets.DopeSheetWidget;
import Artifact.Widgets.Timeline;
import Artifact.Widgets.PerformanceProfilerWidget;
import Artifact.Widgets.AnimationTimelineWidget;
import Artifact.Widgets.AudioMiniWidget;
import Artifact.Widgets.HistoryTimelineWidget;
import Artifact.Widgets.AI.ArtifactAICloudWidget;
import Artifact.Widgets.CompositionEditor;
import Artifact.Widgets.CompositionRenderController;
import Artifact.Widgets.RenderLayerEditor;
import Artifact.Widgets.SoftwareRenderInspectors;
import Artifact.Widgets.MarkdownNoteEditorWidget;
import Artifact.Widgets.ProjectMemoWidget;
import Artifact.Widgets.ClipBufferWidget;
import Artifact.Widgets.CoordinateManager;
import Artifact.Widgets.CoordinateManager;
import Artifact.Widgets.PerformanceHUD;


import Artifact.Widgets.ContextShortcutHelperWidget;
import Artifact.Widgets.Render.QueueManager;
import Artifact.Render.Queue.Service;
import Core.Diagnostics.SessionLedger;
import Core.Diagnostics.Trace;
import Frame.Debug;

import Artifact.Widgets.RenderCenterWindow;
import Artifact.Render.Scheduler;
import Artifact.Contents.Viewer;
import Widgets.ToolBar;
import Widgets.Inspector;
import Widgets.AssetBrowser;
import Artifact.Widgets.ArtifactPropertyWidget;
import Property;
import Artifact.MainWindow;
import Artifact.Project.Manager;
import Artifact.Project.AutoSaveManager;
import Artifact.Script.Hooks;
import Artifact.Widgets.Test.ScrollPoC;

import Diagnostics.Logger;
import Artifact.Widgets.DebugConsoleWidget;
import Artifact.Widgets.FrameDebugViewWidget;
import Artifact.Widgets.AppDebuggerWidget;
import Artifact.Widgets.DebugRenderHarnessWidget;
import Artifact.Widgets.CollabPresenceWidget;
import Network.CollaborationWebSocket;
import Collaborate.Session;
import Collaborate.SessionAdapter;
import Collaborate.Operations;
import Event.Bus;
import Artifact.Event.Types;
import Artifact.Workspace.Manager;
import Artifact.Plugin.Loader;
import ArtifactCore.Plugin.Registry;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

QString currentProjectCollaborationFingerprint();

class CollaborationDockController final : public QObject {
public:
  struct LocalPresenceSnapshot {
    QString compositionId;
    qint64 playbackFrame = 0;
    bool hasPlaybackFrame = false;
    QStringList selectedLayerIds;
  };

  struct SelectedLayerNameCache {
    QString compositionId;
    QStringList layerIds;
    QStringList names;
  };

  CollaborationDockController(CollabPresenceWidget* widget, QObject* parent)
      : QObject(parent), widget_(widget) {
    socket_.setConnectionStateCallback(
        [this](const CollabConnectionState state) { onConnectionState(state); });
    socket_.setRoomReadyCallback([this]() {
      QString pendingClientId;
      qint64 pendingSequence = -1;
      auto* undoManager = UndoManager::instance();
      if (session_ && undoManager &&
          undoManager->pendingCollaborativeOperationIdentity(
              pendingClientId, pendingSequence)) {
        // room_ready follows the complete durable history replay. If the
        // pending operation was committed before a lost ACK, its history echo
        // has already acknowledged it. Absence here proves it was not stored.
        (void)session_->discardPendingLocalOperation(
            pendingClientId, pendingSequence);
        const bool rolledBack = undoManager->rejectCollaborativeOperation(
            pendingClientId, pendingSequence);
        if (widget_) {
          widget_->setLayerEditBlockedStatus(
              rolledBack
                  ? QStringLiteral("A pending edit was absent from server history and has been rolled back")
                  : QStringLiteral("A pending edit was absent from server history; local rollback failed, reconcile the project before continuing"));
        }
      }
      if (widget_) {
        widget_->setConnectionStatus(
            socket_.isReadOnly() ? QStringLiteral("Read-only session")
                                 : QStringLiteral("Connected"),
            true);
        if (undoManager && undoManager->hasPendingCollaborativeOperation()) {
          widget_->setLayerEditBlockedStatus(
              QStringLiteral("A previous edit is still awaiting server confirmation; further edits remain blocked until it is reconciled"));
        }
      }
      refreshSelectedLayerLockControl();
      refreshReviewContext();
    });
    socket_.setProtocolErrorCallback([this](const QString& message) {
      if (!widget_) return;
      widget_->setLayerEditBlockedStatus(message);
      if (!socket_.isRoomReady()) {
        widget_->setConnectionStatus(QStringLiteral("Room sync failed"), false);
        refreshSelectedLayerLockControl();
        refreshReviewContext();
      }
    });
    socket_.setOperationRejectedCallback(
        [this](const QString& clientId, const qint64 sequence,
               const QString& message) {
          if (session_) {
            (void)session_->discardPendingLocalOperation(clientId, sequence);
          }
          const bool rolledBack = UndoManager::instance() &&
              UndoManager::instance()->rejectCollaborativeOperation(
                  clientId, sequence);
          if (widget_) {
            widget_->setLayerEditBlockedStatus(
                QStringLiteral("Server rejected collaboration operation %1: %2%3")
                    .arg(sequence)
                    .arg(message)
                    .arg(rolledBack
                             ? QStringLiteral(". Local edit rolled back")
                             : QStringLiteral(". Local rollback could not be completed; reconcile the project before continuing")));
          }
        });
    if (widget_) {
      widget_->setConnectionActionHandler(
          [this](const QString& serverUrl, const QString& projectId,
                 const QString& userName, const QString& accessToken) {
            if (serverUrl.isEmpty() && projectId.isEmpty() && userName.isEmpty() &&
                accessToken.isEmpty()) {
              disconnectSession();
            } else {
              connectSession(serverUrl, projectId, userName, accessToken);
            }
          });
      widget_->setLayerLockActionHandler(
          [this](const QString& layerId, const bool acquire) {
            onLayerLockAction(layerId, acquire);
          });
      widget_->setReviewCommentHandler(
          [this](const QString& text) { return addReviewComment(text); });
      widget_->setReviewReplyHandler(
          [this](const QString& commentId, const QString& text) {
            return replyReviewComment(commentId, text);
          });
      widget_->setReviewResolveHandler(
          [this](const QString& commentId, const bool resolved) {
            return setReviewCommentResolved(commentId, resolved);
          });
      widget_->setReviewEditHandler(
          [this](const QString& commentId, const QString& text) {
            return editReviewComment(commentId, text);
          });
      widget_->setReviewDeleteHandler(
          [this](const QString& commentId) {
            return deleteReviewComment(commentId);
          });
      widget_->setReviewJumpHandler(
          [this](const QString& commentId) {
            return jumpToReviewAnchor(commentId);
          });
    }
    if (auto* undoManager = UndoManager::instance()) {
      undoManager->setLayerMutationGuard(
          [this](const QStringList& layerIds, QString& rejectionReason) {
            if (!session_) return true;
            if (!socket_.isRoomReady()) {
              rejectionReason = QStringLiteral(
                  "Layer edit blocked until collaboration lock state is synchronized; reconnect or disconnect the session");
              if (widget_) widget_->setLayerEditBlockedStatus(rejectionReason);
              return false;
            }
            if (socket_.isReadOnly()) {
              rejectionReason = QStringLiteral(
                  "Edit blocked: this collaboration session is read-only");
              if (widget_) widget_->setLayerEditBlockedStatus(rejectionReason);
              return false;
            }
            if (layerIds.isEmpty()) {
              rejectionReason = QStringLiteral(
                  "Edit blocked: could not resolve the layer owning this effect");
              if (widget_) widget_->setLayerEditBlockedStatus(rejectionReason);
              return false;
            }
            for (const QString& layerId : layerIds) {
              if (!session_->isLayerLocked(layerId)) {
                rejectionReason = QStringLiteral(
                    "Edit blocked: reserve this layer before editing in a collaboration session");
                if (widget_) widget_->setLayerEditBlockedStatus(rejectionReason);
                return false;
              }
              if (!session_->isLayerLockedByOther(layerId)) continue;
              const CollabLayerLock lock = session_->lockOwner(layerId);
              rejectionReason = lock.userName.isEmpty()
                  ? QStringLiteral("Edit blocked: layer is reserved by another collaborator")
                  : QStringLiteral("Edit blocked: layer is reserved by %1")
                        .arg(lock.userName);
              if (widget_) widget_->setLayerEditBlockedStatus(rejectionReason);
              return false;
            }
            return true;
          });
      undoManager->setCollaborationEditCallback(
          [this](const UndoCommand& command, const QString& action) {
            const bool preflight = action.startsWith(QStringLiteral("preflight."));
            if (action == QStringLiteral("pending")) {
              if (widget_) {
                widget_->setLayerEditBlockedStatus(
                    QStringLiteral("Waiting for the collaboration server to confirm the previous edit"));
              }
              return UndoManager::CollaborationEditDispatch{false, {}, -1};
            }
            if (!session_) {
              return UndoManager::CollaborationEditDispatch{true, {}, -1};
            }
            const UndoCommand* dispatchCommand =
                command.collaborationDispatchCommand();
            QString customOperationType;
            QString customLayerId;
            QJsonObject customOperationPayload;
            const QString operationAction = preflight
                ? action.mid(QStringLiteral("preflight.").size()) : action;
            const bool customCollaborationCommand = dispatchCommand &&
                dispatchCommand->buildCollaborationOperation(
                    operationAction, customOperationType, customLayerId,
                    customOperationPayload);
            const bool singlePropertyCommand = dispatchCommand &&
                dispatchCommand->commandType() ==
                    QStringLiteral("SetLayerPropertyValueCommand");
            const bool singleKeyframeCommand = dispatchCommand &&
                dispatchCommand->commandType() ==
                    QStringLiteral("SetLayerPropertyKeyframesCommand");
            const bool singleExpressionCommand = dispatchCommand &&
                dispatchCommand->commandType() ==
                    QStringLiteral("SetLayerPropertyExpressionCommand");
            const bool singleComponentsCommand = dispatchCommand &&
                dispatchCommand->commandType() ==
                    QStringLiteral("LayerComponentDescriptorSnapshotCommand");
            const bool singleStackCommand = dispatchCommand &&
                (dispatchCommand->commandType() ==
                     QStringLiteral("ClonerTransformStackSnapshotCommand") ||
                 dispatchCommand->commandType() ==
                     QStringLiteral("CloneEffectorStackSnapshotCommand"));
            const bool macroBatch = !dispatchCommand &&
                command.commandType() == QStringLiteral("MacroUndoCommand");
            if (!singlePropertyCommand && !singleKeyframeCommand &&
                !singleExpressionCommand && !singleComponentsCommand &&
                !singleStackCommand &&
                !macroBatch && !customCollaborationCommand) {
              if (widget_) {
                widget_->setLayerEditBlockedStatus(
                    QStringLiteral("This edit type is not synchronized in the current collaboration session"));
              }
              return UndoManager::CollaborationEditDispatch{false, {}, -1};
            }
            if (macroBatch && !command.canSerialize()) {
              return UndoManager::CollaborationEditDispatch{false, {}, -1};
            }
            if (singleKeyframeCommand && !dispatchCommand->canSerialize()) {
              if (widget_) {
                widget_->setLayerEditBlockedStatus(
                    QStringLiteral("This keyframe value cannot be represented by the collaboration protocol"));
              }
              return UndoManager::CollaborationEditDispatch{false, {}, -1};
            }
            if (singleComponentsCommand && !dispatchCommand->canSerialize()) {
              return UndoManager::CollaborationEditDispatch{false, {}, -1};
            }
            if (singleStackCommand && !dispatchCommand->canSerialize()) {
              return UndoManager::CollaborationEditDispatch{false, {}, -1};
            }
            const bool undo = action == QStringLiteral("undo");
            QJsonArray changes;
            QString keyframeLayerId;
            QJsonObject keyframePayload;
            QString expressionLayerId;
            QJsonObject expressionPayload;
            QString componentsLayerId;
            QJsonObject componentsPayload;
            QString stackLayerId;
            QJsonObject stackPayload;
            const auto appendPropertyChange =
                [&changes, undo](const QJsonObject& data) {
                  const QString layerId =
                      data.value(QStringLiteral("layerId")).toString();
                  const QString propertyPath =
                      data.value(QStringLiteral("propertyPath")).toString();
                  const QJsonValue before =
                      data.value(QStringLiteral("beforeValue"));
                  const QJsonValue after =
                      data.value(QStringLiteral("afterValue"));
                  if (layerId.isEmpty() || propertyPath.isEmpty() ||
                      before.isUndefined() || after.isUndefined()) {
                    return false;
                  }
                  changes.append(QJsonObject{
                      {QStringLiteral("layerId"), layerId},
                      {QStringLiteral("propertyPath"), propertyPath},
                      {QStringLiteral("expectedValue"), undo ? after : before},
                      {QStringLiteral("value"), undo ? before : after}});
                  return changes.size() <= 128;
                };
            const auto appendKeyframeChange =
                [&changes, undo](const QJsonObject& data) {
                  const QString layerId = data.value(QStringLiteral("layerId")).toString();
                  const QString propertyPath = data.value(QStringLiteral("propertyPath")).toString();
                  const QJsonValue before = data.value(QStringLiteral("before"));
                  const QJsonValue after = data.value(QStringLiteral("after"));
                  if (layerId.isEmpty() || propertyPath.isEmpty() ||
                      !before.isArray() || !after.isArray() ||
                      QJsonDocument(before.toArray()).toJson(QJsonDocument::Compact).size() > 262144 ||
                      QJsonDocument(after.toArray()).toJson(QJsonDocument::Compact).size() > 262144) return false;
                  QJsonObject change{
                      {QStringLiteral("kind"), QStringLiteral("keyframes")},
                      {QStringLiteral("layerId"), layerId},
                      {QStringLiteral("propertyPath"), propertyPath},
                      {QStringLiteral("expectedKeyframes"), undo ? after : before},
                      {QStringLiteral("keyframes"), undo ? before : after}};
                  const bool hasBeforeAnimatable = data.contains(QStringLiteral("beforeAnimatable"));
                  const bool hasAfterAnimatable = data.contains(QStringLiteral("afterAnimatable"));
                  if (hasBeforeAnimatable != hasAfterAnimatable) return false;
                  if (hasBeforeAnimatable) {
                    const QJsonValue beforeFlag = data.value(QStringLiteral("beforeAnimatable"));
                    const QJsonValue afterFlag = data.value(QStringLiteral("afterAnimatable"));
                    if (!beforeFlag.isBool() || !afterFlag.isBool()) return false;
                    change.insert(QStringLiteral("expectedAnimatable"), undo ? afterFlag : beforeFlag);
                    change.insert(QStringLiteral("animatable"), undo ? beforeFlag : afterFlag);
                  }
                  changes.append(change);
                  return changes.size() <= 128;
                };
            const auto appendExpressionChange =
                [&changes, undo](const QJsonObject& data) {
                  const QString layerId = data.value(QStringLiteral("layerId")).toString();
                  const QString propertyPath = data.value(QStringLiteral("propertyPath")).toString();
                  const QJsonValue before = data.value(QStringLiteral("beforeExpression"));
                  const QJsonValue after = data.value(QStringLiteral("afterExpression"));
                  if (layerId.isEmpty() || propertyPath.isEmpty() ||
                      !before.isString() || !after.isString() ||
                      before.toString().toUtf8().size() > 262144 ||
                      after.toString().toUtf8().size() > 262144) return false;
                  changes.append(QJsonObject{
                      {QStringLiteral("kind"), QStringLiteral("expression")},
                      {QStringLiteral("layerId"), layerId},
                      {QStringLiteral("propertyPath"), propertyPath},
                      {QStringLiteral("expectedExpression"), undo ? after : before},
                      {QStringLiteral("expression"), undo ? before : after}});
                  return changes.size() <= 128;
                };
            if (customCollaborationCommand) {
              // The command supplied its own collaboration wire payload above.
            } else if (singlePropertyCommand) {
              if (!appendPropertyChange(dispatchCommand->serialize())) {
                return UndoManager::CollaborationEditDispatch{false, {}, -1};
              }
            } else if (singleKeyframeCommand) {
              const QJsonObject data = dispatchCommand->serialize();
              keyframeLayerId =
                  data.value(QStringLiteral("layerId")).toString();
              const QString propertyPath =
                  data.value(QStringLiteral("propertyPath")).toString();
              const QJsonValue before = data.value(QStringLiteral("before"));
              const QJsonValue after = data.value(QStringLiteral("after"));
              if (keyframeLayerId.isEmpty() || propertyPath.isEmpty() ||
                  !before.isArray() || !after.isArray()) {
                return UndoManager::CollaborationEditDispatch{false, {}, -1};
              }
              keyframePayload.insert(QStringLiteral("propertyPath"), propertyPath);
              keyframePayload.insert(QStringLiteral("expectedKeyframes"),
                                     undo ? after : before);
              keyframePayload.insert(QStringLiteral("keyframes"),
                                     undo ? before : after);
              const bool hasBeforeAnimatable =
                  data.contains(QStringLiteral("beforeAnimatable"));
              const bool hasAfterAnimatable =
                  data.contains(QStringLiteral("afterAnimatable"));
              if (hasBeforeAnimatable != hasAfterAnimatable) {
                return UndoManager::CollaborationEditDispatch{false, {}, -1};
              }
              if (hasBeforeAnimatable) {
                const QJsonValue beforeAnimatable =
                    data.value(QStringLiteral("beforeAnimatable"));
                const QJsonValue afterAnimatable =
                    data.value(QStringLiteral("afterAnimatable"));
                if (!beforeAnimatable.isBool() || !afterAnimatable.isBool()) {
                  return UndoManager::CollaborationEditDispatch{false, {}, -1};
                }
                keyframePayload.insert(QStringLiteral("expectedAnimatable"),
                                       undo ? afterAnimatable : beforeAnimatable);
                keyframePayload.insert(QStringLiteral("animatable"),
                                       undo ? beforeAnimatable : afterAnimatable);
              }
            } else if (singleExpressionCommand) {
              const QJsonObject data = dispatchCommand->serialize();
              expressionLayerId = data.value(QStringLiteral("layerId")).toString();
              const QString propertyPath = data.value(QStringLiteral("propertyPath")).toString();
              const QJsonValue before = data.value(QStringLiteral("beforeExpression"));
              const QJsonValue after = data.value(QStringLiteral("afterExpression"));
              if (expressionLayerId.isEmpty() || propertyPath.isEmpty() ||
                  !before.isString() || !after.isString()) {
                return UndoManager::CollaborationEditDispatch{false, {}, -1};
              }
              expressionPayload.insert(QStringLiteral("propertyPath"), propertyPath);
              expressionPayload.insert(QStringLiteral("expectedExpression"), undo ? after : before);
              expressionPayload.insert(QStringLiteral("expression"), undo ? before : after);
            } else if (singleComponentsCommand) {
              const QJsonObject data = dispatchCommand->serialize();
              componentsLayerId = data.value(QStringLiteral("layerId")).toString();
              const QJsonValue before = data.value(QStringLiteral("before"));
              const QJsonValue after = data.value(QStringLiteral("after"));
              if (componentsLayerId.isEmpty() || !before.isObject() ||
                  !after.isObject() ||
                  QJsonDocument(before.toObject()).toJson(QJsonDocument::Compact).size() > 262144 ||
                  QJsonDocument(after.toObject()).toJson(QJsonDocument::Compact).size() > 262144) {
                return UndoManager::CollaborationEditDispatch{false, {}, -1};
              }
              componentsPayload.insert(QStringLiteral("expected"), undo ? after : before);
              componentsPayload.insert(QStringLiteral("value"), undo ? before : after);
            } else if (singleStackCommand) {
              const QJsonObject data = dispatchCommand->serialize();
              stackLayerId = data.value(QStringLiteral("layerId")).toString();
              const QJsonValue before = data.value(QStringLiteral("before"));
              const QJsonValue after = data.value(QStringLiteral("after"));
              const QString stackKind = dispatchCommand->commandType() ==
                      QStringLiteral("ClonerTransformStackSnapshotCommand")
                  ? QStringLiteral("clonerTransforms")
                  : QStringLiteral("cloneEffectors");
              if (stackLayerId.isEmpty() || !before.isArray() || !after.isArray() ||
                  QJsonDocument(before.toArray()).toJson(QJsonDocument::Compact).size() > 262144 ||
                  QJsonDocument(after.toArray()).toJson(QJsonDocument::Compact).size() > 262144) {
                return UndoManager::CollaborationEditDispatch{false, {}, -1};
              }
              stackPayload.insert(QStringLiteral("stackKind"), stackKind);
              stackPayload.insert(QStringLiteral("expected"), undo ? after : before);
              stackPayload.insert(QStringLiteral("value"), undo ? before : after);
            } else {
              const QJsonArray children =
                  command.serialize().value(QStringLiteral("children")).toArray();
              if (children.isEmpty() || children.size() > 128) {
                return UndoManager::CollaborationEditDispatch{false, {}, -1};
              }
              for (const QJsonValue& childValue : children) {
                const QJsonObject child = childValue.toObject();
                const QString childType = child.value(QStringLiteral("type")).toString();
                const QJsonObject childData = child.value(QStringLiteral("data")).toObject();
                const bool supported = childType == QStringLiteral("SetLayerPropertyValueCommand")
                    ? appendPropertyChange(childData)
                    : childType == QStringLiteral("SetLayerPropertyKeyframesCommand")
                        ? appendKeyframeChange(childData)
                        : childType == QStringLiteral("SetLayerPropertyExpressionCommand")
                            ? appendExpressionChange(childData)
                        : false;
                if (!supported) {
                  if (widget_) {
                    widget_->setLayerEditBlockedStatus(
                        QStringLiteral("Only batches of layer property value, keyframe, and expression edits are synchronized"));
                  }
                  return UndoManager::CollaborationEditDispatch{false, {}, -1};
                }
              }
              QJsonObject batchPayload;
              batchPayload.insert(QStringLiteral("changes"), changes);
              if (QJsonDocument(batchPayload).toJson(QJsonDocument::Compact).size() > 1048576) {
                return UndoManager::CollaborationEditDispatch{false, {}, -1};
              }
            }
            if (!adapter_ || !socket_.isRoomReady() || socket_.isReadOnly()) {
              if (preflight && widget_) {
                widget_->setLayerEditBlockedStatus(
                    QStringLiteral("Edit blocked until the collaboration room is ready and writable"));
              }
              return UndoManager::CollaborationEditDispatch{false, {}, -1};
            }
            if (preflight) {
              return UndoManager::CollaborationEditDispatch{true, {}, -1};
            }
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            CollabOperationData request;
            if (customCollaborationCommand) {
              request.type = customOperationType;
              request.layerId = customLayerId;
              request.payload = customOperationPayload;
              request.timestampMs = nowMs;
            } else if (singleKeyframeCommand) {
              request.type = QString::fromLatin1(kOpPropertyKeyframes);
              request.layerId = keyframeLayerId;
              request.payload = keyframePayload;
              request.timestampMs = nowMs;
            } else if (singleExpressionCommand) {
              request.type = QString::fromLatin1(kOpPropertyExpression);
              request.layerId = expressionLayerId;
              request.payload = expressionPayload;
              request.timestampMs = nowMs;
            } else if (singleComponentsCommand) {
              request.type = QString::fromLatin1(kOpLayerComponents);
              request.layerId = componentsLayerId;
              request.payload = componentsPayload;
              request.timestampMs = nowMs;
            } else if (singleStackCommand) {
              request.type = QString::fromLatin1(kOpLayerStack);
              request.layerId = stackLayerId;
              request.payload = stackPayload;
              request.timestampMs = nowMs;
            } else if (changes.size() == 1) {
              const QJsonObject change = changes.at(0).toObject();
              request = makePropertyCompareSetOperation(
                  session_->localClientId(),
                  change.value(QStringLiteral("layerId")).toString(),
                  change.value(QStringLiteral("propertyPath")).toString(),
                  change.value(QStringLiteral("expectedValue")),
                  change.value(QStringLiteral("value")), nowMs);
            } else {
              request.type = QString::fromLatin1(kOpPropertyBatch);
              request.timestampMs = nowMs;
              request.payload.insert(QStringLiteral("changes"), changes);
            }
            const CollabOperationData operation =
                session_->createLocalOperation(request, nowMs);
            const bool sent = adapter_->sendLocalOperation(operation);
            if (sent && widget_) {
              widget_->setLayerEditBlockedStatus(
                  QStringLiteral("Waiting for server confirmation of the collaborative edit"));
            } else if (!sent && widget_ &&
                       request.type == QStringLiteral("layer.add")) {
              widget_->setLayerEditBlockedStatus(
                  QStringLiteral("Layer add was blocked: its snapshot must be path-free, match the shared composition, and fit the collaboration size limit; asset synchronization is not enabled"));
            }
            return UndoManager::CollaborationEditDispatch{
                sent, operation.clientId, operation.sequence};
          });
    }
  }

  ~CollaborationDockController() override {
    if (auto* undoManager = UndoManager::instance()) {
      undoManager->setLayerMutationGuard({});
      undoManager->setCollaborationEditCallback({});
    }
    socket_.setConnectionStateCallback({});
    socket_.setRoomReadyCallback({});
    socket_.setProtocolErrorCallback({});
    socket_.setOperationRejectedCallback({});
    socket_.disconnect();
    if (widget_) {
      widget_->setConnectionActionHandler({});
      widget_->setLayerLockActionHandler({});
      widget_->setReviewCommentHandler({});
      widget_->setReviewReplyHandler({});
      widget_->setReviewResolveHandler({});
      widget_->setReviewEditHandler({});
      widget_->setReviewDeleteHandler({});
      widget_->setReviewJumpHandler({});
    }
    clearSession();
  }

private:
  void connectSession(const QString& serverUrl, const QString& projectId,
                      const QString& userName, const QString& accessToken) {
    const QString projectFingerprint =
        currentProjectCollaborationFingerprint();
    if (projectFingerprint.isEmpty()) {
      if (widget_) {
        widget_->setConnectionStatus(
            QStringLiteral("Cannot join: no project baseline is available"), false);
      }
      return;
    }
    disconnectSession();
    review_.clear();

    session_ = new CollaborationSession();
    const QString clientId =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString userId =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QColor userColor = QColor::fromHsv(
        static_cast<int>(qHash(userId) % 360U), 170, 230);
    session_->setLocalIdentity(clientId, userId, userName, userColor.name());

    adapter_ = new CollaborationSessionAdapter(socket_, *session_, projectId);
    adapter_->setParticipantsChangedCallback([this]() { refreshRoster(); });
    adapter_->setLockStateChangedCallback([this](const QString& layerId) {
      pendingLockReleases_.remove(layerId);
      refreshSelectedLayerLockControl();
    });
    adapter_->setOperationAppliedCallback(
        [this](const CollabOperationData& operation) {
          const bool acknowledgedLocalOperation = UndoManager::instance() &&
              UndoManager::instance()->acknowledgeCollaborativeOperation(
                  operation.clientId, operation.sequence);
          if (acknowledgedLocalOperation && widget_) {
            widget_->setLayerEditBlockedStatus(
                QStringLiteral("Collaboration edit confirmed by the server"));
          }
          if (applyReviewOperation(review_, operation)) {
            refreshReviewNotes();
            return;
          }
          if (operation.type == kOpPropertySet) {
            if (acknowledgedLocalOperation ||
                (session_ &&
                 operation.clientId == session_->localClientId())) {
              return;
            }
            const QJsonValue expected =
                operation.payload.value(QStringLiteral("expectedValue"));
            const QString propertyPath =
                operation.payload.value(QStringLiteral("propertyPath")).toString();
            if (!expected.isUndefined() &&
                UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativePropertySet(
                    operation.layerId, propertyPath, expected.toVariant(),
                    operation.payload.value(QStringLiteral("value")).toVariant())) {
              return;
            }
            if (widget_) {
              widget_->setLayerEditBlockedStatus(
                  QStringLiteral("A shared property edit conflicted with the local value and was not applied"));
            }
            return;
          }
          if (operation.type == kOpPropertyBatch) {
            if (acknowledgedLocalOperation ||
                (session_ &&
                 operation.clientId == session_->localClientId())) {
              return;
            }
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativePropertyBatch(
                    operation.payload)) {
              return;
            }
            if (widget_) {
              widget_->setLayerEditBlockedStatus(
                  QStringLiteral("A shared property batch conflicted with local values or could not be fully rolled back; inspect the affected properties"));
            }
            return;
          }
          if (operation.type == kOpPropertyKeyframes) {
            if (acknowledgedLocalOperation ||
                (session_ &&
                 operation.clientId == session_->localClientId())) {
              return;
            }
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativePropertyKeyframes(
                    operation.layerId, operation.payload)) {
              return;
            }
            if (widget_) {
              widget_->setLayerEditBlockedStatus(
                  QStringLiteral("A shared keyframe edit conflicted with local values or could not be applied"));
            }
            return;
          }
          if (operation.type == kOpPropertyExpression) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativePropertyExpression(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared expression edit conflicted with the local expression and was not applied"));
            return;
          }
          if (operation.type == kOpLayerComponents) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerComponents(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared component edit conflicted with local component state and was not applied"));
            return;
          }
          if (operation.type == kOpLayerStack) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerStack(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared stack edit conflicted with local stack state and was not applied"));
            return;
          }
          if (operation.type == kOpLayerAudioDeClickRanges) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerAudioDeClickRanges(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared audio de-click edit conflicted with local ranges and was not applied"));
            return;
          }
          if (operation.type == kOpLayerDeformation2D) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            const auto expected = operation.payload.value(QStringLiteral("expected"));
            const auto value = operation.payload.value(QStringLiteral("value"));
            auto* undo = UndoManager::instance();
            auto* app = ArtifactApplicationManager::instance();
            const auto layer = undo ? undo->resolveLayer(operation.layerId)
                                    : ArtifactAbstractLayerPtr{};
            auto* puppet = app ? app->puppetTool() : nullptr;
            if (layer && puppet && expected.isObject() && value.isObject() &&
                layer->deformation2DData() == expected.toObject()) {
              const QJsonObject expectedState = expected.toObject();
              const QJsonObject nextState = value.toObject();
              if (puppet->restoreLayerData(layer->id(), nextState, layer.get()) &&
                  layer->deformation2DData() == nextState) {
                undo->notifyAnythingChanged();
                ArtifactCore::globalEventBus().publish<ProjectChangedEvent>(
                    {QString(), QString()});
                return;
              }
              puppet->restoreLayerData(layer->id(), expectedState, layer.get());
            }
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared Deformation 2D edit conflicted with local state and was not applied"));
            return;
          }
          if (operation.type == kOpLayerSolidSize) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            const auto decodeSize = [](const QJsonValue& value, QSize& size) {
              if (!value.isObject()) return false;
              const auto object = value.toObject();
              const auto dimension = [](const QJsonValue& input, int& output) {
                if (!input.isDouble() || !std::isfinite(input.toDouble()) ||
                    std::floor(input.toDouble()) != input.toDouble() ||
                    input.toDouble() < 1 || input.toDouble() > 16384) return false;
                output = input.toInt();
                return true;
              };
              return dimension(object.value(QStringLiteral("width")), size.rwidth()) &&
                     dimension(object.value(QStringLiteral("height")), size.rheight());
            };
            QSize expectedSize, nextSize;
            auto* undo = UndoManager::instance();
            const auto layer = undo ? undo->resolveLayer(operation.layerId)
                                    : ArtifactAbstractLayerPtr{};
            bool applied = false;
            if (layer && decodeSize(operation.payload.value(QStringLiteral("expected")),
                                    expectedSize) &&
                decodeSize(operation.payload.value(QStringLiteral("value")), nextSize)) {
              const auto current = layer->sourceSize();
              if (current.width == expectedSize.width() &&
                  current.height == expectedSize.height()) {
                const auto solid = ArtifactCore::dynamicPointerCast<ArtifactSolid2DLayer>(layer);
                const auto solidImage = solid
                    ? ArtifactCore::SharedPtr<ArtifactSolidImageLayer>{}
                    : ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(layer);
                if (solid) solid->setSize(nextSize.width(), nextSize.height());
                else if (solidImage) solidImage->setSize(nextSize.width(), nextSize.height());
                const auto updated = layer->sourceSize();
                applied = (solid || solidImage) &&
                    updated.width == nextSize.width() &&
                    updated.height == nextSize.height();
                if (!applied) {
                  if (solid) solid->setSize(expectedSize.width(), expectedSize.height());
                  else if (solidImage) solidImage->setSize(expectedSize.width(), expectedSize.height());
                }
              }
            }
            if (applied) {
              layer->setDirty(LayerDirtyFlag::Source);
              layer->changed();
              undo->notifyAnythingChanged();
              ArtifactCore::globalEventBus().publish<ProjectChangedEvent>(
                  {QString(), QString()});
              return;
            }
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared solid size edit conflicted with local dimensions and was not applied"));
            return;
          }
          if (operation.type == kOpLayerSourceCrop) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            const auto expected = operation.payload.value(QStringLiteral("expected"));
            const auto value = operation.payload.value(QStringLiteral("value"));
            auto* undo = UndoManager::instance();
            const auto layer = undo ? undo->resolveLayer(operation.layerId)
                                    : ArtifactAbstractLayerPtr{};
            const auto imageLayer = ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer);
            bool applied = false;
            if (imageLayer && expected.isObject() && value.isObject() &&
                imageLayer->sourceCrop().toJson() == expected.toObject()) {
              applied = imageLayer->restoreSourceCropSnapshot(value.toObject()) &&
                  imageLayer->sourceCrop().toJson() == value.toObject();
              if (!applied && imageLayer->sourceCrop().toJson() != expected.toObject())
                imageLayer->restoreSourceCropSnapshot(expected.toObject());
            }
            if (applied) {
              undo->notifyAnythingChanged();
              ArtifactCore::globalEventBus().publish<ProjectChangedEvent>(
                  {QString(), QString()});
              return;
            }
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared source crop edit conflicted with local crop state and was not applied"));
            return;
          }
          if (operation.type == kOpLayerShapePolygon) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            const QJsonValue expected = operation.payload.value(QStringLiteral("expected"));
            const QJsonValue value = operation.payload.value(QStringLiteral("value"));
            auto* undo = UndoManager::instance();
            const auto layer = undo ? undo->resolveLayer(operation.layerId)
                                    : ArtifactAbstractLayerPtr{};
            const auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer);
            bool applied = false;
            if (shape && expected.isObject() && value.isObject() &&
                shape->customPolygonSnapshot() == expected.toObject()) {
              applied = shape->restoreCustomPolygonSnapshot(value.toObject()) &&
                  shape->customPolygonSnapshot() == value.toObject();
              if (!applied && shape->customPolygonSnapshot() != expected.toObject())
                shape->restoreCustomPolygonSnapshot(expected.toObject());
            }
            if (applied) {
              undo->notifyAnythingChanged();
              ArtifactCore::globalEventBus().publish<ProjectChangedEvent>(
                  {QString(), QString()});
              return;
            }
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared polygon edit conflicted with local shape points and was not applied"));
            return;
          }
          if (operation.type == kOpLayerShapePath) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            const QJsonValue expected = operation.payload.value(QStringLiteral("expected"));
            const QJsonValue value = operation.payload.value(QStringLiteral("value"));
            auto* undo = UndoManager::instance();
            const auto layer = undo ? undo->resolveLayer(operation.layerId)
                                    : ArtifactAbstractLayerPtr{};
            const auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer);
            bool applied = false;
            if (shape && expected.isObject() && value.isObject() &&
                shape->customGeometrySnapshot() == expected.toObject()) {
              applied = shape->restoreCustomGeometrySnapshot(value.toObject()) &&
                  shape->customGeometrySnapshot() == value.toObject();
              if (!applied && shape->customGeometrySnapshot() != expected.toObject())
                shape->restoreCustomGeometrySnapshot(expected.toObject());
            }
            if (applied) {
              undo->notifyAnythingChanged();
              ArtifactCore::globalEventBus().publish<ProjectChangedEvent>(
                  {QString(), QString()});
              return;
            }
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared path edit conflicted with local shape vertices and was not applied"));
            return;
          }
          if (operation.type == kOpLayerShapeOperator) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            const QJsonObject payload = operation.payload;
            const int operatorIndex = payload.value(QStringLiteral("operatorIndex")).toInt(-1);
            const QString field = payload.value(QStringLiteral("field")).toString();
            const double expected = payload.value(QStringLiteral("expectedValue")).toDouble();
            const double value = payload.value(QStringLiteral("value")).toDouble();
            auto* undo = UndoManager::instance();
            const auto layer = undo ? undo->resolveLayer(operation.layerId)
                                    : ArtifactAbstractLayerPtr{};
            const auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer);
            bool applied = false;
            if (shape && operatorIndex >= 0 &&
                operatorIndex < shape->shapeOperatorCount() && !field.isEmpty() &&
                shape->shapeOperatorValue(operatorIndex, field).toDouble() == expected) {
              const QString path = QStringLiteral("shape.operator.%1.%2")
                                       .arg(operatorIndex).arg(field);
              applied = shape->setLayerPropertyValue(path, QVariant(value)) &&
                  shape->shapeOperatorValue(operatorIndex, field).toDouble() == value;
              if (!applied &&
                  shape->shapeOperatorValue(operatorIndex, field).toDouble() != expected)
                shape->setLayerPropertyValue(path, QVariant(expected));
            }
            if (applied) {
              undo->notifyAnythingChanged();
              ArtifactCore::globalEventBus().publish<ProjectChangedEvent>(
                  {QString(), QString()});
              return;
            }
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared shape operator edit conflicted with local values and was not applied"));
            return;
          }
          if (operation.type == kOpLayerShapeContents) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            const QJsonObject payload = operation.payload;
            const QJsonValue expected = payload.value(QStringLiteral("expected"));
            const QJsonValue value = payload.value(QStringLiteral("value"));
            auto* undo = UndoManager::instance();
            const auto layer = undo ? undo->resolveLayer(operation.layerId)
                                    : ArtifactAbstractLayerPtr{};
            const auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer);
            bool applied = false;
            if (shape && expected.isObject() && value.isObject() &&
                shape->shapeContentsSnapshot() == expected.toObject()) {
              applied = shape->restoreShapeContentsSnapshot(value.toObject()) &&
                  shape->shapeContentsSnapshot() == value.toObject();
              if (!applied && shape->shapeContentsSnapshot() != expected.toObject())
                shape->restoreShapeContentsSnapshot(expected.toObject());
            }
            if (applied) {
              undo->notifyAnythingChanged();
              ArtifactCore::globalEventBus().publish<ProjectChangedEvent>(
                  {QString(), QString()});
              return;
            }
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared SVG import conflicted with local shape content and was not applied"));
            return;
          }
          if (operation.type == kOpLayerAnimationStack) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerAnimationStack(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared animation layer change conflicted with local stack state and was not applied"));
            return;
          }
          if (operation.type == kOpLayerText) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerText(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared text edit conflicted with local text and was not applied"));
            return;
          }
          if (operation.type == kOpLayerRename) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerRename(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared layer rename conflicted with the local name and was not applied"));
            return;
          }
          if (operation.type == kOpLayerVariant) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerVariant(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared layer variant change conflicted with local variants and was not applied"));
            return;
          }
          if (operation.type == kOpLayerBlendMode) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerBlendMode(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared blend mode change conflicted with local layer state and was not applied"));
            return;
          }
          if (operation.type == kOpLayerOpacity) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerOpacity(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared opacity change conflicted with local layer state and was not applied"));
            return;
          }
          if (operation.type == kOpLayerParent) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerParent(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared parent change conflicted with local hierarchy and was not applied"));
            return;
          }
          if (operation.type == kOpLayerVisibility) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerVisibility(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared visibility change conflicted with local layer state and was not applied"));
            return;
          }
          if (operation.type == kOpLayerFlag) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerFlag(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared solo/shy change conflicted with local layer state and was not applied"));
            return;
          }
          if (operation.type == kOpLayerEditLock) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerEditLock(
                    operation.layerId, operation.payload)) return;
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared project layer lock change conflicted with local state and was not applied"));
            return;
          }
          if (operation.type == kOpLayerAdd ||
              operation.type == kOpLayerRemove ||
              operation.type == kOpLayerReorder) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            if (UndoManager::instance() &&
                UndoManager::instance()->applyCollaborativeLayerMembership(
                    operation.type, operation.layerId, operation.payload)) return;
            if (widget_) {
              widget_->setLayerEditBlockedStatus(
                  QStringLiteral("A shared layer structure change conflicted with local composition state and was not applied"));
            }
          } else if (operation.type == kOpLayerMoveAtFrame) {
            if (acknowledgedLocalOperation ||
                (session_ && operation.clientId == session_->localClientId())) return;
            const QJsonObject payload = operation.payload;
            const qint64 frame = payload.value(QStringLiteral("frame")).toVariant().toLongLong();
            const qint64 timeScale = payload.value(QStringLiteral("timeScale")).toVariant().toLongLong();
            const float expectedX = static_cast<float>(
                payload.value(QStringLiteral("expectedX")).toDouble());
            const float expectedY = static_cast<float>(
                payload.value(QStringLiteral("expectedY")).toDouble());
            const float valueX = static_cast<float>(
                payload.value(QStringLiteral("x")).toDouble());
            const float valueY = static_cast<float>(
                payload.value(QStringLiteral("y")).toDouble());
            auto* undo = UndoManager::instance();
            const auto layer = undo ? undo->resolveLayer(operation.layerId)
                                    : ArtifactAbstractLayerPtr{};
            bool applied = false;
            if (layer && frame >= -1000000000LL && frame <= 1000000000LL &&
                timeScale >= 1 && timeScale <= 10000 &&
                std::isfinite(expectedX) && std::isfinite(expectedY) &&
                std::isfinite(valueX) && std::isfinite(valueY)) {
              const auto x = layer->getProperty(
                  QStringLiteral("transform.position.x"));
              const auto y = layer->getProperty(
                  QStringLiteral("transform.position.y"));
              const ArtifactCore::RationalTime time(frame, timeScale);
              if (x && y && x->interpolateValue(time).toFloat() == expectedX &&
                  y->interpolateValue(time).toFloat() == expectedY) {
                layer->transform3D().setPosition(time, valueX, valueY);
                applied = x->interpolateValue(time).toFloat() == valueX &&
                          y->interpolateValue(time).toFloat() == valueY;
                if (!applied)
                  layer->transform3D().setPosition(time, expectedX, expectedY);
              }
            }
            if (applied) {
              layer->setDirty(LayerDirtyFlag::Transform);
              layer->changed();
              undo->notifyAnythingChanged();
              if (auto* composition = static_cast<ArtifactAbstractComposition*>(
                      layer->composition())) {
                ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                    {composition->id().toString(), layer->id().toString(),
                     LayerChangedEvent::ChangeType::Modified});
              }
              return;
            }
            if (widget_) widget_->setLayerEditBlockedStatus(
                QStringLiteral("A shared layer move conflicted with the local transform at that frame and was not applied"));
          } else if (operation.type == kOpLayerTransform) {
            if (widget_) {
              widget_->setLayerEditBlockedStatus(
                  QStringLiteral("A shared layer edit was received but not applied locally; project-state sync is not enabled yet"));
            }
          } else if (widget_) {
            widget_->setLayerEditBlockedStatus(
                QStringLiteral("A collaboration operation was received but this client has no handler for it"));
          }
        });
    adapter_->setRemoteOperationValidator(
        [this](const CollabOperationData& operation) {
          return validateCollabReviewOperation(review_, operation).isEmpty();
        });

    if (widget_) {
      widget_->setLocalUser(clientId, userName, userColor);
      widget_->setConnectionStatus(QStringLiteral("Connecting"), true);
    }

    JoinMessage join;
    join.projectId = projectId;
    join.projectFingerprint = projectFingerprint;
    join.accessToken = accessToken;
    join.clientId = clientId;
    join.userId = userId;
    join.userName = userName;
    join.userColor = userColor.name();
    socket_.connectToServer(serverUrl, join);
  }

  void disconnectSession() {
    socket_.disconnect();
    clearSession();
    if (widget_) {
      widget_->syncRemoteUsers({});
      widget_->setLocalUser({}, {}, {});
      widget_->setConnectionStatus(QStringLiteral("Disconnected"), false);
    }
  }

  void clearSession() {
    delete adapter_;
    adapter_ = nullptr;
    delete session_;
    session_ = nullptr;
    pendingLockReleases_.clear();
    if (widget_) {
      widget_->setLayerLockControlState({}, false, false, false, false, {});
    }
    refreshTimelineLockIndicators();
  }

  void onConnectionState(const CollabConnectionState state) {
    if (!widget_) return;
    switch (state) {
      case CollabConnectionState::Disconnected:
        if (session_) session_->clearLocks();
        pendingLockReleases_.clear();
        widget_->setConnectionStatus(QStringLiteral("Disconnected"), false);
        widget_->syncRemoteUsers({});
        refreshSelectedLayerLockControl();
        refreshReviewNotes();
        break;
      case CollabConnectionState::Connecting:
        hasLastSentPresence_ = false;
        widget_->setConnectionStatus(QStringLiteral("Connecting"), true);
        refreshReviewNotes();
        break;
      case CollabConnectionState::Connected:
        widget_->setConnectionStatus(QStringLiteral("Connected; syncing room…"), true);
        refreshLocalPresence();
        refreshSelectedLayerLockControl();
        refreshReviewNotes();
        break;
      case CollabConnectionState::Reconnecting:
        hasLastSentPresence_ = false;
        if (session_) session_->clearLocks();
        pendingLockReleases_.clear();
        widget_->setConnectionStatus(QStringLiteral("Reconnecting"), true);
        refreshSelectedLayerLockControl();
        refreshReviewNotes();
        break;
      case CollabConnectionState::Error:
        if (session_) session_->clearLocks();
        pendingLockReleases_.clear();
        widget_->setConnectionStatus(QStringLiteral("Connection failed"), false);
        widget_->syncRemoteUsers({});
        widget_->setLocalUser({}, {}, {});
        refreshSelectedLayerLockControl();
        refreshReviewNotes();
        break;
    }
  }

public:
  void refreshTimelineLockIndicators() {
    QVector<LayerID> lockedLayerIds;
    if (session_) {
      const auto activeLocks = session_->activeLocks();
      lockedLayerIds.reserve(static_cast<qsizetype>(activeLocks.size()));
      for (const CollabLayerLock& lock : activeLocks) {
        const LayerID layerId(lock.layerId);
        if (!layerId.isNil()) lockedLayerIds.append(layerId);
      }
    }
    if (!parent()) return;
    const auto timelines = parent()->findChildren<ArtifactTimelineWidget*>();
    for (ArtifactTimelineWidget* timeline : timelines) {
      if (timeline) timeline->setCollaborationLockedLayers(lockedLayerIds);
    }
  }

  void refreshLocalPresence() {
    const QString previousCompositionId =
        pendingLocalPresence_.compositionId;
    pendingLocalPresence_.compositionId.clear();
    pendingLocalPresence_.playbackFrame = 0;
    pendingLocalPresence_.hasPlaybackFrame = false;
    auto* projectService = ArtifactProjectService::instance();
    if (projectService) {
      if (const auto composition =
              projectService->currentComposition().lock()) {
        pendingLocalPresence_.compositionId =
            composition->id().toString();
      } else {
        pendingLocalPresence_.compositionId.clear();
      }
    }

    if (auto* playbackService = ArtifactPlaybackService::instance()) {
      pendingLocalPresence_.playbackFrame =
          playbackService->currentFrame().framePosition();
      pendingLocalPresence_.hasPlaybackFrame = true;
    }

    pendingLocalPresence_.selectedLayerIds.clear();
    if (auto* app = ArtifactApplicationManager::instance()) {
      if (auto* selectionManager = app->layerSelectionManager()) {
        const auto selectedLayers = selectionManager->selectedLayersInOrder();
        for (const auto& layer : selectedLayers) {
          if (layer) {
            pendingLocalPresence_.selectedLayerIds.append(
                layer->id().toString());
          }
        }
      }
    }

    pendingPresenceDirty_ = true;
    refreshSelectedLayerLockControl();
    if (previousCompositionId != pendingLocalPresence_.compositionId) {
      refreshReviewNotes();
    } else {
      refreshReviewContext();
    }
    schedulePresenceFlush();
  }

  void refreshPlaybackFrame() {
    pendingLocalPresence_.playbackFrame = 0;
    pendingLocalPresence_.hasPlaybackFrame = false;
    if (auto* playbackService = ArtifactPlaybackService::instance()) {
      pendingLocalPresence_.playbackFrame =
          playbackService->currentFrame().framePosition();
      pendingLocalPresence_.hasPlaybackFrame = true;
    }
    pendingPresenceDirty_ = true;
    schedulePresenceFlush();
  }

private:

  void onLayerLockAction(const QString& layerId, const bool acquire) {
    if (!session_ || !adapter_ || !socket_.isRoomReady() ||
        socket_.isReadOnly() || layerId.isEmpty()) {
      refreshSelectedLayerLockControl();
      return;
    }
    if (acquire) {
      if (session_->isLayerLockedByOther(layerId) ||
          session_->hasPendingLockRequest(layerId)) {
        refreshSelectedLayerLockControl();
        return;
      }
      session_->requestLocalLock(layerId);
      if (!adapter_->sendLocalLockRequest(layerId)) {
        session_->releaseLocalLock(layerId);
      }
    } else {
      const CollabLayerLock lock = session_->lockOwner(layerId);
      if (lock.clientId != session_->localClientId()) {
        refreshSelectedLayerLockControl();
        return;
      }
      pendingLockReleases_.insert(layerId);
      if (!adapter_->sendLocalLockRelease(layerId)) {
        pendingLockReleases_.remove(layerId);
      }
    }
    refreshSelectedLayerLockControl();
  }

  void refreshSelectedLayerLockControl() {
    refreshTimelineLockIndicators();
    if (!widget_) return;
    const QString selectedLayerId =
        pendingLocalPresence_.selectedLayerIds.size() == 1
            ? pendingLocalPresence_.selectedLayerIds.constFirst()
            : QString();
    if (!session_ || !adapter_ || selectedLayerId.isEmpty()) {
      widget_->setLayerLockControlState(
          selectedLayerId,
          socket_.isRoomReady() && !socket_.isReadOnly(),
          false, false, false, {});
      return;
    }

    const bool locked = session_->isLayerLocked(selectedLayerId);
    const CollabLayerLock lock = session_->lockOwner(selectedLayerId);
    const bool heldByLocalUser =
        locked && lock.clientId == session_->localClientId();
    QString ownerName = lock.userName;
    if (locked && !heldByLocalUser && ownerName.isEmpty()) {
      ownerName = session_->participant(lock.clientId).userName;
    }
    const bool pending = session_->hasPendingLockRequest(selectedLayerId) ||
                         pendingLockReleases_.contains(selectedLayerId);
    widget_->setLayerLockControlState(
        selectedLayerId,
        socket_.isRoomReady() && !socket_.isReadOnly(),
        locked, heldByLocalUser,
        pending, ownerName, session_->lockDenialReason(selectedLayerId));
  }

  bool addReviewComment(const QString& text) {
    if (!session_ || !adapter_ || !socket_.isConnected() ||
        pendingLocalPresence_.compositionId.isEmpty() || text.trimmed().isEmpty() ||
        text.size() > 4096) {
      return false;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const CollabParticipant local = session_->localIdentity();
    const QString layerId =
        pendingLocalPresence_.selectedLayerIds.size() == 1
            ? pendingLocalPresence_.selectedLayerIds.constFirst()
            : QString();
    CollabComment comment;
    comment.commentId = newCollabId();
    comment.authorClientId = local.clientId;
    comment.authorUserId = local.userId;
    comment.authorName = local.userName;
    comment.compositionId = pendingLocalPresence_.compositionId;
    comment.layerId = layerId;
    comment.frame = pendingLocalPresence_.hasPlaybackFrame
                        ? pendingLocalPresence_.playbackFrame
                        : -1;
    comment.text = text.trimmed();
    comment.createdAtMs = nowMs;
    return sendReviewComment(comment, nowMs);
  }

  bool replyReviewComment(const QString& parentCommentId,
                          const QString& text) {
    if (!session_ || !adapter_ || !socket_.isConnected() ||
        text.trimmed().isEmpty() || text.size() > 4096) {
      return false;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const CollabParticipant local = session_->localIdentity();
    const CollabComment parent = review_.commentForId(parentCommentId);
    if (parent.commentId.isEmpty() || parent.isReply() || parent.resolved ||
        parent.deleted ||
        parent.compositionId != pendingLocalPresence_.compositionId) {
      return false;
    }
    CollabComment reply;
    reply.commentId = newCollabId();
    reply.parentCommentId = parentCommentId;
    reply.authorClientId = local.clientId;
    reply.authorUserId = local.userId;
    reply.authorName = local.userName;
    reply.compositionId = parent.compositionId;
    reply.layerId = parent.layerId;
    reply.frame = parent.frame;
    reply.text = text.trimmed();
    reply.createdAtMs = nowMs;
    return sendReviewComment(reply, nowMs);
  }

  bool sendReviewComment(const CollabComment& comment, const qint64 nowMs) {
    if (!session_ || !adapter_ || !socket_.isConnected() ||
        comment.compositionId != pendingLocalPresence_.compositionId) {
      return false;
    }
    const CollabOperationData request = makeReviewCommentAddOperation(
        comment.authorClientId, comment, nowMs);
    const CollabOperationData operation =
        session_->createLocalOperation(request, nowMs);
    return adapter_->sendLocalOperation(operation);
  }

  bool setReviewCommentResolved(const QString& commentId,
                               const bool resolved) {
    if (!session_ || !adapter_ || !socket_.isConnected()) return false;
    const CollabComment comment = review_.commentForId(commentId);
    if (comment.commentId.isEmpty() || comment.isReply() || comment.deleted ||
        comment.compositionId != pendingLocalPresence_.compositionId ||
        comment.resolved == resolved) {
      return false;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QString clientId = session_->localClientId();
    const CollabOperationData request = makeReviewCommentResolveOperation(
        clientId, commentId, resolved, nowMs);
    const CollabOperationData operation =
        session_->createLocalOperation(request, nowMs);
    return adapter_->sendLocalOperation(operation);
  }

  bool editReviewComment(const QString& commentId, const QString& text) {
    if (!session_ || !adapter_ || !socket_.isConnected() ||
        text.trimmed().isEmpty()) {
      return false;
    }
    const QString clientId = session_->localClientId();
    const CollabComment previous = review_.commentForId(commentId);
    if (previous.commentId.isEmpty() || previous.deleted ||
        previous.compositionId != pendingLocalPresence_.compositionId ||
        previous.authorClientId != clientId || text.size() > 4096) {
      return false;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const CollabOperationData request = makeReviewCommentEditOperation(
        clientId, session_->localIdentity().userName, commentId, text, nowMs);
    const CollabOperationData operation =
        session_->createLocalOperation(request, nowMs);
    return adapter_->sendLocalOperation(operation);
  }

  bool deleteReviewComment(const QString& commentId) {
    if (!session_ || !adapter_ || !socket_.isConnected()) return false;
    const QString clientId = session_->localClientId();
    const CollabComment comment = review_.commentForId(commentId);
    if (comment.commentId.isEmpty() || comment.deleted ||
        comment.compositionId != pendingLocalPresence_.compositionId ||
        !review_.canDeleteComment(commentId, clientId)) {
      return false;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const CollabOperationData request = makeReviewCommentRemoveOperation(
        clientId, commentId, nowMs);
    const CollabOperationData operation =
        session_->createLocalOperation(request, nowMs);
    return adapter_->sendLocalOperation(operation);
  }

  bool jumpToReviewAnchor(const QString& commentId) {
    if (!session_ || !socket_.isConnected()) return false;
    const CollabComment comment = review_.commentForId(commentId);
    if (comment.commentId.isEmpty() ||
        comment.compositionId != pendingLocalPresence_.compositionId) {
      return false;
    }
    auto* projectService = ArtifactProjectService::instance();
    if (!projectService) return false;
    const auto composition = projectService->currentComposition().lock();
    if (!composition || composition->id().toString() != comment.compositionId) {
      return false;
    }

    bool moved = false;
    if (!comment.layerId.isEmpty()) {
      const auto layer = composition->layerById(LayerID(comment.layerId));
      if (layer && !layer->isLocked() && !layer->isSelectionLocked()) {
        if (auto* selectionManager = ArtifactLayerSelectionManager::instance()) {
          selectionManager->selectLayer(layer);
          moved = true;
        }
      }
    }
    if (comment.frame >= 0) {
      if (auto* playbackService = ArtifactPlaybackService::instance()) {
        const auto range = playbackService->frameRange();
        const qint64 target = std::clamp<qint64>(
            comment.frame, range.start(), range.end());
        playbackService->setCurrentFrame(FramePosition(target));
        moved = true;
      }
    }
    return moved;
  }

  void refreshReviewNotes() {
    if (!widget_) return;
    refreshReviewContext();
    const QString compositionId = pendingLocalPresence_.compositionId;
    QList<CollabPresenceWidget::ReviewNote> notes;
    if (socket_.isConnected() && !compositionId.isEmpty()) {
      const auto comments = review_.commentsFor(compositionId, {}, true);
      for (const CollabComment& comment : comments) {
        CollabPresenceWidget::ReviewNote note;
        note.commentId = comment.commentId;
        note.parentCommentId = comment.parentCommentId;
        note.authorName = comment.authorName;
        note.text = comment.text;
        note.resolved = comment.resolved;
        note.deleted = comment.deleted;
        note.revisionHistoryTruncated = comment.revisionHistoryTruncated;
        if (!comment.deleted) {
          for (const CollabCommentRevision& revision : comment.revisions) {
            QString editorName = revision.editorName.isEmpty()
                                     ? revision.editorClientId
                                     : revision.editorName;
            if (revision.editorName.isEmpty() && session_) {
              if (revision.editorClientId == session_->localClientId()) {
                editorName = session_->localIdentity().userName;
              } else {
                const CollabParticipant editor =
                    session_->participant(revision.editorClientId);
                if (!editor.userName.isEmpty()) editorName = editor.userName;
              }
            }
            const QString editedAt = revision.editedAtMs > 0
                ? QDateTime::fromMSecsSinceEpoch(revision.editedAtMs)
                      .toLocalTime().toString(Qt::ISODateWithMs)
                : QStringLiteral("Unknown time");
            note.revisionHistory.append(
                QStringLiteral("%1 · %2\n%3")
                    .arg(editedAt, editorName, revision.previousText));
          }
        }
        if (!comment.layerId.isEmpty()) {
          note.anchor = QStringLiteral("Layer %1").arg(comment.layerId);
        }
        if (comment.frame >= 0) {
          if (!note.anchor.isEmpty()) note.anchor += QStringLiteral(" · ");
          note.anchor += QStringLiteral("Frame %1").arg(comment.frame);
        }
        notes.append(note);
      }
    }
    widget_->setReviewNotes(notes);
  }

  void refreshReviewContext() {
    if (!widget_) return;
    const QString compositionId = pendingLocalPresence_.compositionId;
    const QString layerId =
        pendingLocalPresence_.selectedLayerIds.size() == 1
            ? pendingLocalPresence_.selectedLayerIds.constFirst()
            : QString();
    const bool roomReady = socket_.isRoomReady();
    widget_->setReviewContext(compositionId, layerId, roomReady,
                              roomReady && !socket_.isReadOnly());
  }

  void refreshRoster() {
    if (!widget_ || !session_) return;
    QList<CollabPresenceWidget::UserPresence> users;
    QSet<QString> activeClientIds;
    const auto participants = session_->participants();
    for (const auto& participant : participants) {
      activeClientIds.insert(participant.clientId);
      CollabPresenceWidget::UserPresence user;
      user.userId = participant.clientId;
      user.userName = participant.userName;
      user.color = QColor(participant.userColor);

      const CollabPresenceState presence =
          session_->participantPresence(participant.clientId);
      QJsonObject raw = presence.raw;
      if (raw.contains(QStringLiteral("cursorLocation"))) {
        user.cursorLocation =
            raw.value(QStringLiteral("cursorLocation")).toString();
      } else if (presence.hasPlayback) {
        user.cursorLocation =
            QStringLiteral("Frame %1").arg(presence.playbackFrame);
      } else if (presence.hasComposition) {
        user.cursorLocation = QStringLiteral("Composition");
      } else {
        user.cursorLocation = QStringLiteral("Workspace");
      }

      const QJsonArray selectedLayers =
          raw.value(QStringLiteral("selectedLayers")).toArray();
      for (const QJsonValue& layerId : selectedLayers) {
        const QString value = layerId.toString();
        if (!value.isEmpty()) user.selectedLayers.append(value);
      }
      if (user.selectedLayers.isEmpty() && presence.hasSelection) {
        user.selectedLayers.append(presence.selectedLayerId);
      }
      auto& nameCache = selectedLayerNameCache_[participant.clientId];
      if (nameCache.compositionId != presence.compositionId ||
          nameCache.layerIds != user.selectedLayers) {
        nameCache.compositionId = presence.compositionId;
        nameCache.layerIds = user.selectedLayers;
        nameCache.names.clear();
        if (presence.hasComposition && !user.selectedLayers.isEmpty()) {
          auto* projectService = ArtifactProjectService::instance();
          if (projectService) {
            const auto found = projectService->findComposition(
                CompositionID(presence.compositionId));
            if (found.success) {
              if (const auto composition = found.ptr.lock()) {
                for (const QString& selectedLayerId : user.selectedLayers) {
                  const auto layer = composition->layerById(
                      LayerID(selectedLayerId));
                  if (!layer) continue;
                  const QString layerName = layer->layerName().trimmed();
                  if (!layerName.isEmpty()) {
                    nameCache.names.append(layerName);
                  }
                }
              }
            }
          }
        }
      }
      user.selectedLayerNames = nameCache.names;
      users.append(user);
    }
    for (auto cacheIt = selectedLayerNameCache_.begin();
         cacheIt != selectedLayerNameCache_.end();) {
      if (!activeClientIds.contains(cacheIt.key())) {
        cacheIt = selectedLayerNameCache_.erase(cacheIt);
      } else {
        ++cacheIt;
      }
    }
    widget_->syncRemoteUsers(users);
  }

  void schedulePresenceFlush() {
    if (!pendingPresenceDirty_ || !adapter_ || !socket_.isConnected() ||
        presenceFlushScheduled_) {
      return;
    }
    presenceFlushScheduled_ = true;
    QTimer::singleShot(200, this, [this]() {
      presenceFlushScheduled_ = false;
      if (!pendingPresenceDirty_ || !adapter_ || !socket_.isConnected()) return;
      pendingPresenceDirty_ = false;

      QJsonArray selectedLayers;
      for (const QString& layerId :
           pendingLocalPresence_.selectedLayerIds) {
        selectedLayers.append(layerId);
      }
      QJsonObject payload{
      {QStringLiteral("cursorLocation"),
           pendingLocalPresence_.compositionId.isEmpty()
               ? QStringLiteral("Workspace")
               : QStringLiteral("Composition")},
          {QStringLiteral("selectedLayers"), selectedLayers}};
      if (pendingLocalPresence_.hasPlaybackFrame) {
        payload.insert(QStringLiteral("playbackFrame"),
                       pendingLocalPresence_.playbackFrame);
      }
      if (!pendingLocalPresence_.compositionId.isEmpty()) {
        payload.insert(QStringLiteral("compositionId"),
                       pendingLocalPresence_.compositionId);
      }
      if (hasLastSentPresence_ && payload == lastSentPresence_) return;
      lastSentPresence_ = payload;
      hasLastSentPresence_ = true;

      CollabPresenceState presence;
      presence.raw = payload;
      if (!adapter_->sendLocalPresence(presence)) {
        hasLastSentPresence_ = false;
        pendingPresenceDirty_ = true;
        schedulePresenceRetry();
        return;
      }
      presenceRetryDelayMs_ = 500;
      if (widget_ && session_) {
        widget_->updateUser(session_->localClientId(), payload);
      }
    });
  }

  void schedulePresenceRetry() {
    if (presenceRetryScheduled_ || !pendingPresenceDirty_) return;
    presenceRetryScheduled_ = true;
    const int delayMs = presenceRetryDelayMs_;
    presenceRetryDelayMs_ = std::min(presenceRetryDelayMs_ * 2, 8000);
    QTimer::singleShot(delayMs, this, [this]() {
      presenceRetryScheduled_ = false;
      if (pendingPresenceDirty_ && socket_.isConnected()) {
        schedulePresenceFlush();
      }
    });
  }

  QPointer<CollabPresenceWidget> widget_;
  CollaborationWebSocket socket_;
  CollaborationSession* session_ = nullptr;
  CollaborationSessionAdapter* adapter_ = nullptr;
  LocalPresenceSnapshot pendingLocalPresence_;
  QJsonObject lastSentPresence_;
  CollaborationReview review_;
  bool pendingPresenceDirty_ = false;
  bool presenceFlushScheduled_ = false;
  bool presenceRetryScheduled_ = false;
  int presenceRetryDelayMs_ = 500;
  bool hasLastSentPresence_ = false;
  QSet<QString> pendingLockReleases_;
  QHash<QString, SelectedLayerNameCache> selectedLayerNameCache_;
};

constexpr int kMainWindowLayoutVersion = 11;

void applyConfiguredMessageBoxButtonAlignment(QMessageBox* messageBox) {
  if (!messageBox) {
    return;
  }
  auto* settings = ArtifactCore::ArtifactAppSettings::instance();
  if (!settings) {
    return;
  }
  const QString alignment = settings->accessibilityDialogButtonAlignment();
  if (alignment == QStringLiteral("platform")) {
    return;
  }
  auto* buttonBox = messageBox->findChild<QDialogButtonBox*>();
  auto* layout = messageBox->layout();
  if (!buttonBox || !layout) {
    return;
  }

  buttonBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  layout->setAlignment(buttonBox, alignment == QStringLiteral("left")
                                     ? Qt::AlignLeft
                                     : Qt::AlignRight);
}

class DialogLatencyEventFilter final : public QObject {
 public:
  DialogLatencyEventFilter() { clock_.start(); }

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (!event || (event->type() != QEvent::Polish &&
                   event->type() != QEvent::Show &&
                   event->type() != QEvent::Paint &&
                   event->type() != QEvent::Hide)) {
      return false;
    }
    auto* messageBox = qobject_cast<QMessageBox*>(watched);
    if (!messageBox) {
      return false;
    }

    const qint64 nowNs = clock_.nsecsElapsed();
    switch (event->type()) {
      case QEvent::Polish:
        messageBox->setProperty("artifactDialogPolishNs", nowNs);
        messageBox->setProperty("artifactDialogFirstPaintLogged", false);
        break;
      case QEvent::Show: {
        applyConfiguredMessageBoxButtonAlignment(messageBox);
        messageBox->setProperty("artifactDialogShowNs", nowNs);
        const qint64 polishNs =
            messageBox->property("artifactDialogPolishNs").toLongLong();
        qInfo().noquote()
            << QStringLiteral("[DialogLatency] phase=show title=\"%1\" "
                              "polishToShowMs=%2")
                   .arg(messageBox->windowTitle())
                   .arg(polishNs > 0 ? (nowNs - polishNs) / 1'000'000.0 : 0.0,
                        0, 'f', 2);
        break;
      }
      case QEvent::Paint: {
        if (messageBox->property("artifactDialogFirstPaintLogged").toBool()) {
          break;
        }
        messageBox->setProperty("artifactDialogFirstPaintLogged", true);
        const qint64 showNs =
            messageBox->property("artifactDialogShowNs").toLongLong();
        qInfo().noquote()
            << QStringLiteral("[DialogLatency] phase=first-paint title=\"%1\" "
                              "showToFirstPaintMs=%2")
                   .arg(messageBox->windowTitle())
                   .arg(showNs > 0 ? (nowNs - showNs) / 1'000'000.0 : 0.0,
                        0, 'f', 2);
        break;
      }
      case QEvent::Hide:
        messageBox->setProperty("artifactDialogPolishNs", QVariant{});
        messageBox->setProperty("artifactDialogShowNs", QVariant{});
        messageBox->setProperty("artifactDialogFirstPaintLogged", false);
        break;
      default:
        break;
    }
    return false;
  }

 private:
  QElapsedTimer clock_;
};

class PlaybackDebugAutoReporter final {
 public:
  void observe(const bool playing,
               const ArtifactCore::FrameDebugSnapshot& snapshot) {
    if (playing && !playing_) {
      begin(snapshot);
    }
    if (playing) {
      sample(snapshot);
    } else if (playing_) {
      sample(snapshot);
      finish(snapshot);
    }
    playing_ = playing;
  }

  [[nodiscard]] bool isCapturing() const { return playing_; }

 private:
  static double debugFieldNumber(const QString& text, const QString& key,
                                 const double fallback = 0.0) {
    const QString prefix = key + QLatin1Char('=');
    for (const QString& field : text.split(QLatin1Char(' '),
                                           Qt::SkipEmptyParts)) {
      if (field.startsWith(prefix)) {
        bool ok = false;
        const double value = field.mid(prefix.size()).toDouble(&ok);
        return ok ? value : fallback;
      }
    }
    return fallback;
  }

  void begin(const ArtifactCore::FrameDebugSnapshot& snapshot) {
    reasons_.clear();
    warnedReasons_.clear();
    pendingDecodeSamples_ = 0;
    sampleCount_ = 0;
    startedAt_ = QDateTime::currentDateTimeUtc();
    startFrame_ = snapshot.frame.framePosition();
  }

  void addReason(const QString& reason) {
    if (!reason.isEmpty() && !reasons_.contains(reason)) {
      reasons_.append(reason);
    }
  }

  void addWarningReason(const QString& reason, const QString& detail) {
    addReason(reason);
    if (reason.isEmpty() || warnedReasons_.contains(reason)) {
      return;
    }
    warnedReasons_.insert(reason);
    qWarning().noquote()
        << QStringLiteral("[RenderPathAutoWarning] reason=%1 %2")
               .arg(reason, detail);
  }

  void sample(const ArtifactCore::FrameDebugSnapshot& snapshot) {
    ++sampleCount_;
    if (snapshot.renderBackend.compare(QStringLiteral("cpu"),
                                       Qt::CaseInsensitive) == 0) {
      addWarningReason(
          QStringLiteral("cpu-render-backend"),
          QStringLiteral("frame=%1 backend=%2")
              .arg(snapshot.frame.framePosition())
              .arg(snapshot.renderBackend));
    }
    if (snapshot.failed) {
      addReason(QStringLiteral("render-failed: %1").arg(snapshot.failureReason));
    }
    if (snapshot.renderLastFrameMs > 50.0) {
      addReason(QStringLiteral("render-last-frame-over-50ms"));
    }
    if (snapshot.renderAverageFrameMs > 33.34) {
      addReason(QStringLiteral("render-average-over-frame-budget"));
    }

    bool decodePending = false;
    for (const auto& resource : snapshot.resources) {
      const QString note = resource.note;
      if (resource.label == QStringLiteral("Render Path")) {
        const double cpuRasterLayers =
            debugFieldNumber(note, QStringLiteral("cpuRasterLayers"));
        if (note.contains(QStringLiteral("path=fallback"))) {
          addWarningReason(
              QStringLiteral("composition-render-fallback"),
              QStringLiteral("frame=%1 %2")
                  .arg(snapshot.frame.framePosition())
                  .arg(note));
        }
        if (cpuRasterLayers > 0.0) {
          addWarningReason(
              QStringLiteral("cpu-raster-layer-active"),
              QStringLiteral("frame=%1 cpuRasterLayers=%2 %3")
                  .arg(snapshot.frame.framePosition())
                  .arg(cpuRasterLayers, 0, 'f', 0)
                  .arg(note));
        }
      }
      if (note.contains(QStringLiteral("gpu->cpu"), Qt::CaseInsensitive) ||
          note.contains(QStringLiteral("gpu-to-cpu"), Qt::CaseInsensitive)) {
        addWarningReason(
            QStringLiteral("gpu-readback-active"),
            QStringLiteral("frame=%1 resource=%2 note=%3")
                .arg(snapshot.frame.framePosition())
                .arg(resource.label, note));
      }
      if (resource.type == QStringLiteral("video") ||
          resource.label == QStringLiteral("Video Decode")) {
        decodePending = decodePending ||
                        note.contains(QStringLiteral("state=decode-pending")) ||
                        note.contains(QStringLiteral("stage=decoding"));
        if (note.contains(QStringLiteral("state=decode-failed")) ||
            note.contains(QStringLiteral("state=open-failed")) ||
            note.contains(QStringLiteral("sync-fallback-miss"))) {
          addReason(QStringLiteral("video-decode-failed"));
        }
        if (debugFieldNumber(note, QStringLiteral("avgDecodeMs")) > 50.0) {
          addReason(QStringLiteral("video-decode-over-50ms"));
        }
        if (debugFieldNumber(note, QStringLiteral("lateFrames")) >= 3.0) {
          addReason(QStringLiteral("video-decode-repeatedly-late"));
        }
      }
      if (note.contains(QStringLiteral("presentStatus=failed")) ||
          (note.contains(QStringLiteral("presentFail=")) &&
           !note.contains(QStringLiteral("presentFail=0")))) {
        addReason(QStringLiteral("present-failed"));
      }
    }
    pendingDecodeSamples_ = decodePending ? pendingDecodeSamples_ + 1 : 0;
    if (pendingDecodeSamples_ >= 3) {
      addReason(QStringLiteral("video-decode-pending-for-3-samples"));
    }
  }

  void finish(const ArtifactCore::FrameDebugSnapshot& terminalSnapshot) {
    if (reasons_.isEmpty()) {
      return;
    }

    const QString appData =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir reportDir(appData);
    if (!reportDir.mkpath(QStringLiteral("PlaybackDebugReports"))) {
      qWarning() << "[PlaybackDebugReport] failed to create report directory"
                 << reportDir.absolutePath();
      return;
    }
    reportDir.cd(QStringLiteral("PlaybackDebugReports"));
    QString compositionFilePart = terminalSnapshot.compositionName.isEmpty()
                                      ? QStringLiteral("composition")
                                      : terminalSnapshot.compositionName;
    compositionFilePart.replace(QChar('/'), QChar('_'));
    compositionFilePart.replace(QChar('\\'), QChar('_'));
    compositionFilePart.replace(QChar(':'), QChar('_'));
    const QString fileName = QStringLiteral("playback_%1_%2.txt")
                                 .arg(startedAt_.toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")),
                                      compositionFilePart);
    const QString reportPath = reportDir.filePath(fileName);

    QStringList lines;
    lines << QStringLiteral("ArtifactStudio Playback Debug Report");
    lines << QStringLiteral("startedAt: %1").arg(startedAt_.toString(Qt::ISODateWithMs));
    lines << QStringLiteral("endedAt: %1").arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    lines << QStringLiteral("composition: %1").arg(terminalSnapshot.compositionName);
    lines << QStringLiteral("frameRange: %1 -> %2").arg(startFrame_).arg(terminalSnapshot.frame.framePosition());
    lines << QStringLiteral("samples: %1").arg(sampleCount_);
    lines << QStringLiteral("reasons: %1").arg(reasons_.join(QStringLiteral(", ")));
    lines << QStringLiteral("renderMs: last=%1 average=%2 gpu=%3")
                 .arg(terminalSnapshot.renderLastFrameMs, 0, 'f', 2)
                 .arg(terminalSnapshot.renderAverageFrameMs, 0, 'f', 2)
                 .arg(terminalSnapshot.renderGpuFrameMs, 0, 'f', 2);
    lines << QString();
    lines << QStringLiteral("Resources");
    for (const auto& resource : terminalSnapshot.resources) {
      lines << QStringLiteral("%1 [%2] %3")
                   .arg(resource.label, resource.type, resource.note);
    }
    lines << QString();
    lines << QStringLiteral("Passes");
    for (const auto& pass : terminalSnapshot.passes) {
      lines << QStringLiteral("%1 durationUs=%2 note=%3")
                   .arg(pass.name)
                   .arg(pass.durationUs)
                   .arg(pass.note);
    }

    QSaveFile reportFile(reportPath);
    const QByteArray payload = lines.join(QLatin1Char('\n')).toUtf8();
    if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        reportFile.write(payload) != payload.size() || !reportFile.commit()) {
      qWarning() << "[PlaybackDebugReport] failed to write" << reportPath;
      return;
    }
    qWarning() << "[PlaybackDebugReport] wrote" << reportPath
               << "reasons=" << reasons_;
  }

  bool playing_ = false;
  int pendingDecodeSamples_ = 0;
  int sampleCount_ = 0;
  int startFrame_ = 0;
  QDateTime startedAt_;
  QStringList reasons_;
  QSet<QString> warnedReasons_;
};

bool isArtifactProjectLaunchPath(const QString& filePath)
{
  if (filePath.isEmpty()) {
    return false;
  }

  const QFileInfo info(filePath);
  if (!info.exists() || !info.isFile()) {
    return false;
  }

  const QString lowerName = info.fileName().toLower();
  return lowerName.endsWith(QStringLiteral(".artifact")) ||
         lowerName.endsWith(QStringLiteral(".artifact.json"));
}

QString normalizeLaunchPath(const QString& filePath)
{
  if (filePath.isEmpty()) {
    return QString();
  }

  const QFileInfo info(filePath);
  if (!info.exists()) {
    return QString();
  }

  return info.absoluteFilePath();
}

QStringList collectLaunchProjectPaths(const QStringList& appArgs)
{
  QStringList projectPaths;
  projectPaths.reserve(appArgs.size());

  for (int i = 1; i < appArgs.size(); ++i) {
    const QString arg = appArgs[i];
    if (arg == QStringLiteral("--lang")) {
      ++i;
      continue;
    }
    if (arg.startsWith(QStringLiteral("--"))) {
      continue;
    }
    if (!isArtifactProjectLaunchPath(arg)) {
      continue;
    }

    const QString normalizedPath = normalizeLaunchPath(arg);
    if (!normalizedPath.isEmpty()) {
      projectPaths.append(normalizedPath);
    }
  }

  projectPaths.removeDuplicates();
  return projectPaths;
}

class LaunchOpenRequestFilter final : public QObject {
public:
  explicit LaunchOpenRequestFilter(QObject* parent = nullptr)
      : QObject(parent) {}

  void setProjectOpenHandler(std::function<void(const QString&)> handler)
  {
    projectOpenHandler_ = std::move(handler);
    flushPendingRequests();
  }

  void enqueueLaunchPath(const QString& filePath)
  {
    const QString normalizedPath = normalizeLaunchPath(filePath);
    if (!isArtifactProjectLaunchPath(normalizedPath)) {
      return;
    }

    if (projectOpenHandler_) {
      projectOpenHandler_(normalizedPath);
      return;
    }

    pendingProjectPaths_.append(normalizedPath);
    pendingProjectPaths_.removeDuplicates();
  }

  bool eventFilter(QObject* watched, QEvent* event) override
  {
    if (event && event->type() == QEvent::FileOpen) {
      auto* fileEvent = static_cast<QFileOpenEvent*>(event);
      if (fileEvent && isArtifactProjectLaunchPath(fileEvent->file())) {
        enqueueLaunchPath(fileEvent->file());
        return true;
      }
    }

    return QObject::eventFilter(watched, event);
  }

private:
  void flushPendingRequests()
  {
    if (!projectOpenHandler_ || pendingProjectPaths_.isEmpty()) {
      return;
    }

    const QStringList pending = std::exchange(pendingProjectPaths_, {});
    for (const QString& path : pending) {
      projectOpenHandler_(path);
    }
  }

  QStringList pendingProjectPaths_;
  std::function<void(const QString&)> projectOpenHandler_;
};

class AccessibilityInputEventFilter final : public QObject {
public:
  explicit AccessibilityInputEventFilter(QObject* parent = nullptr)
      : QObject(parent) {}

protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    Q_UNUSED(watched);
    if (!event) {
      return false;
    }
    if (dispatchingSyntheticEvent_) {
      return false;
    }

    if (event->type() == QEvent::KeyPress ||
        event->type() == QEvent::KeyRelease) {
      auto* keyEvent = static_cast<QKeyEvent*>(event);
      if (!keyEvent) {
        return false;
      }
      const int key = keyEvent->key();
      const Qt::KeyboardModifier modifier = modifierForKey(key);
      if (modifier != Qt::NoModifier &&
          Artifact::Accessibility::stickyKeysEnabled()) {
        if (event->type() == QEvent::KeyPress && !keyEvent->isAutoRepeat()) {
          if ((stickyModifiers_ & modifier) != 0) {
            stickyModifiers_ &= ~modifier;
          } else {
            stickyModifiers_ |= modifier;
          }
          updateAccessibilityStatus();
        }
        return true;
      }

      if (event->type() == QEvent::KeyPress && !keyEvent->isAutoRepeat()) {
        const Qt::KeyboardModifiers effectiveModifiers =
            keyEvent->modifiers() | stickyModifiers_;
        if (effectiveModifiers != keyEvent->modifiers()) {
          QKeyEvent syntheticKeyEvent(
              QEvent::KeyPress, keyEvent->key(), effectiveModifiers,
              keyEvent->nativeScanCode(), keyEvent->nativeVirtualKey(),
              keyEvent->nativeModifiers(), keyEvent->text(),
              keyEvent->isAutoRepeat(), keyEvent->count());
          dispatchingSyntheticEvent_ = true;
          QCoreApplication::sendEvent(watched, &syntheticKeyEvent);
          dispatchingSyntheticEvent_ = false;
          if (Artifact::Accessibility::stickyKeysMode() == QStringLiteral("latch") ||
              Artifact::Accessibility::stickyKeysMode() == QStringLiteral("both")) {
            stickyModifiers_ = Qt::NoModifier;
          }
          updateAccessibilityStatus();
          return true;
        }
      }
      return false;
    }

    if (Artifact::Accessibility::singleHandModeEnabled() &&
        (event->type() == QEvent::MouseButtonPress ||
         event->type() == QEvent::MouseButtonRelease ||
         event->type() == QEvent::MouseMove)) {
      auto* mouseEvent = static_cast<QMouseEvent*>(event);
      if (!mouseEvent) {
        return false;
      }
      if (event->type() == QEvent::MouseButtonPress) {
        if (mouseEvent->button() == Qt::XButton1) {
          mouseModifiers_ |= Qt::ShiftModifier;
        } else if (mouseEvent->button() == Qt::XButton2) {
          mouseModifiers_ |= Qt::ControlModifier;
        }
      }
      if (event->type() == QEvent::MouseButtonRelease) {
        if (mouseEvent->button() == Qt::XButton1) {
          mouseModifiers_ &= ~Qt::ShiftModifier;
        } else if (mouseEvent->button() == Qt::XButton2) {
          mouseModifiers_ &= ~Qt::ControlModifier;
        }
      }
    }
    return false;
  }

private:
  void updateAccessibilityStatus() const {
    auto *window = QApplication::activeWindow();
    auto *mainWindow = dynamic_cast<QMainWindow*>(window);
    auto *status = mainWindow
        ? dynamic_cast<ArtifactStatusBar *>(mainWindow->findChild<QStatusBar *>())
        : nullptr;
    if (!status) {
      return;
    }
    QStringList active;
    if (stickyModifiers_.testFlag(Qt::ControlModifier)) active << QStringLiteral("Ctrl");
    if (stickyModifiers_.testFlag(Qt::ShiftModifier)) active << QStringLiteral("Shift");
    if (stickyModifiers_.testFlag(Qt::AltModifier)) active << QStringLiteral("Alt");
    if (stickyModifiers_.testFlag(Qt::MetaModifier)) active << QStringLiteral("Meta");
    status->setAccessibilityText(active.isEmpty() ? QStringLiteral("OFF") : active.join(QStringLiteral(" + ")));
  }

  static Qt::KeyboardModifier modifierForKey(const int key) {
    switch (key) {
      case Qt::Key_Control: return Qt::ControlModifier;
      case Qt::Key_Shift: return Qt::ShiftModifier;
      case Qt::Key_Alt: return Qt::AltModifier;
      case Qt::Key_Meta: return Qt::MetaModifier;
      default: return Qt::NoModifier;
    }
  }

  Qt::KeyboardModifiers stickyModifiers_ = Qt::NoModifier;
  Qt::KeyboardModifiers mouseModifiers_ = Qt::NoModifier;
  bool dispatchingSyntheticEvent_ = false;
};

ArtifactCore::TraceCrashRecord traceCrashFromReportPath(const QString& crashReportPath)
{
  ArtifactCore::TraceCrashRecord record;
  record.summary = QStringLiteral("Crash report: %1").arg(crashReportPath);
  record.threadName = QThread::currentThread()
                          ? QThread::currentThread()->objectName()
                          : QStringLiteral("main-thread");
  record.timestampMs = QDateTime::currentMSecsSinceEpoch();

  QFile file(crashReportPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    record.stack = QStringLiteral("<unable to read crash report>");
    return record;
  }

  const QString report = QString::fromUtf8(file.readAll());
  const QString stackStart = QStringLiteral("--- Stack Trace ---");
  const QString systemStart = QStringLiteral("--- System Info ---");
  const int start = report.indexOf(stackStart);
  const int end = report.indexOf(systemStart);
  if (start >= 0) {
    const int stackBegin = start + stackStart.size();
    const int stackEnd = end > stackBegin ? end : report.size();
    record.stack = report.mid(stackBegin, stackEnd - stackBegin).trimmed();
  } else {
    record.stack = report.left(1200);
  }
  return record;
}

void suppressScrollBarsForViewportWidget(QWidget *widget) {
  if (!widget) {
    return;
  }

  const auto apply = [widget]() {
    QWidget *cursor = widget->parentWidget();
    while (cursor) {
      if (auto *scrollArea = qobject_cast<QAbstractScrollArea *>(cursor)) {
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      }
      cursor = cursor->parentWidget();
    }
  };

  apply();
  QTimer::singleShot(0, widget, apply);
}

quint64 processWorkingSetMB() {
#if defined(_WIN32)
  MEMORYSTATUSEX memStatus{};
  memStatus.dwLength = sizeof(memStatus);
  if (GlobalMemoryStatusEx(&memStatus)) {
    const quint64 totalPhys = static_cast<quint64>(memStatus.ullTotalPhys);
    const quint64 availPhys = static_cast<quint64>(memStatus.ullAvailPhys);
    return (totalPhys - availPhys) / (1024ull * 1024ull);
  }
#endif
  return 0;
}

void registerPythonApplicationAPI() {
  ArtifactCore::CorePythonAPI::registerAll();
  ArtifactCore::CorePythonAPI::setCompositionBridge(
      [](const std::string &method, const std::vector<std::string> &arguments) {
        QVariantList args;
        QByteArray encodedArguments("[");
        for (const auto &argument : arguments) {
          if (encodedArguments.size() > 1) {
            encodedArguments.append(',');
          }
          encodedArguments.append(argument.data(),
                                  static_cast<qsizetype>(argument.size()));
        }
        encodedArguments.append(']');
        QJsonParseError parseError;
        const QJsonDocument parsedArguments = QJsonDocument::fromJson(
            encodedArguments, &parseError);
        if (parseError.error != QJsonParseError::NoError ||
            !parsedArguments.isArray()) {
          return std::string("{\"success\":false,\"errorCode\":\"INVALID_ARGUMENTS\",\"message\":\"Workspace arguments must be JSON values\"}");
        }
        args = parsedArguments.array().toVariantList();
        const QVariant result = Artifact::WorkspaceAutomation::instance().invokeMethod(
            QString::fromStdString(method), args);
        const QJsonValue jsonValue = QJsonValue::fromVariant(result);
        if (jsonValue.isObject() || jsonValue.isArray()) {
          return QString::fromUtf8(
              QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact).constData())
              .toStdString();
        }
        QJsonObject wrapped;
        wrapped.insert(QStringLiteral("value"), jsonValue);
        return QString::fromUtf8(QJsonDocument(wrapped).toJson(QJsonDocument::Compact))
            .toStdString();
      });

  ArtifactPythonAPI::registerAll();
}

void bootstrapPythonScripts() {
  auto &py = PythonEngine::instance();
  if (!py.initialize()) {
    return;
  }

  registerPythonApplicationAPI();

  const QString appDir = QCoreApplication::applicationDirPath();
  const QStringList scriptDirs = {
      QDir(appDir).filePath("scripts"),
      QDir(QDir::currentPath()).filePath("scripts")};

  for (const QString &dirPath : scriptDirs) {
    QDir dir(dirPath);
    if (!dir.exists()) {
      continue;
    }

    py.addSearchPath(dir.absolutePath().toStdString());
    const QFileInfoList files =
        dir.entryInfoList(QStringList() << "*.py", QDir::Files, QDir::Name);
    for (const QFileInfo &fileInfo : files) {
      py.executeFile(fileInfo.absoluteFilePath().toStdString());
    }
  }
}

void configureQtPluginPaths() {
  const QString appDir = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
      appDir,
      QDir(appDir).filePath(QStringLiteral("plugins")),
      QDir(appDir).filePath(
          QStringLiteral("../vcpkg_installed/x64-windows/Qt6/plugins")),
      QDir(appDir).filePath(
          QStringLiteral("../vcpkg_installed/x64-windows/debug/Qt6/plugins")),
      QDir(appDir).filePath(
          QStringLiteral("../vcpkg_installed/x64-windows/Qt6/plugins")),
      QDir(appDir).filePath(
          QStringLiteral("../vcpkg_installed/x64-windows/debug/Qt6/plugins"))};

  for (const QString &path : candidates) {
    if (QDir(path).exists()) {
      QCoreApplication::addLibraryPath(path);
    }
  }

  qDebug() << "[QtPluginPaths] libraryPaths="
           << QCoreApplication::libraryPaths();
  qDebug() << "[QtPluginPaths] supportedImageFormats="
           << QImageReader::supportedImageFormats();
}

QByteArray currentProjectSnapshotJson() {
  auto project =
      ArtifactProjectManager::getInstance().getCurrentProjectSharedPtr();
  if (!project) {
    return {};
  }
  const QJsonDocument doc(project->toJson());
  return doc.toJson(QJsonDocument::Indented);
}

QString currentProjectCollaborationFingerprint() {
  auto project =
      ArtifactProjectManager::getInstance().getCurrentProjectSharedPtr();
  if (!project) {
    return {};
  }
  QJsonObject baseline = project->toJson();
  baseline.remove(QStringLiteral("savedAt"));
  const QByteArray bytes =
      QJsonDocument(baseline).toJson(QJsonDocument::Compact);
  if (bytes.isEmpty()) {
    return {};
  }
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QString debugBridgeFilePath() {
  const QString tempRoot =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  QDir rootDir(tempRoot);
  if (!rootDir.exists(QStringLiteral("ArtifactStudio"))) {
    rootDir.mkpath(QStringLiteral("ArtifactStudio"));
  }
  return rootDir.filePath(QStringLiteral("ArtifactStudio/debug-bridge.json"));
}

QJsonArray trimJsonArray(const QJsonArray& array, const int maxEntries) {
  if (array.size() <= maxEntries) {
    return array;
  }
  QJsonArray trimmed;
  const int start = std::max(0, static_cast<int>(array.size()) - maxEntries);
  for (int i = start; i < array.size(); ++i) {
    trimmed.append(array.at(i));
  }
  return trimmed;
}

QJsonObject diagnosticEventToJson(const ArtifactCore::DiagnosticEvent& event) {
  QJsonObject json;
  json.insert(QStringLiteral("severity"),
              QString::fromUtf8(ArtifactCore::diagnosticSeverityName(event.severity)));
  json.insert(QStringLiteral("code"), QString::fromStdString(event.code));
  json.insert(QStringLiteral("message"), QString::fromStdString(event.message));
  json.insert(QStringLiteral("component"), QString::fromStdString(event.component));
  json.insert(QStringLiteral("operation"), QString::fromStdString(event.operation));
  json.insert(QStringLiteral("objectId"), QString::fromStdString(event.objectId));
  json.insert(QStringLiteral("sequence"), static_cast<qint64>(event.sequence));
  json.insert(QStringLiteral("threadId"), static_cast<qint64>(event.threadId));
  json.insert(QStringLiteral("traceId"), static_cast<qint64>(event.traceId));
  json.insert(QStringLiteral("frameIndex"), static_cast<qint64>(event.frameIndex));
  json.insert(QStringLiteral("timestampNs"), static_cast<qint64>(event.timestampNs));
  json.insert(QStringLiteral("durationNs"), static_cast<qint64>(event.durationNs));
  if (event.location.file) {
    json.insert(QStringLiteral("file"), QString::fromUtf8(event.location.file));
  }
  if (event.location.function) {
    json.insert(QStringLiteral("function"), QString::fromUtf8(event.location.function));
  }
  if (event.location.line > 0) {
    json.insert(QStringLiteral("line"), event.location.line);
  }
  return json;
}

QJsonObject diagnosticSnapshotJson() {
  QJsonObject diagnostics;
  const auto events = ArtifactCore::DiagnosticRecorder::instance().errorsFor();
  QJsonArray eventArray;
  const int first = std::max(0, static_cast<int>(events.size()) - 32);
  for (int i = first; i < static_cast<int>(events.size()); ++i) {
    eventArray.append(diagnosticEventToJson(events[static_cast<std::size_t>(i)]));
  }
  diagnostics.insert(QStringLiteral("latestSequence"),
                     static_cast<qint64>(ArtifactCore::DiagnosticRecorder::instance().latestSequence()));
  diagnostics.insert(QStringLiteral("eventsTruncated"), events.size() > 32);
  diagnostics.insert(QStringLiteral("firstPublishedSequence"),
                     eventArray.isEmpty()
                         ? QJsonValue(0)
                         : eventArray.first().toObject().value(QStringLiteral("sequence")));
  diagnostics.insert(QStringLiteral("events"), eventArray);
  if (!events.empty()) {
    diagnostics.insert(QStringLiteral("latestFailure"),
                       diagnosticEventToJson(events.back()));
  } else {
    diagnostics.insert(QStringLiteral("latestFailure"), QJsonValue::Null);
  }
  return diagnostics;
}

QJsonObject buildDebugBridgeSnapshotJson() {
  QJsonObject root;
  root.insert(QStringLiteral("snapshotVersion"), 1);
  root.insert(QStringLiteral("timestamp"),
              QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
  QJsonObject appJson;
  appJson.insert(QStringLiteral("name"), QStringLiteral("Artifact"));
  appJson.insert(QStringLiteral("pid"),
                 static_cast<qint64>(QCoreApplication::applicationPid()));
  root.insert(QStringLiteral("app"), appJson);

  const QVariantMap workspace =
      Artifact::WorkspaceAutomation::instance()
          .invokeMethod(QStringLiteral("workspaceSnapshot"), {})
          .toMap();
  const QJsonObject workspaceJson = QJsonDocument::fromVariant(workspace).object();
  root.insert(QStringLiteral("workspace"), workspaceJson);
  root.insert(QStringLiteral("project"),
              workspaceJson.value(QStringLiteral("project")).toObject());
  root.insert(QStringLiteral("composition"),
              workspaceJson.value(QStringLiteral("currentComposition")).toObject());
  root.insert(QStringLiteral("selection"),
              workspaceJson.value(QStringLiteral("selection")).toObject());
  root.insert(QStringLiteral("renderQueue"),
              workspaceJson.value(QStringLiteral("renderQueue")).toObject());

  QJsonObject playbackJson;
  if (auto* playback = Artifact::ArtifactPlaybackService::instance()) {
    playbackJson.insert(QStringLiteral("available"), true);
    playbackJson.insert(QStringLiteral("state"), [playback]() {
      switch (playback->state()) {
      case PlaybackState::Playing:
        return QStringLiteral("playing");
      case PlaybackState::Paused:
        return QStringLiteral("paused");
      case PlaybackState::Stopped:
      default:
        return QStringLiteral("stopped");
      }
    }());
    playbackJson.insert(QStringLiteral("frame"),
                        static_cast<qint64>(playback->currentFrame().framePosition()));
    playbackJson.insert(QStringLiteral("frameRate"), playback->frameRate().framerate());
    playbackJson.insert(QStringLiteral("speed"), playback->playbackSpeed());
    playbackJson.insert(QStringLiteral("looping"), playback->isLooping());
    QJsonObject rangeJson;
    rangeJson.insert(QStringLiteral("start"),
                     static_cast<qint64>(playback->frameRange().start()));
    rangeJson.insert(QStringLiteral("end"),
                     static_cast<qint64>(playback->frameRange().end()));
    playbackJson.insert(QStringLiteral("range"), rangeJson);
    playbackJson.insert(QStringLiteral("inPoint"),
                        playback->inPoint()
                            ? QJsonValue(static_cast<qint64>(playback->inPoint()->framePosition()))
                            : QJsonValue(QJsonValue::Null));
    playbackJson.insert(QStringLiteral("outPoint"),
                        playback->outPoint()
                            ? QJsonValue(static_cast<qint64>(playback->outPoint()->framePosition()))
                            : QJsonValue(QJsonValue::Null));
  } else {
    playbackJson.insert(QStringLiteral("available"), false);
  }
  root.insert(QStringLiteral("playback"), playbackJson);

  QJsonObject diagnosticsJson;
  if (auto* projectService = Artifact::ArtifactProjectService::instance()) {
    diagnosticsJson.insert(QStringLiteral("healthState"),
                           projectService->currentProjectHealthStateToken());
    diagnosticsJson.insert(QStringLiteral("summary"),
                           projectService->currentProjectHealthSummaryText());
  } else {
    diagnosticsJson.insert(QStringLiteral("healthState"), QStringLiteral("unknown"));
    diagnosticsJson.insert(QStringLiteral("summary"),
                           QStringLiteral("Project service unavailable."));
  }
  const QJsonObject recordedDiagnostics = diagnosticSnapshotJson();
  for (auto it = recordedDiagnostics.begin(); it != recordedDiagnostics.end(); ++it) {
    diagnosticsJson.insert(it.key(), it.value());
  }
  root.insert(QStringLiteral("diagnostics"), diagnosticsJson);

  QJsonObject traceJson = ArtifactCore::toJson(ArtifactCore::TraceRecorder::instance().snapshot());
  traceJson.insert(QStringLiteral("events"),
                   trimJsonArray(traceJson.value(QStringLiteral("events")).toArray(), 40));
  traceJson.insert(QStringLiteral("frames"),
                   trimJsonArray(traceJson.value(QStringLiteral("frames")).toArray(), 10));
  traceJson.insert(QStringLiteral("scopes"),
                   trimJsonArray(traceJson.value(QStringLiteral("scopes")).toArray(), 40));
  traceJson.insert(QStringLiteral("locks"),
                   trimJsonArray(traceJson.value(QStringLiteral("locks")).toArray(), 40));
  traceJson.insert(QStringLiteral("crashes"),
                   trimJsonArray(traceJson.value(QStringLiteral("crashes")).toArray(), 8));
  root.insert(QStringLiteral("trace"), traceJson);
  root.insert(QStringLiteral("traceText"),
              QString::fromUtf8(QJsonDocument(traceJson).toJson(QJsonDocument::Compact)));

  QJsonArray propertiesJson;
  const auto properties = ArtifactCore::PropertyRegistryReadOnlyAdapter::queryAllProperties();
  for (const auto& property : properties) {
    if (!property.isValid) {
      continue;
    }
    QJsonObject propertyJson;
    propertyJson.insert(QStringLiteral("path"),
                        ArtifactCore::propertyPathJoin(property.ownerPath,
                                                       property.propertyName));
    propertyJson.insert(QStringLiteral("ownerPath"), property.ownerPath);
    propertyJson.insert(QStringLiteral("propertyName"), property.propertyName);
    propertyJson.insert(QStringLiteral("type"), property.propertyType);
    propertyJson.insert(QStringLiteral("value"),
                        QJsonValue::fromVariant(property.currentValue));
    propertyJson.insert(QStringLiteral("readOnly"), property.isReadOnly);
    propertiesJson.append(propertyJson);
  }
  root.insert(QStringLiteral("properties"), propertiesJson);
  return root;
}

QString debugMcpStateFilePath() {
  const QString envPath = qEnvironmentVariable("ARTIFACT_DEBUG_MCP_STATE_FILE");
  if (!envPath.trimmed().isEmpty()) {
    return envPath;
  }

  const QString tempRoot =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  QDir rootDir(tempRoot);
  if (!rootDir.exists(QStringLiteral("ArtifactStudio"))) {
    rootDir.mkpath(QStringLiteral("ArtifactStudio"));
  }
  return rootDir.filePath(QStringLiteral("ArtifactStudio/debug-mcp-state.json"));
}

QJsonObject debugMcpDefaultStateJson() {
  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);

  QJsonObject session;
  session.insert(QStringLiteral("paused"), false);
  session.insert(QStringLiteral("tickCount"), 0);
  session.insert(QStringLiteral("lastAction"), QStringLiteral("idle"));
  session.insert(QStringLiteral("pauseReason"), QJsonValue::Null);
  session.insert(QStringLiteral("wasPlayingBeforePause"), false);
  session.insert(QStringLiteral("pausedAtFrame"), QJsonValue::Null);
  root.insert(QStringLiteral("session"), session);

  root.insert(QStringLiteral("nextConditionId"), 1);
  root.insert(QStringLiteral("breakConditions"), QJsonArray{});
  root.insert(QStringLiteral("nextWatchId"), 1);
  root.insert(QStringLiteral("watchDescriptors"), QJsonArray{});
  root.insert(QStringLiteral("lastBreakHit"), QJsonValue::Null);
  root.insert(QStringLiteral("lastWatchSnapshot"), QJsonValue::Null);
  root.insert(QStringLiteral("history"), QJsonArray{});

  QJsonObject mockSnapshot;
  mockSnapshot.insert(QStringLiteral("snapshotVersion"), 1);
  mockSnapshot.insert(QStringLiteral("timestamp"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
  root.insert(QStringLiteral("mockSnapshot"), mockSnapshot);
  return root;
}

QJsonObject debugMcpNormalizeStateJson(QJsonObject state) {
  const QJsonObject defaults = debugMcpDefaultStateJson();

  if (!state.contains(QStringLiteral("version"))) {
    state.insert(QStringLiteral("version"), defaults.value(QStringLiteral("version")));
  }

  QJsonObject session = state.value(QStringLiteral("session")).toObject();
  const QJsonObject defaultSession = defaults.value(QStringLiteral("session")).toObject();
  if (!session.contains(QStringLiteral("paused"))) {
    session.insert(QStringLiteral("paused"), defaultSession.value(QStringLiteral("paused")));
  }
  if (!session.contains(QStringLiteral("tickCount"))) {
    session.insert(QStringLiteral("tickCount"), defaultSession.value(QStringLiteral("tickCount")));
  }
  if (!session.contains(QStringLiteral("lastAction"))) {
    session.insert(QStringLiteral("lastAction"),
                   defaultSession.value(QStringLiteral("lastAction")));
  }
  if (!session.contains(QStringLiteral("pauseReason"))) {
    session.insert(QStringLiteral("pauseReason"),
                   defaultSession.value(QStringLiteral("pauseReason")));
  }
  if (!session.contains(QStringLiteral("wasPlayingBeforePause"))) {
    session.insert(QStringLiteral("wasPlayingBeforePause"),
                   defaultSession.value(QStringLiteral("wasPlayingBeforePause")));
  }
  if (!session.contains(QStringLiteral("pausedAtFrame"))) {
    session.insert(QStringLiteral("pausedAtFrame"),
                   defaultSession.value(QStringLiteral("pausedAtFrame")));
  }
  state.insert(QStringLiteral("session"), session);

  if (!state.value(QStringLiteral("breakConditions")).isArray()) {
    state.insert(QStringLiteral("breakConditions"), defaults.value(QStringLiteral("breakConditions")));
  }
  if (!state.value(QStringLiteral("watchDescriptors")).isArray()) {
    state.insert(QStringLiteral("watchDescriptors"), defaults.value(QStringLiteral("watchDescriptors")));
  }
  if (!state.value(QStringLiteral("history")).isArray()) {
    state.insert(QStringLiteral("history"), defaults.value(QStringLiteral("history")));
  }
  if (!state.contains(QStringLiteral("nextConditionId"))) {
    state.insert(QStringLiteral("nextConditionId"), defaults.value(QStringLiteral("nextConditionId")));
  }
  if (!state.contains(QStringLiteral("nextWatchId"))) {
    state.insert(QStringLiteral("nextWatchId"), defaults.value(QStringLiteral("nextWatchId")));
  }
  if (!state.contains(QStringLiteral("lastBreakHit"))) {
    state.insert(QStringLiteral("lastBreakHit"), defaults.value(QStringLiteral("lastBreakHit")));
  }
  if (!state.contains(QStringLiteral("lastWatchSnapshot"))) {
    state.insert(QStringLiteral("lastWatchSnapshot"), defaults.value(QStringLiteral("lastWatchSnapshot")));
  }
  if (!state.value(QStringLiteral("mockSnapshot")).isObject()) {
    state.insert(QStringLiteral("mockSnapshot"),
                 defaults.value(QStringLiteral("mockSnapshot")));
  }
  return state;
}

QJsonObject debugMcpReadStateJson() {
  QFile file(debugMcpStateFilePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return debugMcpDefaultStateJson();
  }

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) {
    return debugMcpDefaultStateJson();
  }
  return debugMcpNormalizeStateJson(doc.object());
}

bool debugMcpWriteStateJson(const QJsonObject& state) {
  const QString filePath = debugMcpStateFilePath();
  QDir dirInfo(QFileInfo(filePath).absolutePath());
  if (!dirInfo.exists()) {
    dirInfo.mkpath(QStringLiteral("."));
  }

  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qWarning() << "[DebugMCP] failed to open state file:" << filePath;
    return false;
  }

  const QByteArray payload = QJsonDocument(debugMcpNormalizeStateJson(state))
                                 .toJson(QJsonDocument::Indented);
  if (file.write(payload) != payload.size()) {
    qWarning() << "[DebugMCP] failed to write state file:" << filePath;
    file.cancelWriting();
    return false;
  }
  if (!file.commit()) {
    qWarning() << "[DebugMCP] failed to commit state file:" << filePath;
    return false;
  }
  return true;
}

QJsonArray debugMcpSelectionArray(const QJsonObject& snapshot, const QString& key) {
  const QJsonObject selection = snapshot.value(QStringLiteral("selection")).toObject();
  return selection.value(key).toArray();
}

QString debugMcpJoinedArray(const QJsonArray& array) {
  QStringList parts;
  parts.reserve(array.size());
  for (const QJsonValue& value : array) {
    parts.push_back(value.toString());
  }
  return parts.join(QStringLiteral(","));
}

QString debugMcpSnapshotTraceText(const QJsonObject& snapshot) {
  const QJsonValue traceTextValue = snapshot.value(QStringLiteral("traceText"));
  if (traceTextValue.isString() && !traceTextValue.toString().trimmed().isEmpty()) {
    return traceTextValue.toString();
  }

  const QJsonValue traceValue = snapshot.value(QStringLiteral("trace"));
  if (traceValue.isString() && !traceValue.toString().trimmed().isEmpty()) {
    return traceValue.toString();
  }
  if (traceValue.isObject()) {
    return QString::fromUtf8(
        QJsonDocument(traceValue.toObject()).toJson(QJsonDocument::Compact));
  }
  if (traceValue.isArray()) {
    return QString::fromUtf8(
        QJsonDocument(traceValue.toArray()).toJson(QJsonDocument::Compact));
  }
  return QString();
}

QString debugMcpSnapshotPropertiesText(const QJsonObject& snapshot) {
  const QJsonValue propertiesValue = snapshot.value(QStringLiteral("properties"));
  if (!propertiesValue.isArray()) {
    return QString();
  }

  QStringList parts;
  const QJsonArray properties = propertiesValue.toArray();
  parts.reserve(properties.size());
  for (const QJsonValue& propertyValue : properties) {
    const QJsonObject property = propertyValue.toObject();
    const QString path = property.value(QStringLiteral("path")).toString();
    const QJsonValue serializedValue =
        QJsonValue::fromVariant(property.value(QStringLiteral("value")).toVariant());
    QString value;
    if (serializedValue.isObject()) {
      value = QString::fromUtf8(
          QJsonDocument(serializedValue.toObject()).toJson(QJsonDocument::Compact));
    } else if (serializedValue.isArray()) {
      value = QString::fromUtf8(
          QJsonDocument(serializedValue.toArray()).toJson(QJsonDocument::Compact));
    } else if (serializedValue.isNull() || serializedValue.isUndefined()) {
      value = QStringLiteral("null");
    } else {
      value = serializedValue.toVariant().toString();
    }
    parts.push_back(QStringLiteral("%1=%2").arg(path, value));
  }
  return parts.join(QStringLiteral("\n"));
}

QJsonValue debugMcpWatchValue(const QJsonObject& snapshot, const QString& path, bool* found) {
  if (found) {
    *found = false;
  }
  if (path.startsWith(QStringLiteral("property:"))) {
    const QString propertyPath = path.mid(QStringLiteral("property:").size());
    for (const QJsonValue& entryValue : snapshot.value(QStringLiteral("properties")).toArray()) {
      const QJsonObject entry = entryValue.toObject();
      if (entry.value(QStringLiteral("path")).toString() == propertyPath) {
        if (found) {
          *found = true;
        }
        return entry.value(QStringLiteral("value"));
      }
    }
    return QJsonValue(QJsonValue::Null);
  }

  QJsonValue value(snapshot);
  const QStringList segments = path.split('.', Qt::SkipEmptyParts);
  for (const QString& segment : segments) {
    if (!value.isObject()) {
      return QJsonValue(QJsonValue::Null);
    }
    const QJsonObject object = value.toObject();
    if (!object.contains(segment)) {
      return QJsonValue(QJsonValue::Null);
    }
    value = object.value(segment);
  }
  if (found) {
    *found = !segments.isEmpty();
  }
  return value;
}

QJsonArray debugMcpWatchValues(const QJsonObject& state, const QJsonObject& snapshot) {
  QJsonArray values;
  const QJsonArray descriptors = state.value(QStringLiteral("watchDescriptors")).toArray();
  for (const QJsonValue& descriptorValue : descriptors) {
    const QJsonObject descriptor = descriptorValue.toObject();
    if (!descriptor.value(QStringLiteral("enabled")).toBool(true)) {
      continue;
    }
    const QString path = descriptor.value(QStringLiteral("path")).toString().trimmed();
    if (path.isEmpty()) {
      continue;
    }
    bool found = false;
    QJsonObject value;
    value.insert(QStringLiteral("id"), descriptor.value(QStringLiteral("id")));
    value.insert(QStringLiteral("path"), path);
    value.insert(QStringLiteral("label"), descriptor.value(QStringLiteral("label")));
    value.insert(QStringLiteral("value"), debugMcpWatchValue(snapshot, path, &found));
    value.insert(QStringLiteral("found"), found);
    values.append(value);
  }
  return values;
}

QString debugMcpSnapshotSignature(const QJsonObject& snapshot) {
  const QJsonArray layerIds = debugMcpSelectionArray(snapshot, QStringLiteral("layerIds"));
  const QJsonArray layerNames = debugMcpSelectionArray(snapshot, QStringLiteral("layerNames"));
  const QJsonObject diagnostics = snapshot.value(QStringLiteral("diagnostics")).toObject();
  const QJsonObject latestFailure = diagnostics.value(QStringLiteral("latestFailure")).toObject();
  return QStringLiteral("frame=%1|selectionIds=%2|selectionNames=%3|health=%4|diagSeq=%5|diagSeverity=%6|diagCode=%7|diagComponent=%8|trace=%9|properties=%10")
      .arg(snapshot.value(QStringLiteral("playback"))
               .toObject()
               .value(QStringLiteral("frame"))
               .toVariant()
               .toLongLong())
      .arg(debugMcpJoinedArray(layerIds))
      .arg(debugMcpJoinedArray(layerNames))
      .arg(snapshot.value(QStringLiteral("diagnostics"))
               .toObject()
               .value(QStringLiteral("healthState"))
               .toString())
      .arg(diagnostics.value(QStringLiteral("latestSequence")).toVariant().toLongLong())
      .arg(latestFailure.value(QStringLiteral("severity")).toString())
      .arg(latestFailure.value(QStringLiteral("code")).toString())
      .arg(latestFailure.value(QStringLiteral("component")).toString())
      .arg(debugMcpSnapshotTraceText(snapshot))
      .arg(debugMcpSnapshotPropertiesText(snapshot));
}

QString debugMcpConditionSummary(const QJsonObject& condition) {
  const QString label = condition.value(QStringLiteral("label")).toString().trimmed();
  const QString suffix = label.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(label);
  return QStringLiteral("#%1 %2%3")
      .arg(condition.value(QStringLiteral("id")).toInt())
      .arg(condition.value(QStringLiteral("kind")).toString())
      .arg(suffix);
}

QString debugMcpSnapshotSelectionKey(const QJsonObject& snapshot) {
  const QJsonArray layerIds = debugMcpSelectionArray(snapshot, QStringLiteral("layerIds"));
  const QJsonArray layerNames = debugMcpSelectionArray(snapshot, QStringLiteral("layerNames"));
  return QStringLiteral("%1|%2")
      .arg(debugMcpJoinedArray(layerIds))
      .arg(debugMcpJoinedArray(layerNames));
}

QString debugMcpSnapshotHealthState(const QJsonObject& snapshot) {
  return snapshot.value(QStringLiteral("diagnostics"))
      .toObject()
      .value(QStringLiteral("healthState"))
      .toString();
}

QJsonObject debugMcpLatestFailure(const QJsonObject& snapshot) {
  return snapshot.value(QStringLiteral("diagnostics"))
      .toObject()
      .value(QStringLiteral("latestFailure"))
      .toObject();
}

qint64 debugMcpSnapshotFrame(const QJsonObject& snapshot) {
  return snapshot.value(QStringLiteral("playback"))
      .toObject()
      .value(QStringLiteral("frame"))
      .toVariant()
      .toLongLong();
}

bool debugMcpMatchesCondition(const QJsonObject& condition, const QJsonObject& snapshot) {
  if (!condition.value(QStringLiteral("enabled")).toBool(true)) {
    return false;
  }

  const QString kind = condition.value(QStringLiteral("kind")).toString();
  const QJsonValue rawValue = condition.value(QStringLiteral("value"));

  if (kind == QStringLiteral("frame_equals")) {
    return debugMcpSnapshotFrame(snapshot) ==
           rawValue.toVariant().toLongLong();
  }

  if (kind == QStringLiteral("frame_range")) {
    const QJsonObject value = rawValue.toObject();
    const qint64 frame = debugMcpSnapshotFrame(snapshot);
    const qint64 minFrame = value.value(QStringLiteral("min")).toVariant().toLongLong();
    const qint64 maxFrame = value.value(QStringLiteral("max")).toVariant().toLongLong();
    const bool lowerOk = !value.contains(QStringLiteral("min")) || frame >= minFrame;
    const bool upperOk = !value.contains(QStringLiteral("max")) || frame <= maxFrame;
    return lowerOk && upperOk;
  }

  if (kind == QStringLiteral("selection_contains")) {
    const QString selectionKey = debugMcpSnapshotSelectionKey(snapshot);
    if (rawValue.isArray()) {
      for (const QJsonValue& entry : rawValue.toArray()) {
        const QString needle = entry.toString();
        if (!needle.isEmpty() &&
            (selectionKey.contains(needle, Qt::CaseInsensitive) ||
             debugMcpJoinedArray(debugMcpSelectionArray(snapshot, QStringLiteral("layerIds")))
                 .contains(needle, Qt::CaseInsensitive) ||
             debugMcpJoinedArray(debugMcpSelectionArray(snapshot, QStringLiteral("layerNames")))
                 .contains(needle, Qt::CaseInsensitive))) {
          return true;
        }
      }
      return false;
    }
    const QString needle = rawValue.toString();
    return !needle.isEmpty() && selectionKey.contains(needle, Qt::CaseInsensitive);
  }

  if (kind == QStringLiteral("health_is")) {
    return debugMcpSnapshotHealthState(snapshot).compare(
               rawValue.toString(), Qt::CaseInsensitive) == 0;
  }

  if (kind == QStringLiteral("trace_contains")) {
    const QString needle = rawValue.toString().trimmed();
    return !needle.isEmpty() &&
           debugMcpSnapshotTraceText(snapshot).contains(needle, Qt::CaseInsensitive);
  }

  if (kind == QStringLiteral("diagnostic_severity_is")) {
    const QJsonObject failure = debugMcpLatestFailure(snapshot);
    return !failure.isEmpty() &&
           failure.value(QStringLiteral("severity")).toString().compare(
               rawValue.toString(), Qt::CaseInsensitive) == 0;
  }

  if (kind == QStringLiteral("diagnostic_code_is")) {
    const QJsonObject failure = debugMcpLatestFailure(snapshot);
    return !failure.isEmpty() &&
           failure.value(QStringLiteral("code")).toString().compare(
               rawValue.toString(), Qt::CaseInsensitive) == 0;
  }

  if (kind == QStringLiteral("diagnostic_matches")) {
    const QJsonObject failure = debugMcpLatestFailure(snapshot);
    const QJsonObject expected = rawValue.toObject();
    if (failure.isEmpty()) {
      return false;
    }
    const auto matches = [&failure, &expected](const QString& key) {
      return !expected.contains(key) ||
             failure.value(key).toString().compare(
                 expected.value(key).toString(), Qt::CaseInsensitive) == 0;
    };
    return matches(QStringLiteral("severity")) &&
           matches(QStringLiteral("code")) &&
           matches(QStringLiteral("component")) &&
           matches(QStringLiteral("objectId"));
  }

  if (kind == QStringLiteral("property_equals")) {
    const QJsonObject value = rawValue.toObject();
    const QString path = value.value(QStringLiteral("path")).toString(
        condition.value(QStringLiteral("path")).toString());
    if (path.isEmpty()) {
      return false;
    }

    const QJsonValue propertiesValue = snapshot.value(QStringLiteral("properties"));
    if (!propertiesValue.isArray()) {
      return false;
    }

    const QJsonArray properties = propertiesValue.toArray();
    for (const QJsonValue& entryValue : properties) {
      const QJsonObject entry = entryValue.toObject();
      if (entry.value(QStringLiteral("path")).toString() != path) {
        continue;
      }
      const QJsonValue entrySerialized =
          QJsonValue::fromVariant(entry.value(QStringLiteral("value")).toVariant());
      const QJsonValue expectedSerialized =
          QJsonValue::fromVariant(value.value(QStringLiteral("value")).toVariant());
      if (entrySerialized.isObject() && expectedSerialized.isObject()) {
        return entrySerialized.toObject() == expectedSerialized.toObject();
      }
      if (entrySerialized.isArray() && expectedSerialized.isArray()) {
        return entrySerialized.toArray() == expectedSerialized.toArray();
      }
      return entrySerialized.toVariant() == expectedSerialized.toVariant();
    }
    return false;
  }

  return false;
}

bool debugMcpConditionAlreadyHit(const QJsonObject& state, int conditionId, const QString& signature) {
  const QJsonObject lastBreakHit = state.value(QStringLiteral("lastBreakHit")).toObject();
  return lastBreakHit.value(QStringLiteral("conditionId")).toInt(-1) == conditionId &&
         lastBreakHit.value(QStringLiteral("signature")).toString() == signature;
}

void debugMcpAppendHistoryEntry(QJsonObject& state, QJsonObject entry) {
  QJsonArray history = state.value(QStringLiteral("history")).toArray();
  entry.insert(QStringLiteral("timestamp"),
               QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
  history.append(entry);
  if (history.size() > 100) {
    history = trimJsonArray(history, 100);
  }
  state.insert(QStringLiteral("history"), history);
}

bool debugMcpRecordBreakHit(QJsonObject& state, const QJsonObject& snapshot,
                            const QJsonObject& condition,
                            const QJsonArray& beforeSnapshots) {
  const int conditionId = condition.value(QStringLiteral("id")).toInt(-1);
  if (conditionId < 0) {
    return false;
  }

  const QString signature = debugMcpSnapshotSignature(snapshot);
  if (debugMcpConditionAlreadyHit(state, conditionId, signature)) {
    return false;
  }

  QJsonObject session = state.value(QStringLiteral("session")).toObject();
  const QString playbackState = snapshot.value(QStringLiteral("playback"))
      .toObject()
      .value(QStringLiteral("state"))
      .toString()
      .toLower();
  const bool wasPlaying = playbackState == QStringLiteral("playing");
  session.insert(QStringLiteral("paused"), true);
  session.insert(QStringLiteral("lastAction"), QStringLiteral("break-hit"));
  session.insert(QStringLiteral("pauseReason"), QStringLiteral("breakpoint"));
  session.insert(QStringLiteral("wasPlayingBeforePause"), wasPlaying);
  session.insert(QStringLiteral("pausedAtFrame"), debugMcpSnapshotFrame(snapshot));
  state.insert(QStringLiteral("session"), session);

  QJsonObject lastBreakHit;
  lastBreakHit.insert(QStringLiteral("conditionId"), conditionId);
  lastBreakHit.insert(QStringLiteral("condition"), condition);
  lastBreakHit.insert(QStringLiteral("reason"),
                      QStringLiteral("Matched %1").arg(debugMcpConditionSummary(condition)));
  lastBreakHit.insert(QStringLiteral("matchedAt"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
  lastBreakHit.insert(QStringLiteral("signature"), signature);
  lastBreakHit.insert(QStringLiteral("snapshot"), snapshot);
  lastBreakHit.insert(QStringLiteral("watches"), debugMcpWatchValues(state, snapshot));
  lastBreakHit.insert(QStringLiteral("beforeSnapshots"), trimJsonArray(beforeSnapshots, 8));
  lastBreakHit.insert(QStringLiteral("afterSnapshots"), QJsonArray{});
  state.insert(QStringLiteral("lastBreakHit"), lastBreakHit);

  QJsonObject historyEntry;
  historyEntry.insert(QStringLiteral("type"), QStringLiteral("break-hit"));
  historyEntry.insert(QStringLiteral("conditionId"), conditionId);
  historyEntry.insert(QStringLiteral("conditionKind"),
                      condition.value(QStringLiteral("kind")).toString());
  debugMcpAppendHistoryEntry(state, historyEntry);
  return true;
}

bool debugMcpAutoSyncPlaybackState(QJsonObject& state, ArtifactPlaybackService* playbackService)
{
  if (!playbackService) {
    return false;
  }

  const QJsonObject session = state.value(QStringLiteral("session")).toObject();
  const bool paused = session.value(QStringLiteral("paused")).toBool(false);
  const bool wasPlayingBeforePause = session.value(QStringLiteral("wasPlayingBeforePause")).toBool(false);
  const QString lastAction = session.value(QStringLiteral("lastAction")).toString();
  if (paused && lastAction == QStringLiteral("mcp.stepForward")) {
    const qint64 steps = std::clamp<qint64>(
        session.value(QStringLiteral("stepFrames")).toVariant().toLongLong(), 1, 1000);
    const qint64 current = playbackService->currentFrame().framePosition();
    const qint64 target = std::min(current + steps,
                                   playbackService->frameRange().end());
    playbackService->setCurrentFrame(FramePosition(target));
    QJsonObject updatedSession = session;
    updatedSession.insert(QStringLiteral("lastAction"), QStringLiteral("step-forward"));
    updatedSession.insert(QStringLiteral("stepFrames"), 0);
    state.insert(QStringLiteral("session"), updatedSession);
    return true;
  }
  if (paused && lastAction == QStringLiteral("mcp.stepToFrame")) {
    const qint64 requested = session.value(QStringLiteral("targetFrame"))
                                .toVariant().toLongLong();
    const qint64 target = std::clamp(requested,
                                     playbackService->frameRange().start(),
                                     playbackService->frameRange().end());
    playbackService->setCurrentFrame(FramePosition(target));
    QJsonObject updatedSession = session;
    updatedSession.insert(QStringLiteral("lastAction"), QStringLiteral("step-to-frame"));
    state.insert(QStringLiteral("session"), updatedSession);
    return true;
  }
  if (paused) {
    if (session.value(QStringLiteral("pauseReason")).toString() == QStringLiteral("breakpoint") &&
        playbackService->state() == PlaybackState::Playing) {
      playbackService->pause();
    }
    return false;
  }

  if (wasPlayingBeforePause && playbackService->state() == PlaybackState::Paused) {
    playbackService->play();
    QJsonObject updatedSession = session;
    updatedSession.insert(QStringLiteral("wasPlayingBeforePause"), false);
    updatedSession.insert(QStringLiteral("lastAction"), QStringLiteral("resume"));
    state.insert(QStringLiteral("session"), updatedSession);
    return true;
  }

  return false;
}

class DebugBreakpointPoller final : public QObject {
public:
  explicit DebugBreakpointPoller(QObject* parent, ArtifactPlaybackService* playbackService)
      : QObject(parent), playbackService_(playbackService) {}

  void start()
  {
    if (timerId_ != 0) {
      return;
    }
    evaluate();
    timerId_ = startTimer(200);
  }

protected:
  void timerEvent(QTimerEvent* event) override
  {
    if (!event || event->timerId() != timerId_) {
      return;
    }
    evaluate();
  }

private:
  void evaluate()
  {
    if (!playbackService_) {
      return;
    }

    QJsonObject state = debugMcpNormalizeStateJson(debugMcpReadStateJson());
    bool stateChanged = debugMcpAutoSyncPlaybackState(state, playbackService_);

    const QJsonArray conditions = state.value(QStringLiteral("breakConditions")).toArray();
    const QJsonArray watchDescriptors = state.value(QStringLiteral("watchDescriptors")).toArray();
    if (conditions.isEmpty() && watchDescriptors.isEmpty()) {
      if (stateChanged) {
        debugMcpWriteStateJson(state);
      }
      return;
    }

    const QJsonObject snapshot = buildDebugBridgeSnapshotJson();
    const QJsonArray watchValues = debugMcpWatchValues(state, snapshot);
    if (!watchValues.isEmpty()) {
      QJsonObject watchSnapshot;
      watchSnapshot.insert(QStringLiteral("timestamp"),
                           QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
      watchSnapshot.insert(QStringLiteral("values"), watchValues);
      state.insert(QStringLiteral("lastWatchSnapshot"), watchSnapshot);
      stateChanged = true;
    }
    if (afterSamplesRemaining_ > 0 &&
        !state.value(QStringLiteral("session")).toObject()
             .value(QStringLiteral("paused")).toBool(false)) {
      QJsonObject lastBreakHit = state.value(QStringLiteral("lastBreakHit")).toObject();
      if (lastBreakHit.value(QStringLiteral("conditionId")).toInt(-1) == pendingBreakConditionId_) {
        QJsonArray afterSnapshots = lastBreakHit.value(QStringLiteral("afterSnapshots")).toArray();
        afterSnapshots.append(snapshot);
        lastBreakHit.insert(QStringLiteral("afterSnapshots"), trimJsonArray(afterSnapshots, 8));
        state.insert(QStringLiteral("lastBreakHit"), lastBreakHit);
        --afterSamplesRemaining_;
        debugMcpWriteStateJson(state);
        return;
      }
      afterSamplesRemaining_ = 0;
    }
    if (state.value(QStringLiteral("session")).toObject()
            .value(QStringLiteral("paused")).toBool(false)) {
      if (stateChanged) {
        debugMcpWriteStateJson(state);
      }
      return;
    }

    for (const QJsonValue& value : conditions) {
      const QJsonObject condition = value.toObject();
      if (!debugMcpMatchesCondition(condition, snapshot)) {
        continue;
      }
      if (debugMcpRecordBreakHit(state, snapshot, condition, recentSnapshots_)) {
        if (playbackService_->state() == PlaybackState::Playing) {
          playbackService_->pause();
        }
        pendingBreakConditionId_ = condition.value(QStringLiteral("id")).toInt(-1);
        afterSamplesRemaining_ = 8;
        debugMcpWriteStateJson(state);
      } else if (stateChanged) {
        debugMcpWriteStateJson(state);
      }
      return;
    }

    recentSnapshots_.append(snapshot);
    recentSnapshots_ = trimJsonArray(recentSnapshots_, 8);

    if (stateChanged) {
      debugMcpWriteStateJson(state);
    }
  }

  int timerId_ = 0;
  ArtifactPlaybackService* playbackService_ = nullptr;
  QJsonArray recentSnapshots_;
  int pendingBreakConditionId_ = -1;
  int afterSamplesRemaining_ = 0;
};

class DebugBridgeFileWriter final : public QObject {
public:
  explicit DebugBridgeFileWriter(QObject* parent = nullptr)
      : QObject(parent) {}

  void start()
  {
    if (timerId_ != 0) {
      return;
    }
    writeSnapshot(true);
    timerId_ = startTimer(250);
  }

protected:
  void timerEvent(QTimerEvent* event) override
  {
    if (!event || event->timerId() != timerId_) {
      return;
    }
    writeSnapshot(false);
  }

private:
  void writeSnapshot(const bool force)
  {
    QJsonObject snapshot = buildDebugBridgeSnapshotJson();
    snapshot.insert(QStringLiteral("watches"),
                    debugMcpWatchValues(debugMcpReadStateJson(), snapshot));
    const QByteArray payload = QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
    if (!force && payload == lastPayload_) {
      return;
    }

    const QString filePath = debugBridgeFilePath();
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      qWarning() << "[DebugBridge] failed to open bridge file:" << filePath;
      return;
    }
    if (file.write(payload) != payload.size()) {
      qWarning() << "[DebugBridge] failed to write bridge file:" << filePath;
      file.cancelWriting();
      return;
    }
    if (!file.commit()) {
      qWarning() << "[DebugBridge] failed to commit bridge file:" << filePath;
      return;
    }
    lastPayload_ = payload;
  }

  int timerId_ = 0;
  QByteArray lastPayload_;
};

QString sessionStateFilePath() {
  const QString appDataDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dataDir(appDataDir);
  if (!dataDir.exists()) {
    dataDir.mkpath(QStringLiteral("."));
  }
  return dataDir.filePath(QStringLiteral("session_state.cbor"));
}

QString recoveryDirectoryPath() {
  const QString appDataDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dataDir(appDataDir);
  if (!dataDir.exists()) {
    dataDir.mkpath(QStringLiteral("."));
  }
  return dataDir.filePath(QStringLiteral("Recovery"));
}

bool isStartupDialogSuppressed() {
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  const QString suppressUntilIso =
      sessionStore
          .value(QStringLiteral("Session/startupDialogSuppressUntil"),
                 QString())
          .toString();
  if (suppressUntilIso.isEmpty()) {
    return false;
  }

  const QDateTime suppressUntil =
      QDateTime::fromString(suppressUntilIso, Qt::ISODate);
  if (!suppressUntil.isValid()) {
    return false;
  }
  return QDateTime::currentDateTime() < suppressUntil;
}

void suppressStartupDialogForDays(int days) {
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  const QDateTime suppressUntil =
      QDateTime::currentDateTime().addDays(std::max(1, days));
  sessionStore.setValue(QStringLiteral("Session/startupDialogSuppressUntil"),
                        suppressUntil.toString(Qt::ISODate));
  sessionStore.sync();
}

void sanitizeSessionStateStore() {
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  const QVariant running =
      sessionStore.value(QStringLiteral("Session/isRunning"), false);
  if (running.isValid() && running.typeId() != QMetaType::Bool) {
    sessionStore.setValue(QStringLiteral("Session/isRunning"), false);
  }

  const QVariant pid = sessionStore.value(QStringLiteral("Session/pid"));
  if (pid.isValid() && !pid.canConvert<qlonglong>()) {
    sessionStore.remove(QStringLiteral("Session/pid"));
  }

  const QVariant startTs =
      sessionStore.value(QStringLiteral("Session/startTimestamp"));
  if (startTs.isValid() && startTs.typeId() != QMetaType::QString) {
    sessionStore.remove(QStringLiteral("Session/startTimestamp"));
  }

  const QVariant cleanTs =
      sessionStore.value(QStringLiteral("Session/lastCleanExitTimestamp"));
  if (cleanTs.isValid() && cleanTs.typeId() != QMetaType::QString) {
    sessionStore.remove(QStringLiteral("Session/lastCleanExitTimestamp"));
  }

  const QVariant layoutAttempted =
      sessionStore.value(QStringLiteral("Session/layoutRestoreAttempted"));
  if (layoutAttempted.isValid() &&
      layoutAttempted.typeId() != QMetaType::Bool) {
    sessionStore.remove(QStringLiteral("Session/layoutRestoreAttempted"));
  }

  const QVariant layoutGeomRestored =
      sessionStore.value(QStringLiteral("Session/layoutGeometryRestored"));
  if (layoutGeomRestored.isValid() &&
      layoutGeomRestored.typeId() != QMetaType::Bool) {
    sessionStore.remove(QStringLiteral("Session/layoutGeometryRestored"));
  }

  const QVariant layoutStateRestored =
      sessionStore.value(QStringLiteral("Session/layoutStateRestored"));
  if (layoutStateRestored.isValid() &&
      layoutStateRestored.typeId() != QMetaType::Bool) {
    sessionStore.remove(QStringLiteral("Session/layoutStateRestored"));
  }

  const QVariant layoutResetApplied =
      sessionStore.value(QStringLiteral("Session/layoutResetApplied"));
  if (layoutResetApplied.isValid() &&
      layoutResetApplied.typeId() != QMetaType::Bool) {
    sessionStore.remove(QStringLiteral("Session/layoutResetApplied"));
  }

  const QVariant layoutRestoreTs =
      sessionStore.value(QStringLiteral("Session/layoutRestoreTimestamp"));
  if (layoutRestoreTs.isValid() &&
      layoutRestoreTs.typeId() != QMetaType::QString) {
    sessionStore.remove(QStringLiteral("Session/layoutRestoreTimestamp"));
  }

  const QVariant startupDialogSuppressUntil =
      sessionStore.value(QStringLiteral("Session/startupDialogSuppressUntil"));
  if (startupDialogSuppressUntil.isValid() &&
      startupDialogSuppressUntil.typeId() != QMetaType::QString) {
    sessionStore.remove(QStringLiteral("Session/startupDialogSuppressUntil"));
  }
  sessionStore.sync();
}

void recordLayoutRestoreResult(bool attempted, bool geometryRestored,
                               bool stateRestored, bool resetApplied) {
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  sessionStore.setValue(QStringLiteral("Session/layoutRestoreAttempted"),
                        attempted);
  sessionStore.setValue(QStringLiteral("Session/layoutGeometryRestored"),
                        geometryRestored);
  sessionStore.setValue(QStringLiteral("Session/layoutStateRestored"),
                        stateRestored);
  sessionStore.setValue(QStringLiteral("Session/layoutResetApplied"),
                        resetApplied);
  sessionStore.setValue(QStringLiteral("Session/layoutRestoreTimestamp"),
                        QDateTime::currentDateTime().toString(Qt::ISODate));
  sessionStore.sync();
}

void sanitizeLayoutStore(ArtifactCore::FastSettingsStore &layoutStore) {
  const QVariant geometry =
      layoutStore.value(QStringLiteral("MainWindow/geometry"));
  if (geometry.isValid() && !geometry.canConvert<QByteArray>()) {
    layoutStore.remove(QStringLiteral("MainWindow/geometry"));
  }

  const QVariant state = layoutStore.value(QStringLiteral("MainWindow/state"));
  if (state.isValid() && !state.canConvert<QByteArray>()) {
    layoutStore.remove(QStringLiteral("MainWindow/state"));
  }

  const QVariant version =
      layoutStore.value(QStringLiteral("MainWindow/version"));
  if (version.isValid() && version.typeId() != QMetaType::QString) {
    layoutStore.remove(QStringLiteral("MainWindow/version"));
  }
  layoutStore.sync();
}

QString buildWindowTitle() {
  QString title = QStringLiteral("Artifact %1")
                      .arg(QStringLiteral(ARTIFACT_VERSION_STRING));

  const QString buildHash = QStringLiteral(ARTIFACT_BUILD_GIT_HASH);
  const QString buildStamp = QStringLiteral(ARTIFACT_BUILD_TIMESTAMP);
  const QString buildDirty = QStringLiteral(ARTIFACT_BUILD_DIRTY);

  title += QStringLiteral(" | ");
  title += buildHash;
  if (buildDirty == QStringLiteral("dirty")) {
    title += QStringLiteral("*");
  }
  title += QStringLiteral(" | ");
  title += buildStamp;
  return title;
}

QIcon buildTemporaryAppIcon() {
  QPixmap pix(256, 256);
  pix.fill(QColor(28, 28, 32));

  QPainter painter(&pix);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(212, 125, 50));
  painter.drawRoundedRect(QRectF(28.0, 28.0, 200.0, 200.0), 44.0, 44.0);

  QFont font;
  font.setBold(true);
  font.setPointSize(120);
  painter.setFont(font);
  painter.setPen(Qt::white);
  painter.drawText(pix.rect(), Qt::AlignCenter, QStringLiteral("A"));

  return QIcon(pix);
}

bool markSessionStartAndDetectUncleanExit() {
  sanitizeSessionStateStore();
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  const bool wasRunning =
      sessionStore.value(QStringLiteral("Session/isRunning"), false).toBool();
  sessionStore.setValue(QStringLiteral("Session/isRunning"), true);
  sessionStore.setValue(QStringLiteral("Session/startTimestamp"),
                        QDateTime::currentDateTime().toString(Qt::ISODate));
  sessionStore.setValue(
      QStringLiteral("Session/pid"),
      static_cast<qlonglong>(QCoreApplication::applicationPid()));
  sessionStore.sync();
  return wasRunning;
}

void markSessionEndClean() {
  ArtifactCore::FastSettingsStore sessionStore(sessionStateFilePath());
  sessionStore.setValue(QStringLiteral("Session/isRunning"), false);
  sessionStore.setValue(QStringLiteral("Session/lastCleanExitTimestamp"),
                        QDateTime::currentDateTime().toString(Qt::ISODate));
  sessionStore.sync();
}

char shutdownDiagnosticPathForExit[4096]{};

void recordFinalProcessExitReached() {
  if (shutdownDiagnosticPathForExit[0] == '\0') {
    return;
  }
  if (FILE *file = std::fopen(shutdownDiagnosticPathForExit, "ab")) {
    static constexpr char message[] =
        "FINAL process exit handler reached\r\n";
    std::fwrite(message, 1, sizeof(message) - 1, file);
    std::fflush(file);
    std::fclose(file);
  }
}

void setShutdownDiagnosticPathForExit(const QString &path) {
  const QByteArray encodedPath = QFile::encodeName(path);
  const size_t copyLength = std::min(
      static_cast<size_t>(encodedPath.size()),
      sizeof(shutdownDiagnosticPathForExit) - 1);
  std::memcpy(shutdownDiagnosticPathForExit, encodedPath.constData(),
              copyLength);
  shutdownDiagnosticPathForExit[copyLength] = '\0';
}

void showUncleanExitNoticeIfNeeded(bool hadUncleanExit, QWidget *parent) {
  if (!hadUncleanExit) {
    return;
  }

  QMessageBox box(parent);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle(QStringLiteral("Previous Session Did Not Exit Cleanly"));
  box.setText(QStringLiteral("前回セッションが正常終了していません。"));
  box.setInformativeText(
      QStringLiteral("復旧スナップショットの場所を開きますか？"));
  auto *openFolder = box.addButton(QStringLiteral("Open Recovery Folder"),
                                   QMessageBox::ActionRole);
  box.addButton(QStringLiteral("Continue"), QMessageBox::AcceptRole);
  box.exec();

  if (box.clickedButton() == openFolder) {
    const QString recoveryDir = recoveryDirectoryPath();
    QDir dir(recoveryDir);
    if (!dir.exists()) {
      dir.mkpath(QStringLiteral("."));
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(recoveryDir));
  }
}

bool showRecoveryPrompt(ArtifactAutoSaveManager &autoSave, QWidget *parent) {
  if (!autoSave.hasRecoveryPoint()) {
    return false;
  }

  QMessageBox box(parent);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle("Recovery Snapshot Found");
  box.setText("A crash recovery snapshot was found.");
  box.setInformativeText("Recover the latest snapshot now?");
  auto *recover = box.addButton("Recover", QMessageBox::AcceptRole);
  box.addButton("Ignore", QMessageBox::RejectRole);
  box.exec();

  if (box.clickedButton() != recover) {
    return false;
  }

   ArtifactCore::String recoveredJson;
   ArtifactCore::String sourcePath;
   if (!autoSave.loadLatestRecoveryPoint(&recoveredJson, &sourcePath) ||
       ArtifactCore::toStdString(recoveredJson).empty()) {
    return false;
  }

  const QString tempRoot =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir tempDir(tempRoot);
  tempDir.mkpath(".");
  const QString recoveredPath =
      tempDir.filePath("RecoveredProject.artifact.json");
  QFile out(recoveredPath);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  out.write(QByteArray::fromStdString(
      ArtifactCore::toStdString(recoveredJson)));
  out.close();

  ArtifactProjectManager::getInstance().loadFromFile(recoveredPath);
  return true;
}
} // namespace

void test() {
  cv::Mat mat(400, 640, CV_8UC3, cv::Scalar(0, 0, 0));
  cv::Mat mask(400, 640, CV_8UC3, cv::Scalar(0, 0, 0));
  cv::Mat dst(400, 640, CV_8UC3, cv::Scalar(0, 0, 0));
  drawStar5(mat, cv::Scalar(0, 255, 255), // edgeColor（黄）
            2,                            // edgeThickness
            cv::Scalar(-1, -1, -1),       // fillColor（無視される）
            0.8f);

  float threshold = 0.5f;        // この閾値より明るいピクセルがグローの元になる
  int vertical_blur_radius = 80; // 縦方向ぼかしの半径 (大きいほど縦に伸びる)
  float intensity = 1.1f;        // グローの強度

  cv::Mat glowed_image =
      applyVerticalGlow(mat, threshold, vertical_blur_radius, intensity);
  SetEnvironmentVariableW(L"COREHOST_TRACE", L"1");
  const std::wstring hostfxrTracePath =
      QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
          .filePath(QStringLiteral("hostfxr_trace.log"))
          .toStdWString();
  SetEnvironmentVariableW(L"COREHOST_TRACEFILE", hostfxrTracePath.c_str());

  /*
  DotnetRuntimeHost host;

  // ① .NET SDKのルート or dotnet.exeのルートを指定（例: "C:/Program
  Files/dotnet"） if (!host.initialize("C:/Program Files/dotnet")) { qWarning()
  << "hostfxr 初期化失敗"; return;
  }

  // ② アセンブリパスを指定（.dll）→ ここでは MyApp.dll を仮定
  QString dllPath = QDir(QCoreApplication::applicationDirPath())
      .filePath(QStringLiteral("ArtifactScriptRunner.dll"));

  // 呼び出し先の型名とメソッド名（C#側と一致させる）
  if (!host.loadAssembly(dllPath))
  {
   qWarning() << "アセンブリのロード失敗";
   return;
  }

  void* method = nullptr;
  if (!(method =
  host.getFunctionPointer("ArtifactScriptRunner.ArtifactScriptRunner,ArtifactScriptRunner",
  "Add"))) {
   //std::cerr << hostfxr.getError() << std::endl;
   //return 1;
  }


         */
  // メソッドを呼び出す。
  // int result = reinterpret_cast<int(*)(int, int)>(method)(1, 2);
  // std::cout << result << std::endl;

  /*
  int test_width = 100;
  int test_height = 50;
  cv::Mat input_test_mat(test_height, test_width, CV_32FC4);

  // サンプルデータでMatを埋める (BGRA順)
  // 例えば、左上は青、右上は赤、左下は緑、右下は白
  for (int y = 0; y < test_height; ++y) {
   for (int x = 0; x < test_width; ++x) {
    // OpenCV Vec4f: [Blue, Green, Red, Alpha]
    float B = (float)x / test_width;         // xが進むにつれて青が強くなる
    float G = (float)y / test_height;        // yが進むにつれて緑が強くなる
    float R = 1.0f - (float)x / test_width;  // xが進むにつれて赤が弱くなる
    float A = 1.0f;                          // アルファは常に1.0 (不透明)

    input_test_mat.at<cv::Vec4f>(y, x) = cv::Vec4f(B, G, R, A);
   }
  }

  cv::Mat output_result_mat = process_bgra_mat_with_halide_gpu(input_test_mat);
  */
  cv::Size img_size(800, 600);
  // 背景色: 0.0-1.0の範囲でグレー (RGBA)
  /*
  cv::Scalar bg_color(0.2, 0.2, 0.2, 1.0);

  cv::Point center(400, 300);
  int size = 200;
  int radius = 40; // 角の丸め半径

   cv::Mat pentagon_no_fill = drawFilledRoundedPentagon(img_size, bg_color,
  center, size, radius, cv::Scalar(0.0, 0.0, 0.0, 0.0), // 完全に透明
   cv::Scalar(0.0, 1.0, 0.0, 1.0), // 不透明な緑
   2);

  int test_width = 100;
  int test_height = 50;
  cv::Mat input_test_mat(test_height, test_width, CV_32FC4);

  // サンプルデータでMatを埋める (BGRA順)
  // 例えば、左上は青、右上は赤、左下は緑、右下は白
  for (int y = 0; y < test_height; ++y) {
   for (int x = 0; x < test_width; ++x) {
    // OpenCV Vec4f: [Blue, Green, Red, Alpha]
    float B = (float)x / test_width;         // xが進むにつれて青が強くなる
    float G = (float)y / test_height;        // yが進むにつれて緑が強くなる
    float R = 1.0f - (float)x / test_width;  // xが進むにつれて赤が弱くなる
    float A = 1.0f;                          // アルファは常に1.0 (不透明)

    input_test_mat.at<cv::Vec4f>(y, x) = cv::Vec4f(B, G, R, A);
   }
  }
  */
  // auto testImage = findAndLoadImageInAppDir("test.jpg", CV_32FC4);

  // auto context=new GpuContext();

  // context->Initialize();

  // auto negateCS = new
  // NegateCS(context->D3D12RenderDevice(),context->D3D12DeviceContext());

  // negateCS->loadShaderBinaryFromDirectory(QCoreApplication::applicationDirPath(),
  // "Negate.cso");

  // negateCS->Process(testImage);

  // SpectralGlow glow;

  // glow.ElegantGlow(testImage);

  int width = 600;
  int height = 400;

  // 作成する単色の色 (例: 青)
  // QColor::blue() の代わりに QColor(255, 0, 0) (赤), QColor(0, 255, 0) (緑)
  // なども指定できます。
  // QColor singleColor = QColor(0, 0, 255); // RGB (赤, 緑, 青)
  // の値で青色を設定

  // QImageオブジェクトを作成
  // Format_RGB32 は、各ピクセルが32ビット（RGBA）で表現される形式です。
  // この形式は、多くのシステムで効率的に扱われます。
  // QImage image(width, height, QImage::Format_RGB32);

  // 画像を単色で塗りつぶす
  // fill() メソッドは、指定された色で画像全体を塗りつぶします。
  // image.fill(singleColor);

  // auto file= findFirstFileByLooseExtensionFromAppDir("mov");

  // FFmpegThumbnailExtractor extractor;

  // auto image=extractor.extractThumbnail(file);

  // QString exePath = QCoreApplication::applicationDirPath();

  // 適当なファイル名（例：thumb_1234.png）
  // QString fileName = "thumb_" +
  // QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";

  // フルパスを作成
  // QString fullPath = QDir(exePath).filePath(fileName);
}

static void configureQtPaths() {
  configureQtPluginPaths();
}

#if defined(_WIN32)
static void configureWindowsUtf8Console() {
  if (GetConsoleWindow() != nullptr) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
  }

  std::setlocale(LC_ALL, ".UTF-8");

  const int stdoutFd = _fileno(stdout);
  if (stdoutFd >= 0 && _isatty(stdoutFd)) {
    _setmode(stdoutFd, _O_U8TEXT);
  }

  const int stderrFd = _fileno(stderr);
  if (stderrFd >= 0 && _isatty(stderrFd)) {
    _setmode(stderrFd, _O_U8TEXT);
  }

  const int stdinFd = _fileno(stdin);
  if (stdinFd >= 0 && _isatty(stdinFd)) {
    _setmode(stdinFd, _O_U8TEXT);
  }
}

static void configureWindowsBinaryConsole() {
  const int stdinFd = _fileno(stdin);
  if (stdinFd >= 0) {
    _setmode(stdinFd, _O_BINARY);
  }
  const int stdoutFd = _fileno(stdout);
  if (stdoutFd >= 0) {
    _setmode(stdoutFd, _O_BINARY);
  }
}

static void bindWindowsStandardHandleToCrt(DWORD standardHandleId,
                                           int fileDescriptor) {
  const HANDLE standardHandle = GetStdHandle(standardHandleId);
  if (standardHandle == nullptr || standardHandle == INVALID_HANDLE_VALUE) {
    return;
  }
  const intptr_t existingHandle = _get_osfhandle(fileDescriptor);
  if (existingHandle != -1 && existingHandle != -2 &&
      reinterpret_cast<HANDLE>(existingHandle) == standardHandle) {
    return;
  }

  HANDLE duplicateHandle = INVALID_HANDLE_VALUE;
  if (!DuplicateHandle(GetCurrentProcess(), standardHandle,
                       GetCurrentProcess(), &duplicateHandle, 0, TRUE,
                       DUPLICATE_SAME_ACCESS)) {
    return;
  }
  const int duplicateDescriptor =
      _open_osfhandle(reinterpret_cast<intptr_t>(duplicateHandle), _O_BINARY);
  if (duplicateDescriptor < 0) {
    CloseHandle(duplicateHandle);
    return;
  }
  if (_dup2(duplicateDescriptor, fileDescriptor) != 0) {
    _close(duplicateDescriptor);
    return;
  }
  _close(duplicateDescriptor);
}

static void configureWindowsCliConsole() {
  if (GetConsoleWindow() == nullptr) {
    AttachConsole(ATTACH_PARENT_PROCESS);
  }
  bindWindowsStandardHandleToCrt(STD_INPUT_HANDLE, 0);
  bindWindowsStandardHandleToCrt(STD_OUTPUT_HANDLE, 1);
  bindWindowsStandardHandleToCrt(STD_ERROR_HANDLE, 2);
  if (GetConsoleWindow() != nullptr) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
  }
  std::setlocale(LC_ALL, ".UTF-8");
  const int standardDescriptors[] = {0, 1, 2};
  for (const int descriptor : standardDescriptors) {
    if (_get_osfhandle(descriptor) != -1 && _get_osfhandle(descriptor) != -2) {
      _setmode(descriptor, _O_BINARY);
    }
  }
  std::cin.clear();
  std::cout.clear();
  std::cerr.clear();
}
#else
static void configureWindowsUtf8Console() {}
static void configureWindowsBinaryConsole() {}
static void configureWindowsCliConsole() {}
#endif

static int runMcpTcpServerMode(int argc, char *argv[], quint16 port) {
  QCoreApplication app(argc, argv);
  QTcpServer server;
  if (!server.listen(QHostAddress::LocalHost, port)) {
    qCritical() << "[MCP] Failed to listen on TCP port" << port << server.errorString();
    return 2;
  }
  QObject::connect(&server, &QTcpServer::newConnection, &server, [&server]() {
    while (server.hasPendingConnections()) {
      QTcpSocket *socket = server.nextPendingConnection();
      auto *buffer = new QByteArray();
      QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, buffer]() {
        buffer->append(socket->readAll());
        constexpr qsizetype kMaxMcpFrameBufferBytes = 16 * 1024 * 1024;
        if (buffer->size() > kMaxMcpFrameBufferBytes) {
          qWarning() << "[MCP] Disconnecting client with oversized frame buffer";
          socket->disconnectFromHost();
          return;
        }
        QJsonObject request;
        while (ArtifactCore::McpBridge::tryPopFrame(buffer, &request)) {
          const QByteArray response = ArtifactCore::McpBridge::encodeFrame(
              ArtifactCore::McpBridge::handleRequest(request, ArtifactCore::AIContext()));
          socket->write(response);
        }
        socket->flush();
      });
      QObject::connect(socket, &QTcpSocket::disconnected, socket, [socket, buffer]() {
        delete buffer;
        socket->deleteLater();
      });
    }
  });
  qInfo() << "[MCP] Debug TCP server listening on localhost:" << port;
  return app.exec();
}

static int runMcpServerMode(int argc, char *argv[], quint16 tcpPort = 0) {
  if (tcpPort > 0) {
    return runMcpTcpServerMode(argc, argv, tcpPort);
  }
  configureWindowsBinaryConsole();

  QByteArray inputBuffer;
  char chunk[4096];
  while (true) {
    std::cin.read(chunk, sizeof(chunk));
    const std::streamsize readCount = std::cin.gcount();
    if (readCount <= 0) {
      break;
    }
    inputBuffer.append(chunk, static_cast<int>(readCount));

    QJsonObject request;
    while (ArtifactCore::McpBridge::tryPopFrame(&inputBuffer, &request)) {
      const QJsonObject response = ArtifactCore::McpBridge::handleRequest(
          request, ArtifactCore::AIContext());
      const QByteArray frame = ArtifactCore::McpBridge::encodeFrame(response);
      std::cout.write(frame.constData(),
                      static_cast<std::streamsize>(frame.size()));
      std::cout.flush();
    }
  }

  return 0;
}

static int runPythonCli(int argc, char *argv[],
                        const Artifact::CommandLine& commandLine) {
  QCoreApplication app(argc, argv);
  auto* applicationManager = Artifact::ArtifactApplicationManager::instance();
  (void)applicationManager;
  (void)Artifact::ArtifactProjectService::instance();

  QString capturedStdout;
  QString capturedStderr;
  QString capturedValue;
  const bool machineOutput = commandLine.gui.jsonOutput ||
                             commandLine.python.jsonLines;
  const bool machineRepl = commandLine.python.jsonLines;
  const bool interactive = commandLine.python.action == QStringLiteral("repl");
  auto& python = ArtifactCore::PythonEngine::instance();
  python.setOutputCallback([&](const std::string& text, bool isError) {
    const QString output = QString::fromUtf8(
        text.data(), static_cast<qsizetype>(text.size()));
    if (machineOutput) {
      (isError ? capturedStderr : capturedStdout).append(output);
    }
    if (!machineOutput) {
      std::ostream& stream = isError ? std::cerr : std::cout;
      stream.write(text.data(), static_cast<std::streamsize>(text.size()));
      stream.flush();
    }
  });
  const auto pythonCleanup = qScopeGuard([&python]() {
    python.setOutputCallback({});
    if (python.isInitialized()) {
      python.finalize();
    }
  });
  Q_UNUSED(pythonCleanup);

  const auto writeJsonResponse = [&](bool success, int exitCode,
                                     const QString& errorCode,
                                     const QString& errorMessage) {
    QJsonObject response;
    response[QStringLiteral("schemaVersion")] = 1;
    response[QStringLiteral("ok")] = success;
    response[QStringLiteral("command")] = QStringLiteral("python.%1").arg(
        commandLine.python.action);
    QJsonObject result;
    result[QStringLiteral("stdout")] = capturedStdout;
    result[QStringLiteral("stderr")] = capturedStderr;
    if (commandLine.python.action == QStringLiteral("eval") && success) {
      result[QStringLiteral("value")] = capturedValue;
    }
    response[QStringLiteral("result")] = result;
    QJsonArray warnings;
    if (python.isExternalRuntime()) {
      warnings.append(QStringLiteral(
          "External Python mode cannot call Artifact's in-process C++ API"));
    }
    response[QStringLiteral("warnings")] = warnings;
    if (!success) {
      QJsonObject error;
      error[QStringLiteral("code")] = errorCode;
      error[QStringLiteral("message")] = errorMessage;
      error[QStringLiteral("details")] = QJsonObject{};
      response[QStringLiteral("error")] = error;
    }
    const QByteArray json = QJsonDocument(response).toJson(QJsonDocument::Compact);
    std::cout.write(json.constData(), static_cast<std::streamsize>(json.size()));
    std::cout.put('\n');
    std::cout.flush();
    return exitCode;
  };

  if (!python.initialize()) {
    const std::string error = python.getLastError();
    const QString message = QString::fromUtf8(
        error.data(), static_cast<qsizetype>(error.size()));
    if (machineOutput) {
      return writeJsonResponse(false, 3, QStringLiteral("python_unavailable"), message);
    }
    if (capturedStderr.isEmpty()) {
      std::cerr << (message.isEmpty() ? "Python runtime is unavailable" : message.toStdString())
                << '\n';
    }
    return 3;
  }

  if (python.isExternalRuntime() && interactive) {
    const QString message = QStringLiteral(
        "Interactive Python requires the embedded Python runtime");
    if (machineOutput) {
      return writeJsonResponse(false, 3, QStringLiteral("persistent_runtime_unavailable"), message);
    }
    std::cerr << message.toStdString() << '\n';
    return 3;
  }
  if (python.isExternalRuntime() && !machineOutput) {
    std::cerr << "Warning: external Python cannot call Artifact's in-process C++ API\n";
  }

  registerPythonApplicationAPI();
  python.clearError();
  if (!commandLine.python.projectPath.isEmpty() &&
      !Artifact::ArtifactProjectManager::getInstance().loadFromFile(
          commandLine.python.projectPath)) {
    const QString message = QStringLiteral("Failed to load project: %1")
                                .arg(commandLine.python.projectPath);
    if (machineOutput) {
      return writeJsonResponse(false, 1, QStringLiteral("project_load_failed"), message);
    }
    std::cerr << message.toStdString() << '\n';
    return 1;
  }

  bool succeeded = true;
  if (commandLine.python.action == QStringLiteral("run")) {
    succeeded = python.executeFile(commandLine.python.target.toStdString());
  } else if (commandLine.python.action == QStringLiteral("eval")) {
    const std::string value = python.evaluate(commandLine.python.target.toStdString());
    succeeded = !python.hasError();
    if (succeeded && !machineOutput) {
      std::cout << value << '\n';
    }
    if (succeeded && machineOutput) {
      capturedValue = QString::fromUtf8(value.data(),
                                        static_cast<qsizetype>(value.size()));
    }
  } else {
    bool continuation = false;
    bool hadFailure = false;
    QString pendingRequestId;
    while (true) {
      if (!machineRepl) {
        std::cout << (continuation ? "... " : ">>> ") << std::flush;
      }
      std::string line;
      bool inputReady = false;
      bool requestOverLimit = false;
      if (machineRepl) {
        constexpr size_t maximumRequestBytes = 1024 * 1024;
        bool consumedInput = false;
        char character = 0;
        while (std::cin.get(character)) {
          consumedInput = true;
          if (character == '\n') {
            break;
          }
          if (line.size() < maximumRequestBytes) {
            line.push_back(character);
          } else {
            requestOverLimit = true;
          }
        }
        inputReady = consumedInput;
      } else {
        inputReady = static_cast<bool>(std::getline(std::cin, line));
      }
      if (!inputReady) {
        if (continuation) {
          if (machineRepl) {
            QJsonObject event;
            event[QStringLiteral("schemaVersion")] = 1;
            event[QStringLiteral("event")] = QStringLiteral("session_end");
            event[QStringLiteral("ok")] = false;
            event[QStringLiteral("status")] = QStringLiteral("incomplete_input");
            if (!pendingRequestId.isEmpty()) {
              event[QStringLiteral("requestId")] = pendingRequestId;
            }
            event[QStringLiteral("message")] = QStringLiteral(
                "Input ended while Python was waiting for more lines");
            const QByteArray json = QJsonDocument(event).toJson(QJsonDocument::Compact);
            std::cout.write(json.constData(),
                            static_cast<std::streamsize>(json.size()));
            std::cout.put('\n');
            std::cout.flush();
          } else {
            std::cerr << "Incomplete Python input at end of stream\n";
          }
          hadFailure = true;
          python.resetConsole();
        }
        break;
      }
      if (machineRepl) {
        capturedStdout.clear();
        capturedStderr.clear();
        const QByteArray requestBytes(line.data(),
                                      static_cast<qsizetype>(line.size()));
        QString requestId;
        const auto writeJsonLineResponse = [&requestId](QJsonObject response) {
          response[QStringLiteral("schemaVersion")] = 1;
          if (!requestId.isEmpty()) {
            response[QStringLiteral("requestId")] = requestId;
          }
          const QByteArray json = QJsonDocument(response).toJson(QJsonDocument::Compact);
          std::cout.write(json.constData(), static_cast<std::streamsize>(json.size()));
          std::cout.put('\n');
          std::cout.flush();
        };
        if (requestOverLimit || requestBytes.size() > 1024 * 1024) {
          QJsonObject response;
          response[QStringLiteral("ok")] = false;
          response[QStringLiteral("command")] = QStringLiteral("python.repl");
          response[QStringLiteral("status")] = QStringLiteral("error");
          QJsonObject error;
          error[QStringLiteral("code")] = QStringLiteral("request_too_large");
          error[QStringLiteral("message")] = QStringLiteral(
              "JSON Lines request exceeds 1 MiB");
          error[QStringLiteral("details")] = QJsonObject{};
          response[QStringLiteral("error")] = error;
          writeJsonLineResponse(response);
          hadFailure = true;
          continue;
        }
        QJsonParseError parseError;
        const QJsonDocument requestDocument =
            QJsonDocument::fromJson(requestBytes, &parseError);
        const QJsonObject request = requestDocument.object();
        const QJsonValue codeValue = request.value(QStringLiteral("code"));
        const QJsonValue requestIdValue = request.value(QStringLiteral("requestId"));
        requestId = requestIdValue.toString();
        if (parseError.error != QJsonParseError::NoError ||
            !requestDocument.isObject() || !codeValue.isString() ||
            (!requestIdValue.isUndefined() &&
             (!requestIdValue.isString() || requestId.size() > 256))) {
          QJsonObject response;
          response[QStringLiteral("ok")] = false;
          response[QStringLiteral("command")] = QStringLiteral("python.repl");
          response[QStringLiteral("status")] = QStringLiteral("error");
          QJsonObject error;
          error[QStringLiteral("code")] = QStringLiteral("invalid_request");
          error[QStringLiteral("message")] = QStringLiteral(
              "Each JSON Lines request requires a string code and optional requestId up to 256 characters");
          error[QStringLiteral("details")] = QJsonObject{};
          response[QStringLiteral("error")] = error;
          writeJsonLineResponse(response);
          hadFailure = true;
          continue;
        }
        python.clearError();
        continuation = python.pushConsoleLine(
            codeValue.toString().toUtf8().toStdString());
        pendingRequestId = continuation ? requestId : QString();
        const bool lineFailed = !continuation && python.hasError();
        if (lineFailed) {
          hadFailure = true;
          if (capturedStderr.isEmpty()) {
            const std::string error = python.getLastError();
            capturedStderr = QString::fromUtf8(
                error.data(), static_cast<qsizetype>(error.size()));
          }
        }
        QJsonObject response;
        response[QStringLiteral("ok")] = !lineFailed;
        response[QStringLiteral("command")] = QStringLiteral("python.repl");
        response[QStringLiteral("status")] =
            continuation ? QStringLiteral("needs_more_input")
                         : (lineFailed ? QStringLiteral("error")
                                       : QStringLiteral("executed"));
        QJsonObject result;
        result[QStringLiteral("stdout")] = capturedStdout;
        result[QStringLiteral("stderr")] = capturedStderr;
        response[QStringLiteral("result")] = result;
        if (lineFailed) {
          QJsonObject error;
          error[QStringLiteral("code")] = QStringLiteral("python_execution_failed");
          error[QStringLiteral("message")] = capturedStderr.trimmed();
          error[QStringLiteral("details")] = QJsonObject{};
          response[QStringLiteral("error")] = error;
        }
        writeJsonLineResponse(response);
        continue;
      }
      if (!continuation &&
          (line == "exit()" || line == "quit()" ||
           line == "exit" || line == "quit")) {
        break;
      }
      python.clearError();
      continuation = python.pushConsoleLine(line);
      if (!continuation && python.hasError()) {
        hadFailure = true;
      }
    }
    succeeded = !hadFailure;
  }

  if (!succeeded) {
    const std::string lastError = python.getLastError();
    if (capturedStderr.isEmpty() && !lastError.empty()) {
      capturedStderr = QString::fromUtf8(lastError.data(),
                                         static_cast<qsizetype>(lastError.size()));
    }
  }

  int exitCode = succeeded ? 0 : 1;
  if (machineRepl) {
    return exitCode;
  }
  if (machineOutput) {
    exitCode = writeJsonResponse(
        succeeded, exitCode,
        succeeded ? QString() : QStringLiteral("python_execution_failed"),
        succeeded ? QString() : capturedStderr.trimmed());
  }

  return exitCode;
}

static int runCommandIRCli(int argc, char *argv[],
                           const Artifact::CommandLine& commandLine) {
  QCoreApplication app(argc, argv);
  auto* applicationManager = Artifact::ArtifactApplicationManager::instance();
  (void)applicationManager;
  (void)Artifact::ArtifactProjectService::instance();
  Artifact::WorkspaceAutomation::ensureRegistered();

  struct ProcessedRequest {
    QJsonObject response;
    int exitCode = 0;
  };
  const auto makeError = [](const QString& code, const QString& message,
                            const QString& requestId, int exitCode) {
    QJsonObject response;
    response[QStringLiteral("schemaVersion")] = 1;
    response[QStringLiteral("ok")] = false;
    response[QStringLiteral("command")] = QStringLiteral("command-ir");
    if (!requestId.isEmpty()) {
      response[QStringLiteral("requestId")] = requestId;
    }
    QJsonObject error;
    error[QStringLiteral("code")] = code;
    error[QStringLiteral("message")] = message;
    error[QStringLiteral("details")] = QJsonObject{};
    response[QStringLiteral("error")] = error;
    response[QStringLiteral("warnings")] = QJsonArray{};
    return ProcessedRequest{response, exitCode};
  };

  constexpr qint64 maximumRequestBytes = 1024 * 1024;
  bool projectLoaded = false;
  const auto processRequest = [&](const QByteArray& bytes) -> ProcessedRequest {
    QJsonParseError parseError;
    const QJsonDocument requestDocument = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !requestDocument.isObject()) {
      return makeError(QStringLiteral("invalid_request"),
                       QStringLiteral("Request must be a JSON object: %1")
                           .arg(parseError.errorString()), {}, 2);
    }

    const QJsonObject request = requestDocument.object();
    const QString requestId = request.value(QStringLiteral("requestId")).toString();
    const QString operation = request.value(QStringLiteral("operation")).toString();
    const QJsonValue saveProjectValue = request.value(QStringLiteral("saveProject"));
    if (request.value(QStringLiteral("schemaVersion")).toInt(-1) != 1 ||
        !request.value(QStringLiteral("operation")).isString() ||
        (operation != QStringLiteral("catalog") &&
         operation != QStringLiteral("validate") &&
         operation != QStringLiteral("execute")) ||
        (operation != QStringLiteral("catalog") &&
         !request.value(QStringLiteral("command")).isObject()) ||
        (operation == QStringLiteral("catalog") &&
         !request.value(QStringLiteral("command")).isUndefined()) ||
        (!saveProjectValue.isUndefined() && !saveProjectValue.isBool()) ||
        (!saveProjectValue.isUndefined() &&
         operation != QStringLiteral("execute")) ||
        (!request.value(QStringLiteral("requestId")).isUndefined() &&
         (!request.value(QStringLiteral("requestId")).isString() ||
          requestId.size() > 256))) {
      return makeError(QStringLiteral("invalid_request"),
                       QStringLiteral("Request requires schemaVersion 1, operation catalog, validate, or execute; validate and execute require a command object; saveProject is an optional boolean for execute only; requestId is optional and limited to 256 characters"),
                       requestId.left(256), 2);
    }

    if (operation == QStringLiteral("execute")) {
      if (commandLine.commandIR.projectPath.isEmpty()) {
        return makeError(QStringLiteral("project_required"),
                         QStringLiteral("Command execution requires --project <file>"),
                         requestId, 2);
      }
      if (!projectLoaded) {
        const QString projectPath = commandLine.commandIR.projectPath;
        if (!Artifact::ArtifactProjectManager::getInstance().loadFromFile(projectPath)) {
          return makeError(QStringLiteral("project_load_failed"),
                           QStringLiteral("Failed to load project: %1").arg(projectPath),
                           requestId, 1);
        }
        projectLoaded = true;
      }
    }

    const QString method = operation == QStringLiteral("catalog")
        ? QStringLiteral("commandVocabulary")
        : operation == QStringLiteral("validate")
            ? QStringLiteral("validateCommand") : QStringLiteral("executeCommand");
    QVariantList arguments;
    if (operation != QStringLiteral("catalog")) {
      arguments.append(request.value(QStringLiteral("command")).toObject().toVariantMap());
    }
    const QVariant returned = Artifact::WorkspaceAutomation::instance().invokeMethod(
        method, arguments);
    const QVariantMap result = returned.toMap();
    const bool commandSucceeded = operation == QStringLiteral("catalog") ||
        result.value(QStringLiteral("ok"), result.value(QStringLiteral("success"))).toBool();
    bool projectSaved = false;
    QString saveError;
    QString saveErrorStage;
    if (operation == QStringLiteral("execute") &&
        saveProjectValue.toBool() && commandSucceeded) {
      const ArtifactProjectExporterResult saveResult =
          Artifact::ArtifactProjectManager::getInstance().saveToFile(
              commandLine.commandIR.projectPath);
      projectSaved = saveResult.success;
      if (!projectSaved) {
        saveErrorStage = saveResult.errorStage;
        saveError = saveResult.errorMessage.isEmpty()
            ? QStringLiteral("Project save failed at %1").arg(saveResult.errorStage)
            : saveResult.errorMessage;
      }
    }
    const bool succeeded = commandSucceeded &&
        (!saveProjectValue.toBool() || projectSaved);
    QJsonObject response;
    response[QStringLiteral("schemaVersion")] = 1;
    response[QStringLiteral("ok")] = succeeded;
    response[QStringLiteral("command")] = QStringLiteral("command-ir.%1").arg(operation);
    if (!requestId.isEmpty()) {
      response[QStringLiteral("requestId")] = requestId;
    }
    if (operation == QStringLiteral("catalog")) {
      response[QStringLiteral("result")] = QJsonValue::fromVariant(returned);
    } else {
      QVariantMap resultWithPersistence = result;
      if (operation == QStringLiteral("execute")) {
        resultWithPersistence.insert(QStringLiteral("projectSaved"), projectSaved);
      }
      response[QStringLiteral("result")] =
          QJsonObject::fromVariantMap(resultWithPersistence);
    }
    response[QStringLiteral("warnings")] = QJsonArray{};
    if (!succeeded) {
      QJsonObject error;
      error[QStringLiteral("code")] = !commandSucceeded
          ? result.value(QStringLiteral("errorCode"), QStringLiteral("COMMAND_FAILED")).toString()
          : QStringLiteral("PROJECT_SAVE_FAILED");
      error[QStringLiteral("message")] = !commandSucceeded
          ? result.value(QStringLiteral("error")).toString()
          : QStringLiteral("Command executed but the project was not saved: %1").arg(saveError);
      QJsonObject errorDetails = QJsonObject::fromVariantMap(
          result.value(QStringLiteral("diagnostics")).toMap());
      if (commandSucceeded && saveProjectValue.toBool()) {
        errorDetails[QStringLiteral("projectPath")] = commandLine.commandIR.projectPath;
        errorDetails[QStringLiteral("projectSaved")] = false;
        errorDetails[QStringLiteral("saveStage")] = saveErrorStage;
      }
      error[QStringLiteral("details")] = errorDetails;
      response[QStringLiteral("error")] = error;
    }
    return {response, succeeded ? 0 : 1};
  };

  const auto writeResponse = [](const QJsonObject& response) {
    const QByteArray json = QJsonDocument(response).toJson(QJsonDocument::Compact);
    std::cout.write(json.constData(), static_cast<std::streamsize>(json.size()));
    std::cout.put('\n');
    std::cout.flush();
  };
  const auto makeOversizedRequest = [&makeError]() {
    return makeError(QStringLiteral("request_too_large"),
                     QStringLiteral("Command IR request exceeds 1 MiB"), {}, 2);
  };

  if (commandLine.commandIR.requestPath == QStringLiteral("-")) {
    int processExitCode = 0;
    int processedRequestCount = 0;
    while (true) {
      QByteArray line;
      bool consumedInput = false;
      bool exceedsLimit = false;
      char character = 0;
      while (std::cin.get(character)) {
        consumedInput = true;
        if (character == '\n') {
          break;
        }
        if (line.size() < maximumRequestBytes) {
          line.append(character);
        } else {
          exceedsLimit = true;
        }
      }
      if (!consumedInput) {
        break;
      }
      const ProcessedRequest processed = exceedsLimit
          ? makeOversizedRequest() : processRequest(line);
      writeResponse(processed.response);
      ++processedRequestCount;
      processExitCode = std::max(processExitCode, processed.exitCode);
    }
    if (processedRequestCount == 0) {
      const ProcessedRequest error = makeError(
          QStringLiteral("empty_stream"),
          QStringLiteral("No Command IR JSON Lines requests were received"), {}, 2);
      writeResponse(error.response);
      return error.exitCode;
    }
    return processExitCode;
  }

  QFile requestFile(commandLine.commandIR.requestPath);
  if (!requestFile.open(QIODevice::ReadOnly)) {
    const ProcessedRequest error = makeError(
        QStringLiteral("request_unreadable"),
        QStringLiteral("Unable to open Command IR request"), {}, 2);
    writeResponse(error.response);
    return error.exitCode;
  }
  if (requestFile.size() > maximumRequestBytes) {
    const ProcessedRequest error = makeOversizedRequest();
    writeResponse(error.response);
    return error.exitCode;
  }
  const ProcessedRequest processed = processRequest(requestFile.readAll());
  writeResponse(processed.response);
  return processed.exitCode;
}

int main(int argc, char *argv[]) {
  // Registered before function-local services are constructed, so this runs
  // after their exit handlers. Its marker distinguishes a completed process
  // exit from a shutdown that stalled during late static destruction.
  std::atexit(recordFinalProcessExitReached);
  configureWindowsUtf8Console();
  ArtifactCore::CrashHandler::install();
  ArtifactCore::CrashHandler::setCrashCallback([](const QString& crashReportPath) {
    if (auto *rq = Artifact::ArtifactRenderQueueService::instance()) {
      rq->sessionLedger().recordCrash(
          QStringLiteral("Crash report: %1").arg(crashReportPath));
    }
    ArtifactCore::TraceRecorder::instance().recordCrash(traceCrashFromReportPath(crashReportPath));
  });
  ArtifactCore::Logger::instance()->install();

  // Post-startup ingestion: prior crash reports are pulled into the
  // Core DiagnosticRecorder via the crash-report Adapter (not inside the
  // exception filter, which only writes the file and fires the callback).
  ArtifactCore::CrashHandler::ingestPendingReports();

  qDebug() << "Artifact Debug Console Initialized. Hello ArtifactStudio!";

  const unsigned int startupParallelism =
      std::max(1u, std::thread::hardware_concurrency() > 1
                       ? std::thread::hardware_concurrency() - 1
                       : 1u);
  auto parallelismControl = std::make_unique<StartupParallelismControl>(1u);

  const std::wstring applicationDirectory =
      QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath().toStdWString();
  AddDllDirectory(applicationDirectory.c_str());
  SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
                           LOAD_LIBRARY_SEARCH_USER_DIRS);
  // qsetenv("QT_QPA_PLATFORM", "windows:darkmode=[1]");

  // QTextCodec::setCodecForLocale(QTextCodec::codecForName("Shift-JIS"));

  QStringList appArgs;
  for (int i = 0; i < argc; ++i) {
    appArgs << QString::fromLocal8Bit(argv[i]);
  }
  const Artifact::CommandLine parsedCommandLine =
      Artifact::parseCommandLine(appArgs);
  if (parsedCommandLine.type != Artifact::CommandType::Gui) {
    configureWindowsCliConsole();
  }
  const Artifact::CommandLineResult commandLineResult =
      Artifact::validateCommandLine(parsedCommandLine);
  if (!commandLineResult.isValid()) {
    fprintf(stderr, "%s\n",
            commandLineResult.errorMessage.toLocal8Bit().constData());
    return 2;
  }
  const Artifact::CommandLine& commandLine = commandLineResult.commandLine;

  if (commandLine.type == Artifact::CommandType::Help) {
    printf("ArtifactStudio v%s (%s)\n", ARTIFACT_VERSION_STRING, ARTIFACT_BUILD_GIT_HASH);
    printf("Usage: Artifact.exe [options] [project.artifact]\n");
    printf("       Artifact.exe render <project.artifact> [render options]\n\n");
    printf("Options:\n");
    printf("  -h, --help          Show this help message and exit\n");
    printf("  --version           Show version information and exit\n");
    printf("  --lang <code>       Set UI language (ja/en/zh/zh-tw)\n");
    printf("  --renderer <api>    Select renderer (auto/dx12/vulkan)\n");
    printf("  -i, --interactive   Start the interactive command shell\n");
    printf("  --command <command> Execute one shell command and exit\n");
    printf("  --request <file|-> Execute one JSON request or a stdin JSONL stream\n");
    printf("  command-ir <request.json|-> [--project <file>] Execute Command IR JSON requests\n");
    printf("  --json              Wrap single command result as JSON\n");
    printf("  --script <file>     Execute an interactive command file\n");
    printf("  python run <file>   Execute a Python script\n");
    printf("  python eval <expr>  Evaluate a Python expression\n");
    printf("  python repl         Start an interactive Python session\n");
    printf("  python repl --jsonl Use JSON Lines input/output in the Python REPL\n");
    printf("  --threads <count>   Set render worker thread count\n");
    printf("  --safe-mode         Start with optional integrations disabled\n");
    printf("  --verbose           Enable verbose startup diagnostics\n");
    printf("  --log-file <path>   Select a diagnostic log path\n");
    printf("  --no-splash         Suppress the startup splash screen\n");
    printf("  --mcp-server        Run in MCP (Model Context Protocol) server mode\n");
    printf("  --mcp-debug         Run in MCP debug server mode (stdio unless --mcp-port is set)\n");
    printf("  --mcp-port <port>   Listen on localhost TCP for MCP debug requests\n");
    printf("  --plugin-list       List all registered plugins and exit\n");
    printf("  --plugin-info <id>  Show details for a specific plugin and exit\n");
    printf("\nEnvironment:\n");
    printf("  ARTIFACT_RUN_BUILTIN_TESTS  Run built-in tests and exit\n");
    printf("  ARTIFACT_RUN_GPU_BLEND_TESTS  Run headless GPU blend tests and exit\n");
    return 0;
  }

  if (commandLine.type == Artifact::CommandType::Version) {
    printf("ArtifactStudio v%s (%s)\n", ARTIFACT_VERSION_STRING, ARTIFACT_BUILD_GIT_HASH);
    return 0;
  }

  if (commandLine.type == Artifact::CommandType::McpServer) {
    quint16 mcpPort = 0;
    const int portIndex = appArgs.indexOf(QStringLiteral("--mcp-port"));
    if (portIndex >= 0 && portIndex + 1 < appArgs.size()) {
      bool ok = false;
      const int parsedPort = appArgs[portIndex + 1].toInt(&ok);
      if (ok && parsedPort > 0 && parsedPort <= 65535) {
        mcpPort = static_cast<quint16>(parsedPort);
      } else {
        fprintf(stderr, "Invalid --mcp-port value\n");
        return 2;
      }
    }
    return runMcpServerMode(argc, argv, mcpPort);
  }
  if (commandLine.type == Artifact::CommandType::CommandIR) {
    return runCommandIRCli(argc, argv, commandLine);
  }
  const QStringList launchProjectPaths = commandLine.gui.projectPaths;

  if (commandLine.type == Artifact::CommandType::Interactive) {
    const Artifact::InteractiveShellResult result = Artifact::runInteractiveShell(
        launchProjectPaths, commandLine.gui.scriptPath,
        commandLine.gui.singleCommand, commandLine.gui.jsonOutput,
        commandLine.gui.requestPath);
    return result.exitCode;
  }

  if (commandLine.type == Artifact::CommandType::Python) {
    return runPythonCli(argc, argv, commandLine);
  }

  if (commandLine.type == Artifact::CommandType::Render) {
    fprintf(stderr,
            "The render command is valid but the headless executor is not implemented yet.\n");
    return 3;
  }

  if (commandLine.type == Artifact::CommandType::PluginList) {
    printf("Registered Plugins:\n");
    const auto& registry = ArtifactCore::ArtifactPluginRegistry::instance();
    const auto plugins = registry.allPlugins();
    if (plugins.empty()) {
      printf("  (none)\n");
    } else {
      for (const auto& p : plugins) {
        const char* categoryNames[] = {"Effect", "Layer", "Tool", "ImportExport"};
        const char* cat = (static_cast<int>(p.category) >= 0 && static_cast<int>(p.category) <= 3)
            ? categoryNames[static_cast<int>(p.category)] : "Unknown";
        const char* stateNames[] = {"Discovered", "Validated", "Registered", "Active", "Inactive", "Failed", "Unloaded"};
        const char* st = (static_cast<int>(p.state) >= 0 && static_cast<int>(p.state) <= 6)
            ? stateNames[static_cast<int>(p.state)] : "Unknown";
        printf("  %-40s %-12s %-10s v%s\n", ArtifactCore::toStdString(p.id).c_str(), cat, st, ArtifactCore::toStdString(p.version).c_str());
        printf("    %s\n", ArtifactCore::toStdString(p.displayName).c_str());
      }
    }
    printf("\nTotal: %zu plugin(s)\n", plugins.size());
    return 0;
  }

  {
    if (commandLine.type == Artifact::CommandType::PluginInfo) {
      const QString pluginId = commandLine.pluginInfo.pluginId;
      const auto& registry = ArtifactCore::ArtifactPluginRegistry::instance();
      auto optDesc = registry.pluginById(pluginId.toStdString());
      if (optDesc) {
        const auto& p = *optDesc;
        const char* categoryNames[] = {"Effect", "Layer", "Tool", "ImportExport"};
        const char* cat = (static_cast<int>(p.category) >= 0 && static_cast<int>(p.category) <= 3)
            ? categoryNames[static_cast<int>(p.category)] : "Unknown";
        printf("Plugin: %s\n", ArtifactCore::toStdString(p.id).c_str());
        printf("  Display Name: %s\n", ArtifactCore::toStdString(p.displayName).c_str());
        printf("  Version:      %s\n", ArtifactCore::toStdString(p.version).c_str());
        printf("  Author:       %s\n", ArtifactCore::toStdString(p.author).c_str());
        printf("  Category:     %s\n", cat);
        printf("  Path:         %s\n", ArtifactCore::toStdString(p.pluginPath).c_str());
        printf("  Description:  %s\n", ArtifactCore::toStdString(p.description).c_str());
      } else {
        printf("Plugin not found: %s\n", pluginId.toLocal8Bit().constData());
        return 1;
      }
      return 0;
    }
  }

  // ============================================================
  // 起動言語の決定（--lang → システムロケール → en）
  // ここで一度だけ確定し、以降で再判定しない。
  // ============================================================
  {
    QString localeCode;
    QString decidedBy;

    QString persistedLanguage;
    if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
      persistedLanguage = settings->appLanguageCode().trimmed();
    }

    if (!commandLine.global.languageCode.isEmpty()) {
      localeCode = commandLine.global.languageCode.trimmed();
      decidedBy = QStringLiteral("--lang");
      if (localeCode.compare(QStringLiteral("japanese"), Qt::CaseInsensitive) == 0) {
        localeCode = QStringLiteral("ja");
      } else if (localeCode.compare(QStringLiteral("english"), Qt::CaseInsensitive) == 0) {
        localeCode = QStringLiteral("en");
      } else if (localeCode.compare(QStringLiteral("chinese-traditional"), Qt::CaseInsensitive) == 0) {
        localeCode = QStringLiteral("zh-TW");
      } else if (localeCode.compare(QStringLiteral("chinese-simplified"), Qt::CaseInsensitive) == 0) {
        localeCode = QStringLiteral("zh");
      }
    } else if (!persistedLanguage.isEmpty()) {
      localeCode = persistedLanguage;
      decidedBy = QStringLiteral("saved setting");
    } else {
      const QString sysName = QLocale::system().name().toLower();
      if (sysName.startsWith(QStringLiteral("ja"))) {
        localeCode = QStringLiteral("ja");
      } else if (sysName.startsWith(QStringLiteral("zh_tw")) ||
                 sysName.startsWith(QStringLiteral("zh-hant"))) {
        localeCode = QStringLiteral("zh-TW");
      } else if (sysName.startsWith(QStringLiteral("zh"))) {
        localeCode = QStringLiteral("zh");
      } else if (sysName.startsWith(QStringLiteral("ko"))) {
        localeCode = QStringLiteral("ko");
      } else if (sysName.startsWith(QStringLiteral("fr"))) {
        localeCode = QStringLiteral("fr");
      } else if (sysName.startsWith(QStringLiteral("de"))) {
        localeCode = QStringLiteral("de");
      } else if (sysName.startsWith(QStringLiteral("es"))) {
        localeCode = QStringLiteral("es");
      } else if (sysName.startsWith(QStringLiteral("pt"))) {
        localeCode = QStringLiteral("pt");
      } else if (sysName.startsWith(QStringLiteral("ru"))) {
        localeCode = QStringLiteral("ru");
      } else if (sysName.startsWith(QStringLiteral("ar"))) {
        localeCode = QStringLiteral("ar");
      } else {
        localeCode = QStringLiteral("en");
      }
      decidedBy = QStringLiteral("system locale");
    }

    ArtifactCore::LocalizationManager::instance().setLanguageCode(localeCode);
    qInfo() << "[AppMain] Language decided:" << localeCode << "by" << decidedBy;
  }

  if (appArgs.contains(QStringLiteral("--renderer"))) {
    qputenv("ARTIFACT_RENDER_BACKEND",
            commandLine.global.rendererBackend.toUtf8());
    qInfo() << "[AppMain] Renderer backend selected via --renderer:"
            << commandLine.global.rendererBackend;
  }
  if (commandLine.global.safeMode) {
    qputenv("ARTIFACT_SAFE_MODE", "1");
  }
  if (commandLine.global.verbose) {
    qputenv("ARTIFACT_VERBOSE_LOG", "1");
  }
  if (!commandLine.global.logFile.isEmpty()) {
    qputenv("ARTIFACT_LOG_FILE", commandLine.global.logFile.toUtf8());
  }

  QApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);
  QApplication a(argc, argv);
  DialogLatencyEventFilter dialogLatencyFilter;
  a.installEventFilter(&dialogLatencyFilter);
  configureQtPaths();
  Artifact::WorkspaceAutomation::ensureRegistered();
  auto* launchOpenFilter = new LaunchOpenRequestFilter(&a);
  a.installEventFilter(launchOpenFilter);
  auto* accessibilityInputFilter = new AccessibilityInputEventFilter(&a);
  a.installEventFilter(accessibilityInputFilter);

  // ============================================================
  // 翻訳カタログのロード（言語は起動時に確定済み）
  // ============================================================
  {
    auto &loc = ArtifactCore::LocalizationManager::instance();
    const QString translationsDir =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("translations"));
    if (QDir(translationsDir).exists()) {
      if (!loc.loadFromDirectory(translationsDir)) {
        qWarning() << "[AppMain] Failed to load translations from" << translationsDir;
      }
      qInfo() << "[AppMain] Translations loaded, locale:" << loc.languageCode();
    } else {
      qWarning() << "[AppMain] Translations directory not found:" << translationsDir;
    }
  }

  qInfo() << "[AppMain] Validation diagnostics will be initialized on demand";

  if (qEnvironmentVariableIsSet("ARTIFACT_RUN_BUILTIN_TESTS")) {
    const int builtinTestFailures = Artifact::runAllTests();
    if (builtinTestFailures != 0) {
      return builtinTestFailures;
    }
  }

  if (qEnvironmentVariableIsSet("ARTIFACT_RUN_GPU_BLEND_TESTS")) {
    return Artifact::runGpuBlendTests();
  }

  // Initialize environment variable manager
  auto *envManager = ArtifactCore::EnvironmentVariableManager::instance();
  const QVariantMap applicationEnvironmentOverrides = QSettings()
      .value(QStringLiteral("EnvironmentVariables/ApplicationOverrides"))
      .toMap();
  for (auto it = applicationEnvironmentOverrides.cbegin();
       it != applicationEnvironmentOverrides.cend(); ++it) {
    envManager->setVariable(it.key(), it.value());
  }
  qDebug() << "[AppMain] Environment variables loaded:"
           << envManager->variableNames().size()
           << "application overrides:" << applicationEnvironmentOverrides.size();

  const bool verboseVideoLog =
      qEnvironmentVariableIsSet("ARTIFACT_VIDEO_VERBOSE_LOG");
  QLoggingCategory::setFilterRules(
      QStringLiteral("artifact.compositionview.debug=false\n"
                     "artifact.layer.video.debug=%1")
          .arg(verboseVideoLog ? QStringLiteral("true")
                               : QStringLiteral("false")));
  auto *settings = ArtifactCore::ArtifactAppSettings::instance();
  auto applyThemeFromSettings = [&a, settings]() {
    if (!settings) {
      return;
    }
    ArtifactCore::DccStyleTheme theme = ArtifactCore::getDCCTheme(
        ArtifactCore::themePresetFromName(settings->themeName()));
    const QString themePresetPath = settings->themePresetPath().trimmed();
    if (!themePresetPath.isEmpty()) {
      QString loadError;
      ArtifactCore::DccStyleTheme loadedTheme;
      if (ArtifactCore::loadDCCThemePresetFromFile(themePresetPath,
                                                   &loadedTheme, &loadError)) {
        theme = loadedTheme;
      } else {
        qWarning() << "[AppMain] Failed to load theme preset:"
                   << themePresetPath << loadError;
      }
    }
    const QColor accentOverride(settings->uiAccentColor());
    if (accentOverride.isValid()) {
      theme.accentColor = accentOverride.name();
      const QColor background(theme.backgroundColor);
      theme.selectionColor = QColor::fromRgbF(
          background.redF() * 0.70 + accentOverride.redF() * 0.30,
          background.greenF() * 0.70 + accentOverride.greenF() * 0.30,
          background.blueF() * 0.70 + accentOverride.blueF() * 0.30)
                                 .name();
      // 昇格 token 派生: focus ring は accent 追従 (dcc-comparison方針:
      // focus/selection のみ accent 派生、Warning/Danger/Success・XYZ軸・
      // チャンネル色には波及させない)。buttonInfo/Success・sliderHandle・
      // disabled・muted系は派生対象外で、外部JSON/プリセット値を維持。
      // QColor 無効時は getDCCTheme() の fallback が効くためここでは上書きしない。
      const QColor curFocus(theme.focusRingColor);
      if (!curFocus.isValid() || curFocus.name().compare(
              QStringLiteral("#8FBAFF"), Qt::CaseInsensitive) == 0 ||
          curFocus.name().compare(
              QStringLiteral("#0078D7"), Qt::CaseInsensitive) == 0) {
        // 既定値のまま = ユーザー未指定 → accent から淡色派生
        QColor ring = accentOverride;
        // dark/light 両対応: 白方向へ寄せて視認性確保
        const qreal lum = 0.2126 * accentOverride.redF() +
                          0.7152 * accentOverride.greenF() +
                          0.0722 * accentOverride.blueF();
        ring = lum > 0.5 ? accentOverride.lighter(130) : accentOverride.lighter(150);
        theme.focusRingColor = ring.name();
      }
    }
    QFont applicationFont(settings->defaultFontFamily());
    applicationFont.setPointSizeF(
        static_cast<qreal>(settings->uiFontPointSize()) *
        Artifact::Accessibility::fontScale());
    a.setFont(applicationFont);
    ArtifactCore::applyDCCTheme(a, theme);
    if (auto *style = a.style()) {
      style->polish(&a);
      const auto widgets = QApplication::allWidgets();
      for (QWidget *widget : widgets) {
        if (!widget) {
          continue;
        }
        style->unpolish(widget);
        style->polish(widget);
        widget->update();

        const QString className =
            QString::fromLatin1(widget->metaObject()->className());
        const QColor clear(theme.backgroundColor);
        if (className ==
            QStringLiteral("Artifact::ArtifactCompositionEditor")) {
          auto *compositionEditor =
              static_cast<ArtifactCompositionEditor *>(widget);
          compositionEditor->setClearColor(
              {clear.redF(), clear.greenF(), clear.blueF(), 1.0f});
        } else if (className ==
                   QStringLiteral("Artifact::ArtifactRenderLayerEditor")) {
          auto *layerEditor = static_cast<ArtifactRenderLayerEditor *>(widget);
          if (auto *view = layerEditor->view()) {
            view->setClearColor(
                {clear.redF(), clear.greenF(), clear.blueF(), 1.0f});
          }
        }
      }
    }
  };
  auto applyPreviewPresetFromSettings = [settings]() {
    const QString quality = settings ? settings->previewQualityText().trimmed().toLower()
                                     : QString();
    if (quality.contains(QStringLiteral("draft")) ||
        quality.contains(QStringLiteral("fast"))) {
      return PreviewQualityPreset::Draft;
    }
    if (quality.contains(QStringLiteral("final"))) {
      return PreviewQualityPreset::Final;
    }
    return PreviewQualityPreset::Preview;
  };
  auto workspaceModeFromSettings = [settings]() {
    return Artifact::workspaceModeInfoForText(
               settings ? settings->projectDefaultWorkspaceModeText()
                        : QString())
        .mode;
  };
  applyThemeFromSettings();
  QApplication::setStyle(
      new ArtifactCommonStyle());

  {
    const auto& aiSettings = ArtifactCore::LayeredConfigStore::instance();
    const bool autoInitialize =
        aiSettings.value(QStringLiteral("AI/AutoInitialize"), false).toBool();
    const QString provider =
        aiSettings.value(QStringLiteral("AI/Provider"), QStringLiteral("local"))
            .toString()
            .trimmed();
    QString modelPath =
        aiSettings.value(QStringLiteral("AI/ModelPath")).toString().trimmed();
    if (modelPath.isEmpty()) {
      const QString normalizedProvider = provider.toLower();
      if (normalizedProvider == QStringLiteral("onnx") ||
          normalizedProvider == QStringLiteral("onnx-dml") ||
          normalizedProvider == QStringLiteral("onnxdml") ||
          normalizedProvider == QStringLiteral("directml")) {
        modelPath = QStringLiteral("models/onnx/model.onnx");
      } else {
        modelPath = QStringLiteral("models/llama-3.2-1b-instruct.q4_k_m.gguf");
      }
    }
    if (autoInitialize) {
      const QString normalizedProvider = provider.toLower();
      if (!provider.isEmpty() &&
          normalizedProvider != QStringLiteral("local") &&
          normalizedProvider != QStringLiteral("llama") &&
          normalizedProvider != QStringLiteral("onnx-dml") &&
          normalizedProvider != QStringLiteral("onnx") &&
          normalizedProvider != QStringLiteral("onnxdml") &&
          normalizedProvider != QStringLiteral("directml")) {
        qWarning()
            << "[AppMain] Auto AI initialization skipped: unsupported provider:"
            << provider;
      } else if (modelPath.isEmpty()) {
        qWarning()
            << "[AppMain] Auto AI initialization skipped: model path is empty";
      } else if (!QFileInfo::exists(modelPath)) {
        qWarning()
            << "[AppMain] Auto AI initialization skipped: model not found:"
            << modelPath;
      } else {
        auto *client = AIClient::instance();
        if (client->isInitializing()) {
          qInfo() << "[AppMain] Auto AI initialization skipped: backend is "
                     "already loading";
        } else {
          std::thread([client, modelPath]() {
            if (!client->initialize(modelPath)) {
              if (client->isInitializing()) {
                qInfo() << "[AppMain] Auto AI initialization is already in "
                           "progress:"
                        << modelPath;
              } else {
                qWarning()
                    << "[AppMain] Auto AI initialization failed for model:"
                    << modelPath;
              }
            } else {
              qDebug() << "[AppMain] Auto AI initialized from settings:"
                       << modelPath;
            }
          }).detach();
        }
      }
    }
  }

  // QThreadPool globalInstance prewarm is no longer needed: TBB/TaskSystem
  // lazily creates its arena. Keep the configured count for the legacy pool
  // for any remaining QtConcurrent users, but pre-warm the TaskSystem.
  {
    auto qPool = QThreadPool::globalInstance();
    const int configuredRenderThreads =
        commandLine.global.renderThreads > 0
            ? commandLine.global.renderThreads
            : std::max(1, settings ? settings->renderThreadCount() : 10);
    qPool->setMaxThreadCount(configuredRenderThreads);
    for (int i = 0; i < std::min(2, configuredRenderThreads); ++i) {
      ArtifactCore::TaskSystem::globalInstance().silent_async([] {});
    }
  }

  bootstrapPythonScripts();
  ArtifactPythonHookManager::runHook(QStringLiteral("on_startup"));
  const bool hadUncleanExit = markSessionStartAndDetectUncleanExit();
  const QIcon appIcon = buildTemporaryAppIcon();
  QApplication::setWindowIcon(appIcon);
  using namespace Artifact;
  auto *mw = new ArtifactMainWindow();
  QPointer<ArtifactMainWindow> mainWindowGuard(mw);
  QPointer<CollaborationDockController> collaborationController;
  initializeProjectBundleIpc(mw);
  ArtifactWorkspaceManager workspaceManager;
  mw->setObjectName("ArtifactMainWindow");
  mw->setWindowTitle(buildWindowTitle());
  mw->setWindowIcon(appIcon);
  auto *status = new ArtifactStatusBar(mw);
  mw->setStatusBar(status);
  status->showReadyMessage();
  status->setProjectText("Loaded");
  mw->applyApplicationSettings();
  auto *projectService = ArtifactProjectService::instance();
  if (projectService) {
    projectService->setPreviewQualityPreset(applyPreviewPresetFromSettings());
  }
  launchOpenFilter->setProjectOpenHandler(
      [mw](const QString& filePath) {
        if (!mw || filePath.isEmpty()) {
          return;
        }
        QTimer::singleShot(0, mw, [filePath]() {
          if (!QFileInfo(filePath).isFile()) {
            qWarning() << "[AppMain] Launch project file no longer exists:"
                       << filePath;
            return;
          }
          qInfo() << "[AppMain] Opening launch project file:" << filePath;
          ArtifactProjectManager::getInstance().loadFromFileAsync(
              filePath,
              [filePath](const ArtifactProjectImporterResult& result) {
                if (!result.success) {
                  qWarning() << "[AppMain] Failed to open launch project:"
                             << filePath << result.errorMessage.toQString();
                }
              });
        });
      });
  for (const QString& filePath : launchProjectPaths) {
    launchOpenFilter->enqueueLaunchPath(filePath);
  }
  auto *playbackService = ArtifactPlaybackService::instance();
  if (playbackService && settings) {
    playbackService->setRamPreviewEnabled(settings->previewEnableRamCache());
    playbackService->setDiskPreviewCacheEnabled(
        settings->previewEnableDiskCache());
    playbackService->setDiskPreviewCacheBudgetMB(settings->previewCacheSizeMB());
  }
  auto *playbackShortcuts = new Artifact::ArtifactPlaybackShortcuts(mw);
  if (playbackService && playbackService->controller()) {
    playbackShortcuts->setup(playbackService->controller(), nullptr);
  }
  // Enable output monitoring for debugging
  if (playbackService->controller()) {
    playbackService->controller()->enableOutputMonitoring(true);
    QElapsedTimer outputMonitorAlertCooldown;
    playbackService->controller()->setOutputMonitorCallback(
        [mw, outputMonitorAlertCooldown](bool audioOk, bool videoOk,
                                         const QString &context) mutable {
          if (!audioOk || !videoOk) {
            if (outputMonitorAlertCooldown.isValid() &&
                !outputMonitorAlertCooldown.hasExpired(2000)) {
              return;
            }
            outputMonitorAlertCooldown.restart();
            auto *aiWidget = mw->aiCloudWidget();
            if (aiWidget) {
              QString prompt =
                  QString("プレビュー再生中に問題が発生しました。音: %1, 映像: "
                          "%2, コンテキスト: %3。原因を分析してください。")
                      .arg(audioOk ? "OK" : "NG")
                      .arg(videoOk ? "OK" : "NG")
                      .arg(context);
              aiWidget->startChatRequest(
                  prompt,
                  "あなたはAfter "
                  "Effectsのような動画編集アプリのデバッグアシスタントです。");
            }
          }
        });
  }
  auto *autoSaveManager = new ArtifactAutoSaveManager();
  QPointer<ArtifactRenderCenterWindow> renderCenterWindow;
  QPointer<ArtifactDebugConsoleWidget> debugConsoleWidget;
  QPointer<FrameDebugViewWidget> frameDebugWidget;
  QPointer<DebugRenderHarnessWidget> debugHarnessWidget;
  ArtifactCore::SharedPtr<ArtifactCore::PreciseTicker> frameDebugTimer;
  const QString recoveryDir =
      QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
          .filePath("Recovery");
  const QString sessionLedgerPath =
      QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
          .filePath(QStringLiteral("Session/session-ledger.json"));
  QDir(QFileInfo(sessionLedgerPath).absolutePath()).mkpath(QStringLiteral("."));
  if (auto *renderQueueService = ArtifactRenderQueueService::instance()) {
    if (!renderQueueService->sessionLedger().loadFromFile(sessionLedgerPath)) {
      qInfo() << "[SessionLedger] no previous ledger loaded or file is invalid"
              << sessionLedgerPath;
    }
  }
  // Create the composition editor synchronously so its native HWND exists when
  // mw->show() fires. Deferring this inside singleShot(0) caused the widget to
  // miss its showEvent and never initialize the Diligent renderer.
  QElapsedTimer compositionEditorTotalTimer;
  compositionEditorTotalTimer.start();
  QElapsedTimer compositionEditorFactoryTimer;
  compositionEditorFactoryTimer.start();
  auto *compositionEditor = new ArtifactCompositionEditor(mw);
  const double compositionEditorFactoryMs =
      static_cast<double>(compositionEditorFactoryTimer.nsecsElapsed()) /
      1000000.0;
  compositionEditor->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Expanding);
  compositionEditor->setMinimumSize(720, 420);
  suppressScrollBarsForViewportWidget(compositionEditor);
  mw->setCentralWorkspace(QStringLiteral("Composition Viewer"),
                          compositionEditor);
  WidgetCreationDiagnostics::record(
      compositionEditor, QStringLiteral("Composition Viewer"),
      QStringLiteral("eager-central-workspace"),
      QStringLiteral("startup-required-native-viewport"),
      compositionEditorFactoryMs,
      static_cast<double>(compositionEditorTotalTimer.nsecsElapsed()) /
          1000000.0,
      QStringLiteral("created"), QStringLiteral("Composition Viewer"),
      QStringLiteral("Composition Viewer"));

  // --- Performance HUD (C4D-style viewport overlay) ---
  auto* perfHUD = WidgetCreationDiagnostics::createMeasured(
      QStringLiteral("Performance HUD"), QStringLiteral("eager-widget"),
      QStringLiteral("startup-default-viewport-overlay"),
      [mw]() { return new ArtifactPerformanceHUD(mw); });
  perfHUD->setController(compositionEditor->renderController());
  perfHUD->setEnabled(true);


  const QRect playbackControlFloatingGeometry(120, 828, 720, 210);
  mw->addLazyDockedWidgetFloating(
      QStringLiteral("Playback Control"), QStringLiteral("PlaybackControl"),
      [mw]() -> QWidget * { return new ArtifactPlaybackControlWidget(mw); },
      playbackControlFloatingGeometry);
  mw->addLazyDockedWidgetFloating(
      QStringLiteral("Playback Control Test"),
      QStringLiteral("PlaybackControlTest"),
      [mw]() -> QWidget * {
        return new ArtifactPlaybackControlTestWidget(mw);
      },
      QRect(860, 828, 720, 210));


  // --- Coordinate Manager (C4D-style) ---
  const QRect coordManagerFloatingGeometry(120, 806, 380, 30);
  auto* coordManager = WidgetCreationDiagnostics::createMeasured(
      QStringLiteral("Coordinate Manager"), QStringLiteral("eager-widget"),
      QStringLiteral("startup-default-floating-tool"),
      [mw]() { return new ArtifactCoordinateManagerWidget(mw); });
  mw->addDockedWidgetFloating(
      QStringLiteral("Coordinate Manager"), QStringLiteral("CoordinateManager"),
      coordManager, coordManagerFloatingGeometry);
  mw->setDockVisible(QStringLiteral("Coordinate Manager"), true);

  QTimer::singleShot(
      0, mw,
      [=, &renderCenterWindow, &debugConsoleWidget,
       &frameDebugWidget, &debugHarnessWidget, &frameDebugTimer]() {
    mw->addLazyDockedWidgetFloating(
        QStringLiteral("Debug Console"), QStringLiteral("DebugConsole"),
        [mw, compositionEditor, &debugConsoleWidget]() mutable -> QWidget* {
          auto* widget = new ArtifactDebugConsoleWidget(mw);
          debugConsoleWidget = widget;
          if (compositionEditor) {
            if (auto* controller = compositionEditor->renderController()) {
              widget->setFrameDebugSnapshot(controller->frameDebugSnapshot());
            }
          }
          return widget;
        },
        QRect(200, 200, 800, 400));
    mw->addLazyDockedWidgetFloating(
        QStringLiteral("Frame Debug"), QStringLiteral("FrameDebug"),
        [mw, compositionEditor, &frameDebugWidget]() mutable -> QWidget* {
          auto* widget = new FrameDebugViewWidget(mw);
          frameDebugWidget = widget;
          if (compositionEditor) {
            if (auto* controller = compositionEditor->renderController()) {
              widget->setFrameDebugSnapshot(controller->frameDebugSnapshot());
            }
          }
          return widget;
        },
        QRect(220, 240, 900, 520));
    mw->addLazyDockedWidgetFloating(
        QStringLiteral("App Debugger"), QStringLiteral("AppDebugger"),
        [mw, compositionEditor]() -> QWidget* {
          auto* controller =
              compositionEditor ? compositionEditor->renderController() : nullptr;
          return new AppDebuggerWidget(controller, mw);
        },
        QRect(140, 140, 1080, 640));
    mw->setDockVisible(QStringLiteral("App Debugger"), false);
    mw->addLazyDockedWidgetFloating(
        QStringLiteral("Debug Render Harness"),
        QStringLiteral("DebugRenderHarness"),
        [mw, compositionEditor, &debugHarnessWidget]() mutable -> QWidget* {
          auto* widget = new DebugRenderHarnessWidget(mw);
          debugHarnessWidget = widget;
          widget->setScenePreset(QStringLiteral("mixed-media"));
          if (compositionEditor) {
            if (auto* controller = compositionEditor->renderController()) {
              widget->setFrameDebugSnapshot(controller->frameDebugSnapshot());
            }
          }
          return widget;
        },
        QRect(180, 180, 1100, 660));
    auto playbackDebugReporter = ArtifactCore::makeShared<PlaybackDebugAutoReporter>();
    auto refreshFrameDebugWidgets = [compositionEditor, playbackService,
                                     playbackDebugReporter, &debugConsoleWidget,
                                     &frameDebugWidget, &debugHarnessWidget]() mutable {
      if (!compositionEditor) {
        return;
      }
      const bool debugSurfaceVisible =
          (debugConsoleWidget && debugConsoleWidget->isVisible()) ||
          (frameDebugWidget && frameDebugWidget->isVisible()) ||
          (debugHarnessWidget && debugHarnessWidget->isVisible());
      auto* controller = compositionEditor->renderController();
      if (!controller) {
        return;
      }
      const bool playbackActive = playbackService && playbackService->isPlaying();
      if (!debugSurfaceVisible && !playbackActive &&
          !playbackDebugReporter->isCapturing()) {
        return;
      }
      const auto snapshot = controller->frameDebugSnapshot();
      playbackDebugReporter->observe(playbackActive, snapshot);
      if (!debugSurfaceVisible) {
        return;
      }
      if (debugConsoleWidget) {
        debugConsoleWidget->setFrameDebugSnapshot(snapshot);
      }
      if (frameDebugWidget) {
        frameDebugWidget->setFrameDebugSnapshot(snapshot);
      }
      if (debugHarnessWidget) {
        debugHarnessWidget->setFrameDebugSnapshot(snapshot);
      }
    };
    frameDebugTimer = ArtifactCore::makeShared<ArtifactCore::PreciseTicker>();
    frameDebugTimer->setInterval(std::chrono::milliseconds(750));
    frameDebugTimer->setCallback([mw, refreshFrameDebugWidgets]() {
      QMetaObject::invokeMethod(
          mw,
          [refreshFrameDebugWidgets]() mutable { refreshFrameDebugWidgets(); },
          Qt::QueuedConnection);
    });
    frameDebugTimer->start();

    // Update StatusBar console summary
    auto updateStatusConsole = [mainWindowGuard]() {
      if (!mainWindowGuard)
        return;
      auto *status = dynamic_cast<ArtifactStatusBar *>(
          mainWindowGuard->findChild<QStatusBar *>());
      if (!status) {
        return;
      }
      auto logs = Logger::instance()->getLogs();
      int errors = 0;
      int warnings = 0;
      for (const auto &log : logs) {
        if (log.level == LogLevel::Warning)
          warnings++;
        else if (log.level == LogLevel::Error || log.level == LogLevel::Fatal)
          errors++;
      }
      // status->setConsoleSummary(errors, warnings); // We can use this if the
      // previous edit definitely worked If setConsoleSummary failed to compile,
      // we can manually set text for now to avoid block
      status->setProjectText(
          QString("Logs: %1E %2W").arg(errors).arg(warnings));
    };
    auto queueStatusConsoleUpdate = [mainWindowGuard,
                                     updateStatusConsole]() {
      if (!mainWindowGuard) {
        return;
      }
      const auto dispatch = [updateStatusConsole]() {
        updateStatusConsole();
      };
      if (QThread::currentThread() == mainWindowGuard->thread()) {
        dispatch();
      } else {
        QMetaObject::invokeMethod(mainWindowGuard.data(), dispatch,
                                  Qt::QueuedConnection);
      }
    };
    mw->addLazyDockedWidgetTabbedWithId(
        QStringLiteral("Composition View (Software)"),
        QStringLiteral("Composition View (Software)"),
        DockArea::Center,
        [mw]() -> QWidget * {
          return new ArtifactSoftwareCompositionTestWidget(mw);
        },
        QStringLiteral("Composition Viewer"));
    auto *layerViewEditor = WidgetCreationDiagnostics::createMeasured(
        QStringLiteral("Layer Solo View"), QStringLiteral("eager-widget"),
        QStringLiteral("startup-default-layer-solo-surface"),
        [mw]() { return new ArtifactRenderLayerEditor(mw); });
    layerViewEditor->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Expanding);
    suppressScrollBarsForViewportWidget(layerViewEditor);
    mw->addDockedWidgetTabbed(QStringLiteral("Layer Solo View"),
                              DockArea::Center, layerViewEditor,
                              QStringLiteral("Composition Viewer"));
    mw->addLazyDockedWidgetTabbedWithId(
        QStringLiteral("Layer View (Software)"),
        QStringLiteral("Layer View (Software)"), DockArea::Center,
        [mw]() -> QWidget * {
          return new ArtifactSoftwareLayerTestWidget(mw);
        },
        QStringLiteral("Layer Solo View"));
    auto *projectManagerWidget = WidgetCreationDiagnostics::createMeasured(
        QStringLiteral("Project"), QStringLiteral("eager-widget"),
        QStringLiteral("startup-default-workspace"),
        [mw]() { return new ArtifactProjectManagerWidget(mw); });
    projectManagerWidget->setMinimumWidth(240);
    mw->addDockedWidget(QStringLiteral("Project"), DockArea::Left,
                        projectManagerWidget);
    auto *compositionGraphWidget = WidgetCreationDiagnostics::createMeasured(
        QStringLiteral("Composition Graph"), QStringLiteral("eager-widget"),
        QStringLiteral("startup-registered-left-tool"),
        [mw]() { return new ArtifactCompositionGraphWidget(mw); });
    mw->addDockedWidgetTabbed(QStringLiteral("Composition Graph"),
                              DockArea::Left, compositionGraphWidget,
                              QStringLiteral("Project"));
    auto *shaderGraphWidget = WidgetCreationDiagnostics::createMeasured(
        QStringLiteral("Shader Graph"), QStringLiteral("eager-widget"),
        QStringLiteral("startup-registered-left-tool"),
        [mw]() { return new ArtifactShaderGraphWidget(mw); });
    mw->addDockedWidgetTabbed(QStringLiteral("Shader Graph"),
                              DockArea::Left, shaderGraphWidget,
                              QStringLiteral("Composition Graph"));
    auto *assetBrowser = WidgetCreationDiagnostics::createMeasured(
        QStringLiteral("Asset Browser"), QStringLiteral("eager-widget"),
        QStringLiteral("startup-default-workspace"),
        [mw]() { return new ArtifactAssetBrowser(mw); });
    mw->addDockedWidgetTabbed(QStringLiteral("Asset Browser"),
                              DockArea::Left, assetBrowser,
                              QStringLiteral("Project"));
    auto *clipBufferWidget = WidgetCreationDiagnostics::createMeasured(
        QStringLiteral("Clip Buffer"), QStringLiteral("eager-widget"),
        QStringLiteral("startup-registered-left-tool"),
        [mw]() { return new ArtifactClipBufferWidget(mw); });
    mw->addDockedWidgetTabbed(QStringLiteral("Clip Buffer"),
                              DockArea::Left, clipBufferWidget,
                              QStringLiteral("Project"));
    auto *shortcutHelperWidget = WidgetCreationDiagnostics::createMeasured(
        QStringLiteral("Shortcut Helper"), QStringLiteral("eager-widget"),
        QStringLiteral("startup-registered-left-tool"),
        [mw]() { return new ArtifactContextShortcutHelperWidget(mw); });
    mw->addDockedWidgetTabbed(QStringLiteral("Shortcut Helper"),
                              DockArea::Left, shortcutHelperWidget,
                              QStringLiteral("Project"));
    auto *contentsViewer = WidgetCreationDiagnostics::createMeasured(
        QStringLiteral("Contents Viewer"), QStringLiteral("eager-widget"),
        QStringLiteral("startup-registered-file-preview"),
        [mw]() { return new ArtifactContentsViewer(mw); });
    QPointer<ArtifactProjectManagerWidget> projectManagerWidgetGuard(
        projectManagerWidget);
    QPointer<ArtifactAssetBrowser> assetBrowserGuard(assetBrowser);
    QPointer<ArtifactContentsViewer> contentsViewerGuard(contentsViewer);
    mw->addDockedWidgetTabbed(QStringLiteral("Contents Viewer"),
                              DockArea::Left, contentsViewer,
                              QStringLiteral("Project"));
    mw->setDockVisible(QStringLiteral("Contents Viewer"), false);
    mw->addLazyDockedWidgetTabbedWithId(
        QStringLiteral("History Timeline"), QStringLiteral("History Timeline"),
        DockArea::Bottom,
        [mw]() -> QWidget* {
          auto* panel = new ArtifactHistoryTimelineWidget(mw);
          panel->setMinimumHeight(260);
          return panel;
        },
        QStringLiteral("timeline::"));
    mw->addLazyDockedWidgetTabbedWithId(
        QStringLiteral("Cache / Memory Map"), QStringLiteral("Cache / Memory Map"),
        DockArea::Bottom,
        [mw]() -> QWidget* {
          auto* panel = new ArtifactCacheMemoryMapWidget(mw);
          panel->setMinimumHeight(420);
          return panel;
        },
        QStringLiteral("cache-memory-map::"));
    static ArtifactCore::EventBus appEventBus = ArtifactCore::globalEventBus();
    static std::vector<ArtifactCore::EventBus::Subscription>
        appEventSubscriptions;
    appEventSubscriptions.push_back(
        appEventBus.subscribe<ArtifactCore::LogAddedEvent>(
            [queueStatusConsoleUpdate](const ArtifactCore::LogAddedEvent &) {
              queueStatusConsoleUpdate();
            }));
    appEventSubscriptions.push_back(
        appEventBus.subscribe<ArtifactCore::LogsClearedEvent>(
            [queueStatusConsoleUpdate](const ArtifactCore::LogsClearedEvent &) {
              queueStatusConsoleUpdate();
            }));
    appEventSubscriptions.push_back(
        appEventBus.subscribe<ArtifactCore::AppSettingsChangedEvent>(
            [mainWindowGuard, applyThemeFromSettings,
             applyPreviewPresetFromSettings](
                const ArtifactCore::AppSettingsChangedEvent &) {
              applyThemeFromSettings();
              if (mainWindowGuard) {
                mainWindowGuard->applyApplicationSettings();
              }
              if (auto *service = ArtifactProjectService::instance()) {
                service->setPreviewQualityPreset(
                    applyPreviewPresetFromSettings());
              }
              if (auto *currentSettings =
                      ArtifactCore::ArtifactAppSettings::instance()) {
                const int configuredRenderThreads =
                    std::max(1, currentSettings->renderThreadCount());
                QThreadPool::globalInstance()->setMaxThreadCount(
                    configuredRenderThreads);
              }
            }));
    if (playbackService && settings) {
      appEventSubscriptions.push_back(
          appEventBus.subscribe<ArtifactCore::AppSettingsChangedEvent>(
              [](const ArtifactCore::AppSettingsChangedEvent &) {
                auto *currentSettings =
                    ArtifactCore::ArtifactAppSettings::instance();
                auto *service = ArtifactPlaybackService::instance();
                if (!currentSettings || !service) {
                  return;
                }
                service->setRamPreviewEnabled(
                    currentSettings->previewEnableRamCache());
                service->setDiskPreviewCacheEnabled(
                    currentSettings->previewEnableDiskCache());
                service->setDiskPreviewCacheBudgetMB(
                    currentSettings->previewCacheSizeMB());
              }));
    }
    auto selectionSyncGuard = ArtifactCore::makeShared<bool>(false);
    appEventSubscriptions.push_back(
        appEventBus.subscribe<SelectionChangedEvent>(
            [mainWindowGuard, assetBrowserGuard, contentsViewerGuard](
                const SelectionChangedEvent& event) {
              if (!mainWindowGuard) {
                return;
              }
              if (!event.currentCompositionId.isEmpty()) {
                const CompositionID compositionId(event.currentCompositionId);
                if (compositionId.isNil()) {
                  return;
                }
                if (auto* service = ArtifactProjectService::instance()) {
                  service->changeCurrentComposition(compositionId);
                }
                return;
              }
              if (!contentsViewerGuard || event.currentFootagePath.isEmpty() ||
                  !QFileInfo(event.currentFootagePath).isFile()) {
                return;
              }
              if (assetBrowserGuard) {
                assetBrowserGuard->selectAssetPaths(
                    QStringList{event.currentFootagePath});
              }
              contentsViewerGuard->setFilePath(event.currentFootagePath);
              mainWindowGuard->setDockVisible(
                  QStringLiteral("Contents Viewer"), true);
              mainWindowGuard->activateDock(
                  QStringLiteral("Contents Viewer"));
            }));
    appEventSubscriptions.push_back(
        appEventBus.subscribe<SelectionChangedEvent>(
            [assetBrowserGuard, selectionSyncGuard](
                const SelectionChangedEvent& event) {
              if (!assetBrowserGuard || !selectionSyncGuard ||
                  *selectionSyncGuard || event.selectedFootagePaths.isEmpty()) {
                return;
              }
              *selectionSyncGuard = true;
              assetBrowserGuard->selectAssetPaths(event.selectedFootagePaths);
              *selectionSyncGuard = false;
            }));
    appEventSubscriptions.push_back(
        appEventBus.subscribe<ProjectItemActivatedEvent>(
            [mainWindowGuard, assetBrowserGuard, contentsViewerGuard](
                const ProjectItemActivatedEvent& event) {
              if (!mainWindowGuard) {
                return;
              }
              if (event.kind == ProjectItemActivationKind::Composition) {
                const CompositionID compositionId(event.compositionId);
                if (compositionId.isNil()) {
                  return;
                }
                if (auto* service = ArtifactProjectService::instance()) {
                  service->changeCurrentComposition(compositionId);
                }
                mainWindowGuard->setDockVisible(
                    QStringLiteral("Composition Viewer"), true);
                mainWindowGuard->activateDock(
                    QStringLiteral("Composition Viewer"));
                return;
              }
              if (event.kind != ProjectItemActivationKind::Footage ||
                  !contentsViewerGuard || event.filePath.isEmpty() ||
                  !QFileInfo(event.filePath).isFile()) {
                return;
              }
              if (assetBrowserGuard) {
                assetBrowserGuard->selectAssetPaths(
                    QStringList{event.filePath});
              }
              contentsViewerGuard->setFilePath(event.filePath);
              mainWindowGuard->setDockVisible(
                  QStringLiteral("Contents Viewer"), true);
              mainWindowGuard->activateDock(
                  QStringLiteral("Contents Viewer"));
            }));
    appEventSubscriptions.push_back(
        appEventBus.subscribe<AssetBrowserItemDoubleClickedEvent>(
            [mainWindowGuard, contentsViewerGuard](
                const AssetBrowserItemDoubleClickedEvent& event) {
              if (!contentsViewerGuard || event.itemPath.isEmpty() ||
                  !QFileInfo(event.itemPath).isFile() || !mainWindowGuard) {
                return;
              }
              contentsViewerGuard->setFilePath(event.itemPath);
              mainWindowGuard->setDockVisible(
                  QStringLiteral("Contents Viewer"), true);
              mainWindowGuard->activateDock(
                  QStringLiteral("Contents Viewer"));
            }));
    appEventSubscriptions.push_back(
        appEventBus.subscribe<AssetBrowserSelectionChangedEvent>(
            [projectManagerWidgetGuard, contentsViewerGuard,
             selectionSyncGuard](const AssetBrowserSelectionChangedEvent& event) {
              if (projectManagerWidgetGuard && selectionSyncGuard &&
                  !*selectionSyncGuard) {
                *selectionSyncGuard = true;
                projectManagerWidgetGuard->selectItemsByFilePaths(
                    event.selectedFiles);
                *selectionSyncGuard = false;
              }
              // Selection updates the Viewer source without opening or playing
              // it. Double-click remains the explicit open action.
              if (contentsViewerGuard && event.selectedFiles.size() == 1 &&
                  QFileInfo(event.selectedFiles.front()).isFile()) {
                contentsViewerGuard->setFilePath(event.selectedFiles.front());
              }
            }));
    appEventSubscriptions.push_back(
        appEventBus.subscribe<CurrentCompositionChangedEvent>(
            [projectManagerWidget](
                const CurrentCompositionChangedEvent &event) {
              const CompositionID compId(event.compositionId);
              auto *projectView = projectManagerWidget
                                      ? projectManagerWidget->projectView()
                                      : nullptr;
              if (!projectView || compId.isNil() || !projectView->model()) {
                return;
              }
              auto *model = projectView->model();
              const int rowCount = model->rowCount();
              for (int row = 0; row < rowCount; ++row) {
                const QModelIndex index = model->index(row, 0);
                if (!index.isValid()) {
                  continue;
                }
                const QModelIndex sourceIdx =
                    qobject_cast<const QSortFilterProxyModel *>(index.model())
                        ? qobject_cast<const QSortFilterProxyModel *>(
                              index.model())
                              ->mapToSource(index)
                        : index;
                const QVariant typeVar = sourceIdx.data(
                    Qt::UserRole +
                    static_cast<int>(
                        Artifact::ProjectItemDataRole::ProjectItemType));
                if (!typeVar.isValid() ||
                    typeVar.toInt() !=
                        static_cast<int>(eProjectItemType::Composition)) {
                  continue;
                }
                const QVariant idVar = sourceIdx.data(
                    Qt::UserRole +
                    static_cast<int>(
                        Artifact::ProjectItemDataRole::CompositionId));
                if (!idVar.isValid()) {
                  continue;
                }
                if (CompositionID(idVar.toString()) == compId) {
                  projectView->setCurrentIndex(index);
                  projectView->ensureIndexVisible(index);
                  return;
                }
              }
            }));
    appEventSubscriptions.push_back(
        appEventBus.subscribe<ClipPasteRequestedEvent>(
            [](const ClipPasteRequestedEvent& event) {
        const QVariant& data = event.data;
        if (!data.isValid()) return;
        const QJsonArray layersArray = data.toJsonArray();
        if (layersArray.isEmpty()) return;
        auto *svc = ArtifactProjectService::instance();
        if (!svc) return;
        auto comp = svc->currentComposition().lock();
        if (!comp) return;
        auto *selectionManager = ArtifactLayerSelectionManager::instance();
        QStringList beforeSelectionIds;
        QString beforeCurrentSelectionId;
        if (selectionManager) {
          for (const auto &selectedLayer : selectionManager->selectedLayers()) {
            if (selectedLayer) {
              beforeSelectionIds.push_back(selectedLayer->id().toQString());
            }
          }
          if (const auto currentLayer = selectionManager->currentLayer()) {
            beforeCurrentSelectionId = currentLayer->id().toQString();
          }
        }
        QHash<QString, LayerID> pastedLayerIdMap;
        std::vector<ArtifactAbstractLayerPtr> pastedLayers;
        for (const auto &layerVal : layersArray) {
            if (!layerVal.isObject()) continue;
            const QJsonObject sourceLayerObject = layerVal.toObject();
            const QString sourceLayerId =
                sourceLayerObject.value(QStringLiteral("id")).toString().trimmed();
            QJsonObject layerObj = sourceLayerObject;
            // Pasting creates a new layer; preserve source references but never
            // reuse the copied layer's own composition ID.
            layerObj.remove(QStringLiteral("id"));
            auto layer = ArtifactLayerFactory::createFromJson(layerObj);
            if (layer) {
                const auto appendResult = comp->appendLayerTop(layer);
                if (!appendResult.success) {
                  continue;
                }
                pastedLayers.push_back(layer);
                if (!sourceLayerId.isEmpty() && !LayerID(sourceLayerId).isNil()) {
                  pastedLayerIdMap.insert(sourceLayerId, layer->id());
                }
                if (auto *app = ArtifactApplicationManager::instance()) {
                  if (auto *selectionManager = app->layerSelectionManager()) {
                    selectionManager->selectLayer(layer);
                  }
                }
            }
        }
        if (pastedLayers.empty()) return;
        for (const auto &pastedLayer : pastedLayers) {
            if (!pastedLayer) continue;
            const auto parentIt = pastedLayerIdMap.constFind(
                pastedLayer->parentLayerId().toString());
            if (parentIt != pastedLayerIdMap.constEnd()) {
                pastedLayer->setParentById(parentIt.value());
            }
            if (auto *cloneLayer = dynamic_cast<ArtifactCloneLayer *>(
                    pastedLayer.get())) {
                auto cloneSettings = cloneLayer->cloneSettings();
                const auto sourceIt = pastedLayerIdMap.constFind(
                    cloneSettings.sourceLayerId.toString());
                if (sourceIt != pastedLayerIdMap.constEnd()) {
                    cloneSettings.sourceLayerId = sourceIt.value();
                    cloneLayer->setCloneSettings(cloneSettings);
                }
            }
            auto matteReferences = pastedLayer->matteReferences();
            bool matteReferencesChanged = false;
            for (auto &matteReference : matteReferences) {
                const auto sourceIt = pastedLayerIdMap.constFind(
                    matteReference.sourceLayerId.toString());
                if (sourceIt != pastedLayerIdMap.constEnd()) {
                    matteReference.sourceLayerId = sourceIt.value();
                    matteReferencesChanged = true;
                }
            }
            if (matteReferencesChanged) {
                pastedLayer->setMatteReferences(matteReferences);
            }
        }

        // The event path has already created the layers so that references can
        // be remapped. Repackage that mutation as one undoable transaction.
        struct PastedUndoEntry {
          ArtifactAbstractLayerPtr layer;
          int index = -1;
          std::unique_ptr<AddLayerCommand> addCommand;
        };
        std::vector<PastedUndoEntry> undoEntries;
        QStringList afterSelectionIds;
        for (const auto &pastedLayer : pastedLayers) {
          if (!pastedLayer) continue;
          const auto currentLayers = comp->allLayerRef();
          const int currentIndex = currentLayers.indexOf(pastedLayer);
          if (currentIndex < 0) continue;
          undoEntries.push_back(PastedUndoEntry{
              pastedLayer, currentIndex,
              std::make_unique<AddLayerCommand>(comp, pastedLayer, false)});
          afterSelectionIds.push_back(pastedLayer->id().toQString());
        }
        if (undoEntries.empty()) return;

        auto restorePastedLayers = [&]() {
          for (const auto &entry : undoEntries) {
            if (entry.layer && !comp->containsLayerById(entry.layer->id())) {
              comp->appendLayerTop(entry.layer);
            }
          }
          for (const auto &entry : undoEntries) {
            if (entry.layer && entry.index >= 0 &&
                comp->containsLayerById(entry.layer->id())) {
              comp->moveLayerToIndex(entry.layer->id(), entry.index);
            }
          }
        };

        auto transaction = std::make_unique<MacroUndoCommand>(
            QStringLiteral("Paste Layers"));
        for (auto &entry : undoEntries) {
          comp->removeLayer(entry.layer->id());
          transaction->addChild(std::move(entry.addCommand));
          transaction->addChild(std::make_unique<MoveLayerIndexCommand>(
              comp, entry.layer, 0, entry.index));
        }
        transaction->addChild(std::make_unique<LayerSelectionSnapshotCommand>(
            comp, beforeSelectionIds, beforeCurrentSelectionId,
            afterSelectionIds,
            afterSelectionIds.isEmpty() ? QString() : afterSelectionIds.back()));

        bool pushed = false;
        if (auto *manager = UndoManager::instance()) {
          pushed = manager->push(std::move(transaction));
        }
        if (!pushed) {
          restorePastedLayers();
        }
        if (pushed) {
          bool allPresent = true;
          for (const auto &entry : undoEntries) {
            allPresent = allPresent && entry.layer &&
                         comp->containsLayerById(entry.layer->id());
          }
          if (!allPresent) {
            if (auto *manager = UndoManager::instance()) manager->undo();
            restorePastedLayers();
          }
        }
        comp->changed();
    }));
    auto *inspectorWidget = WidgetCreationDiagnostics::createMeasured(
        QStringLiteral("Inspector"), QStringLiteral("eager-widget"),
        QStringLiteral("startup-default-right-editing-surface"),
        [mw]() { return new ArtifactInspectorWidget(mw); });
    inspectorWidget->setMinimumWidth(240);
    mw->addDockedWidget(QStringLiteral("Inspector"), DockArea::Right,
                        inspectorWidget);
    if (auto *trackerPanel = compositionEditor->trackerPanelWidget()) {
      mw->addDockedWidgetTabbed(QStringLiteral("Tracker"), DockArea::Right,
                                trackerPanel, QStringLiteral("Inspector"));
    }
    const auto detachInspectorTab = [](QWidget *panel) {
      if (!panel) return;
      for (QWidget *ancestor = panel->parentWidget(); ancestor;
           ancestor = ancestor->parentWidget()) {
        auto *sourceTabs = qobject_cast<QTabWidget *>(ancestor);
        if (!sourceTabs) continue;
        const int sourceIndex = sourceTabs->indexOf(panel);
        if (sourceIndex >= 0) sourceTabs->removeTab(sourceIndex);
        return;
      }
    };
    auto *componentsPanel = inspectorWidget->findChild<QWidget *>(
        QStringLiteral("inspectorComponentsSurface"));
    if (componentsPanel) {
      detachInspectorTab(componentsPanel);
      // Keep the editing responsibilities independent while grouping the
      // related surfaces in the Inspector's right-side tab area.
      mw->addDockedWidgetTabbed(QStringLiteral("Components"),
                                DockArea::Right, componentsPanel,
                                QStringLiteral("Inspector"));
    }
    auto *effectsPanel = inspectorWidget->findChild<QWidget *>(
        QStringLiteral("inspectorEffectsScrollArea"));
    if (effectsPanel) {
      detachInspectorTab(effectsPanel);
      effectsPanel->setMinimumWidth(280);
      // Effects is a peer editor, but belongs to the same right-side tab
      // group as Inspector so it is immediately discoverable alongside the
      // current-layer editing surfaces.
      mw->addDockedWidgetTabbed(QStringLiteral("Effects"),
                                DockArea::Right, effectsPanel,
                                QStringLiteral("Inspector"));
    }
    auto *propertyPanel = WidgetCreationDiagnostics::createMeasured(
        QStringLiteral("Properties"), QStringLiteral("eager-widget"),
        QStringLiteral("startup-default-right-editing-surface"),
        [mw]() { return new ArtifactPropertyWidget(mw); });
    mw->addDockedWidgetTabbed(QStringLiteral("Properties"),
                              DockArea::Right, propertyPanel,
                              QStringLiteral("Inspector"));
    auto *collaborationPresence = WidgetCreationDiagnostics::createMeasured(
        QStringLiteral("Collaboration"), QStringLiteral("eager-widget"),
        QStringLiteral("optional-session-presence-surface"),
        [mw]() { return new CollabPresenceWidget(mw); });
    mw->addDockedWidgetTabbed(QStringLiteral("Collaboration"),
                              DockArea::Right, collaborationPresence,
                              QStringLiteral("Inspector"));
    collaborationController =
        new CollaborationDockController(collaborationPresence, mw);
    mw->addLazyDockedWidgetTabbedWithId(
        QStringLiteral("Audio Mixer"), QStringLiteral("Audio Mixer"),
        DockArea::Bottom,
        [mw]() -> QWidget * {
          return new ArtifactCompositionAudioMixerWidget(mw);
        },
        QStringLiteral("timeline::"));
    mw->addLazyDockedWidgetTabbedWithId(
        QStringLiteral("AI Cloud"), QStringLiteral("AI Cloud"),
        DockArea::Right,
        [mw]() -> QWidget * { return new ArtifactAICloudWidget(mw); },
        QString());
    mw->setDockVisible(QStringLiteral("AI Cloud"), false);
  mw->setDockVisible(QStringLiteral("Audio Mixer"), false);
  mw->addLazyDockedWidgetTabbedWithId(
      QStringLiteral("Recovery Workspace"), QStringLiteral("RecoveryWorkspace"),
      DockArea::Right,
      [mw, sessionLedgerPath]() -> QWidget* {
        return new ArtifactRecoveryWorkspaceWidget(sessionLedgerPath, mw);
      },
      QStringLiteral("Inspector"));
  mw->setDockVisible(QStringLiteral("Recovery Workspace"), false);
  mw->setDockVisible(QStringLiteral("Composition View (Software)"), false);
    mw->setDockVisible(QStringLiteral("Layer Solo View"), false);
    mw->setDockVisible(QStringLiteral("Layer View (Software)"), false);

    autoSaveManager->initialize(
        std::filesystem::path("ArtifactProject"),
        std::filesystem::path(recoveryDir.toStdWString()));
    autoSaveManager->start();
    if (!isStartupDialogSuppressed()) {
      const bool hasRecoveryPoint = autoSaveManager->hasRecoveryPoint();
      showUncleanExitNoticeIfNeeded(hadUncleanExit, mw);
      showRecoveryPrompt(*autoSaveManager, mw);
      if (hadUncleanExit || hasRecoveryPoint) {
        suppressStartupDialogForDays(3);
      }
    }

    if (projectService) {
      auto *selectionManager = ArtifactApplicationManager::instance()
                                   ? ArtifactApplicationManager::instance()
                                         ->layerSelectionManager()
                                   : nullptr;
      {
        const auto resolveLayerForUi =
            [projectService, selectionManager](
                const LayerID &layerId) -> ArtifactAbstractLayerPtr {
          auto current = selectionManager ? selectionManager->currentLayer()
                                          : ArtifactAbstractLayerPtr{};
          if (current && (layerId.isNil() || current->id() == layerId)) {
            return current;
          }
          if (projectService) {
            if (const auto comp = projectService->currentComposition().lock()) {
              if (!layerId.isNil()) {
                if (auto target = comp->layerById(layerId)) {
                  return target;
                }
              }
            }
          }
          return layerId.isNil() ? current : ArtifactAbstractLayerPtr{};
        };

        auto retainedPropertyLayerId =
            ArtifactCore::makeShared<LayerID>(LayerID::Nil());
        const auto selectionReasonToString =
            [](LayerSelectionChangeReason reason) -> QString {
          return layerSelectionChangeReasonToString(reason);
        };
        const auto syncPropertyPanelLayer =
            [mw, propertyPanel, resolveLayerForUi, retainedPropertyLayerId,
             selectionReasonToString](const LayerSelectionChangedEvent &event) {
              const LayerID layerId(event.layerId);
              if (!propertyPanel) {
                return;
              }
              const auto applyResolvedLayer =
                  [propertyPanel,
                   retainedPropertyLayerId](const ArtifactAbstractLayerPtr &layer) {
                    propertyPanel->setFocusedEffectId(QString());
                    if (layer) {
                      *retainedPropertyLayerId = layer->id();
                      propertyPanel->setLayer(layer);
                    } else {
                      *retainedPropertyLayerId = LayerID::Nil();
                      propertyPanel->clear();
                    }
                  };
              const auto tryApplyResolvedLayer =
                  [resolveLayerForUi, applyResolvedLayer](
                      const LayerID &candidateId) -> bool {
                    if (const auto resolved = resolveLayerForUi(candidateId)) {
                      applyResolvedLayer(resolved);
                      return true;
                    }
                    return false;
                  };

              if (tryApplyResolvedLayer(layerId)) {
                return;
              }

              qDebug() << "[PropertyPanel] NoLayer"
                       << "reason="
                       << selectionReasonToString(event.reason)
                       << "layerId=" << layerId.toString();

              QPointer<ArtifactPropertyWidget> safePropertyPanel(propertyPanel);
              QPointer<QWidget> safeMw(mw);
              const auto deferredResolve =
                  [safePropertyPanel, safeMw, resolveLayerForUi, layerId,
                   retainedPropertyLayerId, selectionReasonToString,
                   reason = event.reason](int retryMs) {
                    if (!safePropertyPanel) {
                      return;
                    }
                    safePropertyPanel->setFocusedEffectId(QString());
                    if (const auto resolved = resolveLayerForUi(layerId)) {
                      safePropertyPanel->setLayer(resolved);
                      *retainedPropertyLayerId = resolved->id();
                    } else if (retryMs > 0 && safeMw) {
                      // Secondary retry — layer may not be fully wired yet
                      const auto retryReason = reason;
                      QTimer::singleShot(
                          retryMs, safeMw.data(),
                          [safePropertyPanel, resolveLayerForUi, layerId,
                           retainedPropertyLayerId, retryReason,
                           selectionReasonToString]() {
                            if (!safePropertyPanel)
                              return;
                            safePropertyPanel->setFocusedEffectId(QString());
                            if (const auto resolved =
                                    resolveLayerForUi(layerId)) {
                              safePropertyPanel->setLayer(resolved);
                              *retainedPropertyLayerId = resolved->id();
                            } else {
                              *retainedPropertyLayerId = LayerID::Nil();
                              qDebug() << "[PropertyPanel] NoLayer retry"
                                       << "reason="
                                       << selectionReasonToString(retryReason)
                                       << "layerId=" << layerId.toString();
                              safePropertyPanel->clear();
                            }
                          });
                    } else {
                      *retainedPropertyLayerId = LayerID::Nil();
                      qDebug() << "[PropertyPanel] NoLayer final"
                               << "reason="
                               << selectionReasonToString(reason)
                               << "layerId=" << layerId.toString();
                      safePropertyPanel->clear();
                    }
                  };
              QTimer::singleShot(
                  0, mw, [deferredResolve]() { deferredResolve(100); });
            };

        appEventSubscriptions.push_back(
          appEventBus.subscribe<ProjectChangedEvent>(
              [status, autoSaveManager](const ProjectChangedEvent &) {
                if (status) {
                  status->setProjectText("Modified");
                }
                ArtifactPythonHookManager::runHook(
                    QStringLiteral("project_changed"));
                if (autoSaveManager) {
                  autoSaveManager->markDirty();
                }
              }));
        appEventSubscriptions.push_back(appEventBus.subscribe<LayerChangedEvent>(
          [status, autoSaveManager](const LayerChangedEvent &event) {
            if (event.changeType == LayerChangedEvent::ChangeType::Created) {
              if (status) {
                status->setProjectText("Layer Added");
              }
              ArtifactPythonHookManager::runHook(
                  QStringLiteral("layer_added"),
                  QStringList() << event.compositionId << event.layerId);
            } else if (event.changeType ==
                       LayerChangedEvent::ChangeType::Removed) {
              if (status) {
                status->setProjectText("Layer Removed");
              }
              ArtifactPythonHookManager::runHook(
                  QStringLiteral("layer_removed"),
                  QStringList() << event.compositionId << event.layerId);
            }
            if (autoSaveManager) {
              autoSaveManager->markDirty();
            }
          }));
        appEventSubscriptions.push_back(
          appEventBus.subscribe<LayerSelectionChangedEvent>(
              [layerViewEditor, status, projectService, resolveLayerForUi,
               syncPropertyPanelLayer,
               &collaborationController](const LayerSelectionChangedEvent &event) {
                if (collaborationController) {
                  collaborationController->refreshLocalPresence();
                }
                const LayerID incomingLayerId(event.layerId);
                LayerID layerId = incomingLayerId;
                if (layerId.isNil()) {
                  if (const auto resolved = resolveLayerForUi(layerId)) {
                    layerId = resolved->id();
                  }
                }
                if (layerViewEditor) {
                  if (layerId.isNil()) {
                    layerViewEditor->view()->clearTargetLayer();
                  } else {
                    layerViewEditor->setTargetLayer(layerId);
                  }
                }
                syncPropertyPanelLayer(event);
                if (status) {
                  int selectedLayerCount = 0;
                  if (auto *app = ArtifactApplicationManager::instance()) {
                    if (auto *selectionManager = app->layerSelectionManager()) {
                      selectedLayerCount = static_cast<int>(
                          selectionManager->selectedLayers().size());
                    }
                  }
                  status->setSelectionCount(selectedLayerCount);
                  const auto resolveLayerStatusText =
                      [projectService](const LayerID &candidateId) -> QString {
                    if (candidateId.isNil()) {
                      return QStringLiteral("None");
                    }
                    if (auto *app = ArtifactApplicationManager::instance()) {
                      if (auto *selectionManager = app->layerSelectionManager()) {
                        if (auto current = selectionManager->currentLayer()) {
                          if (current->id() == candidateId) {
                            const QString name = current->layerName().trimmed();
                            return name.isEmpty() ? QStringLiteral("Unnamed Layer")
                                                  : name;
                          }
                        }
                      }
                    }
                    if (auto comp =
                            projectService
                                ? projectService->currentComposition().lock()
                                : ArtifactCompositionPtr{}) {
                      if (auto layer = comp->layerById(candidateId)) {
                        const QString name = layer->layerName().trimmed();
                        return name.isEmpty() ? QStringLiteral("Unnamed Layer")
                                              : name;
                      }
                    }
                    return QStringLiteral("None");
                  };
                  if (layerId.isNil()) {
                    status->setLayerText("None");
                  } else {
                    status->setLayerText(resolveLayerStatusText(layerId));
                  }
                }
                  }));
      }
        appEventSubscriptions.push_back(
           appEventBus.subscribe<CurrentCompositionChangedEvent>(
                [mw, compositionEditor, projectService, propertyPanel,
                  layerViewEditor, status,
                  &collaborationController](const CurrentCompositionChangedEvent &event) {
                if (collaborationController) {
                  collaborationController->refreshLocalPresence();
                }
                const CompositionID compId(event.compositionId);
                if (compositionEditor && projectService) {
                  const auto found = projectService->findComposition(compId);
                  if (found.success && !found.ptr.expired()) {
                    auto comp = found.ptr.lock();
                    if (mw) {
                      // ArtifactCompositionEditor already subscribes to
                      // CurrentCompositionChangedEvent and applies the render
                      // state itself. Do not call setComposition here as well:
                      // doing so duplicated renderer initialization and made the
                      // create-composition interaction appear to hang. Defer only
                      // the dock/title update and ignore stale queued events.
                      QTimer::singleShot(0, mw, [mw, compositionEditor,
                                                 projectService, compId]() {
                        if (!mw || !compositionEditor || !projectService) {
                          return;
                        }
                        const auto current =
                            projectService->currentComposition().lock();
                        if (!current || current->id() != compId) {
                          return;
                        }
                        const QString compName =
                            current->settings().compositionName().toQString();
                        if (!compName.isEmpty()) {
                          compositionEditor->setWindowTitle(compName);
                        }
                        mw->setDockVisible(
                            QStringLiteral("Composition Viewer"), true);
                      });
                    }
                  }
                }
                if (propertyPanel) {
                  propertyPanel->setFocusedEffectId(QString());
                  // Only clear the property panel when the composition change
                  // is to a different composition than the one the currently-
                  // displayed layer belongs to. This prevents spurious clears
                  // when an internal call triggers a composition-changed event
                  // for the same composition (e.g., from timeline or property
                  // animation notifications).
                  propertyPanel->clear();
                }
                if (layerViewEditor) {
                  layerViewEditor->view()->clearTargetLayer();
                }
                if (status) {
                  if (compId.isNil()) {
                    status->setSelectionCount(0);
                    status->setLayerText("None");
                    status->setCompositionInfo("NO COMPOSITION", 0, 0, 0);
                  } else if (auto comp =
                                 projectService
                                     ? projectService->currentComposition()
                                           .lock()
                                     : ArtifactCompositionPtr{}) {
                    ArtifactAbstractLayerPtr currentLayer;
                    if (auto *app = ArtifactApplicationManager::instance()) {
                      if (auto *selectionManager = app->layerSelectionManager()) {
                        currentLayer = selectionManager->currentLayer();
                      }
                    }
                    if (currentLayer && !currentLayer->layerName().trimmed().isEmpty()) {
                      status->setLayerText(currentLayer->layerName().trimmed());
                    } else if (currentLayer) {
                      status->setLayerText(QStringLiteral("Unnamed Layer"));
                    } else {
                      status->setLayerText("None");
                    }
                    int selectedLayerCount = 0;
                    if (auto *app = ArtifactApplicationManager::instance()) {
                      if (auto *selectionManager = app->layerSelectionManager()) {
                        selectedLayerCount = static_cast<int>(
                            selectionManager->selectedLayers().size());
                      }
                    }
                    status->setSelectionCount(selectedLayerCount);
                    const auto &settings = comp->settings();
                    status->setCompositionInfo(
                        settings.compositionName().toQString(),
                        settings.compositionSize().width(),
                        settings.compositionSize().height(),
                        comp->frameRate().framerate());
                  }
                }
              }));
      const auto timelineDockTitle =
          [projectService](const CompositionID &compId) {
            QString compositionLabel = compId.toString();
            if (projectService) {
              const auto found = projectService->findComposition(compId);
              if (found.success) {
                if (auto composition = found.ptr.lock()) {
                  const QString liveName = composition->settings()
                                               .compositionName()
                                               .toQString()
                                               .trimmed();
                  if (!liveName.isEmpty()) {
                    compositionLabel = liveName;
                  }
                }
              }
            }
            return compositionLabel;
          };
      const auto timelineDockObjectId = [](const CompositionID &compId) {
        return QStringLiteral("timeline::%1").arg(compId.toString());
      };
      const auto dopeSheetDockTitle =
          [timelineDockTitle](const CompositionID &compId) {
            return QStringLiteral("%1 Dope Sheet").arg(
                timelineDockTitle(compId));
          };
      const auto dopeSheetDockObjectId = [](const CompositionID &compId) {
        return QStringLiteral("dopesheet::%1").arg(compId.toString());
      };
      const auto animationTimelineDockTitle =
          [timelineDockTitle](const CompositionID &compId) {
            return QStringLiteral("%1 Animation Timeline").arg(timelineDockTitle(compId));
          };
      const auto animationTimelineDockObjectId = [](const CompositionID &compId) {
        return QStringLiteral("animation-timeline::%1").arg(compId.toString());
      };
      const auto audioMiniDockTitle = [timelineDockTitle](const CompositionID &compId) {
        return QStringLiteral("%1 Audio Mini").arg(timelineDockTitle(compId));
      };
      const auto audioMiniDockObjectId = [](const CompositionID &compId) {
        return QStringLiteral("audio-mini::%1").arg(compId.toString());
      };
      appEventSubscriptions.push_back(
          appEventBus.subscribe<CompositionCreatedEvent>(
              [mw, timelineDockTitle, timelineDockObjectId, dopeSheetDockTitle,
               dopeSheetDockObjectId, animationTimelineDockTitle,
               animationTimelineDockObjectId, audioMiniDockTitle, audioMiniDockObjectId,
               status, collaborationController](const CompositionCreatedEvent &event) {
                const CompositionID compId(event.compositionId);
                if (!mw || compId.isNil()) {
                  return;
                }

                // The service queues CurrentCompositionChangedEvent immediately
                // after this notification.  Do not suspend main-window updates or
                // construct the per-composition docks ahead of it: doing so held
                // back the composition viewer until the Timeline's expensive
                // setup had completed.
                QTimer::singleShot(
                    1, mw,
                    [mw, compId, timelineDockTitle, timelineDockObjectId,
                     dopeSheetDockTitle, dopeSheetDockObjectId, animationTimelineDockTitle,
                     animationTimelineDockObjectId, audioMiniDockTitle, audioMiniDockObjectId, status,
                     event]() {
                      if (!mw) {
                        return;
                      }
                      QElapsedTimer setupTimer;
                      setupTimer.start();
                      QElapsedTimer phaseTimer;
                      phaseTimer.start();
                      ArtifactPythonHookManager::runHook(
                          QStringLiteral("composition_created"),
                          QStringList() << event.compositionId);
                      const double pythonHookMs =
                          static_cast<double>(phaseTimer.nsecsElapsed()) /
                          1000000.0;
                      const QString dockTitle = timelineDockTitle(compId);
                      const QString dockId = timelineDockObjectId(compId);
                      if (mw->hasDock(dockId)) {
                        return;
                      }
                      // Register the docks in this single queued turn.  A delayed
                      // second timer allowed duplicate CompositionCreated events to
                      // enqueue multiple registrations before the first one became
                      // visible, which caused dock windows to flash open and close.
                      auto *service = ArtifactProjectService::instance();
                      if (!service) {
                        return;
                      }
                      const auto created = service->findComposition(compId);
                      if (!created.success || created.ptr.expired()) {
                        return;
                      }

                      // Register the Dope Sheet for ADS layout restoration, but keep
                      // it hidden until the user explicitly selects that tab.
                      phaseTimer.restart();
                      mw->addLazyDockedWidgetTabbedWithId(
                          dopeSheetDockTitle(compId),
                          dopeSheetDockObjectId(compId),
                          DockArea::Bottom,
                          [mw, compId, dopeSheetDockTitle]() -> QWidget * {
                            auto *panel = new ArtifactDopeSheetWidget(mw);
                            panel->setMinimumHeight(180);
                            panel->setComposition(compId);
                            panel->setWindowTitle(dopeSheetDockTitle(compId));
                            return panel;
                          },
                          QStringLiteral("timeline::"));
                      mw->addLazyDockedWidgetTabbedWithId(
                          animationTimelineDockTitle(compId),
                          animationTimelineDockObjectId(compId),
                          DockArea::Bottom,
                          [mw, compId, animationTimelineDockTitle]() -> QWidget * {
                            auto *panel = new ArtifactAnimationTimelineWidget(mw);
                            panel->setMinimumHeight(96);
                            panel->setComposition(compId);
                            panel->setWindowTitle(animationTimelineDockTitle(compId));
                            return panel;
                          },
                          QStringLiteral("timeline::"));
                      mw->addLazyDockedWidgetTabbedWithId(
                          audioMiniDockTitle(compId), audioMiniDockObjectId(compId),
                          DockArea::Bottom,
                          [mw, compId, audioMiniDockTitle]() -> QWidget * {
                            auto *panel = new ArtifactAudioMiniWidget(mw);
                            panel->setMinimumHeight(180);
                            panel->setComposition(compId);
                            panel->setWindowTitle(audioMiniDockTitle(compId));
                            return panel;
                          },
                          QStringLiteral("timeline::"));
                      const double lazyDopeSheetDockMs =
                          static_cast<double>(phaseTimer.nsecsElapsed()) /
                          1000000.0;

                      // The lazy dock path wraps Timeline in an auto-scroll
                      // host. Its nested owner-drawn panes then miss their
                      // normal geometry/paint pass, leaving the panel blank.
                      // Create the regular dock after this layout transaction
                      // has restored main-window updates.
                      QTimer::singleShot(
                          0, mw, [mw, compId, dockTitle, dockId, status,
                                  collaborationController]() {
                            if (!mw || mw->hasDock(dockId)) {
                              return;
                            }
                            QElapsedTimer totalTimer;
                            totalTimer.start();
                            QElapsedTimer phaseTimer;
                            phaseTimer.start();
                            auto *panel =
                                WidgetCreationDiagnostics::createMeasured(
                                    dockTitle,
                                    QStringLiteral("composition-widget"),
                                    QStringLiteral(
                                        "composition-created-primary-timeline"),
                                    [mw]() {
                                      return new ArtifactTimelineWidget(mw);
                                    });
                            const double constructorMs =
                                static_cast<double>(phaseTimer.nsecsElapsed()) /
                                1000000.0;
                            panel->setMinimumHeight(200);
                            panel->resize(1200, 350);
                            phaseTimer.restart();
                            panel->setComposition(compId);
                            if (collaborationController) {
                              collaborationController->refreshTimelineLockIndicators();
                            }
                            const double setCompositionMs =
                                static_cast<double>(phaseTimer.nsecsElapsed()) /
                                1000000.0;
                            panel->setWindowTitle(dockTitle);
                            phaseTimer.restart();
                            mw->addDockedWidgetTabbedWithId(
                                dockTitle, dockId, DockArea::Bottom,
                                panel, QStringLiteral("timeline::"));
                            const double dockAttachMs =
                                static_cast<double>(phaseTimer.nsecsElapsed()) /
                                1000000.0;
                            phaseTimer.restart();
                            constexpr auto kDockMutationDepth =
                                "artifactProgrammaticDockMutationDepth";
                            const int previousDockMutationDepth =
                                mw->property(kDockMutationDepth).toInt();
                            mw->setProperty(kDockMutationDepth,
                                            previousDockMutationDepth + 1);
                            mw->setDockSplitterSizes(dockId, {700, 350});
                            mw->setDockVisible(dockId, true);
                            mw->activateDock(dockId);
                            panel->setFocus(Qt::OtherFocusReason);
                            mw->setProperty(kDockMutationDepth,
                                            previousDockMutationDepth);
                            const double dockActivationMs =
                                static_cast<double>(phaseTimer.nsecsElapsed()) /
                                1000000.0;
                            const double totalMs =
                                static_cast<double>(totalTimer.nsecsElapsed()) /
                                1000000.0;
                            WidgetCreationDiagnostics::recordPhase(
                                QStringLiteral("Timeline Ready"),
                                QStringLiteral("composition-lifecycle"),
                                QStringLiteral("composition-timeline-ready"),
                                totalMs,
                                QStringLiteral(
                                    "compositionId=%1 constructorMs=%2 "
                                    "setCompositionMs=%3 dockAttachMs=%4 "
                                    "dockActivationMs=%5")
                                    .arg(compId.toString())
                                    .arg(constructorMs, 0, 'f', 2)
                                    .arg(setCompositionMs, 0, 'f', 2)
                                    .arg(dockAttachMs, 0, 'f', 2)
                                    .arg(dockActivationMs, 0, 'f', 2));
                          });

                      const double setupTotalMs =
                          static_cast<double>(setupTimer.nsecsElapsed()) /
                          1000000.0;
                      WidgetCreationDiagnostics::recordPhase(
                          QStringLiteral("Composition Dock Registration"),
                          QStringLiteral("composition-lifecycle"),
                          QStringLiteral("composition-created-queued-turn"),
                          setupTotalMs,
                          QStringLiteral(
                              "compositionId=%1 pythonHookMs=%2 "
                              "lazyDopeSheetDockMs=%3")
                              .arg(compId.toString())
                              .arg(pythonHookMs, 0, 'f', 2)
                              .arg(lazyDopeSheetDockMs, 0, 'f', 2));

                    });
              }));
      appEventSubscriptions.push_back(
          appEventBus.subscribe<CompositionRemovedEvent>(
              [mw, timelineDockObjectId,
               dopeSheetDockObjectId, animationTimelineDockObjectId,
               audioMiniDockObjectId](const CompositionRemovedEvent &event) {
                mw->closeDock(
                    timelineDockObjectId(CompositionID(event.compositionId)));
                mw->closeDock(
                    dopeSheetDockObjectId(CompositionID(event.compositionId)));
                mw->closeDock(animationTimelineDockObjectId(
                    CompositionID(event.compositionId)));
                mw->closeDock(audioMiniDockObjectId(CompositionID(event.compositionId)));
              }));
      appEventSubscriptions.push_back(
          appEventBus.subscribe<ProjectCreatedEvent>(
              [mw, workspaceModeFromSettings](const ProjectCreatedEvent &) {
                ArtifactPythonHookManager::runHook(
                    QStringLiteral("project_opened"));
                const QString projectPath =
                    ArtifactProjectManager::getInstance().currentProjectPath();
                if (QFileInfo(projectPath).isDir()) {
                  auto& config = ArtifactCore::LayeredConfigStore::instance();
                  if (config.isLoaded(ArtifactCore::ConfigLayer::Project)) {
                    config.unloadLayer(ArtifactCore::ConfigLayer::Project);
                  }
                  config.loadLayer(
                      ArtifactCore::ConfigLayer::Project,
                      QDir(projectPath).filePath(QStringLiteral(".artifact/settings.cbor")));
                  if (mw) {
                    mw->setWorkspaceMode(workspaceModeFromSettings());
                  }
                }
              }));
    }

    if (projectService && compositionEditor) {
      if (auto current = projectService->currentComposition().lock()) {
        compositionEditor->setComposition(current);
      }
    }

    auto latestFrame = ArtifactCore::makeShared<std::atomic<long long>>(0);
    auto hasFrameUpdate = ArtifactCore::makeShared<std::atomic_bool>(false);
    auto frameCounter = ArtifactCore::makeShared<std::atomic<int>>(0);

    auto uiTimer = ArtifactCore::makeShared<ArtifactCore::PreciseTicker>();
    uiTimer->setInterval(std::chrono::milliseconds(33)); // ~30Hz UI update
    uiTimer->setCallback([status, latestFrame, hasFrameUpdate]() {
      QMetaObject::invokeMethod(
          status,
          [status, latestFrame, hasFrameUpdate]() {
            if (hasFrameUpdate->exchange(false)) {
              status->setFrame(latestFrame->load());
            }
          },
          Qt::QueuedConnection);
    });
    uiTimer->start();

    auto statsTimer = ArtifactCore::makeShared<ArtifactCore::PreciseTicker>();
    statsTimer->setInterval(std::chrono::milliseconds(500));
    auto fpsElapsed = ArtifactCore::makeShared<QElapsedTimer>();
    auto smoothedFps = ArtifactCore::makeShared<double>(0.0);
    fpsElapsed->start();
    statsTimer->setCallback([status, fpsElapsed, frameCounter, smoothedFps]() {
      QMetaObject::invokeMethod(
          status,
          [status, fpsElapsed, frameCounter, smoothedFps]() {
            status->setMemoryMB(processWorkingSetMB());
            const qint64 elapsedMs = fpsElapsed->elapsed();
            if (elapsedMs > 0) {
              const int frames = frameCounter->exchange(0);
              if (frames > 0) {
                const double sampleFps =
                    frames * 1000.0 / static_cast<double>(elapsedMs);
                if (*smoothedFps <= 0.0) {
                  *smoothedFps = sampleFps;
                } else {
                  *smoothedFps = (*smoothedFps * 0.75) + (sampleFps * 0.25);
                }
              }
              if (*smoothedFps > 0.0) {
                status->setFPS(*smoothedFps);
              }
            }
            fpsElapsed->restart();
          },
          Qt::QueuedConnection);
    });
    statsTimer->start();

    auto *recoveryTimer = new QTimer(mw);
    const int autoSaveIntervalMinutes = std::max(
        1, ArtifactCore::ArtifactAppSettings::instance()
               ->autoSaveIntervalMinutes());
    recoveryTimer->setInterval(autoSaveIntervalMinutes * 60000);
    QObject::connect(recoveryTimer, &QTimer::timeout, mw, [autoSaveManager]() {
      if (!autoSaveManager || !autoSaveManager->isDirty()) {
        return;
      }
      const QByteArray snapshot = currentProjectSnapshotJson();
      if (!snapshot.isEmpty()) {
        autoSaveManager->createRecoveryPoint(snapshot.toStdString());
      }
    });
    recoveryTimer->start();

    (new DebugBridgeFileWriter(mw))->start();
    (new DebugBreakpointPoller(mw, playbackService))->start();

    if (playbackService) {
      appEventSubscriptions.push_back(
          appEventBus.subscribe<FrameChangedEvent>(
              [latestFrame, hasFrameUpdate,
               frameCounter,
               &collaborationController](const FrameChangedEvent &event) {
                if (collaborationController) {
                  collaborationController->refreshPlaybackFrame();
                }
                latestFrame->store(event.frame);
                hasFrameUpdate->store(true);
                frameCounter->fetch_add(1);
              }));
    }

    const QString appDataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dataDir(appDataDir);
    if (!dataDir.exists()) {
      dataDir.mkpath(QStringLiteral("."));
    }
    ArtifactCore::FastSettingsStore layoutStore(
        dataDir.filePath(QStringLiteral("main_window_layout.cbor")));
    sanitizeLayoutStore(layoutStore);
    if (!layoutStore.contains(QStringLiteral("MainWindow/layoutKey")) &&
        !layoutStore.contains(QStringLiteral("MainWindow/geometry")) &&
        !layoutStore.contains(QStringLiteral("MainWindow/state"))) {
      // Backward compatibility: import once from legacy QSettings.
      QSettings legacy(QStringLiteral("ArtifactStudio"),
                       QStringLiteral("Artifact"));
      auto legacyState =
          UiLayoutState::loadFromSettings(legacy, QStringLiteral("MainWindow"));
      if (!legacyState.isEmpty()) {
        legacyState.saveToStore(layoutStore, QStringLiteral("MainWindow"));
        layoutStore.sync();
      }
    }
    UiLayoutState layoutState("ArtifactMainWindow");
    layoutState = UiLayoutState::loadFromStore(layoutStore, "MainWindow");
    if (layoutState.version != kMainWindowLayoutVersion) {
      layoutStore.remove("MainWindow/layoutKey");
      layoutStore.remove("MainWindow/version");
      layoutStore.remove("MainWindow/geometry");
      layoutStore.remove("MainWindow/state");
      layoutStore.remove("MainWindow/dockState");
      layoutStore.remove("MainWindow/portableDockLayout");
      layoutStore.sync();
      layoutState = UiLayoutState("ArtifactMainWindow");
    }
    QElapsedTimer startupLayoutTimer;
    startupLayoutTimer.start();
    bool geometryRestored = true;
    const bool hasGeometry = !layoutState.geometry.isEmpty();
    if (!layoutState.geometry.isEmpty()) {
      geometryRestored = mw->restoreGeometry(layoutState.geometry);
    }
    qInfo() << "[AppMain][Startup] window geometry restore ms="
            << startupLayoutTimer.elapsed();
    // ADS dockState is the sole authoritative workspace layout.  QMainWindow
    // state is intentionally not restored because it can replay stale toolbar
    // and dock geometry before ADS has reached its final graph.
    // Capture the standard ADS arrangement before restoring the user's
    // persisted layout so View > Workspace Presets can restore it later.
    mw->captureDefaultDockManagerState();
    mw->setWorkspaceMode(workspaceModeFromSettings());
    bool resetApplied = false;
    if (!layoutState.geometry.isEmpty() && !geometryRestored) {
      // Saved layout is likely incompatible with current dock/widget set.
      layoutStore.remove("MainWindow/layoutKey");
      layoutStore.remove("MainWindow/version");
      layoutStore.remove("MainWindow/geometry");
      layoutStore.remove("MainWindow/state");
      layoutStore.remove("MainWindow/dockState");
      layoutStore.remove("MainWindow/portableDockLayout");
      layoutStore.sync();
      mw->resize(1600, 900);
      resetApplied = true;
    }
    recordLayoutRestoreResult(hasGeometry, geometryRestored, true, resetApplied);
    // ADS の dock 配置を復元。全 dock が DockManager に登録された後でなければ
    // ならないため、setStartupLayoutFrozen(false) の直前で呼ぶ。
    // 古い version 1 のレイアウト（dockState 無し）はスキップされる。
    startupLayoutTimer.restart();
    if (!layoutState.dockState.isEmpty()) {
      mw->restoreDockManagerState(layoutState.dockState);
    }
    qInfo() << "[AppMain][Startup] ADS dock restore ms="
            << startupLayoutTimer.elapsed();
    // A persisted dock graph can contain animation/timeline surfaces from a
    // previous session.  Re-apply the selected workspace after restore so the
    // workspace visibility contract wins over stale dock visibility (Default
    // must not reopen the Curve Editor on startup).
    mw->setWorkspaceMode(workspaceModeFromSettings());
    // The composition editor is the default central work surface.  This must
    // happen after ADS restore; activating a layer test tab here hid it again.
    mw->setDockVisible(QStringLiteral("Composition Viewer"), true);
    mw->activateDock(QStringLiteral("Composition Viewer"));
    // Auxiliary AI surfaces should never be reopened implicitly by a persisted
    // startup layout.
    mw->setDockVisible(QStringLiteral("AI Cloud"), false);
    startupLayoutTimer.restart();
    mw->setStartupLayoutFrozen(false);
    // Migrate an existing QADS-only session into the backend-neutral model
    // after the legacy state has been applied.  The old blob remains intact
    // as the compatibility fallback while the portable representation is
    // generated once for future backend restoration.
    if (layoutStore.value(QStringLiteral("MainWindow/portableDockLayout"))
            .toByteArray()
            .isEmpty()) {
      const QByteArray migratedPortableLayout =
          mw->savePortableDockLayoutState();
      if (!migratedPortableLayout.isEmpty()) {
        layoutStore.setValue(QStringLiteral("MainWindow/portableDockLayout"),
                             migratedPortableLayout);
        layoutStore.sync();
        qInfo() << "[AppMain][Startup] migrated ADS dock state to portable"
                << "bytes=" << migratedPortableLayout.size();
      }
    }
    const QByteArray usePortableDockLayout =
        qgetenv("ARTIFACT_USE_PORTABLE_DOCK_LAYOUT").trimmed().toLower();
    if (usePortableDockLayout == QByteArrayLiteral("1") ||
        usePortableDockLayout == QByteArrayLiteral("true") ||
        usePortableDockLayout == QByteArrayLiteral("on")) {
      const QByteArray portableDockLayout =
          layoutStore.value(QStringLiteral("MainWindow/portableDockLayout"))
              .toByteArray();
      const bool portableRestored =
          !portableDockLayout.isEmpty() &&
          mw->restorePortableDockLayoutState(portableDockLayout);
      qInfo() << "[AppMain][Startup] portable dock layout restore"
              << "requested=" << !portableDockLayout.isEmpty()
              << "restored=" << portableRestored;
    }
    mw->setDockVisible(QStringLiteral("App Debugger"), false);
    qInfo() << "[AppMain][Startup] layout finalize ms="
            << startupLayoutTimer.elapsed();
  });

  QFile shutdownDiagnosticFile;
  QElapsedTimer shutdownDiagnosticTimer;
  const auto appendShutdownDiagnostic =
      [&shutdownDiagnosticFile, &shutdownDiagnosticTimer](const QString &stage) {
        if (!shutdownDiagnosticFile.isOpen()) {
          const QString appDataDir =
              QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
          QDir logDir(appDataDir);
          if (!logDir.mkpath(QStringLiteral("Logs/ShutdownSessions")) ||
              !logDir.cd(QStringLiteral("Logs/ShutdownSessions"))) {
            qWarning() << "[AppMain][Shutdown] failed to create diagnostic directory";
            return;
          }
          const QString fileName =
              QStringLiteral("shutdown_%1.log")
                  .arg(QDateTime::currentDateTime().toString(
                      QStringLiteral("yyyyMMdd_HHmmss_zzz")));
          shutdownDiagnosticFile.setFileName(logDir.filePath(fileName));
          if (!shutdownDiagnosticFile.open(QIODevice::WriteOnly |
                                           QIODevice::Append)) {
            qWarning() << "[AppMain][Shutdown] failed to open diagnostic log"
                       << shutdownDiagnosticFile.fileName();
            return;
          }
          setShutdownDiagnosticPathForExit(shutdownDiagnosticFile.fileName());
          shutdownDiagnosticTimer.start();
          shutdownDiagnosticFile.write(
              QByteArrayLiteral("Artifact shutdown diagnostic session\r\n"));
        }
        const qint64 elapsedMs = shutdownDiagnosticTimer.isValid()
                                     ? shutdownDiagnosticTimer.elapsed()
                                     : 0;
        const QByteArray line =
            QStringLiteral("[%1][+%2 ms] %3\r\n")
                .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs))
                .arg(elapsedMs)
                .arg(stage)
                .toUtf8();
        shutdownDiagnosticFile.write(line);
        shutdownDiagnosticFile.flush();
      };

  QObject::connect(
      &a, &QCoreApplication::aboutToQuit,
      [mw, &workspaceManager, &appendShutdownDiagnostic]() {
    appendShutdownDiagnostic(QStringLiteral("BEGIN aboutToQuit"));
    appendShutdownDiagnostic(QStringLiteral("workspace save begin"));
    const QString appDataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dataDir(appDataDir);
    if (!dataDir.exists()) {
      dataDir.mkpath(QStringLiteral("."));
    }
    ArtifactCore::FastSettingsStore layoutStore(
        dataDir.filePath(QStringLiteral("main_window_layout.cbor")));
    UiLayoutState layoutState("ArtifactMainWindow");
    layoutState.version = kMainWindowLayoutVersion;
    layoutState.geometry = mw->saveGeometry();
    // ADS の dock 配置（タブグループ、splitter、floating 位置）だけを
    // workspace 配置の正本として保存する。QMainWindow state は保存しない。
    layoutState.dockState = mw->saveDockManagerState();
    layoutState.saveToStore(layoutStore, "MainWindow");
    layoutStore.setValue(QStringLiteral("MainWindow/portableDockLayout"),
                         mw->savePortableDockLayoutState());
    layoutStore.sync();
    workspaceManager.saveSession(mw);
    appendShutdownDiagnostic(QStringLiteral("workspace save complete"));
  });
  QObject::connect(&a, &QCoreApplication::aboutToQuit, [&]() {
    QElapsedTimer shutdownTimer;
    shutdownTimer.start();
    appendShutdownDiagnostic(QStringLiteral("service shutdown begin"));
    // RenderQueueService owns a joinable worker whose destructor waits for it.
    // Request cancellation before tearing down the UI/render controllers;
    // otherwise an active encode can keep Artifact.exe alive after the main
    // window has already disappeared.
    if (auto *renderQueueService = ArtifactRenderQueueService::instance()) {
      renderQueueService->pauseAllJobs();
      if (!renderQueueService->sessionLedger().saveToFile(sessionLedgerPath)) {
        qWarning() << "[SessionLedger] failed to persist ledger"
                   << sessionLedgerPath;
      }
      qInfo() << "[AppMain][Shutdown] render queue stop requested";
    }
    appendShutdownDiagnostic(QStringLiteral("render-queue stop requested"));
    // Do not start queued background previews while shutdown is in progress.
    // Running work observes its owner/generation guards and may finish normally.
    QThreadPool::globalInstance()->clear();
    appendShutdownDiagnostic(QStringLiteral("background queue cleared"));
    if (frameDebugTimer) {
      frameDebugTimer->stop();
      frameDebugTimer.reset();
    }
    if (compositionEditor) {
      appendShutdownDiagnostic(QStringLiteral("composition-editor stop begin"));
      compositionEditor->stop();
      appendShutdownDiagnostic(QStringLiteral("composition-editor stop complete"));
    }
    if (playbackService) {
      appendShutdownDiagnostic(QStringLiteral("playback stop begin"));
      playbackService->stop();
      playbackService->waitForStop();
      appendShutdownDiagnostic(QStringLiteral("playback stop complete"));
    }
    if (autoSaveManager) {
      appendShutdownDiagnostic(QStringLiteral("autosave shutdown begin"));
      if (autoSaveManager->isDirty()) {
        const QByteArray snapshot = currentProjectSnapshotJson();
        if (!snapshot.isEmpty()) {
          autoSaveManager->createRecoveryPoint(snapshot.toStdString());
        }
      }
      autoSaveManager->stop();
      delete autoSaveManager;
      autoSaveManager = nullptr;
      appendShutdownDiagnostic(QStringLiteral("autosave shutdown complete"));
    }
    markSessionEndClean();
    appendShutdownDiagnostic(QStringLiteral("foreground services complete"));
    qInfo() << "[AppMain][Shutdown] foreground services stopped ms="
            << shutdownTimer.elapsed();
  });

  QTimer::singleShot(2000, mw, [startupParallelism, &parallelismControl]() {
    parallelismControl.reset();
    parallelismControl = std::make_unique<StartupParallelismControl>(startupParallelism);
    setRenderSchedulerStartupWarmupComplete(true);
    qDebug() << "[AppMain] startup parallelism restored to"
             << startupParallelism;
  });

  QTimer::singleShot(0, mw, [mw]() {
    mw->show();
  });
  const int exitCode = a.exec();
  appendShutdownDiagnostic(
      QStringLiteral("event loop returned exitCode=%1").arg(exitCode));
  // Explicitly stop playback while all application services are still alive.
  // Function-local singleton destruction order must not own shutdown ordering.
  if (playbackService) {
    playbackService->stop();
    playbackService->waitForStop();
  }
  // deleteLater() queued from aboutToQuit is not guaranteed to run after the
  // main event loop has returned. Deterministically release top-level widgets
  // while QApplication and the rendering services are still alive; otherwise
  // renderer/driver worker threads can keep the process and several GB of GPU
  // resources resident with no window.
  QElapsedTimer uiTeardownTimer;
  uiTeardownTimer.start();
  qInfo() << "[AppMain][Shutdown] top-level UI teardown begin";
  appendShutdownDiagnostic(QStringLiteral("top-level UI teardown begin"));
  if (renderCenterWindow) {
    delete renderCenterWindow.data();
  }
  if (mainWindowGuard) {
    delete mainWindowGuard.data();
  }
  qInfo() << "[AppMain][Shutdown] top-level UI teardown complete ms="
          << uiTeardownTimer.elapsed();
  appendShutdownDiagnostic(
      QStringLiteral("COMPLETE top-level UI teardown elapsedMs=%1")
          .arg(uiTeardownTimer.elapsed()));
  shutdownDiagnosticFile.close();
  return exitCode;
}
