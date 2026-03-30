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
#include <QSplitter>
#include <QScrollArea>
#include <QGroupBox>
#include <QApplication>
#include <QMenu>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QToolButton>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include <wobjectimpl.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <memory>

module Artifact.Widgets.Render.QueueManager;

import Widgets.Utils.CSS;
import Artifact.Render.Queue.Service;
import Artifact.Render.Queue.Presets;
import Artifact.Service.Project;
import Artifact.Widget.Dialog.RenderOutputSetting;
import Artifact.Widgets.RenderQueuePresetSelector;
import Core.FastSettingsStore;
import Artifact.Widgets.AppDialogs;
import Utils.Path;
import Utils.ExplorerUtils;

namespace Artifact
{
 using namespace ArtifactCore;

 namespace {
 QIcon loadIconWithFallback(const QString& fileName)
 {
   const QString resourcePath = ArtifactCore::resolveIconResourcePath(fileName);
   QIcon icon(resourcePath);
   if (!icon.isNull()) {
     return icon;
   }
   return QIcon(ArtifactCore::resolveIconPath(fileName));
 }
 }

 W_OBJECT_IMPL(RenderQueueManagerWidget)

 class RenderQueueManagerWidget::Impl
 {
 public:
  struct JobEntry {
    QString name;
    QString status;
    QString errorMessage;
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
  QListWidget* jobListWidget = nullptr;
  QPushButton* addButton = nullptr;
  QToolButton* duplicateButton = nullptr;
  QToolButton* moveUpButton = nullptr;
  QToolButton* moveDownButton = nullptr;
  QPushButton* removeButton = nullptr;
  QPushButton* clearButton = nullptr;
  QPushButton* startButton = nullptr;
  QPushButton* pauseButton = nullptr;
  QPushButton* cancelButton = nullptr;
  QPushButton* rerunSelectedButton = nullptr;
  QPushButton* rerunDoneFailedButton = nullptr;
  QProgressBar* totalProgressBar = nullptr;
  QLabel* summaryLabel = nullptr;
  QLabel* statusLabel = nullptr;
  QListWidget* historyListWidget = nullptr;
  QPushButton* clearHistoryButton = nullptr;
   QPushButton* exportHistoryButton = nullptr;
   QLabel* previewLabel = nullptr;
  QComboBox* progressLogStepCombo = nullptr;
  QLineEdit* outputPathEdit = nullptr;
  QPushButton* outputBrowseButton = nullptr;
  QLabel* outputSettingsSummaryLabel = nullptr;
  QPushButton* outputSettingsButton = nullptr;
  QLabel* errorLabel = nullptr;
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

  Impl() {
    service = ArtifactRenderQueueService::instance();
    const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appDataDir.isEmpty()) {
      QDir dir(appDataDir);
      if (!dir.exists()) dir.mkpath(".");
      historyStore_ = std::make_unique<ArtifactCore::FastSettingsStore>(dir.filePath("render_queue_history.cbor"));
      presetStore_ = std::make_unique<ArtifactCore::FastSettingsStore>(dir.filePath("render_queue_presets.cbor"));
    }
  }

  ~Impl() {
    saveHistory();
  }

  static QString normalizeStatus(const QString& status) {
    const QString s = status.trimmed().toLower();
    if (s == "rendering" || s == "running") return "Rendering";
    if (s == "completed" || s == "done") return "Completed";
    if (s == "failed" || s == "error") return "Failed";
    if (s == "paused") return "Paused";
    if (s == "canceled" || s == "cancelled") return "Canceled";
    return "Pending";
  }

  void logUiEvent(const QString& event, bool alsoHistory = true) {
    if (statusLabel) statusLabel->setText(QString("[%1] [UI] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), event));
    if (alsoHistory) addHistoryEntry(QString("[UI] %1").arg(event));
  }

  void logServiceEvent(const QString& event, int sourceIndex = -1, bool alsoHistory = true) {
    QString msg = event;
    if (service && sourceIndex >= 0 && sourceIndex < service->jobCount()) {
        msg += QString(" (%1)").arg(service->jobCompositionNameAt(sourceIndex));
    }
    if (statusLabel) statusLabel->setText(QString("[%1] [Service] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), msg));
    if (alsoHistory) addHistoryEntry(QString("[Service] %1").arg(msg));
  }

  void addHistoryEntry(const QString& message) {
    if (!historyListWidget) return;
    historyListWidget->addItem(QString("[%1] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), message));
    while (historyListWidget->count() > 300) delete historyListWidget->takeItem(0);
    historyListWidget->scrollToBottom();
  }

  void saveHistory() const {
    if (!historyListWidget || !historyStore_) return;
    QStringList lines;
    for (int i = 0; i < historyListWidget->count(); ++i) lines << historyListWidget->item(i)->text();
    historyStore_->setValue("historyLines", lines);
    historyStore_->sync();
  }

  void loadHistory() {
    if (!historyListWidget || !historyStore_) return;
    const QStringList lines = historyStore_->value("historyLines").toStringList();
    for (const auto& line : lines) historyListWidget->addItem(line);
    historyListWidget->scrollToBottom();
  }

  int selectedSourceIndex() const {
    if (!jobListWidget) return -1;
    auto* item = jobListWidget->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : -1;
  }

  void syncJobsFromService() {
    if (!service) return;
    jobs.clear();
    for (int i = 0; i < service->jobCount(); ++i) {
      JobEntry e;
      e.name = service->jobCompositionNameAt(i);
      e.status = service->jobStatusAt(i);
      e.progress = service->jobProgressAt(i);
      e.errorMessage = service->jobErrorMessageAt(i);
      jobs.append(e);
    }
    updateJobList();
  }

  void updateJobList() {
    if (!jobListWidget) return;
    QSignalBlocker blocker(jobListWidget);
    jobListWidget->clear();
    visibleToSource.clear();

    QFont fixedFont("Consolas", 10);
    for (int i = 0; i < jobs.size(); ++i) {
      const auto& job = jobs[i];
      QString statusTag = "[WAIT ]";
      QColor textColor(160, 160, 160);
      QString status = normalizeStatus(job.status);
      
      if (status == "Rendering") { statusTag = "[RUN  ]"; textColor = QColor(0, 210, 255); }
      else if (status == "Completed") { statusTag = "[DONE ]"; textColor = QColor(100, 220, 100); }
      else if (status == "Failed") { statusTag = "[ERROR]"; textColor = QColor(255, 80, 80); }

      QString progressBar = "..........";
      int filled = std::clamp(job.progress / 10, 0, 10);
      for(int p=0; p<filled; ++p) progressBar[p] = (status == "Rendering") ? '>' : '#';

      QString line = QString("%1  #%2  %-25s  [%3]  %4%")
        .arg(statusTag).arg(i+1, 2, 10, QChar('0')).arg(job.name.left(25)).arg(progressBar).arg(job.progress, 3);

      auto* item = new QListWidgetItem(line);
      item->setData(Qt::UserRole, i);
      item->setFont(fixedFont);
      item->setForeground(textColor);
      jobListWidget->addItem(item);
      visibleToSource.push_back(i);
    }
    updateSummary();
  }

  void updateSummary() {
    if (!summaryLabel) return;
    int done = 0, running = 0;
    for (const auto& j : jobs) {
        QString s = normalizeStatus(j.status);
        if (s == "Completed") done++;
        else if (s == "Rendering") running++;
    }
    summaryLabel->setText(QString("Jobs: %1 | Running: %2 | Done: %3").arg(jobs.size()).arg(running).arg(done));
    if (startButton) startButton->setEnabled(!jobs.isEmpty());
  }

  void handleJobSelected() {
    bool has = selectedSourceIndex() >= 0;
    if (removeButton) removeButton->setEnabled(has);
    if (duplicateButton) duplicateButton->setEnabled(has);
  }
 };

 RenderQueueManagerWidget::RenderQueueManagerWidget(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  auto* layout = new QVBoxLayout(this);
  
  // Header
  auto* top = new QHBoxLayout();
  auto* title = new QLabel("RENDER QUEUE");
  title->setObjectName("renderQueueTitle");
  impl_->searchEdit = new QLineEdit();
  impl_->searchEdit->setPlaceholderText("Search...");
  impl_->searchEdit->setObjectName("renderQueueSearch");
  top->addWidget(title);
  top->addWidget(impl_->searchEdit, 1);
  layout->addLayout(top);

  // Main Splitter
  auto* splitter = new QSplitter(Qt::Horizontal);
  impl_->jobListWidget = new QListWidget();
  impl_->jobListWidget->setObjectName("renderQueueList");
  impl_->jobListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  
  auto* leftSide = new QWidget();
  auto* leftLayout = new QVBoxLayout(leftSide);
  leftLayout->addWidget(impl_->jobListWidget);
  
  auto* btnLayout = new QHBoxLayout();
  impl_->addButton = new QPushButton("Add");
  impl_->removeButton = new QPushButton("Remove");
  impl_->duplicateButton = new QToolButton();
  impl_->duplicateButton->setText("D");
  btnLayout->addWidget(impl_->addButton);
  btnLayout->addWidget(impl_->duplicateButton);
  btnLayout->addStretch();
  btnLayout->addWidget(impl_->removeButton);
  leftLayout->addLayout(btnLayout);
  
  splitter->addWidget(leftSide);
  
  // Right Pane: Job Details (Scrollable)
  auto* detailScroll = new QScrollArea();
  detailScroll->setWidgetResizable(true);
  detailScroll->setObjectName("renderQueueDetailScroll");
  auto* detailWidget = new QWidget();
  auto* detailLayout = new QVBoxLayout(detailWidget);
  detailLayout->setContentsMargins(8, 0, 8, 0);
  detailLayout->setSpacing(12);

  // Group: Output
  auto* outputGroup = new QGroupBox("Output Settings");
  auto* outputLayout = new QFormLayout(outputGroup);
  impl_->outputPathEdit = new QLineEdit();
  outputLayout->addRow("Path:", impl_->outputPathEdit);
  impl_->outputSettingsButton = new QPushButton("Format...");
  outputLayout->addRow("Settings:", impl_->outputSettingsButton);
  detailLayout->addWidget(outputGroup);

  // Group: Range
  auto* rangeGroup = new QGroupBox("Frame Range");
  auto* rangeLayout = new QFormLayout(rangeGroup);
  impl_->startFrameSpin = new QSpinBox();
  impl_->endFrameSpin = new QSpinBox();
  impl_->startFrameSpin->setRange(0, 1000000);
  impl_->endFrameSpin->setRange(0, 1000000);
  rangeLayout->addRow("Start:", impl_->startFrameSpin);
  rangeLayout->addRow("End:", impl_->endFrameSpin);
  detailLayout->addWidget(rangeGroup);

  // Group: Overlay
  auto* overlayGroup = new QGroupBox("Overlay Transform");
  auto* overlayLayout = new QFormLayout(overlayGroup);
  impl_->overlayXSpin = new QDoubleSpinBox();
  impl_->overlayYSpin = new QDoubleSpinBox();
  impl_->overlayScaleSpin = new QDoubleSpinBox();
  impl_->overlayScaleSpin->setValue(1.0);
  overlayLayout->addRow("X Offset:", impl_->overlayXSpin);
  overlayLayout->addRow("Y Offset:", impl_->overlayYSpin);
  overlayLayout->addRow("Scale:", impl_->overlayScaleSpin);
  detailLayout->addWidget(overlayGroup);

  detailLayout->addStretch();
  detailScroll->setWidget(detailWidget);
  splitter->addWidget(detailScroll);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);
  layout->addWidget(splitter, 1);

  // Live preview
  impl_->previewLabel = new QLabel("No preview");
  impl_->previewLabel->setFixedSize(320, 180);
  impl_->previewLabel->setStyleSheet("QLabel { background: #1a1a1a; border: 1px solid #333; }");
  impl_->previewLabel->setAlignment(Qt::AlignCenter);
  impl_->previewLabel->setScaledContents(false);
  layout->addWidget(impl_->previewLabel);

  // Bottom
  impl_->totalProgressBar = new QProgressBar();
  impl_->summaryLabel = new QLabel("Ready");
  impl_->statusLabel = new QLabel("No active jobs");
  impl_->startButton = new QPushButton("START RENDER");
  impl_->startButton->setObjectName("renderStartBtn");
  
  layout->addWidget(impl_->totalProgressBar);
  layout->addWidget(impl_->summaryLabel);
  layout->addWidget(impl_->statusLabel);
  layout->addWidget(impl_->startButton);

  // Context Menu
  connect(impl_->jobListWidget, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
    int idx = impl_->selectedSourceIndex();
    if (idx < 0 || !impl_->service) return;
    QMenu menu(this);
    QString path = impl_->service->jobOutputPathAt(idx);
    auto* reveal = menu.addAction("Reveal in Explorer");
    auto* open = menu.addAction("Open File");
    auto* act = menu.exec(impl_->jobListWidget->mapToGlobal(pos));
    if (act == reveal) ArtifactCore::openInExplorer(path, true);
    else if (act == open) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  });

  // Services
  if (impl_->service) {
    connect(impl_->service, &ArtifactRenderQueueService::jobProgressChanged, this, [this](int i, int p) {
        impl_->syncJobsFromService();
    });
    connect(impl_->service, &ArtifactRenderQueueService::allJobsCompleted, this, [this]() {
#ifdef _WIN32
        ::MessageBeep(MB_OK);
#else
        QApplication::beep();
#endif
        impl_->logUiEvent("All jobs completed");
    });
    connect(impl_->service, &ArtifactRenderQueueService::previewFrameReady, this, [this](int jobIndex, int frameNumber) {
        QImage frame = impl_->service->lastRenderedFrame();
        if (!frame.isNull() && impl_->previewLabel) {
            impl_->previewLabel->setPixmap(QPixmap::fromImage(frame));
            impl_->previewLabel->setToolTip(QString("Job %1 | Frame %2").arg(jobIndex + 1).arg(frameNumber));
        }
    });
  }

  connect(impl_->startButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) impl_->service->startAllJobs();
  });

  impl_->syncJobsFromService();
 }

 RenderQueueManagerWidget::~RenderQueueManagerWidget() { delete impl_; }
 QSize RenderQueueManagerWidget::sizeHint() const { return QSize(800, 600); }
 void RenderQueueManagerWidget::setFloatingMode(bool f) { setWindowFlag(Qt::Window, f); show(); }
 void RenderQueueManagerWidget::showEvent(QShowEvent* e) { QWidget::showEvent(e); impl_->syncJobsFromService(); }
}
