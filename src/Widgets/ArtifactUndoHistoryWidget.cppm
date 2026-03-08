module;
#include <wobjectimpl.h>
#include <QFrame>

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

 impl_->summaryLabel = new QLabel("Undo: 0 / Redo: 0", this);
 impl_->summaryLabel->setStyleSheet("font-weight: bold; color: #ddd;");
 root->addWidget(impl_->summaryLabel);

 auto* split = new QHBoxLayout();
 split->setSpacing(8);

 auto* undoFrame = new QFrame(this);
 auto* undoLayout = new QVBoxLayout(undoFrame);
 undoLayout->setContentsMargins(6, 6, 6, 6);
 undoLayout->addWidget(new QLabel("Undo Stack", undoFrame));
 impl_->undoList = new QListWidget(undoFrame);
 undoLayout->addWidget(impl_->undoList);
 split->addWidget(undoFrame, 1);

 auto* redoFrame = new QFrame(this);
 auto* redoLayout = new QVBoxLayout(redoFrame);
 redoLayout->setContentsMargins(6, 6, 6, 6);
 redoLayout->addWidget(new QLabel("Redo Stack", redoFrame));
 impl_->redoList = new QListWidget(redoFrame);
 redoLayout->addWidget(impl_->redoList);
 split->addWidget(redoFrame, 1);

 root->addLayout(split, 1);

 auto* buttons = new QHBoxLayout();
 buttons->setSpacing(6);
 impl_->undoButton = new QPushButton("Undo", this);
 impl_->redoButton = new QPushButton("Redo", this);
 impl_->clearButton = new QPushButton("Clear", this);
 buttons->addWidget(impl_->undoButton);
 buttons->addWidget(impl_->redoButton);
 buttons->addStretch();
 buttons->addWidget(impl_->clearButton);
 root->addLayout(buttons);

 setStyleSheet(R"(
  QWidget { background-color: #1e1e1e; color: #ccc; }
  QFrame { border: 1px solid #333; border-radius: 4px; background-color: #252526; }
  QListWidget { border: 1px solid #3a3a3a; background: #1e1e1e; }
  QPushButton { background: #3b3b3b; border: 1px solid #5a5a5a; padding: 4px 8px; border-radius: 3px; }
  QPushButton:hover { background: #4a4a4a; }
 )");

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
