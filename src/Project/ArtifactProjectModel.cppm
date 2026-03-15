module;
#include <QDebug>
#include <QStandardItemModel>
#include <QIcon>
#include <QPixmap>
#include <QColor>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QPainter>
#include <QtSVG/QSvgRenderer>
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
module Artifact.Project.Model;




import Artifact.Project;
import Artifact.Project.Items;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Project.Roles;
import Utils.Path;

namespace Artifact
{

namespace {

QIcon makeProjectItemIcon(const QColor& fill, const QString& text = {})
{
  QPixmap px(16, 16);
  px.fill(Qt::transparent);
  QPainter painter(&px);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(QColor(24, 24, 24), 1.0));
  painter.setBrush(fill);
  painter.drawRoundedRect(QRectF(0.5, 0.5, 15.0, 15.0), 3.0, 3.0);
  if (!text.isEmpty()) {
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(8);
    painter.setFont(font);
    painter.setPen(QColor(245, 245, 245));
    painter.drawText(QRectF(0, 0, 16, 16), Qt::AlignCenter, text.left(1).toUpper());
  }
  return QIcon(px);
}

QIcon loadProjectIcon(const QString& relativePath)
{
  const QString path = ArtifactCore::resolveIconPath(relativePath);
  if (path.isEmpty()) {
    return QIcon();
  }

  // For SVG files, use QSvgRenderer directly to avoid dependency on the
  // qsvgicon icon-engine plugin which may not be on the library search path.
  if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
    QSvgRenderer renderer(path);
    if (renderer.isValid()) {
      QPixmap pixmap(16, 16);
      pixmap.fill(Qt::transparent);
      QPainter painter(&pixmap);
      renderer.render(&painter);
      painter.end();
      if (!pixmap.isNull()) {
        return QIcon(pixmap);
      }
    }
    return QIcon();
  }

  QIcon icon(path);
  if (icon.isNull() || icon.pixmap(16, 16).isNull()) {
    return QIcon();
  }
  return icon;
}

QStandardItem* projectItemFromModelIndex(const QModelIndex& index)
{
  if (!index.isValid()) {
    return nullptr;
  }
  return static_cast<QStandardItem*>(index.internalPointer());
}

} // namespace

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
  // six columns: Name, Size, Duration, Frame Rate, Updated, ID
  model_->setColumnCount(6);
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
 this->model_->setColumnCount(6);
 this->model_->setHorizontalHeaderLabels(QStringList()
  << QObject::tr("Name")
  << QObject::tr("Size")
  << QObject::tr("Duration")
  << QObject::tr("Frame Rate")
  << QObject::tr("Updated")
  << QObject::tr("ID"));

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

 auto iconForProjectItem = [](auto* it) -> QIcon {
  auto iconOrFallback = [](const QString& relativePath, const QColor& fallbackColor, const QString& fallbackText) -> QIcon {
   QIcon icon = loadProjectIcon(relativePath);
   if (!icon.isNull()) {
    return icon;
   }
   return makeProjectItemIcon(fallbackColor, fallbackText);
  };

  if (!it) {
   return makeProjectItemIcon(QColor(90, 90, 90), QStringLiteral("?"));
  }
  switch (it->type()) {
  case Artifact::eProjectItemType::Folder:
   return iconOrFallback(QStringLiteral("MaterialVS/yellow/folder.svg"), QColor(176, 138, 46), QStringLiteral("F"));
  case Artifact::eProjectItemType::Composition:
   return iconOrFallback(QStringLiteral("MaterialVS/blue/movie.svg"), QColor(74, 128, 191), QStringLiteral("C"));
  case Artifact::eProjectItemType::Solid:
   return iconOrFallback(QStringLiteral("MaterialVS/purple/format_shapes.svg"), QColor(110, 88, 170), QStringLiteral("S"));
  case Artifact::eProjectItemType::Footage: {
   auto* footage = static_cast<Artifact::FootageItem*>(it);
   const QFileInfo info(footage->filePath);
   if (!info.exists()) {
    return iconOrFallback(QStringLiteral("MaterialVS/red/error.svg"), QColor(140, 54, 54), QStringLiteral("!"));
   }
   const QString suffix = info.suffix().toLower();
   if (QStringList{QStringLiteral("ttf"), QStringLiteral("otf"), QStringLiteral("ttc"), QStringLiteral("woff"), QStringLiteral("woff2")}.contains(suffix)) {
    return iconOrFallback(QStringLiteral("MaterialVS/purple/title.svg"), QColor(121, 82, 168), QStringLiteral("T"));
   }
   if (QStringList{QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp"),
                   QStringLiteral("gif"), QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("webp"),
                   QStringLiteral("exr")}.contains(suffix)) {
    return iconOrFallback(QStringLiteral("MaterialVS/green/photo_library.svg"), QColor(66, 148, 98), QStringLiteral("I"));
   }
   if (QStringList{QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("avi"), QStringLiteral("mkv"),
                   QStringLiteral("webm")}.contains(suffix)) {
    return iconOrFallback(QStringLiteral("MaterialVS/blue/video_library.svg"), QColor(170, 90, 48), QStringLiteral("V"));
   }
   if (QStringList{QStringLiteral("wav"), QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("ogg"),
                   QStringLiteral("m4a"), QStringLiteral("aac")}.contains(suffix)) {
    return iconOrFallback(QStringLiteral("MaterialVS/orange/audiotrack.svg"), QColor(52, 120, 148), QStringLiteral("A"));
   }
   return iconOrFallback(QStringLiteral("MaterialVS/neutral/inventory.svg"), QColor(96, 96, 96), QStringLiteral("F"));
  }
  default:
   return iconOrFallback(QStringLiteral("MaterialVS/neutral/help.svg"), QColor(90, 90, 90), QStringLiteral("?"));
  }
 };

 std::function<QList<QStandardItem*>(Artifact::ProjectItem*)> buildItem =
   [&](Artifact::ProjectItem* it) -> QList<QStandardItem*> {
  QString text = it->name.toQString();
  QStandardItem* item = new QStandardItem(text);
  QStandardItem* sizeItem = new QStandardItem();
  QStandardItem* durationItem = new QStandardItem();
  QStandardItem* frameRateItem = new QStandardItem();
  QStandardItem* updatedItem = new QStandardItem();
  QStandardItem* idItem = new QStandardItem();

  // store item type using ProjectItemDataRole.ProjectItemType
  item->setData(static_cast<int>(it->type()), Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
  // store item raw pointer for quick access from view/menus
  item->setData(QVariant::fromValue(reinterpret_cast<quintptr>(it)), Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));

  item->setIcon(iconForProjectItem(it));

  // store composition ID as string in UserRole+1 instead of raw pointer
  // store composition ID using ProjectItemDataRole enum to avoid magic numbers
  if (it->type() == Artifact::eProjectItemType::Composition) {
    Artifact::CompositionItem* comp = static_cast<Artifact::CompositionItem*>(it);
    item->setData(comp->compositionId.toString(), Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));

    // Display ID in the ID column
    idItem->setText(comp->compositionId.toString());

    // Composition情報を取得
    auto service = projectService();
    if (service) {
      auto findResult = service->findComposition(comp->compositionId);
      if (findResult.success && findResult.ptr.lock()) {
        auto composition = findResult.ptr.lock();

        const QSize compSize = composition->settings().compositionSize();
        if (compSize.width() > 0 && compSize.height() > 0) {
          sizeItem->setText(QStringLiteral("%1x%2").arg(compSize.width()).arg(compSize.height()));
        } else {
          sizeItem->setText(QStringLiteral("-"));
        }

        const auto frameRange = composition->frameRange();
        const auto fps = composition->frameRate().framerate();
        const int64_t totalFrames = std::max<int64_t>(0, frameRange.end() - frameRange.start());
        durationItem->setText(QStringLiteral("%1 frames").arg(totalFrames));
        frameRateItem->setText(QStringLiteral("%1 fps").arg(QString::number(fps, 'f', fps == std::floor(fps) ? 0 : 3)));

        qDebug() << "[ProjectModel] Added composition metadata - ID:" << idItem->text()
                 << "Size:" << sizeItem->text() 
                 << "Duration:" << durationItem->text() << "FPS:" << frameRateItem->text();
      }
    }
  } else {
    // clear/empty for non-composition items
    item->setData(QString(), Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
  }

  if (it->type() == Artifact::eProjectItemType::Footage) {
    auto* footage = static_cast<Artifact::FootageItem*>(it);
    const QByteArray digest = QCryptographicHash::hash(footage->filePath.toUtf8(), QCryptographicHash::Sha1).toHex();
    item->setData(QString::fromUtf8(digest.left(16)), Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::AssetId));
    idItem->setText(QString::fromUtf8(digest.left(16)));
    const QFileInfo fi(footage->filePath);
    if (fi.exists()) {
      sizeItem->setText(QString::number(fi.size() / 1024) + " KB");
      durationItem->setText(fi.suffix().toUpper());
      const QString suffix = fi.suffix().toLower();
      if (QStringList{QStringLiteral("ttf"), QStringLiteral("otf"), QStringLiteral("ttc"), QStringLiteral("woff"), QStringLiteral("woff2")}.contains(suffix)) {
        frameRateItem->setText(QStringLiteral("Font"));
      } else if (QStringList{QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp"),
                      QStringLiteral("gif"), QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("webp"),
                      QStringLiteral("exr")}.contains(suffix)) {
        frameRateItem->setText(QStringLiteral("Image"));
      } else if (QStringList{QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("avi"), QStringLiteral("mkv"),
                             QStringLiteral("webm")}.contains(suffix)) {
        frameRateItem->setText(QStringLiteral("Video"));
      } else if (QStringList{QStringLiteral("wav"), QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("ogg"),
                             QStringLiteral("m4a"), QStringLiteral("aac")}.contains(suffix)) {
        frameRateItem->setText(QStringLiteral("Audio"));
      } else {
        frameRateItem->setText(QStringLiteral("Footage"));
      }
      updatedItem->setText(fi.lastModified().toString("yyyy-MM-dd HH:mm"));
    } else {
      sizeItem->setText("-");
      durationItem->setText(QStringLiteral("Missing"));
      frameRateItem->setText(QStringLiteral("-"));
      updatedItem->setText("Missing");
    }
  } else {
    item->setData(QString(), Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::AssetId));
    updatedItem->setText("-");
  }

  // children (non-owning raw pointers)
  for (auto childPtr : it->children) {
    QList<QStandardItem*> childRow = buildItem(childPtr);
    item->appendRow(childRow);
  }

  return QList<QStandardItem*>() << item << sizeItem << durationItem << frameRateItem << updatedItem << idItem;
 };

 // Treat only an explicit "Project Root" folder as a hidden placeholder.
 // Older/broken project data may have non-placeholder items at index 0.
 Artifact::ProjectItem* projectPlaceholder = nullptr;
 if (!roots.isEmpty()) {
     Artifact::ProjectItem* first = roots.at(0);
     if (first &&
         first->type() == Artifact::eProjectItemType::Folder &&
         first->name.toQString() == QStringLiteral("Project Root")) {
         projectPlaceholder = first;
     }
 }

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
    impl_->model_->setHorizontalHeaderLabels(QStringList() << tr("Name") << tr("Size") << tr("Duration") << tr("Frame Rate") << tr("Updated") << tr("ID"));
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

  QStandardItem* item = projectItemFromModelIndex(index);
  if (!item) {
   const QModelIndex srcIndex = mapToSource(index);
   item = impl_->model_->itemFromIndex(srcIndex);
  }
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

  if (auto* item = projectItemFromModelIndex(proxyIndex)) {
   return impl_->model_->indexFromItem(item);
  }

  const QModelIndex parentSource = mapToSource(proxyIndex.parent());
  return impl_->model_->index(proxyIndex.row(), proxyIndex.column(), parentSource);
 }

 int ArtifactProjectModel::rowCount(const QModelIndex& parent) const
 {
  if (!impl_ || !impl_->model_) {
   return 0;
  }
  if (parent.isValid() && parent.column() != 0) {
   return 0;
  }
  const QModelIndex srcParent = mapToSource(parent);
  return impl_->model_->rowCount(srcParent);
 }

int ArtifactProjectModel::columnCount(const QModelIndex& parent) const
{
  if (!impl_ || !impl_->model_) return 6;
  const QModelIndex srcParent = mapToSource(parent);
  const int c = impl_->model_->columnCount(srcParent);
  return c > 0 ? c : 6;
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
    if (section == 2) return tr("Duration");
    if (section == 3) return tr("Frame Rate");
    if (section == 4) return tr("Updated");
    if (section == 5) return tr("ID");
    return QVariant();
  }
  return impl_->model_->headerData(section, orientation, role);
}

QModelIndex ArtifactProjectModel::parent(const QModelIndex& index) const
{
  if (!index.isValid() || !impl_->model_) return QModelIndex();

  const QModelIndex srcIndex = mapToSource(index);
  if (!srcIndex.isValid()) {
   return QModelIndex();
  }

  const QModelIndex srcParent = srcIndex.parent();
  if (!srcParent.isValid()) {
   return QModelIndex();
  }

  auto* parentItem = impl_->model_->itemFromIndex(srcParent);
  if (!parentItem) {
   return QModelIndex();
  }
  return createIndex(srcParent.row(), srcParent.column(), parentItem);
}

Qt::ItemFlags ArtifactProjectModel::flags(const QModelIndex &index) const
{
  if (!index.isValid()) {
    return Qt::ItemIsDropEnabled;
  }

  Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
  const QVariant typeVar = data(
    index.siblingAtColumn(0),
    Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
  if (typeVar.isValid() && typeVar.toInt() == static_cast<int>(eProjectItemType::Folder)) {
    flags |= Qt::ItemIsDropEnabled;
  }
  return flags;
}

Qt::DropActions ArtifactProjectModel::supportedDropActions() const
{
  return Qt::MoveAction;
}

 QModelIndex ArtifactProjectModel::index(int row, int column, const QModelIndex& parent) const
 {
  if (!impl_ || !impl_->model_ || row < 0 || column < 0) {
   return QModelIndex();
  }
  const QModelIndex srcParent = mapToSource(parent);
  const QModelIndex srcIndex = impl_->model_->index(row, column, srcParent);
  if (!srcIndex.isValid()) {
   return QModelIndex();
  }
  auto* item = impl_->model_->itemFromIndex(srcIndex);
  if (!item) {
   return QModelIndex();
  }
  return createIndex(row, column, item);
 }


};
