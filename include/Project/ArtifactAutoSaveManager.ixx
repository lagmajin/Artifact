module;
#include <QObject>
#include <QString>
#include <QTimer>

export module Artifact.Project.AutoSaveManager;

import std;

export namespace Artifact {

  enum class AutoSaveStatus {
    Idle,
    Saving,
    SaveComplete,
    SaveFailed,
    RecoveringFromCrash
  };

  // Simple auto-save manager (simplified version)
  class ArtifactAutoSaveManager {
  private:
    int autoSaveIntervalMinutes_ = 5;
    QString autoSaveDirectory_;
    QString projectFilePath_;
    bool isDirty_ = false;
    AutoSaveStatus status_ = AutoSaveStatus::Idle;

  public:
    ArtifactAutoSaveManager() = default;
    ~ArtifactAutoSaveManager() = default;

    void initialize(const QString& projectPath, const QString& autoSaveDir) {
      projectFilePath_ = projectPath;
      autoSaveDirectory_ = autoSaveDir;
    }

    void start() {
      status_ = AutoSaveStatus::SaveComplete;
    }

    void stop() {
      status_ = AutoSaveStatus::Idle;
    }

    void setAutoSaveInterval(int minutes) {
      autoSaveIntervalMinutes_ = minutes;
    }

    void markDirty() { isDirty_ = true; }
    void markClean() { isDirty_ = false; }
    bool isDirty() const { return isDirty_; }

    AutoSaveStatus getStatus() const { return status_; }

    QString getStatusString() const {
      switch (status_) {
        case AutoSaveStatus::Idle:
          return "Idle";
        case AutoSaveStatus::Saving:
          return "Saving...";
        case AutoSaveStatus::SaveComplete:
          return "Auto-save enabled";
        case AutoSaveStatus::SaveFailed:
          return "Save failed";
        case AutoSaveStatus::RecoveringFromCrash:
          return "Recovery available";
        default:
          return "Unknown";
      }
    }
  };

}
