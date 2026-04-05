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
#include <QDialog>
#include <QFrame>
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
import Artifact.Event.Types;
import Event.Bus;
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
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

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
  QSpinBox* skipFrameSpin = nullptr;
  QComboBox* outputFormatCombo = nullptr;
  QComboBox* outputPresetCombo = nullptr;
  QComboBox* qualityCombo = nullptr;
  QComboBox* threadsCombo = nullptr;
  QComboBox* gpuCombo = nullptr;
  QLabel* jobsHeaderLabel = nullptr;
  QLabel* statusBarLabel = nullptr;
  QPushButton* stopButton = nullptr;
  // kept for service sync (not displayed, but read from service)
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

  // v2 preview / playback controls
  QPushButton*  previewToggleButton    = nullptr;
  QLabel*       previewFrameLabel      = nullptr;
  QProgressBar* previewScrubBar        = nullptr;
  QPushButton*  firstFrameButton       = nullptr;
  QPushButton*  prevFrameButton        = nullptr;
  QPushButton*  playButton             = nullptr;
  QPushButton*  nextFrameButton        = nullptr;
  QPushButton*  lastFrameButton        = nullptr;
  QLabel*       frameCountLabel        = nullptr;
  QLabel*       playbackTimecodeLabel  = nullptr;
  QComboBox*    previewResCombo        = nullptr;
  QComboBox*    previewIntervalCombo   = nullptr;
  QComboBox*    previewColorCombo      = nullptr;
  bool previewEnabled          = true;
  int  previewIntervalFrames   = 10;
  int  lastPreviewUpdateFrame  = -999;

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
    if (statusBarLabel) statusBarLabel->setText(QString("[%1] [UI] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), event));
    if (alsoHistory) addHistoryEntry(QString("[UI] %1").arg(event));
  }

  void logServiceEvent(const QString& event, int sourceIndex = -1, bool alsoHistory = true) {
    QString msg = event;
    if (service && sourceIndex >= 0 && sourceIndex < service->jobCount()) {
        msg += QString(" (%1)").arg(service->jobCompositionNameAt(sourceIndex));
    }
    if (statusBarLabel) statusBarLabel->setText(QString("[%1] [Service] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), msg));
    if (alsoHistory) addHistoryEntry(QString("[Service] %1").arg(msg));
  }

  void addHistoryEntry(const QString& message) {
    if (!historyListWidget) return;
    historyListWidget->addItem(QString("[%1] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), message));
    while (historyListWidget->count() > 300) delete historyListWidget->takeItem(0);
    historyListWidget->scrollToBottom();
    saveHistory();
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
    const int selectedSource = selectedSourceIndex();
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
    if (jobListWidget && selectedSource >= 0 && selectedSource < jobListWidget->count()) {
      jobListWidget->setCurrentRow(selectedSource);
    }
  }

  void postQueueChanged(const QString& reason = QString()) {
    eventBus_.post<RenderQueueChangedEvent>(RenderQueueChangedEvent{
        service ? service->jobCount() : 0,
        selectedSourceIndex(),
        reason
    });
    eventBus_.drain();
  }

  void postHistoryMessage(const QString& message, int sourceIndex = -1, bool alsoHistory = true) {
    eventBus_.post<RenderQueueLogEvent>(RenderQueueLogEvent{message, sourceIndex, alsoHistory});
    eventBus_.drain();
  }

  void updateJobList() {
    if (!jobListWidget) return;
    QSignalBlocker blocker(jobListWidget);
    jobListWidget->clear();
    visibleToSource.clear();

    QFont nameFont("Segoe UI", 10);
    QFont subFont("Segoe UI", 9);
    for (int i = 0; i < jobs.size(); ++i) {
      const auto& job = jobs[i];
      QString statusTag = "[WAIT]";
      QColor textColor(140, 140, 140);
      QString statusJp = u8"待機";
      const QString status = normalizeStatus(job.status);

      if (status == "Rendering") {
        statusTag = "[RUN ]"; textColor = QColor(0, 210, 255); statusJp = u8"実行中";
      } else if (status == "Completed") {
        statusTag = "[DON ]"; textColor = QColor(80, 200, 80); statusJp = u8"完了";
      } else if (status == "Failed") {
        statusTag = "[ERR ]"; textColor = QColor(255, 80, 80); statusJp = u8"失敗";
      } else if (status == "Canceled") {
        statusTag = "[CNCL]"; textColor = QColor(160, 100, 60); statusJp = u8"中止";
      }

      int startF = 0, endF = 0;
      if (service) service->jobFrameRangeAt(i, &startF, &endF);
      const int totalF = std::max(0, endF - startF + 1);

      const QString line1 = QString("%1  #%2  %3").arg(statusTag).arg(i+1, 2, 10, QChar('0')).arg(job.name.left(30));
      const QString line2 = QString("     %1F  ·  %2%  %3").arg(totalF).arg(job.progress, 3).arg(statusJp);

      auto* item = new QListWidgetItem(line1 + '\n' + line2);
      item->setData(Qt::UserRole, i);
      item->setFont(nameFont);
      item->setForeground(textColor);
      item->setSizeHint(QSize(0, 46));
      jobListWidget->addItem(item);
      visibleToSource.push_back(i);
    }
    updateSummary();
  }

  void updateSummary() {
    if (!summaryLabel) return;
    int done = 0, running = 0;
    int totalProgress = 0;
    for (const auto& j : jobs) {
        QString s = normalizeStatus(j.status);
        if (s == "Completed") done++;
        else if (s == "Rendering") running++;
        totalProgress += std::clamp(j.progress, 0, 100);
    }
    summaryLabel->setText(QString(u8"Jobs: %1 | Running: %2 | Done: %3").arg(jobs.size()).arg(running).arg(done));
    const int avgPct = jobs.isEmpty() ? 0 : totalProgress / jobs.size();
    if (totalProgressBar) {
      totalProgressBar->setRange(0, 100);
      totalProgressBar->setValue(avgPct);
    }
    if (statusLabel) statusLabel->setText(QString("%1%").arg(avgPct));
    if (jobsHeaderLabel) jobsHeaderLabel->setText(QString("JOBS %1").arg(jobs.size()));
    if (startButton) startButton->setEnabled(!jobs.isEmpty());
    if (stopButton)  stopButton->setEnabled(running > 0);
  }

  void handleJobSelected() {
    bool has = selectedSourceIndex() >= 0;
    if (removeButton) removeButton->setEnabled(has);
    if (duplicateButton) duplicateButton->setEnabled(has);
    if (outputSettingsButton) outputSettingsButton->setEnabled(has);
  }

  void syncDetailEditorsFromJob(int index) {
    if (!service || index < 0 || index >= service->jobCount()) {
      return;
    }

    syncingJobDetails = true;

    if (outputPathEdit) {
      QSignalBlocker b(outputPathEdit);
      outputPathEdit->setText(service->jobOutputPathAt(index));
    }

    int startFrame = 0;
    int endFrame = 0;
    if (service->jobFrameRangeAt(index, &startFrame, &endFrame)) {
      if (startFrameSpin) { QSignalBlocker b(startFrameSpin); startFrameSpin->setValue(startFrame); }
      if (endFrameSpin)   { QSignalBlocker b(endFrameSpin);   endFrameSpin->setValue(endFrame); }
    }

    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float scale = 1.0f;
    float rotationDeg = 0.0f;
    if (service->jobOverlayTransformAt(index, &offsetX, &offsetY, &scale, &rotationDeg)) {
      if (overlayXSpin) overlayXSpin->setValue(offsetX);
      if (overlayYSpin) overlayYSpin->setValue(offsetY);
      if (overlayScaleSpin) overlayScaleSpin->setValue(scale);
      if (overlayRotationSpin) overlayRotationSpin->setValue(rotationDeg);
    }

    syncingJobDetails = false;
  }
 };

 RenderQueueManagerWidget::RenderQueueManagerWidget(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  const QString sectionBarStyle =
      "background-color: #1e2a2e; border-bottom: 1px solid #2a3a3e;";
  const QString groupLabelStyle =
      "color: #00d4e8; font-size: 11px; font-weight: bold; padding: 4px 0;";
  const QString fieldLabelStyle =
      "color: #aaaaaa; font-size: 11px;";

  // ── Status Bar ──────────────────────────────────────────────────────────
  impl_->statusBarLabel = new QLabel(u8"現在 — | 残り — | 完了予定 --:-- | 経過 00:00:00 | 速度 — fps", this);
  impl_->statusBarLabel->setStyleSheet(
      "background-color: #0a1a1a; color: #00d4e8; font-size: 10px; padding: 3px 10px;");
  root->addWidget(impl_->statusBarLabel);

  // ── Toolbar ──────────────────────────────────────────────────────────
  auto* toolbarWidget = new QWidget(this);
  toolbarWidget->setFixedHeight(38);
  toolbarWidget->setStyleSheet("background-color: #1a1a1a; border-bottom: 1px solid #2a2a2a;");
  {
    auto* hl = new QHBoxLayout(toolbarWidget);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(6);
    auto* queueLbl = new QLabel("QUEUE", toolbarWidget);
    queueLbl->setStyleSheet("color: #888888; font-size: 11px; font-weight: bold;");
    impl_->searchEdit = new QLineEdit(toolbarWidget);
    impl_->searchEdit->setPlaceholderText(u8"Search...");
    impl_->searchEdit->setObjectName("renderQueueSearch");
    impl_->searchEdit->setFixedWidth(140);
    impl_->addButton       = new QPushButton(u8"＋ Add",   toolbarWidget);
    impl_->duplicateButton = new QToolButton(toolbarWidget);
    impl_->duplicateButton->setText(u8"Dup");
    impl_->removeButton    = new QPushButton(u8"× Remove", toolbarWidget);
    impl_->previewToggleButton = new QPushButton(u8"Preview ON", toolbarWidget);
    impl_->previewToggleButton->setCheckable(true);
    impl_->previewToggleButton->setChecked(true);
    impl_->previewToggleButton->setStyleSheet(
        "QPushButton { background: #2a2a2a; border: 1px solid #555; color: #aaa; padding: 0 8px; border-radius: 3px; }"
        "QPushButton:checked { background: #1a3a4a; border-color: #00d4e8; color: #00d4e8; }");
    impl_->clearHistoryButton = new QPushButton(u8"Clear Log", toolbarWidget);
    for (auto* btn : { impl_->addButton, impl_->removeButton,
                       impl_->clearHistoryButton, impl_->previewToggleButton })
      btn->setFixedHeight(26);
    impl_->duplicateButton->setFixedHeight(26);
    impl_->removeButton->setEnabled(false);
    impl_->duplicateButton->setEnabled(false);
    hl->addWidget(queueLbl);
    hl->addSpacing(4);
    hl->addWidget(impl_->searchEdit);
    hl->addSpacing(4);
    hl->addWidget(impl_->addButton);
    hl->addWidget(impl_->duplicateButton);
    hl->addWidget(impl_->removeButton);
    hl->addStretch();
    hl->addWidget(impl_->previewToggleButton);
    hl->addSpacing(4);
    hl->addWidget(impl_->clearHistoryButton);
  }
  root->addWidget(toolbarWidget);

  // ── Main Splitter: Left (preview + jobs + log) / Right (settings) ─────
  auto* splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setChildrenCollapsible(false);
  splitter->setHandleWidth(2);

  // ─── Left Pane ─────────────────────────────────────────────────────────
  auto* leftWidget = new QWidget(splitter);
  leftWidget->setMinimumWidth(300);
  auto* leftLay = new QVBoxLayout(leftWidget);
  leftLay->setContentsMargins(0, 0, 0, 0);
  leftLay->setSpacing(0);

  // PREVIEW header
  {
    auto* phWidget = new QWidget(leftWidget);
    phWidget->setFixedHeight(26);
    phWidget->setStyleSheet(sectionBarStyle);
    auto* phLay = new QHBoxLayout(phWidget);
    phLay->setContentsMargins(8, 0, 8, 0);
    auto* bar = new QFrame(phWidget);
    bar->setFixedSize(3, 20);
    bar->setStyleSheet("background-color: #00d4e8;");
    auto* previewLbl = new QLabel("PREVIEW", phWidget);
    previewLbl->setStyleSheet(groupLabelStyle);
    auto* fitBtn = new QPushButton("Fit", phWidget);
    fitBtn->setFixedSize(32, 18);
    fitBtn->setStyleSheet(
        "QPushButton { background:#333; color:#aaa; border:1px solid #444; font-size:10px; border-radius:2px; }"
        "QPushButton:hover { background:#444; }");
    impl_->previewFrameLabel = new QLabel("Frame — / —", phWidget);
    impl_->previewFrameLabel->setStyleSheet("color: #00d4e8; font-size: 10px;");
    phLay->addWidget(bar);
    phLay->addSpacing(4);
    phLay->addWidget(previewLbl);
    phLay->addSpacing(6);
    phLay->addWidget(fitBtn);
    phLay->addStretch();
    phLay->addWidget(impl_->previewFrameLabel);
    leftLay->addWidget(phWidget);
    Q_UNUSED(fitBtn);
  }

  // Preview label (large)
  impl_->previewLabel = new QLabel(leftWidget);
  impl_->previewLabel->setAlignment(Qt::AlignCenter);
  impl_->previewLabel->setMinimumHeight(150);
  impl_->previewLabel->setStyleSheet("background-color: #080808; border: none;");
  leftLay->addWidget(impl_->previewLabel, 2);

  // Scrubber
  impl_->previewScrubBar = new QProgressBar(leftWidget);
  impl_->previewScrubBar->setRange(0, 100);
  impl_->previewScrubBar->setValue(0);
  impl_->previewScrubBar->setFixedHeight(6);
  impl_->previewScrubBar->setTextVisible(false);
  impl_->previewScrubBar->setStyleSheet(
      "QProgressBar { background:#222; border:none; } "
      "QProgressBar::chunk { background:#0080ff; }");
  leftLay->addWidget(impl_->previewScrubBar);

  // Playback controls row
  {
    auto* playRow = new QWidget(leftWidget);
    playRow->setFixedHeight(30);
    playRow->setStyleSheet("background-color: #141414;");
    auto* prLay = new QHBoxLayout(playRow);
    prLay->setContentsMargins(6, 2, 6, 2);
    prLay->setSpacing(2);
    const QString pbStyle =
        "QPushButton { background:#2a2a2a; color:#ccc; border:1px solid #3a3a3a; font-size:12px; "
        "padding:0 3px; border-radius:2px; min-width:22px; } "
        "QPushButton:hover { background:#3a3a3a; }";
    impl_->firstFrameButton = new QPushButton(u8"«", playRow);
    impl_->prevFrameButton  = new QPushButton(u8"‹", playRow);
    impl_->playButton       = new QPushButton(u8"▶", playRow);
    impl_->nextFrameButton  = new QPushButton(u8"›", playRow);
    impl_->lastFrameButton  = new QPushButton(u8"»", playRow);
    for (auto* b : { impl_->firstFrameButton, impl_->prevFrameButton, impl_->playButton,
                     impl_->nextFrameButton, impl_->lastFrameButton }) {
      b->setFixedSize(24, 22);
      b->setStyleSheet(pbStyle);
    }
    impl_->frameCountLabel = new QLabel("f — / —", playRow);
    impl_->frameCountLabel->setStyleSheet("color:#888; font-size:10px; margin-left:4px;");
    impl_->playbackTimecodeLabel = new QLabel("00:00:00:00 / 00:00:00:00", playRow);
    impl_->playbackTimecodeLabel->setStyleSheet("color:#555; font-size:10px;");
    prLay->addWidget(impl_->firstFrameButton);
    prLay->addWidget(impl_->prevFrameButton);
    prLay->addWidget(impl_->playButton);
    prLay->addWidget(impl_->nextFrameButton);
    prLay->addWidget(impl_->lastFrameButton);
    prLay->addWidget(impl_->frameCountLabel);
    prLay->addStretch();
    prLay->addWidget(impl_->playbackTimecodeLabel);
    leftLay->addWidget(playRow);
  }

  // JOBS header
  impl_->jobsHeaderLabel = new QLabel(u8"JOBS 0", leftWidget);
  impl_->jobsHeaderLabel->setFixedHeight(28);
  impl_->jobsHeaderLabel->setStyleSheet(
      "background-color: #1e2a2e; color: #00d4e8; font-size: 11px; font-weight: bold; padding: 4px 8px;");
  leftLay->addWidget(impl_->jobsHeaderLabel);

  impl_->jobListWidget = new QListWidget(leftWidget);
  impl_->jobListWidget->setObjectName("renderQueueList");
  impl_->jobListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  impl_->jobListWidget->setStyleSheet(
      "QListWidget { background: #141414; border: none; color: #cccccc; }"
      "QListWidget::item { padding: 4px 6px; border-bottom: 1px solid #1e1e1e; }"
      "QListWidget::item:selected { background: #1e3a4a; }");
  leftLay->addWidget(impl_->jobListWidget, 1);

  // LOG header
  {
    auto* logHdr = new QWidget(leftWidget);
    logHdr->setFixedHeight(24);
    logHdr->setStyleSheet(sectionBarStyle);
    auto* lhLay = new QHBoxLayout(logHdr);
    lhLay->setContentsMargins(4, 0, 8, 0);
    auto* bar = new QFrame(logHdr);
    bar->setFixedSize(3, 16);
    bar->setStyleSheet("background-color: #00d4e8;");
    auto* lbl = new QLabel("LOG", logHdr);
    lbl->setStyleSheet(groupLabelStyle);
    impl_->exportHistoryButton = new QPushButton(u8"Export...", logHdr);
    impl_->exportHistoryButton->setFixedHeight(18);
    impl_->exportHistoryButton->setStyleSheet("font-size:10px;");
    lhLay->addWidget(bar);
    lhLay->addSpacing(4);
    lhLay->addWidget(lbl);
    lhLay->addStretch();
    lhLay->addWidget(impl_->exportHistoryButton);
    leftLay->addWidget(logHdr);
  }
  impl_->historyListWidget = new QListWidget(leftWidget);
  impl_->historyListWidget->setObjectName("renderQueueHistory");
  impl_->historyListWidget->setFixedHeight(100);
  impl_->historyListWidget->setStyleSheet(
      "QListWidget { background:#0d0d0d; border:none; color:#4a9f4a; font-size:10px; font-family:Consolas; }"
      "QListWidget::item { padding: 1px 6px; }");
  leftLay->addWidget(impl_->historyListWidget);

  // ─── Right Pane (scroll) ──────────────────────────────────────────────
  auto* rightScroll = new QScrollArea(splitter);
  rightScroll->setWidgetResizable(true);
  rightScroll->setMinimumWidth(260);
  rightScroll->setStyleSheet("QScrollArea { border: none; background: #181818; }");
  auto* rightWidget = new QWidget();
  auto* rightLay = new QVBoxLayout(rightWidget);
  rightLay->setContentsMargins(0, 0, 0, 0);
  rightLay->setSpacing(0);

  const auto makeSection = [&](const QString& title) -> QVBoxLayout* {
    auto* hdr = new QWidget(rightWidget);
    hdr->setFixedHeight(26);
    hdr->setStyleSheet(sectionBarStyle);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(4, 0, 8, 0);
    auto* bar = new QFrame(hdr);
    bar->setFixedSize(3, 20);
    bar->setStyleSheet("background-color: #00d4e8;");
    auto* lbl = new QLabel(title, hdr);
    lbl->setStyleSheet(groupLabelStyle);
    hl->addWidget(bar);
    hl->addSpacing(4);
    hl->addWidget(lbl);
    hl->addStretch();
    rightLay->addWidget(hdr);
    auto* body = new QWidget(rightWidget);
    auto* sbl = new QVBoxLayout(body);
    sbl->setContentsMargins(10, 8, 10, 8);
    sbl->setSpacing(6);
    rightLay->addWidget(body);
    return sbl;
  };

  // OUTPUT section
  {
    auto* sbl = makeSection("OUTPUT");
    auto* rowPath = new QHBoxLayout();
    impl_->outputPathEdit = new QLineEdit(rightWidget);
    impl_->outputPathEdit->setPlaceholderText(u8"出力パス...");
    impl_->outputBrowseButton = new QPushButton("...", rightWidget);
    impl_->outputBrowseButton->setFixedWidth(28);
    rowPath->addWidget(impl_->outputPathEdit, 1);
    rowPath->addWidget(impl_->outputBrowseButton);
    sbl->addLayout(rowPath);

    auto* rowFormat = new QHBoxLayout();
    auto* fmtLbl = new QLabel("Format", rightWidget);
    fmtLbl->setStyleSheet(fieldLabelStyle);
    fmtLbl->setFixedWidth(70);
    impl_->outputFormatCombo = new QComboBox(rightWidget);
    impl_->outputFormatCombo->addItems({"H.264 / MP4", "H.265 / MP4", "ProRes / MOV", "PNG Sequence", "EXR Sequence"});
    rowFormat->addWidget(fmtLbl);
    rowFormat->addWidget(impl_->outputFormatCombo, 1);
    sbl->addLayout(rowFormat);

    auto* rowPreset = new QHBoxLayout();
    auto* presetLbl = new QLabel("Preset", rightWidget);
    presetLbl->setStyleSheet(fieldLabelStyle);
    presetLbl->setFixedWidth(70);
    impl_->outputPresetCombo = new QComboBox(rightWidget);
    impl_->outputPresetCombo->addItems({u8"High Quality", u8"Web 1080p", u8"Web 720p", u8"Draft"});
    rowPreset->addWidget(presetLbl);
    rowPreset->addWidget(impl_->outputPresetCombo, 1);
    sbl->addLayout(rowPreset);

    impl_->outputSettingsButton = new QPushButton(u8"Format Settings...", rightWidget);
    sbl->addWidget(impl_->outputSettingsButton);
  }

  // FRAME RANGE section
  {
    auto* sbl = makeSection("FRAME RANGE");
    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setContentsMargins(0, 0, 0, 0);
    impl_->startFrameSpin = new QSpinBox(rightWidget);
    impl_->endFrameSpin   = new QSpinBox(rightWidget);
    impl_->skipFrameSpin  = new QSpinBox(rightWidget);
    impl_->startFrameSpin->setRange(0, 1000000);
    impl_->endFrameSpin->setRange(0, 1000000);
    impl_->endFrameSpin->setValue(300);
    impl_->skipFrameSpin->setRange(1, 100);
    impl_->skipFrameSpin->setValue(1);
    form->addRow("Start", impl_->startFrameSpin);
    form->addRow("End",   impl_->endFrameSpin);
    form->addRow("Skip",  impl_->skipFrameSpin);
    sbl->addLayout(form);
  }

  // RENDER SETTINGS section
  {
    auto* sbl = makeSection("RENDER SETTINGS");
    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setContentsMargins(0, 0, 0, 0);
    impl_->qualityCombo = new QComboBox(rightWidget);
    impl_->threadsCombo = new QComboBox(rightWidget);
    impl_->gpuCombo     = new QComboBox(rightWidget);
    impl_->qualityCombo->addItems({u8"Best", u8"High", u8"Medium", u8"Draft"});
    impl_->threadsCombo->addItems({u8"Auto", "1", "2", "4", "8", "16"});
    impl_->gpuCombo->addItems({u8"CPU only", "CUDA : 0", "Vulkan : 0"});
    form->addRow("Quality", impl_->qualityCombo);
    form->addRow("Threads", impl_->threadsCombo);
    form->addRow("GPU",     impl_->gpuCombo);
    sbl->addLayout(form);
  }

  // PREVIEW section
  {
    auto* sbl = makeSection("PREVIEW");
    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setContentsMargins(0, 0, 0, 0);
    impl_->previewResCombo      = new QComboBox(rightWidget);
    impl_->previewIntervalCombo = new QComboBox(rightWidget);
    impl_->previewColorCombo    = new QComboBox(rightWidget);
    impl_->previewResCombo->addItems({"1/1 (Full)", "1/2", "1/4", "1/8"});
    impl_->previewResCombo->setCurrentIndex(1);
    impl_->previewIntervalCombo->addItems({"1f ごと", "5f ごと", "10f ごと", "30f ごと", "60f ごと"});
    impl_->previewIntervalCombo->setCurrentIndex(2);
    impl_->previewColorCombo->addItems({"RGB", "Alpha", "Luminance"});
    form->addRow(u8"解像度",   impl_->previewResCombo);
    form->addRow(u8"更新間隔", impl_->previewIntervalCombo);
    form->addRow(u8"カラー",   impl_->previewColorCombo);
    sbl->addLayout(form);
  }

  rightLay->addStretch();
  rightScroll->setWidget(rightWidget);

  splitter->addWidget(leftWidget);
  splitter->addWidget(rightScroll);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);
  splitter->setSizes({500, 300});
  root->addWidget(splitter, 1);

  // Hidden service-sync widgets
  impl_->overlayXSpin        = new QDoubleSpinBox(this);
  impl_->overlayYSpin        = new QDoubleSpinBox(this);
  impl_->overlayScaleSpin    = new QDoubleSpinBox(this);
  impl_->overlayRotationSpin = new QDoubleSpinBox(this);
  for (auto* s : { impl_->overlayXSpin, impl_->overlayYSpin,
                   impl_->overlayScaleSpin, impl_->overlayRotationSpin })
    s->hide();

  // ── Footer ──────────────────────────────────────────────────────────
  auto* footer = new QWidget(this);
  footer->setFixedHeight(38);
  footer->setStyleSheet("background-color: #141414; border-top: 1px solid #2a2a2a;");
  {
    auto* fl = new QHBoxLayout(footer);
    fl->setContentsMargins(10, 0, 10, 0);
    impl_->summaryLabel = new QLabel(u8"Jobs: 0 | Running: 0 | Done: 0", footer);
    impl_->summaryLabel->setStyleSheet("color: #888888; font-size: 11px;");
    impl_->totalProgressBar = new QProgressBar(footer);
    impl_->totalProgressBar->setRange(0, 100);
    impl_->totalProgressBar->setValue(0);
    impl_->totalProgressBar->setFixedHeight(14);
    impl_->totalProgressBar->setTextVisible(false);
    impl_->statusLabel = new QLabel("0%", footer);
    impl_->statusLabel->setStyleSheet("color: #00d4e8; font-size: 11px;");
    impl_->statusLabel->setFixedWidth(38);
    impl_->startButton = new QPushButton(u8"▶ START RENDER", footer);
    impl_->startButton->setObjectName("renderStartBtn");
    impl_->startButton->setFixedSize(130, 26);
    impl_->startButton->setEnabled(false);
    impl_->stopButton = new QPushButton(u8"■ STOP", footer);
    impl_->stopButton->setFixedSize(80, 26);
    impl_->stopButton->setEnabled(false);
    fl->addWidget(impl_->summaryLabel);
    fl->addWidget(impl_->totalProgressBar, 1);
    fl->addWidget(impl_->statusLabel);
    fl->addSpacing(8);
    fl->addWidget(impl_->startButton);
    fl->addWidget(impl_->stopButton);
  }
  root->addWidget(footer);

  // ── Signal Connections ──────────────────────────────────────────────────

  // Context menu
  connect(impl_->jobListWidget, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
    int idx = impl_->selectedSourceIndex();
    if (idx < 0 || !impl_->service) return;
    QMenu menu(this);
    QString path = impl_->service->jobOutputPathAt(idx);
    auto* reveal = menu.addAction("Reveal in Explorer");
    auto* open   = menu.addAction("Open File");
    auto* act    = menu.exec(impl_->jobListWidget->mapToGlobal(pos));
    if (act == reveal)    ArtifactCore::openInExplorer(path, true);
    else if (act == open) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  });

  connect(impl_->jobListWidget, &QListWidget::currentRowChanged, this, [this](int row) {
    impl_->handleJobSelected();
    const int sourceIndex = (row >= 0 && row < impl_->visibleToSource.size())
        ? impl_->visibleToSource[row] : impl_->selectedSourceIndex();
    impl_->syncDetailEditorsFromJob(sourceIndex);
  });

  connect(impl_->outputSettingsButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->service) return;
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) return;
    ArtifactRenderOutputSettingDialog dialog(this);
    QString outputFormat, codec, codecProfile;
    int width = 0, height = 0, bitrateKbps = 0;
    double fps = 0.0;
    impl_->service->jobOutputSettingsAt(index, &outputFormat, &codec, &codecProfile, &width, &height, &fps, &bitrateKbps);
    dialog.setOutputPath(impl_->service->jobOutputPathAt(index));
    dialog.setOutputFormat(outputFormat);
    dialog.setCodec(codec);
    dialog.setCodecProfile(codecProfile);
    dialog.setEncoderBackend(impl_->service->jobEncoderBackendAt(index));
    dialog.setResolution(width, height);
    dialog.setFrameRate(fps);
    dialog.setBitrateKbps(bitrateKbps);
    if (dialog.exec() == QDialog::Accepted) {
      impl_->service->setJobOutputPathAt(index, dialog.outputPath());
      impl_->service->setJobOutputSettingsAt(index, dialog.outputFormat(), dialog.codec(),
          dialog.codecProfile(), dialog.outputWidth(), dialog.outputHeight(),
          dialog.frameRate(), dialog.bitrateKbps());
      impl_->service->setJobEncoderBackendAt(index, dialog.encoderBackend());
      impl_->syncJobsFromService();
      impl_->syncDetailEditorsFromJob(index);
    }
  });

  // Frame range write-back
  connect(impl_->startFrameSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
    if (impl_->syncingJobDetails || !impl_->service) return;
    const int idx = impl_->selectedSourceIndex();
    if (idx < 0) return;
    impl_->service->setJobFrameRangeAt(idx, impl_->startFrameSpin->value(), impl_->endFrameSpin->value());
  });
  connect(impl_->endFrameSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
    if (impl_->syncingJobDetails || !impl_->service) return;
    const int idx = impl_->selectedSourceIndex();
    if (idx < 0) return;
    impl_->service->setJobFrameRangeAt(idx, impl_->startFrameSpin->value(), impl_->endFrameSpin->value());
  });

  // Output path write-back
  connect(impl_->outputPathEdit, &QLineEdit::editingFinished, this, [this]() {
    if (impl_->syncingJobDetails || !impl_->service) return;
    const int idx = impl_->selectedSourceIndex();
    if (idx < 0) return;
    impl_->service->setJobOutputPathAt(idx, impl_->outputPathEdit->text());
  });

  // Preview toggle
  connect(impl_->previewToggleButton, &QPushButton::toggled, this, [this](bool on) {
    impl_->previewEnabled = on;
    if (!on && impl_->previewLabel) {
      impl_->previewLabel->clear();
      impl_->previewLabel->setText("—");
    }
  });

  // Preview interval
  connect(impl_->previewIntervalCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
    static const int intervals[] = {1, 5, 10, 30, 60};
    impl_->previewIntervalFrames = (idx >= 0 && idx < 5) ? intervals[idx] : 10;
  });

  // Service signals
  if (impl_->service) {
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<RenderQueueChangedEvent>([this](const RenderQueueChangedEvent& event) {
          Q_UNUSED(event);
          if (!impl_) return;
          impl_->syncJobsFromService();
        }));
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<RenderQueueLogEvent>([this](const RenderQueueLogEvent& event) {
          if (!impl_) return;
          if (event.message.trimmed().isEmpty()) return;
          impl_->logServiceEvent(event.message, event.sourceIndex, event.alsoHistory);
        }));
    connect(impl_->service, &ArtifactRenderQueueService::jobAdded, this, [this](int) {
        if (!impl_) return;
        impl_->postQueueChanged(QStringLiteral("Job added"));
    });
    connect(impl_->service, &ArtifactRenderQueueService::jobRemoved, this, [this](int) {
        if (!impl_) return;
        impl_->postQueueChanged(QStringLiteral("Job removed"));
    });
    connect(impl_->service, &ArtifactRenderQueueService::jobUpdated, this, [this](int) {
        if (!impl_) return;
        impl_->postQueueChanged(QStringLiteral("Job updated"));
    });
    connect(impl_->service, &ArtifactRenderQueueService::jobProgressChanged, this, [this](int, int) {
        if (!impl_) return;
        impl_->postQueueChanged(QStringLiteral("Job progress updated"));
    });
    connect(impl_->service, &ArtifactRenderQueueService::jobStatusChanged, this, [this](int index, int) {
        if (!impl_ || !impl_->service) return;
        const QString jobName   = impl_->service->jobCompositionNameAt(index);
        const QString jobStatus = impl_->service->jobStatusAt(index);
        if (jobStatus == "Failed") {
          const QString error = impl_->service->jobErrorMessageAt(index);
          impl_->postHistoryMessage(QString("Job failed: %1%2").arg(jobName)
              .arg(error.trimmed().isEmpty() ? QString() : QString(" | %1").arg(error)), index);
        } else if (jobStatus == "Completed") {
          impl_->postHistoryMessage(QString("Job completed: %1").arg(jobName), index);
        } else if (jobStatus == "Rendering") {
          impl_->postHistoryMessage(QString("Job started: %1").arg(jobName), index);
        } else {
          impl_->postHistoryMessage(QString("Job status -> %1: %2").arg(jobName, jobStatus), index, false);
        }
        impl_->postQueueChanged(QStringLiteral("Job status changed"));
    });
    connect(impl_->service, &ArtifactRenderQueueService::queueReordered, this, [this](int, int) {
        if (!impl_) return;
        impl_->postQueueChanged(QStringLiteral("Queue reordered"));
    });
    connect(impl_->service, &ArtifactRenderQueueService::allJobsCompleted, this, [this]() {
#ifdef _WIN32
        ::MessageBeep(MB_OK);
#else
        QApplication::beep();
#endif
        if (impl_) impl_->postHistoryMessage(QStringLiteral("All jobs completed"));
    });
    connect(impl_->service, &ArtifactRenderQueueService::previewFrameReady, this, [this](int jobIndex, int frameNumber) {
        if (!impl_->previewEnabled) return;
        // Throttle based on the selected interval
        const int interval = impl_->previewIntervalFrames;
        if (interval > 1 && (frameNumber - impl_->lastPreviewUpdateFrame) < interval &&
            frameNumber != impl_->endFrameSpin->value())
          return;
        impl_->lastPreviewUpdateFrame = frameNumber;
        const QImage frame = impl_->service->lastRenderedFrame();
        if (!frame.isNull() && impl_->previewLabel) {
          const QSize labelSize = impl_->previewLabel->size();
          QPixmap px = QPixmap::fromImage(frame);
          if (labelSize.isValid() && labelSize.width() > 10 && labelSize.height() > 10)
            px = px.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
          impl_->previewLabel->setPixmap(px);
          impl_->previewLabel->setToolTip(QString("Job %1 | Frame %2").arg(jobIndex + 1).arg(frameNumber));
        }
        const int totalFrames = impl_->endFrameSpin ? impl_->endFrameSpin->value() : 0;
        if (impl_->previewFrameLabel)
          impl_->previewFrameLabel->setText(QString("Frame %1 / %2").arg(frameNumber + 1).arg(totalFrames));
        if (impl_->frameCountLabel)
          impl_->frameCountLabel->setText(QString("f %1 / %2").arg(frameNumber + 1).arg(totalFrames));
        if (impl_->previewScrubBar && totalFrames > 0)
          impl_->previewScrubBar->setValue((frameNumber + 1) * 100 / totalFrames);
    });
  }

  connect(impl_->addButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) impl_->service->addRenderQueue();
  });
  connect(impl_->removeButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) {
      const int index = impl_->selectedSourceIndex();
      if (index >= 0) impl_->service->removeRenderQueueAt(index);
    }
  });
  connect(impl_->duplicateButton, &QToolButton::clicked, this, [this]() {
    if (impl_->service) {
      const int index = impl_->selectedSourceIndex();
      if (index >= 0) impl_->service->duplicateRenderQueueAt(index);
    }
  });
  connect(impl_->startButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) impl_->service->startAllJobs();
  });
  connect(impl_->stopButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) impl_->service->cancelAllJobs();
  });
  connect(impl_->outputBrowseButton, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getSaveFileName(
        this, u8"出力先を選択",
        impl_->outputPathEdit ? impl_->outputPathEdit->text() : QString(),
        "Video Files (*.mp4 *.mov *.mkv);;All Files (*)");
    if (!path.isEmpty() && impl_->outputPathEdit) impl_->outputPathEdit->setText(path);
  });
  connect(impl_->clearHistoryButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->historyListWidget) return;
    impl_->historyListWidget->clear();
    impl_->saveHistory();
  });
  connect(impl_->exportHistoryButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->historyListWidget) return;
    const QString filePath = QFileDialog::getSaveFileName(
        this, "Export Render History",
        QDir::homePath() + "/Desktop/render_queue_history.log",
        "Log Files (*.log *.txt);;All Files (*)");
    if (filePath.isEmpty()) return;
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
      QMessageBox::warning(this, "Export Failed", QString("Failed to open %1").arg(filePath));
      return;
    }
    QTextStream out(&file);
    for (int i = 0; i < impl_->historyListWidget->count(); ++i)
      out << impl_->historyListWidget->item(i)->text() << '\n';
  });

  impl_->syncJobsFromService();
  impl_->loadHistory();
  impl_->handleJobSelected();
 }

 RenderQueueManagerWidget::~RenderQueueManagerWidget() { delete impl_; }
 QSize RenderQueueManagerWidget::sizeHint() const { return QSize(800, 600); }
 void RenderQueueManagerWidget::setFloatingMode(bool f) { setWindowFlag(Qt::Window, f); show(); }
 void RenderQueueManagerWidget::showEvent(QShowEvent* e) { QWidget::showEvent(e); impl_->syncJobsFromService(); }
}
