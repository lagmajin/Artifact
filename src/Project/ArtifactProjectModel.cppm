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
 // create the internal model with no parent for now; ownership will be transferred
 model_ = new QStandardItemModel();
 // ensure at least one column so header logic has a stable column count
 model_->setColumnCount(1);
}

ArtifactProjectModel::Impl::~Impl()
{
 if (compositionConnection_)
  QObject::disconnect(compositionConnection_);
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

 // Treat the first element in the project's root list as the project-root placeholder
 // and do not display it as a top-level item. Append its children as top-level rows
 // instead. This avoids relying on the root's name for identification.
 ProjectItem* projectPlaceholder = nullptr;
 if (!roots.isEmpty())
     projectPlaceholder = roots.at(0);

 for (auto root : roots) {
  if (!root) continue;
  if (root == projectPlaceholder) {
    // append children (if any) as top-level rows and skip this placeholder
    for (auto childPtr : root->children) {
      if (!childPtr) continue;
      QStandardItem* childItem = buildItem(childPtr);
      model_->appendRow(childItem);
    }
    continue;
  }
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

 // For robustness, rebuild the model when compositions are created. Although more
 // costly than incremental insert, this avoids subtle index-mapping bugs between
 // the internal QStandardItemModel and this proxy QAbstractItemModel.
 beginResetModel();
 impl_->refreshTree();
 endResetModel();
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
  // Transfer ownership of the internal model to this QObject so Qt manages its lifetime
  if (impl_ && impl_->model_)
    impl_->model_->setParent(this);

  // Ensure the internal model provides a horizontal header label for the first column
  if (impl_->model_) {
    impl_->model_->setHorizontalHeaderLabels(QStringList() << tr("Name"));
  }
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

int ArtifactProjectModel::columnCount(const QModelIndex& parent) const
{
  if (!impl_->model_) return 0;
  if (!parent.isValid()) {
    int c = impl_->model_->columnCount();
    return c > 0 ? c : 1; // ensure at least one column for the view
  }
  QModelIndex srcParent = impl_->model_->index(parent.row(), parent.column(), QModelIndex());
  int c = impl_->model_->columnCount(srcParent);
  return c > 0 ? c : 1;
}

QVariant ArtifactProjectModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (!impl_->model_) return QVariant();
  // Prefer explicit label for first horizontal header
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section == 0) {
    QVariant v = impl_->model_->headerData(section, orientation, role);
    if (!v.isValid() || v.toString().isEmpty() || v.toString() == "1")
      return tr("Name");
    return v;
  }
  return impl_->model_->headerData(section, orientation, role);
}

QModelIndex ArtifactProjectModel::parent(const QModelIndex& index) const
{
  if (!index.isValid() || !impl_->model_) return QModelIndex();

  QStandardItem* item = nullptr;
  if (index.internalPointer()) item = static_cast<QStandardItem*>(index.internalPointer());
  if (!item) {
    // Fallback: try to get item from internal model using the index
    QModelIndex src = impl_->model_->index(index.row(), index.column(), QModelIndex());
    item = impl_->model_->itemFromIndex(src);
    if (!item) return QModelIndex();
  }

  QStandardItem* parentItem = item->parent();
  if (!parentItem) return QModelIndex();

  return createIndex(parentItem->row(), parentItem->column(), parentItem);
}

Qt::ItemFlags ArtifactProjectModel::flags(const QModelIndex &index) const
{
  if (!index.isValid()) return Qt::NoItemFlags;
  // Basic selectable/enabled flags; expand as needed
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
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