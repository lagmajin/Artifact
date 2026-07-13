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
#include <QFileInfo>
#include <QByteArray>
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
  std::unique_ptr<ArtifactCore::FastSettingsStore> historyStore_;
  std::unique_ptr<ArtifactCore::FastSettingsStore> presetStore_;
  bool syncingTransformControls = false;
  bool syncingJobDetails = false;
  bool syncingPresetCombo = false;
  std::map<int, int> lastProgressBucketByJob;
  int progressLogStepPercent = 25;
  QFont fixedFont_{"Consolas", 10};

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
                              .arg(preflightText));
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
        previewSummaryLabel->setText(
            QStringLiteral("Format: %1 (%2)\nResolution: %3 × %4\nFrame Rate: %5 FPS\nFrames: %6 – %7")
                .arg(outputFormat.isEmpty() ? QStringLiteral("MP4") : outputFormat)
                .arg(codec.isEmpty() ? QStringLiteral("H.264") : codec)
                .arg(width > 0 ? QString::number(width) : QStringLiteral("Auto"))
                .arg(height > 0 ? QString::number(height) : QStringLiteral("Auto"))
                .arg(fps > 0.0 ? QString::number(fps, 'f', 2) : QStringLiteral("Auto"))
                .arg(previewStartFrame)
                .arg(previewEndFrame));
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
  impl_->searchEdit->setObjectName("renderQueueSearch");
  impl_->searchEdit->setMaximumWidth(440);
  impl_->searchEdit->changed = [this](const QString& text) {
    if (!impl_) return;
    impl_->searchQuery = text;
    impl_->updateJobList();
    impl_->handleJobSelected();
  };
  impl_->addButton = new QPushButton("+  Add Composition");
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
  batchAllBtn->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/playlist_add.svg")));
  batchAllBtn->setToolTip(QStringLiteral("Add all compositions to queue"));
  auto* batchTmplBtn = new QPushButton(QStringLiteral("Batch Template"));
  batchTmplBtn->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/compositionmenu_presets.svg")));
  batchTmplBtn->setToolTip(QStringLiteral("Batch add using a template"));
  auto* presetButton = new QToolButton();
  presetButton->setText(QStringLiteral("Preset:  H.264 High Quality"));
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
  impl_->jobListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  impl_->jobListWidget->setDragEnabled(true);
  impl_->jobListWidget->setAcceptDrops(true);
  impl_->jobListWidget->setDropIndicatorShown(true);
  impl_->jobListWidget->setDragDropMode(QAbstractItemView::InternalMove);
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
  impl_->removeButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/delete.svg")));
  impl_->duplicateButton = new QToolButton();
  impl_->duplicateButton->setIcon(
      loadIconWithFallback(QStringLiteral("Studio/editmenu_duplicate.svg")));
  impl_->duplicateButton->setToolTip(QStringLiteral("Duplicate selected job"));
  btnLayout->addWidget(batchTmplBtn);
  btnLayout->addWidget(impl_->duplicateButton);
  btnLayout->addStretch();
  impl_->clearButton = new RenderQueueActionButton(QStringLiteral("Clear Completed"));
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
  impl_->outputBrowseButton = new RenderQueueActionButton(QStringLiteral("Browse"));
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
  historyLayout->addWidget(impl_->historyListWidget, 1);
  auto* historyButtonLayout = new QHBoxLayout();
  impl_->clearHistoryButton = new QPushButton("Clear");
  impl_->exportHistoryButton = new QPushButton("Export...");
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
    menu.addSeparator();
    auto* copyPath = menu.addAction("Copy Path");
    auto* act = menu.exec(impl_->jobListWidget->mapToGlobal(pos));
    if (act == reveal) ArtifactCore::openInExplorer(path, true);
    else if (act == open) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    else if (act == retry) {
      impl_->service->resetJobForRerun(idx);
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
          impl_->jobs[index].progress = progress;
          impl_->updateJobItemAtIndex(index);
        }
    });
    connect(impl_->service, &ArtifactRenderQueueService::jobStatusChanged, this, [this](int index, int status) {
        Q_UNUSED(status);
        if (!impl_ || !impl_->service) {
          return;
        }
        const QString jobName = impl_->service->jobCompositionNameAt(index);
        const QString jobStatus = impl_->service->jobStatusAt(index);
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
