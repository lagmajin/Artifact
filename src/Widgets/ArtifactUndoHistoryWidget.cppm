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

import Artifact.Widgets.UndoHistoryWidget;
import Undo.UndoManager;
import Widgets.Utils.CSS;

namespace Artifact {

W_OBJECT_IMPL(ArtifactUndoHistoryWidget)

class ArtifactUndoHistoryWidget::Impl {
public:
 QLabel* summaryLabel = nullptr;
 QListWidget* undoList = nullptr;
 QListWidget* redoList = nullptr;
 QPushButton* undoButton = nullptr;
 QPushButton* redoButton = nullptr;
 QPushButton* clearButton = nullptr;
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

 impl_->summaryLabel = new QLabel("Undo: 0 / Redo: 0", this);
 {
  QFont f = impl_->summaryLabel->font();
  f.setBold(true);
  impl_->summaryLabel->setFont(f);
  QPalette pal = impl_->summaryLabel->palette();
  pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
  impl_->summaryLabel->setPalette(pal);
 }
 root->addWidget(impl_->summaryLabel);

 auto* split = new QHBoxLayout();
 split->setSpacing(8);

 auto* undoFrame = new QFrame(this);
 undoFrame->setFrameShape(QFrame::StyledPanel);
  auto* undoLayout = new QVBoxLayout(undoFrame);
  undoLayout->setContentsMargins(6, 6, 6, 6);
  undoLayout->addWidget(new QLabel("Undo Stack", undoFrame));
 impl_->undoList = new QListWidget(undoFrame);
 {
  QPalette pal = impl_->undoList->palette();
  pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
  pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
  pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
  pal.setColor(QPalette::Highlight, QColor(ArtifactCore::currentDCCTheme().accentColor));
  impl_->undoList->setPalette(pal);
 }
  undoLayout->addWidget(impl_->undoList);
 split->addWidget(undoFrame, 1);

 auto* redoFrame = new QFrame(this);
 redoFrame->setFrameShape(QFrame::StyledPanel);
 auto* redoLayout = new QVBoxLayout(redoFrame);
 redoLayout->setContentsMargins(6, 6, 6, 6);
 redoLayout->addWidget(new QLabel("Redo Stack", redoFrame));
 impl_->redoList = new QListWidget(redoFrame);
 {
  QPalette pal = impl_->redoList->palette();
  pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
  pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
  pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
  pal.setColor(QPalette::Highlight, QColor(ArtifactCore::currentDCCTheme().accentColor));
  impl_->redoList->setPalette(pal);
 }
  redoLayout->addWidget(impl_->redoList);
 split->addWidget(redoFrame, 1);

 root->addLayout(split, 1);

 auto* buttons = new QHBoxLayout();
 buttons->setSpacing(6);
 impl_->undoButton = new QPushButton("Undo", this);
 impl_->redoButton = new QPushButton("Redo", this);
 impl_->clearButton = new QPushButton("Clear", this);
 {
  QPalette pal = impl_->undoButton->palette();
  pal.setColor(QPalette::Button, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
  pal.setColor(QPalette::ButtonText, QColor(ArtifactCore::currentDCCTheme().textColor));
  impl_->undoButton->setPalette(pal);
  impl_->redoButton->setPalette(pal);
  impl_->clearButton->setPalette(pal);
 }
  buttons->addWidget(impl_->undoButton);
  buttons->addWidget(impl_->redoButton);
  buttons->addStretch();
  buttons->addWidget(impl_->clearButton);
  root->addLayout(buttons);

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
  connect(mgr, &UndoManager::historyChanged, this, &ArtifactUndoHistoryWidget::refreshHistory);
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
 impl_->undoList->addItems(undoLabels);
 impl_->redoList->addItems(redoLabels);

 impl_->undoButton->setEnabled(mgr->canUndo());
 impl_->redoButton->setEnabled(mgr->canRedo());
 impl_->summaryLabel->setText(QString("Undo: %1 / Redo: %2").arg(undoLabels.size()).arg(redoLabels.size()));
}

}
