module;

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

export module Artifact.Service.DetachedTask;

export namespace Artifact {

/// What Detached execution is allowed to do with a command type.
enum class DetachedCommandPolicy {
  /// Reads state only.  Runs immediately, without the interaction gate or an
  /// approval round trip.
  ReadOnly,
  /// Mutates state.  Waits for the interaction gate and restores the view
  /// afterwards.
  PreserveView,
  /// Removes user content.  Requires an explicit approval before running.
  Destructive,
  /// Hands the work to the render queue.  Completes as soon as it is queued.
  LongJob,
  /// Coupled to what the user is looking at.  Never runs detached.
  Rejected,
};

/// Lifecycle of a detached task.
enum class DetachedTaskState {
  Pending,
  Deferred,
  AwaitingApproval,
  Running,
  Succeeded,
  Failed,
  Cancelled,
  Denied,
};

/// Why a task cannot start yet.
enum class DetachedDeferral {
  None,
  InteractionBusy,
  ModalDialog,
};

/// One detached task.  Detached execution runs on the UI thread, so records
/// are only touched from that thread.
struct DetachedTaskRecord {
  QString taskId;
  QString label;
  QString commandType;
  DetachedCommandPolicy policy = DetachedCommandPolicy::PreserveView;
  DetachedTaskState state = DetachedTaskState::Pending;
  DetachedDeferral deferral = DetachedDeferral::None;
  QString statusText;
  QString summary;
  QString errorCode;
  QString undoLabel;
  QString producedCompositionId;
  int progressPercent = 0;
  QVariantMap command;
};

/// Stable display names for event payloads.
QString detachedTaskStateName(DetachedTaskState state);
QString detachedCommandPolicyName(DetachedCommandPolicy policy);
QString detachedDeferralName(DetachedDeferral deferral);

/// Runs CommandIR requests without taking over the current view.
///
/// Tasks are executed one at a time in arrival order on the UI thread.
/// Execution is deferred while the user is interacting with the viewport or a
/// modal dialog is open, and destructive commands wait for an explicit
/// approve()/deny().
class ArtifactDetachedTaskService {
public:
  static ArtifactDetachedTaskService* instance();

  /// Queues a validated CommandIR request.  Returns the task id, or an empty
  /// string when the request has no usable command type.
  QString submitCommand(const QVariantMap& command);

  /// Queues a CommandIR request from text.  The text must be a JSON object;
  /// anything else produces a failed task so the caller still gets feedback.
  QString submitCommandText(const QString& text);

  void cancel(const QString& taskId);
  void approve(const QString& taskId);
  void deny(const QString& taskId);

  /// Removes finished tasks.  Running and waiting tasks are kept.
  void clearFinished();

  QVariantList taskSnapshot() const;
  QVariantMap executionContextSnapshot() const;

  /// Registered by the app bootstrap so this service does not depend on the
  /// widget layer.  Missing probes are treated as "not busy".
  void setInteractionBusyProbe(std::function<bool()> probe);
  void setTimelineInteractingProbe(std::function<bool()> probe);

  static DetachedCommandPolicy classify(const QString& commandType);
  static bool commandRequiresApproval(const QString& commandType);

private:
  ArtifactDetachedTaskService();
  ~ArtifactDetachedTaskService();
  ArtifactDetachedTaskService(const ArtifactDetachedTaskService&) = delete;
  ArtifactDetachedTaskService& operator=(const ArtifactDetachedTaskService&) = delete;

  class Impl;
  Impl* impl_ = nullptr;
};

} // namespace Artifact
