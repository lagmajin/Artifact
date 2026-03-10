module;
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
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
  QPushButton* removeButton;
  QPushButton* clearButton;
  QPushButton* startButton;
  QPushButton* pauseButton;
  QPushButton* cancelButton;
  QProgressBar* totalProgressBar;
  QLabel* summaryLabel;
  QLabel* statusLabel;

  static QString normalizeStatus(const QString& status);
  bool statusMatchesFilter(const QString& status) const;
  void updateSummary();
  void updateJobList();
  void handleJobSelected();
  void handleJobAdded(int index);
  void handleJobRemoved(int index);
  void handleJobUpdated(int index);
  void handleJobStatusChanged(int index, int status);
  void handleJobProgressChanged(int index, int progress);
  void handleProjectClosed();
 };

RenderQueueManagerWidget::Impl::Impl()
{
  service = new ArtifactRenderQueueService();
}

RenderQueueManagerWidget::Impl::~Impl()
{
  delete service;
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
}

 void RenderQueueManagerWidget::Impl::updateJobList()
 {
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
 }

 void RenderQueueManagerWidget::Impl::handleJobAdded(int index)
 {
  const int insertIndex = std::clamp(index, 0, jobs.size());
  JobEntry entry;
  entry.name = QString("Render Job %1").arg(insertIndex + 1);
  entry.status = "Pending";
  entry.progress = 0;
  jobs.insert(insertIndex, entry);
  updateJobList();
 }

 void RenderQueueManagerWidget::Impl::handleJobRemoved(int index)
 {
  if (index >= 0 && index < jobs.size()) {
    jobs.removeAt(index);
  }
  updateJobList();
 }

 void RenderQueueManagerWidget::Impl::handleJobUpdated(int index)
 {
  updateJobList();
 }

 void RenderQueueManagerWidget::Impl::handleJobStatusChanged(int index, int status)
 {
  Q_UNUSED(status);
  if (index >= 0 && index < jobs.size()) {
    jobs[index].status = "Rendering";
  }
  updateJobList();
 }

 void RenderQueueManagerWidget::Impl::handleJobProgressChanged(int index, int progress)
 {
  if (index >= 0 && index < jobs.size()) {
    jobs[index].progress = std::clamp(progress, 0, 100);
    if (jobs[index].progress >= 100) {
      jobs[index].status = "Completed";
    } else if (jobs[index].progress > 0 && jobs[index].status == "Pending") {
      jobs[index].status = "Rendering";
    }
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

  // コントロールボタン
  auto controlLayout = new QHBoxLayout();
  impl_->addButton = new QPushButton("Add", this);
  impl_->removeButton = new QPushButton("Remove", this);
  impl_->clearButton = new QPushButton("Clear All", this);
  impl_->startButton = new QPushButton("Start", this);
  impl_->pauseButton = new QPushButton("Pause", this);
  impl_->cancelButton = new QPushButton("Cancel", this);

  controlLayout->addWidget(impl_->addButton);
  controlLayout->addWidget(impl_->removeButton);
  controlLayout->addWidget(impl_->clearButton);
  controlLayout->addWidget(impl_->startButton);
  controlLayout->addWidget(impl_->pauseButton);
  controlLayout->addWidget(impl_->cancelButton);

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
      if (sourceIndex >= 0 && sourceIndex < impl_->jobs.size()) {
        impl_->jobs.removeAt(sourceIndex);
        impl_->updateJobList();
      }
      if (impl_->service) {
        // Current service API has no indexed removal; keep behavior best-effort.
        impl_->service->removeRenderQueue();
      }
      impl_->statusLabel->setText("Removed selected row (service-side indexed remove is not available yet)");
    }
  });

  connect(impl_->clearButton, &QPushButton::clicked, this, [this]() {
    impl_->jobs.clear();
    impl_->updateJobList();
    if (impl_->service) {
      impl_->service->removeAllRenderQueues();
    }
    impl_->statusLabel->setText("Cleared all render jobs");
  });

  connect(impl_->startButton, &QPushButton::clicked, this, [this]() {
    for (auto& job : impl_->jobs) {
      if (Impl::normalizeStatus(job.status) == "Pending" || Impl::normalizeStatus(job.status) == "Paused") {
        job.status = "Rendering";
      }
    }
    impl_->updateJobList();
    if (impl_->service) {
      impl_->service->startAllJobs();
    }
    impl_->statusLabel->setText("Started all jobs");
  });

  connect(impl_->pauseButton, &QPushButton::clicked, this, [this]() {
    for (auto& job : impl_->jobs) {
      if (Impl::normalizeStatus(job.status) == "Rendering") {
        job.status = "Paused";
      }
    }
    impl_->updateJobList();
    if (impl_->service) {
      impl_->service->pauseAllJobs();
    }
    impl_->statusLabel->setText("Paused rendering jobs");
  });

  connect(impl_->cancelButton, &QPushButton::clicked, this, [this]() {
    for (auto& job : impl_->jobs) {
      const QString normalized = Impl::normalizeStatus(job.status);
      if (normalized == "Rendering" || normalized == "Paused" || normalized == "Pending") {
        job.status = "Canceled";
      }
    }
    impl_->updateJobList();
    if (impl_->service) {
      impl_->service->cancelAllJobs();
    }
    impl_->statusLabel->setText("Canceled all jobs");
  });

  connect(impl_->jobListWidget, &QListWidget::currentRowChanged, this, [this](int row) {
    impl_->removeButton->setEnabled(row != -1);
  });

  if (impl_->service) {
    impl_->service->setJobAddedCallback([this](int index) { impl_->handleJobAdded(index); });
    impl_->service->setJobRemovedCallback([this](int index) { impl_->handleJobRemoved(index); });
    impl_->service->setJobUpdatedCallback([this](int index) { impl_->handleJobUpdated(index); });
    impl_->service->setJobProgressChangedCallback([this](int index, int progress) { impl_->handleJobProgressChanged(index, progress); });
    impl_->service->setAllJobsRemovedCallback([this]() { impl_->handleProjectClosed(); });
  }
  
  // Initialize UI
  impl_->updateJobList();

  // 初期状態でRemoveボタンを無効化
  impl_->removeButton->setEnabled(false);
 }

 RenderQueueManagerWidget::~RenderQueueManagerWidget()
 {
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
