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
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QShortcut>
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
  bool syncingTransformControls = false;
  bool syncingJobDetails = false;

  static QString normalizeStatus(const QString& status);
  void loadHistory();
  void saveHistory() const;
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
  }
}

RenderQueueManagerWidget::Impl::~Impl()
{
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
    const QString name = service->jobCompositionNameAt(i).trimmed();
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

    auto* item = new QListWidgetItem(QString("#%1  %2  [%3]  %4%")
      .arg(i + 1).arg(job.name).arg(status).arg(job.progress));
    item->setData(Qt::UserRole, i);
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
  statusLabel->setText(QString("Total Progress: %1%").arg(totalProgress));
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
  Q_UNUSED(index);
  syncJobsFromService();
 }

 void RenderQueueManagerWidget::Impl::handleJobRemoved(int index)
 {
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
        statusLabel->setText(QString("Completed: %1").arg(path));
      }
    } else if (status == "Failed") {
      const QString error = service->jobErrorMessageAt(index).trimmed();
      if (!error.isEmpty() && statusLabel) {
        statusLabel->setText(QString("Failed: %1").arg(error));
      }
    }
  }
  syncJobsFromService();
 }

 void RenderQueueManagerWidget::Impl::handleJobStatusChanged(int index, int status)
 {
  Q_UNUSED(status);
  if (service) {
    const QString st = normalizeStatus(service->jobStatusAt(index));
    if (statusLabel) {
      statusLabel->setText(QString("Job #%1: %2").arg(index + 1).arg(st));
    }
  }
  syncJobsFromService();
 }

 void RenderQueueManagerWidget::Impl::handleJobProgressChanged(int index, int progress)
 {
  Q_UNUSED(index);
  Q_UNUSED(progress);
  if (service) {
    syncJobsFromService();
    return;
  }
  updateJobList();
 }

 void RenderQueueManagerWidget::Impl::handleProjectClosed()
 {
  jobs.clear();
  updateJobList();
 }

 RenderQueueManagerWidget::RenderQueueManagerWidget(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl())
 {
 auto mainLayout = new QVBoxLayout(this);

  auto topControls = new QHBoxLayout();
  impl_->searchEdit = new QLineEdit(this);
  impl_->searchEdit->setPlaceholderText("Search jobs...");
  impl_->filterCombo = new QComboBox(this);
  impl_->filterCombo->addItems(QStringList{
    "All", "Pending", "Rendering", "Paused", "Completed", "Failed", "Canceled"
  });
  topControls->addWidget(impl_->searchEdit, 1);
  topControls->addWidget(impl_->filterCombo, 0);
  mainLayout->addLayout(topControls);

  // ジョブリスト
  impl_->jobListWidget = new QListWidget(this);
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
  impl_->addButton = new QPushButton("Add", this);
  impl_->duplicateButton = new QPushButton("Duplicate", this);
  impl_->moveUpButton = new QPushButton("Up", this);
  impl_->moveDownButton = new QPushButton("Down", this);
  impl_->removeButton = new QPushButton("Remove", this);
  impl_->clearButton = new QPushButton("Clear All", this);
  impl_->startButton = new QPushButton("Start", this);
  impl_->pauseButton = new QPushButton("Pause", this);
  impl_->cancelButton = new QPushButton("Cancel", this);
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
  impl_->clearHistoryButton = new QPushButton("Clear History", this);
  historyHeader->addWidget(historyLabel, 0);
  historyHeader->addStretch(1);
  historyHeader->addWidget(impl_->clearHistoryButton, 0);
  mainLayout->addLayout(historyHeader);

  impl_->historyListWidget = new QListWidget(this);
  impl_->historyListWidget->setMinimumHeight(110);
  mainLayout->addWidget(impl_->historyListWidget);
  impl_->loadHistory();

  // スタイルの設定
  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);
  setStyleSheet(style);

  connect(impl_->searchEdit, &QLineEdit::textChanged, this, [this](const QString&) {
    impl_->updateJobList();
  });
  connect(impl_->filterCombo, &QComboBox::currentTextChanged, this, [this](const QString&) {
    impl_->updateJobList();
  });

  connect(impl_->addButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) {
      impl_->service->addRenderQueue();
      impl_->statusLabel->setText("Added render job");
    }
  });

  connect(impl_->removeButton, &QPushButton::clicked, this, [this]() {
    int selectedIndex = impl_->jobListWidget->currentRow();
    if (selectedIndex != -1) {
      const int sourceIndex = impl_->jobListWidget->item(selectedIndex)->data(Qt::UserRole).toInt();
      if (impl_->service) {
        impl_->service->removeRenderQueueAt(sourceIndex);
      }
      impl_->statusLabel->setText("Removed selected render job");
      impl_->addHistoryEntry(QString("Removed job #%1").arg(sourceIndex + 1));
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
    impl_->statusLabel->setText("Duplicated selected render job");
    impl_->addHistoryEntry(QString("Duplicated job #%1").arg(sourceIndex + 1));
  });

  connect(impl_->moveUpButton, &QPushButton::clicked, this, [this]() {
    const int sourceIndex = impl_->selectedSourceIndex();
    if (sourceIndex <= 0 || !impl_->service) {
      return;
    }
    impl_->service->moveRenderQueue(sourceIndex, sourceIndex - 1);
    impl_->syncJobsFromService();
    impl_->selectSourceIndex(sourceIndex - 1);
    impl_->statusLabel->setText("Moved job up");
    impl_->addHistoryEntry(QString("Moved job #%1 up").arg(sourceIndex + 1));
  });

  connect(impl_->moveDownButton, &QPushButton::clicked, this, [this]() {
    const int sourceIndex = impl_->selectedSourceIndex();
    if (sourceIndex < 0 || sourceIndex >= (impl_->jobs.size() - 1) || !impl_->service) {
      return;
    }
    impl_->service->moveRenderQueue(sourceIndex, sourceIndex + 1);
    impl_->syncJobsFromService();
    impl_->selectSourceIndex(sourceIndex + 1);
    impl_->statusLabel->setText("Moved job down");
    impl_->addHistoryEntry(QString("Moved job #%1 down").arg(sourceIndex + 1));
  });

  connect(impl_->clearButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) {
      impl_->service->removeAllRenderQueues();
    }
    impl_->syncJobsFromService();
    impl_->statusLabel->setText("Cleared all render jobs");
    impl_->addHistoryEntry("Cleared all render jobs");
  });

  connect(impl_->startButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) {
      for (int i = 0; i < impl_->service->jobCount(); ++i) {
        impl_->addJobSettingsSnapshotToHistory(i);
      }
      impl_->service->startAllJobs();
      impl_->statusLabel->setText("Started all jobs");
      impl_->addHistoryEntry("Start all jobs");
    } else {
      impl_->statusLabel->setText("Render service unavailable");
    }
  });

  connect(impl_->pauseButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) {
      impl_->service->pauseAllJobs();
      impl_->statusLabel->setText("Paused rendering jobs");
      impl_->addHistoryEntry("Paused rendering jobs");
    } else {
      impl_->statusLabel->setText("Render service unavailable");
    }
  });

  connect(impl_->cancelButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) {
      impl_->service->cancelAllJobs();
      impl_->statusLabel->setText("Canceled all jobs");
      impl_->addHistoryEntry("Canceled all jobs");
    } else {
      impl_->statusLabel->setText("Render service unavailable");
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
    impl_->statusLabel->setText(QString("Reset job #%1 for rerun").arg(sourceIndex + 1));
    impl_->addHistoryEntry(QString("Rerun reset: job #%1").arg(sourceIndex + 1));
  });

  connect(impl_->rerunDoneFailedButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->service) {
      return;
    }
    const int changed = impl_->service->resetCompletedAndFailedJobsForRerun();
    impl_->syncJobsFromService();
    impl_->statusLabel->setText(QString("Reset %1 completed/failed job(s)").arg(changed));
    impl_->addHistoryEntry(QString("Rerun reset batch: %1 job(s)").arg(changed));
  });

  connect(impl_->clearHistoryButton, &QPushButton::clicked, this, [this]() {
    if (impl_->historyListWidget) {
      impl_->historyListWidget->clear();
      impl_->saveHistory();
    }
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
    impl_->statusLabel->setText(QString("Output set: %1").arg(path));
    impl_->addHistoryEntry(QString("Output updated for job #%1").arg(sourceIndex + 1));
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
  }
  
  // Initialize UI
  impl_->syncJobsFromService();
  impl_->setJobDetailEditorsEnabled(false);
  impl_->setTransformEditorsEnabled(false);
  impl_->duplicateButton->setEnabled(false);
  impl_->moveUpButton->setEnabled(false);
  impl_->moveDownButton->setEnabled(false);
  impl_->rerunSelectedButton->setEnabled(false);
  impl_->rerunDoneFailedButton->setEnabled(false);

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
