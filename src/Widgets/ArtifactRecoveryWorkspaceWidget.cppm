module;
#include <wobjectimpl.h>
#include <QComboBox>
#include <QDateTime>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <vector>

module Artifact.Widgets.RecoveryWorkspace;

import Core.Diagnostics.SessionLedger;
import Artifact.Project.Manager;
import Artifact.Render.Queue.Service;

namespace Artifact {

W_OBJECT_IMPL(ArtifactRecoveryWorkspaceWidget)

class ArtifactRecoveryWorkspaceWidget::Impl {
public:
  QString ledgerPath;
  ArtifactCore::SessionLedger ledger;
  QLabel* summary = nullptr;
  QComboBox* filter = nullptr;
  QListWidget* entries = nullptr;
  QPushButton* refresh = nullptr;
  QPushButton* action = nullptr;
  std::vector<ArtifactCore::SessionLedgerEntry> visibleEntries;
};

ArtifactRecoveryWorkspaceWidget::ArtifactRecoveryWorkspaceWidget(
    const QString& ledgerPath, QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
  impl_->ledgerPath = ledgerPath;
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(6);

  impl_->summary = new QLabel(this);
  root->addWidget(impl_->summary);
  auto* controls = new QHBoxLayout();
  impl_->filter = new QComboBox(this);
  impl_->filter->addItems({QStringLiteral("Recent"), QStringLiteral("Failed"),
                           QStringLiteral("Recoverable")});
  impl_->refresh = new QPushButton(QStringLiteral("Refresh"), this);
  impl_->action = new QPushButton(QStringLiteral("Open / Resume Selected"), this);
  controls->addWidget(impl_->filter, 1);
  controls->addWidget(impl_->action);
  controls->addWidget(impl_->refresh);
  root->addLayout(controls);
  impl_->entries = new QListWidget(this);
  root->addWidget(impl_->entries, 1);

  QObject::connect(impl_->filter, &QComboBox::currentIndexChanged, this,
                   [this](int) { refreshLedger(); });
  QObject::connect(impl_->refresh, &QPushButton::clicked, this,
                   [this]() { refreshLedger(); });
  QObject::connect(impl_->action, &QPushButton::clicked, this, [this]() {
    if (!impl_ || !impl_->entries || !impl_->entries->currentItem()) return;
    const int row = impl_->entries->row(impl_->entries->currentItem());
    if (row < 0 || row >= static_cast<int>(impl_->visibleEntries.size())) return;
    const auto& entry = impl_->visibleEntries[static_cast<size_t>(row)];
    if (entry.kind == ArtifactCore::SessionEntryKind::RenderFailed &&
        entry.jobIndex >= 0) {
      if (auto* service = ArtifactRenderQueueService::instance()) {
        service->resumeRenderQueuesAt({entry.jobIndex});
      }
      return;
    }
    for (const auto& point : impl_->ledger.recoveryPoints()) {
      if (!point.snapshotPath.isEmpty() &&
          (entry.projectId.isEmpty() || entry.projectId == point.projectId)) {
        ArtifactProjectManager::getInstance().loadFromFile(point.snapshotPath);
        return;
      }
    }
  });
  refreshLedger();
}

ArtifactRecoveryWorkspaceWidget::~ArtifactRecoveryWorkspaceWidget() {
  delete impl_;
}

void ArtifactRecoveryWorkspaceWidget::refreshLedger() {
  if (!impl_) return;
  impl_->entries->clear();
  impl_->visibleEntries.clear();
  const bool loaded = impl_->ledger.loadFromFile(impl_->ledgerPath);
  const auto& all = impl_->ledger.entries();
  int shown = 0;
  const int mode = impl_->filter->currentIndex();
  for (auto it = all.rbegin(); it != all.rend(); ++it) {
    if (mode == 1 && it->kind != ArtifactCore::SessionEntryKind::RenderFailed) continue;
    if (mode == 2 && !it->isRecoverable) continue;
    const auto timestamp = QDateTime::fromMSecsSinceEpoch(it->timestampMs);
    const QString label = QStringLiteral("%1  %2  %3")
        .arg(timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
             ArtifactCore::sessionEntryKindToString(it->kind), it->detail);
    impl_->entries->addItem(label);
    impl_->visibleEntries.push_back(*it);
    ++shown;
  }
  impl_->summary->setText(QStringLiteral("%1 entries  |  %2 recovery points%3")
      .arg(shown)
      .arg(static_cast<int>(impl_->ledger.recoveryPoints().size()))
      .arg(loaded ? QString() : QStringLiteral("  (ledger not found)")));
}

}
