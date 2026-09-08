module;
#include <wobjectimpl.h>
#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QMetaObject>
#include <QThread>
#include <QStyle>
#include <QSize>
#include <QIcon>

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
module Artifact.Widgets.UndoHistoryWidget;

import Undo.UndoManager;
import Widgets.Utils.CSS;
import Event.Bus;
import Artifact.Event.Types;

namespace Artifact {

W_OBJECT_IMPL(ArtifactUndoHistoryWidget)

namespace {
QIcon historyIconForLabel(QStyle* style, const QString& label) {
 if (!style) return {};
 if (label.contains(QStringLiteral("keyframe"), Qt::CaseInsensitive))
  return style->standardIcon(QStyle::SP_DialogApplyButton);
 if (label.contains(QStringLiteral("delete"), Qt::CaseInsensitive) ||
     label.contains(QStringLiteral("remove"), Qt::CaseInsensitive))
  return style->standardIcon(QStyle::SP_TrashIcon);
 if (label.contains(QStringLiteral("add"), Qt::CaseInsensitive) ||
     label.contains(QStringLiteral("create"), Qt::CaseInsensitive))
  return style->standardIcon(QStyle::SP_FileDialogNewFolder);
 if (label.contains(QStringLiteral("move"), Qt::CaseInsensitive) ||
     label.contains(QStringLiteral("position"), Qt::CaseInsensitive))
  return style->standardIcon(QStyle::SP_ArrowRight);
 if (label.contains(QStringLiteral("font"), Qt::CaseInsensitive) ||
     label.contains(QStringLiteral("text"), Qt::CaseInsensitive))
  return style->standardIcon(QStyle::SP_FileIcon);
 return style->standardIcon(QStyle::SP_CommandLink);
}

void addHistoryRows(QListWidget* list, const QStringList& labels,
                    QStyle* style) {
 if (!list) return;
 for (const QString& label : labels) {
  auto* item = new QListWidgetItem(historyIconForLabel(style, label), label);
  item->setToolTip(label);
  item->setSizeHint(QSize(item->sizeHint().width(), 30));
  list->addItem(item);
 }
}
} // namespace

class ArtifactUndoHistoryWidget::Impl {
public:
 QLabel* summaryLabel = nullptr;
 QLabel* nextUndoLabel = nullptr;
 QLabel* nextRedoLabel = nullptr;
 QListWidget* undoList = nullptr;
 QListWidget* redoList = nullptr;
 QPushButton* undoButton = nullptr;
 QPushButton* redoButton = nullptr;
 QPushButton* clearButton = nullptr;
 ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
 std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
};

ArtifactUndoHistoryWidget::ArtifactUndoHistoryWidget(QWidget* parent)
 : QWidget(parent), impl_(new Impl())
{
 auto* root = new QVBoxLayout(this);
 root->setContentsMargins(8, 8, 8, 8);
 root->setSpacing(8);

 setAutoFillBackground(true);
 QPalette widgetPalette = palette();
 widgetPalette.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
 widgetPalette.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
 setPalette(widgetPalette);

 auto* toolbar = new QHBoxLayout();
 toolbar->setSpacing(6);
 impl_->undoButton = new QPushButton("Undo", this);
 impl_->redoButton = new QPushButton("Redo", this);
 impl_->clearButton = new QPushButton("Clear History", this);
 impl_->undoButton->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
 impl_->redoButton->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
 impl_->clearButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
 toolbar->addWidget(impl_->undoButton);
 toolbar->addWidget(impl_->redoButton);
 toolbar->addStretch();
 impl_->summaryLabel = new QLabel("Undo: 0 / Redo: 0", this);
 {
  QFont f = impl_->summaryLabel->font();
  f.setBold(true);
  impl_->summaryLabel->setFont(f);
  QPalette pal = impl_->summaryLabel->palette();
  pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
  impl_->summaryLabel->setPalette(pal);
 }
 toolbar->addWidget(impl_->summaryLabel);
 toolbar->addSpacing(8);
 toolbar->addWidget(impl_->clearButton);
 root->addLayout(toolbar);

 auto* split = new QHBoxLayout();
 split->setSpacing(8);

 auto* undoFrame = new QFrame(this);
 undoFrame->setFrameShape(QFrame::StyledPanel);
  auto* undoLayout = new QVBoxLayout(undoFrame);
  undoLayout->setContentsMargins(6, 6, 6, 6);
  auto* undoTitle = new QLabel("Undo Stack", undoFrame);
  QFont undoTitleFont = undoTitle->font();
  undoTitleFont.setBold(true);
  undoTitle->setFont(undoTitleFont);
  undoLayout->addWidget(undoTitle);
 impl_->nextUndoLabel = new QLabel("Next undo: —", undoFrame);
 undoLayout->addWidget(impl_->nextUndoLabel);
 impl_->undoList = new QListWidget(undoFrame);
 impl_->undoList->setIconSize(QSize(18, 18));
 {
  QPalette pal = impl_->undoList->palette();
  pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
  pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
  pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
  pal.setColor(QPalette::Highlight, QColor(ArtifactCore::currentDCCTheme().accentColor));
  impl_->undoList->setPalette(pal);
 }
  undoLayout->addWidget(impl_->undoList);
 split->addWidget(undoFrame, 3);

 auto* redoFrame = new QFrame(this);
 redoFrame->setFrameShape(QFrame::StyledPanel);
 auto* redoLayout = new QVBoxLayout(redoFrame);
 redoLayout->setContentsMargins(6, 6, 6, 6);
 auto* redoTitle = new QLabel("Redo Stack", redoFrame);
 QFont redoTitleFont = redoTitle->font();
 redoTitleFont.setBold(true);
 redoTitle->setFont(redoTitleFont);
 redoLayout->addWidget(redoTitle);
 impl_->nextRedoLabel = new QLabel("Next redo: —", redoFrame);
 redoLayout->addWidget(impl_->nextRedoLabel);
 impl_->redoList = new QListWidget(redoFrame);
 impl_->redoList->setIconSize(QSize(18, 18));
 {
  QPalette pal = impl_->redoList->palette();
  pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
  pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
  pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
  pal.setColor(QPalette::Highlight, QColor(ArtifactCore::currentDCCTheme().accentColor));
  impl_->redoList->setPalette(pal);
 }
  redoLayout->addWidget(impl_->redoList);
 split->addWidget(redoFrame, 2);

 root->addLayout(split, 1);

 {
  QPalette pal = impl_->undoButton->palette();
  pal.setColor(QPalette::Button, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
  pal.setColor(QPalette::ButtonText, QColor(ArtifactCore::currentDCCTheme().textColor));
  impl_->undoButton->setPalette(pal);
  impl_->redoButton->setPalette(pal);
  impl_->clearButton->setPalette(pal);
 }
 UndoManager* mgr = UndoManager::instance();
 connect(impl_->undoButton, &QPushButton::clicked, this, [mgr]() {
  if (mgr) mgr->undo();
 });
 connect(impl_->redoButton, &QPushButton::clicked, this, [mgr]() {
  if (mgr) mgr->redo();
 });
 connect(impl_->clearButton, &QPushButton::clicked, this, [mgr]() {
  if (mgr) mgr->clearHistory();
 });
 if (mgr) {
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<UndoManagerChangedEvent>(
          [this](const UndoManagerChangedEvent& event) {
            if (event.kind != UndoManagerChangeKind::HistoryChanged) return;
            const auto refresh = [this]() {
              if (impl_) refreshHistory();
            };
            if (QThread::currentThread() == thread()) {
              refresh();
            } else {
              QMetaObject::invokeMethod(this, refresh, Qt::QueuedConnection);
            }
          }));
 }

 refreshHistory();
}

ArtifactUndoHistoryWidget::~ArtifactUndoHistoryWidget() {
 delete impl_;
}

void ArtifactUndoHistoryWidget::refreshHistory() {
 UndoManager* mgr = UndoManager::instance();
 if (!mgr || !impl_) return;

 const QStringList undoLabels = mgr->undoHistoryLabels();
 const QStringList redoLabels = mgr->redoHistoryLabels();

 impl_->undoList->clear();
 impl_->redoList->clear();
 addHistoryRows(impl_->undoList, undoLabels, style());
 addHistoryRows(impl_->redoList, redoLabels, style());

 if (!undoLabels.isEmpty()) {
  impl_->undoList->setCurrentRow(0);
 }

 impl_->undoButton->setEnabled(mgr->canUndo());
 impl_->redoButton->setEnabled(mgr->canRedo());
 impl_->nextUndoLabel->setText(
     QStringLiteral("Next undo: %1")
         .arg(undoLabels.isEmpty() ? QStringLiteral("—") : undoLabels.front()));
 impl_->nextRedoLabel->setText(
     QStringLiteral("Next redo: %1")
         .arg(redoLabels.isEmpty() ? QStringLiteral("—") : redoLabels.front()));
 impl_->summaryLabel->setText(QString("Undo: %1 / Redo: %2").arg(undoLabels.size()).arg(redoLabels.size()));
}

}
