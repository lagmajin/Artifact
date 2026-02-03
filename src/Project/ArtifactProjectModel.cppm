module;
#include <QDebug>
#include <QStandardItemModel>
#include <QIcon>
#include <QPixmap>
#include <QColor>
module Artifact.Project.Model;

import std;
import Artifact.Project;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Project.Roles;

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
  // ensure two columns: Name and Size
  model_->setColumnCount(2);
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

 std::function<QList<QStandardItem*>(ProjectItem*)> buildItem = [&](ProjectItem* it)->QList<QStandardItem*> {
  QString text = it->name.toQString();
  QStandardItem* item = new QStandardItem(text);
  QStandardItem* sizeItem = new QStandardItem();
  // store item type using ProjectItemDataRole.ProjectItemType
  item->setData(static_cast<int>(it->type()), Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
  // If this is a composition item, set a simple solid-color square icon
  if (it->type() == eProjectItemType::Composition) {
    // create a small pixmap filled with a single color
    QPixmap px(16, 16);
    QColor col(120, 160, 200); // simple default tint; could be derived from id
    px.fill(col);
    item->setIcon(QIcon(px));
  }
  // store composition ID as string in UserRole+1 instead of raw pointer
  // store composition ID using ProjectItemDataRole enum to avoid magic numbers
  if (it->type() == eProjectItemType::Composition) {
    CompositionItem* comp = static_cast<CompositionItem*>(it);
    item->setData(comp->compositionId.toString(), Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
  } else {
    // clear/empty for non-composition items
    item->setData(QString(), Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
  }
  // set default size text for compositions; leave empty for folders
  if (it->type() == eProjectItemType::Composition) {
    // dummy fixed size for now
    sizeItem->setText("800x600");
  }

  // children (non-owning raw pointers)
  for (auto childPtr : it->children) {
    QList<QStandardItem*> childRow = buildItem(childPtr);
    item->appendRow(childRow);
  }

  return QList<QStandardItem*>() << item << sizeItem;
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
      QList<QStandardItem*> childRow = buildItem(childPtr);
      model_->appendRow(childRow);
    }
    continue;
  }
  QList<QStandardItem*> rootRow = buildItem(root);
  model_->appendRow(rootRow);
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
  // Also refresh when the project/service notifies of general changes
  connect(impl_->projectService(), &ArtifactProjectService::projectChanged, this, [this]() {
    if (impl_->projectPtr_.lock()) {
      beginResetModel();
      impl_->refreshTree();
      endResetModel();
    }
  });
  // Transfer ownership of the internal model to this QObject so Qt manages its lifetime
  if (impl_ && impl_->model_)
    impl_->model_->setParent(this);

  // Ensure the internal model provides horizontal header labels for columns
  if (impl_->model_) {
    impl_->model_->setHorizontalHeaderLabels(QStringList() << tr("Name") << tr("Size"));
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
  if (!index.isValid() || !impl_ || !impl_->model_)
   return QVariant();
  
  // 常に内部モデルから直接アイテムを取得（internalPointerはダングリングの可能性あり）
  QModelIndex srcIndex = mapToSource(index);
  QStandardItem* item = impl_->model_->itemFromIndex(srcIndex);
  if (!item) return QVariant();

  switch (role) {
  case Qt::DisplayRole: // 「画面に表示する文字は何？」
   return item->data(Qt::DisplayRole);

  case Qt::UserRole + 1: // CompositionID文字列
   return item->data(Qt::UserRole + 1);

  case Qt::ToolTipRole: // 「マウスホバーした時の説明は？」
   return QVariant();

  case Qt::DecorationRole: // 「アイコンは何にする？」
   return item->data(Qt::DecorationRole);

  case Qt::ForegroundRole: // 「文字の色は何色？」
   return QVariant();

  case Qt::TextAlignmentRole: // 「文字の配置は？」
   // Left align text and vertically center
   return QVariant(static_cast<int>(Qt::AlignVCenter | Qt::AlignLeft));

  default:
   return item->data(role);
  }

  return QVariant();
 }

 QModelIndex ArtifactProjectModel::mapToSource(const QModelIndex& proxyIndex) const
 {
  if (!proxyIndex.isValid() || !impl_ || !impl_->model_)
   return QModelIndex();

  // 親インデックスを再帰的にマップ
  QModelIndex parentProxy = proxyIndex.parent();
  if (!parentProxy.isValid()) {
   // トップレベルアイテム
   return impl_->model_->index(proxyIndex.row(), proxyIndex.column());
  }

  // 親のソースインデックスを取得
  QModelIndex parentSource = mapToSource(parentProxy);
  QStandardItem* parentItem = impl_->model_->itemFromIndex(parentSource);
  if (!parentItem) return QModelIndex();

  QStandardItem* childItem = parentItem->child(proxyIndex.row(), proxyIndex.column());
  if (!childItem) return QModelIndex();

  return impl_->model_->indexFromItem(childItem);
 }

 int ArtifactProjectModel::rowCount(const QModelIndex& parent) const
 {
 if (!impl_->model_) return 0;
 if (!parent.isValid()) {
  return impl_->model_->rowCount();
 }
 // map from proxy index to source
QModelIndex srcParent = impl_->model_->index(parent.row(), 0, QModelIndex());
return impl_->model_->rowCount(srcParent);
 }

int ArtifactProjectModel::columnCount(const QModelIndex& parent) const
{
  if (!impl_->model_) return 0;
  if (!parent.isValid()) {
    int c = impl_->model_->columnCount();
    return c > 0 ? c : 2; // ensure at least two columns for the view
  }
  QModelIndex srcParent = impl_->model_->index(parent.row(), 0, QModelIndex());
  int c = impl_->model_->columnCount(srcParent);
  return c > 0 ? c : 2;
}

QVariant ArtifactProjectModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (!impl_->model_) return QVariant();
  // Prefer explicit label for first horizontal header
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    QVariant v = impl_->model_->headerData(section, orientation, role);
    // if model supplied a usable string, return it
    if (v.isValid() && v.canConvert<QString>()) {
      QString s = v.toString();
      if (!s.isEmpty() && s != "1" && s != "2") return v;
    }
    // fallback to explicit known labels per column
    if (section == 0) return tr("Name");
    if (section == 1) return tr("Size");
    return QVariant();
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