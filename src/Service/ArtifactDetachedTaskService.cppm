module;

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>
#include <QTimer>
#include <QWidget>
#include <deque>
#include <memory>
#include <utility>
#include <vector>
#include <functional>

module Artifact.Service.DetachedTask;

import Core.AI.CommandIR;
import Event.Bus;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.AI.WorkspaceAutomation;
import Artifact.Service.ActiveContext;

namespace Artifact {

namespace {

constexpr int kPumpIntervalMs = 100;

QString newDetachedTaskId() {
  static quint64 counter = 0;
  return QStringLiteral("detached-%1").arg(++counter);
}

QString commandLabel(const QString& commandType, const QVariantMap& command) {
  const QString name = command.value(QStringLiteral("name")).toString().trimmed();
  if (!name.isEmpty()) {
    return QStringLiteral("%1 \"%2\"").arg(commandType, name);
  }
  return commandType;
}

QString commandApprovalSummary(const QString& commandType, const QVariantMap& command) {
  const QVariantMap target = command.value(QStringLiteral("target")).toMap();
  QStringList parts;
  for (auto it = target.constBegin(); it != target.constEnd(); ++it) {
    const QString value = it.value().toString().trimmed();
    if (!value.isEmpty()) {
      parts << QStringLiteral("%1=%2").arg(it.key(), value);
    }
  }
  if (parts.isEmpty()) {
    return commandType;
  }
  return QStringLiteral("%1 (%2)").arg(commandType, parts.join(QStringLiteral(", ")));
}

bool isTerminalState(DetachedTaskState state) {
  return state == DetachedTaskState::Succeeded ||
         state == DetachedTaskState::Failed ||
         state == DetachedTaskState::Cancelled ||
         state == DetachedTaskState::Denied;
}

} // namespace

QString detachedTaskStateName(DetachedTaskState state) {
  switch (state) {
  case DetachedTaskState::Pending: return QStringLiteral("Pending");
  case DetachedTaskState::Deferred: return QStringLiteral("Deferred");
  case DetachedTaskState::AwaitingApproval: return QStringLiteral("AwaitingApproval");
  case DetachedTaskState::Running: return QStringLiteral("Running");
  case DetachedTaskState::Succeeded: return QStringLiteral("Succeeded");
  case DetachedTaskState::Failed: return QStringLiteral("Failed");
  case DetachedTaskState::Cancelled: return QStringLiteral("Cancelled");
  case DetachedTaskState::Denied: return QStringLiteral("Denied");
  }
  return QStringLiteral("Unknown");
}

QString detachedCommandPolicyName(DetachedCommandPolicy policy) {
  switch (policy) {
  case DetachedCommandPolicy::ReadOnly: return QStringLiteral("ReadOnly");
  case DetachedCommandPolicy::PreserveView: return QStringLiteral("PreserveView");
  case DetachedCommandPolicy::Destructive: return QStringLiteral("Destructive");
  case DetachedCommandPolicy::LongJob: return QStringLiteral("LongJob");
  case DetachedCommandPolicy::Rejected: return QStringLiteral("Rejected");
  }
  return QStringLiteral("Unknown");
}

QString detachedDeferralName(DetachedDeferral deferral) {
  switch (deferral) {
  case DetachedDeferral::None: return QString();
  case DetachedDeferral::InteractionBusy: return QStringLiteral("ビューポート操作中");
  case DetachedDeferral::ModalDialog: return QStringLiteral("ダイアログ表示中");
  }
  return QString();
}

class ArtifactDetachedTaskService::Impl {
public:
  std::vector<DetachedTaskRecord> tasks;
  std::deque<QString> queue;
  QTimer* pump = nullptr;
  std::function<bool()> interactionBusyProbe;
  std::function<bool()> timelineInteractingProbe;

  /// Every accessor re-resolves by id: callers invoke application services
  /// in between, and those calls can push more tasks into `tasks`, which would
  /// invalidate a held reference.
  DetachedTaskRecord* find(const QString& taskId) {
    for (auto& record : tasks) {
      if (record.taskId == taskId) {
        return &record;
      }
    }
    return nullptr;
  }

  bool canExecuteNow(DetachedDeferral* deferralOut) const {
    if (QApplication::activeModalWidget() != nullptr) {
      if (deferralOut) *deferralOut = DetachedDeferral::ModalDialog;
      return false;
    }
    if (interactionBusyProbe && interactionBusyProbe()) {
      if (deferralOut) *deferralOut = DetachedDeferral::InteractionBusy;
      return false;
    }
    if (timelineInteractingProbe && timelineInteractingProbe()) {
      if (deferralOut) *deferralOut = DetachedDeferral::InteractionBusy;
      return false;
    }
    if (deferralOut) *deferralOut = DetachedDeferral::None;
    return true;
  }

  static QString activeCompositionId() {
    auto* active = ArtifactActiveContextService::instance();
    if (!active) {
      return QString();
    }
    const auto composition = active->activeComposition();
    return composition ? composition->id().toString() : QString();
  }

  /// Detached execution must not move what the user is looking at.  Restoring
  /// through the canonical ProjectService path keeps the ProjectService /
  /// ActiveContext / Playback owners consistent.
  static void restoreActiveComposition(const QString& compositionId) {
    if (compositionId.isEmpty() || activeCompositionId() == compositionId) {
      return;
    }
    Artifact::WorkspaceAutomation::instance().invokeMethod(
        QStringLiteral("changeCurrentComposition"), {compositionId});
  }

  void startPump() {
    if (!pump) {
      pump = new QTimer();
      pump->setInterval(kPumpIntervalMs);
      QObject::connect(pump, &QTimer::timeout, pump, [this]() { pumpOnce(); });
    }
    if (!pump->isActive()) {
      pump->start();
    }
  }

  void stopPump() {
    if (pump && pump->isActive()) {
      pump->stop();
    }
  }

  void removeFromQueue(const QString& taskId) {
    for (auto it = queue.begin(); it != queue.end(); ++it) {
      if (*it == taskId) {
        queue.erase(it);
        return;
      }
    }
  }

  /// Runs the next runnable task, or marks the rest Deferred while the user is
  /// busy.  Tasks waiting for approval never block the ones behind them.
  void pumpOnce() {
    DetachedDeferral deferral = DetachedDeferral::None;
    const bool canRun = canExecuteNow(&deferral);

    if (!canRun) {
      for (const QString& taskId : queue) {
        auto* record = find(taskId);
        if (!record || record->state == DetachedTaskState::AwaitingApproval) {
          continue;
        }
        record->state = DetachedTaskState::Deferred;
        record->deferral = deferral;
        record->statusText = detachedDeferralName(deferral);
        publishChanged(*record);
      }
      return;
    }

    QString nextTaskId;
    for (const QString& taskId : queue) {
      auto* record = find(taskId);
      if (record && record->state != DetachedTaskState::AwaitingApproval) {
        nextTaskId = taskId;
        break;
      }
    }

    if (!nextTaskId.isEmpty()) {
      removeFromQueue(nextTaskId);
      executeRecord(nextTaskId);
      return;
    }

    bool anyUnfinished = false;
    for (const auto& record : tasks) {
      if (!isTerminalState(record.state)) {
        anyUnfinished = true;
        break;
      }
    }
    if (!anyUnfinished) {
      stopPump();
    }
  }

  void executeRecord(const QString& taskId) {
    const QString beforeCompositionId = activeCompositionId();

    QVariantMap command;
    {
      auto* record = find(taskId);
      if (!record) {
        return;
      }
      command = record->command;

      const QVariantMap validated = Artifact::WorkspaceAutomation::instance()
                                        .invokeMethod(QStringLiteral("validateCommand"), {command})
                                        .toMap();
      if (!validated.value(QStringLiteral("ok")).toBool()) {
        record->errorCode = validated.value(QStringLiteral("errorCode")).toString();
        if (record->errorCode.isEmpty()) {
          record->errorCode = QStringLiteral("COMMAND_INVALID");
        }
        const QString detail = validated.value(QStringLiteral("error")).toString();
        finish(taskId, DetachedTaskState::Failed, detail);
        return;
      }

      record->state = DetachedTaskState::Running;
      record->deferral = DetachedDeferral::None;
      record->statusText = QStringLiteral("実行中");
      record->progressPercent = 0;
      publishChanged(*record);
    }

    const QVariantMap executed = Artifact::WorkspaceAutomation::instance()
                                    .invokeMethod(QStringLiteral("executeCommand"), {command})
                                    .toMap();
    const bool succeeded = executed.value(QStringLiteral("ok")).toBool();

    restoreActiveComposition(beforeCompositionId);

    if (!succeeded) {
      if (auto* record = find(taskId)) {
        record->errorCode = executed.value(QStringLiteral("errorCode")).toString();
        if (record->errorCode.isEmpty()) {
          record->errorCode = QStringLiteral("EXECUTION_FAILED");
        }
      }
      finish(taskId, DetachedTaskState::Failed,
             executed.value(QStringLiteral("error")).toString());
      return;
    }

    if (auto* record = find(taskId)) {
      record->undoLabel = executed.value(QStringLiteral("undoLabel")).toString();
    }
    finish(taskId, DetachedTaskState::Succeeded, QString());
  }

  void finish(const QString& taskId, DetachedTaskState state, const QString& detail) {
    auto* record = find(taskId);
    if (!record) {
      return;
    }
    record->state = state;
    record->deferral = DetachedDeferral::None;
    record->progressPercent = 100;
    if (state == DetachedTaskState::Succeeded) {
      record->statusText = QStringLiteral("完了");
      record->summary = record->undoLabel.isEmpty()
                            ? QStringLiteral("表示は切り替えていません")
                            : QStringLiteral("Undo: %1 / 表示は切り替えていません")
                                  .arg(record->undoLabel);
    } else if (state == DetachedTaskState::Denied) {
      record->statusText = QStringLiteral("拒否されました");
      record->summary = detail;
    } else if (state == DetachedTaskState::Cancelled) {
      record->statusText = QStringLiteral("キャンセル");
      record->summary = detail;
    } else {
      record->statusText = QStringLiteral("失敗");
      record->summary = detail;
    }
    publishFinished(*record);
  }

  void publishAdded(const DetachedTaskRecord& record) const {
    ArtifactCore::globalEventBus().publish<DetachedTaskAddedEvent>({
        record.taskId,
        record.label,
        QStringLiteral("Command"),
        detachedTaskStateName(record.state),
    });
  }

  void publishChanged(const DetachedTaskRecord& record) const {
    ArtifactCore::globalEventBus().publish<DetachedTaskChangedEvent>({
        record.taskId,
        detachedTaskStateName(record.state),
        record.progressPercent,
        record.statusText,
    });
  }

  void publishFinished(const DetachedTaskRecord& record) const {
    ArtifactCore::globalEventBus().publish<DetachedTaskFinishedEvent>({
        record.taskId,
        detachedTaskStateName(record.state),
        record.summary,
        record.errorCode,
        record.undoLabel,
        record.producedCompositionId,
    });
  }
};

ArtifactDetachedTaskService* ArtifactDetachedTaskService::instance() {
  static ArtifactDetachedTaskService service;
  return &service;
}

ArtifactDetachedTaskService::ArtifactDetachedTaskService()
    : impl_(new Impl()) {}

ArtifactDetachedTaskService::~ArtifactDetachedTaskService() {
  if (impl_) {
    if (impl_->pump) {
      impl_->pump->stop();
      delete impl_->pump;
      impl_->pump = nullptr;
    }
    delete impl_;
    impl_ = nullptr;
  }
}

DetachedCommandPolicy ArtifactDetachedTaskService::classify(const QString& commandType) {
  const QString type = commandType.trimmed();
  if (ArtifactCore::CommandIR::isReadOnlyType(type)) {
    return DetachedCommandPolicy::ReadOnly;
  }
  if (type == QStringLiteral("export_composition") ||
      type == QStringLiteral("start_render_queue")) {
    return DetachedCommandPolicy::LongJob;
  }
  // Coupled to what the user is looking at: never run these detached.
  if (type == QStringLiteral("switch_composition") ||
      type == QStringLiteral("set_playback_state")) {
    return DetachedCommandPolicy::Rejected;
  }
  if (commandRequiresApproval(type)) {
    return DetachedCommandPolicy::Destructive;
  }
  return DetachedCommandPolicy::PreserveView;
}

bool ArtifactDetachedTaskService::commandRequiresApproval(const QString& commandType) {
  static const QStringList kDestructiveCommandTypes = {
      QStringLiteral("delete_layer"),
      QStringLiteral("remove_effect"),
      QStringLiteral("delete_keyframe"),
  };
  return kDestructiveCommandTypes.contains(commandType.trimmed());
}

QString ArtifactDetachedTaskService::submitCommand(const QVariantMap& command) {
  const ArtifactCore::CommandRequest request = ArtifactCore::CommandIR::fromVariantMap(command);
  if (request.type.trimmed().isEmpty()) {
    return QString();
  }

  DetachedTaskRecord record;
  record.taskId = newDetachedTaskId();
  record.commandType = request.type.trimmed();
  record.command = command;
  record.policy = classify(record.commandType);
  record.label = commandLabel(record.commandType, command);
  record.statusText = QStringLiteral("待機中");
  if (record.policy == DetachedCommandPolicy::Destructive) {
    record.state = DetachedTaskState::AwaitingApproval;
    record.statusText = QStringLiteral("承認待ち");
    record.summary = commandApprovalSummary(record.commandType, command);
  }

  const QString taskId = record.taskId;
  impl_->tasks.push_back(record);
  impl_->publishAdded(impl_->tasks.back());

  if (record.policy == DetachedCommandPolicy::Rejected) {
    // Nothing is executed; the task exists only to explain the refusal.
    if (auto* stored = impl_->find(taskId)) {
      stored->errorCode = QStringLiteral("DETACHED_UNSUPPORTED_COMMAND");
      stored->statusText = QStringLiteral("Detached 対象外");
    }
    impl_->finish(taskId, DetachedTaskState::Failed,
                  QStringLiteral("表示の切替・再生は Detached 対象外です"));
    return taskId;
  }

  if (record.policy == DetachedCommandPolicy::ReadOnly) {
    // Read-only commands have no side effects: run them straight away without
    // the interaction gate so status queries never wait on a drag.
    impl_->executeRecord(taskId);
    return taskId;
  }

  impl_->queue.push_back(taskId);
  impl_->startPump();
  impl_->pumpOnce();
  return taskId;
}

QString ArtifactDetachedTaskService::submitCommandText(const QString& text) {
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return QString();
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    // Command mode takes a CommandIR JSON object.  Natural language belongs to
    // the AI mode, which is not wired for detached execution yet.
    DetachedTaskRecord record;
    record.taskId = newDetachedTaskId();
    record.label = trimmed.left(48);
    record.statusText = QStringLiteral("解釈できません");
    record.errorCode = QStringLiteral("UNPARSED_COMMAND");
    const QString taskId = record.taskId;
    impl_->tasks.push_back(record);
    impl_->publishAdded(impl_->tasks.back());
    impl_->finish(taskId, DetachedTaskState::Failed,
                  QStringLiteral("コマンドは CommandIR の JSON オブジェクトで指定してください"));
    return taskId;
  }

  return submitCommand(document.object().toVariantMap());
}

void ArtifactDetachedTaskService::cancel(const QString& taskId) {
  auto* record = impl_->find(taskId);
  if (!record || isTerminalState(record->state)) {
    return;
  }
  impl_->removeFromQueue(taskId);
  impl_->finish(taskId, DetachedTaskState::Cancelled, QStringLiteral("キャンセルしました"));
}

void ArtifactDetachedTaskService::approve(const QString& taskId) {
  auto* record = impl_->find(taskId);
  if (!record || record->state != DetachedTaskState::AwaitingApproval) {
    return;
  }
  record->state = DetachedTaskState::Pending;
  record->statusText = QStringLiteral("待機中");
  impl_->startPump();
  impl_->pumpOnce();
}

void ArtifactDetachedTaskService::deny(const QString& taskId) {
  auto* record = impl_->find(taskId);
  if (!record || record->state != DetachedTaskState::AwaitingApproval) {
    return;
  }
  impl_->removeFromQueue(taskId);
  impl_->finish(taskId, DetachedTaskState::Denied, QStringLiteral("承認されませんでした"));
}

void ArtifactDetachedTaskService::clearFinished() {
  auto& tasks = impl_->tasks;
  for (auto it = tasks.begin(); it != tasks.end();) {
    if (isTerminalState(it->state)) {
      it = tasks.erase(it);
    } else {
      ++it;
    }
  }
}

QVariantList ArtifactDetachedTaskService::taskSnapshot() const {
  QVariantList result;
  for (const auto& record : impl_->tasks) {
    QVariantMap entry;
    entry.insert(QStringLiteral("taskId"), record.taskId);
    entry.insert(QStringLiteral("label"), record.label);
    entry.insert(QStringLiteral("commandType"), record.commandType);
    entry.insert(QStringLiteral("policy"), detachedCommandPolicyName(record.policy));
    entry.insert(QStringLiteral("state"), detachedTaskStateName(record.state));
    entry.insert(QStringLiteral("statusText"), record.statusText);
    entry.insert(QStringLiteral("summary"), record.summary);
    entry.insert(QStringLiteral("errorCode"), record.errorCode);
    entry.insert(QStringLiteral("undoLabel"), record.undoLabel);
    entry.insert(QStringLiteral("producedCompositionId"), record.producedCompositionId);
    entry.insert(QStringLiteral("progressPercent"), record.progressPercent);
    entry.insert(QStringLiteral("deferral"), detachedDeferralName(record.deferral));
    result.append(entry);
  }
  return result;
}

QVariantMap ArtifactDetachedTaskService::executionContextSnapshot() const {
  DetachedDeferral deferral = DetachedDeferral::None;
  const bool canExecute = impl_->canExecuteNow(&deferral);

  int pendingCount = 0;
  int awaitingApprovalCount = 0;
  int runningCount = 0;
  for (const auto& record : impl_->tasks) {
    if (record.state == DetachedTaskState::AwaitingApproval) {
      ++awaitingApprovalCount;
    } else if (record.state == DetachedTaskState::Running) {
      ++runningCount;
    } else if (!isTerminalState(record.state)) {
      ++pendingCount;
    }
  }

  QVariantMap result;
  result.insert(QStringLiteral("canExecute"), canExecute);
  result.insert(QStringLiteral("deferral"), detachedDeferralName(deferral));
  result.insert(QStringLiteral("pendingCount"), pendingCount);
  result.insert(QStringLiteral("awaitingApprovalCount"), awaitingApprovalCount);
  result.insert(QStringLiteral("runningCount"), runningCount);
  result.insert(QStringLiteral("queuedCount"), static_cast<int>(impl_->queue.size()));
  return result;
}

void ArtifactDetachedTaskService::setInteractionBusyProbe(std::function<bool()> probe) {
  impl_->interactionBusyProbe = std::move(probe);
}

void ArtifactDetachedTaskService::setTimelineInteractingProbe(std::function<bool()> probe) {
  impl_->timelineInteractingProbe = std::move(probe);
}

} // namespace Artifact
