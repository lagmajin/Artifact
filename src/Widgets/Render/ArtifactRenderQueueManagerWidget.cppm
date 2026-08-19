module;
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QStringList>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QByteArray>
#include <QVariantMap>
#include <QListWidgetItem>
#include <QAbstractItemView>
#include <QSignalBlocker>
#include <QShortcut>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QDropEvent>
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
import Artifact.Render.Batch;
import Artifact.Render.Queue.Presets;
import Artifact.Service.Project;
import Artifact.Layers.Selection.Manager;
import Artifact.Widget.Dialog.RenderOutputSetting;
import Artifact.Widgets.RenderQueuePresetSelector;
import Core.FastSettingsStore;
import Artifact.Widgets.AppDialogs;
import Utils.Path;
import Utils.ExplorerUtils;
import Settings.Accessibility;

namespace Artifact
{
 using namespace ArtifactCore;

 namespace {
 QIcon loadIconWithFallback(const QString& fileName);

 class RenderQueueSearchEdit final : public QLineEdit
 {
 public:
   std::function<void(const QString&)> changed;
   using QLineEdit::QLineEdit;

 protected:
   void keyReleaseEvent(QKeyEvent* event) override
   {
     QLineEdit::keyReleaseEvent(event);
     if (event->key() == Qt::Key_Escape && !text().isEmpty()) {
       clear();
     }
     if (changed) changed(text());
   }
 };

 class RenderQueueActionButton final : public QPushButton
 {
 public:
   std::function<void()> action;
   using QPushButton::QPushButton;

 protected:
   void mouseReleaseEvent(QMouseEvent* event) override
   {
     QPushButton::mouseReleaseEvent(event);
     if (event->button() == Qt::LeftButton && action) action();
   }
 };

 class RenderQueueListWidget final : public QListWidget
 {
 public:
   std::function<void(int, int)> reordered;
   using QListWidget::QListWidget;

 protected:
   void dropEvent(QDropEvent* event) override
   {
     const int sourceRow = currentRow();
     const int sourceId = sourceRow >= 0 && sourceRow < count()
         ? item(sourceRow)->data(Qt::UserRole).toInt()
         : -1;
     QListWidget::dropEvent(event);
     int resolvedTarget = -1;
     if (sourceId >= 0) {
       for (int row = 0; row < count(); ++row) {
         if (item(row)->data(Qt::UserRole).toInt() == sourceId) {
           resolvedTarget = row;
           break;
         }
       }
     }
     if (reordered && sourceRow >= 0 && resolvedTarget >= 0 && sourceRow != resolvedTarget) {
       reordered(sourceRow, resolvedTarget);
     }
   }
 };

 class RenderQueuePathEdit final : public QLineEdit
 {
 public:
   std::function<void(const QString&)> committed;
   using QLineEdit::QLineEdit;

 protected:
   void keyReleaseEvent(QKeyEvent* event) override
   {
     QLineEdit::keyReleaseEvent(event);
     if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
       if (committed) committed(text());
     }
   }

   void focusOutEvent(QFocusEvent* event) override
   {
     QLineEdit::focusOutEvent(event);
   }
 };

 class RenderQueueIntSpinBox final : public QSpinBox
 {
 public:
   std::function<void(int)> committed;
   using QSpinBox::QSpinBox;
 protected:
   void keyReleaseEvent(QKeyEvent* event) override
   {
     QSpinBox::keyReleaseEvent(event);
     if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
       if (committed) committed(value());
     }
   }
   void focusOutEvent(QFocusEvent* event) override
   {
     QSpinBox::focusOutEvent(event);
   }
 };

 class RenderQueueDoubleSpinBox final : public QDoubleSpinBox
 {
 public:
   std::function<void(double)> committed;
   using QDoubleSpinBox::QDoubleSpinBox;
 protected:
   void keyReleaseEvent(QKeyEvent* event) override
   {
     QDoubleSpinBox::keyReleaseEvent(event);
     if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
       if (committed) committed(value());
     }
   }
   void focusOutEvent(QFocusEvent* event) override
   {
     QDoubleSpinBox::focusOutEvent(event);
   }
 };

 class RenderQueueJobCard final : public QFrame
 {
 public:
   QLabel* statusLabel = nullptr;
   QLabel* statusIconLabel = nullptr;
   QLabel* thumbnailLabel = nullptr;
   QLabel* nameLabel = nullptr;
   QLabel* outputLabel = nullptr;
   QLabel* backendLabel = nullptr;
   QProgressBar* progressBar = nullptr;

   explicit RenderQueueJobCard(QWidget* parent = nullptr)
       : QFrame(parent)
   {
     setFrameShape(QFrame::StyledPanel);
     auto* root = new QHBoxLayout(this);
     root->setContentsMargins(12, 10, 14, 10);
     root->setSpacing(14);

     statusLabel = new QLabel("WAIT");
     statusLabel->setMinimumWidth(86);
     statusLabel->setAlignment(Qt::AlignCenter);
     statusIconLabel = new QLabel();
     statusIconLabel->setFixedSize(18, 18);
     statusIconLabel->setAlignment(Qt::AlignCenter);

     thumbnailLabel = new QLabel(QStringLiteral("PREVIEW"));
     thumbnailLabel->setFixedSize(280, 172);
     thumbnailLabel->setAlignment(Qt::AlignCenter);
     thumbnailLabel->setScaledContents(false);
     thumbnailLabel->setAutoFillBackground(true);
     QPalette thumbnailPalette = thumbnailLabel->palette();
     thumbnailPalette.setColor(QPalette::Window, QColor(18, 24, 29));
     thumbnailPalette.setColor(QPalette::WindowText, QColor(130, 145, 155));
     thumbnailLabel->setPalette(thumbnailPalette);
     root->addWidget(thumbnailLabel);

     auto* body = new QVBoxLayout();
     body->setSpacing(7);
     nameLabel = new QLabel();
     QFont nameFont = nameLabel->font();
     nameFont.setPointSize(nameFont.pointSize() + 2);
     nameFont.setBold(true);
     nameLabel->setFont(nameFont);
     outputLabel = new QLabel();
     backendLabel = new QLabel();
     outputLabel->setWordWrap(true);
     backendLabel->setWordWrap(true);
     auto* cardHeader = new QHBoxLayout();
     cardHeader->setContentsMargins(0, 0, 0, 0);
     cardHeader->addWidget(nameLabel, 1);
     cardHeader->addWidget(statusIconLabel);
     cardHeader->addWidget(statusLabel);
     body->addLayout(cardHeader);
     body->addWidget(outputLabel);
     body->addWidget(backendLabel);
     body->addStretch();
     progressBar = new QProgressBar();
     progressBar->setRange(0, 100);
     progressBar->setTextVisible(true);
     progressBar->setMinimumWidth(190);
     body->addWidget(progressBar);
     root->addLayout(body, 1);
   }

   void setJob(const QString& status, const QString& name, const QString& output,
               const QString& backend, const QString& errorMessage,
               int progress, const QColor& accent)
   {
     statusLabel->setText(status.toUpper());
     QString statusIcon = QStringLiteral("Studio/animationmenu_schedule.svg");
     if (status.compare(QStringLiteral("Rendering"), Qt::CaseInsensitive) == 0) {
       statusIcon = QStringLiteral("Studio/figma_media_play.svg");
     } else if (status.compare(QStringLiteral("Completed"), Qt::CaseInsensitive) == 0) {
       statusIcon = QStringLiteral("Studio/check_circle.svg");
     } else if (status.compare(QStringLiteral("Failed"), Qt::CaseInsensitive) == 0) {
       statusIcon = QStringLiteral("Studio/asset_missing_small.svg");
     } else if (status.compare(QStringLiteral("Paused"), Qt::CaseInsensitive) == 0) {
       statusIcon = QStringLiteral("Studio/animationmenu_pause.svg");
     }
     statusIconLabel->setPixmap(
         loadIconWithFallback(statusIcon).pixmap(QSize(16, 16)));
     nameLabel->setText(name);
     outputLabel->setText(errorMessage.trimmed().isEmpty()
         ? QStringLiteral("Output  •  %1").arg(output)
         : QStringLiteral("Error  •  %1").arg(errorMessage));
     QPalette outputPalette = outputLabel->palette();
     outputPalette.setColor(QPalette::WindowText,
         errorMessage.trimmed().isEmpty()
             ? QColor(155, 165, 175)
             : QColor(225, 95, 85));
     outputLabel->setPalette(outputPalette);
     backendLabel->setText(errorMessage.trimmed().isEmpty()
         ? backend
         : QStringLiteral("%1  |  action: retry").arg(backend));
     progressBar->setValue(std::clamp(progress, 0, 100));
     QPalette palette = statusLabel->palette();
     palette.setColor(QPalette::WindowText, accent);
     statusLabel->setPalette(palette);
     QPalette barPalette = progressBar->palette();
     barPalette.setColor(QPalette::Highlight, accent);
     progressBar->setPalette(barPalette);
   }

   void setPreview(const QPixmap& pixmap)
   {
     if (!thumbnailLabel || pixmap.isNull()) return;
     thumbnailLabel->setPixmap(pixmap.scaled(
         thumbnailLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
     thumbnailLabel->setToolTip(QStringLiteral("Latest rendered frame"));
   }
 };

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
    QString encoderBackend;
    QString renderBackend;
    QString outputPath;
  };

  ArtifactRenderQueueService* service = nullptr;
  QList<JobEntry> jobs;
  QVector<int> visibleToSource;
  ArtifactCore::EventBus eventBus_;
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  RenderQueueSearchEdit* searchEdit = nullptr;
  QString searchQuery;
  QComboBox* filterCombo = nullptr;
  QComboBox* presetCombo = nullptr;
  QPushButton* savePresetButton = nullptr;
  QPushButton* loadPresetButton = nullptr;
  QPushButton* deletePresetButton = nullptr;
  RenderQueueListWidget* jobListWidget = nullptr;
  QPushButton* addButton = nullptr;
  QToolButton* duplicateButton = nullptr;
  QToolButton* moveUpButton = nullptr;
  QToolButton* moveDownButton = nullptr;
  QPushButton* removeButton = nullptr;
  RenderQueueActionButton* clearButton = nullptr;
  QPushButton* startButton = nullptr;
  RenderQueueActionButton* pauseButton = nullptr;
  RenderQueueActionButton* cancelButton = nullptr;
  QPushButton* rerunSelectedButton = nullptr;
  RenderQueueActionButton* rerunDoneFailedButton = nullptr;
  QProgressBar* totalProgressBar = nullptr;
  QLabel* summaryLabel = nullptr;
  QLabel* statusLabel = nullptr;
  QLabel* runningCountLabel = nullptr;
  QLabel* queueStateLabel = nullptr;
  QLabel* filterAllLabel = nullptr;
  QLabel* filterRunningLabel = nullptr;
  QLabel* filterQueuedLabel = nullptr;
  QLabel* filterCompletedLabel = nullptr;
  QLabel* filterFailedLabel = nullptr;
  QLabel* inspectorJobLabel = nullptr;
   QLabel* preflightBadge = nullptr;
   QLabel* previewSummaryLabel = nullptr;
  QListWidget* historyListWidget = nullptr;
  QPushButton* clearHistoryButton = nullptr;
  QPushButton* exportHistoryButton = nullptr;
  QPushButton* applySettingsToSelectionButton = nullptr;
   QLabel* previewLabel = nullptr;
  QComboBox* progressLogStepCombo = nullptr;
  RenderQueuePathEdit* outputPathEdit = nullptr;
  RenderQueueActionButton* outputBrowseButton = nullptr;
  QLabel* outputSettingsSummaryLabel = nullptr;
  QPushButton* outputSettingsButton = nullptr;
  QLabel* errorLabel = nullptr;
  RenderQueueIntSpinBox* startFrameSpin = nullptr;
  RenderQueueIntSpinBox* endFrameSpin = nullptr;
  RenderQueueDoubleSpinBox* overlayXSpin = nullptr;
  RenderQueueDoubleSpinBox* overlayYSpin = nullptr;
  RenderQueueDoubleSpinBox* overlayScaleSpin = nullptr;
  RenderQueueDoubleSpinBox* overlayRotationSpin = nullptr;
  QCheckBox* excludeAdjustmentLayersCheck = nullptr;
  QCheckBox* excludeGuideLayersCheck = nullptr;
  QCheckBox* splitPassesCheck = nullptr;
  QComboBox* resolutionPresetCombo = nullptr;
  QComboBox* frameRangeModeCombo = nullptr;
  RenderQueueIntSpinBox* selectedRangeStartSpin = nullptr;
  RenderQueueIntSpinBox* selectedRangeEndSpin = nullptr;
  QPushButton* addSelectedRangeButton = nullptr;
  QPushButton* clearSelectedRangesButton = nullptr;
  QLabel* selectedRangesSummaryLabel = nullptr;
  QComboBox* regionModeCombo = nullptr;
  QComboBox* layerFilterModeCombo = nullptr;
  QPushButton* configurePassesButton = nullptr;
  QPushButton* useSelectedLayersButton = nullptr;
  QPushButton* excludeSelectedLayersButton = nullptr;
  QPushButton* clearWhitelistButton = nullptr;
  QPushButton* clearBlacklistButton = nullptr;
  QLabel* layerFilterSummaryLabel = nullptr;
  RenderQueueIntSpinBox* cropXSpin = nullptr;
  RenderQueueIntSpinBox* cropYSpin = nullptr;
  RenderQueueIntSpinBox* cropWSpin = nullptr;
  RenderQueueIntSpinBox* cropHSpin = nullptr;
  std::unique_ptr<ArtifactCore::FastSettingsStore> historyStore_;
  std::unique_ptr<ArtifactCore::FastSettingsStore> presetStore_;
  bool syncingTransformControls = false;
  bool syncingJobDetails = false;
  bool syncingPresetCombo = false;
  std::map<int, int> lastProgressBucketByJob;
  std::map<int, qint64> progressStartedAtMsByJob;
  QElapsedTimer progressClock_;
  int progressLogStepPercent = 25;
  QFont fixedFont_{"Consolas", 10};

  Impl() {
    progressClock_.start();
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

  QString jobHistoryMetadata(int sourceIndex) const {
    if (!service || sourceIndex < 0 || sourceIndex >= service->jobCount()) {
      return QString();
    }
    int startFrame = 0;
    int endFrame = 0;
    service->jobFrameRangeAt(sourceIndex, &startFrame, &endFrame);
    const QString jobId = QStringLiteral("render-queue-%1-%2")
                              .arg(service->jobCompositionIdAt(sourceIndex).toString())
                              .arg(startFrame);
    const QString error = service->jobErrorMessageAt(sourceIndex).trimmed();
    const QString stage = error.isEmpty()
                              ? QStringLiteral("none")
                              : (error.contains(QStringLiteral("encode"), Qt::CaseInsensitive)
                                     ? QStringLiteral("encode")
                                     : error.contains(QStringLiteral("read"), Qt::CaseInsensitive)
                                           ? QStringLiteral("readback")
                                           : QStringLiteral("render"));
    return QStringLiteral("jobId=%1 frames=[%2,%3) failureStage=%4 action=Retry Job")
        .arg(jobId).arg(startFrame).arg(endFrame).arg(stage);
  }

  void logServiceEvent(const QString& event, int sourceIndex = -1, bool alsoHistory = true) {
    QString msg = event;
    if (service && sourceIndex >= 0 && sourceIndex < service->jobCount()) {
        msg += QString(" (%1)").arg(service->jobCompositionNameAt(sourceIndex));
    }
    if (statusLabel) statusLabel->setText(QString("[%1] [Service] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), msg));
    if (alsoHistory) addHistoryEntry(QString("[Service] %1%2").arg(msg)
                                         .arg(sourceIndex >= 0
                                                  ? QStringLiteral(" | %1").arg(jobHistoryMetadata(sourceIndex))
                                                  : QString()),
                                     sourceIndex);
  }

  void addHistoryEntry(const QString& message, int sourceIndex = -1) {
    if (!historyListWidget) return;
    auto* item = new QListWidgetItem(
        QString("[%1] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), message));
    item->setData(Qt::UserRole, sourceIndex);
    historyListWidget->addItem(item);
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
    for (auto it = progressStartedAtMsByJob.begin();
         it != progressStartedAtMsByJob.end();) {
      if (it->first < 0 || it->first >= service->jobCount()) {
        it = progressStartedAtMsByJob.erase(it);
      } else {
        ++it;
      }
    }
    jobs.clear();
    for (int i = 0; i < service->jobCount(); ++i) {
      JobEntry e;
      e.name = service->jobNameAt(i);
      e.status = service->jobStatusAt(i);
      e.progress = service->jobProgressAt(i);
      e.errorMessage = service->jobErrorMessageAt(i);
      e.encoderBackend = service->jobEncoderBackendAt(i);
      e.renderBackend = service->jobRenderBackendAt(i);
      e.outputPath = service->jobOutputPathAt(i);
      jobs.append(e);
    }
    updateJobList();
    if (jobListWidget && selectedSource >= 0) {
      for (int row = 0; row < jobListWidget->count(); ++row) {
        if (jobListWidget->item(row)->data(Qt::UserRole).toInt() == selectedSource) {
          jobListWidget->setCurrentRow(row);
          break;
        }
      }
    }
    updateSummary();
  }

  void postQueueChanged(const QString& reason = QString()) {
    eventBus_.post<RenderQueueChangedEvent>(RenderQueueChangedEvent{
        service ? service->jobCount() : 0,
        selectedSourceIndex(),
        reason
    });
    (void)eventBus_.drain();
  }

  void postHistoryMessage(const QString& message, int sourceIndex = -1, bool alsoHistory = true) {
    eventBus_.post<RenderQueueLogEvent>(RenderQueueLogEvent{message, sourceIndex, alsoHistory});
    (void)eventBus_.drain();
  }


  struct JobLineData {
    QString line;
    QColor textColor;
    QString tooltip;
  };

  static QString shortBackendLabel(const QString& backend)
  {
    const QString value = backend.trimmed().toLower();
    if (value.isEmpty() || value == QStringLiteral("auto")) return QStringLiteral("auto");
    if (value == QStringLiteral("pipe-hw")) return QStringLiteral("hw");
    if (value == QStringLiteral("pipe-vulkan")) return QStringLiteral("vk");
    if (value == QStringLiteral("native")) return QStringLiteral("native");
    if (value == QStringLiteral("gpu") || value.startsWith(QStringLiteral("gpu:"))) return QStringLiteral("gpu");
    if (value == QStringLiteral("external-cycles")) return QStringLiteral("cycles");
    if (value == QStringLiteral("external")) return QStringLiteral("ext");
    return value.left(8);
  }

  JobLineData buildJobLineData(int i) const {
    const auto& job = jobs[i];
    QString statusTag = "WAIT";
    QColor textColor(160, 160, 160);
    QString status = normalizeStatus(job.status);
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor accent(theme.accentColor);
    const QColor selection(theme.selectionColor);
    const QColor border(theme.borderColor);

    if (status == "Rendering") {
      statusTag = "RUN";
      textColor = accent;
    } else if (status == "Completed") {
      statusTag = "DONE";
      textColor = selection.lighter(130);
    } else if (status == "Failed") {
      statusTag = "ERROR";
      textColor = border.lighter(160);
    } else if (status == "Paused") {
      textColor = QColor(theme.textColor).darker(110);
    }

    const QString outputPath = job.outputPath.trimmed();
    const QString outputName = outputPath.isEmpty()
        ? QStringLiteral("output")
        : QFileInfo(outputPath).fileName();
    const QString progressText = status == QStringLiteral("Rendering")
        ? QStringLiteral("Rendering  •  %1%")
        : QStringLiteral("Progress  •  %1%");
    QString line = QStringLiteral("%1  #%2  %3\n%4\n%5")
      .arg(statusTag)
      .arg(i + 1, 2, 10, QChar('0'))
      .arg(job.name.left(42))
      .arg(QStringLiteral("Output  •  %1").arg(outputName))
      .arg(progressText.arg(job.progress));
    QString lineSuffix = QStringLiteral("  |  enc:%1  |  render:%2")
        .arg(shortBackendLabel(job.encoderBackend))
        .arg(shortBackendLabel(job.renderBackend));
    line += lineSuffix;
    QString tooltip = QString("Output: %1\nEncode: %2\nRender: %3")
        .arg(outputPath.isEmpty() ? QStringLiteral("(auto)") : outputPath)
        .arg(job.encoderBackend.isEmpty() ? QStringLiteral("auto") : job.encoderBackend)
        .arg(job.renderBackend.isEmpty() ? QStringLiteral("auto") : job.renderBackend);
    if (!job.errorMessage.isEmpty()) {
      tooltip += QString("\nError: %1").arg(job.errorMessage);
    }
    return {line, textColor, tooltip};
  }

  // Updates a single list item in-place without rebuilding the whole list.
  // Falls back to full rebuild only when list/job count mismatch is detected.
  void updateJobItemAtIndex(int index) {
    if (!jobListWidget) return;
    if (index < 0 || index >= static_cast<int>(jobs.size())
        || jobListWidget->count() != static_cast<int>(jobs.size())) {
      updateJobList();
      return;
    }
    auto* item = jobListWidget->item(index);
    if (!item) {
      updateJobList();
      return;
    }
    const auto data = buildJobLineData(index);
    item->setToolTip(data.tooltip);
    if (auto* card = static_cast<RenderQueueJobCard*>(jobListWidget->itemWidget(item))) {
      const auto& job = jobs[index];
      card->setJob(normalizeStatus(job.status), job.name,
                   QFileInfo(job.outputPath).fileName(),
                   QStringLiteral("enc:%1  |  render:%2")
                       .arg(shortBackendLabel(job.encoderBackend))
                       .arg(shortBackendLabel(job.renderBackend)),
                   job.errorMessage,
                   job.progress, data.textColor);
    } else {
      item->setText(data.line);
      item->setForeground(data.textColor);
    }
    updateSummary();
  }

  void updateJobList() {
    if (!jobListWidget) return;
    QSignalBlocker blocker(jobListWidget);
    jobListWidget->clear();
    visibleToSource.clear();

    const QString query = searchQuery.trimmed().toLower();
    for (int i = 0; i < jobs.size(); ++i) {
      const auto& job = jobs[i];
      if (!query.isEmpty()
          && !job.name.toLower().contains(query)
          && !job.outputPath.toLower().contains(query)
          && !normalizeStatus(job.status).toLower().contains(query)
          && !job.encoderBackend.toLower().contains(query)
          && !job.renderBackend.toLower().contains(query)) {
        continue;
      }
      const auto data = buildJobLineData(i);
      auto* item = new QListWidgetItem();
      item->setData(Qt::UserRole, i);
      item->setToolTip(data.tooltip);
      jobListWidget->addItem(item);
      auto* card = new RenderQueueJobCard(jobListWidget);
      card->setJob(normalizeStatus(job.status), job.name,
                   QFileInfo(job.outputPath).fileName(),
                   QStringLiteral("enc:%1  |  render:%2")
                       .arg(shortBackendLabel(job.encoderBackend))
                       .arg(shortBackendLabel(job.renderBackend)),
                   job.errorMessage,
                   job.progress, data.textColor);
      item->setSizeHint(QSize(0, 196));
      jobListWidget->setItemWidget(item, card);
      visibleToSource.push_back(i);
    }
    updateSummary();
  }

  void updateSummary() {
    if (!summaryLabel) return;
    int done = 0, failed = 0, running = 0, pending = 0;
    int totalProgress = 0;
    for (const auto& j : jobs) {
        QString s = normalizeStatus(j.status);
        if (s == "Completed") done++;
        else if (s == "Failed") failed++;
        else if (s == "Rendering") running++;
        else if (s == "Pending" || s == "Paused") pending++;
        totalProgress += std::clamp(j.progress, 0, 100);
    }
    QString preflightText;
    double etaSeconds = 0.0;
    bool hasEta = false;
    const qint64 nowMs = progressClock_.elapsed();
    for (int index = 0; index < jobs.size(); ++index) {
      const auto& job = jobs[index];
      if (normalizeStatus(job.status) != QStringLiteral("Rendering") ||
          job.progress <= 0) {
        continue;
      }
      const auto started = progressStartedAtMsByJob.find(index);
      if (started == progressStartedAtMsByJob.end()) continue;
      const qint64 elapsedMs = nowMs - started->second;
      if (elapsedMs < 1000) continue;
      const double rate = static_cast<double>(job.progress) /
                          (static_cast<double>(elapsedMs) / 1000.0);
      if (rate <= 0.0) continue;
      etaSeconds += (100.0 - static_cast<double>(job.progress)) / rate;
      hasEta = true;
    }
    const QString etaText = hasEta
        ? QStringLiteral(" | ETA: ~%1s")
              .arg(static_cast<qint64>(std::llround(etaSeconds)))
        : (running > 0 ? QStringLiteral(" | ETA: calculating") : QString());
    QString farmText;
    if (service && service->farmEnabled()) {
      farmText = QStringLiteral(" | Farm: %1 workers")
                     .arg(service->farmWorkerCount());
      if (service->isFarmRpcServerRunning()) {
        farmText += QStringLiteral(" | RPC online");
      }
      if (service->farmAllowRemoteWorkers()) {
        farmText += QStringLiteral(" | remote workers allowed");
      }
    }
    const int selected = selectedSourceIndex();
    if (service && selected >= 0 && selected < service->jobCount()) {
      const auto preflight = service->preflightRenderQueueAt(selected);
      preflightText = QStringLiteral(" | Preflight: %1E/%2W")
          .arg(preflight.getErrorCount())
          .arg(preflight.getWarningCount());
    }
    summaryLabel->setText(QString("Jobs: %1 | Running: %2 | Queued: %3 | Done: %4 | Failed: %5%6")
                              .arg(jobs.size())
                              .arg(running)
                              .arg(pending)
                              .arg(done)
                              .arg(failed)
                              .arg(preflightText + etaText + farmText));
    if (runningCountLabel) {
      runningCountLabel->setText(QString("%1 RUNNING").arg(running));
      const auto& theme = ArtifactCore::currentDCCTheme();
      QPalette palette = runningCountLabel->palette();
      palette.setColor(QPalette::WindowText,
          running > 0 ? QColor(theme.accentColor).lighter(135)
                      : QColor(theme.textColor).darker(125));
      runningCountLabel->setPalette(palette);
    }
    if (queueStateLabel) {
      queueStateLabel->setText(running > 0
          ? QStringLiteral("Rendering %1 of %2 jobs").arg(running).arg(jobs.size())
          : (jobs.isEmpty() ? QStringLiteral("Queue is empty")
                            : QStringLiteral("Queue ready")));
    }
    if (filterAllLabel) filterAllLabel->setText(QStringLiteral("All   %1").arg(jobs.size()));
    if (filterRunningLabel) filterRunningLabel->setText(QStringLiteral("Running   %1").arg(running));
    if (filterQueuedLabel) filterQueuedLabel->setText(QStringLiteral("Queued   %1").arg(pending));
    if (filterCompletedLabel) filterCompletedLabel->setText(QStringLiteral("Completed   %1").arg(done));
    if (filterFailedLabel) filterFailedLabel->setText(QStringLiteral("Failed   %1").arg(failed));
    if (totalProgressBar) {
      totalProgressBar->setRange(0, 100);
      totalProgressBar->setValue(jobs.isEmpty() ? 0 : totalProgress / jobs.size());
    }
    if (startButton) startButton->setEnabled(pending > 0 || running > 0);
    if (pauseButton) pauseButton->setEnabled(running > 0);
    if (cancelButton) cancelButton->setEnabled(running > 0);
    if (clearButton) clearButton->setEnabled(done > 0);
    if (rerunDoneFailedButton) rerunDoneFailedButton->setEnabled(done > 0 || failed > 0);
  }

  void handleJobSelected() {
    bool has = selectedSourceIndex() >= 0;
    if (removeButton) removeButton->setEnabled(has);
    if (duplicateButton) duplicateButton->setEnabled(has);
    if (applySettingsToSelectionButton) {
      int selectedCount = 0;
      if (jobListWidget) {
        for (int row = 0; row < jobListWidget->count(); ++row) {
          if (auto *item = jobListWidget->item(row); item && item->isSelected()) {
            ++selectedCount;
          }
        }
      }
      applySettingsToSelectionButton->setEnabled(has && selectedCount > 1);
    }
    if (outputSettingsButton) outputSettingsButton->setEnabled(has);
    if (jobListWidget) {
      const auto& theme = ArtifactCore::currentDCCTheme();
      for (int row = 0; row < jobListWidget->count(); ++row) {
        auto* item = jobListWidget->item(row);
        auto* card = static_cast<RenderQueueJobCard*>(jobListWidget->itemWidget(item));
        if (!card) continue;
        card->setAutoFillBackground(true);
        QPalette palette = card->palette();
        palette.setColor(QPalette::Window,
            row == jobListWidget->currentRow()
                ? QColor(theme.selectionColor).darker(125)
                : QColor(theme.secondaryBackgroundColor));
        card->setPalette(palette);
        card->setFrameStyle(row == jobListWidget->currentRow()
            ? QFrame::StyledPanel | QFrame::Sunken
            : QFrame::StyledPanel | QFrame::Plain);
      }
    }
    updateSummary();
  }

  void syncDetailEditorsFromJob(int index) {
    syncingJobDetails = true;
    if (!service || index < 0 || index >= service->jobCount()) {
      if (inspectorJobLabel) inspectorJobLabel->setText(QStringLiteral("No job selected"));
      if (preflightBadge) preflightBadge->setText(QStringLiteral("PREFLIGHT  •  SELECT A JOB"));
      if (previewLabel) {
        previewLabel->clear();
        previewLabel->setText(QStringLiteral("Select a job to preview"));
      }
      if (previewSummaryLabel) previewSummaryLabel->clear();
      if (selectedRangesSummaryLabel) {
        selectedRangesSummaryLabel->setText(QStringLiteral("Ranges: none"));
      }
      if (layerFilterSummaryLabel) {
        layerFilterSummaryLabel->setText(QStringLiteral(
            "Mode: All\nIncluded: none\nExcluded: none"));
      }
      if (selectedRangeStartSpin) selectedRangeStartSpin->setEnabled(false);
      if (selectedRangeEndSpin) selectedRangeEndSpin->setEnabled(false);
      if (addSelectedRangeButton) addSelectedRangeButton->setEnabled(false);
      if (clearSelectedRangesButton) clearSelectedRangesButton->setEnabled(false);
      if (configurePassesButton) {
        configurePassesButton->setText(QStringLiteral("Configure Passes…"));
        configurePassesButton->setEnabled(false);
      }
      syncingJobDetails = false;
      return;
    }
    if (inspectorJobLabel) {
      inspectorJobLabel->setText(service->jobNameAt(index));
    }

    const QSignalBlocker blockPath(outputPathEdit);
    const QSignalBlocker blockStart(startFrameSpin);
    const QSignalBlocker blockEnd(endFrameSpin);
    const QSignalBlocker blockX(overlayXSpin);
    const QSignalBlocker blockY(overlayYSpin);
    const QSignalBlocker blockScale(overlayScaleSpin);
    const QSignalBlocker blockRotation(overlayRotationSpin);
    const QSignalBlocker blockExclude(excludeAdjustmentLayersCheck);
    const QSignalBlocker blockGuide(excludeGuideLayersCheck);
    const QSignalBlocker blockSplit(splitPassesCheck);
    const QSignalBlocker blockResolution(resolutionPresetCombo);
    const QSignalBlocker blockFrameMode(frameRangeModeCombo);
    const QSignalBlocker blockSelectedRangeStart(selectedRangeStartSpin);
    const QSignalBlocker blockSelectedRangeEnd(selectedRangeEndSpin);
    const QSignalBlocker blockRegionMode(regionModeCombo);
    const QSignalBlocker blockLayerFilter(layerFilterModeCombo);
    const QSignalBlocker blockCropX(cropXSpin);
    const QSignalBlocker blockCropY(cropYSpin);
    const QSignalBlocker blockCropW(cropWSpin);
    const QSignalBlocker blockCropH(cropHSpin);

    if (outputPathEdit) outputPathEdit->setText(service->jobOutputPathAt(index));

    if (outputSettingsSummaryLabel) {
      QString outputFormat;
      QString codec;
      QString codecProfile;
      int width = 0;
      int height = 0;
      double fps = 0.0;
      int bitrateKbps = 0;
      service->jobOutputSettingsAt(index, &outputFormat, &codec, &codecProfile, &width, &height, &fps, &bitrateKbps);
      const QString renderBackend = service->jobRenderBackendAt(index);
      const QString encoderBackend = service->jobEncoderBackendAt(index);
      const bool audioEnabled = service->jobIntegratedRenderEnabledAt(index);
      const int audioSampleRate = service->jobAudioSampleRateAt(index);
      const QString audioRateText = audioSampleRate > 0
          ? QStringLiteral("%1kHz").arg(audioSampleRate / 1000)
          : QStringLiteral("source rate");
      const QString audioInfo = audioEnabled
          ? QStringLiteral(" | Audio: %1@%2kbps/%3/%4")
                .arg(service->jobAudioCodecAt(index))
                .arg(service->jobAudioBitrateKbpsAt(index))
                .arg(service->jobAudioChannelModeAt(index), audioRateText)
          : QStringLiteral(" | Audio: off");
      const auto preflight = service->preflightRenderQueueAt(index);
      if (previewSummaryLabel) {
        int previewStartFrame = 0;
        int previewEndFrame = 0;
        service->jobFrameRangeAt(index, &previewStartFrame, &previewEndFrame);
        const QVariantMap selective = service->jobSelectiveSettingsAt(index);
        const QVariantMap rangeLabels{{QStringLiteral("0"), QStringLiteral("Composition")},
                                      {QStringLiteral("1"), QStringLiteral("Work Area")},
                                      {QStringLiteral("2"), QStringLiteral("Custom")},
                                      {QStringLiteral("3"), QStringLiteral("Selected Frames")},
                                      {QStringLiteral("4"), QStringLiteral("Current Frame")}};
        const QVariantMap regionLabels{{QStringLiteral("0"), QStringLiteral("Full")},
                                       {QStringLiteral("1"), QStringLiteral("Region of Interest")},
                                       {QStringLiteral("2"), QStringLiteral("Custom Crop")}};
        const QVariantMap filterLabels{{QStringLiteral("0"), QStringLiteral("All Layers")},
                                       {QStringLiteral("1"), QStringLiteral("Selected Layers")},
                                       {QStringLiteral("2"), QStringLiteral("Solo Layers")},
                                       {QStringLiteral("3"), QStringLiteral("Visible Layers")},
                                       {QStringLiteral("4"), QStringLiteral("Custom Layers")}};
        const QVariantMap resolutionLabels{{QStringLiteral("0"), QStringLiteral("Custom")},
                                           {QStringLiteral("1"), QStringLiteral("Composition")},
                                           {QStringLiteral("2"), QStringLiteral("Half")},
                                           {QStringLiteral("3"), QStringLiteral("Third")},
                                           {QStringLiteral("4"), QStringLiteral("Quarter")}};
        const auto lookup = [](const QVariantMap& labels, int value,
                               const QString& fallback) {
          return labels.value(QString::number(value), fallback).toString();
        };
        QString selectiveSummary = QStringLiteral("Range: %1 | Region: %2 | Layers: %3 | Resolution: %4")
            .arg(lookup(rangeLabels, selective.value(QStringLiteral("frameRangeMode")).toInt(), QStringLiteral("Composition")))
            .arg(lookup(regionLabels, selective.value(QStringLiteral("regionMode")).toInt(), QStringLiteral("Full")))
            .arg(lookup(filterLabels, selective.value(QStringLiteral("layerFilterMode")).toInt(), QStringLiteral("All Layers")))
            .arg(lookup(resolutionLabels, selective.value(QStringLiteral("resolutionPreset")).toInt(), QStringLiteral("Composition")));
        if (selective.value(QStringLiteral("frameRangeMode")).toInt() == 3) {
          const int selectedRangeCount = selective.value(
              QStringLiteral("selectedFrameRanges")).toList().size();
          selectiveSummary += QStringLiteral(" | Selected ranges: %1")
              .arg(selectedRangeCount);
        }
        const bool excludeAdjustmentLayers = selective.value(
            QStringLiteral("excludeAdjustmentLayers")).toBool();
        const bool excludeGuideLayers = selective.value(
            QStringLiteral("excludeGuideLayers")).toBool();
        const bool splitPasses = selective.value(
            QStringLiteral("splitPasses")).toBool();
        if (excludeAdjustmentLayers || excludeGuideLayers || splitPasses) {
          selectiveSummary += QStringLiteral(" | %1%2%3")
              .arg(excludeAdjustmentLayers ? QStringLiteral("No Adjustments") : QString())
              .arg(excludeGuideLayers
                       ? ((excludeAdjustmentLayers) ? QStringLiteral(", No Guides")
                                                     : QStringLiteral("No Guides"))
                       : QString())
              .arg(splitPasses
                       ? ((excludeAdjustmentLayers || excludeGuideLayers)
                              ? QStringLiteral(", Split Passes")
                                                   : QStringLiteral("Split Passes"))
                       : QString());
        }
        if (splitPasses) {
          QStringList passNames;
          for (const auto& rawPass : selective.value(
                   QStringLiteral("renderPasses")).toList()) {
            const QVariantMap pass = rawPass.toMap();
            if (pass.value(QStringLiteral("enabled"), true).toBool()) {
              const QString name = pass.value(QStringLiteral("name")).toString().trimmed();
              if (!name.isEmpty()) {
                passNames.append(name);
              }
            }
          }
          if (!passNames.isEmpty()) {
            selectiveSummary += QStringLiteral(" | Passes: %1")
                .arg(passNames.join(QStringLiteral(", ")));
          }
        }
        const int whitelistCount = selective.value(
            QStringLiteral("layerWhitelist")).toStringList().size();
        const int blacklistCount = selective.value(
            QStringLiteral("layerBlacklist")).toStringList().size();
        if (whitelistCount > 0 || blacklistCount > 0) {
          selectiveSummary += QStringLiteral(" | Layer IDs: %1 included / %2 excluded")
              .arg(whitelistCount)
              .arg(blacklistCount);
        }
        if (selective.value(QStringLiteral("regionMode")).toInt() != 0) {
          selectiveSummary += QStringLiteral(" | Crop: %1,%2 %3×%4")
              .arg(selective.value(QStringLiteral("cropX")).toDouble(), 0, 'f', 1)
              .arg(selective.value(QStringLiteral("cropY")).toDouble(), 0, 'f', 1)
              .arg(selective.value(QStringLiteral("cropW")).toDouble(), 0, 'f', 1)
              .arg(selective.value(QStringLiteral("cropH")).toDouble(), 0, 'f', 1);
        }
        previewSummaryLabel->setText(
            QStringLiteral("Format: %1 (%2)\nResolution: %3 × %4\nFrame Rate: %5 FPS\nFrames: %6 – %7\n%8")
                .arg(outputFormat.isEmpty() ? QStringLiteral("MP4") : outputFormat)
                .arg(codec.isEmpty() ? QStringLiteral("H.264") : codec)
                .arg(width > 0 ? QString::number(width) : QStringLiteral("Auto"))
                .arg(height > 0 ? QString::number(height) : QStringLiteral("Auto"))
                .arg(fps > 0.0 ? QString::number(fps, 'f', 2) : QStringLiteral("Auto"))
                .arg(previewStartFrame)
                .arg(previewEndFrame)
                .arg(selectiveSummary));
      }
      outputSettingsSummaryLabel->setText(
          QString("Format: %1 | Codec: %2%3\nBackends: Encode %4  •  Render %5%6\nPreflight: %7 errors  •  %8 warnings")
              .arg(outputFormat.isEmpty() ? QStringLiteral("MP4") : outputFormat)
              .arg(codec.isEmpty() ? QStringLiteral("H.264") : codec)
              .arg(codecProfile.trimmed().isEmpty() ? QString() : QStringLiteral(" (%1)").arg(codecProfile))
              .arg(encoderBackend)
              .arg(renderBackend)
              .arg(audioInfo)
              .arg(preflight.getErrorCount())
              .arg(preflight.getWarningCount()));
      if (preflightBadge) {
        const int errors = preflight.getErrorCount();
        const int warnings = preflight.getWarningCount();
        preflightBadge->setText(errors > 0
            ? QStringLiteral("PREFLIGHT  •  %1 ERRORS").arg(errors)
            : (warnings > 0
                ? QStringLiteral("PREFLIGHT  •  %1 WARNINGS").arg(warnings)
                : QStringLiteral("PREFLIGHT  •  READY")));
        QPalette badgePalette = preflightBadge->palette();
        badgePalette.setColor(QPalette::WindowText,
            errors > 0 ? QColor(220, 90, 80)
                        : (warnings > 0 ? QColor(225, 175, 70)
                                        : QColor(110, 205, 120)));
        preflightBadge->setPalette(badgePalette);
      }
    }

    int startFrame = 0;
    int endFrame = 0;
    if (service->jobFrameRangeAt(index, &startFrame, &endFrame)) {
      if (startFrameSpin) startFrameSpin->setValue(startFrame);
      if (endFrameSpin) endFrameSpin->setValue(endFrame);
    }
    const QVariantMap selective = service->jobSelectiveSettingsAt(index);
    if (excludeAdjustmentLayersCheck) {
      excludeAdjustmentLayersCheck->setChecked(
          selective.value(QStringLiteral("excludeAdjustmentLayers")).toBool());
    }
    if (excludeGuideLayersCheck) {
      excludeGuideLayersCheck->setChecked(
          selective.value(QStringLiteral("excludeGuideLayers")).toBool());
    }
    if (splitPassesCheck) {
      splitPassesCheck->setChecked(
          selective.value(QStringLiteral("splitPasses")).toBool());
    }
    if (configurePassesButton) {
      configurePassesButton->setEnabled(
          selective.value(QStringLiteral("splitPasses")).toBool());
    }
    if (resolutionPresetCombo) {
      const int preset = selective.value(QStringLiteral("resolutionPreset"), 1).toInt();
      const int comboIndex = resolutionPresetCombo->findData(preset);
      resolutionPresetCombo->setCurrentIndex(comboIndex >= 0 ? comboIndex : 1);
    }
    if (frameRangeModeCombo) {
      const int mode = selective.value(QStringLiteral("frameRangeMode"), 0).toInt();
      const int comboIndex = frameRangeModeCombo->findData(mode);
      frameRangeModeCombo->setCurrentIndex(comboIndex >= 0 ? comboIndex : 0);
      const bool selectedRangeEnabled = mode == 3;
      if (selectedRangeStartSpin) selectedRangeStartSpin->setEnabled(selectedRangeEnabled);
      if (selectedRangeEndSpin) selectedRangeEndSpin->setEnabled(selectedRangeEnabled);
      const QVariantList ranges = selective.value(
          QStringLiteral("selectedFrameRanges")).toList();
      if (addSelectedRangeButton) addSelectedRangeButton->setEnabled(selectedRangeEnabled);
      if (clearSelectedRangesButton) {
        clearSelectedRangesButton->setEnabled(selectedRangeEnabled && !ranges.isEmpty());
      }
      if (selectedRangesSummaryLabel) {
        QStringList rangeLabels;
        for (const auto& rawRange : ranges) {
          const QVariantMap range = rawRange.toMap();
          rangeLabels.append(QStringLiteral("[%1, %2)")
                                 .arg(range.value(QStringLiteral("start"), 0).toInt())
                                 .arg(range.value(QStringLiteral("end"), 1).toInt()));
        }
        selectedRangesSummaryLabel->setText(
            rangeLabels.isEmpty() ? QStringLiteral("Ranges: none")
                                  : QStringLiteral("Ranges: %1")
                                        .arg(rangeLabels.join(QStringLiteral(", "))));
      }
      if (!ranges.isEmpty()) {
        const QVariantMap firstRange = ranges.first().toMap();
        if (selectedRangeStartSpin) {
          selectedRangeStartSpin->setValue(firstRange.value(QStringLiteral("start"), 0).toInt());
        }
        if (selectedRangeEndSpin) {
          selectedRangeEndSpin->setValue(firstRange.value(QStringLiteral("end"), 1).toInt());
        }
      } else if (selectedRangeEnabled) {
        int jobStart = 0;
        int jobEnd = 1;
        if (service->jobFrameRangeAt(index, &jobStart, &jobEnd)) {
          if (selectedRangeStartSpin) selectedRangeStartSpin->setValue(jobStart);
          if (selectedRangeEndSpin) selectedRangeEndSpin->setValue(std::max(jobStart + 1, jobEnd));
        }
      }
    }
    if (regionModeCombo) {
      const int mode = selective.value(QStringLiteral("regionMode"), 0).toInt();
      const int comboIndex = regionModeCombo->findData(mode);
      regionModeCombo->setCurrentIndex(comboIndex >= 0 ? comboIndex : 0);
      const bool cropEnabled = mode != 0;
      if (cropXSpin) cropXSpin->setEnabled(cropEnabled);
      if (cropYSpin) cropYSpin->setEnabled(cropEnabled);
      if (cropWSpin) cropWSpin->setEnabled(cropEnabled);
      if (cropHSpin) cropHSpin->setEnabled(cropEnabled);
    }
    if (clearWhitelistButton) {
      clearWhitelistButton->setEnabled(
          !selective.value(QStringLiteral("layerWhitelist")).toStringList().isEmpty());
    }
    if (clearBlacklistButton) {
      clearBlacklistButton->setEnabled(
          !selective.value(QStringLiteral("layerBlacklist")).toStringList().isEmpty());
    }
    if (layerFilterSummaryLabel) {
      const QStringList whitelist = selective.value(
          QStringLiteral("layerWhitelist")).toStringList();
      const QStringList blacklist = selective.value(
          QStringLiteral("layerBlacklist")).toStringList();
      const int filterMode = selective.value(QStringLiteral("layerFilterMode"), 0).toInt();
      const QStringList modeLabels{QStringLiteral("All"), QStringLiteral("Selected"),
                                  QStringLiteral("Solo"), QStringLiteral("Visible"),
                                  QStringLiteral("Custom")};
      const QString modeText = filterMode >= 0 && filterMode < modeLabels.size()
          ? modeLabels.at(filterMode) : QStringLiteral("Unknown");
      const auto summarizeIds = [](const QStringList& ids) {
        QStringList shortened;
        for (const QString& id : ids) {
          const QString trimmed = id.trimmed();
          if (trimmed.isEmpty()) continue;
          shortened.append(trimmed.size() > 12
              ? trimmed.left(8) + QStringLiteral("…") + trimmed.right(3)
              : trimmed);
          if (shortened.size() >= 4) break;
        }
        QString result = shortened.join(QStringLiteral(", "));
        if (ids.size() > shortened.size()) {
          result += QStringLiteral(" … +%1").arg(ids.size() - shortened.size());
        }
        return result.isEmpty() ? QStringLiteral("none") : result;
      };
      layerFilterSummaryLabel->setText(
          QStringLiteral("Mode: %1\nIncluded: %2\nExcluded: %3")
              .arg(modeText, summarizeIds(whitelist), summarizeIds(blacklist)));
    }
    if (layerFilterModeCombo) {
      const int mode = selective.value(QStringLiteral("layerFilterMode"), 0).toInt();
      const int comboIndex = layerFilterModeCombo->findData(mode);
      layerFilterModeCombo->setCurrentIndex(comboIndex >= 0 ? comboIndex : 0);
    }
    if (configurePassesButton) {
      int enabledPassCount = 0;
      for (const auto& rawPass : selective.value(
               QStringLiteral("renderPasses")).toList()) {
        const QVariantMap pass = rawPass.toMap();
        if (pass.value(QStringLiteral("enabled"), true).toBool() &&
            !pass.value(QStringLiteral("name")).toString().trimmed().isEmpty()) {
          ++enabledPassCount;
        }
      }
      configurePassesButton->setText(
          enabledPassCount > 0
              ? QStringLiteral("Configure Passes (%1)…").arg(enabledPassCount)
              : QStringLiteral("Configure Passes…"));
      configurePassesButton->setEnabled(
          selective.value(QStringLiteral("splitPasses")).toBool());
    }
    if (cropXSpin) cropXSpin->setValue(selective.value(QStringLiteral("cropX"), 0).toInt());
    if (cropYSpin) cropYSpin->setValue(selective.value(QStringLiteral("cropY"), 0).toInt());
    if (cropWSpin) cropWSpin->setValue(selective.value(QStringLiteral("cropW"), 0).toInt());
    if (cropHSpin) cropHSpin->setValue(selective.value(QStringLiteral("cropH"), 0).toInt());

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
  setAccessibleName(QStringLiteral("Render Queue"));
  setAccessibleDescription(
      QStringLiteral("Review, reorder, and monitor composition render jobs."));
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 10, 10, 8);
  layout->setSpacing(8);

  const auto& theme = ArtifactCore::currentDCCTheme();
  setAutoFillBackground(true);
  {
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, QColor(theme.backgroundColor));
    palette.setColor(QPalette::WindowText, QColor(theme.textColor));
    setPalette(palette);
  }
  
  // Header
  auto* top = new QHBoxLayout();
  top->setContentsMargins(4, 0, 4, 4);
  top->setSpacing(12);
  auto* title = new QLabel("RENDER MANAGER");
  title->setObjectName("renderQueueTitle");
  QFont titleFont = title->font();
  titleFont.setPointSize(titleFont.pointSize() + 2);
  titleFont.setBold(true);
  title->setFont(titleFont);
  impl_->runningCountLabel = new QLabel("0 RUNNING");
  impl_->runningCountLabel->setMinimumWidth(96);
  impl_->runningCountLabel->setAlignment(Qt::AlignCenter);
  impl_->searchEdit = new RenderQueueSearchEdit();
  impl_->searchEdit->setPlaceholderText("Search jobs...");
  impl_->searchEdit->setMinimumHeight(
      Artifact::Accessibility::scaledSize(24));
  impl_->searchEdit->setAccessibleName(QStringLiteral("Render job search"));
  impl_->searchEdit->setAccessibleDescription(
      QStringLiteral("Filter the render queue by job name or composition."));
  impl_->searchEdit->setObjectName("renderQueueSearch");
  impl_->searchEdit->setMaximumWidth(440);
  impl_->searchEdit->changed = [this](const QString& text) {
    if (!impl_) return;
    impl_->searchQuery = text;
    impl_->updateJobList();
    impl_->handleJobSelected();
  };
  impl_->addButton = new QPushButton("+  Add Composition");
  impl_->addButton->setAccessibleName(QStringLiteral("Add composition"));
  impl_->addButton->setAccessibleDescription(
      QStringLiteral("Add a composition as a render job."));
  impl_->addButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/add.svg")));
  {
    QPalette buttonPalette = impl_->addButton->palette();
    buttonPalette.setColor(QPalette::Button, QColor(theme.selectionColor));
    buttonPalette.setColor(QPalette::ButtonText, QColor(theme.textColor));
    impl_->addButton->setAutoFillBackground(true);
    impl_->addButton->setPalette(buttonPalette);
  }
  auto* batchAllBtn = new QPushButton(QStringLiteral("Add All"));
  batchAllBtn->setAccessibleName(QStringLiteral("Add all compositions"));
  batchAllBtn->setAccessibleDescription(
      QStringLiteral("Add every composition to the render queue."));
  batchAllBtn->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/playlist_add.svg")));
  batchAllBtn->setToolTip(QStringLiteral("Add all compositions to queue"));
  auto* batchTmplBtn = new QPushButton(QStringLiteral("Batch Template"));
  batchTmplBtn->setAccessibleName(QStringLiteral("Add batch template"));
  batchTmplBtn->setAccessibleDescription(
      QStringLiteral("Add render jobs using a batch template."));
  batchTmplBtn->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/compositionmenu_presets.svg")));
  batchTmplBtn->setToolTip(QStringLiteral("Batch add using a template"));
  auto* presetButton = new QToolButton();
  presetButton->setText(QStringLiteral("Preset:  H.264 High Quality"));
  presetButton->setAccessibleName(QStringLiteral("Render preset"));
  presetButton->setAccessibleDescription(
      QStringLiteral("Choose the output preset for queued render jobs."));
  presetButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/compositionmenu_presets.svg")));
  presetButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  top->addWidget(title);
  top->addWidget(impl_->runningCountLabel);
  top->addWidget(impl_->searchEdit, 1);
  top->addWidget(impl_->addButton);
  top->addWidget(batchAllBtn);
  top->addWidget(presetButton);
  layout->addLayout(top);

  // Main Splitter
  auto* splitter = new QSplitter(Qt::Horizontal);
  splitter->setChildrenCollapsible(false);

  auto* filterSide = new QFrame();
  filterSide->setFrameShape(QFrame::StyledPanel);
  filterSide->setMinimumWidth(216);
  filterSide->setMaximumWidth(244);
  auto* filterLayout = new QVBoxLayout(filterSide);
  filterLayout->setContentsMargins(12, 12, 12, 12);
  filterLayout->setSpacing(8);
  auto* filtersTitle = new QLabel(QStringLiteral("FILTERS"));
  QFont filtersTitleFont = filtersTitle->font();
  filtersTitleFont.setBold(true);
  filtersTitle->setFont(filtersTitleFont);
  filterLayout->addWidget(filtersTitle);
  const auto addFilterLabel = [&filterLayout, &theme](
                                  const QString& text,
                                  const QString& iconName,
                                  bool active = false) -> QLabel* {
    auto* rowHost = new QWidget();
    auto* rowLayout = new QHBoxLayout(rowHost);
    rowLayout->setContentsMargins(8, 0, 8, 0);
    rowLayout->setSpacing(9);
    auto* icon = new QLabel();
    icon->setFixedSize(18, 18);
    icon->setPixmap(loadIconWithFallback(iconName).pixmap(QSize(16, 16)));
    auto* row = new QLabel(text);
    row->setMinimumHeight(34);
    row->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    if (active) {
      rowHost->setAutoFillBackground(true);
      QPalette rowPalette = rowHost->palette();
      rowPalette.setColor(QPalette::Window, QColor(theme.selectionColor).darker(130));
      rowPalette.setColor(QPalette::WindowText, QColor(theme.textColor));
      rowHost->setPalette(rowPalette);
    }
    rowLayout->addWidget(icon);
    rowLayout->addWidget(row, 1);
    filterLayout->addWidget(rowHost);
    return row;
  };
  impl_->filterAllLabel = addFilterLabel(
      QStringLiteral("All"), QStringLiteral("Studio/effectmenu_layers.svg"), true);
  impl_->filterRunningLabel = addFilterLabel(
      QStringLiteral("Running"), QStringLiteral("Studio/figma_media_play.svg"));
  impl_->filterQueuedLabel = addFilterLabel(
      QStringLiteral("Queued"), QStringLiteral("Studio/animationmenu_schedule.svg"));
  impl_->filterCompletedLabel = addFilterLabel(
      QStringLiteral("Completed"), QStringLiteral("Studio/check_circle.svg"));
  impl_->filterFailedLabel = addFilterLabel(
      QStringLiteral("Failed"), QStringLiteral("Studio/asset_missing_small.svg"));
  auto* filterDivider = new QFrame();
  filterDivider->setFrameShape(QFrame::HLine);
  filterDivider->setFrameShadow(QFrame::Sunken);
  filterLayout->addWidget(filterDivider);
  addFilterLabel(QStringLiteral("Presets"),
                 QStringLiteral("Studio/effectmenu_tune.svg"));
  addFilterLabel(QStringLiteral("History"),
                 QStringLiteral("Studio/editmenu_history.svg"));
  filterLayout->addStretch();
  splitter->addWidget(filterSide);

  impl_->jobListWidget = new RenderQueueListWidget();
  impl_->jobListWidget->setObjectName("renderQueueList");
  impl_->jobListWidget->setAccessibleName(QStringLiteral("Render jobs"));
  impl_->jobListWidget->setAccessibleDescription(
      QStringLiteral("Select, reorder, and inspect queued render jobs."));
  impl_->jobListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  impl_->jobListWidget->setDragEnabled(true);
  impl_->jobListWidget->setAcceptDrops(true);
  impl_->jobListWidget->setDropIndicatorShown(true);
  impl_->jobListWidget->setDragDropMode(QAbstractItemView::InternalMove);
  impl_->jobListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
  impl_->jobListWidget->reordered = [this](int visibleFrom, int visibleTo) {
    if (!impl_ || !impl_->service) return;
    if (visibleFrom < 0 || visibleFrom >= impl_->visibleToSource.size()
        || visibleTo < 0 || visibleTo >= impl_->visibleToSource.size()) return;
    impl_->service->moveRenderQueue(impl_->visibleToSource[visibleFrom],
                                    impl_->visibleToSource[visibleTo]);
    impl_->syncJobsFromService();
  };
  impl_->jobListWidget->setAlternatingRowColors(true);
  impl_->jobListWidget->setSpacing(4);
  impl_->jobListWidget->setMinimumWidth(560);
  
  auto* leftSide = new QWidget();
  leftSide->setMinimumWidth(560);
  auto* leftLayout = new QVBoxLayout(leftSide);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  auto* queueHeader = new QHBoxLayout();
  auto* queueTitle = new QLabel(QStringLiteral("QUEUE"));
  QFont queueTitleFont = queueTitle->font();
  queueTitleFont.setBold(true);
  queueTitle->setFont(queueTitleFont);
  impl_->queueStateLabel = new QLabel(QStringLiteral("Queue is empty"));
  QPalette queueStatePalette = impl_->queueStateLabel->palette();
  queueStatePalette.setColor(QPalette::WindowText, QColor(theme.textColor).darker(125));
  impl_->queueStateLabel->setPalette(queueStatePalette);
  queueHeader->addWidget(queueTitle);
  queueHeader->addStretch();
  queueHeader->addWidget(impl_->queueStateLabel);
  leftLayout->addLayout(queueHeader);
  leftLayout->addWidget(impl_->jobListWidget);
  
  auto* btnLayout = new QHBoxLayout();
  impl_->removeButton = new QPushButton("Remove");
  impl_->removeButton->setAccessibleName(QStringLiteral("Remove render job"));
  impl_->removeButton->setAccessibleDescription(
      QStringLiteral("Remove the selected render job from the queue."));
  impl_->removeButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/delete.svg")));
  impl_->duplicateButton = new QToolButton();
  impl_->duplicateButton->setAccessibleName(QStringLiteral("Duplicate render job"));
  impl_->duplicateButton->setAccessibleDescription(
      QStringLiteral("Duplicate the selected render job."));
  impl_->duplicateButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/editmenu_duplicate.svg")));
  impl_->duplicateButton->setToolTip(QStringLiteral("Duplicate selected job"));
  btnLayout->addWidget(batchTmplBtn);
  btnLayout->addWidget(impl_->duplicateButton);
  impl_->applySettingsToSelectionButton =
      new QPushButton(QStringLiteral("Apply Settings"));
  impl_->applySettingsToSelectionButton->setAccessibleName(
      QStringLiteral("Apply settings to selected render jobs"));
  impl_->applySettingsToSelectionButton->setAccessibleDescription(
      QStringLiteral("Copy the current job's output settings and render backends to all selected jobs."));
  impl_->applySettingsToSelectionButton->setToolTip(
      QStringLiteral("Copy current job settings to selected jobs"));
  btnLayout->addWidget(impl_->applySettingsToSelectionButton);
  btnLayout->addStretch();
  impl_->clearButton = new RenderQueueActionButton(QStringLiteral("Clear Completed"));
  impl_->clearButton->setAccessibleName(QStringLiteral("Clear completed jobs"));
  impl_->clearButton->setAccessibleDescription(
      QStringLiteral("Remove completed jobs from the render queue."));
  impl_->clearButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/clear_all.svg")));
  impl_->clearButton->action = [this]() {
    if (!impl_ || !impl_->service) return;
    for (int index = impl_->service->jobCount() - 1; index >= 0; --index) {
      if (Impl::normalizeStatus(impl_->service->jobStatusAt(index)) == QStringLiteral("Completed")) {
        impl_->service->removeRenderQueueAt(index);
      }
    }
    impl_->syncJobsFromService();
  };
  btnLayout->addWidget(impl_->clearButton);
  impl_->rerunDoneFailedButton = new RenderQueueActionButton(QStringLiteral("Retry Failed"));
  impl_->rerunDoneFailedButton->setAccessibleName(QStringLiteral("Retry failed jobs"));
  impl_->rerunDoneFailedButton->setAccessibleDescription(
      QStringLiteral("Reset failed jobs and start them again."));
  impl_->rerunDoneFailedButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/replay.svg")));
  impl_->rerunDoneFailedButton->action = [this]() {
    if (!impl_ || !impl_->service) return;
    if (impl_->service->resetCompletedAndFailedJobsForRerun() > 0) {
      impl_->service->startAllJobs();
    }
  };
  btnLayout->addWidget(impl_->rerunDoneFailedButton);
  btnLayout->addWidget(impl_->removeButton);
  leftLayout->addLayout(btnLayout);
  
  splitter->addWidget(leftSide);
  
  // Right Pane: Job Details (Scrollable)
  auto* detailScroll = new QScrollArea();
  detailScroll->setMinimumWidth(340);
  detailScroll->setWidgetResizable(true);
  detailScroll->setObjectName("renderQueueDetailScroll");
  auto* detailWidget = new QWidget();
  detailWidget->setMinimumWidth(320);
  auto* detailLayout = new QVBoxLayout(detailWidget);
  detailLayout->setContentsMargins(8, 0, 8, 0);
  detailLayout->setSpacing(8);

  auto* inspectorHeader = new QHBoxLayout();
  auto* inspectorTitle = new QLabel(QStringLiteral("JOB SETTINGS"));
  QFont inspectorTitleFont = inspectorTitle->font();
  inspectorTitleFont.setBold(true);
  inspectorTitle->setFont(inspectorTitleFont);
  impl_->inspectorJobLabel = new QLabel(QStringLiteral("No job selected"));
  QPalette inspectorJobPalette = impl_->inspectorJobLabel->palette();
  inspectorJobPalette.setColor(QPalette::WindowText,
                               QColor(theme.textColor).darker(115));
  impl_->inspectorJobLabel->setPalette(inspectorJobPalette);
  inspectorHeader->addWidget(inspectorTitle);
  inspectorHeader->addStretch();
  inspectorHeader->addWidget(impl_->inspectorJobLabel);
  detailLayout->addLayout(inspectorHeader);

  impl_->previewLabel = new QLabel("Select a job to preview");
  impl_->previewLabel->setMinimumSize(176, 104);
  impl_->previewLabel->setMaximumSize(220, 132);
  {
    QPalette pal = impl_->previewLabel->palette();
    pal.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor));
    pal.setColor(QPalette::WindowText, QColor(theme.textColor).darker(120));
    impl_->previewLabel->setAutoFillBackground(true);
    impl_->previewLabel->setPalette(pal);
  }
  impl_->previewLabel->setAlignment(Qt::AlignCenter);
  impl_->previewLabel->setScaledContents(false);
  impl_->previewSummaryLabel = new QLabel();
  impl_->previewSummaryLabel->setWordWrap(true);
  {
    QPalette summaryPalette = impl_->previewSummaryLabel->palette();
    summaryPalette.setColor(QPalette::WindowText, QColor(theme.textColor).darker(110));
    impl_->previewSummaryLabel->setPalette(summaryPalette);
  }
  auto* previewCard = new QFrame();
  previewCard->setFrameShape(QFrame::StyledPanel);
  auto* previewCardLayout = new QHBoxLayout(previewCard);
  previewCardLayout->setContentsMargins(8, 8, 8, 8);
  previewCardLayout->setSpacing(12);
  previewCardLayout->addWidget(impl_->previewLabel);
  previewCardLayout->addWidget(impl_->previewSummaryLabel, 1,
                               Qt::AlignTop);
  detailLayout->addWidget(previewCard);

  impl_->preflightBadge = new QLabel(QStringLiteral("PREFLIGHT  •  SELECT A JOB"));
  impl_->preflightBadge->setAlignment(Qt::AlignCenter);
  impl_->preflightBadge->setMinimumHeight(26);
  detailLayout->addWidget(impl_->preflightBadge);

  // Group: Output
  auto* outputGroup = new QGroupBox("Output Settings");
  auto* outputLayout = new QFormLayout(outputGroup);
  impl_->outputPathEdit = new RenderQueuePathEdit();
  impl_->outputPathEdit->committed = [this](const QString& path) {
    if (!impl_ || impl_->syncingJobDetails || !impl_->service) return;
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) return;
    impl_->service->setJobOutputPathAt(index, path.trimmed());
    impl_->logUiEvent(QStringLiteral("Output path updated"), false);
  };
  auto* outputPathRow = new QWidget();
  auto* outputPathRowLayout = new QHBoxLayout(outputPathRow);
  outputPathRowLayout->setContentsMargins(0, 0, 0, 0);
  outputPathRowLayout->addWidget(impl_->outputPathEdit, 1);
  impl_->outputPathEdit->setAccessibleName(QStringLiteral("Render output path"));
  impl_->outputPathEdit->setAccessibleDescription(
      QStringLiteral("Enter or review the output file path for the selected render job"));
  impl_->outputBrowseButton = new RenderQueueActionButton(QStringLiteral("Browse"));
  impl_->outputBrowseButton->setAccessibleName(QStringLiteral("Browse output path"));
  impl_->outputBrowseButton->setAccessibleDescription(
      QStringLiteral("Choose the output file path for the selected render job"));
  impl_->outputBrowseButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/animationmenu_folder_open.svg")));
  impl_->outputBrowseButton->action = [this]() {
    if (!impl_ || !impl_->service) return;
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) return;
    const QString currentPath = impl_->service->jobOutputPathAt(index);
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Select render output"),
        currentPath.isEmpty() ? QDir::homePath() : currentPath,
        QStringLiteral("Video and image files (*.mp4 *.mov *.png *.exr);;All files (*)"));
    if (path.isEmpty()) return;
    impl_->outputPathEdit->setText(path);
    impl_->outputPathEdit->committed(path);
  };
  outputPathRowLayout->addWidget(impl_->outputBrowseButton);
  outputLayout->addRow("Path:", outputPathRow);
  impl_->outputSettingsButton = new QPushButton("Format...");
  impl_->outputSettingsButton->setAccessibleName(QStringLiteral("Render output settings"));
  impl_->outputSettingsButton->setAccessibleDescription(
      QStringLiteral("Configure format, codec, and render settings"));
  impl_->outputSettingsButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/compositionmenu_settings.svg")));
  outputLayout->addRow("Settings:", impl_->outputSettingsButton);
  impl_->outputSettingsSummaryLabel = new QLabel("Format: MP4 | Codec: H.264 | Encode: auto | Render: auto");
  impl_->outputSettingsSummaryLabel->setWordWrap(true);
  outputLayout->addRow("Summary:", impl_->outputSettingsSummaryLabel);
  detailLayout->addWidget(outputGroup);

  // Group: Range
  auto* rangeGroup = new QGroupBox("Frame Range");
  auto* rangeLayout = new QFormLayout(rangeGroup);
  impl_->startFrameSpin = new RenderQueueIntSpinBox();
  impl_->endFrameSpin = new RenderQueueIntSpinBox();
  impl_->startFrameSpin->setRange(0, 1000000);
  impl_->endFrameSpin->setRange(0, 1000000);
  impl_->startFrameSpin->setAccessibleName(QStringLiteral("Render start frame"));
  impl_->startFrameSpin->setAccessibleDescription(QStringLiteral("Set the first frame to render"));
  impl_->endFrameSpin->setAccessibleName(QStringLiteral("Render end frame"));
  impl_->endFrameSpin->setAccessibleDescription(QStringLiteral("Set the last frame to render"));
  auto commitFrameRange = [this]() {
    if (!impl_ || impl_->syncingJobDetails || !impl_->service) return;
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) return;
    impl_->service->setJobFrameRangeAt(index, impl_->startFrameSpin->value(),
                                       impl_->endFrameSpin->value());
  };
  impl_->startFrameSpin->committed = [commitFrameRange](int) { commitFrameRange(); };
  impl_->endFrameSpin->committed = [commitFrameRange](int) { commitFrameRange(); };
  rangeLayout->addRow("Start:", impl_->startFrameSpin);
  rangeLayout->addRow("End:", impl_->endFrameSpin);
  detailLayout->addWidget(rangeGroup);

  // Group: Selective render
  auto* selectiveGroup = new QGroupBox(QStringLiteral("Selective Render"));
  auto* selectiveLayout = new QVBoxLayout(selectiveGroup);
  impl_->excludeAdjustmentLayersCheck = new QCheckBox(
      QStringLiteral("Exclude Adjustment Layers"));
  impl_->excludeAdjustmentLayersCheck->setAccessibleName(
      QStringLiteral("Exclude adjustment layers"));
  impl_->excludeAdjustmentLayersCheck->setAccessibleDescription(
      QStringLiteral("Skip adjustment layers when rendering this job."));
  impl_->excludeGuideLayersCheck = new QCheckBox(
      QStringLiteral("Exclude Guide Layers"));
  impl_->excludeGuideLayersCheck->setAccessibleName(
      QStringLiteral("Exclude guide layers"));
  impl_->excludeGuideLayersCheck->setAccessibleDescription(
      QStringLiteral("Skip guide layers when rendering this job."));
  impl_->splitPassesCheck = new QCheckBox(QStringLiteral("Split Passes"));
  impl_->splitPassesCheck->setAccessibleName(QStringLiteral("Split render passes"));
  impl_->splitPassesCheck->setAccessibleDescription(
      QStringLiteral("Render enabled named passes as separate outputs."));
  selectiveLayout->addWidget(impl_->excludeAdjustmentLayersCheck);
  selectiveLayout->addWidget(impl_->excludeGuideLayersCheck);
  selectiveLayout->addWidget(impl_->splitPassesCheck);
  impl_->configurePassesButton = new QPushButton(QStringLiteral("Configure Passes…"));
  impl_->configurePassesButton->setAccessibleName(QStringLiteral("Configure render passes"));
  impl_->configurePassesButton->setAccessibleDescription(
      QStringLiteral("Edit the names of the enabled split render passes."));
  impl_->configurePassesButton->setEnabled(false);
  selectiveLayout->addWidget(impl_->configurePassesButton);
  auto* resolutionRow = new QHBoxLayout();
  resolutionRow->addWidget(new QLabel(QStringLiteral("Resolution:")));
  impl_->resolutionPresetCombo = new QComboBox();
  impl_->resolutionPresetCombo->addItem(QStringLiteral("Custom"), 0);
  impl_->resolutionPresetCombo->addItem(QStringLiteral("Composition"), 1);
  impl_->resolutionPresetCombo->addItem(QStringLiteral("Half"), 2);
  impl_->resolutionPresetCombo->addItem(QStringLiteral("Third"), 3);
  impl_->resolutionPresetCombo->addItem(QStringLiteral("Quarter"), 4);
  impl_->resolutionPresetCombo->setAccessibleName(QStringLiteral("Render resolution preset"));
  impl_->resolutionPresetCombo->setAccessibleDescription(
      QStringLiteral("Choose the output resolution preset for this render job."));
  resolutionRow->addWidget(impl_->resolutionPresetCombo, 1);
  selectiveLayout->addLayout(resolutionRow);
  auto* rangeModeRow = new QHBoxLayout();
  rangeModeRow->addWidget(new QLabel(QStringLiteral("Frame Range:")));
  impl_->frameRangeModeCombo = new QComboBox();
  impl_->frameRangeModeCombo->addItem(QStringLiteral("Composition"), 0);
  impl_->frameRangeModeCombo->addItem(QStringLiteral("Work Area"), 1);
  impl_->frameRangeModeCombo->addItem(QStringLiteral("Custom"), 2);
  impl_->frameRangeModeCombo->addItem(QStringLiteral("Selected Frames"), 3);
  impl_->frameRangeModeCombo->addItem(QStringLiteral("Current Frame"), 4);
  impl_->frameRangeModeCombo->setAccessibleName(QStringLiteral("Render frame range mode"));
  impl_->frameRangeModeCombo->setAccessibleDescription(
      QStringLiteral("Choose composition, work area, custom, selected, or current frame output."));
  rangeModeRow->addWidget(impl_->frameRangeModeCombo, 1);
  selectiveLayout->addLayout(rangeModeRow);
  auto* selectedRangeRow = new QHBoxLayout();
  selectedRangeRow->addWidget(new QLabel(QStringLiteral("Selected range:")));
  impl_->selectedRangeStartSpin = new RenderQueueIntSpinBox();
  impl_->selectedRangeEndSpin = new RenderQueueIntSpinBox();
  for (auto* spin : {impl_->selectedRangeStartSpin,
                     impl_->selectedRangeEndSpin}) {
    spin->setRange(std::numeric_limits<int>::min(),
                   std::numeric_limits<int>::max());
    spin->setEnabled(false);
  }
  impl_->selectedRangeStartSpin->setAccessibleName(
      QStringLiteral("Selected frame range start"));
  impl_->selectedRangeEndSpin->setAccessibleName(
      QStringLiteral("Selected frame range end"));
  selectedRangeRow->addWidget(impl_->selectedRangeStartSpin);
  selectedRangeRow->addWidget(new QLabel(QStringLiteral("to")));
  selectedRangeRow->addWidget(impl_->selectedRangeEndSpin);
  impl_->addSelectedRangeButton = new QPushButton(QStringLiteral("Add Range"));
  impl_->clearSelectedRangesButton = new QPushButton(QStringLiteral("Clear Ranges"));
  impl_->addSelectedRangeButton->setEnabled(false);
  impl_->clearSelectedRangesButton->setEnabled(false);
  selectedRangeRow->addWidget(impl_->addSelectedRangeButton);
  selectedRangeRow->addWidget(impl_->clearSelectedRangesButton);
  selectiveLayout->addLayout(selectedRangeRow);
  impl_->selectedRangesSummaryLabel = new QLabel(QStringLiteral("Ranges: none"));
  impl_->selectedRangesSummaryLabel->setWordWrap(true);
  impl_->selectedRangesSummaryLabel->setAccessibleName(
      QStringLiteral("Selected frame ranges summary"));
  selectiveLayout->addWidget(impl_->selectedRangesSummaryLabel);
  auto* regionModeRow = new QHBoxLayout();
  regionModeRow->addWidget(new QLabel(QStringLiteral("Region:")));
  impl_->regionModeCombo = new QComboBox();
  impl_->regionModeCombo->addItem(QStringLiteral("Full"), 0);
  impl_->regionModeCombo->addItem(QStringLiteral("Region of Interest"), 1);
  impl_->regionModeCombo->addItem(QStringLiteral("Custom Crop"), 2);
  impl_->regionModeCombo->setAccessibleName(QStringLiteral("Render region mode"));
  regionModeRow->addWidget(impl_->regionModeCombo, 1);
  selectiveLayout->addLayout(regionModeRow);
  auto* layerFilterRow = new QHBoxLayout();
  layerFilterRow->addWidget(new QLabel(QStringLiteral("Layers:")));
  impl_->layerFilterModeCombo = new QComboBox();
  impl_->layerFilterModeCombo->addItem(QStringLiteral("All Layers"), 0);
  impl_->layerFilterModeCombo->addItem(QStringLiteral("Selected Layers"), 1);
  impl_->layerFilterModeCombo->addItem(QStringLiteral("Solo Layers"), 2);
  impl_->layerFilterModeCombo->addItem(QStringLiteral("Visible Layers"), 3);
  impl_->layerFilterModeCombo->addItem(QStringLiteral("Custom Layers"), 4);
  impl_->layerFilterModeCombo->setAccessibleName(QStringLiteral("Render layer filter mode"));
  impl_->layerFilterModeCombo->setAccessibleDescription(
      QStringLiteral("Choose which layers are included in this render job."));
  layerFilterRow->addWidget(impl_->layerFilterModeCombo, 1);
  selectiveLayout->addLayout(layerFilterRow);
  impl_->useSelectedLayersButton = new QPushButton(
      QStringLiteral("Use Current Selection"));
  impl_->useSelectedLayersButton->setAccessibleName(
      QStringLiteral("Use current layer selection for render"));
  impl_->useSelectedLayersButton->setAccessibleDescription(
      QStringLiteral("Copy the current layer selection into the Custom Layers filter."));
  selectiveLayout->addWidget(impl_->useSelectedLayersButton);
  impl_->excludeSelectedLayersButton = new QPushButton(
      QStringLiteral("Exclude Current Selection"));
  impl_->excludeSelectedLayersButton->setAccessibleName(
      QStringLiteral("Exclude current layer selection from render"));
  impl_->excludeSelectedLayersButton->setAccessibleDescription(
      QStringLiteral("Add the current layer selection to the render blacklist."));
  selectiveLayout->addWidget(impl_->excludeSelectedLayersButton);
  auto* layerFilterClearRow = new QHBoxLayout();
  impl_->clearWhitelistButton = new QPushButton(QStringLiteral("Clear Included"));
  impl_->clearBlacklistButton = new QPushButton(QStringLiteral("Clear Excluded"));
  impl_->clearWhitelistButton->setAccessibleName(QStringLiteral("Clear included layers"));
  impl_->clearBlacklistButton->setAccessibleName(QStringLiteral("Clear excluded layers"));
  layerFilterClearRow->addWidget(impl_->clearWhitelistButton);
  layerFilterClearRow->addWidget(impl_->clearBlacklistButton);
  selectiveLayout->addLayout(layerFilterClearRow);
  impl_->layerFilterSummaryLabel = new QLabel(QStringLiteral(
      "Mode: All\nIncluded: none\nExcluded: none"));
  impl_->layerFilterSummaryLabel->setWordWrap(true);
  impl_->layerFilterSummaryLabel->setAccessibleName(
      QStringLiteral("Render layer filter summary"));
  selectiveLayout->addWidget(impl_->layerFilterSummaryLabel);
  auto* cropGrid = new QGridLayout();
  cropGrid->addWidget(new QLabel(QStringLiteral("Crop X:")), 0, 0);
  cropGrid->addWidget(new QLabel(QStringLiteral("Crop Y:")), 0, 2);
  cropGrid->addWidget(new QLabel(QStringLiteral("Width:")), 1, 0);
  cropGrid->addWidget(new QLabel(QStringLiteral("Height:")), 1, 2);
  impl_->cropXSpin = new RenderQueueIntSpinBox();
  impl_->cropYSpin = new RenderQueueIntSpinBox();
  impl_->cropWSpin = new RenderQueueIntSpinBox();
  impl_->cropHSpin = new RenderQueueIntSpinBox();
  for (auto* spin : {impl_->cropXSpin, impl_->cropYSpin,
                     impl_->cropWSpin, impl_->cropHSpin}) {
    spin->setRange(0, 1000000);
  }
  impl_->cropXSpin->setAccessibleName(QStringLiteral("Crop X"));
  impl_->cropYSpin->setAccessibleName(QStringLiteral("Crop Y"));
  impl_->cropWSpin->setAccessibleName(QStringLiteral("Crop width"));
  impl_->cropHSpin->setAccessibleName(QStringLiteral("Crop height"));
  cropGrid->addWidget(impl_->cropXSpin, 0, 1);
  cropGrid->addWidget(impl_->cropYSpin, 0, 3);
  cropGrid->addWidget(impl_->cropWSpin, 1, 1);
  cropGrid->addWidget(impl_->cropHSpin, 1, 3);
  selectiveLayout->addLayout(cropGrid);
  detailLayout->addWidget(selectiveGroup);

  auto commitSelectiveSetting = [this](const QString& key, bool value) {
    if (!impl_ || impl_->syncingJobDetails || !impl_->service) {
      return;
    }
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) {
      return;
    }
    QVariantMap settings = impl_->service->jobSelectiveSettingsAt(index);
    settings.insert(key, value);
    impl_->service->setJobSelectiveSettingsAt(index, settings);
    impl_->syncDetailEditorsFromJob(index);
  };
  connect(impl_->excludeAdjustmentLayersCheck, &QCheckBox::toggled, this,
          [commitSelectiveSetting](bool checked) {
            commitSelectiveSetting(QStringLiteral("excludeAdjustmentLayers"), checked);
          });
  connect(impl_->excludeGuideLayersCheck, &QCheckBox::toggled, this,
          [commitSelectiveSetting](bool checked) {
            commitSelectiveSetting(QStringLiteral("excludeGuideLayers"), checked);
          });
  connect(impl_->splitPassesCheck, &QCheckBox::toggled, this,
          [this, commitSelectiveSetting](bool checked) {
            if (!checked || !impl_ || impl_->syncingJobDetails ||
                !impl_->service) {
              commitSelectiveSetting(QStringLiteral("splitPasses"), checked);
              return;
            }
            const int index = impl_->selectedSourceIndex();
            if (index < 0 || index >= impl_->service->jobCount()) {
              return;
            }
            QVariantMap settings = impl_->service->jobSelectiveSettingsAt(index);
            settings.insert(QStringLiteral("splitPasses"), true);
            const QVariantList existingPasses = settings.value(
                QStringLiteral("renderPasses")).toList();
            if (existingPasses.isEmpty()) {
              QVariantMap beautyPass;
              beautyPass.insert(QStringLiteral("name"), QStringLiteral("Beauty"));
              beautyPass.insert(QStringLiteral("layerFilter"), 0);
              beautyPass.insert(QStringLiteral("enabled"), true);
              QVariantList defaultPasses;
              defaultPasses.append(beautyPass);
              settings.insert(QStringLiteral("renderPasses"), defaultPasses);
            }
            impl_->service->setJobSelectiveSettingsAt(index, settings);
            impl_->syncDetailEditorsFromJob(index);
          });
  connect(impl_->configurePassesButton, &QPushButton::clicked, this, [this]() {
    if (!impl_ || !impl_->service || !impl_->configurePassesButton) {
      return;
    }
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) {
      return;
    }
    const QVariantMap current = impl_->service->jobSelectiveSettingsAt(index);
    QStringList currentNames;
    for (const auto& rawPass : current.value(QStringLiteral("renderPasses")).toList()) {
      const QVariantMap pass = rawPass.toMap();
      if (pass.value(QStringLiteral("enabled"), true).toBool()) {
        const QString name = pass.value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty()) {
          currentNames.append(name);
        }
      }
    }
    bool accepted = false;
    const QString text = QInputDialog::getText(
        this, QStringLiteral("Configure Render Passes"),
        QStringLiteral("Enabled pass names (comma separated):"),
        QLineEdit::Normal, currentNames.join(QStringLiteral(", ")), &accepted);
    if (!accepted) {
      return;
    }
    QStringList names;
    for (const QString& rawName : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
      const QString name = rawName.trimmed();
      if (!name.isEmpty() && !names.contains(name, Qt::CaseInsensitive)) {
        names.append(name);
      }
    }
    if (names.isEmpty()) {
      return;
    }
    QVariantList passes;
    for (const QString& name : names) {
      QVariantMap pass;
      pass.insert(QStringLiteral("name"), name);
      pass.insert(QStringLiteral("layerFilter"), 0);
      pass.insert(QStringLiteral("enabled"), true);
      passes.append(pass);
    }
    QVariantMap settings = current;
    settings.insert(QStringLiteral("splitPasses"), true);
    settings.insert(QStringLiteral("renderPasses"), passes);
    impl_->service->setJobSelectiveSettingsAt(index, settings);
    impl_->syncDetailEditorsFromJob(index);
  });
  connect(impl_->resolutionPresetCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int comboIndex) {
            if (!impl_ || impl_->syncingJobDetails || !impl_->service ||
                !impl_->resolutionPresetCombo) {
              return;
            }
            const int index = impl_->selectedSourceIndex();
            if (index < 0 || index >= impl_->service->jobCount()) {
              return;
            }
            QVariantMap settings = impl_->service->jobSelectiveSettingsAt(index);
            settings.insert(QStringLiteral("resolutionPreset"),
                            impl_->resolutionPresetCombo->itemData(comboIndex));
            impl_->service->setJobSelectiveSettingsAt(index, settings);
            impl_->syncDetailEditorsFromJob(index);
          });
  auto commitSelectiveCombo = [this](QComboBox* combo, const QString& key) {
    if (!impl_ || impl_->syncingJobDetails || !impl_->service || !combo) {
      return;
    }
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) {
      return;
    }
    QVariantMap settings = impl_->service->jobSelectiveSettingsAt(index);
    settings.insert(key, combo->currentData());
    if (key == QStringLiteral("frameRangeMode") &&
        combo->currentData().toInt() == 3 &&
        settings.value(QStringLiteral("selectedFrameRanges")).toList().isEmpty()) {
      int startFrame = 0;
      int endFrame = 0;
      if (impl_->service->jobFrameRangeAt(index, &startFrame, &endFrame) &&
          endFrame > startFrame) {
        QVariantMap initialRange;
        initialRange.insert(QStringLiteral("start"), startFrame);
        initialRange.insert(QStringLiteral("end"), endFrame);
        settings.insert(QStringLiteral("selectedFrameRanges"),
                        QVariantList{initialRange});
      }
    }
    impl_->service->setJobSelectiveSettingsAt(index, settings);
    impl_->syncDetailEditorsFromJob(index);
  };
  auto commitSelectedRange = [this]() {
    if (!impl_ || impl_->syncingJobDetails || !impl_->service ||
        !impl_->selectedRangeStartSpin || !impl_->selectedRangeEndSpin) {
      return;
    }
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) {
      return;
    }
    const int start = impl_->selectedRangeStartSpin->value();
    const int end = impl_->selectedRangeEndSpin->value();
    if (end <= start) {
      impl_->selectedRangeEndSpin->setValue(start + 1);
    }
    QVariantMap settings = impl_->service->jobSelectiveSettingsAt(index);
    QVariantMap range;
    range.insert(QStringLiteral("start"), start);
    range.insert(QStringLiteral("end"),
                 std::max(end, start + 1));
    settings.insert(QStringLiteral("selectedFrameRanges"),
                    QVariantList{range});
    settings.insert(QStringLiteral("frameRangeMode"), 3);
    impl_->service->setJobSelectiveSettingsAt(index, settings);
    impl_->syncDetailEditorsFromJob(index);
  };
  connect(impl_->frameRangeModeCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this, commitSelectiveCombo](int) {
            commitSelectiveCombo(impl_->frameRangeModeCombo,
                                 QStringLiteral("frameRangeMode"));
          });
  impl_->selectedRangeStartSpin->committed =
      [commitSelectedRange](int) { commitSelectedRange(); };
  impl_->selectedRangeEndSpin->committed =
      [commitSelectedRange](int) { commitSelectedRange(); };
  connect(impl_->addSelectedRangeButton, &QPushButton::clicked, this,
          [this]() {
            if (!impl_ || impl_->syncingJobDetails || !impl_->service ||
                !impl_->selectedRangeStartSpin || !impl_->selectedRangeEndSpin) {
              return;
            }
            const int index = impl_->selectedSourceIndex();
            if (index < 0 || index >= impl_->service->jobCount()) return;
            const int start = impl_->selectedRangeStartSpin->value();
            const int end = impl_->selectedRangeEndSpin->value();
            if (end <= start) return;
            QVariantMap settings = impl_->service->jobSelectiveSettingsAt(index);
            QVariantList ranges = settings.value(
                QStringLiteral("selectedFrameRanges")).toList();
            for (const auto &rawRange : ranges) {
              const QVariantMap existing = rawRange.toMap();
              if (existing.value(QStringLiteral("start")).toInt() == start &&
                  existing.value(QStringLiteral("end")).toInt() == end) {
                impl_->syncDetailEditorsFromJob(index);
                return;
              }
            }
            QVariantMap range;
            range.insert(QStringLiteral("start"), start);
            range.insert(QStringLiteral("end"), end);
            ranges.append(range);
            std::sort(ranges.begin(), ranges.end(), [](const QVariant &left,
                                                       const QVariant &right) {
              const QVariantMap lhs = left.toMap();
              const QVariantMap rhs = right.toMap();
              const int lhsStart = lhs.value(QStringLiteral("start")).toInt();
              const int rhsStart = rhs.value(QStringLiteral("start")).toInt();
              if (lhsStart != rhsStart) return lhsStart < rhsStart;
              return lhs.value(QStringLiteral("end")).toInt() <
                     rhs.value(QStringLiteral("end")).toInt();
            });
            QVariantList mergedRanges;
            for (const auto &rawRange : ranges) {
              const QVariantMap current = rawRange.toMap();
              const int currentStart = current.value(QStringLiteral("start")).toInt();
              const int currentEnd = current.value(QStringLiteral("end")).toInt();
              if (!mergedRanges.isEmpty()) {
                QVariantMap previous = mergedRanges.last().toMap();
                const int previousEnd = previous.value(QStringLiteral("end")).toInt();
                if (currentStart <= previousEnd) {
                  previous.insert(QStringLiteral("end"),
                                  std::max(previousEnd, currentEnd));
                  mergedRanges.last() = previous;
                  continue;
                }
              }
              mergedRanges.append(current);
            }
            settings.insert(QStringLiteral("selectedFrameRanges"), mergedRanges);
            settings.insert(QStringLiteral("frameRangeMode"), 3);
            impl_->service->setJobSelectiveSettingsAt(index, settings);
            impl_->syncDetailEditorsFromJob(index);
          });
  connect(impl_->clearSelectedRangesButton, &QPushButton::clicked, this,
          [this]() {
            if (!impl_ || impl_->syncingJobDetails || !impl_->service) return;
            const int index = impl_->selectedSourceIndex();
            if (index < 0 || index >= impl_->service->jobCount()) return;
            QVariantMap settings = impl_->service->jobSelectiveSettingsAt(index);
            settings.insert(QStringLiteral("selectedFrameRanges"), QVariantList{});
            impl_->service->setJobSelectiveSettingsAt(index, settings);
            impl_->syncDetailEditorsFromJob(index);
          });
  connect(impl_->regionModeCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this, commitSelectiveCombo](int) {
            commitSelectiveCombo(impl_->regionModeCombo,
                                 QStringLiteral("regionMode"));
            if (impl_ && impl_->regionModeCombo) {
              const bool cropEnabled = impl_->regionModeCombo->currentData().toInt() != 0;
              if (impl_->cropXSpin) impl_->cropXSpin->setEnabled(cropEnabled);
              if (impl_->cropYSpin) impl_->cropYSpin->setEnabled(cropEnabled);
              if (impl_->cropWSpin) impl_->cropWSpin->setEnabled(cropEnabled);
              if (impl_->cropHSpin) impl_->cropHSpin->setEnabled(cropEnabled);
            }
          });
  connect(impl_->layerFilterModeCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int comboIndex) {
            if (!impl_ || impl_->syncingJobDetails || !impl_->service ||
                !impl_->layerFilterModeCombo) {
              return;
            }
            const int index = impl_->selectedSourceIndex();
            if (index < 0 || index >= impl_->service->jobCount()) {
              return;
            }
            QVariantMap settings = impl_->service->jobSelectiveSettingsAt(index);
            settings.insert(QStringLiteral("layerFilterMode"),
                            impl_->layerFilterModeCombo->itemData(comboIndex));
            impl_->service->setJobSelectiveSettingsAt(index, settings);
            impl_->syncDetailEditorsFromJob(index);
          });
  connect(impl_->useSelectedLayersButton, &QPushButton::clicked, this, [this]() {
    if (!impl_ || !impl_->service) {
      return;
    }
    auto* selectionManager = ArtifactLayerSelectionManager::instance();
    if (!selectionManager) {
      return;
    }
    const auto selectedLayers = selectionManager->selectedLayersInOrder();
    if (selectedLayers.isEmpty()) {
      return;
    }
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) {
      return;
    }
    QStringList whitelist;
    for (const auto& layer : selectedLayers) {
      if (layer) {
        whitelist.append(layer->id().toString());
      }
    }
    if (whitelist.isEmpty()) {
      return;
    }
    QVariantMap settings = impl_->service->jobSelectiveSettingsAt(index);
    settings.insert(QStringLiteral("layerFilterMode"), 4);
    settings.insert(QStringLiteral("layerWhitelist"), whitelist);
    QStringList blacklist = settings.value(QStringLiteral("layerBlacklist"))
                                .toStringList();
    for (const QString& id : whitelist) {
      blacklist.removeAll(id);
    }
    settings.insert(QStringLiteral("layerBlacklist"), blacklist);
    impl_->service->setJobSelectiveSettingsAt(index, settings);
    impl_->syncDetailEditorsFromJob(index);
  });
  connect(impl_->excludeSelectedLayersButton, &QPushButton::clicked, this, [this]() {
    if (!impl_ || !impl_->service) {
      return;
    }
    auto* selectionManager = ArtifactLayerSelectionManager::instance();
    if (!selectionManager) {
      return;
    }
    const auto selectedLayers = selectionManager->selectedLayersInOrder();
    if (selectedLayers.isEmpty()) {
      return;
    }
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) {
      return;
    }
    QVariantMap settings = impl_->service->jobSelectiveSettingsAt(index);
    QStringList blacklist = settings.value(QStringLiteral("layerBlacklist"))
                                .toStringList();
    for (const auto& layer : selectedLayers) {
      if (layer) {
        const QString id = layer->id().toString();
        if (!blacklist.contains(id)) {
          blacklist.append(id);
        }
      }
    }
    if (blacklist.isEmpty()) {
      return;
    }
    QStringList whitelist = settings.value(QStringLiteral("layerWhitelist"))
                                .toStringList();
    for (const QString& id : blacklist) {
      whitelist.removeAll(id);
    }
    settings.insert(QStringLiteral("layerWhitelist"), whitelist);
    settings.insert(QStringLiteral("layerBlacklist"), blacklist);
    impl_->service->setJobSelectiveSettingsAt(index, settings);
    impl_->syncDetailEditorsFromJob(index);
  });
  auto clearLayerFilterList = [this](const QString& key) {
    if (!impl_ || !impl_->service) {
      return;
    }
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) {
      return;
    }
    QVariantMap settings = impl_->service->jobSelectiveSettingsAt(index);
    settings.insert(key, QStringList{});
    impl_->service->setJobSelectiveSettingsAt(index, settings);
    impl_->syncDetailEditorsFromJob(index);
  };
  connect(impl_->clearWhitelistButton, &QPushButton::clicked, this,
          [clearLayerFilterList]() {
            clearLayerFilterList(QStringLiteral("layerWhitelist"));
          });
  connect(impl_->clearBlacklistButton, &QPushButton::clicked, this,
          [clearLayerFilterList]() {
            clearLayerFilterList(QStringLiteral("layerBlacklist"));
          });
  auto commitCrop = [this]() {
    if (!impl_ || impl_->syncingJobDetails || !impl_->service) {
      return;
    }
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) {
      return;
    }
    QVariantMap settings = impl_->service->jobSelectiveSettingsAt(index);
    settings.insert(QStringLiteral("cropX"), impl_->cropXSpin->value());
    settings.insert(QStringLiteral("cropY"), impl_->cropYSpin->value());
    settings.insert(QStringLiteral("cropW"), impl_->cropWSpin->value());
    settings.insert(QStringLiteral("cropH"), impl_->cropHSpin->value());
    impl_->service->setJobSelectiveSettingsAt(index, settings);
    impl_->syncDetailEditorsFromJob(index);
  };
  impl_->cropXSpin->committed = [commitCrop](int) { commitCrop(); };
  impl_->cropYSpin->committed = [commitCrop](int) { commitCrop(); };
  impl_->cropWSpin->committed = [commitCrop](int) { commitCrop(); };
  impl_->cropHSpin->committed = [commitCrop](int) { commitCrop(); };

  // Group: Overlay
  auto* overlayGroup = new QGroupBox("Overlay Transform");
  auto* overlayLayout = new QFormLayout(overlayGroup);
  impl_->overlayXSpin = new RenderQueueDoubleSpinBox();
  impl_->overlayYSpin = new RenderQueueDoubleSpinBox();
  impl_->overlayScaleSpin = new RenderQueueDoubleSpinBox();
  impl_->overlayScaleSpin->setValue(1.0);
  impl_->overlayRotationSpin = new RenderQueueDoubleSpinBox();
  auto commitOverlay = [this]() {
    if (!impl_ || impl_->syncingJobDetails || !impl_->service) return;
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) return;
    impl_->service->setJobOverlayTransform(index,
        static_cast<float>(impl_->overlayXSpin->value()),
        static_cast<float>(impl_->overlayYSpin->value()),
        static_cast<float>(impl_->overlayScaleSpin->value()),
        static_cast<float>(impl_->overlayRotationSpin->value()));
  };
  impl_->overlayXSpin->committed = [commitOverlay](double) { commitOverlay(); };
  impl_->overlayYSpin->committed = [commitOverlay](double) { commitOverlay(); };
  impl_->overlayScaleSpin->committed = [commitOverlay](double) { commitOverlay(); };
  impl_->overlayRotationSpin->committed = [commitOverlay](double) { commitOverlay(); };
  overlayLayout->addRow("X Offset:", impl_->overlayXSpin);
  overlayLayout->addRow("Y Offset:", impl_->overlayYSpin);
  overlayLayout->addRow("Scale:", impl_->overlayScaleSpin);
  overlayLayout->addRow("Rotation:", impl_->overlayRotationSpin);
  detailLayout->addWidget(overlayGroup);

  detailLayout->addStretch();
  detailScroll->setWidget(detailWidget);
  splitter->addWidget(detailScroll);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 3);
  splitter->setStretchFactor(2, 2);
  splitter->setSizes({224, 760, 400});
  layout->addWidget(splitter, 1);

  auto* historyGroup = new QGroupBox("Render History / Log");
  auto* historyLayout = new QVBoxLayout(historyGroup);
  impl_->historyListWidget = new QListWidget();
  impl_->historyListWidget->setObjectName("renderQueueHistory");
  impl_->historyListWidget->setAccessibleName(QStringLiteral("Render history"));
  impl_->historyListWidget->setAccessibleDescription(
      QStringLiteral("Review completed and failed render job history."));
  impl_->historyListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  historyLayout->addWidget(impl_->historyListWidget, 1);
  connect(impl_->historyListWidget, &QListWidget::customContextMenuRequested,
          this, [this](const QPoint& position) {
            if (!impl_ || !impl_->historyListWidget || !impl_->service) return;
            auto* item = impl_->historyListWidget->itemAt(position);
            if (!item) return;
            const int index = item->data(Qt::UserRole).toInt();
            if (index < 0 || index >= impl_->service->jobCount()) return;
            QMenu menu(this);
            QAction* retry = menu.addAction(QStringLiteral("Retry Job"));
            QAction* reveal = menu.addAction(QStringLiteral("Reveal Output"));
            const QAction* chosen = menu.exec(
                impl_->historyListWidget->viewport()->mapToGlobal(position));
            if (chosen == retry) {
              impl_->service->resetJobForRerun(index);
              impl_->service->startRenderQueueAt(index);
              impl_->postHistoryMessage(QStringLiteral("Retry requested from history"), index);
            } else if (chosen == reveal) {
              const QString path = impl_->service->jobOutputPathAt(index);
              if (!path.trimmed().isEmpty()) {
                QDesktopServices::openUrl(
                    QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
              }
            }
          });
  auto* historyButtonLayout = new QHBoxLayout();
  impl_->clearHistoryButton = new QPushButton("Clear");
  impl_->exportHistoryButton = new QPushButton("Export...");
  impl_->clearHistoryButton->setAccessibleName(QStringLiteral("Clear render history"));
  impl_->clearHistoryButton->setAccessibleDescription(
      QStringLiteral("Remove all entries from the render history."));
  impl_->exportHistoryButton->setAccessibleName(QStringLiteral("Export render history"));
  impl_->exportHistoryButton->setAccessibleDescription(
      QStringLiteral("Save the render history to a file."));
  impl_->clearHistoryButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/clear_all.svg")));
  impl_->exportHistoryButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/figma_render_export.svg")));
  historyButtonLayout->addWidget(impl_->clearHistoryButton);
  historyButtonLayout->addWidget(impl_->exportHistoryButton);
  historyButtonLayout->addStretch();
  historyLayout->addLayout(historyButtonLayout);
  historyGroup->setFixedHeight(78);
  layout->addWidget(historyGroup);
  impl_->loadHistory();

  // Bottom
  impl_->totalProgressBar = new QProgressBar();
  impl_->summaryLabel = new QLabel("Ready");
  impl_->statusLabel = new QLabel("No active jobs");
  impl_->startButton = new QPushButton("Start Queue");
  impl_->startButton->setAccessibleName(QStringLiteral("Start render queue"));
  impl_->startButton->setAccessibleDescription(
      QStringLiteral("Start processing the queued render jobs."));
  impl_->startButton->setObjectName("renderStartBtn");
  impl_->startButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/figma_media_play.svg")));
  {
    QPalette buttonPalette = impl_->startButton->palette();
    buttonPalette.setColor(QPalette::Button, QColor(theme.selectionColor));
    buttonPalette.setColor(QPalette::ButtonText, QColor(theme.textColor));
    impl_->startButton->setAutoFillBackground(true);
    impl_->startButton->setPalette(buttonPalette);
  }
  
  auto* activityFrame = new QFrame();
  activityFrame->setFrameShape(QFrame::StyledPanel);
  auto* activityLayout = new QHBoxLayout(activityFrame);
  activityLayout->setContentsMargins(10, 6, 6, 6);
  auto* progressLayout = new QVBoxLayout();
  progressLayout->setSpacing(2);
  progressLayout->addWidget(impl_->summaryLabel);
  progressLayout->addWidget(impl_->totalProgressBar);
  progressLayout->addWidget(impl_->statusLabel);
  activityLayout->addLayout(progressLayout, 1);
  impl_->pauseButton = new RenderQueueActionButton(QStringLiteral("Pause"));
  impl_->cancelButton = new RenderQueueActionButton(QStringLiteral("Stop"));
  impl_->pauseButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/animationmenu_pause.svg")));
  impl_->cancelButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/figma_media_stop.svg")));
  impl_->pauseButton->setEnabled(false);
  impl_->cancelButton->setEnabled(false);
  impl_->pauseButton->action = [this]() {
    if (impl_ && impl_->service) impl_->service->pauseAllJobs();
  };
  impl_->cancelButton->action = [this]() {
    if (impl_ && impl_->service) impl_->service->cancelAllJobs();
  };
  activityLayout->addWidget(impl_->pauseButton);
  activityLayout->addWidget(impl_->cancelButton);
  activityLayout->addWidget(impl_->startButton);
  layout->addWidget(activityFrame);

  // Context Menu
  connect(impl_->jobListWidget, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
    int idx = impl_->selectedSourceIndex();
    if (idx < 0 || !impl_->service) return;
    QMenu menu(this);
    QString path = impl_->service->jobOutputPathAt(idx);
    auto* reveal = menu.addAction("Reveal in Explorer");
    auto* open = menu.addAction("Open File");
    auto* retry = menu.addAction("Retry Job");
    auto* retryFailedFrames = menu.addAction("Retry Detected Failed Frames");
    const auto detectedFailedFrames = impl_->service->detectFailedFrames(idx);
    retryFailedFrames->setEnabled(!detectedFailedFrames.isEmpty());
    menu.addSeparator();
    auto* copyPath = menu.addAction("Copy Path");
    const QPoint origin = impl_->jobListWidget->mapToGlobal(pos);
    int menuX = origin.x();
    int menuY = origin.y();
    Accessibility::adjustContextMenuPosition(menuX, menuY,
                                              menu.sizeHint().width());
    auto* act = menu.exec(QPoint(menuX, menuY));
    if (act == reveal) ArtifactCore::openInExplorer(path, true);
    else if (act == open) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    else if (act == retry) {
      impl_->service->resetJobForRerun(idx);
      impl_->service->startRenderQueueAt(idx);
    }
    else if (act == retryFailedFrames) {
      impl_->service->rerenderAllDetectedFailedFrames(idx);
      impl_->service->startRenderQueueAt(idx);
    }
    else if (act == copyPath) QApplication::clipboard()->setText(path);
  });

  connect(impl_->jobListWidget, &QListWidget::currentRowChanged, this, [this](int row) {
    impl_->handleJobSelected();
    const int sourceIndex = (row >= 0 && row < impl_->visibleToSource.size())
        ? impl_->visibleToSource[row]
        : impl_->selectedSourceIndex();
    impl_->syncDetailEditorsFromJob(sourceIndex);
  });

  connect(impl_->outputSettingsButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->service) {
      return;
    }
    const int index = impl_->selectedSourceIndex();
    if (index < 0 || index >= impl_->service->jobCount()) {
      return;
    }

    ArtifactRenderOutputSettingDialog dialog(this);
    QString outputFormat;
    QString codec;
    QString codecProfile;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    int bitrateKbps = 0;
    impl_->service->jobOutputSettingsAt(index, &outputFormat, &codec, &codecProfile, &width, &height, &fps, &bitrateKbps);
    dialog.setOutputPath(impl_->service->jobOutputPathAt(index));
    dialog.setOutputFormat(outputFormat);
    dialog.setCodec(codec);
    dialog.setCodecProfile(codecProfile);
    dialog.setEncoderBackend(impl_->service->jobEncoderBackendAt(index));
    dialog.setRenderBackend(impl_->service->jobRenderBackendAt(index));
    dialog.setResolution(width, height);
    dialog.setFrameRate(fps);
    dialog.setBitrateKbps(bitrateKbps);
    dialog.setIncludeAudio(impl_->service->jobIntegratedRenderEnabledAt(index));
    dialog.setMultiChannelEnabled(impl_->service->jobMultiChannelEnabledAt(index));
    dialog.setMultiChannelChannels(impl_->service->jobMultiChannelChannelsAt(index));
    dialog.setFramePadding(impl_->service->jobFramePaddingAt(index));
    dialog.setAudioCodec(impl_->service->jobAudioCodecAt(index));
    dialog.setAudioBitrateKbps(impl_->service->jobAudioBitrateKbpsAt(index));
    dialog.setAudioChannelMode(impl_->service->jobAudioChannelModeAt(index));
    dialog.setAudioSampleRate(impl_->service->jobAudioSampleRateAt(index));
    const auto preflight = impl_->service->preflightRenderQueueAt(index);
    dialog.setPreflightSummary(ArtifactRenderQueueService::formatPreflightSummary(preflight));
    dialog.setPreflightDetails(ArtifactRenderQueueService::formatPreflightDetails(preflight));

    if (dialog.exec() == QDialog::Accepted) {
      impl_->service->setJobOutputPathAt(index, dialog.outputPath());
      impl_->service->setJobOutputSettingsAt(
          index,
          dialog.outputFormat(),
          dialog.codec(),
          dialog.codecProfile(),
          dialog.outputWidth(),
          dialog.outputHeight(),
          dialog.frameRate(),
          dialog.bitrateKbps());
      impl_->service->setJobEncoderBackendAt(index, dialog.encoderBackend());
      impl_->service->setJobRenderBackendAt(index, dialog.renderBackend());
      impl_->service->setJobIntegratedRenderEnabledAt(index, dialog.includeAudio());
      impl_->service->setJobMultiChannelEnabledAt(index, dialog.multiChannelEnabled());
      impl_->service->setJobMultiChannelChannelsAt(index, dialog.multiChannelChannels());
      impl_->service->setJobFramePaddingAt(index, dialog.framePadding());
      impl_->service->setJobAudioCodecAt(index, dialog.audioCodec());
      impl_->service->setJobAudioBitrateKbpsAt(index, dialog.audioBitrateKbps());
      impl_->service->setJobAudioChannelModeAt(index, dialog.audioChannelMode());
      impl_->service->setJobAudioSampleRateAt(index, dialog.audioSampleRate());
      impl_->syncJobsFromService();
      impl_->syncDetailEditorsFromJob(index);
    }
  });

  // Services
  if (impl_->service) {
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<RenderQueueChangedEvent>([this](const RenderQueueChangedEvent& event) {
          Q_UNUSED(event);
          if (!impl_) {
            return;
          }
          impl_->syncJobsFromService();
        }));
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<RenderQueueLogEvent>([this](const RenderQueueLogEvent& event) {
          if (!impl_) {
            return;
          }
          if (event.message.trimmed().isEmpty()) {
            return;
          }
          impl_->logServiceEvent(event.message, event.sourceIndex, event.alsoHistory);
        }));
    connect(impl_->service, &ArtifactRenderQueueService::jobAdded, this, [this](int index) {
        Q_UNUSED(index);
        if (!impl_) {
          return;
        }
        impl_->postQueueChanged(QStringLiteral("Job added"));
    });
    connect(impl_->service, &ArtifactRenderQueueService::jobRemoved, this, [this](int index) {
        Q_UNUSED(index);
        if (!impl_) {
          return;
        }
        impl_->progressStartedAtMsByJob.clear();
        impl_->postQueueChanged(QStringLiteral("Job removed"));
    });
    connect(impl_->service, &ArtifactRenderQueueService::jobUpdated, this, [this](int index) {
        Q_UNUSED(index);
        if (!impl_) {
          return;
        }
        impl_->postQueueChanged(QStringLiteral("Job updated"));
    });
    connect(impl_->service, &ArtifactRenderQueueService::jobProgressChanged, this, [this](int index, int progress) {
        if (!impl_ || !impl_->service) return;
        if (index >= 0 && index < static_cast<int>(impl_->jobs.size())) {
          const bool progressRolledBack =
              progress < impl_->jobs[index].progress;
          if (impl_->progressStartedAtMsByJob.find(index) ==
                  impl_->progressStartedAtMsByJob.end() || progressRolledBack) {
            impl_->progressStartedAtMsByJob[index] =
                impl_->progressClock_.elapsed();
          }
          impl_->jobs[index].progress = progress;
          impl_->updateJobItemAtIndex(index);
          impl_->updateSummary();
        }
    });
    connect(impl_->service, &ArtifactRenderQueueService::jobStatusChanged, this, [this](int index, int status) {
        Q_UNUSED(status);
        if (!impl_ || !impl_->service) {
          return;
        }
        const QString jobName = impl_->service->jobCompositionNameAt(index);
        const QString jobStatus = impl_->service->jobStatusAt(index);
        if (jobStatus == QStringLiteral("Rendering")) {
          impl_->progressStartedAtMsByJob[index] =
              impl_->progressClock_.elapsed();
        } else if (jobStatus == QStringLiteral("Completed") ||
                   jobStatus == QStringLiteral("Failed") ||
                   jobStatus == QStringLiteral("Canceled")) {
          impl_->progressStartedAtMsByJob.erase(index);
        }
        if (jobStatus == "Failed") {
          const QString error = impl_->service->jobErrorMessageAt(index);
          impl_->postHistoryMessage(QString("Job failed: %1%2")
              .arg(jobName)
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
    connect(impl_->service, &ArtifactRenderQueueService::queueReordered, this, [this](int fromIndex, int toIndex) {
        Q_UNUSED(fromIndex);
        Q_UNUSED(toIndex);
        if (!impl_) {
          return;
        }
        impl_->progressStartedAtMsByJob.clear();
        impl_->postQueueChanged(QStringLiteral("Queue reordered"));
    });
    connect(impl_->service, &ArtifactRenderQueueService::allJobsCompleted, this, [this]() {
#ifdef _WIN32
        ::MessageBeep(MB_OK);
#else
        QApplication::beep();
#endif
        if (impl_) {
          impl_->postHistoryMessage(QStringLiteral("All jobs completed"));
        }
    });
    connect(impl_->service, &ArtifactRenderQueueService::previewFrameReady, this, [this](int jobIndex, int frameNumber) {
        QImage frame = impl_->service->lastRenderedFrame();
        if (!frame.isNull() && impl_->previewLabel) {
            const QPixmap pixmap = QPixmap::fromImage(frame);
            impl_->previewLabel->setPixmap(pixmap);
            impl_->previewLabel->setToolTip(QString("Job %1 | Frame %2").arg(jobIndex + 1).arg(frameNumber));
            if (impl_->jobListWidget && jobIndex >= 0) {
              for (int row = 0; row < impl_->jobListWidget->count(); ++row) {
                auto* item = impl_->jobListWidget->item(row);
                if (item->data(Qt::UserRole).toInt() != jobIndex) continue;
                if (auto* card = static_cast<RenderQueueJobCard*>(impl_->jobListWidget->itemWidget(item))) {
                  card->setPreview(pixmap);
                }
                break;
              }
            }
        }
    });
  }

  connect(impl_->addButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) {
      impl_->service->addRenderQueue();
    }
  });

  // Batch actions via ArtifactBatchRenderer
  connect(batchAllBtn, &QPushButton::clicked, this, [this]() {
    auto* batchRenderer = ArtifactBatchRenderer::instance();
    if (batchRenderer) {
      const QString outDir = QFileDialog::getExistingDirectory(this,
          QStringLiteral("Batch Output Directory"),
          QDir::homePath());
      if (outDir.isEmpty()) return;
      const int count = batchRenderer->addAllCompositions(outDir);
      impl_->logUiEvent(QStringLiteral("Batch add all: %1 jobs").arg(count));
      impl_->syncJobsFromService();
      impl_->postQueueChanged(QStringLiteral("batch-all"));
    }
  });

  connect(batchTmplBtn, &QPushButton::clicked, this, [this]() {
    auto* batchRenderer = ArtifactBatchRenderer::instance();
    if (!batchRenderer) return;
    const auto templates = batchRenderer->availableTemplates();
    if (templates.isEmpty()) {
      // Create a default template on first use
      BatchTemplate tmpl = batchRenderer->defaultTemplate();
      tmpl.outputDirectory = QFileDialog::getExistingDirectory(this,
          QStringLiteral("Select Output Directory"), QDir::homePath());
      if (tmpl.outputDirectory.isEmpty()) return;
      batchRenderer->saveTemplate(tmpl);
      const int count = batchRenderer->addAllCompositions(
          tmpl.outputDirectory, tmpl.fileNamePattern);
      impl_->logUiEvent(QStringLiteral("Batch default template: %1 jobs").arg(count));
    } else {
      // Pick first template for now
      const auto& tmpl = templates.first();
      const int count = batchRenderer->addAllCompositions(
          tmpl.outputDirectory, tmpl.fileNamePattern);
      impl_->logUiEvent(QStringLiteral("Batch template '%1': %2 jobs").arg(tmpl.name).arg(count));
    }
    impl_->syncJobsFromService();
    impl_->postQueueChanged(QStringLiteral("batch-template"));
  });

  connect(impl_->removeButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) {
      const int index = impl_->selectedSourceIndex();
      if (index >= 0) {
        impl_->service->removeRenderQueueAt(index);
      }
    }
  });

  connect(impl_->duplicateButton, &QToolButton::clicked, this, [this]() {
    if (impl_->service) {
      const int index = impl_->selectedSourceIndex();
      if (index >= 0) {
        impl_->service->duplicateRenderQueueAt(index);
      }
    }
  });

  connect(impl_->applySettingsToSelectionButton, &QPushButton::clicked,
          this, [this]() {
    if (!impl_ || !impl_->service || !impl_->jobListWidget) return;
    const int sourceIndex = impl_->selectedSourceIndex();
    if (sourceIndex < 0 || sourceIndex >= impl_->service->jobCount()) return;

    QString outputFormat;
    QString codec;
    QString codecProfile;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    int bitrateKbps = 0;
    if (!impl_->service->jobOutputSettingsAt(
            sourceIndex, &outputFormat, &codec, &codecProfile, &width,
            &height, &fps, &bitrateKbps)) {
      return;
    }
    const QString encoderBackend =
        impl_->service->jobEncoderBackendAt(sourceIndex);
    const QString renderBackend =
        impl_->service->jobRenderBackendAt(sourceIndex);

    int applied = 0;
    for (int row = 0; row < impl_->jobListWidget->count(); ++row) {
      auto *item = impl_->jobListWidget->item(row);
      if (!item || !item->isSelected()) continue;
      const int targetIndex = item->data(Qt::UserRole).toInt();
      if (targetIndex < 0 || targetIndex >= impl_->service->jobCount() ||
          targetIndex == sourceIndex) {
        continue;
      }
      impl_->service->setJobOutputSettingsAt(
          targetIndex, outputFormat, codec, codecProfile, width, height, fps,
          bitrateKbps);
      impl_->service->setJobEncoderBackendAt(targetIndex, encoderBackend);
      impl_->service->setJobRenderBackendAt(targetIndex, renderBackend);
      ++applied;
    }
    if (applied > 0) {
      impl_->logUiEvent(QStringLiteral("Applied output settings to %1 jobs")
                            .arg(applied));
      impl_->syncJobsFromService();
      impl_->syncDetailEditorsFromJob(sourceIndex);
    }
  });

  connect(impl_->startButton, &QPushButton::clicked, this, [this]() {
    if (impl_->service) impl_->service->startAllJobs();
  });

  connect(impl_->clearHistoryButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->historyListWidget) return;
    impl_->historyListWidget->clear();
    impl_->saveHistory();
  });

  connect(impl_->exportHistoryButton, &QPushButton::clicked, this, [this]() {
    if (!impl_->historyListWidget) return;
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Render History",
        QDir::homePath() + "/Desktop/render_queue_history.log",
        "Log Files (*.log *.txt);;All Files (*)");
    if (filePath.isEmpty()) return;
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
      QMessageBox::warning(this, "Export Failed", QString("Failed to open %1").arg(filePath));
      return;
    }
    QTextStream out(&file);
    for (int i = 0; i < impl_->historyListWidget->count(); ++i) {
      out << impl_->historyListWidget->item(i)->text() << '\n';
    }
  });

  impl_->syncJobsFromService();
  impl_->handleJobSelected();
 }

 RenderQueueManagerWidget::~RenderQueueManagerWidget() { delete impl_; }
 QSize RenderQueueManagerWidget::sizeHint() const { return QSize(800, 600); }
 void RenderQueueManagerWidget::setFloatingMode(bool f) { setWindowFlag(Qt::Window, f); show(); }
 void RenderQueueManagerWidget::showEvent(QShowEvent* e) { QWidget::showEvent(e); impl_->syncJobsFromService(); }
}
