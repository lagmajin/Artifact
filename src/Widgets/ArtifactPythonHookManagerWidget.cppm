module;
#include <wobjectimpl.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QPushButton>
#include <QHeaderView>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>

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
module Artifact.Widgets.PythonHookManagerWidget;

import Artifact.Widgets.PythonHookManagerWidget;
import Artifact.Script.Hooks;

namespace Artifact {

W_OBJECT_IMPL(ArtifactPythonHookManagerWidget)

class ArtifactPythonHookManagerWidget::Impl {
public:
 QTreeWidget* table = nullptr;
 bool updating = false;
};

ArtifactPythonHookManagerWidget::ArtifactPythonHookManagerWidget(QWidget* parent)
 : QWidget(parent), impl_(new Impl())
{
 auto* root = new QVBoxLayout(this);
 root->setContentsMargins(8, 8, 8, 8);
 root->setSpacing(6);

 impl_->table = new QTreeWidget(this);
 impl_->table->setColumnCount(3);
 impl_->table->setHeaderLabels(QStringList() << "Hook" << "Enabled" << "Script Path");
 impl_->table->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
 impl_->table->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
 impl_->table->header()->setSectionResizeMode(2, QHeaderView::Stretch);
 root->addWidget(impl_->table, 1);

 auto* buttons = new QHBoxLayout();
 auto* refreshBtn = new QPushButton("Refresh", this);
 auto* openBtn = new QPushButton("Open Script", this);
 buttons->addWidget(refreshBtn);
 buttons->addWidget(openBtn);
 buttons->addStretch();
 root->addLayout(buttons);

 connect(refreshBtn, &QPushButton::clicked, this, &ArtifactPythonHookManagerWidget::refresh);
 connect(openBtn, &QPushButton::clicked, this, [this]() {
  auto* item = impl_->table->currentItem();
  if (!item) return;
  const QString path = item->text(2);
  if (QFileInfo::exists(path)) {
   QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  }
 });

 connect(impl_->table, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int col) {
  if (!item || col != 1 || impl_->updating) return;
  ArtifactPythonHookManager::setHookEnabled(item->text(0), item->checkState(1) == Qt::Checked);
 });

 refresh();
}

ArtifactPythonHookManagerWidget::~ArtifactPythonHookManagerWidget()
{
 delete impl_;
}

void ArtifactPythonHookManagerWidget::refresh()
{
 if (!impl_ || !impl_->table) return;
 impl_->updating = true;
 impl_->table->clear();

 for (const QString& hook : ArtifactPythonHookManager::knownHooks()) {
  auto* item = new QTreeWidgetItem(impl_->table);
  item->setText(0, hook);
  item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
  item->setCheckState(1, ArtifactPythonHookManager::isHookEnabled(hook) ? Qt::Checked : Qt::Unchecked);
  item->setText(2, ArtifactPythonHookManager::hookScriptPath(hook));
 }

 impl_->updating = false;
}

}
