module;
#include <QFileSystemModel>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWidget>
#include <QListView>
#include <QListWidget>
#include <QToolButton>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QButtonGroup>
#include <QPixmap>
#include <QIcon>
#include <QHash>
#include <QFileInfo>
#include <wobjectimpl.h>
#include <boost/asio/basic_signal_set.hpp>

#include <qcoro6/qcoro/qcorotask.h>
module Widgets.AssetBrowser;
import Widgets.Utils.CSS;

import Artifact.Service.Project;
import Artifact.Project.Manager;


namespace Artifact {

 using namespace ArtifactCore;

 W_OBJECT_IMPL(ArtifactAssetBrowserToolBar)



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

 ArtifactAssetBrowserToolBar::ArtifactAssetBrowserToolBar(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl())
 {

 	
  auto layout = new QHBoxLayout();
  layout->addWidget(impl_->searchWidget);
 	

  setLayout(layout);
 }

 ArtifactAssetBrowserToolBar::~ArtifactAssetBrowserToolBar()
 {
  delete impl_;
 }

 W_OBJECT_IMPL(ArtifactAssetBrowser)

  class ArtifactAssetBrowser::Impl
 {
 private:
  QHash<QString, QIcon> thumbnailCache_;  // Cache thumbnails by file path
  QSize thumbnailSize_{64, 64};
  QIcon defaultFileIcon_;
  QIcon defaultImageIcon_;
  QIcon defaultVideoIcon_;
  QIcon defaultAudioIcon_;

 public:
  Impl();
  ~Impl();
 	QTreeView* directoryView_ = nullptr;
  QListWidget* fileView_ = nullptr;
  QLineEdit* searchEdit_ = nullptr;
  QFileSystemModel* fileModel_ = nullptr;
  QButtonGroup* filterButtonGroup_ = nullptr;
  QString currentFileTypeFilter_ = "all";
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
 };

  ArtifactAssetBrowser::Impl::Impl()
  {
    // Initialize default icons (using Qt standard icons as fallback)
    // You can replace these with custom icons later
    defaultFileIcon_ = QIcon(":/icons/file.png");  // Fallback to empty icon
    defaultImageIcon_ = QIcon(":/icons/image.png");
    defaultVideoIcon_ = QIcon(":/icons/video.png");
    defaultAudioIcon_ = QIcon(":/icons/audio.png");
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
    
    return true;
  }

  bool ArtifactAssetBrowser::Impl::matchesSearchFilter(const QString& fileName) const
  {
    if (currentSearchFilter_.isEmpty()) return true;
    return fileName.contains(currentSearchFilter_, Qt::CaseInsensitive);
  }

  bool ArtifactAssetBrowser::Impl::isImageFile(const QString& fileName) const
  {
    QString lower = fileName.toLower();
    return lower.endsWith(".png") || lower.endsWith(".jpg") || 
           lower.endsWith(".jpeg") || lower.endsWith(".bmp") ||
           lower.endsWith(".gif") || lower.endsWith(".tga") ||
           lower.endsWith(".tiff") || lower.endsWith(".exr");
  }

  bool ArtifactAssetBrowser::Impl::isVideoFile(const QString& fileName) const
  {
    QString lower = fileName.toLower();
    return lower.endsWith(".mp4") || lower.endsWith(".mov") ||
           lower.endsWith(".avi") || lower.endsWith(".mkv") ||
           lower.endsWith(".webm") || lower.endsWith(".flv");
  }

  bool ArtifactAssetBrowser::Impl::isAudioFile(const QString& fileName) const
  {
    QString lower = fileName.toLower();
    return lower.endsWith(".mp3") || lower.endsWith(".wav") ||
           lower.endsWith(".ogg") || lower.endsWith(".flac") ||
           lower.endsWith(".aac") || lower.endsWith(".m4a");
  }

  QIcon ArtifactAssetBrowser::Impl::generateThumbnail(const QString& filePath)
  {
    // Check cache first
    if (thumbnailCache_.contains(filePath)) {
      return thumbnailCache_[filePath];
    }
    
    QFileInfo fileInfo(filePath);
    
    // Generate thumbnail for image files
    if (isImageFile(fileInfo.fileName())) {
      QPixmap pixmap(filePath);
      if (!pixmap.isNull()) {
        QPixmap scaled = pixmap.scaled(thumbnailSize_, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QIcon icon(scaled);
        thumbnailCache_[filePath] = icon;
        return icon;
      }
    }
    
    // For video files, use a default video icon
    // TODO: In the future, extract first frame as thumbnail
    if (isVideoFile(fileInfo.fileName())) {
      thumbnailCache_[filePath] = defaultVideoIcon_;
      return defaultVideoIcon_;
    }
    
    // For audio files, use a default audio icon
    if (isAudioFile(fileInfo.fileName())) {
      thumbnailCache_[filePath] = defaultAudioIcon_;
      return defaultAudioIcon_;
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

  void ArtifactAssetBrowser::Impl::applyFilters()
  {
    if (!fileView_ || !fileModel_) return;
    
    // Clear and rebuild file list
    fileView_->clear();
    
    QModelIndex rootIndex = fileView_->rootIndex();
    if (!rootIndex.isValid()) return;
    
    QString rootPath = fileModel_->filePath(rootIndex);
    QDir dir(rootPath);
    
    QStringList files = dir.entryList(QDir::Files);
    
    for (const QString& file : files) {
      if (matchesFileTypeFilter(file) && matchesSearchFilter(file)) {
        QString fullPath = dir.absoluteFilePath(file);
        QIcon icon = getFileIcon(file, fullPath);
        
        QListWidgetItem* item = new QListWidgetItem(icon, file);
        item->setData(Qt::UserRole, fullPath);  // Store full path in user data
        fileView_->addItem(item);
      }
    }
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
  
  impl_->filterButtonGroup_->addButton(allButton, 0);
  impl_->filterButtonGroup_->addButton(imagesButton, 1);
  impl_->filterButtonGroup_->addButton(videosButton, 2);
  impl_->filterButtonGroup_->addButton(audioButton, 3);
  
  filterButtonsLayout->addWidget(allButton);
  filterButtonsLayout->addWidget(imagesButton);
  filterButtonsLayout->addWidget(videosButton);
  filterButtonsLayout->addWidget(audioButton);
  filterButtonsLayout->addStretch();

  auto vLayout = new QVBoxLayout();


  auto layout = new QHBoxLayout();

  auto directoryView = impl_->directoryView_ = new QTreeView();
  auto model = new QFileSystemModel(this);
  model->setRootPath(""); // 空にしておくと全体が見える
  model->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);

  directoryView->setModel(model);
  directoryView->setColumnHidden(1, true); // Size
  directoryView->setColumnHidden(2, true); // Type
  directoryView->setColumnHidden(3, true);
  directoryView->setHeaderHidden(true);

  QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  directoryView->setRootIndex(model->index(desktopPath));

  directoryView->setIndentation(15);          // 階層のインデント
  directoryView->setExpandsOnDoubleClick(true);
  directoryView->setAnimated(true);

  auto assetPathLabel = new QLabel("Assets > 2D Texture");
  assetPathLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  assetPathLabel->setStyleSheet("font-weight: bold;");

  auto fileView = impl_->fileView_ = new QListWidget();
  impl_->fileModel_ = model;
  fileView->setViewMode(QListView::IconMode);
  fileView->setIconSize(QSize(64, 64));
  fileView->setResizeMode(QListView::Adjust);
  fileView->setFlow(QListView::LeftToRight);
  //fileView->setModel(model);  // QFileSystemModel をセット
  fileView->setRootIndex(model->index(desktopPath));
  fileView->setTextElideMode(Qt::ElideRight);
  fileView->setDragEnabled(true);
  // デスクトップを表示
  
  // Connect search filter
  if (impl_->searchEdit_) {
    connect(impl_->searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
      impl_->currentSearchFilter_ = text;
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
    }
    impl_->applyFilters();
  });
  
  // Connect directory change to update file list
  connect(directoryView, &QTreeView::clicked, this, [this, model](const QModelIndex& index) {
    if (impl_->fileView_) {
      QString path = model->filePath(index);
      impl_->fileView_->setRootIndex(model->index(path));
      impl_->clearThumbnailCache();  // Clear cache when changing directory
      impl_->applyFilters();
      folderChanged(path);
    }
  });
  
  // Initial load
  impl_->applyFilters();


  auto filePathLabel = new QLabel();

  auto VBoxLayout = new  QVBoxLayout();
  VBoxLayout->addWidget(assetPathLabel);
  VBoxLayout->addWidget(fileView);
  VBoxLayout->addWidget(filePathLabel);

  vLayout->addWidget(assetToolBar);
  vLayout->addLayout(filterButtonsLayout);
  layout->addWidget(directoryView);
  layout->addLayout(VBoxLayout);
  setLayout(layout);

 }

 ArtifactAssetBrowser::~ArtifactAssetBrowser()
 {
  delete impl_;
 }

 void ArtifactAssetBrowser::mousePressEvent(QMouseEvent* event)
 {
  impl_->defaultHandleMousePressEvent(event);
 }

 void ArtifactAssetBrowser::keyPressEvent(QKeyEvent* event)
 {

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
      // Import files to project
      auto& projectManager = ArtifactProjectManager::getInstance();
      projectManager.addAssetsFromFilePaths(filePaths);
      
      // Emit signal
      filesDropped(filePaths);
      
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
  }
  
  impl_->applyFilters();
 }

};