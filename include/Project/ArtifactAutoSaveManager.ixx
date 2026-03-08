module;
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Project.AutoSaveManager;

export namespace Artifact {

  enum class AutoSaveStatus {
    Idle,
    Saving,
    SaveComplete,
    SaveFailed,
    RecoveringFromCrash
  };

  class ArtifactAutoSaveManager {
  private:
    int autoSaveIntervalMinutes_ = 5;
    int maxRecoveryPoints_ = 20;
    QString autoSaveDirectory_;
    QString projectFilePath_;
    bool isDirty_ = false;
    AutoSaveStatus status_ = AutoSaveStatus::Idle;
    QString lastError_;

    QString ensureAutoSaveDir() const {
      if (autoSaveDirectory_.isEmpty()) return QString();
      QDir dir(autoSaveDirectory_);
      if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
      }
      return dir.absolutePath();
    }

    QString checkpointFileName() const {
      const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
      QFileInfo fi(projectFilePath_);
      const QString base = fi.completeBaseName().isEmpty() ? QStringLiteral("project") : fi.completeBaseName();
      return QStringLiteral("%1.autosave.%2.json").arg(base, stamp);
    }

    void pruneOldRecoveryPoints() const {
      if (maxRecoveryPoints_ <= 0) return;
      QDir dir(ensureAutoSaveDir());
      if (!dir.exists()) return;
      QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("*.autosave.*.json"), QDir::Files, QDir::Time | QDir::Reversed);
      const int removeCount = files.size() - maxRecoveryPoints_;
      for (int i = 0; i < removeCount; ++i) {
        QFile::remove(files.at(i).absoluteFilePath());
      }
    }

  public:
    ArtifactAutoSaveManager() = default;
    ~ArtifactAutoSaveManager() = default;

    void initialize(const QString& projectPath, const QString& autoSaveDir) {
      projectFilePath_ = projectPath;
      autoSaveDirectory_ = autoSaveDir;
      ensureAutoSaveDir();
    }

    void start() {
      status_ = AutoSaveStatus::SaveComplete;
      lastError_.clear();
    }

    void stop() {
      status_ = AutoSaveStatus::Idle;
    }

    void setAutoSaveInterval(int minutes) {
      autoSaveIntervalMinutes_ = std::max(1, minutes);
    }

    int autoSaveInterval() const {
      return autoSaveIntervalMinutes_;
    }

    void setMaxRecoveryPoints(int count) {
      maxRecoveryPoints_ = std::max(1, count);
      pruneOldRecoveryPoints();
    }

    int maxRecoveryPoints() const {
      return maxRecoveryPoints_;
    }

    void markDirty() { isDirty_ = true; }
    void markClean() { isDirty_ = false; }
    bool isDirty() const { return isDirty_; }

    AutoSaveStatus getStatus() const { return status_; }

    QString lastError() const { return lastError_; }

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

    bool createRecoveryPoint(const QByteArray& projectSnapshotJsonUtf8, QString* outPath = nullptr) {
      const QString dirPath = ensureAutoSaveDir();
      if (dirPath.isEmpty()) {
        status_ = AutoSaveStatus::SaveFailed;
        lastError_ = QStringLiteral("Auto-save directory is empty");
        return false;
      }

      status_ = AutoSaveStatus::Saving;
      const QString fullPath = QDir(dirPath).filePath(checkpointFileName());
      QFile file(fullPath);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        status_ = AutoSaveStatus::SaveFailed;
        lastError_ = QStringLiteral("Failed to open recovery file: %1").arg(fullPath);
        return false;
      }

      const qint64 written = file.write(projectSnapshotJsonUtf8);
      file.close();
      if (written <= 0) {
        status_ = AutoSaveStatus::SaveFailed;
        lastError_ = QStringLiteral("Failed to write recovery snapshot");
        return false;
      }

      pruneOldRecoveryPoints();
      isDirty_ = false;
      status_ = AutoSaveStatus::SaveComplete;
      lastError_.clear();
      if (outPath) *outPath = fullPath;
      return true;
    }

    bool hasRecoveryPoint() const {
      QDir dir(ensureAutoSaveDir());
      if (!dir.exists()) return false;
      return !dir.entryInfoList(QStringList() << QStringLiteral("*.autosave.*.json"), QDir::Files, QDir::Time).isEmpty();
    }

    QStringList listRecoveryPoints() const {
      QDir dir(ensureAutoSaveDir());
      QStringList result;
      if (!dir.exists()) return result;
      QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("*.autosave.*.json"), QDir::Files, QDir::Time);
      for (const auto& fi : files) {
        result.push_back(fi.absoluteFilePath());
      }
      return result;
    }

    bool loadLatestRecoveryPoint(QByteArray* outJsonUtf8, QString* outPath = nullptr) {
      if (!outJsonUtf8) return false;
      QDir dir(ensureAutoSaveDir());
      if (!dir.exists()) return false;

      QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("*.autosave.*.json"), QDir::Files, QDir::Time);
      if (files.isEmpty()) return false;

      const QString latest = files.first().absoluteFilePath();
      QFile file(latest);
      if (!file.open(QIODevice::ReadOnly)) return false;

      *outJsonUtf8 = file.readAll();
      file.close();
      if (outPath) *outPath = latest;
      status_ = AutoSaveStatus::RecoveringFromCrash;
      return !outJsonUtf8->isEmpty();
    }
  };

}
