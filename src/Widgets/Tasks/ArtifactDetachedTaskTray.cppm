module;

#include <QApplication>
#include <QElapsedTimer>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <wobjectimpl.h>

module Artifact.Widgets.DetachedTaskTray;

import Core.ArtifactMath;
import Widgets.Utils.CSS;
import Artifact.Event.Types;
import Artifact.Service.DetachedTask;

namespace Artifact {

namespace {

constexpr int kRefreshIntervalMs = 250;
constexpr int kCollapseGraceMs = 4000;
constexpr int kExpandedWidth = 360;
constexpr int kExpandedHeight = 260;
constexpr int kBadgeWidth = 190;
constexpr int kBadgeHeight = 30;

QString geometryKey() { return QStringLiteral("Artifact/DetachedTasks/Geometry"); }
QString expandedKey() { return QStringLiteral("Artifact/DetachedTasks/Expanded"); }

bool isTerminalStateName(const QString& state) {
  return state == QStringLiteral("Succeeded") || state == QStringLiteral("Failed") ||
         state == QStringLiteral("Cancelled") || state == QStringLiteral("Denied");
}

void setWidgetTextColor(QWidget* widget, const QColor& color) {
  QPalette palette = widget->palette();
  palette.setColor(QPalette::WindowText, color);
  palette.setColor(QPalette::Text, color);
  palette.setColor(QPalette::ButtonText, color);
  widget->setPalette(palette);
}

} // namespace

W_OBJECT_IMPL(ArtifactDetachedTaskTray)

class ArtifactDetachedTaskTray::Impl {
public:
  explicit Impl(ArtifactDetachedTaskTray* owner) : owner_(owner) {}

  ArtifactDetachedTaskTray* owner_ = nullptr;
  QLabel* titleLabel_ = nullptr;
  QLabel* countsLabel_ = nullptr;
  QLabel* deferredLabel_ = nullptr;
  QPushButton* clearButton_ = nullptr;
  QPushButton* collapseButton_ = nullptr;
  QWidget* rowsHost_ = nullptr;
  QVBoxLayout* rowsLayout_ = nullptr;
  QLineEdit* commandEdit_ = nullptr;
  QPushButton* submitButton_ = nullptr;
  QPushButton* aiButton_ = nullptr;
  QTimer* refreshTimer_ = nullptr;

  bool expanded_ = true;
  QPoint dragOffset_;
  bool dragging_ = false;
  QString lastSignature_;
  QElapsedTimer idleTimer_;
  bool idleTimerValid_ = false;

  void applyWindowFlags() {
    owner_->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool |
                           Qt::NoDropShadowWindowHint);
    owner_->setAttribute(Qt::WA_TranslucentBackground);
    owner_->setAttribute(Qt::WA_ShowWithoutActivating);
  }

  void restoreSettings() {
    QSettings settings;
    const QByteArray geometry = settings.value(geometryKey()).toByteArray();
    expanded_ = settings.value(expandedKey(), true).toBool();
    if (!geometry.isEmpty()) {
      owner_->restoreGeometry(geometry);
    }
    owner_->resize(expanded_ ? QSize(kExpandedWidth, kExpandedHeight)
                             : QSize(kBadgeWidth, kBadgeHeight));
  }

  void saveSettings() const {
    QSettings settings;
    settings.setValue(geometryKey(), owner_->saveGeometry());
    settings.setValue(expandedKey(), expanded_);
  }

  void buildUi() {
    auto* outer = new QVBoxLayout(owner_);
    outer->setContentsMargins(10, 8, 10, 8);
    outer->setSpacing(6);

    auto* header = new QWidget(owner_);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    titleLabel_ = new QLabel(QStringLiteral("Detached"), header);
    QFont titleFont = titleLabel_->font();
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);
    headerLayout->addWidget(titleLabel_);

    countsLabel_ = new QLabel(QString(), header);
    headerLayout->addWidget(countsLabel_, 1);

    clearButton_ = new QPushButton(QStringLiteral("クリア"), header);
    clearButton_->setToolTip(QStringLiteral("完了したタスクを一覧から消す"));
    headerLayout->addWidget(clearButton_);

    collapseButton_ = new QPushButton(QStringLiteral("–"), header);
    collapseButton_->setToolTip(QStringLiteral("畳む / 展開する"));
    collapseButton_->setFixedWidth(24);
    headerLayout->addWidget(collapseButton_);

    outer->addWidget(header);

    deferredLabel_ = new QLabel(QString(), owner_);
    deferredLabel_->setWordWrap(true);
    outer->addWidget(deferredLabel_);

    auto* scroll = new QScrollArea(owner_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    rowsHost_ = new QWidget(scroll);
    rowsLayout_ = new QVBoxLayout(rowsHost_);
    rowsLayout_->setContentsMargins(0, 0, 0, 0);
    rowsLayout_->setSpacing(8);
    rowsLayout_->addStretch(1);
    scroll->setWidget(rowsHost_);
    outer->addWidget(scroll, 1);

    auto* footer = new QWidget(owner_);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setSpacing(6);

    commandEdit_ = new QLineEdit(footer);
    commandEdit_->setPlaceholderText(
        QStringLiteral("CommandIR を JSON で入力（例: {\"type\":\"create_composition\",\"name\":\"Logo\"}）"));
    footerLayout->addWidget(commandEdit_, 1);

    submitButton_ = new QPushButton(QStringLiteral("実行"), footer);
    footerLayout->addWidget(submitButton_);

    aiButton_ = new QPushButton(QStringLiteral("AI"), footer);
    aiButton_->setEnabled(false);
    aiButton_->setToolTip(
        QStringLiteral("AI モードは承認設計を含めて次の段で対応します"));
    footerLayout->addWidget(aiButton_);

    outer->addWidget(footer);

    QObject::connect(clearButton_, &QPushButton::clicked, owner_, []() {
      if (auto* service = ArtifactDetachedTaskService::instance()) {
        service->clearFinished();
      }
    });
    QObject::connect(collapseButton_, &QPushButton::clicked, owner_, [this]() {
      owner_->setExpanded(!expanded_);
      saveSettings();
    });
    QObject::connect(submitButton_, &QPushButton::clicked, owner_, [this]() {
      submitFromEditor();
    });
    QObject::connect(commandEdit_, &QLineEdit::returnPressed, owner_, [this]() {
      submitFromEditor();
    });
  }

  void submitFromEditor() {
    const QString text = commandEdit_ ? commandEdit_->text().trimmed() : QString();
    if (text.isEmpty()) {
      return;
    }
    if (auto* service = ArtifactDetachedTaskService::instance()) {
      service->submitCommandText(text);
    }
    commandEdit_->clear();
    owner_->setExpanded(true);
    refresh();
  }

  QColor textColor() const {
    return QColor(ArtifactCore::currentDCCTheme().textColor);
  }
  QColor accentColor() const {
    return QColor(ArtifactCore::currentDCCTheme().accentColor);
  }
  QColor surfaceColor() const {
    return QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor);
  }
  QColor borderColor() const {
    return QColor(ArtifactCore::currentDCCTheme().borderColor);
  }

  void applyPalette() {
    QPalette palette = owner_->palette();
    palette.setColor(QPalette::Window, surfaceColor());
    palette.setColor(QPalette::Base, surfaceColor());
    palette.setColor(QPalette::WindowText, textColor());
    palette.setColor(QPalette::Text, textColor());
    palette.setColor(QPalette::ButtonText, textColor());
    palette.setColor(QPalette::Highlight, accentColor());
    owner_->setPalette(palette);
  }

  void clearRows() {
    if (!rowsLayout_) {
      return;
    }
    while (auto* item = rowsLayout_->takeAt(0)) {
      if (auto* widget = item->widget()) {
        widget->deleteLater();
      }
      delete item;
    }
  }

  void appendRow(const QVariantMap& task) {
    const QString taskId = task.value(QStringLiteral("taskId")).toString();
    const QString state = task.value(QStringLiteral("state")).toString();
    const QString label = task.value(QStringLiteral("label")).toString();
    const QString statusText = task.value(QStringLiteral("statusText")).toString();
    const QString summary = task.value(QStringLiteral("summary")).toString();
    const int progress = task.value(QStringLiteral("progressPercent")).toInt();

    auto* row = new QWidget(rowsHost_);
    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(2);

    auto* top = new QWidget(row);
    auto* topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(6);

    auto* stateLabel = new QLabel(state, top);
    QFont stateFont = stateLabel->font();
    stateFont.setBold(true);
    stateLabel->setFont(stateFont);
    setWidgetTextColor(stateLabel,
                       (state == QStringLiteral("Running") ||
                        state == QStringLiteral("AwaitingApproval"))
                           ? accentColor()
                           : textColor());
    topLayout->addWidget(stateLabel);

    auto* labelLabel = new QLabel(label, top);
    setWidgetTextColor(labelLabel, textColor());
    topLayout->addWidget(labelLabel, 1);
    layout->addWidget(top);

    const QString detail = summary.isEmpty() ? statusText : summary;
    if (!detail.isEmpty()) {
      auto* detailLabel = new QLabel(detail, row);
      detailLabel->setWordWrap(true);
      setWidgetTextColor(detailLabel, textColor());
      layout->addWidget(detailLabel);
    }

    if (!isTerminalStateName(state)) {
      auto* progressBar = new QProgressBar(row);
      progressBar->setRange(0, 100);
      progressBar->setValue(progress);
      progressBar->setTextVisible(false);
      progressBar->setFixedHeight(6);
      layout->addWidget(progressBar);
    }

    auto* actions = new QWidget(row);
    auto* actionsLayout = new QHBoxLayout(actions);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(6);

    if (state == QStringLiteral("AwaitingApproval")) {
      auto* approveButton = new QPushButton(QStringLiteral("許可"), actions);
      actionsLayout->addWidget(approveButton);
      QObject::connect(approveButton, &QPushButton::clicked, owner_, [this, taskId]() {
        if (auto* service = ArtifactDetachedTaskService::instance()) {
          service->approve(taskId);
        }
        refresh();
      });
      auto* denyButton = new QPushButton(QStringLiteral("拒否"), actions);
      actionsLayout->addWidget(denyButton);
      QObject::connect(denyButton, &QPushButton::clicked, owner_, [this, taskId]() {
        if (auto* service = ArtifactDetachedTaskService::instance()) {
          service->deny(taskId);
        }
        refresh();
      });
    }

    if (!isTerminalStateName(state)) {
      auto* cancelButton = new QPushButton(QStringLiteral("キャンセル"), actions);
      actionsLayout->addWidget(cancelButton);
      QObject::connect(cancelButton, &QPushButton::clicked, owner_, [this, taskId]() {
        if (auto* service = ArtifactDetachedTaskService::instance()) {
          service->cancel(taskId);
        }
        refresh();
      });
    }

    const QString produced =
        task.value(QStringLiteral("producedCompositionId")).toString();
    auto* attachButton = new QPushButton(QStringLiteral("表示"), actions);
    attachButton->setEnabled(!produced.isEmpty());
    if (produced.isEmpty()) {
      attachButton->setToolTip(
          QStringLiteral("この段では生成物の ID を返していないため未対応です"));
    }
    actionsLayout->addWidget(attachButton);
    actionsLayout->addStretch(1);
    layout->addWidget(actions);

    rowsLayout_->addWidget(row);
  }

  void rebuildRows(const QVariantList& tasks) {
    clearRows();
    for (const QVariant& entry : tasks) {
      appendRow(entry.toMap());
    }
    rowsLayout_->addStretch(1);
  }

  void updateIdleTracking(bool allTerminal, bool hasTasks) {
    if (!hasTasks || !allTerminal) {
      idleTimerValid_ = false;
      return;
    }
    if (!idleTimerValid_) {
      idleTimer_.start();
      idleTimerValid_ = true;
      return;
    }
    // Waiting on approval must never be hidden by the collapse behaviour.
    if (expanded_ && idleTimer_.elapsed() >= kCollapseGraceMs) {
      owner_->setExpanded(false);
      saveSettings();
    }
  }

  void refresh() {
    auto* service = ArtifactDetachedTaskService::instance();
    if (!service) {
      return;
    }
    const QVariantList tasks = service->taskSnapshot();
    const QVariantMap context = service->executionContextSnapshot();

    const QString signature = QString::fromUtf8(
        QJsonDocument::fromVariant(tasks).toJson(QJsonDocument::Compact));
    if (signature != lastSignature_) {
      lastSignature_ = signature;
      rebuildRows(tasks);
    }

    const int awaiting = context.value(QStringLiteral("awaitingApprovalCount")).toInt();
    const int running = context.value(QStringLiteral("runningCount")).toInt();
    const int pending = context.value(QStringLiteral("pendingCount")).toInt();
    const int queued = context.value(QStringLiteral("queuedCount")).toInt();

    if (countsLabel_) {
      countsLabel_->setText(QStringLiteral("実行 %1 / 待機 %2 / 承認待ち %3")
                                .arg(running)
                                .arg(ArtifactCore::artifactMax(pending, queued))
                                .arg(awaiting));
      setWidgetTextColor(countsLabel_, textColor());
    }

    const QString deferral = context.value(QStringLiteral("deferral")).toString();
    if (deferredLabel_) {
      deferredLabel_->setText(
          deferral.isEmpty() ? QString()
                             : QStringLiteral("待機理由: %1").arg(deferral));
      deferredLabel_->setVisible(!deferral.isEmpty());
      setWidgetTextColor(deferredLabel_, textColor());
    }

    bool hasTasks = false;
    bool allTerminal = true;
    for (const QVariant& entry : tasks) {
      const QString state = entry.toMap().value(QStringLiteral("state")).toString();
      hasTasks = true;
      if (!isTerminalStateName(state)) {
        allTerminal = false;
        break;
      }
    }
    updateIdleTracking(allTerminal, hasTasks);

    if (!expanded_ && titleLabel_) {
      titleLabel_->setText(QStringLiteral("Detached (%1)").arg(tasks.size()));
    } else if (titleLabel_) {
      titleLabel_->setText(QStringLiteral("Detached"));
    }
  }

  void setExpandedState(bool expanded) {
    expanded_ = expanded;
    if (!owner_->layout()) {
      return;
    }
    const int count = owner_->layout()->count();
    for (int index = 1; index < count; ++index) {
      if (auto* item = owner_->layout()->itemAt(index); item && item->widget()) {
        item->widget()->setVisible(expanded_);
      }
    }
    owner_->resize(expanded_ ? QSize(kExpandedWidth, kExpandedHeight)
                             : QSize(kBadgeWidth, kBadgeHeight));
    if (auto* layout = owner_->layout(); layout) {
      layout->setContentsMargins(expanded_ ? 10 : 6, expanded_ ? 8 : 4,
                                 expanded_ ? 10 : 6, expanded_ ? 8 : 4);
    }
    refresh();
  }
};

ArtifactDetachedTaskTray::ArtifactDetachedTaskTray(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this)) {
  impl_->applyWindowFlags();
  impl_->buildUi();
  impl_->applyPalette();
  impl_->restoreSettings();
  impl_->setExpandedState(impl_->expanded_);

  impl_->refreshTimer_ = new QTimer(this);
  impl_->refreshTimer_->setInterval(kRefreshIntervalMs);
  connect(impl_->refreshTimer_, &QTimer::timeout, this, [this]() { refresh(); });
  impl_->refreshTimer_->start();

  // Task state can be published from whichever thread ran the task, so marshal
  // every notification onto this widget's thread before touching the UI.
  const auto scheduleRefresh = [this]() {
    if (QThread::currentThread() == thread()) {
      refresh();
    } else {
      QMetaObject::invokeMethod(this, [this]() { refresh(); }, Qt::QueuedConnection);
    }
  };
  eventBusSubscriptions_.push_back(
      eventBus_.subscribe<DetachedTaskAddedEvent>(
          [scheduleRefresh](const DetachedTaskAddedEvent&) { scheduleRefresh(); }));
  eventBusSubscriptions_.push_back(
      eventBus_.subscribe<DetachedTaskChangedEvent>(
          [scheduleRefresh](const DetachedTaskChangedEvent&) { scheduleRefresh(); }));
  eventBusSubscriptions_.push_back(
      eventBus_.subscribe<DetachedTaskFinishedEvent>(
          [scheduleRefresh](const DetachedTaskFinishedEvent&) { scheduleRefresh(); }));
}

ArtifactDetachedTaskTray::~ArtifactDetachedTaskTray() {
  if (impl_) {
    impl_->saveSettings();
    delete impl_;
    impl_ = nullptr;
  }
}

void ArtifactDetachedTaskTray::showTray() {
  refresh();
  show();
  raise();
}

void ArtifactDetachedTaskTray::hideTray() {
  if (impl_) {
    impl_->saveSettings();
  }
  hide();
}

void ArtifactDetachedTaskTray::toggleTray() {
  if (isVisible()) {
    hideTray();
  } else {
    showTray();
  }
}

void ArtifactDetachedTaskTray::setExpanded(bool expanded) {
  if (!impl_ || impl_->expanded_ == expanded) {
    return;
  }
  impl_->setExpandedState(expanded);
  impl_->saveSettings();
}

bool ArtifactDetachedTaskTray::isExpanded() const {
  return impl_ && impl_->expanded_;
}

void ArtifactDetachedTaskTray::refresh() {
  if (impl_) {
    impl_->refresh();
  }
}

void ArtifactDetachedTaskTray::paintEvent(QPaintEvent*) {
  if (!impl_) {
    return;
  }
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QColor fill = impl_->surfaceColor();
  fill.setAlpha(235);
  QPainterPath path;
  path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
  painter.fillPath(path, fill);

  QPen pen(impl_->borderColor());
  pen.setWidth(1);
  painter.setPen(pen);
  painter.drawPath(path);
}

void ArtifactDetachedTaskTray::mousePressEvent(QMouseEvent* event) {
  if (!impl_ || event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }
  impl_->dragging_ = true;
  impl_->dragOffset_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
  event->accept();
}

void ArtifactDetachedTaskTray::mouseMoveEvent(QMouseEvent* event) {
  if (!impl_ || !impl_->dragging_) {
    QWidget::mouseMoveEvent(event);
    return;
  }
  move(event->globalPosition().toPoint() - impl_->dragOffset_);
  event->accept();
}

void ArtifactDetachedTaskTray::mouseReleaseEvent(QMouseEvent* event) {
  if (!impl_) {
    QWidget::mouseReleaseEvent(event);
    return;
  }
  if (impl_->dragging_) {
    impl_->dragging_ = false;
    impl_->saveSettings();
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

} // namespace Artifact
