module;
#include <algorithm>
#include <QColor>
#include <QAbstractItemView>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPalette>
#include <QPushButton>
#include <QSize>
#include <QStyle>
#include <QStringList>
#include <QTabWidget>
#include <QThread>
#include <QMetaObject>
#include <QVBoxLayout>
#include <wobjectimpl.h>

module Artifact.Widgets.HistoryTimelineWidget;

import Artifact.Event.Types;
import Event.Bus;
import Undo.UndoManager;
import Widgets.Utils.CSS;
import std;

namespace Artifact {

namespace {
enum HistoryRoles {
  UndoDepthRole = Qt::UserRole + 1,
  HistoryKindRole,
};

QString actorForLabel(const QString& label) {
  if (label.startsWith(QStringLiteral("AI"), Qt::CaseInsensitive) ||
      label.contains(QStringLiteral("generated"), Qt::CaseInsensitive) ||
      label.contains(QStringLiteral("optimized"), Qt::CaseInsensitive)) {
    return QStringLiteral("AI");
  }
  return QStringLiteral("User");
}

bool isCheckpointLabel(const QString& label) {
  return label.contains(QStringLiteral("checkpoint"), Qt::CaseInsensitive) ||
         label.contains(QStringLiteral("snapshot"), Qt::CaseInsensitive) ||
         label.contains(QStringLiteral("working version"), Qt::CaseInsensitive);
}

QColor actorColor(const QString& actor, bool checkpoint) {
  if (checkpoint) return QColor(232, 177, 65);
  if (actor == QStringLiteral("AI")) return QColor(164, 104, 232);
  return QColor(54, 166, 232);
}
} // namespace

class ArtifactHistoryTimelineWidget::Impl {
public:
  QListWidget* projectList = nullptr;
  QLabel* selectedTitle = nullptr;
  QLabel* selectedMeta = nullptr;
  QLabel* previewState = nullptr;
  QPushButton* previewButton = nullptr;
  QPushButton* restoreButton = nullptr;
  QPushButton* partialRestoreButton = nullptr;
  ArtifactCore::EventBus eventBus = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> subscriptions;
};

W_OBJECT_IMPL(ArtifactHistoryTimelineWidget)

ArtifactHistoryTimelineWidget::ArtifactHistoryTimelineWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
  setMinimumSize(620, 260);
  setAutoFillBackground(true);
  QPalette rootPalette = palette();
  const auto& theme = ArtifactCore::currentDCCTheme();
  rootPalette.setColor(QPalette::Window, QColor(theme.backgroundColor));
  rootPalette.setColor(QPalette::WindowText, QColor(theme.textColor));
  setPalette(rootPalette);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(6);

  auto* tabs = new QTabWidget(this);
  tabs->setDocumentMode(true);
  auto* projectPage = new QWidget(tabs);
  auto* projectLayout = new QHBoxLayout(projectPage);
  projectLayout->setContentsMargins(0, 4, 0, 0);
  projectLayout->setSpacing(8);

  impl_->projectList = new QListWidget(projectPage);
  impl_->projectList->setAlternatingRowColors(true);
  impl_->projectList->setSelectionMode(QAbstractItemView::SingleSelection);
  impl_->projectList->setMinimumWidth(350);
  impl_->projectList->installEventFilter(this);
  projectLayout->addWidget(impl_->projectList, 3);

  auto* inspector = new QFrame(projectPage);
  inspector->setFrameShape(QFrame::StyledPanel);
  auto* inspectorLayout = new QVBoxLayout(inspector);
  inspectorLayout->setContentsMargins(10, 8, 10, 8);
  inspectorLayout->setSpacing(6);
  auto* inspectorHeading = new QLabel(QStringLiteral("CHECKPOINT INSPECTOR"), inspector);
  QFont headingFont = inspectorHeading->font();
  headingFont.setBold(true);
  inspectorHeading->setFont(headingFont);
  inspectorLayout->addWidget(inspectorHeading);
  impl_->selectedTitle = new QLabel(QStringLiteral("Current"), inspector);
  QFont titleFont = impl_->selectedTitle->font();
  titleFont.setPointSize(titleFont.pointSize() + 2);
  titleFont.setBold(true);
  impl_->selectedTitle->setFont(titleFont);
  inspectorLayout->addWidget(impl_->selectedTitle);
  impl_->selectedMeta = new QLabel(QStringLiteral("Select an operation to inspect its state."), inspector);
  impl_->selectedMeta->setWordWrap(true);
  inspectorLayout->addWidget(impl_->selectedMeta);
  impl_->previewState = new QLabel(
      QStringLiteral("Preview is non-destructive. Restore applies Undo operations only after an explicit click."),
      inspector);
  impl_->previewState->setWordWrap(true);
  impl_->previewState->setFrameShape(QFrame::StyledPanel);
  impl_->previewState->setMinimumHeight(64);
  inspectorLayout->addWidget(impl_->previewState);
  impl_->previewButton = new QPushButton(QStringLiteral("Preview State"), inspector);
  impl_->restoreButton = new QPushButton(QStringLiteral("Restore to Selected"), inspector);
  impl_->partialRestoreButton = new QPushButton(QStringLiteral("Copy Settings to Current"), inspector);
  impl_->previewButton->installEventFilter(this);
  impl_->restoreButton->installEventFilter(this);
  impl_->partialRestoreButton->setEnabled(false);
  impl_->partialRestoreButton->setToolTip(
      QStringLiteral("Available when the selected command exposes a serializable property payload."));
  inspectorLayout->addWidget(impl_->previewButton);
  inspectorLayout->addWidget(impl_->restoreButton);
  inspectorLayout->addWidget(impl_->partialRestoreButton);
  inspectorLayout->addStretch();
  projectLayout->addWidget(inspector, 2);
  tabs->addTab(projectPage, QStringLiteral("Project History"));

  auto* patchPage = new QWidget(tabs);
  auto* patchLayout = new QVBoxLayout(patchPage);
  patchLayout->setContentsMargins(12, 12, 12, 12);
  auto* patchTitle = new QLabel(QStringLiteral("Source Patch History"), patchPage);
  patchTitle->setFont(titleFont);
  patchLayout->addWidget(patchTitle);
  auto* patchInfo = new QLabel(
      QStringLiteral("Source patches appear here when an AI or automation records a patch-backed edit. "
                     "Project operations remain separate and can be inspected without checking out source state."),
      patchPage);
  patchInfo->setWordWrap(true);
  patchLayout->addWidget(patchInfo);
  patchLayout->addStretch();
  tabs->addTab(patchPage, QStringLiteral("Source Patch History"));
  root->addWidget(tabs, 1);

  if (UndoManager::instance()) {
    impl_->subscriptions.push_back(impl_->eventBus.subscribe<UndoManagerChangedEvent>(
        [this](const UndoManagerChangedEvent& event) {
          if (event.kind != UndoManagerChangeKind::HistoryChanged) return;
          const auto refresh = [this]() { if (impl_) refreshHistory(); };
          if (QThread::currentThread() == thread()) refresh();
          else QMetaObject::invokeMethod(this, refresh, Qt::QueuedConnection);
        }));
  }
  refreshHistory();
}

ArtifactHistoryTimelineWidget::~ArtifactHistoryTimelineWidget() {
  delete impl_;
  impl_ = nullptr;
}

void ArtifactHistoryTimelineWidget::refreshHistory() {
  if (!impl_ || !impl_->projectList) return;
  auto* manager = UndoManager::instance();
  const QStringList undoLabels = manager ? manager->undoHistoryLabels() : QStringList{};
  const QStringList redoLabels = manager ? manager->redoHistoryLabels() : QStringList{};
  impl_->projectList->clear();

  auto* start = new QListWidgetItem(QStringLiteral("○  Start"), impl_->projectList);
  start->setFlags(start->flags() & ~Qt::ItemIsSelectable);
  int depth = static_cast<int>(undoLabels.size());
  for (auto it = undoLabels.crbegin(); it != undoLabels.crend(); ++it) {
    const QString actor = actorForLabel(*it);
    const bool checkpoint = isCheckpointLabel(*it);
    const QString node = checkpoint ? QStringLiteral("◆") : QStringLiteral("●");
    auto* item = new QListWidgetItem(
        QStringLiteral("│  %1  %2 · %3").arg(node, actor, *it), impl_->projectList);
    item->setData(UndoDepthRole, depth--);
    item->setData(HistoryKindRole, checkpoint ? QStringLiteral("Checkpoint") : actor);
    item->setForeground(actorColor(actor, checkpoint));
    item->setToolTip(QStringLiteral("Preview or restore the project to this operation."));
    item->setSizeHint(QSize(item->sizeHint().width(), 30));
  }
  auto* current = new QListWidgetItem(QStringLiteral("│  ◉  Current"), impl_->projectList);
  current->setData(UndoDepthRole, 0);
  current->setData(HistoryKindRole, QStringLiteral("Current"));
  current->setForeground(QColor(92, 205, 232));
  current->setSizeHint(QSize(current->sizeHint().width(), 32));

  int branch = 1;
  for (const QString& label : redoLabels) {
    auto* item = new QListWidgetItem(
        QStringLiteral("└─○  Alternate %1 · %2").arg(branch++, label), impl_->projectList);
    item->setData(UndoDepthRole, -1);
    item->setData(HistoryKindRole, QStringLiteral("Redo branch"));
    item->setForeground(QColor(128, 135, 145));
  }
  impl_->projectList->setCurrentItem(current);
  updateInspector();
}

void ArtifactHistoryTimelineWidget::updateInspector() {
  if (!impl_) return;
  const auto* item = impl_->projectList ? impl_->projectList->currentItem() : nullptr;
  if (!item) return;
  const QString title = item->text().section(QStringLiteral(" · "), -1);
  const QString kind = item->data(HistoryKindRole).toString();
  const int depth = item->data(UndoDepthRole).toInt();
  impl_->selectedTitle->setText(title);
  impl_->selectedMeta->setText(
      QStringLiteral("%1 · %2 undo step(s) from Current").arg(kind).arg(std::max(0, depth)));
  impl_->restoreButton->setEnabled(depth > 0);
  impl_->previewButton->setEnabled(depth >= 0);
  impl_->previewState->setText(
      depth > 0
          ? QStringLiteral("Temporary state preview selected. The current project is unchanged.")
          : QStringLiteral("This is the current project state."));
}

void ArtifactHistoryTimelineWidget::restoreSelectedState() {
  if (!impl_ || !impl_->projectList) return;
  const auto* item = impl_->projectList->currentItem();
  int count = item ? item->data(UndoDepthRole).toInt() : 0;
  auto* manager = UndoManager::instance();
  while (manager && count-- > 0 && manager->canUndo()) manager->undo();
}

bool ArtifactHistoryTimelineWidget::eventFilter(QObject* watched, QEvent* event) {
  if (!impl_ || !event) return QWidget::eventFilter(watched, event);
  if (watched == impl_->projectList && event->type() == QEvent::MouseButtonRelease) {
    updateInspector();
  } else if (event->type() == QEvent::MouseButtonRelease) {
    auto* mouse = static_cast<QMouseEvent*>(event);
    if (mouse->button() == Qt::LeftButton && watched == impl_->previewButton) {
      updateInspector();
      return true;
    }
    if (mouse->button() == Qt::LeftButton && watched == impl_->restoreButton) {
      restoreSelectedState();
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

} // namespace Artifact
