module;
#include <utility>
#include <QFileSystemModel>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWidget>
#include <QListView>
#include <QListWidget>
#include <QLayoutItem>
#include <QToolButton>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QDrag>
#include <QButtonGroup>
#include <QPixmap>
#include <QIcon>
#include <QHash>
#include <QFileInfo>
#include <QStyle>
#include <QApplication>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>
#include <QApplication>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <opencv2/opencv.hpp>
#include <QSlider>
#include <QGroupBox>
#include <QGridLayout>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QAbstractItemView>
#include <wobjectimpl.h>

module Widgets.AssetBrowser;

import Widgets.Utils.CSS;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Project.Cleanup;
import AssetMenuModel;
import AssetDirectoryModel;
import Utils.String.UniString;
import File.TypeDetector;

namespace Artifact {

using namespace ArtifactCore;

namespace {
QString normalizeAssetPath(const QString& path)
{
  if (path.trimmed().isEmpty()) {
   return {};
  }
  const QFileInfo info(path);
  const QString canonical = info.canonicalFilePath();
  return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}
}

class ArtifactBreadcrumbWidget::Impl
{
public:
 Impl() = default;
 QWidget* owner_ = nullptr;
 QHBoxLayout* layout_ = nullptr;
 QString rootPath_;
 QString currentPath_;

 void rebuild();
};

W_OBJECT_IMPL(ArtifactBreadcrumbWidget)

void ArtifactBreadcrumbWidget::Impl::rebuild()
{
 if (!owner_ || !layout_) {
  return;
 }
 while (QLayoutItem* item = layout_->takeAt(0)) {
  if (auto* widget = item->widget()) {
   widget->deleteLater();
  }
  delete item;
 }
 const QString current = currentPath_.isEmpty() ? rootPath_ : currentPath_;
 QStringList parts = current.split(QDir::separator(), Qt::SkipEmptyParts);
 QString path;
 if (!rootPath_.isEmpty()) {
  const QString root = QDir::cleanPath(rootPath_);
  auto* rootButton = new QToolButton(owner_);
  rootButton->setText(QFileInfo(root).fileName().isEmpty() ? root : QFileInfo(root).fileName());
  QObject::connect(rootButton, &QToolButton::clicked, owner_, [this, root]() {
   Q_EMIT static_cast<ArtifactBreadcrumbWidget*>(owner_)->pathClicked(root);
  });
  layout_->addWidget(rootButton);
  path = root;
 }
 for (const QString& part : parts) {
  if (!path.isEmpty()) {
   path += QDir::separator();
  }
  path += part;
  auto* button = new QToolButton(owner_);
  button->setText(part);
  QObject::connect(button, &QToolButton::clicked, owner_, [this, path]() {
   Q_EMIT static_cast<ArtifactBreadcrumbWidget*>(owner_)->pathClicked(path);
  });
  layout_->addWidget(button);
 }
 layout_->addStretch(1);
}

ArtifactBreadcrumbWidget::ArtifactBreadcrumbWidget(QWidget* parent)
  : QFrame(parent), impl_(new Impl())
{
 impl_->owner_ = this;
 impl_->layout_ = new QHBoxLayout(this);
 impl_->layout_->setContentsMargins(0, 0, 0, 0);
 impl_->layout_->setSpacing(4);
 setFrameShape(QFrame::NoFrame);
}

ArtifactBreadcrumbWidget::~ArtifactBreadcrumbWidget()
{
 delete impl_;
}

void ArtifactBreadcrumbWidget::setRootPath(const QString& rootPath)
{
 if (!impl_) {
  return;
 }
 impl_->rootPath_ = normalizeAssetPath(rootPath);
 impl_->rebuild();
}

void ArtifactBreadcrumbWidget::setPath(const QString& path)
{
 if (!impl_) {
  return;
 }
 impl_->currentPath_ = normalizeAssetPath(path);
 impl_->rebuild();
}

 class AssetFileListView final : public QListView
 {
 public:
  explicit AssetFileListView(QWidget* parent = nullptr) : QListView(parent) {}

 protected:
  void startDrag(Qt::DropActions supportedActions) override
  {
   const QModelIndexList indexes = selectedIndexes();
   if (indexes.isEmpty() || !model()) {
    return;
   }

   QElapsedTimer dragTimer;
   dragTimer.start();

   std::unique_ptr<QMimeData> mimeData(model()->mimeData(indexes));
   if (!mimeData) {
    return;
   }

   auto* drag = new QDrag(this);
   drag->setMimeData(mimeData.release());

   QPixmap dragPixmap(160, 28);
   dragPixmap.fill(QColor(32, 32, 32, 220));
   {
    QPainter painter(&dragPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor(255, 255, 255, 235));
    painter.drawRoundedRect(dragPixmap.rect().adjusted(0, 0, -1, -1), 6, 6);
    const QString label = indexes.size() == 1
     ? model()->data(indexes.first(), Qt::DisplayRole).toString()
     : QStringLiteral("%1 items").arg(indexes.size());
    painter.drawText(dragPixmap.rect().adjusted(10, 0, -10, 0),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     QFontMetrics(font()).elidedText(label, Qt::ElideRight, 140));
   }
   drag->setPixmap(dragPixmap);
   drag->setHotSpot(QPoint(12, dragPixmap.height() / 2));

   qDebug() << "[AssetBrowser][Drag]" << "mimeMs=" << dragTimer.elapsed()
            << "items=" << indexes.size();
   drag->exec(supportedActions, Qt::CopyAction);
  }
 };



 class ArtifactAssetBrowserToolBar::Impl
 {
 private:
 public:
  Impl();
  ~Impl();
  QLineEdit* searchWidget = nullptr;
 };

 ArtifactAssetBrowserToolBar::Impl::Impl()
 {
  searchWidget = new QLineEdit();
 }

 ArtifactAssetBrowserToolBar::Impl::~Impl()
 {
 }

 W_OBJECT_IMPL(ArtifactAssetBrowserToolBar)

 ArtifactAssetBrowserToolBar::ArtifactAssetBrowserToolBar(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl())
 {
  auto layout = new QHBoxLayout();
  auto upButton = new QToolButton(this);
  upButton->setObjectName(QStringLiteral("assetBrowserUpButton"));
  upButton->setText(QStringLiteral("Up"));
  upButton->setToolTip(QStringLiteral("Go to parent folder"));
  auto refreshButton = new QToolButton(this);
  refreshButton->setObjectName(QStringLiteral("assetBrowserRefreshButton"));
  refreshButton->setText(QStringLiteral("Refresh"));
  refreshButton->setToolTip(QStringLiteral("Refresh current folder"));
  impl_->searchWidget->setPlaceholderText(QStringLiteral("Search assets..."));
  impl_->searchWidget->setClearButtonEnabled(true);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(upButton);
  layout->addWidget(refreshButton);
  layout->addWidget(impl_->searchWidget);
  setLayout(layout);
 }

 ArtifactAssetBrowserToolBar::~ArtifactAssetBrowserToolBar()
 {
  delete impl_;
 }

 class ArtifactAssetBrowser::Impl
 {
 private:
  QHash<QString, QIcon> thumbnailCache_;  // Cache thumbnails by file path
  QSize thumbnailSize_{64, 64};
  QIcon defaultFileIcon_;
  QIcon defaultImageIcon_;
  QIcon defaultVideoIcon_;
  QIcon defaultAudioIcon_;
  QIcon defaultFontIcon_;
  QSet<QString> unusedAssetPaths_;
 public:
  Impl();
  ~Impl();
  QToolButton* upButton_ = nullptr;
  QToolButton* refreshButton_ = nullptr;
  QTreeView* directoryView_ = nullptr;
  AssetDirectoryModel* directoryModel_ = nullptr;
  QListView* fileView_ = nullptr;
  AssetMenuModel* assetModel_ = nullptr;
  QLineEdit* searchEdit_ = nullptr;
  QFileSystemModel* fileModel_ = nullptr;
  QButtonGroup* filterButtonGroup_ = nullptr;
  QLabel* currentPathLabel_ = nullptr;
  QLabel* fileInfoLabel_ = nullptr;  // File details display
  QSlider* thumbnailSizeSlider_ = nullptr;  // Thumbnail size adjustment
   QString currentDirectoryPath_;
   QString currentFileTypeFilter_ = "all";
   QString currentStatusFilter_ = "all";
   QString currentSearchFilter_;

  void handleDirectryChanged();
  void handleDoubleClicked();
  void defaultHandleMousePressEvent(QMouseEvent* event);
  void applyFilters();
  bool matchesFileTypeFilter(const QString& fileName) const;
  bool matchesSearchFilter(const QString& fileName) const;
  QIcon generateThumbnail(const QString& filePath);
  QIcon getFileIcon(const QString& fileName, const QString& filePath);
  void clearThumbnailCache();
   bool isImageFile(const QString& fileName) const;
   bool isVideoFile(const QString& fileName) const;
   bool isAudioFile(const QString& fileName) const;
   bool isFontFile(const QString& fileName) const;
   ArtifactCore::FileType fileType(const QString& fileName) const;
  bool isImportedAssetPath(const QString& filePath) const;
  bool isUnusedAssetPath(const QString& filePath) const;
  bool isMissingAssetPath(const QString& filePath) const;
  QStringList selectedAssetPaths() const;
  void syncProjectAssetRoot();
  void syncDirectorySelection();
  void refreshUnusedAssetCache();
 };

 ArtifactAssetBrowser::Impl::Impl()
 {
  // Initialize default icons using Qt standard icons
  QStyle* style = QApplication::style();
  if (style) {
   defaultFileIcon_ = style->standardIcon(QStyle::SP_FileIcon);
   defaultImageIcon_ = style->standardIcon(QStyle::SP_FileIcon);
   defaultVideoIcon_ = style->standardIcon(QStyle::SP_MediaPlay);
   defaultAudioIcon_ = style->standardIcon(QStyle::SP_MediaVolume);
   defaultFontIcon_ = style->standardIcon(QStyle::SP_FileDialogDetailedView);
  }
 }

 ArtifactAssetBrowser::Impl::~Impl()
 {
 }

 void ArtifactAssetBrowser::Impl::handleDoubleClicked()
 {
 }

 void ArtifactAssetBrowser::Impl::defaultHandleMousePressEvent(QMouseEvent* event)
 {
 }

 bool ArtifactAssetBrowser::Impl::matchesFileTypeFilter(const QString& fileName) const
 {
  if (currentFileTypeFilter_ == "all") return true;

  QString lower = fileName.toLower();

  if (currentFileTypeFilter_ == "images") {
   return lower.endsWith(".png") || lower.endsWith(".jpg") ||
          lower.endsWith(".jpeg") || lower.endsWith(".bmp") ||
          lower.endsWith(".gif") || lower.endsWith(".tga") ||
          lower.endsWith(".tiff") || lower.endsWith(".exr");
  }
  else if (currentFileTypeFilter_ == "videos") {
   return lower.endsWith(".mp4") || lower.endsWith(".mov") ||
          lower.endsWith(".avi") || lower.endsWith(".mkv") ||
          lower.endsWith(".webm") || lower.endsWith(".flv");
  }
  else if (currentFileTypeFilter_ == "audio") {
   return lower.endsWith(".mp3") || lower.endsWith(".wav") ||
          lower.endsWith(".ogg") || lower.endsWith(".flac") ||
          lower.endsWith(".aac") || lower.endsWith(".m4a");
  }
  else if (currentFileTypeFilter_ == "fonts") {
   return lower.endsWith(".ttf") || lower.endsWith(".otf") ||
          lower.endsWith(".ttc") || lower.endsWith(".woff") ||
          lower.endsWith(".woff2");
  }

  return true;
 }

 bool ArtifactAssetBrowser::Impl::matchesSearchFilter(const QString& fileName) const
 {
  if (currentSearchFilter_.isEmpty()) return true;
  return fileName.contains(currentSearchFilter_, Qt::CaseInsensitive);
 }

 FileType ArtifactAssetBrowser::Impl::fileType(const QString& fileName) const
 {
  static FileTypeDetector detector;
  return detector.detectByExtension(fileName);
 }

 bool ArtifactAssetBrowser::Impl::isImageFile(const QString& fileName) const
 {
  return fileType(fileName) == ArtifactCore::FileType::Image;
 }

 bool ArtifactAssetBrowser::Impl::isVideoFile(const QString& fileName) const
 {
  return fileType(fileName) == ArtifactCore::FileType::Video;
 }

 bool ArtifactAssetBrowser::Impl::isAudioFile(const QString& fileName) const
 {
  return fileType(fileName) == ArtifactCore::FileType::Audio;
 }

 bool ArtifactAssetBrowser::Impl::isFontFile(const QString& fileName) const
 {
  QString lower = fileName.toLower();
  return lower.endsWith(".ttf") || lower.endsWith(".otf") ||
         lower.endsWith(".ttc") || lower.endsWith(".woff") ||
         lower.endsWith(".woff2");
 }

 bool ArtifactAssetBrowser::Impl::isImportedAssetPath(const QString& filePath) const
{
  if (filePath.isEmpty()) {
   return false;
  }

  auto* svc = ArtifactProjectService::instance();
  if (!svc) {
   return false;
  }

  const QString canonicalTarget = QFileInfo(filePath).canonicalFilePath().isEmpty()
    ? QFileInfo(filePath).absoluteFilePath()
    : QFileInfo(filePath).canonicalFilePath();

  std::function<bool(ProjectItem*)> containsPath = [&](ProjectItem* item) -> bool {
   if (!item) {
    return false;
   }
   if (item->type() == eProjectItemType::Footage) {
    const QString candidatePath = static_cast<FootageItem*>(item)->filePath;
    const QString canonicalCandidate = QFileInfo(candidatePath).canonicalFilePath().isEmpty()
      ? QFileInfo(candidatePath).absoluteFilePath()
      : QFileInfo(candidatePath).canonicalFilePath();
    if (QDir::cleanPath(canonicalCandidate) == QDir::cleanPath(canonicalTarget)) {
     return true;
    }
   }
   for (auto* child : item->children) {
    if (containsPath(child)) {
     return true;
    }
   }
   return false;
  };

  const auto roots = svc->projectItems();
  for (auto* root : roots) {
   if (containsPath(root)) {
    return true;
   }
  }
  return false;
}

QStringList ArtifactAssetBrowser::Impl::selectedAssetPaths() const
{
  QStringList paths;
  if (!fileView_ || !assetModel_ || !fileView_->selectionModel()) {
   return paths;
  }

  const QModelIndexList selectedIndexes = fileView_->selectionModel()->selectedIndexes();
  paths.reserve(selectedIndexes.size());
  for (const QModelIndex& index : selectedIndexes) {
   const AssetMenuItem item = assetModel_->itemAt(index.row());
   if (!item.isFolder) {
    const QString path = item.path.toQString();
    if (!path.isEmpty()) {
     paths.append(path);
    }
   }
  }
  paths.removeDuplicates();
  return paths;
}

bool ArtifactAssetBrowser::Impl::isUnusedAssetPath(const QString& filePath) const
{
  const QString canonicalPath = QFileInfo(filePath).canonicalFilePath().isEmpty()
    ? QFileInfo(filePath).absoluteFilePath()
    : QFileInfo(filePath).canonicalFilePath();
  return unusedAssetPaths_.contains(QDir::cleanPath(canonicalPath))
    || unusedAssetPaths_.contains(QDir::cleanPath(filePath));
}

bool ArtifactAssetBrowser::Impl::isMissingAssetPath(const QString& filePath) const
{
  if (filePath.isEmpty()) {
   return false;
  }
  return !QFileInfo::exists(filePath);
}

 QIcon ArtifactAssetBrowser::Impl::generateThumbnail(const QString& filePath)
 {
  // Check cache first
  if (thumbnailCache_.contains(filePath)) {
   return thumbnailCache_[filePath];
  }

  QFileInfo fileInfo(filePath);

  // For folders, use folder icon
  if (fileInfo.isDir()) {
   QStyle* style = QApplication::style();
   if (style) {
    QIcon folderIcon = style->standardIcon(QStyle::SP_DirIcon);
    thumbnailCache_[filePath] = folderIcon;
    return folderIcon;
   }
   return defaultFileIcon_;
  }

  // Generate thumbnail for image files
  if (isImageFile(fileInfo.fileName())) {
   QPixmap pixmap(filePath);
   if (pixmap.isNull()) {
    qWarning() << "[AssetBrowser] Failed to load image thumbnail via QPixmap:" << filePath;
   } else {
    QPixmap scaled = pixmap.scaled(thumbnailSize_, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QIcon icon(scaled);
    thumbnailCache_[filePath] = icon;
    return icon;
   }
  }

  // Extract first frame as thumbnail for video files
  if (isVideoFile(fileInfo.fileName())) {
      cv::VideoCapture cap(filePath.toLocal8Bit().constData());
      if (cap.isOpened()) {
          cv::Mat frame;
          if (cap.read(frame) && !frame.empty()) {
              cv::Mat rgb;
              cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
              QImage qimg(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
              QPixmap pixmap = QPixmap::fromImage(qimg).scaled(thumbnailSize_, Qt::KeepAspectRatio, Qt::SmoothTransformation);
              QIcon icon(pixmap);
              thumbnailCache_[filePath] = icon;
              return icon;
          }
      }
      // Fallback
      thumbnailCache_[filePath] = defaultVideoIcon_;
      return defaultVideoIcon_;
  }

  // For audio files, use a default audio icon
  if (isAudioFile(fileInfo.fileName())) {
   thumbnailCache_[filePath] = defaultAudioIcon_;
   return defaultAudioIcon_;
  }

  if (isFontFile(fileInfo.fileName())) {
   thumbnailCache_[filePath] = defaultFontIcon_;
   return defaultFontIcon_;
  }

  // Default file icon
  thumbnailCache_[filePath] = defaultFileIcon_;
  return defaultFileIcon_;
 }

 QIcon ArtifactAssetBrowser::Impl::getFileIcon(const QString& fileName, const QString& filePath)
 {
  return generateThumbnail(filePath);
 }

 void ArtifactAssetBrowser::Impl::clearThumbnailCache()
 {
  thumbnailCache_.clear();
 }

void ArtifactAssetBrowser::Impl::syncProjectAssetRoot()
{
  if (!directoryModel_) return;

  QString assetsPath = ArtifactProjectManager::getInstance().currentProjectAssetsPath();
  if (assetsPath.isEmpty()) {
   assetsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Assets";
  }

  QDir assetsDir(assetsPath);
  if (!assetsDir.exists()) {
   assetsDir.mkpath(".");
  }

  QString previousRoot = currentDirectoryPath_;
  directoryModel_->setAssetRootPath(assetsPath);

  if (previousRoot.isEmpty() || !QDir(previousRoot).exists() || !previousRoot.startsWith(assetsPath, Qt::CaseInsensitive)) {
   currentDirectoryPath_ = assetsPath;
  } else {
   currentDirectoryPath_ = previousRoot;
  }

  refreshUnusedAssetCache();
  clearThumbnailCache();
  applyFilters();
  syncDirectorySelection();
}

void ArtifactAssetBrowser::Impl::refreshUnusedAssetCache()
{
  unusedAssetPaths_.clear();
  auto* svc = ArtifactProjectService::instance();
  if (!svc) {
   return;
  }
  auto project = svc->getCurrentProjectSharedPtr();
  if (!project) {
   return;
  }
  const QStringList unused = ArtifactProjectCleanupTool::findUnusedAssetPaths(project.get());
  for (const QString& path : unused) {
   const QString canonicalPath = QFileInfo(path).canonicalFilePath().isEmpty()
    ? QFileInfo(path).absoluteFilePath()
    : QFileInfo(path).canonicalFilePath();
   unusedAssetPaths_.insert(QDir::cleanPath(path));
   unusedAssetPaths_.insert(QDir::cleanPath(canonicalPath));
  }
}

 void ArtifactAssetBrowser::Impl::syncDirectorySelection()
 {
  if (!directoryView_ || !directoryModel_ || currentDirectoryPath_.isEmpty()) {
   return;
  }

  std::function<QModelIndex(const QModelIndex&)> findByPath = [&](const QModelIndex& parent) -> QModelIndex {
   const int rowCount = directoryModel_->rowCount(parent);
   for (int row = 0; row < rowCount; ++row) {
    const QModelIndex index = directoryModel_->index(row, 0, parent);
    if (!index.isValid()) {
     continue;
    }
    const QString path = directoryModel_->pathFromIndex(index);
    if (QDir::cleanPath(path) == QDir::cleanPath(currentDirectoryPath_)) {
     return index;
    }
    if (directoryModel_->canFetchMore(index)) {
     directoryModel_->fetchMore(index);
    }
    if (const QModelIndex child = findByPath(index); child.isValid()) {
     return child;
    }
   }
   return {};
  };

  const QModelIndex matchedIndex = findByPath({});
  if (!matchedIndex.isValid()) {
   return;
  }

  directoryView_->expand(matchedIndex.parent());
  directoryView_->setCurrentIndex(matchedIndex);
  directoryView_->scrollTo(matchedIndex, QAbstractItemView::PositionAtCenter);
 }

 void ArtifactAssetBrowser::Impl::applyFilters()
 {
  if (!fileView_ || !assetModel_ || currentDirectoryPath_.isEmpty()) return;

  QDir dir(currentDirectoryPath_);
  if (!dir.exists()) return;

  // Update path label
  if (currentPathLabel_) {
   currentPathLabel_->setText(currentDirectoryPath_);
  }

  // Get both files and directories, excluding . and ..
  QStringList entries = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
  QList<AssetMenuItem> items;

  for (const QString& entry : entries) {
   QString fullPath = dir.absoluteFilePath(entry);
   QFileInfo fileInfo(fullPath);

   // Skip directories if filtering for specific file types (except "all")
   bool isDir = fileInfo.isDir();
   if (isDir && currentFileTypeFilter_ != "all") {
    continue;
   }

   // Check search filter
   if (!matchesSearchFilter(entry)) {
    continue;
   }

    // For files, check type filter
    if (!isDir && !matchesFileTypeFilter(entry)) {
     continue;
    }

    // Check status filter
    if (!isDir && currentStatusFilter_ != "all") {
     const bool imported = isImportedAssetPath(fullPath);
     const bool unused = isUnusedAssetPath(fullPath);
     const bool missing = isMissingAssetPath(fullPath);
     if (currentStatusFilter_ == "imported" && !imported) continue;
     if (currentStatusFilter_ == "missing" && !missing) continue;
     if (currentStatusFilter_ == "unused" && !unused) continue;
    }

    AssetMenuItem item;
   item.name = UniString::fromQString(entry);
   item.path = UniString::fromQString(fullPath);
   QString itemType = isDir ? QStringLiteral("Folder") : fileInfo.suffix().toUpper();
   if (!isDir) {
    const bool imported = isImportedAssetPath(fullPath);
    const bool unused = isUnusedAssetPath(fullPath);
    const bool missing = isMissingAssetPath(fullPath);
    if (missing && imported && unused) {
     itemType = QStringLiteral("Missing • Imported • Unused • %1").arg(itemType);
    } else if (missing && imported) {
     itemType = QStringLiteral("Missing • Imported • %1").arg(itemType);
    } else if (missing) {
     itemType = QStringLiteral("Missing • %1").arg(itemType);
    } else if (imported && unused) {
     itemType = QStringLiteral("Imported • Unused • %1").arg(itemType);
    } else if (imported) {
     itemType = QStringLiteral("Imported • %1").arg(itemType);
    } else if (unused) {
     itemType = QStringLiteral("Unused • %1").arg(itemType);
    }
   }
   item.type = UniString::fromQString(itemType);
   item.isFolder = isDir;

   // Generate thumbnail/icon for display
   item.icon = generateThumbnail(fullPath);

   items.append(item);
  }

  assetModel_->setItems(items);
 }

 ArtifactAssetBrowser::ArtifactAssetBrowser(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  setWindowTitle("AssetBrowser");

  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);
  setStyleSheet(style);

  // Enable drag and drop
  setAcceptDrops(true);

  auto assetToolBar = new ArtifactAssetBrowserToolBar();
  impl_->searchEdit_ = assetToolBar->findChild<QLineEdit*>();
  impl_->upButton_ = assetToolBar->findChild<QToolButton*>(QStringLiteral("assetBrowserUpButton"));
  impl_->refreshButton_ = assetToolBar->findChild<QToolButton*>(QStringLiteral("assetBrowserRefreshButton"));

  // File type filter buttons
  auto filterButtonsLayout = new QHBoxLayout();
  impl_->filterButtonGroup_ = new QButtonGroup(this);

  auto allButton = new QToolButton();
  allButton->setText("All");
  allButton->setCheckable(true);
  allButton->setChecked(true);

  auto imagesButton = new QToolButton();
  imagesButton->setText("Images");
  imagesButton->setCheckable(true);

  auto videosButton = new QToolButton();
  videosButton->setText("Videos");
  videosButton->setCheckable(true);

  auto audioButton = new QToolButton();
  audioButton->setText("Audio");
  audioButton->setCheckable(true);

  auto fontsButton = new QToolButton();
  fontsButton->setText("Fonts");
  fontsButton->setCheckable(true);

  impl_->filterButtonGroup_->addButton(allButton, 0);
  impl_->filterButtonGroup_->addButton(imagesButton, 1);
  impl_->filterButtonGroup_->addButton(videosButton, 2);
  impl_->filterButtonGroup_->addButton(audioButton, 3);
  impl_->filterButtonGroup_->addButton(fontsButton, 4);

   filterButtonsLayout->addWidget(allButton);
   filterButtonsLayout->addWidget(imagesButton);
   filterButtonsLayout->addWidget(videosButton);
   filterButtonsLayout->addWidget(audioButton);
   filterButtonsLayout->addWidget(fontsButton);

   // Status filter: All / Imported / Missing / Unused
   auto* sep = new QFrame();
   sep->setFrameShape(QFrame::VLine);
   sep->setFixedHeight(20);
   filterButtonsLayout->addWidget(sep);

   auto statusAllBtn = new QToolButton();
   statusAllBtn->setText("Status: All");
   statusAllBtn->setCheckable(true);
   statusAllBtn->setChecked(true);

   auto importedBtn = new QToolButton();
   importedBtn->setText("Imported");
   importedBtn->setCheckable(true);

   auto missingBtn = new QToolButton();
   missingBtn->setText("Missing");
   missingBtn->setCheckable(true);

   auto unusedBtn = new QToolButton();
   unusedBtn->setText("Unused");
   unusedBtn->setCheckable(true);

   auto* statusGroup = new QButtonGroup(this);
   statusGroup->setExclusive(true);
   statusGroup->addButton(statusAllBtn, 0);
   statusGroup->addButton(importedBtn, 1);
   statusGroup->addButton(missingBtn, 2);
   statusGroup->addButton(unusedBtn, 3);

   filterButtonsLayout->addWidget(statusAllBtn);
   filterButtonsLayout->addWidget(importedBtn);
   filterButtonsLayout->addWidget(missingBtn);
   filterButtonsLayout->addWidget(unusedBtn);
   filterButtonsLayout->addStretch();

   connect(statusGroup, &QButtonGroup::idClicked, this, [this](int id) {
    switch (id) {
     case 0: impl_->currentStatusFilter_ = "all"; break;
     case 1: impl_->currentStatusFilter_ = "imported"; break;
     case 2: impl_->currentStatusFilter_ = "missing"; break;
     case 3: impl_->currentStatusFilter_ = "unused"; break;
    }
    impl_->applyFilters();
   });

  auto vLayout = new QVBoxLayout();

  auto layout = new QHBoxLayout();

  auto directoryView = impl_->directoryView_ = new QTreeView();
  auto directoryModel = impl_->directoryModel_ = new AssetDirectoryModel(this);

  QString assetsPath = ArtifactProjectManager::getInstance().currentProjectAssetsPath();
  if (assetsPath.isEmpty()) {
   assetsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Assets";
  }
  QString packagesPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Packages";

  directoryModel->setAssetRootPath(assetsPath);
  directoryModel->setPackageRootPath(packagesPath);

  directoryView->setModel(directoryModel);
  directoryView->setHeaderHidden(true);
  directoryView->setIndentation(15);
  directoryView->setExpandsOnDoubleClick(true);
   directoryView->setAnimated(true);
   directoryView->setAcceptDrops(true);
   directoryView->setDropIndicatorShown(true);
   directoryView->setDragDropMode(QAbstractItemView::DropOnly);

  QString desktopPath = assetsPath;

  auto assetPathLabel = new QLabel("Assets");
  assetPathLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  assetPathLabel->setStyleSheet("font-weight: bold;");

  auto filePathLabel = impl_->currentPathLabel_ = new QLabel(desktopPath);
  filePathLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  filePathLabel->setStyleSheet("color: gray; font-size: 10pt;");
  filePathLabel->setWordWrap(true);

  auto assetModel = impl_->assetModel_ = new AssetMenuModel(this);
  auto fileView = impl_->fileView_ = new AssetFileListView();
  fileView->setModel(assetModel);
  impl_->currentDirectoryPath_ = desktopPath;  // Set initial directory
  fileView->setViewMode(QListView::IconMode);
  fileView->setIconSize(QSize(64, 64));
  fileView->setGridSize(QSize(100, 100));  // Fixed grid size for uniform spacing
  fileView->setResizeMode(QListView::Adjust);
  fileView->setFlow(QListView::LeftToRight);
  fileView->setTextElideMode(Qt::ElideMiddle);  // Show "longfile...name.png"
  fileView->setWordWrap(true);
  fileView->setSpacing(5);  // Uniform spacing between items
  fileView->setUniformItemSizes(true);  // Optimize rendering with uniform sizes
  fileView->setDragEnabled(true);
  fileView->setAcceptDrops(true);
  fileView->setDropIndicatorShown(true);
  fileView->setDragDropMode(QAbstractItemView::DragDrop);
  fileView->setDefaultDropAction(Qt::CopyAction);
  fileView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  fileView->setContextMenuPolicy(Qt::CustomContextMenu);  // Enable custom context menu

  // Connect search filter
  if (impl_->searchEdit_) {
   connect(impl_->searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
    impl_->currentSearchFilter_ = text;
    impl_->applyFilters();
   });
  }

  if (impl_->upButton_) {
   connect(impl_->upButton_, &QToolButton::clicked, this, [this]() {
    if (impl_->currentDirectoryPath_.isEmpty()) return;
    const QString assetsRoot = ArtifactProjectManager::getInstance().currentProjectAssetsPath();
    const QDir currentDir(impl_->currentDirectoryPath_);
    QString nextPath = QFileInfo(currentDir.absolutePath()).dir().absolutePath();
    if (nextPath.isEmpty()) {
     nextPath = assetsRoot;
    }
   if (!assetsRoot.isEmpty() && !nextPath.startsWith(assetsRoot, Qt::CaseInsensitive)) {
     nextPath = assetsRoot;
    }
    if (nextPath.isEmpty() || nextPath == impl_->currentDirectoryPath_) return;
    impl_->currentDirectoryPath_ = nextPath;
    impl_->clearThumbnailCache();
    impl_->applyFilters();
    impl_->syncDirectorySelection();
    folderChanged(nextPath);
   });
  }

  if (impl_->refreshButton_) {
   connect(impl_->refreshButton_, &QToolButton::clicked, this, [this]() {
    impl_->clearThumbnailCache();
    impl_->applyFilters();
   });
  }

  // Connect file type filter buttons
  connect(impl_->filterButtonGroup_, &QButtonGroup::idClicked, this, [this](int id) {
   switch(id) {
    case 0: impl_->currentFileTypeFilter_ = "all"; break;
   case 1: impl_->currentFileTypeFilter_ = "images"; break;
   case 2: impl_->currentFileTypeFilter_ = "videos"; break;
   case 3: impl_->currentFileTypeFilter_ = "audio"; break;
   case 4: impl_->currentFileTypeFilter_ = "fonts"; break;
   }
   impl_->applyFilters();
  });

  // Connect directory change to update file list (LEFT -> RIGHT widget coordination)
  connect(directoryView, &QTreeView::clicked, this, [this, directoryModel](const QModelIndex& index) {
   QString path = directoryModel->pathFromIndex(index);

   if (!path.isEmpty()) {
    impl_->currentDirectoryPath_ = path;
    impl_->clearThumbnailCache();
    impl_->applyFilters();
    impl_->syncDirectorySelection();
    folderChanged(path);
   }
  });

  // Connect file double-click to add to project or navigate into folder
  connect(fileView, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
   if (!index.isValid()) return;
   AssetMenuItem item = impl_->assetModel_->itemAt(index.row());
   QString filePath = item.path.toQString();
   if (filePath.isEmpty()) return;
   itemDoubleClicked(filePath);

   // If it's a folder, navigate into it
   if (item.isFolder) {
    impl_->currentDirectoryPath_ = filePath;
    impl_->clearThumbnailCache();
    impl_->applyFilters();
    impl_->syncDirectorySelection();
    folderChanged(filePath);
    return;
   }

   // Otherwise, add file to project
   auto* svc = ArtifactProjectService::instance();
   if (!svc) return;
   svc->importAssetsFromPaths(QStringList() << filePath);
  });

  // Connect right-click context menu
  connect(fileView, &QListView::customContextMenuRequested, this, &ArtifactAssetBrowser::showContextMenu);

  // Connect file item selection to update details
  connect(fileView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
   QModelIndexList selectedIndexes = impl_->fileView_->selectionModel()->selectedIndexes();
   QStringList selectedFiles;
   selectedFiles.reserve(selectedIndexes.size());
   if (!selectedIndexes.isEmpty()) {
    for (const QModelIndex& index : selectedIndexes) {
     const AssetMenuItem item = impl_->assetModel_->itemAt(index.row());
     const QString filePath = item.path.toQString();
     if (!filePath.isEmpty()) {
      selectedFiles.append(filePath);
     }
    }
    if (!selectedFiles.isEmpty()) {
     updateFileInfo(selectedFiles.first());
    }
   } else if (impl_->fileInfoLabel_) {
    impl_->fileInfoLabel_->setText(QStringLiteral("No file selected"));
   }
   selectionChanged(selectedFiles);
  });

  // Create thumbnail size adjustment
  auto thumbnailControlGroup = new QGroupBox("Thumbnail Size");
  auto thumbnailLayout = new QHBoxLayout();

  auto sizeLabel = new QLabel("64px");
  auto sizeSlider = impl_->thumbnailSizeSlider_ = new QSlider(Qt::Horizontal);
  sizeSlider->setMinimum(32);  // Min 32px
  sizeSlider->setMaximum(256);  // Max 256px
  sizeSlider->setValue(64);  // Default 64px
  sizeSlider->setTickPosition(QSlider::TicksBelow);
  sizeSlider->setTickInterval(32);

  connect(sizeSlider, &QSlider::valueChanged, this, [this, sizeLabel, fileView](int value) {
   sizeLabel->setText(QString("%1px").arg(value));

   // Update icon size and grid size
   fileView->setIconSize(QSize(value, value));
   int gridSize = value + 36;  // Add padding for text and spacing
   fileView->setGridSize(QSize(gridSize, gridSize));
  });

  thumbnailLayout->addWidget(new QLabel("Size:"));
  thumbnailLayout->addWidget(sizeSlider);
  thumbnailLayout->addWidget(sizeLabel);
  thumbnailControlGroup->setLayout(thumbnailLayout);

  // Create file info panel
  auto fileInfoGroup = new QGroupBox("File Details");
  auto fileInfoLayout = new QVBoxLayout();

  auto fileInfoLabel = impl_->fileInfoLabel_ = new QLabel("No file selected");
  fileInfoLabel->setWordWrap(true);
  fileInfoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  fileInfoLabel->setStyleSheet("color: gray; font-size: 9pt;");

  fileInfoLayout->addWidget(fileInfoLabel);
  fileInfoGroup->setLayout(fileInfoLayout);
  fileInfoGroup->setMaximumHeight(150);

  // Initial load
  impl_->applyFilters();

  auto* projectService = ArtifactProjectService::instance();
  if (projectService) {
   connect(projectService, &ArtifactProjectService::projectCreated, this, [this]() {
    impl_->syncProjectAssetRoot();
   });
   connect(projectService, &ArtifactProjectService::projectChanged, this, [this]() {
    impl_->syncProjectAssetRoot();
   });
  }

  auto VBoxLayout = new  QVBoxLayout();
  VBoxLayout->addWidget(assetPathLabel);
  VBoxLayout->addWidget(filePathLabel);
  VBoxLayout->addWidget(thumbnailControlGroup);
  VBoxLayout->addWidget(fileView);
  VBoxLayout->addWidget(fileInfoGroup);

  vLayout->addWidget(assetToolBar);
  vLayout->addLayout(filterButtonsLayout);
  layout->addWidget(directoryView, 1);
  layout->addLayout(VBoxLayout, 3);
  setLayout(layout);
 }

 ArtifactAssetBrowser::~ArtifactAssetBrowser()
 {
  delete impl_;
 }

 QSize ArtifactAssetBrowser::sizeHint() const
 {
  return QSize(250, 600);
 }

 W_OBJECT_IMPL(ArtifactAssetBrowser)

 void ArtifactAssetBrowser::mousePressEvent(QMouseEvent* event)
 {
  impl_->defaultHandleMousePressEvent(event);
 }

 void ArtifactAssetBrowser::keyPressEvent(QKeyEvent* event)
 {
  if (!impl_->fileView_ || !impl_->assetModel_) {
   QWidget::keyPressEvent(event);
   return;
  }

  const auto* sel = impl_->fileView_->selectionModel();

  // Ctrl+A — 全選択
  if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
   impl_->fileView_->selectAll();
   event->accept();
   return;
  }

  // Ctrl+C — パスをコピー
  if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
   QStringList paths = impl_->selectedAssetPaths();
   if (!paths.isEmpty()) {
    QApplication::clipboard()->setText(paths.join("\n"));
   }
   event->accept();
   return;
  }

  // Ctrl+V — クリップボードのファイルをインポート
  if (event->key() == Qt::Key_V && (event->modifiers() & Qt::ControlModifier)) {
   const QMimeData* mime = QApplication::clipboard()->mimeData();
   if (mime && mime->hasUrls()) {
    QStringList paths;
    for (const QUrl& url : mime->urls()) {
     if (url.isLocalFile()) paths.append(url.toLocalFile());
    }
    if (!paths.isEmpty() && ArtifactProjectService::instance()) {
     ArtifactProjectService::instance()->importAssetsFromPaths(paths);
    }
   }
   event->accept();
   return;
  }

  // Delete — 選択ファイルをプロジェクトから削除
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
   QStringList paths = impl_->selectedAssetPaths();
   if (!paths.isEmpty()) {
    auto* svc = ArtifactProjectService::instance();
    if (svc) {
     // 個別削除APIが未実装のため、removeAllAssets() は呼ばない
     // TODO: removeAssetFromProject(path) API 追加後に差し替え
     qWarning() << "[AssetBrowser] Delete requested for" << paths.size() << "items (individual removal not yet implemented)";
    }
   }
   event->accept();
   return;
  }

  // Enter — ダブルクリック相当
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
   if (sel && sel->hasSelection()) {
    QModelIndex idx = sel->currentIndex();
    if (idx.isValid()) {
     impl_->handleDoubleClicked();
    }
   }
   event->accept();
   return;
  }

  QWidget::keyPressEvent(event);
 }

 void ArtifactAssetBrowser::keyReleaseEvent(QKeyEvent* event)
 {
 }

 void ArtifactAssetBrowser::dragEnterEvent(QDragEnterEvent* event)
 {
  // Accept file drops from external sources
  if (event->mimeData()->hasUrls()) {
   event->acceptProposedAction();
  }
 }

void ArtifactAssetBrowser::selectAssetPaths(const QStringList& filePaths)
{
  if (!impl_ || !impl_->fileView_ || !impl_->assetModel_) {
   return;
  }

  QStringList normalizedPaths;
  normalizedPaths.reserve(filePaths.size());
  for (const QString& filePath : filePaths) {
   const QString normalized = normalizeAssetPath(filePath);
   if (!normalized.isEmpty()) {
    normalizedPaths.append(normalized);
   }
  }
  normalizedPaths.removeDuplicates();
  if (normalizedPaths.isEmpty()) {
   return;
  }

  const QString targetFolder = QFileInfo(normalizedPaths.first()).absolutePath();
  if (!targetFolder.isEmpty() && targetFolder != impl_->currentDirectoryPath_) {
   navigateToFolder(targetFolder);
  } else {
   impl_->applyFilters();
  }

  auto* selection = impl_->fileView_->selectionModel();
  if (!selection) {
   return;
  }

  QSignalBlocker blocker(selection);
  selection->clearSelection();
  QModelIndex firstSelected;
  for (int row = 0; row < impl_->assetModel_->rowCount(); ++row) {
   const AssetMenuItem item = impl_->assetModel_->itemAt(row);
   if (item.isFolder) {
    continue;
   }
   const QString itemPath = normalizeAssetPath(item.path.toQString());
   if (itemPath.isEmpty() || !normalizedPaths.contains(itemPath)) {
    continue;
   }
   const QModelIndex index = impl_->assetModel_->index(row, 0);
   if (!index.isValid()) {
    continue;
   }
   selection->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
   if (!firstSelected.isValid()) {
    firstSelected = index;
   }
  }

  if (firstSelected.isValid()) {
   impl_->fileView_->setCurrentIndex(firstSelected);
   updateFileInfo(normalizedPaths.first());
   emit selectionChanged(normalizedPaths);
  }
}

 void ArtifactAssetBrowser::dropEvent(QDropEvent* event)
 {
  const QMimeData* mimeData = event->mimeData();

  if (mimeData->hasUrls()) {
   QStringList filePaths;
   QList<QUrl> urls = mimeData->urls();

   for (const QUrl& url : urls) {
    if (url.isLocalFile()) {
     QString filePath = url.toLocalFile();
     filePaths.append(filePath);
    }
   }

   if (!filePaths.isEmpty()) {
    auto* svc = ArtifactProjectService::instance();
    if (svc) {
     QStringList imported = svc->importAssetsFromPaths(filePaths);
     if (!imported.isEmpty()) {
      filesDropped(imported);
     }
    }
    // Refresh file view
    impl_->applyFilters();
   }

   event->acceptProposedAction();
  }
 }

 void ArtifactAssetBrowser::setSearchFilter(const QString& filter)
 {
  impl_->currentSearchFilter_ = filter;
  if (impl_->searchEdit_) {
   impl_->searchEdit_->setText(filter);
  }
  impl_->applyFilters();
 }

  void ArtifactAssetBrowser::setFileTypeFilter(const QString& type)
  {
   impl_->currentFileTypeFilter_ = type;

   // Update button state
   if (impl_->filterButtonGroup_) {
    if (type == "all") impl_->filterButtonGroup_->button(0)->setChecked(true);
    else if (type == "images") impl_->filterButtonGroup_->button(1)->setChecked(true);
    else if (type == "videos") impl_->filterButtonGroup_->button(2)->setChecked(true);
    else if (type == "audio") impl_->filterButtonGroup_->button(3)->setChecked(true);
    else if (type == "fonts") impl_->filterButtonGroup_->button(4)->setChecked(true);
   }

   impl_->applyFilters();
  }

  void ArtifactAssetBrowser::setStatusFilter(const QString& status)
  {
   impl_->currentStatusFilter_ = status;
   impl_->applyFilters();
  }

  void ArtifactAssetBrowser::navigateToFolder(const QString& folderPath)
  {
   if (folderPath.isEmpty() || !QDir(folderPath).exists()) return;
   impl_->currentDirectoryPath_ = folderPath;
   impl_->clearThumbnailCache();
   impl_->applyFilters();
   impl_->syncDirectorySelection();
   folderChanged(folderPath);
  }

 void ArtifactAssetBrowser::updateFileInfo(const QString& filePath)
 {
  if (filePath.isEmpty() || !impl_->fileInfoLabel_) return;

  QFileInfo fileInfo(filePath);

  if (!fileInfo.exists()) {
   impl_->fileInfoLabel_->setText("File not found");
   return;
  }

  // Build information string
  QString info;
  info += QString("<b>%1</b><br>").arg(fileInfo.fileName());

  // Check if it's a folder
  if (fileInfo.isDir()) {
   info += "Type: Folder<br>";
   info += QString("Entries: %1<br>").arg(QDir(filePath).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).size());
   info += QString("Project: %1<br>").arg(impl_->isImportedAssetPath(filePath) ? QStringLiteral("Imported") : QStringLiteral("Not Imported"));
   info += QString("Usage: %1<br>").arg(impl_->isUnusedAssetPath(filePath) ? QStringLiteral("Unused") : QStringLiteral("In Use / N.A."));
   info += QString("Status: %1<br>").arg(impl_->isMissingAssetPath(filePath) ? QStringLiteral("Missing") : QStringLiteral("OK"));
   impl_->fileInfoLabel_->setText(info);
   return;
  }

  info += QString("Size: %1 KB<br>").arg(fileInfo.size() / 1024);
  info += QString("Type: %1<br>").arg(fileInfo.suffix().toUpper());
  info += QString("Modified: %1<br>").arg(fileInfo.lastModified().toString("yyyy-MM-dd hh:mm"));
  info += QString("Project: %1<br>").arg(impl_->isImportedAssetPath(filePath) ? QStringLiteral("Imported") : QStringLiteral("Not Imported"));
  info += QString("Usage: %1<br>").arg(impl_->isUnusedAssetPath(filePath) ? QStringLiteral("Unused") : QStringLiteral("In Use"));
  info += QString("Status: %1<br>").arg(impl_->isMissingAssetPath(filePath) ? QStringLiteral("Missing") : QStringLiteral("OK"));

  // Get image resolution for image files
  QString fileName = fileInfo.fileName();
  QString lowerName = fileName.toLower();

  if (lowerName.endsWith(".png") || lowerName.endsWith(".jpg") ||
      lowerName.endsWith(".jpeg") || lowerName.endsWith(".bmp") ||
      lowerName.endsWith(".gif") || lowerName.endsWith(".tga") ||
      lowerName.endsWith(".tiff") || lowerName.endsWith(".exr")) {
   QImageReader imageReader(filePath);
   const QSize imageSize = imageReader.size();
   if (imageSize.isValid()) {
    info += QString("Resolution: %1 x %2 px<br>").arg(imageSize.width()).arg(imageSize.height());
    const QByteArray imageFormat = imageReader.format();
    if (!imageFormat.isEmpty()) {
     info += QString("Format: %1").arg(QString::fromLatin1(imageFormat).toUpper());
    }
   }
  }
  else if (impl_->isVideoFile(fileName)) {
   cv::VideoCapture cap(filePath.toLocal8Bit().constData());
   if (cap.isOpened()) {
    const double width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    const double height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    const double fps = cap.get(cv::CAP_PROP_FPS);
    const double frameCount = cap.get(cv::CAP_PROP_FRAME_COUNT);
    if (width > 0.0 && height > 0.0) {
     info += QString("Resolution: %1 x %2 px<br>").arg(static_cast<int>(width)).arg(static_cast<int>(height));
    }
    if (fps > 0.0) {
     info += QString("FPS: %1<br>").arg(QString::number(fps, 'f', fps == std::floor(fps) ? 0 : 2));
    }
    if (fps > 0.0 && frameCount > 0.0) {
     info += QString("Duration: %1 s<br>").arg(QString::number(frameCount / fps, 'f', 2));
    }
   }
  }
  else if (impl_->isAudioFile(fileName)) {
   info += QString("Kind: Audio<br>");
  }
  else if (impl_->isFontFile(fileName)) {
   info += QString("Kind: Font<br>");
  }

  impl_->fileInfoLabel_->setText(info);
}

 void ArtifactAssetBrowser::showContextMenu(const QPoint& pos)
 {
  QModelIndex index = impl_->fileView_->indexAt(pos);
  if (!index.isValid()) return;  // No item under cursor

  AssetMenuItem item = impl_->assetModel_->itemAt(index.row());
  QString filePath = item.path.toQString();
  if (filePath.isEmpty()) return;

  // Create context menu
  QMenu contextMenu;

  const QStringList selectedAssetPaths = impl_->selectedAssetPaths();
  const QStringList importTargets = selectedAssetPaths.isEmpty() ? QStringList{filePath} : selectedAssetPaths;

  // Add to Project action
  const QString addActionLabel = importTargets.size() > 1
   ? QStringLiteral("Add %1 Items to Project").arg(importTargets.size())
   : QStringLiteral("Add to Project");
  QAction* addToProjectAction = contextMenu.addAction(addActionLabel);
  connect(addToProjectAction, &QAction::triggered, this, [this, importTargets, filePath]() {
   if (importTargets.isEmpty() && filePath.isEmpty()) return;
   auto* svc = ArtifactProjectService::instance();
   if (!svc) return;
   const QStringList imported = svc->importAssetsFromPaths(importTargets.isEmpty() ? QStringList{filePath} : importTargets);
   if (!imported.isEmpty()) {
    filesDropped(imported);
    impl_->applyFilters();
   }
  });

  contextMenu.addSeparator();

  if (!item.isFolder) {
   QAction* previewAction = contextMenu.addAction("Preview in Contents Viewer");
   connect(previewAction, &QAction::triggered, this, [this, filePath]() {
    if (filePath.isEmpty()) return;
    itemDoubleClicked(filePath);
   });
  }

  if (item.isFolder) {
   QAction* openFolderAction = contextMenu.addAction("Open Folder");
   connect(openFolderAction, &QAction::triggered, this, [this, filePath]() {
    if (filePath.isEmpty()) return;
    impl_->currentDirectoryPath_ = filePath;
    impl_->clearThumbnailCache();
    impl_->applyFilters();
    impl_->syncDirectorySelection();
    folderChanged(filePath);
   });
   contextMenu.addSeparator();
  }

  // Open in File Explorer action
  QAction* openInExplorerAction = contextMenu.addAction("Open in File Explorer");
  connect(openInExplorerAction, &QAction::triggered, this, [filePath]() {
   QFileInfo fileInfo(filePath);
   QString folderPath = fileInfo.dir().absolutePath();
   QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
  });

  // Copy file path action
  QAction* copyPathAction = contextMenu.addAction("Copy File Path");
  connect(copyPathAction, &QAction::triggered, this, [filePath]() {
   QApplication::clipboard()->setText(filePath);
  });

  contextMenu.addSeparator();

  // Show file properties action
  QAction* showPropertiesAction = contextMenu.addAction("Properties");
  connect(showPropertiesAction, &QAction::triggered, this, [filePath]() {
   QFileInfo fileInfo(filePath);
   QString info = QString("Name: %1\nSize: %2 bytes\nType: %3\nPath: %4")
     .arg(fileInfo.fileName())
     .arg(fileInfo.size())
     .arg(fileInfo.suffix())
     .arg(filePath);
   // TODO: Show in a dialog or status bar
  });

  // Show menu at cursor position
  contextMenu.exec(impl_->fileView_->mapToGlobal(pos));
 }

}
