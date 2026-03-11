module;
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QByteArray>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QShortcut>
#include <QInputDialog>
#include <QMessageBox>
#include <wobjectimpl.h>

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
module Artifact.Widgets.Render.QueueManager;




import Widgets.Utils.CSS;
import Artifact.Render.Queue.Service;
import Artifact.Service.Project;
import Core.FastSettingsStore;


namespace Artifact
{
 using namespace ArtifactCore;


	W_OBJECT_IMPL(RenderQueueManagerWidget)

 class RenderQueueManagerWidget::Impl
 {
 private:
 public:
 Impl();
  ~Impl();

  struct JobEntry {
   QString name;
   QString status;
   int progress = 0;
  };

  ArtifactRenderQueueService* service = nullptr;
  QList<JobEntry> jobs;
  QVector<int> visibleToSource;

  QLineEdit* searchEdit = nullptr;
  QComboBox* filterCombo = nullptr;
  QComboBox* presetCombo = nullptr;
  QPushButton* savePresetButton = nullptr;
  QPushButton* loadPresetButton = nullptr;
  QPushButton* deletePresetButton = nullptr;
  QListWidget* jobListWidget;
  QPushButton* addButton;
  QPushButton* duplicateButton;
  QPushButton* moveUpButton;
  QPushButton* moveDownButton;
  QPushButton* removeButton;
  QPushButton* clearButton;
  QPushButton* startButton;
  QPushButton* pauseButton;
  QPushButton* cancelButton;
  QPushButton* rerunSelectedButton = nullptr;
  QPushButton* rerunDoneFailedButton = nullptr;
  QProgressBar* totalProgressBar;
  QLabel* summaryLabel;
  QLabel* statusLabel;
  QListWidget* historyListWidget = nullptr;
  QPushButton* clearHistoryButton = nullptr;
  QPushButton* exportHistoryButton = nullptr;
  QComboBox* progressLogStepCombo = nullptr;
  QLineEdit* outputPathEdit = nullptr;
  QPushButton* outputBrowseButton = nullptr;
  QComboBox* outputFormatCombo = nullptr;
  QComboBox* codecCombo = nullptr;
  QSpinBox* outputWidthSpin = nullptr;
  QSpinBox* outputHeightSpin = nullptr;
  QDoubleSpinBox* fpsSpin = nullptr;
  QSpinBox* bitrateSpin = nullptr;
  QSpinBox* startFrameSpin = nullptr;
  QSpinBox* endFrameSpin = nullptr;
  QDoubleSpinBox* overlayXSpin = nullptr;
  QDoubleSpinBox* overlayYSpin = nullptr;
  QDoubleSpinBox* overlayScaleSpin = nullptr;
  QDoubleSpinBox* overlayRotationSpin = nullptr;
  std::unique_ptr<ArtifactCore::FastSettingsStore> historyStore_;
  std::unique_ptr<ArtifactCore::FastSettingsStore> presetStore_;
  bool syncingTransformControls = false;
  bool syncingJobDetails = false;
  bool syncingPresetCombo = false;
  std::map<int, int> lastProgressBucketByJob;
  int progressLogStepPercent = 25;

  static QString normalizeStatus(const QString& status);
  void setStatusMessage(const QString& message, bool alsoHistory = false);
  void logUiEvent(const QString& event, bool alsoHistory = true);
  void logServiceEvent(const QString& event, int sourceIndex = -1, bool alsoHistory = true);
  void loadHistory();
  void saveHistory() const;
  void loadUiPreferences();
  void saveUiPreferences() const;
  void setProgressLogStepPercent(int stepPercent);
  QStringList presetNames() const;
  QString presetKeyForName(const QString& name) const;
  void reloadPresetList();
  bool saveCurrentSelectionAsPreset(const QString& presetName);
  bool applyPresetToSelection(const QString& presetName);
  bool removePreset(const QString& presetName);
  int selectedSourceIndex() const;
  bool statusMatchesFilter(const QString& status) const;
  void updateSummary();
  void addHistoryEntry(const QString& message);
  void addJobSettingsSnapshotToHistory(int sourceIndex);
  void syncJobsFromService();
  void selectSourceIndex(int sourceIndex);
  void updateJobList();
  void handleJobSelected();
  void updateJobDetailEditorsForSelection();
  void setJobDetailEditorsEnabled(bool enabled);
  void updateTransformEditorsForSelection();
  void setTransformEditorsEnabled(bool enabled);
  void handleJobAdded(int index);
  void handleJobRemoved(int index);
  void handleJobUpdated(int index);
  void handleJobStatusChanged(int index, int status);
  void handleJobProgressChanged(int index, int progress);
  void handleProjectClosed();
 };

RenderQueueManagerWidget::Impl::Impl()
{
  service = ArtifactRenderQueueService::instance();
  const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (!appDataDir.isEmpty()) {
    QDir dir(appDataDir);
    if (!dir.exists()) {
      dir.mkpath(QStringLiteral("."));
    }
    const QString historyFile = dir.filePath(QStringLiteral("render_queue_history.cbor"));
    historyStore_ = std::make_unique<ArtifactCore::FastSettingsStore>(historyFile);
    historyStore_->setAutoSyncThreshold(8);
    const QString presetFile = dir.filePath(QStringLiteral("render_queue_presets.cbor"));
    presetStore_ = std::make_unique<ArtifactCore::FastSettingsStore>(presetFile);
    presetStore_->setAutoSyncThreshold(4);
  }
  loadUiPreferences();
}

RenderQueueManagerWidget::Impl::~Impl()
{
  saveUiPreferences();
  saveHistory();
  service = nullptr;
}

QString RenderQueueManagerWidget::Impl::normalizeStatus(const QString& status)
{
  const QString s = status.trimmed().toLower();
  if (s == "rendering" || s == "running") return "Rendering";
  if (s == "canceled" || s == "cancelled") return "Canceled";
  if (s == "completed" || s == "done") return "Completed";
  if (s == "failed" || s == "error") return "Failed";
  if (s == "paused") return "Paused";
  return "Pending";
}

int RenderQueueManagerWidget::Impl::selectedSourceIndex() const
{
  const int currentRow = jobListWidget ? jobListWidget->currentRow() : -1;
  if (currentRow < 0 || !jobListWidget) {
    return -1;
  }
  const auto* item = jobListWidget->item(currentRow);
  if (!item) {
    return -1;
  }
  return item->data(Qt::UserRole).toInt();
}

bool RenderQueueManagerWidget::Impl::statusMatchesFilter(const QString& status) const
{
  if (!filterCombo) return true;
  const QString current = filterCombo->currentText();
  if (current == "All") return true;
  return normalizeStatus(status) == current;
}

void RenderQueueManagerWidget::Impl::updateSummary()
{
  int pending = 0;
  int rendering = 0;
  int completed = 0;
  int failed = 0;
  int canceled = 0;
  int paused = 0;

  for (const auto& job : jobs) {
    const QString status = normalizeStatus(job.status);
    if (status == "Rendering") ++rendering;
    else if (status == "Completed") ++completed;
    else if (status == "Failed") ++failed;
    else if (status == "Canceled") ++canceled;
    else if (status == "Paused") ++paused;
    else ++pending;
  }

  if (summaryLabel) {
    summaryLabel->setText(QString("Jobs: %1  Pending:%2  Rendering:%3  Paused:%4  Done:%5  Failed:%6  Canceled:%7")
      .arg(jobs.size()).arg(pending).arg(rendering).arg(paused).arg(completed).arg(failed).arg(canceled));
  }

  if (startButton) {
    startButton->setEnabled((pending + paused) > 0);
  }
  if (pauseButton) {
    pauseButton->setEnabled(rendering > 0);
  }
  if (cancelButton) {
    cancelButton->setEnabled((rendering + paused + pending) > 0);
  }
}

void RenderQueueManagerWidget::Impl::addHistoryEntry(const QString& message)
{
  if (!historyListWidget || message.trimmed().isEmpty()) {
    return;
  }
  const QString stamp = QDateTime::currentDateTime().toString("HH:mm:ss");
  historyListWidget->addItem(QString("[%1] %2").arg(stamp, message));
  while (historyListWidget->count() > 300) {
    delete historyListWidget->takeItem(0);
  }
  historyListWidget->scrollToBottom();
  saveHistory();
}

void RenderQueueManagerWidget::Impl::setStatusMessage(const QString& message, bool alsoHistory)
{
  if (!statusLabel) {
    return;
  }
  const QString stamp = QDateTime::currentDateTime().toString("HH:mm:ss");
  const QString text = QString("[%1] %2").arg(stamp, message.trimmed());
  statusLabel->setText(text);
  if (alsoHistory) {
    addHistoryEntry(message);
  }
}

void RenderQueueManagerWidget::Impl::logUiEvent(const QString& event, bool alsoHistory)
{
  setStatusMessage(QString("[UI] %1").arg(event), false);
  if (alsoHistory) {
    addHistoryEntry(QString("[UI] %1").arg(event));
  }
}

void RenderQueueManagerWidget::Impl::logServiceEvent(const QString& event, int sourceIndex, bool alsoHistory)
{
  QString suffix;
  if (service && sourceIndex >= 0 && sourceIndex < service->jobCount()) {
    const QString name = service->jobCompositionNameAt(sourceIndex).trimmed();
    suffix = name.isEmpty()
      ? QString(" (Job #%1)").arg(sourceIndex + 1)
      : QString(" (%1)").arg(name);
  } else if (sourceIndex >= 0) {
    suffix = QString(" (Job #%1)").arg(sourceIndex + 1);
  }
  setStatusMessage(QString("[Service] %1%2").arg(event, suffix), false);
  if (alsoHistory) {
    addHistoryEntry(QString("[Service] %1%2").arg(event, suffix));
  }
}

void RenderQueueManagerWidget::Impl::addJobSettingsSnapshotToHistory(int sourceIndex)
{
  if (!service || sourceIndex < 0 || sourceIndex >= service->jobCount()) {
    return;
  }
  QString format;
  QString codec;
  int width = 0;
  int height = 0;
  double fps = 0.0;
  int bitrateKbps = 0;
  int startFrame = 0;
  int endFrame = 0;
  service->jobOutputSettingsAt(sourceIndex, &format, &codec, &width, &height, &fps, &bitrateKbps);
  service->jobFrameRangeAt(sourceIndex, &startFrame, &endFrame);
  const QString jobName = service->jobCompositionNameAt(sourceIndex).trimmed().isEmpty()
    ? QString("Job #%1").arg(sourceIndex + 1)
    : service->jobCompositionNameAt(sourceIndex).trimmed();
  addHistoryEntry(QString("Snapshot: %1 | %2/%3 | %4x%5 @ %6fps | %7 kbps | %8-%9")
    .arg(jobName)
    .arg(format)
    .arg(codec)
    .arg(width)
    .arg(height)
    .arg(QString::number(fps, 'f', 3))
    .arg(bitrateKbps)
    .arg(startFrame)
    .arg(endFrame));
}

void RenderQueueManagerWidget::Impl::loadHistory()
{
  if (!historyListWidget || !historyStore_) {
    return;
  }
  const QStringList lines = historyStore_->value(QStringLiteral("renderQueue/historyLines")).toStringList();
  for (const QString& line : lines) {
    if (!line.trimmed().isEmpty()) {
      historyListWidget->addItem(line);
    }
  }
  while (historyListWidget->count() > 300) {
    delete historyListWidget->takeItem(0);
  }
  historyListWidget->scrollToBottom();
}

void RenderQueueManagerWidget::Impl::saveHistory() const
{
  if (!historyListWidget || !historyStore_) {
    return;
  }
  QStringList lines;
  lines.reserve(historyListWidget->count());
  for (int i = 0; i < historyListWidget->count(); ++i) {
    auto* item = historyListWidget->item(i);
    if (!item) {
      continue;
    }
    const QString text = item->text();
    if (!text.trimmed().isEmpty()) {
      lines.push_back(text);
    }
  }
  historyStore_->setValue(QStringLiteral("renderQueue/historyLines"), lines);
  historyStore_->sync();
}

void RenderQueueManagerWidget::Impl::loadUiPreferences()
{
  if (!historyStore_) {
    progressLogStepPercent = 25;
    return;
  }
  const int stored = historyStore_->value(QStringLiteral("renderQueue/progressLogStepPercent"), 25).toInt();
  setProgressLogStepPercent(stored);
}

void RenderQueueManagerWidget::Impl::saveUiPreferences() const
{
  if (!historyStore_) {
    return;
  }
  historyStore_->setValue(QStringLiteral("renderQueue/progressLogStepPercent"), progressLogStepPercent);
  historyStore_->sync();
}

void RenderQueueManagerWidget::Impl::setProgressLogStepPercent(int stepPercent)
{
  progressLogStepPercent = std::clamp(stepPercent, 5, 100);
}

QStringList RenderQueueManagerWidget::Impl::presetNames() const
{
  if (!presetStore_) {
    return {};
  }
  QStringList names;
  const QStringList keys = presetStore_->keys();
  for (const QString& key : keys) {
    if (!key.startsWith(QStringLiteral("renderQueue/presets/"))) {
      continue;
    }
    const QString encoded = key.mid(QStringLiteral("renderQueue/presets/").size());
    const QString decoded = QString::fromUtf8(QByteArray::fromBase64(encoded.toUtf8()));
    if (!decoded.trimmed().isEmpty()) {
      names.push_back(decoded);
    }
  }
  names.removeDuplicates();
  std::sort(names.begin(), names.end(), [](const QString& a, const QString& b) {
    return QString::localeAwareCompare(a, b) < 0;
  });
  return names;
}

QString RenderQueueManagerWidget::Impl::presetKeyForName(const QString& name) const
{
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }
  const QString encoded = QString::fromUtf8(trimmed.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
  return QStringLiteral("renderQueue/presets/%1").arg(encoded);
}

void RenderQueueManagerWidget::Impl::reloadPresetList()
{
  if (!presetCombo) {
    return;
  }
  syncingPresetCombo = true;
  QSignalBlocker blocker(presetCombo);
  const QString previous = presetCombo->currentText();
  presetCombo->clear();
  const QStringList names = presetNames();
  presetCombo->addItems(names);
  const int idx = presetCombo->findText(previous);
  if (idx >= 0) {
    presetCombo->setCurrentIndex(idx);
  } else if (!names.isEmpty()) {
    presetCombo->setCurrentIndex(0);
  }
  syncingPresetCombo = false;
}

bool RenderQueueManagerWidget::Impl::saveCurrentSelectionAsPreset(const QString& presetName)
{
  if (!presetStore_ || !service) {
    return false;
  }
  const int sourceIndex = selectedSourceIndex();
  if (sourceIndex < 0 || sourceIndex >= service->jobCount()) {
    return false;
  }
  const QString key = presetKeyForName(presetName);
  if (key.isEmpty()) {
    return false;
  }

  QString format;
  QString codec;
  int width = 1920;
  int height = 1080;
  double fps = 30.0;
  int bitrateKbps = 8000;
  int startFrame = 0;
  int endFrame = 100;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  float scale = 1.0f;
  float rotation = 0.0f;

  service->jobOutputSettingsAt(sourceIndex, &format, &codec, &width, &height, &fps, &bitrateKbps);
  service->jobFrameRangeAt(sourceIndex, &startFrame, &endFrame);
  service->jobOverlayTransformAt(sourceIndex, &offsetX, &offsetY, &scale, &rotation);

  QVariantMap map;
  map.insert(QStringLiteral("outputFormat"), format);
  map.insert(QStringLiteral("codec"), codec);
  map.insert(QStringLiteral("width"), width);
  map.insert(QStringLiteral("height"), height);
  map.insert(QStringLiteral("fps"), fps);
  map.insert(QStringLiteral("bitrateKbps"), bitrateKbps);
  map.insert(QStringLiteral("startFrame"), startFrame);
  map.insert(QStringLiteral("endFrame"), endFrame);
  map.insert(QStringLiteral("overlayOffsetX"), static_cast<double>(offsetX));
  map.insert(QStringLiteral("overlayOffsetY"), static_cast<double>(offsetY));
  map.insert(QStringLiteral("overlayScale"), static_cast<double>(scale));
  map.insert(QStringLiteral("overlayRotationDeg"), static_cast<double>(rotation));

  presetStore_->setValue(key, map);
  return presetStore_->sync();
}

bool RenderQueueManagerWidget::Impl::applyPresetToSelection(const QString& presetName)
{
  if (!presetStore_ || !service) {
    return false;
  }
  const int sourceIndex = selectedSourceIndex();
  if (sourceIndex < 0 || sourceIndex >= service->jobCount()) {
    return false;
  }
  const QString key = presetKeyForName(presetName);
  if (key.isEmpty() || !presetStore_->contains(key)) {
    return false;
  }
  const QVariantMap map = presetStore_->value(key).toMap();
  if (map.isEmpty()) {
    return false;
  }

  const QString format = map.value(QStringLiteral("outputFormat"), QStringLiteral("MP4")).toString();
  const QString codec = map.value(QStringLiteral("codec"), QStringLiteral("H.264")).toString();
  const int width = map.value(QStringLiteral("width"), 1920).toInt();
  const int height = map.value(QStringLiteral("height"), 1080).toInt();
  const double fps = map.value(QStringLiteral("fps"), 30.0).toDouble();
  const int bitrateKbps = map.value(QStringLiteral("bitrateKbps"), 8000).toInt();
  const int startFrame = map.value(QStringLiteral("startFrame"), 0).toInt();
  const int endFrame = map.value(QStringLiteral("endFrame"), 100).toInt();
  const float offsetX = static_cast<float>(map.value(QStringLiteral("overlayOffsetX"), 0.0).toDouble());
  const float offsetY = static_cast<float>(map.value(QStringLiteral("overlayOffsetY"), 0.0).toDouble());
  const float scale = static_cast<float>(map.value(QStringLiteral("overlayScale"), 1.0).toDouble());
  const float rotation = static_cast<float>(map.value(QStringLiteral("overlayRotationDeg"), 0.0).toDouble());

  service->setJobOutputSettingsAt(sourceIndex, format, codec, width, height, fps, bitrateKbps);
  service->setJobFrameRangeAt(sourceIndex, startFrame, endFrame);
  service->setJobOverlayTransform(sourceIndex, offsetX, offsetY, scale, rotation);
  syncJobsFromService();
  selectSourceIndex(sourceIndex);
  return true;
}

bool RenderQueueManagerWidget::Impl::removePreset(const QString& presetName)
{
  if (!presetStore_) {
    return false;
  }
  const QString key = presetKeyForName(presetName);
  if (key.isEmpty() || !presetStore_->contains(key)) {
    return false;
  }
  presetStore_->remove(key);
  return presetStore_->sync();
}

void RenderQueueManagerWidget::Impl::syncJobsFromService()
{
  const QList<JobEntry> previousJobs = jobs;
  if (!service) {
    jobs.clear();
    updateJobList();
    return;
  }

  jobs.clear();
  const int count = service->jobCount();
  jobs.reserve(count);
  for (int i = 0; i < count; ++i) {
    JobEntry entry;
    QString name = service->jobCompositionNameAt(i).trimmed();
    const auto compositionId = service->jobCompositionIdAt(i);
    if (!compositionId.isNil()) {
      if (auto* projectService = ArtifactProjectService::instance()) {
        const auto found = projectService->findComposition(compositionId);
        if (found.success) {
          if (const auto composition = found.ptr.lock()) {
            const QString liveName = composition->settings().compositionName().toQString().trimmed();
            if (!liveName.isEmpty()) {
              name = liveName;
            }
          }
        }
      }
    }
    entry.name = name.isEmpty() ? QString("Render Job %1").arg(i + 1) : name;
    entry.status = normalizeStatus(service->jobStatusAt(i));
    entry.progress = std::clamp(service->jobProgressAt(i), 0, 100);
    jobs.push_back(entry);
  }

  if (!previousJobs.isEmpty()) {
    const int common = std::min(previousJobs.size(), jobs.size());
    for (int i = 0; i < common; ++i) {
      const QString prevStatus = normalizeStatus(previousJobs[i].status);
      const QString currStatus = normalizeStatus(jobs[i].status);
      if (prevStatus == currStatus) {
        continue;
      }
      const QString jobName = jobs[i].name;
      if (currStatus == "Completed") {
        const QString path = service->jobOutputPathAt(i).trimmed();
        if (!path.isEmpty()) {
          addHistoryEntry(QString("Completed: %1 -> %2").arg(jobName, path));
        } else {
          addHistoryEntry(QString("Completed: %1").arg(jobName));
        }
      } else if (currStatus == "Failed") {
        const QString err = service->jobErrorMessageAt(i).trimmed();
        if (!err.isEmpty()) {
          addHistoryEntry(QString("Failed: %1 (%2)").arg(jobName, err));
        } else {
          addHistoryEntry(QString("Failed: %1").arg(jobName));
        }
      } else if (currStatus == "Canceled") {
        addHistoryEntry(QString("Canceled: %1").arg(jobName));
      } else if (currStatus == "Rendering" && prevStatus != "Rendering") {
        addHistoryEntry(QString("Started: %1").arg(jobName));
      }
    }

    if (jobs.size() > previousJobs.size()) {
      addHistoryEntry(QString("Queue: %1 job(s) added").arg(jobs.size() - previousJobs.size()));
    } else if (jobs.size() < previousJobs.size()) {
      addHistoryEntry(QString("Queue: %1 job(s) removed").arg(previousJobs.size() - jobs.size()));
    }
  }

  updateJobList();
}

void RenderQueueManagerWidget::Impl::selectSourceIndex(int sourceIndex)
{
  if (!jobListWidget || sourceIndex < 0) {
    return;
  }
  for (int row = 0; row < jobListWidget->count(); ++row) {
    auto* item = jobListWidget->item(row);
    if (item && item->data(Qt::UserRole).toInt() == sourceIndex) {
      jobListWidget->setCurrentRow(row);
      return;
    }
  }
}

 void RenderQueueManagerWidget::Impl::updateJobList()
 {
  const int selectedSourceBefore = selectedSourceIndex();
  visibleToSource.clear();
  const QString needle = searchEdit ? searchEdit->text().trimmed() : QString();
  const bool hasNeedle = !needle.isEmpty();

  QSignalBlocker blocker(jobListWidget);
  jobListWidget->clear();

  int progressSum = 0;
  int progressCount = 0;
  for (int i = 0; i < jobs.size(); ++i) {
    const auto& job = jobs[i];
    const QString status = normalizeStatus(job.status);
    if (!statusMatchesFilter(status)) continue;
    if (hasNeedle && !job.name.contains(needle, Qt::CaseInsensitive)) continue;

    auto* item = new QListWidgetItem(QString("#%1  %2    %3%")
      .arg(i + 1, 2, 10, QChar('0')).arg(job.name).arg(job.progress, 3));
    item->setData(Qt::UserRole, i);
    item->setToolTip(QString("Status: %1\nProgress: %2%\nJob: %3")
      .arg(status).arg(job.progress).arg(job.name));
    item->setSizeHint(QSize(0, 28));
    if (status == "Rendering") {
      item->setForeground(QColor(255, 211, 94));
      item->setBackground(QColor(58, 49, 28));
    } else if (status == "Completed") {
      item->setForeground(QColor(167, 231, 168));
      item->setBackground(QColor(30, 56, 36));
    } else if (status == "Failed") {
      item->setForeground(QColor(255, 165, 165));
      item->setBackground(QColor(64, 31, 31));
    } else if (status == "Paused") {
      item->setForeground(QColor(178, 200, 255));
      item->setBackground(QColor(34, 46, 71));
    } else if (status == "Canceled") {
      item->setForeground(QColor(203, 203, 203));
      item->setBackground(QColor(45, 45, 45));
    } else {
      item->setForeground(QColor(221, 221, 221));
      item->setBackground(QColor(41, 41, 41));
    }
    jobListWidget->addItem(item);
    visibleToSource.push_back(i);

    if (status == "Rendering" || status == "Completed" || status == "Paused") {
      progressSum += job.progress;
      ++progressCount;
    }
  }

  if (selectedSourceBefore >= 0) {
    for (int row = 0; row < jobListWidget->count(); ++row) {
      const auto* item = jobListWidget->item(row);
      if (item && item->data(Qt::UserRole).toInt() == selectedSourceBefore) {
        jobListWidget->setCurrentRow(row);
        break;
      }
    }
  }

  const int totalProgress = progressCount > 0 ? (progressSum / progressCount) : 0;
  totalProgressBar->setValue(totalProgress);
  updateSummary();
  handleJobSelected();
 }

 void RenderQueueManagerWidget::Impl::handleJobSelected()
 {
 const bool hasSelection = jobListWidget->currentRow() >= 0;
 removeButton->setEnabled(hasSelection);
 if (duplicateButton) duplicateButton->setEnabled(hasSelection);
  const int sourceIndex = selectedSourceIndex();
  if (moveUpButton) moveUpButton->setEnabled(hasSelection && sourceIndex > 0);
  if (moveDownButton) moveDownButton->setEnabled(hasSelection && sourceIndex >= 0 && sourceIndex < (jobs.size() - 1));
  setJobDetailEditorsEnabled(hasSelection);
  setTransformEditorsEnabled(hasSelection);
  if (hasSelection) {
    updateJobDetailEditorsForSelection();
    updateTransformEditorsForSelection();
  }
  const QString selectedStatus = (sourceIndex >= 0 && sourceIndex < jobs.size())
    ? normalizeStatus(jobs[sourceIndex].status)
    : QString();
  const bool canRerunSelected = (selectedStatus == "Completed" || selectedStatus == "Failed" || selectedStatus == "Canceled");
  if (rerunSelectedButton) {
    rerunSelectedButton->setEnabled(canRerunSelected);
  }
  bool hasRerunnable = false;
  for (const auto& job : jobs) {
    const QString status = normalizeStatus(job.status);
    if (status == "Completed" || status == "Failed") {
      hasRerunnable = true;
      break;
    }
  }
  if (rerunDoneFailedButton) {
    rerunDoneFailedButton->setEnabled(hasRerunnable);
  }
  const bool hasPreset = presetCombo && presetCombo->count() > 0 && !presetCombo->currentText().trimmed().isEmpty();
  if (savePresetButton) {
    savePresetButton->setEnabled(hasSelection);
  }
  if (loadPresetButton) {
    loadPresetButton->setEnabled(hasSelection && hasPreset);
  }
  if (deletePresetButton) {
    deletePresetButton->setEnabled(hasPreset);
  }
 }

void RenderQueueManagerWidget::Impl::setJobDetailEditorsEnabled(bool enabled)
{
  if (outputPathEdit) outputPathEdit->setEnabled(enabled);
  if (outputBrowseButton) outputBrowseButton->setEnabled(enabled);
  if (outputFormatCombo) outputFormatCombo->setEnabled(enabled);
  if (codecCombo) codecCombo->setEnabled(enabled);
  if (outputWidthSpin) outputWidthSpin->setEnabled(enabled);
  if (outputHeightSpin) outputHeightSpin->setEnabled(enabled);
  if (fpsSpin) fpsSpin->setEnabled(enabled);
  if (bitrateSpin) bitrateSpin->setEnabled(enabled);
  if (startFrameSpin) startFrameSpin->setEnabled(enabled);
  if (endFrameSpin) endFrameSpin->setEnabled(enabled);
}

void RenderQueueManagerWidget::Impl::updateJobDetailEditorsForSelection()
{
  if (!service || syncingJobDetails) {
    return;
  }
  const int sourceIndex = selectedSourceIndex();
  if (sourceIndex < 0) {
    return;
  }
  syncingJobDetails = true;
  if (outputPathEdit) {
    QSignalBlocker block(outputPathEdit);
    outputPathEdit->setText(service->jobOutputPathAt(sourceIndex));
  }
  QString outputFormat;
  QString codec;
  int width = 1920;
  int height = 1080;
  double fps = 30.0;
  int bitrateKbps = 8000;
  if (service->jobOutputSettingsAt(sourceIndex, &outputFormat, &codec, &width, &height, &fps, &bitrateKbps)) {
    if (outputFormatCombo) {
      QSignalBlocker block(outputFormatCombo);
      int idx = outputFormatCombo->findText(outputFormat);
      if (idx < 0) {
        outputFormatCombo->addItem(outputFormat);
        idx = outputFormatCombo->findText(outputFormat);
      }
      outputFormatCombo->setCurrentIndex(std::max(0, idx));
    }
    if (codecCombo) {
      QSignalBlocker block(codecCombo);
      int idx = codecCombo->findText(codec);
      if (idx < 0) {
        codecCombo->addItem(codec);
        idx = codecCombo->findText(codec);
      }
      codecCombo->setCurrentIndex(std::max(0, idx));
    }
    if (outputWidthSpin) {
      QSignalBlocker block(outputWidthSpin);
      outputWidthSpin->setValue(width);
    }
    if (outputHeightSpin) {
      QSignalBlocker block(outputHeightSpin);
      outputHeightSpin->setValue(height);
    }
    if (fpsSpin) {
      QSignalBlocker block(fpsSpin);
      fpsSpin->setValue(fps);
    }
    if (bitrateSpin) {
      QSignalBlocker block(bitrateSpin);
      bitrateSpin->setValue(bitrateKbps);
    }
  }
  int startFrame = 0;
  int endFrame = 100;
  if (service->jobFrameRangeAt(sourceIndex, &startFrame, &endFrame)) {
    if (startFrameSpin) {
      QSignalBlocker block(startFrameSpin);
      startFrameSpin->setValue(startFrame);
    }
    if (endFrameSpin) {
      QSignalBlocker block(endFrameSpin);
      endFrameSpin->setValue(endFrame);
    }
  }
  syncingJobDetails = false;
}

void RenderQueueManagerWidget::Impl::setTransformEditorsEnabled(bool enabled)
{
  if (overlayXSpin) overlayXSpin->setEnabled(enabled);
  if (overlayYSpin) overlayYSpin->setEnabled(enabled);
  if (overlayScaleSpin) overlayScaleSpin->setEnabled(enabled);
  if (overlayRotationSpin) overlayRotationSpin->setEnabled(enabled);
}

void RenderQueueManagerWidget::Impl::updateTransformEditorsForSelection()
{
  if (!service || syncingTransformControls) {
    return;
  }
  const int sourceIndex = selectedSourceIndex();
  if (sourceIndex < 0) {
    return;
  }
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  float scale = 1.0f;
  float rotation = 0.0f;
  if (!service->jobOverlayTransformAt(sourceIndex, &offsetX, &offsetY, &scale, &rotation)) {
    return;
  }
  syncingTransformControls = true;
  if (overlayXSpin) {
    QSignalBlocker block(overlayXSpin);
    overlayXSpin->setValue(static_cast<double>(offsetX));
  }
  if (overlayYSpin) {
    QSignalBlocker block(overlayYSpin);
    overlayYSpin->setValue(static_cast<double>(offsetY));
  }
  if (overlayScaleSpin) {
    QSignalBlocker block(overlayScaleSpin);
    overlayScaleSpin->setValue(static_cast<double>(scale));
  }
  if (overlayRotationSpin) {
    QSignalBlocker block(overlayRotationSpin);
    overlayRotationSpin->setValue(static_cast<double>(rotation));
  }
  syncingTransformControls = false;
}

 void RenderQueueManagerWidget::Impl::handleJobAdded(int index)
 {
  logServiceEvent(QStringLiteral("Queue updated: job added"), index, true);
  Q_UNUSED(index);
  syncJobsFromService();
 }

 void RenderQueueManagerWidget::Impl::handleJobRemoved(int index)
 {
  logServiceEvent(QStringLiteral("Queue updated: job removed"), index, true);
  Q_UNUSED(index);
  syncJobsFromService();
 }

 void RenderQueueManagerWidget::Impl::handleJobUpdated(int index)
 {
  if (service) {
    const QString status = normalizeStatus(service->jobStatusAt(index));
    if (status == "Completed") {
      const QString path = service->jobOutputPathAt(index).trimmed();
      if (!path.isEmpty() && statusLabel) {
        logServiceEvent(QString("Completed: %1").arg(path), index, true);
      }
    } else if (status == "Failed") {
      const QString error = service->jobErrorMessageAt(index).trimmed();
      if (!error.isEmpty() && statusLabel) {
        logServiceEvent(QString("Failed: %1").arg(error), index, true);
      }
    } else {
      logServiceEvent(QStringLiteral("Job updated"), index, false);
    }
  }
  syncJobsFromService();
 }

 void RenderQueueManagerWidget::Impl::handleJobStatusChanged(int index, int status)
 {
  Q_UNUSED(status);
  if (service) {
    const QString st = normalizeStatus(service->jobStatusAt(index));
    logServiceEvent(QString("Status changed -> %1").arg(st), index, true);
  }
  syncJobsFromService();
 }

 void RenderQueueManagerWidget::Impl::handleJobProgressChanged(int index, int progress)
 {
 const int safeProgress = std::clamp(progress, 0, 100);
  const int step = std::max(5, progressLogStepPercent);
  const int bucket = safeProgress / step;
  auto it = lastProgressBucketByJob.find(index);
  if (it == lastProgressBucketByJob.end() || it->second != bucket || safeProgress == 100) {
    lastProgressBucketByJob[index] = bucket;
    if (service && index >= 0 && index < service->jobCount()) {
      const QString st = normalizeStatus(service->jobStatusAt(index));
      if (st == "Rendering" || st == "Completed") {
        logServiceEvent(QString("Progress %1%").arg(safeProgress), index, true);
      }
    }
  }
  if (service) {
    syncJobsFromService();
    return;
  }
  updateJobList();
 }

void RenderQueueManagerWidget::Impl::handleProjectClosed()
{
  logServiceEvent(QStringLiteral("Queue cleared: project closed"), -1, true);
  lastProgressBucketByJob.clear();
  jobs.clear();
  updateJobList();
}

 RenderQueueManagerWidget::RenderQueueManagerWidget(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl())
 {
 auto mainLayout = new QVBoxLayout(this);
 mainLayout->setContentsMargins(8, 8, 8, 8);
 mainLayout->setSpacing(6);

 auto* titleLabel = new QLabel("Render Queue", this);
 titleLabel->setObjectName("renderQueueTitle");
 mainLayout->addWidget(titleLabel);

  auto topControls = new QHBoxLayout();
  impl_->searchEdit = new QLineEdit(this);
  impl_->searchEdit->setPlaceholderText("Search jobs...");
  impl_->searchEdit->setObjectName("renderQueueSearch");
  impl_->filterCombo = new QComboBox(this);
  impl_->filterCombo->setObjectName("renderQueueFilter");
  impl_->filterCombo->addItems(QStringList{
    "All", "Pending", "Rendering", "Paused", "Completed", "Failed", "Canceled"
  });
  topControls->addWidget(impl_->searchEdit, 1);
  topControls->addWidget(impl_->filterCombo, 0);
  mainLayout->addLayout(topControls);

  auto presetControls = new QHBoxLayout();
  auto* presetLabel = new QLabel("Preset", this);
  impl_->presetCombo = new QComboBox(this);
  impl_->savePresetButton = new QPushButton("Save Preset", this);
  impl_->loadPresetButton = new QPushButton("Load Preset", this);
  impl_->deletePresetButton = new QPushButton("Delete Preset", this);
  presetControls->addWidget(presetLabel, 0);
  presetControls->addWidget(impl_->presetCombo, 1);
  presetControls->addWidget(impl_->savePresetButton, 0);
  presetControls->addWidget(impl_->loadPresetButton, 0);
  presetControls->addWidget(impl_->deletePresetButton, 0);
  mainLayout->addLayout(presetControls);

  // ジョブリスト
  impl_->jobListWidget = new QListWidget(this);
  impl_->jobListWidget->setObjectName("renderQueueList");
  impl_->jobListWidget->setAlternatingRowColors(false);
  impl_->jobListWidget->setUniformItemSizes(true);
  mainLayout->addWidget(impl_->jobListWidget);

  auto* ioLayout = new QFormLayout();
  auto* outputPathRow = new QHBoxLayout();
  impl_->outputPathEdit = new QLineEdit(this);
  impl_->outputBrowseButton = new QPushButton("...", this);
  impl_->outputBrowseButton->setFixedWidth(28);
  outputPathRow->addWidget(impl_->outputPathEdit, 1);
  outputPathRow->addWidget(impl_->outputBrowseButton, 0);
  ioLayout->addRow("Output", outputPathRow);

  impl_->outputFormatCombo = new QComboBox(this);
  impl_->outputFormatCombo->addItems(QStringList{
    "MP4", "PNG Sequence", "EXR Sequence"
  });
  ioLayout->addRow("Format", impl_->outputFormatCombo);

  impl_->codecCombo = new QComboBox(this);
  impl_->codecCombo->addItems(QStringList{
    "H.264", "H.265", "ProRes", "PNG", "EXR"
  });
  ioLayout->addRow("Codec", impl_->codecCombo);

  auto* resolutionRow = new QHBoxLayout();
  impl_->outputWidthSpin = new QSpinBox(this);
  impl_->outputHeightSpin = new QSpinBox(this);
  impl_->outputWidthSpin->setRange(16, 16384);
  impl_->outputHeightSpin->setRange(16, 16384);
  impl_->outputWidthSpin->setValue(1920);
  impl_->outputHeightSpin->setValue(1080);
  impl_->outputWidthSpin->setSingleStep(8);
  impl_->outputHeightSpin->setSingleStep(8);
  resolutionRow->addWidget(impl_->outputWidthSpin, 1);
  resolutionRow->addWidget(new QLabel("x", this));
  resolutionRow->addWidget(impl_->outputHeightSpin, 1);
  ioLayout->addRow("Resolution", resolutionRow);

  impl_->fpsSpin = new QDoubleSpinBox(this);
  impl_->fpsSpin->setRange(1.0, 240.0);
  impl_->fpsSpin->setDecimals(3);
  impl_->fpsSpin->setSingleStep(0.5);
  impl_->fpsSpin->setValue(30.0);
  ioLayout->addRow("FPS", impl_->fpsSpin);

  impl_->bitrateSpin = new QSpinBox(this);
  impl_->bitrateSpin->setRange(128, 200000);
  impl_->bitrateSpin->setSingleStep(100);
  impl_->bitrateSpin->setValue(8000);
  ioLayout->addRow("Bitrate", impl_->bitrateSpin);

  impl_->startFrameSpin = new QSpinBox(this);
  impl_->endFrameSpin = new QSpinBox(this);
  impl_->startFrameSpin->setRange(0, 2000000);
  impl_->endFrameSpin->setRange(0, 2000000);
  ioLayout->addRow("Start", impl_->startFrameSpin);
  ioLayout->addRow("End", impl_->endFrameSpin);
  mainLayout->addLayout(ioLayout);

  auto* transformLayout = new QFormLayout();
  impl_->overlayXSpin = new QDoubleSpinBox(this);
  impl_->overlayYSpin = new QDoubleSpinBox(this);
  impl_->overlayScaleSpin = new QDoubleSpinBox(this);
  impl_->overlayRotationSpin = new QDoubleSpinBox(this);

  impl_->overlayXSpin->setRange(-8192.0, 8192.0);
  impl_->overlayYSpin->setRange(-8192.0, 8192.0);
  impl_->overlayScaleSpin->setRange(0.05, 8.0);
  impl_->overlayRotationSpin->setRange(-3600.0, 3600.0);

  impl_->overlayXSpin->setDecimals(1);
  impl_->overlayYSpin->setDecimals(1);
  impl_->overlayScaleSpin->setDecimals(3);
  impl_->overlayRotationSpin->setDecimals(1);

  impl_->overlayXSpin->setSingleStep(1.0);
  impl_->overlayYSpin->setSingleStep(1.0);
  impl_->overlayScaleSpin->setSingleStep(0.05);
  impl_->overlayRotationSpin->setSingleStep(1.0);

  transformLayout->addRow("Overlay X", impl_->overlayXSpin);
  transformLayout->addRow("Overlay Y", impl_->overlayYSpin);
  transformLayout->addRow("Overlay Scale", impl_->overlayScaleSpin);
  transformLayout->addRow("Overlay Rot", impl_->overlayRotationSpin);
  mainLayout->addLayout(transformLayout);

  // コントロールボタン
  auto controlLayout = new QHBoxLayout();
  impl_->addButton = new QPushButton("Add to Queue", this);
  impl_->duplicateButton = new QPushButton("Duplicate", this);
  impl_->moveUpButton = new QPushButton("Up", this);
  impl_->moveDownButton = new QPushButton("Down", this);
  impl_->removeButton = new QPushButton("Remove", this);
  impl_->clearButton = new QPushButton("Clear All", this);
  impl_->startButton = new QPushButton("Render", this);
  impl_->pauseButton = new QPushButton("Pause", this);
  impl_->cancelButton = new QPushButton("Stop", this);
  impl_->rerunSelectedButton = new QPushButton("Rerun Selected", this);
  impl_->rerunDoneFailedButton = new QPushButton("Rerun Done/Failed", this);

  controlLayout->addWidget(impl_->addButton);
  controlLayout->addWidget(impl_->duplicateButton);
  controlLayout->addWidget(impl_->moveUpButton);
  controlLayout->addWidget(impl_->moveDownButton);
  controlLayout->addWidget(impl_->removeButton);
  controlLayout->addWidget(impl_->clearButton);
  controlLayout->addWidget(impl_->startButton);
  controlLayout->addWidget(impl_->pauseButton);
  controlLayout->addWidget(impl_->cancelButton);
  controlLayout->addWidget(impl_->rerunSelectedButton);
  controlLayout->addWidget(impl_->rerunDoneFailedButton);

  mainLayout->addLayout(controlLayout);

  // 進捗表示
  impl_->totalProgressBar = new QProgressBar(this);
  impl_->totalProgressBar->setRange(0, 100);
  impl_->totalProgressBar->setValue(0);
  mainLayout->addWidget(impl_->totalProgressBar);

  impl_->summaryLabel = new QLabel("Jobs: 0", this);
  mainLayout->addWidget(impl_->summaryLabel);

  impl_->statusLabel = new QLabel("Ready", this);
  mainLayout->addWidget(impl_->statusLabel);

  auto* historyHeader = new QHBoxLayout();
  auto* historyLabel = new QLabel("History", this);
  auto* progressLogLabel = new QLabel("Progress Log Step", this);
  impl_->progressLogStepCombo = new QComboBox(this);
  impl_->progressLogStepCombo->addItem("10%", 10);
  impl_->progressLogStepCombo->addItem("25%", 25);
  impl_->progressLogStepCombo->addItem("50%", 50);
  impl_->progressLogStepCombo->addItem("100%", 100);
  impl_->exportHistoryButton = new QPushButton("Export History", this);
  impl_->clearHistoryButton = new QPushButton("Clear History", this);
  historyHeader->addWidget(historyLabel, 0);
  historyHeader->addStretch(1);
  historyHeader->addWidget(progressLogLabel, 0);
  historyHeader->addWidget(impl_->progressLogStepCombo, 0);
  historyHeader->addWidget(impl_->exportHistoryButton, 0);
  historyHeader->addWidget(impl_->clearHistoryButton, 0);
  mainLayout->addLayout(historyHeader);

  impl_->historyListWidget = new QListWidget(this);
  impl_->historyListWidget->setMinimumHeight(110);
  mainLayout->addWidget(impl_->historyListWidget);
  impl_->loadHistory();
  if (impl_->progressLogStepCombo) {
    QSignalBlocker block(impl_->progressLogStepCombo);
    const int idx = impl_->progressLogStepCombo->findData(impl_->progressLogStepPercent);
    impl_->progressLogStepCombo->setCurrentIndex(idx >= 0 ? idx : 1);
  }

  // スタイルの設定
  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);
  style += QStringLiteral(R"(
QLabel#renderQueueTitle {
  font-size: 12px;
  font-weight: 700;
  color: #d7d7d7;
  padding: 2px 2px 4px 2px;
}
QLineEdit#renderQueueSearch, QComboBox#renderQueueFilter {
  min-height: 22px;
  border: 1px solid #3a3a3a;
  background: #242424;
  color: #dfdfdf;
  border-radius: 3px;
  padding: 2px 6px;
}
QListWidget#renderQueueList {
  border: 1px solid #393939;
  background: #1d1d1d;
  outline: none;
}
QListWidget#renderQueueList::item {
  border: 1px solid #2d2d2d;
  margin: 2px 2px;
  padding: 4px 8px;
  border-radius: 3px;
}
QListWidget#renderQueueList::item:selected {
  border: 1px solid #4e8fd6;
  background: #223247;
}
QPushButton {
  min-height: 22px;
}
)");
  setStyleSheet(style);

  connect(impl_->searchEdit, &QLineEdit::textChanged, this, [this](const QString&) {
    impl_->updateJobList();
  });
  connect(impl_->filterCombo, &QComboBox::currentTextChanged, this, [this](const QString&) {
    impl_->updateJobList();
  });
  connect(impl_->presetCombo, &QComboBox::currentTextChanged, this, [this](const QString&) {
    impl_->handleJobSelected();
  });

  connect(impl_->savePresetButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->service || impl_->selectedSourceIndex() < 0) {
      return;
    }
    bool ok = false;
    const QString suggested = impl_->presetCombo ? impl_->presetCombo->currentText() : QString();
    const QString name = QInputDialog::getText(
      this,
      QStringLiteral("Save Render Preset"),
      QStringLiteral("Preset name:"),
      QLineEdit::Normal,
      suggested,
      &ok);
    if (!ok || name.trimmed().isEmpty()) {
      return;
    }
    if (impl_->presetStore_) {
      const QString key = impl_->presetKeyForName(name.trimmed());
      if (!key.isEmpty() && impl_->presetStore_->contains(key)) {
        const auto overwrite = QMessageBox::question(
          this,
          QStringLiteral("Overwrite Preset"),
          QStringLiteral("Preset \"%1\" already exists. Overwrite?").arg(name.trimmed()),
          QMessageBox::Yes | QMessageBox::No,
          QMessageBox::No);
        if (overwrite != QMessageBox::Yes) {
          return;
        }
      }
    }
    if (!impl_->saveCurrentSelectionAsPreset(name.trimmed())) {
      QMessageBox::warning(this, QStringLiteral("Preset"), QStringLiteral("Failed to save preset."));
      return;
    }
    impl_->reloadPresetList();
    if (impl_->presetCombo) {
      const int idx = impl_->presetCombo->findText(name.trimmed());
      if (idx >= 0) {
        impl_->presetCombo->setCurrentIndex(idx);
      }
    }
    impl_->logUiEvent(QString("Preset saved: %1").arg(name.trimmed()), true);
  });

  connect(impl_->loadPresetButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->presetCombo || impl_->presetCombo->currentText().trimmed().isEmpty()) {
      return;
    }
    const QString name = impl_->presetCombo->currentText().trimmed();
    if (!impl_->applyPresetToSelection(name)) {
      QMessageBox::warning(this, QStringLiteral("Preset"), QStringLiteral("Failed to apply preset."));
      return;
    }
    impl_->logUiEvent(QString("Preset applied: %1").arg(name), true);
  });

  connect(impl_->deletePresetButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->presetCombo || impl_->presetCombo->currentText().trimmed().isEmpty()) {
      return;
    }
    const QString name = impl_->presetCombo->currentText().trimmed();
    const auto answer = QMessageBox::question(
      this,
      QStringLiteral("Delete Preset"),
      QStringLiteral("Delete preset \"%1\"?").arg(name),
      QMessageBox::Yes | QMessageBox::No,
      QMessageBox::No);
    if (answer != QMessageBox::Yes) {
      return;
    }
    if (!impl_->removePreset(name)) {
      QMessageBox::warning(this, QStringLiteral("Preset"), QStringLiteral("Failed to delete preset."));
      return;
    }
    impl_->reloadPresetList();
    impl_->logUiEvent(QString("Preset deleted: %1").arg(name), true);
  });

  connect(impl_->addButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) {
      impl_->service->addRenderQueue();
      impl_->logUiEvent(QStringLiteral("Requested: add render job"), true);
    }
  });

  connect(impl_->removeButton, &QPushButton::clicked, this, [this]() {
    int selectedIndex = impl_->jobListWidget->currentRow();
    if (selectedIndex != -1) {
      const int sourceIndex = impl_->jobListWidget->item(selectedIndex)->data(Qt::UserRole).toInt();
      if (impl_->service) {
        impl_->service->removeRenderQueueAt(sourceIndex);
      }
      impl_->logUiEvent(QString("Requested: remove job #%1").arg(sourceIndex + 1), true);
    }
  });

  connect(impl_->duplicateButton, &QPushButton::clicked, this, [this]() {
    const int sourceIndex = impl_->selectedSourceIndex();
    if (sourceIndex < 0 || !impl_->service) {
      return;
    }
    impl_->service->duplicateRenderQueueAt(sourceIndex);
    impl_->syncJobsFromService();
    impl_->selectSourceIndex(sourceIndex + 1);
    impl_->logUiEvent(QString("Requested: duplicate job #%1").arg(sourceIndex + 1), true);
  });

  connect(impl_->moveUpButton, &QPushButton::clicked, this, [this]() {
    const int sourceIndex = impl_->selectedSourceIndex();
    if (sourceIndex <= 0 || !impl_->service) {
      return;
    }
    impl_->service->moveRenderQueue(sourceIndex, sourceIndex - 1);
    impl_->logUiEvent(QString("Requested: move job #%1 up").arg(sourceIndex + 1), true);
  });

  connect(impl_->moveDownButton, &QPushButton::clicked, this, [this]() {
    const int sourceIndex = impl_->selectedSourceIndex();
    if (sourceIndex < 0 || sourceIndex >= (impl_->jobs.size() - 1) || !impl_->service) {
      return;
    }
    impl_->service->moveRenderQueue(sourceIndex, sourceIndex + 1);
    impl_->logUiEvent(QString("Requested: move job #%1 down").arg(sourceIndex + 1), true);
  });

  connect(impl_->clearButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->service || impl_->service->jobCount() <= 0) {
      return;
    }
    const auto answer = QMessageBox::question(
      this,
      QStringLiteral("Clear Render Queue"),
      QStringLiteral("レンダーキュー内の全ジョブを削除しますか？"),
      QMessageBox::Yes | QMessageBox::No,
      QMessageBox::No);
    if (answer != QMessageBox::Yes) {
      impl_->logUiEvent(QStringLiteral("Requested: clear all render jobs (canceled)"), true);
      return;
    }
    if (impl_->service) {
      impl_->service->removeAllRenderQueues();
    }
    impl_->syncJobsFromService();
    impl_->logUiEvent(QStringLiteral("Requested: clear all render jobs"), true);
  });

  connect(impl_->startButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) {
      for (int i = 0; i < impl_->service->jobCount(); ++i) {
        impl_->addJobSettingsSnapshotToHistory(i);
      }
      impl_->service->startAllJobs();
      impl_->logUiEvent(QStringLiteral("Requested: start all jobs"), true);
    } else {
      impl_->logUiEvent(QStringLiteral("Render service unavailable"), true);
    }
  });

  connect(impl_->pauseButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) {
      impl_->service->pauseAllJobs();
      impl_->logUiEvent(QStringLiteral("Requested: pause all jobs"), true);
    } else {
      impl_->logUiEvent(QStringLiteral("Render service unavailable"), true);
    }
  });

  connect(impl_->cancelButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) {
      impl_->service->cancelAllJobs();
      impl_->logUiEvent(QStringLiteral("Requested: cancel all jobs"), true);
    } else {
      impl_->logUiEvent(QStringLiteral("Render service unavailable"), true);
    }
  });

  connect(impl_->rerunSelectedButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->service) {
      return;
    }
    const int sourceIndex = impl_->selectedSourceIndex();
    if (sourceIndex < 0) {
      return;
    }
    impl_->service->resetJobForRerun(sourceIndex);
    impl_->syncJobsFromService();
    impl_->selectSourceIndex(sourceIndex);
    impl_->logUiEvent(QString("Requested: rerun reset job #%1").arg(sourceIndex + 1), true);
  });

  connect(impl_->rerunDoneFailedButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->service) {
      return;
    }
    const int changed = impl_->service->resetCompletedAndFailedJobsForRerun();
    impl_->syncJobsFromService();
    impl_->logUiEvent(QString("Requested: rerun reset batch (%1 jobs)").arg(changed), true);
  });

  connect(impl_->clearHistoryButton, &QPushButton::clicked, this, [this]() {
    if (impl_->historyListWidget) {
      impl_->historyListWidget->clear();
      impl_->saveHistory();
    }
  });

  connect(impl_->progressLogStepCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
    if (!impl_->progressLogStepCombo) {
      return;
    }
    const int step = impl_->progressLogStepCombo->currentData().toInt();
    impl_->setProgressLogStepPercent(step);
    impl_->lastProgressBucketByJob.clear();
    impl_->saveUiPreferences();
    impl_->logUiEvent(QString("Progress log step set to %1%").arg(impl_->progressLogStepPercent), true);
  });

  connect(impl_->exportHistoryButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->historyListWidget || impl_->historyListWidget->count() == 0) {
      impl_->logUiEvent(QStringLiteral("History export skipped: no entries"), true);
      return;
    }
    const QString defaultName = QString("render_queue_history_%1.log")
      .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    const QString defaultPath = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(defaultName);
    const QString path = QFileDialog::getSaveFileName(
      this,
      QStringLiteral("Export Render Queue History"),
      defaultPath,
      QStringLiteral("Log Files (*.log);;Text Files (*.txt);;All Files (*)"));
    if (path.trimmed().isEmpty()) {
      return;
    }
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
      QMessageBox::warning(this, QStringLiteral("Export History"), QStringLiteral("Failed to open file for writing."));
      impl_->logUiEvent(QString("History export failed: %1").arg(path), true);
      return;
    }
    QTextStream ts(&out);
    for (int i = 0; i < impl_->historyListWidget->count(); ++i) {
      if (auto* item = impl_->historyListWidget->item(i)) {
        ts << item->text() << '\n';
      }
    }
    out.close();
    impl_->logUiEvent(QString("History exported: %1").arg(path), true);
  });

  connect(impl_->jobListWidget, &QListWidget::currentRowChanged, this, [this](int row) {
    Q_UNUSED(row);
    impl_->handleJobSelected();
  });

  connect(impl_->outputPathEdit, &QLineEdit::editingFinished, this, [this]() {
    if (!impl_->service || impl_->syncingJobDetails) {
      return;
    }
    const int sourceIndex = impl_->selectedSourceIndex();
    if (sourceIndex < 0) {
      return;
    }
    impl_->service->setJobOutputPathAt(sourceIndex, impl_->outputPathEdit->text());
  });

  connect(impl_->outputBrowseButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->service || !impl_->outputPathEdit) {
      return;
    }
    const int sourceIndex = impl_->selectedSourceIndex();
    if (sourceIndex < 0) {
      return;
    }
    const QString initial = impl_->outputPathEdit->text().trimmed();
    const QString path = QFileDialog::getSaveFileName(
      this,
      QStringLiteral("Select Render Output"),
      initial.isEmpty() ? QStringLiteral("render.png") : initial,
      QStringLiteral("PNG Image (*.png);;All Files (*.*)")
    );
    if (path.isEmpty()) {
      return;
    }
    impl_->outputPathEdit->setText(path);
    impl_->service->setJobOutputPathAt(sourceIndex, path);
    impl_->logUiEvent(QString("Output updated for job #%1").arg(sourceIndex + 1), true);
  });

  auto applyOutputSettings = [this]() {
    if (!impl_->service || impl_->syncingJobDetails) {
      return;
    }
    const int sourceIndex = impl_->selectedSourceIndex();
    if (sourceIndex < 0) {
      return;
    }
    const QString format = impl_->outputFormatCombo ? impl_->outputFormatCombo->currentText() : QStringLiteral("MP4");
    const QString codec = impl_->codecCombo ? impl_->codecCombo->currentText() : QStringLiteral("H.264");
    const int width = impl_->outputWidthSpin ? impl_->outputWidthSpin->value() : 1920;
    const int height = impl_->outputHeightSpin ? impl_->outputHeightSpin->value() : 1080;
    const double fps = impl_->fpsSpin ? impl_->fpsSpin->value() : 30.0;
    const int bitrateKbps = impl_->bitrateSpin ? impl_->bitrateSpin->value() : 8000;
    impl_->service->setJobOutputSettingsAt(sourceIndex, format, codec, width, height, fps, bitrateKbps);
  };

  connect(impl_->outputFormatCombo, &QComboBox::currentTextChanged, this, [applyOutputSettings](const QString&) {
    applyOutputSettings();
  });
  connect(impl_->codecCombo, &QComboBox::currentTextChanged, this, [applyOutputSettings](const QString&) {
    applyOutputSettings();
  });
  connect(impl_->outputWidthSpin, qOverload<int>(&QSpinBox::valueChanged), this, [applyOutputSettings](int) {
    applyOutputSettings();
  });
  connect(impl_->outputHeightSpin, qOverload<int>(&QSpinBox::valueChanged), this, [applyOutputSettings](int) {
    applyOutputSettings();
  });
  connect(impl_->fpsSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyOutputSettings](double) {
    applyOutputSettings();
  });
  connect(impl_->bitrateSpin, qOverload<int>(&QSpinBox::valueChanged), this, [applyOutputSettings](int) {
    applyOutputSettings();
  });

  auto applyFrameRange = [this]() {
    if (!impl_->service || impl_->syncingJobDetails) {
      return;
    }
    const int sourceIndex = impl_->selectedSourceIndex();
    if (sourceIndex < 0 || !impl_->startFrameSpin || !impl_->endFrameSpin) {
      return;
    }
    const int startFrame = impl_->startFrameSpin->value();
    const int endFrame = std::max(startFrame, impl_->endFrameSpin->value());
    if (endFrame != impl_->endFrameSpin->value()) {
      QSignalBlocker block(impl_->endFrameSpin);
      impl_->endFrameSpin->setValue(endFrame);
    }
    impl_->service->setJobFrameRangeAt(sourceIndex, startFrame, endFrame);
  };

  connect(impl_->startFrameSpin, qOverload<int>(&QSpinBox::valueChanged), this, [applyFrameRange](int) {
    applyFrameRange();
  });
  connect(impl_->endFrameSpin, qOverload<int>(&QSpinBox::valueChanged), this, [applyFrameRange](int) {
    applyFrameRange();
  });

  auto applyOverlayTransform = [this]() {
    if (!impl_->service || impl_->syncingTransformControls) {
      return;
    }
    const int sourceIndex = impl_->selectedSourceIndex();
    if (sourceIndex < 0) {
      return;
    }
    impl_->service->setJobOverlayTransform(
      sourceIndex,
      static_cast<float>(impl_->overlayXSpin ? impl_->overlayXSpin->value() : 0.0),
      static_cast<float>(impl_->overlayYSpin ? impl_->overlayYSpin->value() : 0.0),
      static_cast<float>(impl_->overlayScaleSpin ? impl_->overlayScaleSpin->value() : 1.0),
      static_cast<float>(impl_->overlayRotationSpin ? impl_->overlayRotationSpin->value() : 0.0)
    );
  };

  connect(impl_->overlayXSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyOverlayTransform](double) {
    applyOverlayTransform();
  });
  connect(impl_->overlayYSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyOverlayTransform](double) {
    applyOverlayTransform();
  });
  connect(impl_->overlayScaleSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyOverlayTransform](double) {
    applyOverlayTransform();
  });
  connect(impl_->overlayRotationSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyOverlayTransform](double) {
    applyOverlayTransform();
  });

  if (impl_->service) {
    impl_->service->setJobAddedCallback([this](int index) { impl_->handleJobAdded(index); });
    impl_->service->setJobRemovedCallback([this](int index) { impl_->handleJobRemoved(index); });
    impl_->service->setJobUpdatedCallback([this](int index) { impl_->handleJobUpdated(index); });
    impl_->service->setJobStatusChangedCallback([this](int index, int status) { impl_->handleJobStatusChanged(index, status); });
    impl_->service->setJobProgressChangedCallback([this](int index, int progress) { impl_->handleJobProgressChanged(index, progress); });
    impl_->service->setAllJobsRemovedCallback([this]() { impl_->handleProjectClosed(); });
    impl_->service->setQueueReorderedCallback([this](int from, int to) {
        impl_->syncJobsFromService();
        impl_->selectSourceIndex(to);
    });
  }
  
  // Initialize UI
  impl_->reloadPresetList();
  impl_->syncJobsFromService();
  impl_->setJobDetailEditorsEnabled(false);
  impl_->setTransformEditorsEnabled(false);
  impl_->duplicateButton->setEnabled(false);
  impl_->moveUpButton->setEnabled(false);
  impl_->moveDownButton->setEnabled(false);
  impl_->rerunSelectedButton->setEnabled(false);
  impl_->rerunDoneFailedButton->setEnabled(false);
  impl_->savePresetButton->setEnabled(false);
  impl_->loadPresetButton->setEnabled(false);
  impl_->deletePresetButton->setEnabled(impl_->presetCombo && impl_->presetCombo->count() > 0);

  // 初期状態でRemoveボタンを無効化
  impl_->removeButton->setEnabled(false);
 }

 RenderQueueManagerWidget::~RenderQueueManagerWidget()
 {
  if (impl_ && impl_->service) {
    impl_->service->setJobAddedCallback({});
    impl_->service->setJobRemovedCallback({});
    impl_->service->setJobUpdatedCallback({});
    impl_->service->setJobStatusChangedCallback({});
    impl_->service->setJobProgressChangedCallback({});
    impl_->service->setAllJobsRemovedCallback({});
    impl_->service->setQueueReorderedCallback({});
  }
  delete impl_;
 }
 QSize RenderQueueManagerWidget::sizeHint() const
 {
  return QSize(600, 600);
 }

 void RenderQueueManagerWidget::setFloatingMode(const bool isFloating)
 {
  if (isFloating) {
    setWindowFlag(Qt::Window, true);
    resize(760, 520);
  } else {
    setWindowFlag(Qt::Window, false);
  }
  show();
 }


};
