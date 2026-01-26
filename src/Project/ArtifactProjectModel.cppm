module;
#include <QDebug>
#include <QStandardItemModel>
module Artifact.Project.Model;

import std;
import Artifact.Project;
import Artifact.Service.Project;
import Artifact.Project.Manager;

namespace Artifact
{

 class ArtifactProjectModel::Impl
 {
 private:
  

 public:
  Impl();
  ~Impl();
  ArtifactProjectWeakPtr projectPtr_;
  QStandardItemModel* model_ = nullptr;
  QMetaObject::Connection compositionConnection_;
  void refreshTree();
  static ArtifactProjectService* projectService();
  Impl();
  ~Impl();
 };

ArtifactProjectModel::Impl::Impl()
{
 model_ = new QStandardItemModel();
}

ArtifactProjectModel::Impl::~Impl()
{
 if (compositionConnection_)
  QObject::disconnect(compositionConnection_);
 delete model_;
}

void ArtifactProjectModel::Impl::refreshTree()
 {
 if (!model_) return;
 // reset model so view updates cleanly
 this->model_->clear();

 auto shared = projectPtr_.lock();
 if (!shared) return;
 auto roots = shared->projectItems();

 qDebug() << "ArtifactProjectModel::refreshTree - roots count:" << roots.size();
 for (auto r : roots) {
  if (!r) continue;
  qDebug() << " root:" << r->name.toQString() << " children:" << r->children.size();
  for (auto c : r->children) {
    if (!c) continue;
    qDebug() << "  child:" << c->name.toQString() << " type:" << (int)c->type();
  }
 }

 std::function<QStandardItem*(ProjectItem*)> buildItem = [&](ProjectItem* it)->QStandardItem* {
  QString text = it->name.toQString();
  QStandardItem* item = new QStandardItem(text);
  // store pointer for later (optional)
  item->setData(QVariant::fromValue(reinterpret_cast<quintptr>(it)), Qt::UserRole+1);
  // children (non-owning raw pointers)
  for (auto childPtr : it->children) {
    QStandardItem* childItem = buildItem(childPtr);
    item->appendRow(childItem);
  }
  return item;
 };

 for (auto root : roots) {
  if (!root) continue;
  QStandardItem* rootItem = buildItem(root);
  model_->appendRow(rootItem);
 }
 }
// incremental handler: called when a composition is created globally
void ArtifactProjectModel::onCompositionCreated(const ArtifactCore::CompositionID& id)
{
 if (!impl_ || !impl_->model_) return;
 auto shared = impl_->projectPtr_.lock();
 if (!shared) return;

 // Find the CompositionItem with the given id and append it to model
 for (auto root : shared->projectItems()) {
  if (!root) continue;
  // if the root itself is the composition added
  if (root->type() == eProjectItemType::Composition) {
    CompositionItem* rci = static_cast<CompositionItem*>(root);
    if (rci->compositionId == id) {
      QString name = rci->name.toQString();
      QString idStr = id.toString();
      qDebug().noquote() << "Model: Detected new composition (root):" << name << "(ID:" << idStr << ")";
      QStandardItem* item = new QStandardItem(name);
      item->setData(QVariant::fromValue(reinterpret_cast<quintptr>(rci)), Qt::UserRole+1);
      impl_->model_->appendRow(item);
      return;
    }
  }

  // otherwise check children
  for (auto child : root->children) {
    if (!child) continue;
    if (child->type() == eProjectItemType::Composition) {
      CompositionItem* ci = static_cast<CompositionItem*>(child);
      if (ci->compositionId == id) {
        QString name = ci->name.toQString();
        QString idStr = id.toString();
        qDebug().noquote() << "Model: Detected new composition (child):" << name << "(ID:" << idStr << ")";
        QStandardItem* item = new QStandardItem(name);
        item->setData(QVariant::fromValue(reinterpret_cast<quintptr>(ci)), Qt::UserRole+1);
        // insert into the first root (simple behavior); better: find exact parent
        if (impl_->model_->rowCount() == 0) {
          impl_->model_->appendRow(item);
        } else {
          QStandardItem* firstRoot = impl_->model_->item(0);
          if (firstRoot) firstRoot->appendRow(item);
          else impl_->model_->appendRow(item);
        }
        return;
      }
    }
  }
 }

 // If we didn't find the item by ID, fall back to full refresh to ensure view consistency
 qDebug() << "Model: composition ID not found in project items, falling back to full refresh";
 impl_->refreshTree();
}

 ArtifactProjectService* ArtifactProjectModel::Impl::projectService()
 {

  return ArtifactProjectService::instance();
 }

// (Impl ctor/dtor implemented above)

ArtifactProjectModel::ArtifactProjectModel(QObject* parent/*=nullptr*/) :QAbstractItemModel(parent), impl_(new Impl())
{
  connect(impl_->projectService(), &ArtifactProjectService::layerCreated, this, [this]() {
    if (impl_->projectPtr_.lock()) {
      beginResetModel();
      impl_->refreshTree();
      endResetModel();
    }
  });
}

void ArtifactProjectModel::setProject(const std::shared_ptr<ArtifactProject>& project)
{
  if (!impl_) return;
  // disconnect previous connection
  if (impl_->compositionConnection_) QObject::disconnect(impl_->compositionConnection_);
  impl_->projectPtr_ = project;

  beginResetModel();
  impl_->refreshTree();
  endResetModel();

  if (auto shared = impl_->projectPtr_.lock()) {
    impl_->compositionConnection_ = connect(shared.get(), &ArtifactProject::compositionCreated, this, &ArtifactProjectModel::onCompositionCreated);
  }
}

 ArtifactProjectModel::~ArtifactProjectModel()
 {
  delete impl_;
 }

 QVariant ArtifactProjectModel::data(const QModelIndex& index, int role) const
 {
  if (!index.isValid())
   return QVariant();
  
  QStandardItem* item = nullptr;
  if (index.internalPointer()) item = static_cast<QStandardItem*>(index.internalPointer());
  if (!item) item = impl_->model_->itemFromIndex(impl_->model_->index(index.row(), index.column(), QModelIndex()));
  if (!item) return QVariant();
  if (!item) return QVariant();

  switch (role) {
  case Qt::DisplayRole: // 「画面に表示する文字は何？」
   return item->text();

  case Qt::ToolTipRole: // 「マウスホバーした時の説明は？」
   return QVariant();

  case Qt::DecorationRole: // 「アイコンは何にする？」
   return QVariant(); // QIconを返せる

  case Qt::ForegroundRole: // 「文字の色は何色？」
   return QVariant();
   break;

  case Qt::TextAlignmentRole: // 「文字の配置は？」
   return Qt::AlignCenter;
  }

  return QVariant();
 }

 int ArtifactProjectModel::rowCount(const QModelIndex& parent) const
 {
 if (!impl_->model_) return 0;
 if (!parent.isValid()) {
  return impl_->model_->rowCount();
 }
 // map from proxy index to source
 QModelIndex srcParent = impl_->model_->index(parent.row(), parent.column(), QModelIndex());
 return impl_->model_->rowCount(srcParent);
 }

 QModelIndex ArtifactProjectModel::index(int row, int column, const QModelIndex& parent) const
 {
 if (!impl_->model_) return QModelIndex();
 if (!parent.isValid()) {
  QStandardItem* it = impl_->model_->item(row);
  return createIndex(row, column, it);
 }
 QStandardItem* parentItem = impl_->model_->itemFromIndex(impl_->model_->index(parent.row(), parent.column()));
 if (!parentItem) return QModelIndex();
 QStandardItem* child = parentItem->child(row, column);
 if (!child) return QModelIndex();
 return createIndex(row, column, child);
 }


};