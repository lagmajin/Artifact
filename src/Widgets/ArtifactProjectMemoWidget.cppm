module;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListView>
#include <QPushButton>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QInputDialog>
#include <QLabel>
#include <wobjectimpl.h>

module Artifact.Widgets.ProjectMemoWidget;

import std;
import Artifact.Widgets.ProjectMemoModel;
import Localization.Localization;

namespace Artifact {

W_OBJECT_IMPL(ArtifactProjectMemoWidget)

class ArtifactProjectMemoWidget::Impl {
public:
  explicit Impl(ArtifactProjectMemoWidget *parent);

  ArtifactProjectMemoWidget *widget = nullptr;
  ArtifactProjectMemoModel model_;
  QListView *listView_ = nullptr;
  QStandardItemModel *itemModel_ = nullptr;
  QPushButton *addButton_ = nullptr;
  QPushButton *removeButton_ = nullptr;
  QPushButton *editButton_ = nullptr;
  qint64 currentFrame_ = 0;

  void refreshList();
  void onAddMemo();
  void onRemoveMemo();
  void onEditMemo();
  void onDoubleClicked(const QModelIndex &index);
};

ArtifactProjectMemoWidget::Impl::Impl(ArtifactProjectMemoWidget *parent)
    : widget(parent) {
  auto *mainLayout = new QVBoxLayout(parent);
  mainLayout->setContentsMargins(4, 4, 4, 4);
  mainLayout->setSpacing(4);

  auto *buttonLayout = new QHBoxLayout();
  addButton_ = new QPushButton(QObject::tr("Add"), parent);
  removeButton_ = new QPushButton(QObject::tr("Remove"), parent);
  editButton_ = new QPushButton(QObject::tr("Edit"), parent);
  buttonLayout->addWidget(addButton_);
  buttonLayout->addWidget(removeButton_);
  buttonLayout->addWidget(editButton_);
  buttonLayout->addStretch();
  mainLayout->addLayout(buttonLayout);

  listView_ = new QListView(parent);
  listView_->setSelectionMode(QAbstractItemView::SingleSelection);
  itemModel_ = new QStandardItemModel(parent);
  listView_->setModel(itemModel_);
  mainLayout->addWidget(listView_);

  auto *hintLabel = new QLabel(QObject::tr("Double-click a memo to jump to frame."), parent);
  hintLabel->setWordWrap(true);
  mainLayout->addWidget(hintLabel);

  QObject::connect(addButton_, &QPushButton::clicked, parent,
                   [this]() { onAddMemo(); });
  QObject::connect(removeButton_, &QPushButton::clicked, parent,
                   [this]() { onRemoveMemo(); });
  QObject::connect(editButton_, &QPushButton::clicked, parent,
                   [this]() { onEditMemo(); });
  QObject::connect(listView_, &QListView::doubleClicked, parent,
                   [this](const QModelIndex &index) { onDoubleClicked(index); });
}

void ArtifactProjectMemoWidget::Impl::refreshList() {
  itemModel_->clear();
  const auto memos = model_.memos();
  for (const auto &memo : memos) {
    auto *item = new QStandardItem(
        QStringLiteral("F%1: %2").arg(memo.frame).arg(memo.text));
    item->setData(memo.id.toString(), Qt::UserRole);
    item->setToolTip(memo.text);
    itemModel_->appendRow(item);
  }
}

void ArtifactProjectMemoWidget::Impl::onAddMemo() {
  bool ok = false;
  const QString text = QInputDialog::getText(
      widget, QObject::tr("Add Memo"), QObject::tr("Memo text:"),
      QLineEdit::Normal, QString(), &ok);
  if (!ok || text.isEmpty()) {
    return;
  }
  model_.addMemo(currentFrame_, text, QColor(255, 220, 100));
  refreshList();
}

void ArtifactProjectMemoWidget::Impl::onRemoveMemo() {
  const QModelIndex index = listView_->currentIndex();
  if (!index.isValid()) {
    return;
  }
  const QString idString = index.data(Qt::UserRole).toString();
  model_.removeMemo(QUuid(idString));
  refreshList();
}

void ArtifactProjectMemoWidget::Impl::onEditMemo() {
  const QModelIndex index = listView_->currentIndex();
  if (!index.isValid()) {
    return;
  }
  const QString idString = index.data(Qt::UserRole).toString();
  const QUuid id(idString);
  const auto memos = model_.memos();
  const auto it = std::find_if(
      memos.cbegin(), memos.cend(),
      [&id](const ProjectMemo &memo) { return memo.id == id; });
  if (it == memos.cend()) {
    return;
  }
  bool ok = false;
  const QString text = QInputDialog::getText(
      widget, QObject::tr("Edit Memo"), QObject::tr("Memo text:"),
      QLineEdit::Normal, it->text, &ok);
  if (!ok) {
    return;
  }
  model_.updateMemo(id, text, it->color);
  refreshList();
}

void ArtifactProjectMemoWidget::Impl::onDoubleClicked(const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }
  const QString idString = index.data(Qt::UserRole).toString();
  const QUuid id(idString);
  const auto memos = model_.memos();
  const auto it = std::find_if(
      memos.cbegin(), memos.cend(),
      [&id](const ProjectMemo &memo) { return memo.id == id; });
  if (it == memos.cend()) {
    return;
  }
  widget->memoJumpRequested(it->frame);
}

ArtifactProjectMemoWidget::ArtifactProjectMemoWidget(QWidget *parent)
    : QWidget(parent), impl_(new Impl(this)) {}

ArtifactProjectMemoWidget::~ArtifactProjectMemoWidget() {
  delete impl_;
}

void ArtifactProjectMemoWidget::setCompositionId(const QString &compositionId) {
  impl_->model_.setCompositionId(compositionId);
  impl_->refreshList();
}

void ArtifactProjectMemoWidget::setCurrentFrame(qint64 frame) {
  impl_->currentFrame_ = frame;
}

} // namespace Artifact
