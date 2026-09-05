module;
#include <utility>
#include <functional>
#include <QFileSystemModel>
#include <QDir>
#include <QDirIterator>
#include <QLabel>
#include <QLineEdit>
#include <QCompleter>
#include <QStringListModel>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWidget>
#include <QListView>
#include <QListWidget>
#include <QLayoutItem>
#include <QToolButton>
#include <QPushButton>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QDrag>
#include <QButtonGroup>
#include <QPixmap>
#include <QIcon>
#include <QHash>
#include <QPair>
#include <QVector>
#include <QFileInfo>
#include <QStyle>
#include <QApplication>
#include <QScreen>
#include <QEvent>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QFocusEvent>
#include <QFont>
#include <QColor>
#include <QClipboard>
#include <QApplication>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPoint>
#include <QRect>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QThread>
#include <opencv2/opencv.hpp>
#include <QSlider>
#include <QFrame>
#include <QGridLayout>
#include <QElapsedTimer>
#include <QSet>
#include <QPointer>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>
#include <QDateTime>
#include <QHBoxLayout>
#include <QAbstractItemView>
#include <QComboBox>
#include <QTabWidget>
#include <QMouseEvent>
#include <QCursor>
#include <cstdint>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <vector>
#include <wobjectimpl.h>

// Async waveform thumbnail
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QCache>
#include <QScrollBar>

// Audio waveform includes
#include <QAudioFormat>
#include <QAudioDecoder>
#ifdef emit
#pragma push_macro("emit")
#undef emit
#define ARTIFACT_RESTORE_QT_EMIT_MACRO
#endif
#ifdef ARTIFACT_RESTORE_QT_EMIT_MACRO
#pragma pop_macro("emit")
#undef ARTIFACT_RESTORE_QT_EMIT_MACRO
#endif

module Widgets.AssetBrowser;

import Artifact.Widgets.AssetThumbnailPipeline;
import Artifact.Widgets.AssetBrowserUndoCommands;
import Artifact.Widgets.AssetBrowserPresentation;
import Widgets.Utils.CSS;
import Artifact.Service.Project;
import Artifact.Project;
import Artifact.Application.Manager;
import Artifact.Service.FootageInterpret;
import Widgets.Dialog.InterpretFootage;
import Artifact.Event.Types;
import Event.Bus;
import Asset;
import Configuration.LayeredConfigStore;
import Artifact.Project.Manager;
import Artifact.Project.PresetManager;
import Artifact.Mask.LayerMask;
import Asset.Manager;
import AssetMenuModel;
import AssetDirectoryModel;
import Utils.String.UniString;
import Media.SourceInterpret;
import File.TypeDetector;
import Codec.Thumbnail.FFmpeg;
import Artifact.Audio.Waveform;
import Audio.Segment;
import Audio.SimpleWav;
import Input.Operator;
import Undo.UndoManager;
import Settings.Accessibility;
import Artifact.Template.Document;
import Artifact.Widgets.TemplateLibrary;

namespace Artifact {

static QPoint accessibilityMenuPosition(const QMenu &menu,
                                        const QPoint &origin) {
  int x = origin.x();
  int y = origin.y();
  Accessibility::adjustContextMenuPosition(x, y, menu.sizeHint().width());
  return QPoint(x, y);
}

namespace {

void discardStaleThumbnailWatcher(QFutureWatcher<QImage>* watcher)
{
 if (!watcher) return;
 QObject::disconnect(watcher, nullptr, nullptr, nullptr);
 watcher->cancel();
 watcher->deleteLater();
}

int naturalAssetNameCompare(const QString& lhs, const QString& rhs) {
 int left = 0;
 int right = 0;
 while (left < lhs.size() && right < rhs.size()) {
  const QChar lc = lhs.at(left);
  const QChar rc = rhs.at(right);
  if (lc.isDigit() && rc.isDigit()) {
   const int leftStart = left;
   const int rightStart = right;
   while (left < lhs.size() && lhs.at(left).isDigit()) ++left;
   while (right < rhs.size() && rhs.at(right).isDigit()) ++right;
   int leftSignificant = leftStart;
   int rightSignificant = rightStart;
   while (leftSignificant < left && lhs.at(leftSignificant) == QChar('0')) ++leftSignificant;
   while (rightSignificant < right && rhs.at(rightSignificant) == QChar('0')) ++rightSignificant;
   const int leftDigits = left - leftSignificant;
   const int rightDigits = right - rightSignificant;
   if (leftDigits != rightDigits) return leftDigits < rightDigits ? -1 : 1;
   const int digitCompare = lhs.mid(leftSignificant, leftDigits)
                                .compare(rhs.mid(rightSignificant, rightDigits),
                                         Qt::CaseInsensitive);
   if (digitCompare != 0) return digitCompare < 0 ? -1 : 1;
   const int leftRun = left - leftStart;
   const int rightRun = right - rightStart;
   if (leftRun != rightRun) return leftRun < rightRun ? -1 : 1;
   continue;
  }
  const int charCompare = QString(lc).compare(QString(rc), Qt::CaseInsensitive);
  if (charCompare != 0) return charCompare < 0 ? -1 : 1;
  ++left;
  ++right;
 }
 if (left == lhs.size() && right == rhs.size()) return 0;
 return left == lhs.size() ? -1 : 1;
}


} // namespace

using namespace ArtifactCore;
using namespace detail;

namespace {
constexpr int kAssetThumbnailMinPx = 25;
constexpr int kAssetThumbnailMaxPx = 256;
constexpr int kAssetThumbnailDefaultPx = 128;
constexpr auto kAssetBrowserContext = "Panel.AssetBrowser";

void collectAssetBrowserProjectPaths(const QJsonArray& items,
                                     QSet<QString>& assetPaths)
{
  for (const auto& value : items) {
    const QJsonObject item = value.toObject();
    if (item.value(QStringLiteral("type")).toString().compare(
            QStringLiteral("footage"), Qt::CaseInsensitive) == 0) {
      const QString path = item.value(QStringLiteral("filePath")).toString().trimmed();
      if (!path.isEmpty()) {
        assetPaths.insert(path);
      }
    }
    collectAssetBrowserProjectPaths(item.value(QStringLiteral("children")).toArray(),
                                    assetPaths);
  }
}

void collectAssetBrowserReferences(const QJsonValue& value, const QString& key,
                                   QSet<QString>& referencedPaths)
{
  if (value.isObject()) {
    const QJsonObject object = value.toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
      collectAssetBrowserReferences(it.value(), it.key(), referencedPaths);
    }
    return;
  }
  if (value.isArray()) {
    for (const auto& child : value.toArray()) {
      collectAssetBrowserReferences(child, key, referencedPaths);
    }
    return;
  }
  if (!value.isString()) {
    return;
  }
  const QString normalizedKey = key.trimmed().toLower();
  if (!normalizedKey.contains(QStringLiteral("source")) &&
      !normalizedKey.contains(QStringLiteral("path")) &&
      !normalizedKey.contains(QStringLiteral("file"))) {
    return;
  }
  const QString path = value.toString().trimmed();
  if (path.isEmpty()) {
    return;
  }
  referencedPaths.insert(path);
  const QString fileName = QFileInfo(path).fileName();
  if (!fileName.isEmpty()) {
    referencedPaths.insert(fileName);
  }
}

QSet<QString> findUnusedAssetBrowserPaths(const QJsonObject& snapshot)
{
  QSet<QString> allAssetPaths;
  QSet<QString> referencedPaths;
  collectAssetBrowserProjectPaths(
      snapshot.value(QStringLiteral("projectItems")).toArray(), allAssetPaths);
  collectAssetBrowserReferences(snapshot.value(QStringLiteral("compositions")),
                                QStringLiteral("compositions"), referencedPaths);

  QSet<QString> unusedPaths;
  for (const QString& path : allAssetPaths) {
    const QString fileName = QFileInfo(path).fileName();
    if (referencedPaths.contains(path) ||
        (!fileName.isEmpty() && referencedPaths.contains(fileName))) {
      continue;
    }
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath().isEmpty()
        ? info.absoluteFilePath()
        : info.canonicalFilePath();
    unusedPaths.insert(QDir::cleanPath(path));
    unusedPaths.insert(QDir::cleanPath(canonicalPath));
  }
  return unusedPaths;
}

void applyAssetBrowserPanelPalette(QWidget* widget)
{
  if (!widget) {
    return;
  }

  QPalette pal = widget->palette();
  const auto& theme = ArtifactCore::currentDCCTheme();
  const QColor surface(18, 23, 29);
  const QColor base(12, 17, 22);
  pal.setColor(QPalette::Window, surface);
  pal.setColor(QPalette::Base, base);
  pal.setColor(QPalette::AlternateBase, QColor(24, 30, 37));
  pal.setColor(QPalette::WindowText, QColor(theme.textColor));
  pal.setColor(QPalette::Text, QColor(theme.textColor));
  pal.setColor(QPalette::Button, surface);
  pal.setColor(QPalette::ButtonText, QColor(theme.textColor));
  pal.setColor(QPalette::Mid, QColor(49, 58, 68));
  widget->setAutoFillBackground(true);
  widget->setPalette(pal);
}

void applyAssetBrowserFilterPalette(QAbstractButton* button)
{
  if (!button) {
    return;
  }
  const auto& theme = ArtifactCore::currentDCCTheme();
  QPalette pal = button->palette();
  const QColor text(theme.textColor);
  const QColor surface(theme.secondaryBackgroundColor);
  const QColor accent(theme.accentColor);
  pal.setColor(QPalette::Button, surface.darker(108));
  pal.setColor(QPalette::ButtonText, text);
  pal.setColor(QPalette::Highlight, accent);
  pal.setColor(QPalette::HighlightedText, QColor(Qt::white));
  button->setAutoFillBackground(true);
  button->setPalette(pal);
}

QFrame* makeAssetBrowserPanel(QWidget* parent = nullptr)
{
  auto* frame = new QFrame(parent);
  frame->setFrameShape(QFrame::StyledPanel);
  frame->setFrameShadow(QFrame::Plain);
  applyAssetBrowserPanelPalette(frame);
  return frame;
}

QString normalizeAssetPath(const QString& path)
{
  if (path.trimmed().isEmpty()) {
   return {};
  }
  const QFileInfo info(path);
  const QString canonical = info.canonicalFilePath();
  return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

QSize assetGridSizeForThumbnail(const int thumbnailPx)
{
  const int clamped = std::clamp(thumbnailPx, kAssetThumbnailMinPx,
                                 kAssetThumbnailMaxPx);
  return QSize(clamped + 30, clamped + 58);
}

void applyAssetBrowserViewMode(QListView* view, const QListView::ViewMode mode,
                               const int thumbnailPx)
{
  if (!view) {
    return;
  }

  const int clamped = std::clamp(thumbnailPx, kAssetThumbnailMinPx,
                                 kAssetThumbnailMaxPx);
  view->setViewMode(mode);
  if (mode == QListView::ListMode) {
    view->setIconSize(QSize(32, 32));
    view->setFlow(QListView::TopToBottom);
    view->setWrapping(false);
    view->setGridSize(QSize());
    view->setWordWrap(true);
    view->setSpacing(0);
  } else {
    view->setIconSize(QSize(clamped, clamped));
    view->setFlow(QListView::LeftToRight);
    view->setWrapping(true);
    view->setGridSize(assetGridSizeForThumbnail(clamped));
    view->setWordWrap(true);
    view->setSpacing(6);
  }
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
 const QString root = QDir::cleanPath(rootPath_);
 const QString current = QDir::cleanPath(currentPath_.isEmpty() ? rootPath_ : currentPath_);
 QStringList parts;
 QString path;
 if (!root.isEmpty() && current.startsWith(root, Qt::CaseInsensitive)) {
  const QString relative = QDir(root).relativeFilePath(current);
  if (relative != QStringLiteral(".")) {
   parts = relative.split(QDir::separator(), Qt::SkipEmptyParts);
  }
 } else {
  parts = current.split(QDir::separator(), Qt::SkipEmptyParts);
 }
 auto addSeparator = [this]() {
  auto* separator = new QLabel(QStringLiteral("/"), owner_);
  separator->setObjectName(QStringLiteral("artifactBreadcrumbSeparator"));
  layout_->addWidget(separator);
 };
 auto configureButton = [](QToolButton* button) {
  button->setAutoRaise(true);
  button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  button->setCursor(Qt::PointingHandCursor);
 };
 bool needsSeparator = false;
 if (!root.isEmpty()) {
  auto* rootButton = new QToolButton(owner_);
  rootButton->setText(QFileInfo(root).fileName().isEmpty() ? root : QFileInfo(root).fileName());
  configureButton(rootButton);
  QObject::connect(rootButton, &QToolButton::clicked, owner_, [this, root]() {
   Q_EMIT static_cast<ArtifactBreadcrumbWidget*>(owner_)->pathClicked(root);
  });
  layout_->addWidget(rootButton);
  path = root;
  needsSeparator = true;
 }
 for (const QString& part : parts) {
  if (needsSeparator) {
   addSeparator();
  }
  if (!path.isEmpty()) {
   path += QDir::separator();
  }
  path += part;
  auto* button = new QToolButton(owner_);
  button->setText(part);
  configureButton(button);
  QObject::connect(button, &QToolButton::clicked, owner_, [this, path]() {
   Q_EMIT static_cast<ArtifactBreadcrumbWidget*>(owner_)->pathClicked(path);
  });
  layout_->addWidget(button);
  needsSeparator = true;
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

class ArtifactAssetBrowserToolBar::Impl
{
 private:
 public:
  Impl();
  ~Impl();
  QLineEdit* searchWidget = nullptr;
  QToolButton* gridViewButton = nullptr;
  QToolButton* listViewButton = nullptr;
};

ArtifactAssetBrowserToolBar::Impl::Impl()
{
  searchWidget = new QLineEdit();
  gridViewButton = new QToolButton();
  listViewButton = new QToolButton();
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
  upButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
  upButton->setFixedSize(28, 28);
  upButton->setToolTip(QStringLiteral("Go to parent folder"));
  upButton->setAccessibleName(QStringLiteral("Parent folder"));
  upButton->setAccessibleDescription(QStringLiteral("Go to the parent folder"));
  auto refreshButton = new QToolButton(this);
  refreshButton->setObjectName(QStringLiteral("assetBrowserRefreshButton"));
  refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
  refreshButton->setFixedSize(28, 28);
  refreshButton->setToolTip(QStringLiteral("Refresh current folder"));
  refreshButton->setAccessibleName(QStringLiteral("Refresh folder"));
  refreshButton->setAccessibleDescription(QStringLiteral("Refresh the current folder contents"));
  impl_->gridViewButton->setObjectName(QStringLiteral("assetBrowserGridViewButton"));
  impl_->gridViewButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogContentsView));
  impl_->gridViewButton->setFixedSize(30, 28);
  impl_->gridViewButton->setToolTip(QStringLiteral("Show assets in grid view"));
  impl_->gridViewButton->setAccessibleName(QStringLiteral("Grid view"));
  impl_->gridViewButton->setAccessibleDescription(QStringLiteral("Show assets as a grid"));
  impl_->gridViewButton->setCheckable(true);
  impl_->gridViewButton->setChecked(true);
  impl_->listViewButton->setObjectName(QStringLiteral("assetBrowserListViewButton"));
  impl_->listViewButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
  impl_->listViewButton->setFixedSize(30, 28);
  impl_->listViewButton->setToolTip(QStringLiteral("Show assets in list view"));
  impl_->listViewButton->setAccessibleName(QStringLiteral("List view"));
  impl_->listViewButton->setAccessibleDescription(QStringLiteral("Show assets as a list"));
  impl_->listViewButton->setCheckable(true);
  impl_->searchWidget->setPlaceholderText(QStringLiteral("Search assets"));
  impl_->searchWidget->setAccessibleName(QStringLiteral("Asset search"));
  impl_->searchWidget->setAccessibleDescription(QStringLiteral("Search assets in the current folder"));
  impl_->searchWidget->setClearButtonEnabled(true);
  impl_->searchWidget->setMinimumWidth(220);
  impl_->searchWidget->setMinimumHeight(30);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  // Discovery comes first: search, then presentation, then navigation.
  layout->addWidget(impl_->searchWidget, 1);
  layout->addWidget(impl_->gridViewButton);
  layout->addWidget(impl_->listViewButton);
  layout->addSpacing(4);
  layout->addWidget(upButton);
  layout->addWidget(refreshButton);
  setLayout(layout);
 }

 ArtifactAssetBrowserToolBar::~ArtifactAssetBrowserToolBar()
 {
  delete impl_;
 }

 void ArtifactAssetBrowserToolBar::addSeparator()
 {
  auto* bar = new QFrame(this);
  bar->setFrameShape(QFrame::VLine);
  bar->setFrameShadow(QFrame::Plain);
  bar->setObjectName(QStringLiteral("assetBrowserToolBarSeparator"));
  if (auto* layout = this->layout()) {
   layout->addWidget(bar);
  } else {
   bar->deleteLater();
  }
 }

void ArtifactAssetBrowserToolBar::addWidget(QWidget* widget, int stretch)
{
  if (!widget) {
    return;
  }
  if (auto* boxLayout = qobject_cast<QBoxLayout*>(this->layout())) {
   boxLayout->addWidget(widget, stretch);
  } else if (auto* layout = this->layout()) {
   layout->addWidget(widget);
  }
}

 class ArtifactAssetBrowser::Impl
 {
 private:
 QHash<QString, QIcon> thumbnailCache_;  // Cache thumbnails by file path
  QHash<QString, QDateTime> thumbnailCacheModified_;
  mutable std::mutex thumbnailMutex_;  // Protects thumbnail cache access
  QSize thumbnailSize_{kAssetThumbnailDefaultPx, kAssetThumbnailDefaultPx};
  QIcon defaultFileIcon_;
  QIcon defaultImageIcon_;
  QIcon defaultVideoIcon_;
  QIcon defaultAudioIcon_;
  QIcon defaultFontIcon_;
  QSet<QString> unusedAssetPaths_;
  mutable QSet<QString> importedAssetPathsCache_;
  mutable bool importedAssetPathsCacheValid_ = false;
  std::atomic<std::uint64_t> thumbnailGeneration_{0};

  // Async preview thumbnail generation for image / video files
  struct PendingPreviewJob {
    QString filePath;
    QFutureWatcher<QImage>* watcher = nullptr;
    std::uint64_t generation = 0;
  };
  QHash<QString, PendingPreviewJob> pendingPreviewJobs_;
  QSet<QString> failedPreviewPaths_;
  QHash<QString, QString> previewFailureReasons_;

  // Async waveform thumbnail generation
  struct PendingWaveJob {
    QString filePath;
    QFutureWatcher<QImage>* watcher = nullptr;
    std::uint64_t generation = 0;
  };
  QHash<QString, PendingWaveJob> pendingWaveJobs_;
  QSet<QString> failedWavePaths_;  // Don't retry failed loads
  HoverPreviewPopup* hoverPreviewPopup_ = nullptr;
  QTimer* hoverPreviewTimer_ = nullptr;
  QString hoverPreviewPath_;
  QPoint hoverPreviewGlobalPos_;

  // Optimized: partial audio loading for thumbnails
  // Only load first N seconds for waveform thumbnail
  static constexpr int kMaxThumbnailAudioSeconds = 30;
 public:
  Impl();
  ~Impl();
 QToolButton* upButton_ = nullptr;
 QToolButton* refreshButton_ = nullptr;
  ArtifactAssetBrowser* owner_ = nullptr;
 QTreeView* directoryView_ = nullptr;
  AssetDirectoryModel* directoryModel_ = nullptr;
  QListView* fileView_ = nullptr;
  AssetMenuModel* assetModel_ = nullptr;
  QLabel* syncStateLabel_ = nullptr;
  QLineEdit* searchEdit_ = nullptr;
  QFileSystemModel* fileModel_ = nullptr;
  QButtonGroup* filterButtonGroup_ = nullptr;
   ArtifactBreadcrumbWidget* breadcrumb_ = nullptr;
   QLabel* currentPathLabel_ = nullptr;
   QLabel* leftHubSummaryLabel_ = nullptr;
   QLabel* leftHubRecentLabel_ = nullptr;
   QLabel* leftHubSelectionLabel_ = nullptr;
   QVector<RecentFolderButton*> recentFolderButtons_;
   QLabel* filePreviewLabel_ = nullptr;
   QLabel* fileInfoLabel_ = nullptr;  // File details display
   QSlider* thumbnailSizeSlider_ = nullptr;  // Thumbnail size adjustment
    QString currentDirectoryPath_;
    QSet<QString> expandedSequencePaths_;
    QString currentFileTypeFilter_ = "all";
    QString currentStatusFilter_ = "all";
    QString currentSearchFilter_;
    QString currentSearchScope_ = QStringLiteral("current");
    QString currentSortBy_ = "date";  // name, date, size, type
    bool sortAscending_ = false;
    ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
    std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
    bool unusedAssetSnapshotQueued_ = false;
    bool unusedAssetRefreshInFlight_ = false;
    bool unusedAssetRefreshPending_ = false;
    quint64 unusedAssetRefreshGeneration_ = 0;

  void handleDirectryChanged();
  void handleDoubleClicked();
  void defaultHandleMousePressEvent(QMouseEvent* event);
 void applyFilters();
  void warmVisibleThumbnails();
  bool matchesFileTypeFilter(const QString& fileName) const;
  bool matchesSearchFilter(const QString& fileName) const;
  QIcon generateThumbnail(const QString& filePath);
  bool isCurrentThumbnail(const QString& filePath) const;
  bool hasCurrentThumbnail(const QString& filePath);
  void cacheThumbnail(const QString& filePath, const QIcon& icon);
  QIcon fileTypeIconFor(const QString& fileName) const;
  QIcon getFileIcon(const QString& fileName, const QString& filePath);
  void clearThumbnailCache();
  void startAsyncPreviewThumbnailGeneration(const QString& filePath);
  QString thumbnailDebugStatus(const QString& filePath) const;
  void showHoverPreview(const QString& filePath, const QPoint& globalPos);
  void hideHoverPreview();
  void scheduleHoverPreview(const QString& filePath, const QPoint& globalPos);
  bool isHoverPreviewPath(const QString& filePath) const { return filePath == hoverPreviewPath_; }
   bool isImageFile(const QString& fileName) const;
   bool isVideoFile(const QString& fileName) const;
   bool isAudioFile(const QString& fileName) const;
   bool isFontFile(const QString& fileName) const;
   QIcon generateAudioWaveformThumbnail(const QString& audioFilePath);
   ArtifactCore::AudioSegment loadAudioFile(const QString& audioFilePath);
   void startAsyncWaveformGeneration(const QString& audioFilePath);
   ArtifactCore::FileType fileType(const QString& fileName) const;
  bool isImportedAssetPath(const QString& filePath) const;
  void refreshImportedAssetPathCache() const;
  bool isFavoriteAssetPath(const QString& filePath) const;
  bool isUnusedAssetPath(const QString& filePath) const;
  int sourceUseCountForPath(const QString& filePath,
                            const QStringList& sequencePaths = {}) const;
  bool isMissingAssetPath(const QString& filePath) const;
  // Shared status aggregation used by row markers, the info/preview pane,
  // status filtering, and post-import/relink refresh.
  struct AssetStatusSummary {
   bool favorite = false;
   bool imported = false;
   bool unused = false;
   bool missing = false;
  };
  AssetStatusSummary assetStatusForPaths(const QString& filePath,
                                         const QStringList& sequencePaths = {}) const;
  QStringList assetStatusMarkers(const AssetStatusSummary& status) const;
  QString assetStatusInfoHtml(const AssetStatusSummary& status) const;
  bool matchesStatusFilter(const AssetStatusSummary& status) const;
  bool findAssetItemByPath(const QString& filePath, AssetMenuItem* outItem) const;
  void toggleFavoritePath(const QString& filePath);
  QStringList selectedAssetPaths() const;
  void syncProjectAssetRoot();
  void syncDirectorySelection();
  void refreshUnusedAssetCache();
  void refreshLeftHubSummary();
  QString syncStateText() const;
   int thumbnailSizePx() const;
   void setThumbnailSizePx(int value);
  QFileSystemWatcher* fsWatcher_ = nullptr;
  bool watchScheduled_ = false;
   void setupFileSystemWatcher();
   void watchCurrentDirectory();
   void handleFileRenamed(const QString& oldPath, const QString& newPath);
   void handleFileDeleted(const QString& path);
   void createNewFolder();
   void renameSelected();
   void deleteSelected();
   QString promptNewName(const QString& currentName) const;
   QString promptNewFolderName() const;
   bool confirmDelete(const QStringList& paths) const;
   };

ArtifactAssetBrowser::Impl::Impl()
{
  // Initialize default icons using Qt standard icons
  QStyle* style = QApplication::style();
  if (style) {
   defaultFileIcon_ = style->standardIcon(QStyle::SP_FileIcon);
   defaultImageIcon_ = QIcon(QStringLiteral(":/icons/Studio/asset_file_image.svg"));
   if (defaultImageIcon_.isNull()) {
    defaultImageIcon_ = style->standardIcon(QStyle::SP_FileIcon);
   }
   defaultVideoIcon_ = QIcon(QStringLiteral(":/icons/Studio/asset_file_video.svg"));
   if (defaultVideoIcon_.isNull()) {
    defaultVideoIcon_ = style->standardIcon(QStyle::SP_MediaPlay);
   }
   defaultAudioIcon_ = QIcon(QStringLiteral(":/icons/Studio/asset_file_audio.svg"));
   if (defaultAudioIcon_.isNull()) {
    defaultAudioIcon_ = style->standardIcon(QStyle::SP_MediaVolume);
   }
   defaultFontIcon_ = style->standardIcon(QStyle::SP_FileDialogDetailedView);
  }
  hoverPreviewTimer_ = new QTimer();
  hoverPreviewTimer_->setSingleShot(true);
  hoverPreviewPopup_ = new HoverPreviewPopup();
}

ArtifactAssetBrowser::Impl::~Impl()
{
  thumbnailGeneration_.fetch_add(1, std::memory_order_relaxed);
  delete hoverPreviewTimer_;
  delete hoverPreviewPopup_;

  for (auto it = pendingPreviewJobs_.begin(); it != pendingPreviewJobs_.end(); ++it) {
    if (auto* watcher = it.value().watcher) {
      QObject::disconnect(watcher, nullptr, nullptr, nullptr);
      watcher->cancel();
      watcher->waitForFinished();
      delete watcher;
    }
  }
  pendingPreviewJobs_.clear();

  for (auto it = pendingWaveJobs_.begin(); it != pendingWaveJobs_.end(); ++it) {
    if (auto* watcher = it.value().watcher) {
      QObject::disconnect(watcher, nullptr, nullptr, nullptr);
      watcher->cancel();
      watcher->waitForFinished();
      delete watcher;
    }
  }
  pendingWaveJobs_.clear();
}

 int ArtifactAssetBrowser::Impl::thumbnailSizePx() const
 {
  return thumbnailSize_.width();
 }

 void ArtifactAssetBrowser::Impl::setThumbnailSizePx(int value)
 {
  const int clamped = std::clamp(value, kAssetThumbnailMinPx, kAssetThumbnailMaxPx);
  thumbnailSize_ = QSize(clamped, clamped);
 }

 void ArtifactAssetBrowser::Impl::handleDoubleClicked()
 {
  if (!owner_ || !fileView_ || !assetModel_ || !fileView_->selectionModel()) {
   return;
  }

  const QModelIndex index = fileView_->selectionModel()->currentIndex();
  if (!index.isValid()) {
   return;
  }

  const AssetMenuItem item = assetModel_->itemAt(index.row());
  const QString path = item.path.toQString();
  if (path.isEmpty()) {
   return;
  }

  if (item.isFolder) {
   owner_->navigateToFolder(path);
   return;
  }

  // Browsing and importing are deliberately separate actions. Double-click
  // only selects the source and refreshes the metadata/preview surface;
  // import remains an explicit toolbar or context-menu action.
  owner_->updateFileInfo(path);
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
          lower.endsWith(".jpeg") || lower.endsWith(".jpe") ||
          lower.endsWith(".jfif") || lower.endsWith(".bmp") ||
          lower.endsWith(".gif") || lower.endsWith(".tga") ||
          lower.endsWith(".tif") || lower.endsWith(".tiff") ||
          lower.endsWith(".hdr") || lower.endsWith(".exr") ||
          lower.endsWith(".webp") || lower.endsWith(".ico") ||
          lower.endsWith(".dds") || lower.endsWith(".ktx") ||
          lower.endsWith(".psd") || lower.endsWith(".psb");
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
  else if (currentFileTypeFilter_ == "3d") {
   return lower.endsWith(".fbx") || lower.endsWith(".obj") ||
          lower.endsWith(".gltf") || lower.endsWith(".glb") ||
          lower.endsWith(".pmd") || lower.endsWith(".ply") ||
          lower.endsWith(".las") || lower.endsWith(".usd") ||
          lower.endsWith(".usda") ||
          lower.endsWith(".usdc") || lower.endsWith(".abc") ||
          lower.endsWith(".stl");
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

void ArtifactAssetBrowser::Impl::refreshImportedAssetPathCache() const
{
  importedAssetPathsCache_.clear();
  auto* svc = ArtifactProjectService::instance();
  if (!svc) {
    importedAssetPathsCacheValid_ = true;
    return;
  }

  const auto normalizePath = [](const QString& path) {
    if (path.trimmed().isEmpty()) return QString();
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
  };
  std::function<void(ProjectItem*)> collectPaths = [&](ProjectItem* item) {
    if (!item) return;
    if (item->type() == eProjectItemType::Footage) {
      const auto* footage = static_cast<const FootageItem*>(item);
      const QString normalizedPath = normalizePath(footage->filePath);
      if (!normalizedPath.isEmpty()) importedAssetPathsCache_.insert(normalizedPath);
      for (const QString& sequencePath : footage->sequencePaths) {
        const QString normalizedSequencePath = normalizePath(sequencePath);
        if (!normalizedSequencePath.isEmpty()) {
          importedAssetPathsCache_.insert(normalizedSequencePath);
        }
      }
    }
    for (auto* child : item->children) collectPaths(child);
  };
  for (auto* root : svc->projectItems()) collectPaths(root);
  importedAssetPathsCacheValid_ = true;
}

bool ArtifactAssetBrowser::Impl::isImportedAssetPath(const QString& filePath) const
{
  if (filePath.trimmed().isEmpty()) return false;
  if (!importedAssetPathsCacheValid_) refreshImportedAssetPathCache();
  const QFileInfo info(filePath);
  const QString canonical = info.canonicalFilePath();
  const QString normalizedPath = QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
  return importedAssetPathsCache_.contains(normalizedPath);
}

bool ArtifactAssetBrowser::Impl::isFavoriteAssetPath(const QString& filePath) const
{
  return directoryModel_ && directoryModel_->isFavoritePath(filePath);
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
    if (item.isSequence && !item.sequencePaths.isEmpty()) {
     for (const QString& sequencePath : item.sequencePaths) {
      if (!sequencePath.isEmpty()) {
       paths.append(sequencePath);
      }
     }
    } else if (item.isSequenceFrame && !item.sequenceParentPath.isEmpty()) {
     const QString parentPath = item.sequenceParentPath;
     for (int row = 0; row < assetModel_->rowCount(); ++row) {
      const AssetMenuItem parent = assetModel_->itemAt(row);
      if (parent.isSequence && parent.path.toQString() == parentPath) {
       for (const QString& sequencePath : parent.sequencePaths) {
        if (!sequencePath.isEmpty()) paths.append(sequencePath);
       }
       break;
      }
     }
    } else {
     const QString path = item.path.toQString();
     if (!path.isEmpty()) {
      paths.append(path);
     }
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

int ArtifactAssetBrowser::Impl::sourceUseCountForPath(
    const QString& filePath, const QStringList& sequencePaths) const
{
  if (filePath.trimmed().isEmpty() || !isImportedAssetPath(filePath)) {
    return 0;
  }

  auto& assetManager = ArtifactCore::AssetManager::instance();
  const auto countForPath = [&assetManager](const QString& candidate) {
    const QString trimmed = candidate.trimmed();
    if (trimmed.isEmpty()) {
      return 0;
    }
    QUuid sourceId = assetManager.sourceId(trimmed);
    if (sourceId.isNull()) {
      const QString absolutePath = QFileInfo(trimmed).absoluteFilePath();
      if (absolutePath != trimmed) {
        sourceId = assetManager.sourceId(absolutePath);
      }
    }
    return sourceId.isNull() ? 0 : assetManager.useCount(sourceId);
  };

  int useCount = countForPath(filePath);
  for (const QString& sequencePath : sequencePaths) {
    useCount = std::max(useCount, countForPath(sequencePath));
  }
  return useCount;
}

bool ArtifactAssetBrowser::Impl::isMissingAssetPath(const QString& filePath) const
{
  if (filePath.isEmpty()) {
   return false;
  }
 return !QFileInfo::exists(filePath);
}

ArtifactAssetBrowser::Impl::AssetStatusSummary
ArtifactAssetBrowser::Impl::assetStatusForPaths(const QString& filePath,
                                                const QStringList& sequencePaths) const
{
  // Aggregation rule shared everywhere: favorite/imported/unused require
  // every frame, missing triggers on any frame.
  AssetStatusSummary status;
  const QStringList paths = sequencePaths.isEmpty() ? QStringList{filePath} : sequencePaths;
  status.favorite = true;
  status.imported = true;
  status.unused = true;
  status.missing = false;
  for (const QString& path : paths) {
   if (!isFavoriteAssetPath(path)) status.favorite = false;
   if (!isImportedAssetPath(path)) status.imported = false;
   if (!isUnusedAssetPath(path)) status.unused = false;
   if (isMissingAssetPath(path)) status.missing = true;
  }
  return status;
}

QStringList ArtifactAssetBrowser::Impl::assetStatusMarkers(const AssetStatusSummary& status) const
{
  QStringList markers;
  if (status.favorite) markers.append(QStringLiteral("Favorite"));
  if (status.missing) markers.append(QStringLiteral("Missing"));
  if (status.imported) markers.append(QStringLiteral("Imported"));
  if (status.unused) markers.append(QStringLiteral("Unused"));
  return markers;
}

QString ArtifactAssetBrowser::Impl::assetStatusInfoHtml(const AssetStatusSummary& status) const
{
  QString info;
  info += QStringLiteral("Favorite: %1<br>").arg(status.favorite ? QStringLiteral("Yes") : QStringLiteral("No"));
  info += QStringLiteral("Project: %1<br>").arg(status.imported ? QStringLiteral("Imported") : QStringLiteral("Not Imported"));
  info += QStringLiteral("Usage: %1<br>").arg(status.unused ? QStringLiteral("Unused") : QStringLiteral("In Use / N.A."));
  info += QStringLiteral("Status: %1<br>").arg(status.missing ? QStringLiteral("Missing") : QStringLiteral("OK"));
  return info;
}

bool ArtifactAssetBrowser::Impl::matchesStatusFilter(const AssetStatusSummary& status) const
{
  if (currentStatusFilter_ == QStringLiteral("all")) return true;
  if (currentStatusFilter_ == QStringLiteral("imported")) return status.imported;
  if (currentStatusFilter_ == QStringLiteral("favorite")) return status.favorite;
  if (currentStatusFilter_ == QStringLiteral("missing")) return status.missing;
  if (currentStatusFilter_ == QStringLiteral("unused")) return status.unused;
  return true;
}

bool ArtifactAssetBrowser::Impl::findAssetItemByPath(const QString& filePath, AssetMenuItem* outItem) const
{
  if (!assetModel_ || filePath.isEmpty()) {
   return false;
  }
  const int rows = assetModel_->rowCount();
  for (int row = 0; row < rows; ++row) {
   const AssetMenuItem item = assetModel_->itemAt(row);
   if (item.path.toQString() == filePath ||
       (item.isSequence && item.sequencePaths.contains(filePath))) {
    if (outItem) *outItem = item;
    return true;
   }
  }
  return false;
}

void ArtifactAssetBrowser::Impl::toggleFavoritePath(const QString& filePath)
{
  if (!directoryModel_ || filePath.trimmed().isEmpty()) {
   return;
  }

  if (directoryModel_->isFavoritePath(filePath)) {
   const QString guid = directoryModel_->favoriteGuidForPath(filePath);
   if (!guid.isEmpty()) {
    directoryModel_->removeFavorite(guid);
   }
   return;
  }

  const QFileInfo info(filePath);
  const QString displayName = info.fileName().isEmpty() ? filePath : info.fileName();
  directoryModel_->addFavorite(filePath, displayName);
}

QString ArtifactAssetBrowser::Impl::syncStateText() const
{
 auto* svc = ArtifactProjectService::instance();
  return svc ? QStringLiteral("Status: Project linked") : QStringLiteral("Status: Open a folder to browse assets");
}

QIcon ArtifactAssetBrowser::Impl::fileTypeIconFor(const QString& fileName) const
{
 const QString suffix = QFileInfo(fileName).suffix().toLower();
 static const QHash<QString, QString> iconBySuffix = {
  {QStringLiteral("jpg"), QStringLiteral("asset_file_jpeg.svg")},
  {QStringLiteral("jpeg"), QStringLiteral("asset_file_jpeg.svg")},
  {QStringLiteral("jpe"), QStringLiteral("asset_file_jpeg.svg")},
  {QStringLiteral("jfif"), QStringLiteral("asset_file_jpeg.svg")},
  {QStringLiteral("png"), QStringLiteral("asset_file_png.svg")},
  {QStringLiteral("exr"), QStringLiteral("asset_file_exr.svg")},
  {QStringLiteral("webp"), QStringLiteral("asset_file_webp.svg")},
  {QStringLiteral("gif"), QStringLiteral("asset_file_gif.svg")},
  {QStringLiteral("tif"), QStringLiteral("asset_file_tiff.svg")},
  {QStringLiteral("tiff"), QStringLiteral("asset_file_tiff.svg")},
  {QStringLiteral("psd"), QStringLiteral("asset_file_psd.svg")},
  {QStringLiteral("psb"), QStringLiteral("asset_file_psd.svg")},
  {QStringLiteral("ai"), QStringLiteral("asset_file_ai.svg")},
  {QStringLiteral("eps"), QStringLiteral("asset_file_eps.svg")},
  {QStringLiteral("svg"), QStringLiteral("asset_file_svg.svg")},
  {QStringLiteral("pdf"), QStringLiteral("asset_file_pdf.svg")},
  {QStringLiteral("aep"), QStringLiteral("asset_file_aep.svg")},
  {QStringLiteral("aepx"), QStringLiteral("asset_file_aep.svg")},
  {QStringLiteral("mp4"), QStringLiteral("asset_file_mp4.svg")},
  {QStringLiteral("mov"), QStringLiteral("asset_file_mov.svg")},
  {QStringLiteral("avi"), QStringLiteral("asset_file_avi.svg")},
  {QStringLiteral("mkv"), QStringLiteral("asset_file_mkv.svg")},
  {QStringLiteral("wav"), QStringLiteral("asset_file_wav.svg")},
  {QStringLiteral("mp3"), QStringLiteral("asset_file_mp3.svg")},
  {QStringLiteral("aac"), QStringLiteral("asset_file_aac.svg")},
  {QStringLiteral("m4a"), QStringLiteral("asset_file_aac.svg")},
  {QStringLiteral("flac"), QStringLiteral("asset_file_flac.svg")},
  {QStringLiteral("obj"), QStringLiteral("asset_file_obj.svg")},
  {QStringLiteral("fbx"), QStringLiteral("asset_file_fbx.svg")},
  {QStringLiteral("glb"), QStringLiteral("asset_file_gltf.svg")},
  {QStringLiteral("gltf"), QStringLiteral("asset_file_gltf.svg")},
  {QStringLiteral("abc"), QStringLiteral("asset_file_abc.svg")},
  {QStringLiteral("stl"), QStringLiteral("asset_file_stl.svg")},
  {QStringLiteral("usd"), QStringLiteral("asset_file_usd.svg")},
  {QStringLiteral("usda"), QStringLiteral("asset_file_usd.svg")},
  {QStringLiteral("usdc"), QStringLiteral("asset_file_usd.svg")},
  {QStringLiteral("usdz"), QStringLiteral("asset_file_usd.svg")},
  {QStringLiteral("blend"), QStringLiteral("asset_file_blend.svg")},
  {QStringLiteral("c4d"), QStringLiteral("asset_file_c4d.svg")},
  {QStringLiteral("ma"), QStringLiteral("asset_file_maya.svg")},
  {QStringLiteral("mb"), QStringLiteral("asset_file_maya.svg")},
  {QStringLiteral("hip"), QStringLiteral("asset_file_hip.svg")},
  {QStringLiteral("hiplc"), QStringLiteral("asset_file_hip.svg")},
  {QStringLiteral("hipnc"), QStringLiteral("asset_file_hip.svg")},
  {QStringLiteral("ztl"), QStringLiteral("asset_file_zbrush.svg")},
  {QStringLiteral("zpr"), QStringLiteral("asset_file_zbrush.svg")},
  {QStringLiteral("kra"), QStringLiteral("asset_file_kra.svg")},
  {QStringLiteral("krz"), QStringLiteral("asset_file_kra.svg")},
  {QStringLiteral("clip"), QStringLiteral("asset_file_clip.svg")},
  {QStringLiteral("ase"), QStringLiteral("asset_file_aseprite.svg")},
  {QStringLiteral("aseprite"), QStringLiteral("asset_file_aseprite.svg")}
 };

 QString iconName = iconBySuffix.value(suffix);
 if (!iconName.isEmpty()) {
  iconName.replace(QStringLiteral(".svg"), QStringLiteral("_ext.svg"));
 }
 if (!iconName.isEmpty()) {
  const QIcon icon(QStringLiteral(":/icons/Studio/%1").arg(iconName));
  if (!icon.isNull()) {
   return icon;
  }
 }
 if (isImageFile(fileName)) {
  return defaultImageIcon_;
 }
 if (isVideoFile(fileName)) {
  return defaultVideoIcon_;
 }
 if (isAudioFile(fileName)) {
  return defaultAudioIcon_;
 }
 static const QSet<QString> modelSuffixes = {
  QStringLiteral("obj"),
  QStringLiteral("fbx"),
  QStringLiteral("glb"),
  QStringLiteral("gltf"),
  QStringLiteral("pmd"),
  QStringLiteral("abc"),
  QStringLiteral("stl"),
  QStringLiteral("usd"),
  QStringLiteral("usda"),
  QStringLiteral("usdc"),
  QStringLiteral("usdz")
 };
 if (modelSuffixes.contains(suffix)) {
  const QIcon icon(QStringLiteral(":/icons/Studio/asset_file_3d.svg"));
  if (!icon.isNull()) {
   return icon;
  }
 }
 return defaultFileIcon_;
}

bool ArtifactAssetBrowser::Impl::isCurrentThumbnail(const QString& filePath) const
{
  if (!thumbnailCache_.contains(filePath)) return false;
  return thumbnailCacheModified_.value(filePath) == QFileInfo(filePath).lastModified();
}

bool ArtifactAssetBrowser::Impl::hasCurrentThumbnail(const QString& filePath)
{
  if (!isCurrentThumbnail(filePath)) {
    thumbnailCache_.remove(filePath);
    thumbnailCacheModified_.remove(filePath);
    return false;
  }
  return true;
}

void ArtifactAssetBrowser::Impl::cacheThumbnail(const QString& filePath, const QIcon& icon)
{
  constexpr int kMaxMemoryThumbnailEntries = 512;
  thumbnailCache_[filePath] = icon;
  thumbnailCacheModified_[filePath] = QFileInfo(filePath).lastModified();
  while (thumbnailCache_.size() > kMaxMemoryThumbnailEntries) {
    auto it = thumbnailCache_.constBegin();
    if (it == thumbnailCache_.constEnd()) break;
    const QString evictedPath = it.key();
    thumbnailCache_.remove(evictedPath);
    thumbnailCacheModified_.remove(evictedPath);
  }
}

 QIcon ArtifactAssetBrowser::Impl::generateThumbnail(const QString& filePath)
 {
  std::unique_lock<std::mutex> lock(thumbnailMutex_);
  // Check cache first
  if (hasCurrentThumbnail(filePath)) {
   return thumbnailCache_[filePath];
  }

  const std::uint64_t currentGeneration = thumbnailGeneration_.load(std::memory_order_relaxed);

  QFileInfo fileInfo(filePath);

  if (const QIcon diskIcon = AssetThumbnail::loadFromDisk(fileInfo);
      !diskIcon.isNull()) {
    cacheThumbnail(filePath, diskIcon);
    return diskIcon;
  }

  // For folders, use folder icon
  if (fileInfo.isDir()) {
   QStyle* style = QApplication::style();
   if (style) {
    QIcon folderIcon = style->standardIcon(QStyle::SP_DirIcon);
    cacheThumbnail(filePath, folderIcon);
    return folderIcon;
   }
   return defaultFileIcon_;
  }

  // Generate thumbnail for image files
  if (isImageFile(fileInfo.fileName())) {
   const QIcon placeholder = fileTypeIconFor(fileInfo.fileName());
   if (auto it = pendingPreviewJobs_.find(filePath); it != pendingPreviewJobs_.end()) {
    if (it.value().generation == currentGeneration) {
     return placeholder;
    }
    auto* staleWatcher = it.value().watcher;
    pendingPreviewJobs_.erase(it);
    discardStaleThumbnailWatcher(staleWatcher);
   }
   if (failedPreviewPaths_.contains(filePath)) {
    return placeholder;
   }
   lock.unlock();
   startAsyncPreviewThumbnailGeneration(filePath);
   return placeholder;
  }

  // Extract first frame as thumbnail for video files
  if (isVideoFile(fileInfo.fileName())) {
      const QIcon placeholder = fileTypeIconFor(fileInfo.fileName());
      if (auto it = pendingPreviewJobs_.find(filePath); it != pendingPreviewJobs_.end()) {
       if (it.value().generation == currentGeneration) {
        return placeholder;
       }
       auto* staleWatcher = it.value().watcher;
       pendingPreviewJobs_.erase(it);
       discardStaleThumbnailWatcher(staleWatcher);
      }
      if (failedPreviewPaths_.contains(filePath)) {
       return placeholder;
      }
      lock.unlock();
      startAsyncPreviewThumbnailGeneration(filePath);
      return placeholder;
  }

  // For audio files, generate waveform thumbnail
  if (isAudioFile(fileInfo.fileName())) {
   QIcon waveIcon = generateAudioWaveformThumbnail(filePath);
   if (!waveIcon.isNull()) {
    cacheThumbnail(filePath, waveIcon);
    return waveIcon;
   }
   // Fallback to default audio icon
   const QIcon placeholder = fileTypeIconFor(fileInfo.fileName());
   cacheThumbnail(filePath, placeholder);
   return placeholder;
  }

  if (isFontFile(fileInfo.fileName())) {
   cacheThumbnail(filePath, defaultFontIcon_);
   return defaultFontIcon_;
  }

  // Ask the platform for a real thumbnail before falling back to the
  // application-owned file type icon.
  const QIcon placeholder = fileTypeIconFor(fileInfo.fileName());
  if (auto it = pendingPreviewJobs_.find(filePath); it != pendingPreviewJobs_.end()) {
   if (it.value().generation == currentGeneration) {
    return placeholder;
   }
   auto* staleWatcher = it.value().watcher;
   pendingPreviewJobs_.erase(it);
   discardStaleThumbnailWatcher(staleWatcher);
  }
  if (!failedPreviewPaths_.contains(filePath)) {
   cacheThumbnail(filePath, placeholder);
   lock.unlock();
   startAsyncPreviewThumbnailGeneration(filePath);
   return placeholder;
  }
  cacheThumbnail(filePath, placeholder);
  return placeholder;
 }

 QIcon ArtifactAssetBrowser::Impl::getFileIcon(const QString& fileName, const QString& filePath)
 {
  return generateThumbnail(filePath);
 }

void ArtifactAssetBrowser::Impl::clearThumbnailCache()
{
  thumbnailGeneration_.fetch_add(1, std::memory_order_relaxed);
  importedAssetPathsCacheValid_ = false;
  {
    std::lock_guard<std::mutex> lock(thumbnailMutex_);
    thumbnailCache_.clear();
    thumbnailCacheModified_.clear();
    failedPreviewPaths_.clear();
    failedWavePaths_.clear();
    previewFailureReasons_.clear();

    for (auto it = pendingPreviewJobs_.begin(); it != pendingPreviewJobs_.end(); ++it) {
      discardStaleThumbnailWatcher(it.value().watcher);
    }
    pendingPreviewJobs_.clear();

    for (auto it = pendingWaveJobs_.begin(); it != pendingWaveJobs_.end(); ++it) {
      discardStaleThumbnailWatcher(it.value().watcher);
    }
    pendingWaveJobs_.clear();
  }
  if (fileView_) {
    fileView_->update();
  }
}

void ArtifactAssetBrowser::Impl::startAsyncPreviewThumbnailGeneration(const QString& filePath)
{
  if (!owner_) {
    return;
  }

  if (QThread::currentThread() != owner_->thread()) {
    QTimer::singleShot(0, owner_, [this, filePath]() {
      startAsyncPreviewThumbnailGeneration(filePath);
    });
    return;
  }

  {
    std::lock_guard<std::mutex> lock(thumbnailMutex_);
    if (pendingPreviewJobs_.contains(filePath)) {
      const auto existing = pendingPreviewJobs_.value(filePath);
      if (existing.generation == thumbnailGeneration_.load(std::memory_order_relaxed)) {
        return;
      }
      const auto staleJob = pendingPreviewJobs_.take(filePath);
      discardStaleThumbnailWatcher(staleJob.watcher);
    }
    previewFailureReasons_.remove(filePath);
  }

  const quint64 jobGeneration = thumbnailGeneration_.load(std::memory_order_relaxed);
  auto* watcher = new QFutureWatcher<QImage>();
  QObject::connect(watcher, &QFutureWatcher<QImage>::finished, [this, watcher, filePath, jobGeneration]() {
    const QImage image = watcher->result();
    {
      std::lock_guard<std::mutex> lock(thumbnailMutex_);
      pendingPreviewJobs_.remove(filePath);
    }
    if (jobGeneration != thumbnailGeneration_.load(std::memory_order_relaxed)) {
      watcher->deleteLater();
      return;
    }
    if (!image.isNull()) {
      const QIcon icon(QPixmap::fromImage(image));
      AssetThumbnail::saveToDisk(QFileInfo(filePath), image);
      {
        std::lock_guard<std::mutex> lock(thumbnailMutex_);
        cacheThumbnail(filePath, icon);
        failedPreviewPaths_.remove(filePath);
        previewFailureReasons_.remove(filePath);
      }
      if (assetModel_ && assetModel_->updateItemIconByPath(filePath, icon)) {
        // model updated via dataChanged
      } else if (fileView_) {
        fileView_->update();
      }
      if (owner_ && fileView_ && fileView_->selectionModel()) {
        const QModelIndexList selectedIndexes = fileView_->selectionModel()->selectedIndexes();
        for (const QModelIndex& index : selectedIndexes) {
          const AssetMenuItem item = assetModel_->itemAt(index.row());
          if (item.path.toQString() == filePath) {
            owner_->updateFileInfo(filePath);
            break;
          }
        }
      }
    } else {
      QFileInfo fileInfo(filePath);
      const QString failureReason =
          QStringLiteral("Async thumbnail decode returned empty image for %1.")
              .arg(fileInfo.suffix().toUpper());
      {
        std::lock_guard<std::mutex> lock(thumbnailMutex_);
        failedPreviewPaths_.insert(filePath);
        previewFailureReasons_[filePath] = failureReason;
      }
      qWarning().noquote() << "[AssetBrowser][Thumbnail]" << filePath << failureReason;
      if (owner_ && fileView_ && fileView_->selectionModel()) {
        const QModelIndexList selectedIndexes = fileView_->selectionModel()->selectedIndexes();
        for (const QModelIndex& index : selectedIndexes) {
          const AssetMenuItem item = assetModel_->itemAt(index.row());
          if (item.path.toQString() == filePath) {
            owner_->updateFileInfo(filePath);
            break;
          }
        }
      }
    }
    watcher->deleteLater();
  });

  const QSize thumbSize = thumbnailSize_;
  QFuture<QImage> future = QtConcurrent::run([filePath, thumbSize]() -> QImage {
    const QFileInfo fileInfo(filePath);
    const QString suffix = fileInfo.suffix().toLower();
    const auto isJpegExt = [&]() {
      return suffix == QStringLiteral("jpg")
          || suffix == QStringLiteral("jpeg")
          || suffix == QStringLiteral("jpe")
          || suffix == QStringLiteral("jfif");
    };
    const auto isImageExt = [&]() {
      return suffix == QStringLiteral("png")
          || isJpegExt()
          || suffix == QStringLiteral("bmp")
          || suffix == QStringLiteral("gif")
          || suffix == QStringLiteral("tga")
          || suffix == QStringLiteral("tif")
          || suffix == QStringLiteral("tiff")
          || suffix == QStringLiteral("hdr")
          || suffix == QStringLiteral("exr")
          || suffix == QStringLiteral("webp")
          || suffix == QStringLiteral("ico")
          || suffix == QStringLiteral("dds")
          || suffix == QStringLiteral("ktx")
          || suffix == QStringLiteral("psd")
          || suffix == QStringLiteral("psb");
    };
    const auto isVideoExt = [&]() {
      return suffix == QStringLiteral("mp4")
          || suffix == QStringLiteral("mov")
          || suffix == QStringLiteral("avi")
          || suffix == QStringLiteral("mkv")
          || suffix == QStringLiteral("webm")
          || suffix == QStringLiteral("flv");
    };

    if (isImageExt()) {
      QString oiioError;
      const bool preferQtReaderFirst = suffix == QStringLiteral("webp");
      QImage image;
      if (!preferQtReaderFirst) {
        image = AssetThumbnail::loadImageViaOIIO(filePath, thumbSize, &oiioError);
      }
      if (!image.isNull()) {
        return image;
      }

#ifdef _WIN32
      if (isJpegExt()) {
        QString wicError;
        image = AssetThumbnail::loadImageViaWIC(filePath, thumbSize, &wicError);
        if (!image.isNull()) {
          return image;
        }
      }
#endif

      QImageReader reader(filePath);
      reader.setAutoTransform(true);
      const QImage fallbackImage = reader.read();
      if (!fallbackImage.isNull()) {
        return fallbackImage.scaled(thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      }
#ifdef _WIN32
      if (!isJpegExt()) {
        QString wicError;
        image = AssetThumbnail::loadImageViaWIC(filePath, thumbSize, &wicError);
        if (!image.isNull()) {
          return image;
        }
      }
      QString shellError;
      image = AssetThumbnail::loadImageViaWindowsShell(filePath, thumbSize, &shellError);
      if (!image.isNull()) {
        return image;
      }
#endif
      return {};
    }

    if (isVideoExt()) {
      FFmpegThumbnailExtractor extractor;
      const auto result = extractor.extractThumbnail(UniString::fromQString(filePath));
      if (result.success && !result.image.isNull()) {
        return result.image.scaled(thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      }
      return {};
    }

#ifdef _WIN32
    QString shellError;
    return AssetThumbnail::loadImageViaWindowsShell(filePath, thumbSize, &shellError);
#else
    return {};
#endif
  });

  watcher->setFuture(future);
  {
    std::lock_guard<std::mutex> lock(thumbnailMutex_);
    pendingPreviewJobs_[filePath] = {filePath, watcher, jobGeneration};
  }
 }

QString ArtifactAssetBrowser::Impl::thumbnailDebugStatus(const QString& filePath) const
{
  std::lock_guard<std::mutex> lock(thumbnailMutex_);
  if (isCurrentThumbnail(filePath)) {
    return QStringLiteral("Ready");
  }
  if (pendingPreviewJobs_.contains(filePath)) {
    return QStringLiteral("Pending");
  }
  if (failedPreviewPaths_.contains(filePath)) {
    const QString failureReason = previewFailureReasons_.value(filePath);
    if (!failureReason.isEmpty()) {
      return QStringLiteral("Failed (%1)").arg(failureReason);
    }
    return QStringLiteral("Failed");
  }
  return QStringLiteral("Placeholder");
}

void ArtifactAssetBrowser::Impl::syncProjectAssetRoot()
{
  if (!directoryModel_) return;

  QString assetsPath = ArtifactProjectService::instance()
                           ? ArtifactProjectService::instance()->currentProjectAssetsPath()
                           : QString();
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

 if (breadcrumb_) {
   breadcrumb_->setRootPath(assetsPath);
   breadcrumb_->setPath(currentDirectoryPath_);
  }

  refreshUnusedAssetCache();
  refreshLeftHubSummary();
  clearThumbnailCache();
  applyFilters();
  syncDirectorySelection();
}

void ArtifactAssetBrowser::Impl::refreshUnusedAssetCache()
{
  if (!owner_) {
    return;
  }
  if (unusedAssetRefreshInFlight_) {
    unusedAssetRefreshPending_ = true;
    return;
  }
  if (unusedAssetSnapshotQueued_) {
    return;
  }
  unusedAssetSnapshotQueued_ = true;
  QPointer<ArtifactAssetBrowser> owner(owner_);
  QMetaObject::invokeMethod(owner_, [owner]() {
    if (!owner || !owner->impl_) {
      return;
    }
    auto* impl = owner->impl_;
    impl->unusedAssetSnapshotQueued_ = false;
    auto* service = ArtifactProjectService::instance();
    auto project = service ? service->getCurrentProjectSharedPtr() : nullptr;
    const quint64 generation = ++impl->unusedAssetRefreshGeneration_;
    if (!project) {
      impl->unusedAssetPaths_.clear();
      impl->unusedAssetRefreshPending_ = false;
      impl->applyFilters();
      impl->refreshLeftHubSummary();
      return;
    }

    const QJsonObject snapshot = project->toJson();
    impl->unusedAssetRefreshInFlight_ = true;
    [[maybe_unused]] auto unusedAssetFuture =
        QtConcurrent::run([owner, generation, snapshot]() {
          QSet<QString> unusedPaths = findUnusedAssetBrowserPaths(snapshot);
          if (!owner) {
            return;
          }
          QMetaObject::invokeMethod(owner, [owner, generation,
                                            unusedPaths = std::move(unusedPaths)]() mutable {
            if (!owner || !owner->impl_) {
              return;
            }
            auto* impl = owner->impl_;
            impl->unusedAssetRefreshInFlight_ = false;
            if (generation == impl->unusedAssetRefreshGeneration_) {
              impl->unusedAssetPaths_ = std::move(unusedPaths);
              impl->applyFilters();
              impl->refreshLeftHubSummary();
            }
            const bool refreshAgain = impl->unusedAssetRefreshPending_;
            impl->unusedAssetRefreshPending_ = false;
            if (refreshAgain) {
              impl->refreshUnusedAssetCache();
            }
          }, Qt::QueuedConnection);
        });
  }, Qt::QueuedConnection);
}

void ArtifactAssetBrowser::Impl::hideHoverPreview()
{
  if (hoverPreviewTimer_) {
    hoverPreviewTimer_->stop();
  }
  if (hoverPreviewPopup_) {
    hoverPreviewPopup_->hide();
  }
  hoverPreviewPath_.clear();
}

void ArtifactAssetBrowser::Impl::showHoverPreview(const QString& filePath, const QPoint& globalPos)
{
  if (!hoverPreviewPopup_ || filePath.isEmpty()) {
    return;
  }
  const QFileInfo info(filePath);
  const QIcon icon = generateThumbnail(filePath);
  hoverPreviewPopup_->showFile(filePath, globalPos, icon, info);
}

void ArtifactAssetBrowser::Impl::scheduleHoverPreview(const QString& filePath, const QPoint& globalPos)
{
  if (!hoverPreviewTimer_) {
    return;
  }
  hoverPreviewPath_ = filePath;
  hoverPreviewGlobalPos_ = globalPos;
  hoverPreviewTimer_->stop();
  QObject::disconnect(hoverPreviewTimer_, nullptr, nullptr, nullptr);
  QObject::connect(hoverPreviewTimer_, &QTimer::timeout, [this]() {
    showHoverPreview(hoverPreviewPath_, hoverPreviewGlobalPos_);
  });
  hoverPreviewTimer_->start(300);
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

 void ArtifactAssetBrowser::Impl::refreshLeftHubSummary()
 {
  if (currentPathLabel_) {
   QString currentName;
   if (currentDirectoryPath_.isEmpty()) {
    currentName = QStringLiteral("(none)");
   } else {
    currentName = QFileInfo(currentDirectoryPath_).fileName();
    if (currentName.isEmpty()) {
     currentName = currentDirectoryPath_;
    }
   }
   currentPathLabel_->setText(QStringLiteral("Current: Library Hub / %1").arg(currentName));
   currentPathLabel_->setToolTip(currentDirectoryPath_.isEmpty()
                                     ? QStringLiteral("Select a folder to browse assets")
                                     : currentDirectoryPath_);
  }
  if (leftHubSummaryLabel_) {
   const int recentCount = directoryModel_ ? directoryModel_->recentEntries().size() : 0;
   const int favoriteCount = directoryModel_ ? directoryModel_->favoriteEntries().size() : 0;
   const int sourceCount =
       (directoryModel_ && directoryModel_->indexFromGuid(QStringLiteral("assets")).isValid() ? 1 : 0) +
       (directoryModel_ && directoryModel_->indexFromGuid(QStringLiteral("packages")).isValid() ? 1 : 0);
   const int visibleCount = assetModel_ ? assetModel_->rowCount() : 0;
   QString statusText = QStringLiteral("All");
   if (currentStatusFilter_ == QStringLiteral("imported")) {
    statusText = QStringLiteral("Imported");
   } else if (currentStatusFilter_ == QStringLiteral("favorite")) {
    statusText = QStringLiteral("Favorite");
   } else if (currentStatusFilter_ == QStringLiteral("missing")) {
    statusText = QStringLiteral("Missing");
   } else if (currentStatusFilter_ == QStringLiteral("unused")) {
    statusText = QStringLiteral("Unused");
   }
   QString typeText = QStringLiteral("All");
   if (currentFileTypeFilter_ == QStringLiteral("images")) typeText = QStringLiteral("Images");
   else if (currentFileTypeFilter_ == QStringLiteral("videos")) typeText = QStringLiteral("Videos");
   else if (currentFileTypeFilter_ == QStringLiteral("audio")) typeText = QStringLiteral("Audio");
   else if (currentFileTypeFilter_ == QStringLiteral("3d")) typeText = QStringLiteral("3D");
   const QString searchText = currentSearchFilter_.trimmed();
   const QString searchPart = searchText.isEmpty()
       ? QString()
       : QStringLiteral("  •  Search: \"%1\"").arg(searchText);
   leftHubSummaryLabel_->setText(
       QStringLiteral("Showing: %1  •  Favorites: %2  •  Sources: %3  •  Type: %4  •  Status: %5%6")
           .arg(visibleCount)
           .arg(favoriteCount)
           .arg(sourceCount)
           .arg(typeText)
           .arg(statusText)
           .arg(searchPart));
   leftHubSummaryLabel_->setToolTip(QStringLiteral("Status follows the current asset filter."));
  }
  if (leftHubRecentLabel_) {
   const QVector<RecentEntry> entries = directoryModel_ ? directoryModel_->recentEntries() : QVector<RecentEntry>{};
   if (entries.isEmpty()) {
    leftHubRecentLabel_->setText(QStringLiteral("Open a folder to continue | Recent folders appear here"));
    leftHubRecentLabel_->setToolTip(QStringLiteral("Select a folder to browse assets."));
   } else {
    QStringList names;
    const int limit = static_cast<int>(std::min<qsizetype>(entries.size(), 3));
    names.reserve(limit);
    for (int i = 0; i < limit; ++i) {
     QString label = entries[i].name.trimmed();
     if (label.isEmpty()) {
      label = QFileInfo(entries[i].path).fileName();
     }
     if (label.isEmpty()) {
      label = entries[i].path;
     }
     names.append(label);
    }
    leftHubRecentLabel_->setText(QStringLiteral("Recent folders: %1").arg(names.join(QStringLiteral("  •  "))));
    leftHubRecentLabel_->setToolTip(entries.first().path);
   }
  }
  if (leftHubSelectionLabel_) {
   const QStringList paths = selectedAssetPaths();
   if (paths.isEmpty()) {
    leftHubSelectionLabel_->setText(QStringLiteral("Select an asset to inspect"));
    leftHubSelectionLabel_->setToolTip(QStringLiteral("Select an asset to inspect details."));
   } else {
    QString name = QFileInfo(paths.first()).fileName();
    if (name.isEmpty()) {
     name = paths.first();
    }
    const int selectedRowCount =
        (fileView_ && fileView_->selectionModel())
            ? fileView_->selectionModel()->selectedRows().size()
            : 0;
    const QString sourceCountText =
        paths.size() == selectedRowCount
            ? QString()
            : QStringLiteral(" • %1 source paths").arg(paths.size());
    leftHubSelectionLabel_->setText(
        QStringLiteral("Selection: %1 selected%2 | %3")
            .arg(selectedRowCount)
            .arg(sourceCountText)
            .arg(name));
    leftHubSelectionLabel_->setToolTip(paths.join(QStringLiteral("\n")));
   }
  }
  if (!recentFolderButtons_.isEmpty()) {
   const QVector<RecentEntry> entries = directoryModel_ ? directoryModel_->recentEntries() : QVector<RecentEntry>{};
   for (int i = 0; i < recentFolderButtons_.size(); ++i) {
    RecentFolderButton* button = recentFolderButtons_[i];
    if (!button) {
     continue;
    }
    if (i < entries.size()) {
     QString label = entries[i].name.trimmed();
     if (label.isEmpty()) {
      label = QFileInfo(entries[i].path).fileName();
     }
     if (label.isEmpty()) {
      label = entries[i].path;
     }
     button->setEntry(label, entries[i].path, [this](const QString& path) {
      if (owner_) {
       owner_->navigateToFolder(path);
      }
     });
    } else {
     button->setEntry(QString(), QString(), {});
    }
   }
  }
 }

 void ArtifactAssetBrowser::Impl::applyFilters()
 {
  if (!fileView_ || !assetModel_ || currentDirectoryPath_.isEmpty()) return;

  refreshImportedAssetPathCache();

   const QString searchRoot = currentSearchScope_ == QStringLiteral("current")
       ? currentDirectoryPath_
       : ArtifactProjectManager::getInstance().currentProjectAssetsPath();
   QDir dir(searchRoot.isEmpty() ? currentDirectoryPath_ : searchRoot);
   if (!dir.exists()) return;

  const QString currentDirectoryPrefix =
      QDir::cleanPath(currentDirectoryPath_) + QDir::separator();
  for (const QString& expandedPath : expandedSequencePaths_.values()) {
   if (!expandedPath.startsWith(currentDirectoryPrefix, Qt::CaseInsensitive)) {
    expandedSequencePaths_.remove(expandedPath);
   }
  }

  if (breadcrumb_) {
   breadcrumb_->setPath(currentDirectoryPath_);
  }

   // Get both files and directories, excluding . and ..
   QStringList entries;
   if (currentSearchScope_ == QStringLiteral("current")) {
    entries = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
   } else {
    QDirIterator iterator(dir.absolutePath(), QDir::Files,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
     const QString absolutePath = iterator.next();
     entries.append(dir.relativeFilePath(absolutePath));
    }
   }
   QList<AssetMenuItem> items;

   // --- Phase 1: pre-filter directories ---
   QStringList dirNames;
   for (const QString& entry : entries) {
    QString fullPath = dir.absoluteFilePath(entry);
    QFileInfo fileInfo(fullPath);
    if (!fileInfo.isDir()) continue;
    if (currentFileTypeFilter_ != "all") continue;
    if (!matchesSearchFilter(entry)) continue;
    dirNames.append(entry);
   }

   // --- Phase 2: pre-filter files and detect sequences ---
   struct SeqFile { QString name; qint64 frame; int pad; QString fullPath; };
   static const QRegularExpression kSeqRx(QStringLiteral(R"(^(.*?)([._-]?)(\d{3,})(\.[a-zA-Z0-9]+)$)"));
   QMap<QString, QList<SeqFile>> seqMap;
   QSet<QString> seqFiles;

   for (const QString& entry : entries) {
    QString fullPath = dir.absoluteFilePath(entry);
    QFileInfo fileInfo(fullPath);
    if (fileInfo.isDir()) continue;
    if (!matchesSearchFilter(entry) || !matchesFileTypeFilter(entry)) continue;

    QRegularExpressionMatch m = kSeqRx.match(entry);
    if (!m.hasMatch()) continue;
    QString frameStr = m.captured(3);
    bool ok = false;
    const qint64 frameNum = frameStr.toLongLong(&ok);
    if (!ok) continue;
    // Padding is part of the sequence identity. Mixing `001` and `0002`
    // would produce an invalid display pattern and an ambiguous import.
    QString key = m.captured(1) + m.captured(2) + m.captured(4) +
                  QStringLiteral("|pad=") + QString::number(frameStr.length());
    seqMap[key].append({entry, frameNum, static_cast<int>(frameStr.length()), fullPath});
   }

   for (auto it = seqMap.begin(); it != seqMap.end(); ++it) {
    if (it.value().size() < 2) continue;
    for (const auto& sf : it.value()) seqFiles.insert(sf.name);
   }

   // --- Phase 3: build items (dirs → sequences → standalone files) ---

   // Directories
   for (const QString& dirName : dirNames) {
    AssetMenuItem item;
    QString fullPath = dir.absoluteFilePath(dirName);
    item.name = UniString::fromQString(dirName);
    item.path = UniString::fromQString(fullPath);
    QStringList markers;
    if (isFavoriteAssetPath(fullPath)) markers.append(QStringLiteral("Favorite"));
    QString itemType = QStringLiteral("Folder");
    if (!markers.isEmpty()) itemType = QStringLiteral("%1 • Folder").arg(markers.join(QStringLiteral(" • ")));
    item.type = UniString::fromQString(itemType);
    item.isFolder = true;
     item.icon = fileTypeIconFor(dirName);
    items.append(item);
   }

   // Sequences
   for (auto it = seqMap.begin(); it != seqMap.end(); ++it) {
    if (it.value().size() < 2) continue;
    auto& seq = it.value();
    std::sort(seq.begin(), seq.end(), [](const SeqFile& a, const SeqFile& b) { return a.frame < b.frame; });

    AssetMenuItem item;
    const qint64 firstFrame = seq.first().frame;
    const qint64 lastFrame = seq.last().frame;
    int count = seq.size();
    int pad = seq.first().pad;
    qint64 missingFrameCount = 0;
    int unreadableFrameCount = 0;
    bool hasSizeMismatch = false;
    QSize sequenceFrameSize;
    for (int frameIndex = 1; frameIndex < seq.size(); ++frameIndex) {
     const qint64 gap = seq[frameIndex].frame - seq[frameIndex - 1].frame - 1;
     if (gap > 0) missingFrameCount += gap;
    }
    for (const auto& sf : seq) {
     QImageReader reader(sf.fullPath);
     if (!reader.canRead()) {
      ++unreadableFrameCount;
      continue;
     }
     const QSize frameSize = reader.size();
     if (!frameSize.isValid()) {
      continue;
     }
     if (!sequenceFrameSize.isValid()) {
      sequenceFrameSize = frameSize;
     } else if (sequenceFrameSize != frameSize) {
      hasSizeMismatch = true;
     }
    }
    QFileInfo fi(seq.first().name);
    QString ext = fi.suffix().toUpper();

    // Name: "prefix[####].ext (N frames)"
    QString prefix;
    {
     QRegularExpressionMatch m2 = kSeqRx.match(seq.first().name);
     prefix = m2.hasMatch() ? m2.captured(1) : fi.completeBaseName();
    }
    item.name = UniString::fromQString(QStringLiteral("%1[%2-%3].%4 (%5 frames)")
      .arg(prefix)
      .arg(firstFrame, pad, 10, QLatin1Char('0'))
      .arg(lastFrame, pad, 10, QLatin1Char('0'))
      .arg(ext.toLower())
      .arg(count));
    item.path = UniString::fromQString(seq.first().fullPath);
    item.isSequence = true;
    item.sequenceFrameCount = count;
    item.sequenceStartFrame = firstFrame;
    item.sequencePadding = pad;
    for (const auto& sf : seq) item.sequencePaths.append(sf.fullPath);

    // Status markers (aggregated across all frames via the shared helper,
    // extended with frame-level diagnostics from the sequence scan)
    AssetStatusSummary status = assetStatusForPaths(item.path.toQString(), item.sequencePaths);
    status.missing = status.missing || missingFrameCount > 0 ||
                     unreadableFrameCount > 0 || hasSizeMismatch;
    if ((currentSearchScope_ == QStringLiteral("missing") && !status.missing) ||
        (currentSearchScope_ == QStringLiteral("unused") && !status.unused) ||
        !matchesStatusFilter(status)) {
     continue;
    }

    QStringList markers = assetStatusMarkers(status);
    QStringList frameDetailMarkers;
    if (missingFrameCount > 0) {
     frameDetailMarkers.append(QStringLiteral("Missing Frames: %1").arg(missingFrameCount));
    }
    if (unreadableFrameCount > 0) {
     frameDetailMarkers.append(QStringLiteral("Unreadable: %1").arg(unreadableFrameCount));
    }
    if (hasSizeMismatch) {
     frameDetailMarkers.append(QStringLiteral("Size Mismatch"));
    }
    if (!frameDetailMarkers.isEmpty()) {
     const int insertAt = markers.indexOf(QStringLiteral("Missing")) + 1;
     for (int i = 0; i < frameDetailMarkers.size(); ++i) {
      markers.insert(insertAt + i, frameDetailMarkers.at(i));
     }
    }
    QString seqType = QStringLiteral("Sequence • %1").arg(ext);
    if (!markers.isEmpty()) seqType = QStringLiteral("%1 • %2").arg(markers.join(QStringLiteral(" • ")), seqType);
    item.type = UniString::fromQString(seqType);

      item.icon = fileTypeIconFor(seq.first().name);
    items.append(item);
    if (expandedSequencePaths_.contains(item.path.toQString())) {
     int frameIndex = 0;
     for (const auto& sf : seq) {
      AssetMenuItem frameItem;
      frameItem.name = UniString::fromQString(
          QStringLiteral("  └ %1").arg(sf.name));
      frameItem.path = UniString::fromQString(sf.fullPath);
      QStringList frameMarkers;
      QImageReader frameReader(sf.fullPath);
      if (!frameReader.canRead()) {
       frameMarkers.append(QStringLiteral("Unreadable"));
      } else if (sequenceFrameSize.isValid() &&
                 frameReader.size().isValid() &&
                 frameReader.size() != sequenceFrameSize) {
       frameMarkers.append(QStringLiteral("Size Mismatch"));
      }
      if (isMissingAssetPath(sf.fullPath)) {
       frameMarkers.append(QStringLiteral("Missing"));
      }
      QString frameType = QStringLiteral("Sequence Frame • %1").arg(sf.frame);
      if (!frameMarkers.isEmpty()) {
       frameType = QStringLiteral("%1 • %2")
           .arg(frameMarkers.join(QStringLiteral(" • ")), frameType);
      }
      frameItem.type = UniString::fromQString(frameType);
      frameItem.isSequenceFrame = true;
      frameItem.sequenceParentPath = item.path.toQString();
      frameItem.sequenceFrameNumber = sf.frame;
      frameItem.sequenceFrameCount = item.sequenceFrameCount;
      frameItem.sequenceStartFrame = item.sequenceStartFrame;
      frameItem.sequencePadding = item.sequencePadding;
      frameItem.sequencePaths = item.sequencePaths;
      frameItem.icon = (frameIndex++ == 0) ? item.icon : QIcon();
      items.append(std::move(frameItem));
     }
    }
   }

   // Standalone files
   std::vector<AssetMenuItem> standaloneItems(entries.size());
   const auto processStandaloneRange =
    [&](int begin, int end) {
     for (int i = begin; i < end; ++i) {
      const QString& entry = entries.at(i);
      const QString fullPath = dir.absoluteFilePath(entry);
      const QFileInfo fileInfo(fullPath);
      if (fileInfo.isDir() || !matchesSearchFilter(entry) || !matchesFileTypeFilter(entry) || seqFiles.contains(entry)) {
       continue;
      }

      // Status markers via the shared helper (same rule and filter as sequences)
      const AssetStatusSummary status = assetStatusForPaths(fullPath);
      if (!matchesStatusFilter(status)) {
       continue;
      }

      AssetMenuItem item;
      item.name = UniString::fromQString(entry);
      item.path = UniString::fromQString(fullPath);
      const QStringList markers = assetStatusMarkers(status);
      QString itemType = fileInfo.suffix().toUpper();
      if (!markers.isEmpty()) itemType = QStringLiteral("%1 • %2").arg(markers.join(QStringLiteral(" • ")), itemType);
      item.type = UniString::fromQString(itemType);
      item.isFolder = false;
      item.icon = fileTypeIconFor(entry);
      standaloneItems[static_cast<size_t>(i)] = std::move(item);
     }
    };
   processStandaloneRange(0, static_cast<int>(entries.size()));

   for (auto& item : standaloneItems) {
    if (!item.name.toQString().isEmpty()) {
     items.append(std::move(item));
    }
   }

   // Source use count is project state, so decorate the browser metadata only
   // after the filesystem scan has completed. This keeps the parallel scan
   // focused on filesystem/project filtering and avoids adding row widgets.
   for (auto& item : items) {
    if (item.isFolder) {
     continue;
    }
    const QString path = item.path.toQString();
    if (path.isEmpty()) {
     continue;
    }
    const int sourceUseCount = sourceUseCountForPath(path, item.sequencePaths);
    if (sourceUseCount <= 0 && !isImportedAssetPath(path)) {
     continue;
    }
    QString typeText = item.type.toQString();
    if (!typeText.contains(QStringLiteral("Source Uses:"))) {
     typeText += QStringLiteral(" • Source Uses: %1").arg(sourceUseCount);
     item.type = UniString::fromQString(typeText);
    }
   }

   // Sort items
   std::sort(items.begin(), items.end(), [this](const AssetMenuItem& a, const AssetMenuItem& b) {
    const auto compareNaturalName = [&a, &b]() {
     int result = naturalAssetNameCompare(a.name.toQString(), b.name.toQString());
     if (result == 0) {
      result = QString::compare(a.path.toQString(), b.path.toQString(), Qt::CaseInsensitive);
     }
     return result;
    };
    // Folders always first
    if (a.isFolder && !b.isFolder) return true;
    if (!a.isFolder && b.isFolder) return false;

    if (a.isSequenceFrame && !b.isSequenceFrame &&
        a.sequenceParentPath == b.path.toQString()) return false;
    if (!a.isSequenceFrame && b.isSequenceFrame &&
        a.path.toQString() == b.sequenceParentPath) return true;
    if (a.isSequenceFrame && b.isSequenceFrame &&
        a.sequenceParentPath == b.sequenceParentPath &&
        a.sequenceFrameNumber != b.sequenceFrameNumber) {
     return a.sequenceFrameNumber < b.sequenceFrameNumber;
    }

    if (currentSortBy_ == "name") {
     int result = compareNaturalName();
     return sortAscending_ ? result < 0 : result > 0;
    } else if (currentSortBy_ == "date_name") {
     QFileInfo infoA(a.path.toQString());
     QFileInfo infoB(b.path.toQString());
     const QDateTime dateA = infoA.lastModified();
     const QDateTime dateB = infoB.lastModified();
     int result = dateA < dateB ? -1 : (dateA > dateB ? 1 : 0);
     if (result == 0) result = compareNaturalName();
     return sortAscending_ ? result < 0 : result > 0;
    } else if (currentSortBy_ == "size_name") {
     const qint64 sizeA = QFileInfo(a.path.toQString()).size();
     const qint64 sizeB = QFileInfo(b.path.toQString()).size();
     int result = sizeA < sizeB ? -1 : (sizeA > sizeB ? 1 : 0);
     if (result == 0) result = compareNaturalName();
     return sortAscending_ ? result < 0 : result > 0;
    } else if (currentSortBy_ == "type_name") {
     int result = a.type.toQString().compare(b.type.toQString(), Qt::CaseInsensitive);
     if (result == 0) result = compareNaturalName();
     return sortAscending_ ? result < 0 : result > 0;
    } else if (currentSortBy_ == "date") {
     QFileInfo infoA(a.path.toQString());
     QFileInfo infoB(b.path.toQString());
     QDateTime dateA = infoA.lastModified();
     QDateTime dateB = infoB.lastModified();
     int result = dateA < dateB ? -1 : (dateA > dateB ? 1 : 0);
     if (result == 0) result = compareNaturalName();
     return sortAscending_ ? result < 0 : result > 0;
    } else if (currentSortBy_ == "size") {
     qint64 sizeA = QFileInfo(a.path.toQString()).size();
     qint64 sizeB = QFileInfo(b.path.toQString()).size();
     int result = sizeA < sizeB ? -1 : (sizeA > sizeB ? 1 : 0);
     if (result == 0) result = compareNaturalName();
     return sortAscending_ ? result < 0 : result > 0;
    } else if (currentSortBy_ == "type") {
     int result = a.type.toQString().compare(b.type.toQString(), Qt::CaseInsensitive);
     if (result == 0) result = compareNaturalName();
     return sortAscending_ ? result < 0 : result > 0;
    }
    return compareNaturalName() < 0;
   });

   assetModel_->setItems(items);
   if (auto* assetListView = static_cast<AssetFileListView*>(fileView_)) {
    QString emptyMessage;
    if (currentDirectoryPath_.isEmpty()) {
     emptyMessage = QStringLiteral("Open a folder to browse assets.");
    } else if (!currentSearchFilter_.trimmed().isEmpty() ||
               currentFileTypeFilter_ != QStringLiteral("all") ||
               currentStatusFilter_ != QStringLiteral("all")) {
     emptyMessage = QStringLiteral("No assets match the current search and filters.");
    } else {
     emptyMessage = QStringLiteral("No assets in this folder. Import or drop files here.");
    }
   assetListView->setEmptyStateMessage(emptyMessage);
   }
   QTimer::singleShot(0, owner_, [this]() { warmVisibleThumbnails(); });
 }

 void ArtifactAssetBrowser::Impl::warmVisibleThumbnails()
 {
  if (!fileView_ || !assetModel_ || !fileView_->model()) {
   return;
  }

  const QRect viewportRect = fileView_->viewport()->rect();
  if (viewportRect.isEmpty()) {
   return;
  }
  const QModelIndex top = fileView_->indexAt(viewportRect.topLeft());
  const QModelIndex bottom = fileView_->indexAt(viewportRect.bottomRight());
  if (!top.isValid() || !bottom.isValid()) {
   return;
  }

  const int first = std::max(0, std::min(top.row(), bottom.row()) - 2);
  const int last = std::min(assetModel_->rowCount() - 1,
                            std::max(top.row(), bottom.row()) + 2);
  for (int row = first; row <= last; ++row) {
   const AssetMenuItem item = assetModel_->itemAt(row);
   if (item.isFolder || item.path.toQString().isEmpty()) {
    continue;
   }
   const QString path = item.path.toQString();
   if (isImageFile(QFileInfo(path).fileName()) ||
       isVideoFile(QFileInfo(path).fileName())) {
    (void)getFileIcon(QFileInfo(path).fileName(), path);
   }
  }
 }

 ArtifactAssetBrowser::ArtifactAssetBrowser(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
 impl_->owner_ = this;
  setWindowTitle("AssetBrowser");
  setFocusPolicy(Qt::StrongFocus);
  setAccessibleName(QStringLiteral("Asset Browser"));
  setAccessibleDescription(QStringLiteral("Browse, search, import, and organize project assets"));

  // Enable drag and drop
  setAcceptDrops(true);

  auto assetToolBar = new ArtifactAssetBrowserToolBar();
  impl_->searchEdit_ = assetToolBar->findChild<QLineEdit*>();
  if (impl_->searchEdit_) {
   impl_->searchEdit_->installEventFilter(this);
   QSettings settings;
   auto *searchHistoryModel = new QStringListModel(
       settings.value(QStringLiteral("AssetBrowser/SearchHistory")).toStringList(),
       impl_->searchEdit_);
   auto *searchCompleter = new QCompleter(searchHistoryModel, impl_->searchEdit_);
   searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
   searchCompleter->setFilterMode(Qt::MatchContains);
   impl_->searchEdit_->setCompleter(searchCompleter);
  }
  impl_->upButton_ = assetToolBar->findChild<QToolButton*>(QStringLiteral("assetBrowserUpButton"));
  impl_->refreshButton_ = assetToolBar->findChild<QToolButton*>(QStringLiteral("assetBrowserRefreshButton"));
  auto* gridViewButton = assetToolBar->findChild<QToolButton*>(QStringLiteral("assetBrowserGridViewButton"));
  auto* listViewButton = assetToolBar->findChild<QToolButton*>(QStringLiteral("assetBrowserListViewButton"));

  // File type filter buttons
  auto typeFiltersLayout = new QHBoxLayout();
  typeFiltersLayout->setContentsMargins(0, 0, 0, 0);
  typeFiltersLayout->setSpacing(4);
  impl_->filterButtonGroup_ = new QButtonGroup(this);

  auto allButton = new QToolButton();
  allButton->setText("All");
  allButton->setAccessibleName(QStringLiteral("All asset types"));
  allButton->setAccessibleDescription(QStringLiteral("Show all asset types"));
  allButton->setCheckable(true);
  allButton->setChecked(true);

  auto imagesButton = new QToolButton();
  imagesButton->setText("Images");
  imagesButton->setAccessibleName(QStringLiteral("Images"));
  imagesButton->setAccessibleDescription(QStringLiteral("Show image assets"));
  imagesButton->setCheckable(true);

  auto videosButton = new QToolButton();
  videosButton->setText("Videos");
  videosButton->setAccessibleName(QStringLiteral("Videos"));
  videosButton->setAccessibleDescription(QStringLiteral("Show video assets"));
  videosButton->setCheckable(true);

  auto audioButton = new QToolButton();
  audioButton->setText("Audio");
  audioButton->setAccessibleName(QStringLiteral("Audio"));
  audioButton->setAccessibleDescription(QStringLiteral("Show audio assets"));
  audioButton->setCheckable(true);

  auto fontsButton = new QToolButton();
  fontsButton->setText("3D");
  fontsButton->setAccessibleName(QStringLiteral("3D assets"));
  fontsButton->setAccessibleDescription(QStringLiteral("Show 3D assets"));
  fontsButton->setCheckable(true);

  impl_->filterButtonGroup_->addButton(allButton, 0);
  impl_->filterButtonGroup_->addButton(imagesButton, 1);
  impl_->filterButtonGroup_->addButton(videosButton, 2);
  impl_->filterButtonGroup_->addButton(audioButton, 3);
  impl_->filterButtonGroup_->addButton(fontsButton, 4);

   typeFiltersLayout->addWidget(allButton);
   typeFiltersLayout->addWidget(imagesButton);
   typeFiltersLayout->addWidget(videosButton);
   typeFiltersLayout->addWidget(audioButton);
   typeFiltersLayout->addWidget(fontsButton);
   typeFiltersLayout->addStretch();

   auto statusAllBtn = new QToolButton(this);
   statusAllBtn->setText("Status: All");
   statusAllBtn->setAccessibleName(QStringLiteral("All asset statuses"));
   statusAllBtn->setAccessibleDescription(QStringLiteral("Show assets with any status"));
   statusAllBtn->setCheckable(true);
   statusAllBtn->setChecked(true);

   auto importedBtn = new QToolButton(this);
   importedBtn->setText("Imported");
   importedBtn->setAccessibleName(QStringLiteral("Imported assets"));
   importedBtn->setAccessibleDescription(QStringLiteral("Show assets already imported into the project"));
   importedBtn->setCheckable(true);

   auto favoriteBtn = new QToolButton(this);
   favoriteBtn->setText("Favorite");
   favoriteBtn->setAccessibleName(QStringLiteral("Favorite assets"));
   favoriteBtn->setAccessibleDescription(QStringLiteral("Show favorite assets"));
   favoriteBtn->setCheckable(true);

   auto missingBtn = new QToolButton(this);
   missingBtn->setText("Missing");
   missingBtn->setAccessibleName(QStringLiteral("Missing assets"));
   missingBtn->setAccessibleDescription(QStringLiteral("Show assets whose source is missing"));
   missingBtn->setCheckable(true);

   auto unusedBtn = new QToolButton(this);
   unusedBtn->setText("Unused");
   unusedBtn->setAccessibleName(QStringLiteral("Unused assets"));
   unusedBtn->setAccessibleDescription(QStringLiteral("Show assets not used by the project"));
   unusedBtn->setCheckable(true);

 auto* statusGroup = new QButtonGroup(this);
   statusGroup->setExclusive(true);
   statusGroup->addButton(statusAllBtn, 0);
   statusGroup->addButton(importedBtn, 1);
   statusGroup->addButton(favoriteBtn, 2);
   statusGroup->addButton(missingBtn, 3);
   statusGroup->addButton(unusedBtn, 4);
   auto* scopeCurrentBtn = new QToolButton(this);
   scopeCurrentBtn->setText(QStringLiteral("Current Folder"));
   scopeCurrentBtn->setCheckable(true);
   scopeCurrentBtn->setAccessibleName(QStringLiteral("Search current folder"));
   auto* scopeProjectBtn = new QToolButton(this);
   scopeProjectBtn->setText(QStringLiteral("Project Assets"));
   scopeProjectBtn->setCheckable(true);
   scopeProjectBtn->setAccessibleName(QStringLiteral("Search project assets"));
   auto* scopeMissingBtn = new QToolButton(this);
   scopeMissingBtn->setText(QStringLiteral("Missing"));
   scopeMissingBtn->setCheckable(true);
   scopeMissingBtn->setAccessibleName(QStringLiteral("Search missing assets"));
   auto* scopeUnusedBtn = new QToolButton(this);
   scopeUnusedBtn->setText(QStringLiteral("Unused"));
   scopeUnusedBtn->setCheckable(true);
   scopeUnusedBtn->setAccessibleName(QStringLiteral("Search unused assets"));
   auto* scopeGroup = new QButtonGroup(this);
   scopeGroup->setExclusive(true);
   scopeGroup->addButton(scopeCurrentBtn, 0);
   scopeGroup->addButton(scopeProjectBtn, 1);
   scopeGroup->addButton(scopeMissingBtn, 2);
   scopeGroup->addButton(scopeUnusedBtn, 3);
   auto& config = ArtifactCore::LayeredConfigStore::instance();
   const QString savedStatusFilter = config.valueString(
       QStringLiteral("AssetBrowser/StatusFilter"), QStringLiteral("all"));
   const QHash<QString, int> statusIds{{QStringLiteral("all"), 0},
                                       {QStringLiteral("imported"), 1},
                                       {QStringLiteral("favorite"), 2},
                                       {QStringLiteral("missing"), 3},
                                       {QStringLiteral("unused"), 4}};
   const int savedStatusId = statusIds.value(savedStatusFilter, 0);
   impl_->currentStatusFilter_ = statusIds.contains(savedStatusFilter)
       ? savedStatusFilter
       : QStringLiteral("all");
   if (auto* savedStatusButton = statusGroup->button(savedStatusId)) {
    savedStatusButton->setChecked(true);
   }
   const QString savedScope = config.valueString(
       QStringLiteral("AssetBrowser/SearchScope"), QStringLiteral("current"));
   const QHash<QString, int> scopeIds{{QStringLiteral("current"), 10},
                                      {QStringLiteral("project"), 11},
                                      {QStringLiteral("missing"), 12},
                                      {QStringLiteral("unused"), 13}};
   impl_->currentSearchScope_ = scopeIds.contains(savedScope)
       ? savedScope : QStringLiteral("current");
   if (auto* savedScopeButton = scopeGroup->button(
           scopeIds.value(impl_->currentSearchScope_, 10) - 10)) {
    savedScopeButton->setChecked(true);
   }
   for (auto* button : {scopeCurrentBtn, scopeProjectBtn, scopeMissingBtn,
                        scopeUnusedBtn}) {
    button->setAutoRaise(true);
    button->setMinimumHeight(26);
    button->setCursor(Qt::PointingHandCursor);
    applyAssetBrowserFilterPalette(button);
   }
   assetToolBar->addSeparator();
   assetToolBar->addWidget(scopeCurrentBtn);
   assetToolBar->addWidget(scopeProjectBtn);
   assetToolBar->addWidget(scopeMissingBtn);
   assetToolBar->addWidget(scopeUnusedBtn);
   const QString savedTypeFilter = config.valueString(
       QStringLiteral("AssetBrowser/FileTypeFilter"), QStringLiteral("all"));
   const QHash<QString, int> typeIds{{QStringLiteral("all"), 0},
                                     {QStringLiteral("images"), 1},
                                     {QStringLiteral("videos"), 2},
                                     {QStringLiteral("audio"), 3},
                                     {QStringLiteral("3d"), 4}};
   impl_->currentFileTypeFilter_ = typeIds.contains(savedTypeFilter)
       ? savedTypeFilter
       : QStringLiteral("all");
   if (auto* savedTypeButton = impl_->filterButtonGroup_->button(
           typeIds.value(impl_->currentFileTypeFilter_, 0))) {
    savedTypeButton->setChecked(true);
   }

   // Status filters remain available to the model/context menu, but the main
   // browser header follows the mockup's compact type-first presentation.
   for (auto* button : {statusAllBtn, importedBtn, favoriteBtn, missingBtn,
                        unusedBtn}) {
    button->hide();
   }

   for (auto *filterButton : {allButton, imagesButton, videosButton,
                              audioButton, fontsButton, statusAllBtn,
                              importedBtn, favoriteBtn, missingBtn, unusedBtn}) {
    filterButton->setAutoRaise(true);
    filterButton->setMinimumHeight(26);
    filterButton->setCursor(Qt::PointingHandCursor);
    applyAssetBrowserFilterPalette(filterButton);
   }

   assetToolBar->addSeparator();

   // Keep the unused-asset query close to the type filters.  The underlying
   // reference scan is already shared with the status filter; exposing this
   // action avoids requiring a context-menu-only workflow.
   unusedBtn->show();
   unusedBtn->setText(QStringLiteral("Unused"));
   unusedBtn->setToolTip(QStringLiteral("Show assets with no project references"));
   assetToolBar->addWidget(unusedBtn);

   // Missing uses the same status aggregation as row markers and relink
   // actions; keep it visible beside Unused so the existing filter is
   // reachable without requiring an external setStatusFilter() call.
   missingBtn->show();
   missingBtn->setText(QStringLiteral("Missing"));
   missingBtn->setToolTip(QStringLiteral("Show assets whose source is missing"));
   assetToolBar->addWidget(missingBtn);

   // Sort by combo box
   auto* sortByCombo = new QComboBox();
   sortByCombo->setAccessibleName(QStringLiteral("Asset sort order"));
   sortByCombo->setAccessibleDescription(QStringLiteral("Choose how visible assets are sorted"));
   sortByCombo->addItem("Sort: Name", "name");
   sortByCombo->addItem("Sort: Date", "date");
   sortByCombo->addItem("Sort: Size", "size");
   sortByCombo->addItem("Sort: Type", "type");
   sortByCombo->addItem("Sort: Type → Name", "type_name");
   sortByCombo->addItem("Sort: Date → Name", "date_name");
   sortByCombo->addItem("Sort: Size → Name", "size_name");
   sortByCombo->setCurrentIndex(1);
   sortByCombo->setMinimumWidth(112);
   sortByCombo->setMinimumHeight(28);
   sortByCombo->setToolTip(QStringLiteral("Sort visible assets"));
   assetToolBar->addWidget(sortByCombo);

   const QString savedSortKey = config.valueString(
       QStringLiteral("AssetBrowser/SortKey"), QStringLiteral("date"));
   const int savedSortIndex = sortByCombo->findData(savedSortKey);
   if (savedSortIndex >= 0) {
    sortByCombo->setCurrentIndex(savedSortIndex);
    impl_->currentSortBy_ = savedSortKey;
   }

   // Sort order toggle button
   auto* sortOrderBtn = new QToolButton();
   sortOrderBtn->setAccessibleName(QStringLiteral("Toggle asset sort direction"));
   sortOrderBtn->setAccessibleDescription(QStringLiteral("Switch between ascending and descending asset order"));
   sortOrderBtn->setText("\u2191"); // Up arrow
   sortOrderBtn->setCheckable(true);
   sortOrderBtn->setChecked(false);
   sortOrderBtn->setText("↓");
   sortOrderBtn->setFixedWidth(30);
   sortOrderBtn->setToolTip("Sort Order: Ascending/Descending");
   assetToolBar->addWidget(sortOrderBtn);
   sortOrderBtn->setChecked(config.valueBool(
       QStringLiteral("AssetBrowser/SortAscending"), false));
   impl_->sortAscending_ = sortOrderBtn->isChecked();

   connect(statusGroup, &QButtonGroup::idClicked, this, [this](int id) {
    switch (id) {
     case 0: impl_->currentStatusFilter_ = "all"; break;
     case 1: impl_->currentStatusFilter_ = "imported"; break;
     case 2: impl_->currentStatusFilter_ = "favorite"; break;
    case 3: impl_->currentStatusFilter_ = "missing"; break;
    case 4: impl_->currentStatusFilter_ = "unused"; break;
   }
    ArtifactCore::LayeredConfigStore::instance().setValue(
        QStringLiteral("AssetBrowser/StatusFilter"), impl_->currentStatusFilter_);
    impl_->applyFilters();
    impl_->refreshLeftHubSummary();
   });
   connect(scopeGroup, &QButtonGroup::idClicked, this, [this](int id) {
    static const QStringList scopes = {QStringLiteral("current"),
                                       QStringLiteral("project"),
                                       QStringLiteral("missing"),
                                       QStringLiteral("unused")};
    impl_->currentSearchScope_ = scopes.at(id);
    ArtifactCore::LayeredConfigStore::instance().setValue(
        QStringLiteral("AssetBrowser/SearchScope"), impl_->currentSearchScope_);
    impl_->applyFilters();
    impl_->refreshLeftHubSummary();
   });

   connect(sortByCombo, &QComboBox::currentIndexChanged, this, [this, sortByCombo](int) {
    impl_->currentSortBy_ = sortByCombo->currentData().toString();
    ArtifactCore::LayeredConfigStore::instance().setValue(
        QStringLiteral("AssetBrowser/SortKey"), impl_->currentSortBy_);
    impl_->applyFilters();
   });

   connect(sortOrderBtn, &QToolButton::toggled, this, [this, sortOrderBtn](bool checked) {
    impl_->sortAscending_ = checked;
    ArtifactCore::LayeredConfigStore::instance().setValue(
        QStringLiteral("AssetBrowser/SortAscending"), checked);
    sortOrderBtn->setText(checked ? "\u2191" : "\u2193");
    impl_->applyFilters();
   });

  auto vLayout = new QVBoxLayout();
  vLayout->setContentsMargins(0, 0, 0, 0);
  vLayout->setSpacing(0);

  auto layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto directoryView = impl_->directoryView_ = new QTreeView();
  auto directoryModel = impl_->directoryModel_ = new AssetDirectoryModel(this);

  QString assetsPath = ArtifactProjectService::instance()
                           ? ArtifactProjectService::instance()->currentProjectAssetsPath()
                           : QString();
  if (assetsPath.isEmpty()) {
   assetsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Assets";
  }
  QString packagesPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Packages";

  directoryModel->setAssetRootPath(assetsPath);
  directoryModel->setPackageRootPath(packagesPath);

  directoryView->setModel(directoryModel);
  directoryView->setHeaderHidden(true);
  directoryView->setIndentation(16);
  directoryView->setMinimumWidth(156);
  directoryView->setMaximumWidth(224);
  directoryView->setUniformRowHeights(true);
  directoryView->setAlternatingRowColors(false);
  directoryView->setExpandsOnDoubleClick(true);
   directoryView->setAnimated(true);
   directoryView->setAcceptDrops(true);
   directoryView->setDropIndicatorShown(true);
   directoryView->setDragDropMode(QAbstractItemView::DropOnly);

  QString desktopPath = assetsPath;

  auto* breadcrumbBar = impl_->breadcrumb_ = new ArtifactBreadcrumbWidget(this);
  breadcrumbBar->setRootPath(assetsPath);
  breadcrumbBar->setPath(desktopPath);
  breadcrumbBar->setToolTip(QStringLiteral("Current asset folder"));
  connect(breadcrumbBar, &ArtifactBreadcrumbWidget::pathClicked, this,
          [this](const QString& path) {
            navigateToFolder(path);
          });

  impl_->syncStateLabel_ = new QLabel(impl_->syncStateText(), this);
  impl_->syncStateLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  impl_->syncStateLabel_->setMinimumHeight(24);
  applyAssetBrowserPanelPalette(impl_->syncStateLabel_);

  auto* leftHubCard = makeAssetBrowserPanel(this);
  auto* leftHubLayout = new QVBoxLayout();
  leftHubLayout->setContentsMargins(10, 10, 10, 8);
  leftHubLayout->setSpacing(5);
  auto* leftHubTitle = new QLabel(QStringLiteral("Sources"), leftHubCard);
  {
   QFont font = leftHubTitle->font();
   font.setBold(true);
   leftHubTitle->setFont(font);
  }
  impl_->currentPathLabel_ = new QLabel(QStringLiteral("Current: %1").arg(desktopPath), leftHubCard);
  impl_->leftHubSummaryLabel_ = new QLabel(leftHubCard);
  impl_->leftHubRecentLabel_ = new QLabel(leftHubCard);
  impl_->leftHubSelectionLabel_ = new QLabel(leftHubCard);
  impl_->recentFolderButtons_.clear();
  impl_->currentPathLabel_->setWordWrap(false);
  impl_->leftHubSummaryLabel_->setWordWrap(true);
  impl_->leftHubRecentLabel_->setWordWrap(true);
  impl_->leftHubSelectionLabel_->setWordWrap(true);
  impl_->currentPathLabel_->setMaximumHeight(24);
  impl_->leftHubSummaryLabel_->setMaximumHeight(40);
  impl_->leftHubRecentLabel_->setMaximumHeight(40);
  impl_->leftHubSelectionLabel_->setMaximumHeight(40);
  impl_->currentPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  impl_->leftHubSummaryLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  impl_->leftHubRecentLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  impl_->leftHubSelectionLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  applyAssetBrowserPanelPalette(leftHubTitle);
  applyAssetBrowserPanelPalette(impl_->currentPathLabel_);
  applyAssetBrowserPanelPalette(impl_->leftHubSummaryLabel_);
  applyAssetBrowserPanelPalette(impl_->leftHubRecentLabel_);
  applyAssetBrowserPanelPalette(impl_->leftHubSelectionLabel_);
  leftHubLayout->addWidget(leftHubTitle);
  auto* leftHubSection = new QLabel(QStringLiteral("FAVORITES"), leftHubCard);
  {
   QFont font = leftHubSection->font();
   font.setPointSizeF(std::max<qreal>(8.0, font.pointSizeF() - 1.0));
   font.setWeight(QFont::DemiBold);
   leftHubSection->setFont(font);
   QPalette pal = leftHubSection->palette();
   pal.setColor(QPalette::WindowText,
                QColor(ArtifactCore::currentDCCTheme().textColor).darker(135));
   leftHubSection->setPalette(pal);
  }
  leftHubLayout->addWidget(leftHubSection);
  auto* allFavoritesButton = new QToolButton(leftHubCard);
  allFavoritesButton->setAccessibleName(QStringLiteral("All favorite assets"));
  allFavoritesButton->setAccessibleDescription(QStringLiteral("Show all favorite assets"));
  allFavoritesButton->setText(QStringLiteral("All Favorites"));
  allFavoritesButton->setIcon(QIcon(QStringLiteral(":/icons/Studio/shape_star.svg")));
  allFavoritesButton->setIconSize(QSize(16, 16));
  allFavoritesButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  allFavoritesButton->setAutoRaise(true);
  allFavoritesButton->setCursor(Qt::PointingHandCursor);
  leftHubLayout->addWidget(allFavoritesButton);
  connect(allFavoritesButton, &QToolButton::clicked, favoriteBtn,
          &QToolButton::click);
  impl_->currentPathLabel_->hide();
  impl_->leftHubSummaryLabel_->hide();
  impl_->leftHubRecentLabel_->hide();
  impl_->leftHubSelectionLabel_->hide();
  auto* leftHubRecentSection = new QLabel(QStringLiteral("RECENT"), leftHubCard);
  {
   QFont font = leftHubRecentSection->font();
   font.setPointSizeF(std::max<qreal>(8.0, font.pointSizeF() - 1.0));
   font.setWeight(QFont::DemiBold);
   leftHubRecentSection->setFont(font);
   QPalette pal = leftHubRecentSection->palette();
   pal.setColor(QPalette::WindowText,
                QColor(ArtifactCore::currentDCCTheme().textColor).darker(135));
   leftHubRecentSection->setPalette(pal);
  }
  leftHubLayout->addWidget(leftHubRecentSection);
  for (int i = 0; i < 3; ++i) {
   auto* recentButton = new RecentFolderButton(leftHubCard);
   recentButton->setVisible(false);
   leftHubLayout->addWidget(recentButton);
   impl_->recentFolderButtons_.append(recentButton);
  }
  leftHubCard->setLayout(leftHubLayout);

  auto assetModel = impl_->assetModel_ = new AssetMenuModel(this);
 auto fileView = impl_->fileView_ = new AssetFileListView();
  fileView->setAccessibleName(QStringLiteral("Asset file list"));
  fileView->setAccessibleDescription(QStringLiteral("Browse and select assets in the current folder"));
  fileView->setModel(assetModel);
  fileView->setItemDelegate(new AssetCardDelegate(fileView));
  impl_->currentDirectoryPath_ = desktopPath;  // Set initial directory
  const QString savedDirectoryPath = ArtifactCore::LayeredConfigStore::instance().valueString(
      QStringLiteral("AssetBrowser/CurrentDirectory"));
  if (!savedDirectoryPath.isEmpty() && QDir(savedDirectoryPath).exists()) {
    impl_->currentDirectoryPath_ = savedDirectoryPath;
  }
  fileView->setResizeMode(QListView::Adjust);
  fileView->setTextElideMode(Qt::ElideRight);
  fileView->setUniformItemSizes(true);  // Optimize rendering with uniform sizes
  fileView->setDragEnabled(true);
  fileView->setAcceptDrops(true);
  fileView->setDropIndicatorShown(true);
  fileView->setDragDropMode(QAbstractItemView::DragDrop);
  fileView->setDefaultDropAction(Qt::CopyAction);
  fileView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  fileView->setContextMenuPolicy(Qt::CustomContextMenu);  // Enable custom context menu
  fileView->setMouseTracking(true);
  fileView->viewport()->setMouseTracking(true);
  fileView->viewport()->installEventFilter(this);
  QObject::connect(fileView->verticalScrollBar(), &QScrollBar::valueChanged,
                   this, [this](int) {
                     if (impl_) {
                       impl_->warmVisibleThumbnails();
                     }
                   });
  QObject::connect(fileView->horizontalScrollBar(), &QScrollBar::valueChanged,
                   this, [this](int) {
                     if (impl_) {
                       impl_->warmVisibleThumbnails();
                     }
                   });
  fileView->setContentsMargins(8, 8, 8, 8);
  const QListView::ViewMode savedViewMode =
      config.valueInt64(QStringLiteral("AssetBrowser/ViewMode"),
                        static_cast<int>(QListView::IconMode)) ==
              static_cast<int>(QListView::ListMode)
          ? QListView::ListMode
          : QListView::IconMode;
  applyAssetBrowserViewMode(fileView, savedViewMode, impl_->thumbnailSizePx());
  if (gridViewButton && listViewButton) {
    gridViewButton->setChecked(savedViewMode == QListView::IconMode);
    listViewButton->setChecked(savedViewMode == QListView::ListMode);
  }
  {
   QPalette palette = fileView->palette();
   const auto& theme = ArtifactCore::currentDCCTheme();
   palette.setColor(QPalette::Base, QColor(12, 17, 22));
   palette.setColor(QPalette::AlternateBase, QColor(24, 30, 37));
   palette.setColor(QPalette::Text, QColor(theme.textColor));
   palette.setColor(QPalette::Highlight, QColor(theme.accentColor).darker(145));
   palette.setColor(QPalette::HighlightedText, QColor(0xF5, 0xF7, 0xFA));
   fileView->setPalette(palette);
   fileView->viewport()->setAutoFillBackground(true);
  }

  auto* viewModeGroup = new QButtonGroup(this);
  viewModeGroup->setExclusive(true);
  if (gridViewButton) {
    viewModeGroup->addButton(gridViewButton, static_cast<int>(QListView::IconMode));
  }
  if (listViewButton) {
    viewModeGroup->addButton(listViewButton, static_cast<int>(QListView::ListMode));
  }
  connect(viewModeGroup, &QButtonGroup::idClicked, this, [this, fileView](int id) {
    const auto mode = static_cast<QListView::ViewMode>(id);
    applyAssetBrowserViewMode(fileView, mode, impl_->thumbnailSizePx());
    ArtifactCore::LayeredConfigStore::instance().setValue(
        QStringLiteral("AssetBrowser/ViewMode"), static_cast<int>(mode));
  });

  // Connect search filter
  if (impl_->searchEdit_) {
   auto *searchCompleter = impl_->searchEdit_->completer();
   auto *searchHistoryModel = searchCompleter
       ? qobject_cast<QStringListModel *>(searchCompleter->model())
       : nullptr;
   connect(impl_->searchEdit_, &QLineEdit::textChanged, this,
           [this, searchCompleter, searchHistoryModel](const QString& text) {
    impl_->currentSearchFilter_ = text;
    impl_->applyFilters();
    const QString normalized = text.trimmed();
    if (normalized.size() < 2 || !searchHistoryModel) {
      return;
    }
    QStringList history = searchHistoryModel->stringList();
    history.removeAll(normalized);
    history.prepend(normalized);
    while (history.size() > 12) {
      history.removeLast();
    }
    searchHistoryModel->setStringList(history);
    QSettings settings;
    settings.setValue(QStringLiteral("AssetBrowser/SearchHistory"), history);
    Q_UNUSED(searchCompleter);
   });
  }

  if (impl_->upButton_) {
   connect(impl_->upButton_, &QToolButton::clicked, this, [this]() {
    if (impl_->currentDirectoryPath_.isEmpty()) return;
    const QString assetsRoot = ArtifactProjectService::instance()
                                   ? ArtifactProjectService::instance()->currentProjectAssetsPath()
                                   : QString();
    const QDir currentDir(impl_->currentDirectoryPath_);
    QString nextPath = QFileInfo(currentDir.absolutePath()).dir().absolutePath();
    if (nextPath.isEmpty()) {
     nextPath = assetsRoot;
    }
    if (!assetsRoot.isEmpty() && !nextPath.startsWith(assetsRoot, Qt::CaseInsensitive)) {
     nextPath = assetsRoot;
    }
    if (nextPath.isEmpty() || nextPath == impl_->currentDirectoryPath_) return;
    navigateToFolder(nextPath);
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
   case 4: impl_->currentFileTypeFilter_ = "3d"; break;
   }
   ArtifactCore::LayeredConfigStore::instance().setValue(
       QStringLiteral("AssetBrowser/FileTypeFilter"), impl_->currentFileTypeFilter_);
   impl_->applyFilters();
  });

  // Connect directory change to update file list (LEFT -> RIGHT widget coordination)
  connect(directoryView, &QTreeView::clicked, this, [this, directoryModel](const QModelIndex& index) {
   QString path = directoryModel->pathFromIndex(index);

   if (!path.isEmpty()) {
     navigateToFolder(path);
    }
   });

  // Connect file double-click to preview/select or navigate into a folder.
  // Import remains an explicit context-menu action so a sequence is never
  // accidentally registered as only its representative frame.
  connect(fileView, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
   if (!index.isValid()) return;
   AssetMenuItem item = impl_->assetModel_->itemAt(index.row());
   QString filePath = item.path.toQString();
   if (filePath.isEmpty()) return;
   itemDoubleClicked(filePath);

   // If it's a folder, navigate into it
   if (item.isFolder) {
    navigateToFolder(filePath);
    return;
   }
  });

  // Connect right-click context menu
  connect(fileView, &QListView::customContextMenuRequested, this, &ArtifactAssetBrowser::showContextMenu);

  // Connect file item selection to update details
  connect(fileView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
   QModelIndexList selectedIndexes = impl_->fileView_->selectionModel()->selectedIndexes();
   QStringList selectedFiles = impl_->selectedAssetPaths();
   if (!selectedIndexes.isEmpty()) {
    if (!selectedFiles.isEmpty()) {
     updateFileInfo(selectedFiles.first());
    }
   } else if (impl_->fileInfoLabel_) {
    impl_->fileInfoLabel_->setText(QStringLiteral("Open a file to inspect details"));
    if (impl_->filePreviewLabel_) {
     impl_->filePreviewLabel_->setPixmap(QPixmap());
     impl_->filePreviewLabel_->setText(QStringLiteral("Select an asset to preview"));
    }
   }
   selectionChanged(selectedFiles);
   impl_->refreshLeftHubSummary();
  });

  // Create thumbnail size adjustment
  auto thumbnailControlGroup = makeAssetBrowserPanel(this);
  auto thumbnailLayout = new QHBoxLayout();
  thumbnailLayout->setContentsMargins(8, 5, 8, 5);
  thumbnailLayout->setSpacing(6);

  auto sizeLabel = new QLabel(QStringLiteral("%1px").arg(kAssetThumbnailDefaultPx));
  auto sizeSlider = impl_->thumbnailSizeSlider_ = new QSlider(Qt::Horizontal);
  sizeSlider->setAccessibleName(QStringLiteral("Asset thumbnail size"));
  sizeSlider->setAccessibleDescription(QStringLiteral("Adjust the size of asset thumbnails"));
  sizeSlider->setMinimum(kAssetThumbnailMinPx);
  sizeSlider->setMaximum(kAssetThumbnailMaxPx);
  sizeSlider->setValue(kAssetThumbnailDefaultPx);
  sizeSlider->setTickPosition(QSlider::TicksBelow);
  sizeSlider->setTickInterval(25);

  connect(sizeSlider, &QSlider::valueChanged, this, [this, sizeLabel, fileView](int value) {
    sizeLabel->setText(QString("%1px").arg(value));
    impl_->setThumbnailSizePx(value);
    applyAssetBrowserViewMode(fileView, fileView->viewMode(), value);
  });
  connect(sizeSlider, &QSlider::sliderReleased, this, [this]() {
    impl_->clearThumbnailCache();
    impl_->applyFilters();
  });

  auto* thumbnailLabel = new QLabel("Thumbnail:");
  thumbnailLabel->setAccessibleName(QStringLiteral("Thumbnail size control"));
  thumbnailLayout->addWidget(thumbnailLabel);
  thumbnailLayout->addWidget(sizeSlider);
  thumbnailLayout->addWidget(sizeLabel);
  thumbnailControlGroup->setLayout(thumbnailLayout);

  // Create file info panel
  auto fileInfoGroup = makeAssetBrowserPanel(this);
  auto fileInfoLayout = new QHBoxLayout();
  fileInfoLayout->setContentsMargins(10, 10, 10, 10);
  fileInfoLayout->setSpacing(14);

  auto* filePreviewLabel = impl_->filePreviewLabel_ = new QLabel(fileInfoGroup);
  filePreviewLabel->setAccessibleName(QStringLiteral("Asset preview"));
  filePreviewLabel->setAccessibleDescription(QStringLiteral("Preview of the selected asset"));
  filePreviewLabel->setAlignment(Qt::AlignCenter);
  filePreviewLabel->setFixedSize(280, 164);
  filePreviewLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  filePreviewLabel->setText(QStringLiteral("Select an asset to preview"));
  {
   QPalette pal = filePreviewLabel->palette();
   const auto& theme = ArtifactCore::currentDCCTheme();
   pal.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor).darker(112));
   pal.setColor(QPalette::WindowText, QColor(theme.textColor).darker(135));
   filePreviewLabel->setAutoFillBackground(true);
   filePreviewLabel->setPalette(pal);
  }

  auto fileInfoLabel = impl_->fileInfoLabel_ = new QLabel(QStringLiteral("Open a file to inspect details"));
  fileInfoLabel->setAccessibleName(QStringLiteral("Asset details"));
  fileInfoLabel->setAccessibleDescription(QStringLiteral("Details for the selected asset"));
  fileInfoLabel->setWordWrap(true);
  fileInfoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  {
    QPalette pal = fileInfoLabel->palette();
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
    pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
    fileInfoLabel->setPalette(pal);
  }

  applyAssetBrowserPanelPalette(fileInfoLabel);

  fileInfoLayout->addWidget(filePreviewLabel);
  auto* detailsColumn = new QWidget(fileInfoGroup);
  auto* detailsLayout = new QVBoxLayout(detailsColumn);
  detailsLayout->setContentsMargins(0, 0, 0, 0);
  detailsLayout->setSpacing(6);
  auto* fileInfoTitle = new QLabel(QStringLiteral("Asset Details"), detailsColumn);
  fileInfoTitle->setAccessibleName(QStringLiteral("Asset details heading"));
  {
   QFont font = fileInfoTitle->font();
   font.setBold(true);
   font.setPointSizeF(std::max<qreal>(11.0, font.pointSizeF()));
   fileInfoTitle->setFont(font);
  }
  applyAssetBrowserPanelPalette(fileInfoTitle);
  detailsLayout->addWidget(fileInfoTitle);
  detailsLayout->addWidget(fileInfoLabel, 1);
  auto* importButton = new QPushButton(QStringLiteral("Import"), detailsColumn);
  importButton->setAccessibleName(QStringLiteral("Import selected asset"));
  importButton->setAccessibleDescription(QStringLiteral("Import the selected asset into the project"));
  importButton->setMinimumWidth(92);
  importButton->setMinimumHeight(30);
  importButton->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
  {
   QPalette pal = importButton->palette();
   const auto& theme = ArtifactCore::currentDCCTheme();
   pal.setColor(QPalette::Button, QColor(theme.accentColor));
   pal.setColor(QPalette::ButtonText, QColor(Qt::white));
   pal.setColor(QPalette::Highlight, QColor(theme.accentColor).lighter(112));
   pal.setColor(QPalette::HighlightedText, QColor(Qt::white));
   importButton->setAutoFillBackground(true);
   importButton->setPalette(pal);
  }
  importButton->setEnabled(false);
  detailsLayout->addWidget(importButton, 0, Qt::AlignRight);
  fileInfoLayout->addWidget(detailsColumn, 1);
  fileInfoGroup->setLayout(fileInfoLayout);
  fileInfoGroup->setFixedHeight(188);

  connect(fileView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, [fileView, importButton]() {
            importButton->setEnabled(
                fileView->selectionModel() &&
                !fileView->selectionModel()->selectedIndexes().isEmpty());
          });
  connect(importButton, &QPushButton::clicked, this, [this]() {
    if (!impl_ || !impl_->fileView_ || !impl_->fileView_->selectionModel()) {
      return;
    }
    QStringList paths;
    for (const QModelIndex& index :
         impl_->fileView_->selectionModel()->selectedIndexes()) {
      const AssetMenuItem item = impl_->assetModel_->itemAt(index.row());
      if (!item.isFolder && !item.path.toQString().isEmpty()) {
        paths.append(item.path.toQString());
      }
    }
    if (auto* service = ArtifactProjectService::instance();
        service && !paths.isEmpty()) {
      service->importAssetsFromPathsAsync(paths, {});
    }
  });

  // Initial load
  impl_->applyFilters();

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectCreatedEvent>([this](const ProjectCreatedEvent&) {
        impl_->syncProjectAssetRoot();
        if (impl_->syncStateLabel_) {
          impl_->syncStateLabel_->setText(impl_->syncStateText());
        }
      }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectChangedEvent>([this](const ProjectChangedEvent&) {
        impl_->syncProjectAssetRoot();
        if (impl_->syncStateLabel_) {
          impl_->syncStateLabel_->setText(impl_->syncStateText());
        }
      }));

  auto* navigationHeader = makeAssetBrowserPanel(this);
  auto* navigationHeaderLayout = new QVBoxLayout(navigationHeader);
  navigationHeaderLayout->setContentsMargins(12, 10, 12, 9);
  navigationHeaderLayout->setSpacing(8);
  auto* browserTitle = new QLabel(QStringLiteral("Asset Browser"), navigationHeader);
  {
   QFont font = browserTitle->font();
   font.setPointSizeF(std::max<qreal>(12.0, font.pointSizeF()));
   font.setBold(true);
   browserTitle->setFont(font);
   applyAssetBrowserPanelPalette(browserTitle);
  }
  auto* titleRow = new QHBoxLayout();
  titleRow->setContentsMargins(0, 0, 0, 0);
  titleRow->setSpacing(16);
  titleRow->addWidget(browserTitle);
  titleRow->addWidget(breadcrumbBar, 1);
  navigationHeaderLayout->addLayout(titleRow);
  navigationHeaderLayout->addWidget(assetToolBar);
  navigationHeaderLayout->addLayout(typeFiltersLayout);

  auto* browserSurface = makeAssetBrowserPanel(this);
  auto* VBoxLayout = new QVBoxLayout(browserSurface);
  VBoxLayout->setContentsMargins(0, 0, 0, 0);
  VBoxLayout->setSpacing(0);
  impl_->syncStateLabel_->hide();
  VBoxLayout->addWidget(fileView);
  VBoxLayout->addWidget(fileInfoGroup);
  thumbnailControlGroup->hide();

  auto* contentTabs = new QTabWidget(this);
  contentTabs->setObjectName(QStringLiteral("assetBrowserContentTabs"));
  contentTabs->setDocumentMode(true);
  contentTabs->addTab(browserSurface, QStringLiteral("Assets"));
  auto* templateLibrary = new Artifact::ArtifactTemplateLibraryWidget(contentTabs);
  templateLibrary->setObjectName(QStringLiteral("assetBrowserTemplateLibrary"));
  templateLibrary->setLibrary(Artifact::ArtifactTemplateLibrary());
  contentTabs->addTab(templateLibrary, QStringLiteral("Templates"));

  auto leftColumnLayout = new QVBoxLayout();
  leftColumnLayout->setContentsMargins(0, 0, 0, 0);
  leftColumnLayout->setSpacing(0);
  leftColumnLayout->addWidget(leftHubCard);
  auto* localSection = new QLabel(QStringLiteral("LOCAL"), this);
  {
   QFont font = localSection->font();
   font.setPointSizeF(std::max<qreal>(8.0, font.pointSizeF() - 1.0));
   font.setWeight(QFont::DemiBold);
   localSection->setFont(font);
   QPalette pal = localSection->palette();
   pal.setColor(QPalette::Window, QColor(18, 23, 29));
   pal.setColor(QPalette::WindowText,
                QColor(ArtifactCore::currentDCCTheme().textColor).darker(135));
   localSection->setAutoFillBackground(true);
   localSection->setPalette(pal);
   localSection->setContentsMargins(10, 9, 10, 5);
  }
  leftColumnLayout->addWidget(localSection);
  leftColumnLayout->addWidget(directoryView, 1);

  vLayout->addWidget(navigationHeader);
  layout->addLayout(leftColumnLayout, 2);
  layout->addWidget(contentTabs, 7);
  vLayout->addLayout(layout);
  setLayout(vLayout);

  impl_->refreshLeftHubSummary();

  impl_->setupFileSystemWatcher();
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

 void ArtifactAssetBrowser::selectionChanged(const QStringList& selectedFiles)
 {
  ArtifactCore::globalEventBus().publish<AssetBrowserSelectionChangedEvent>(
      AssetBrowserSelectionChangedEvent{selectedFiles});
 }

 void ArtifactAssetBrowser::itemDoubleClicked(const QString& itemPath)
 {
  ArtifactCore::globalEventBus().publish<AssetBrowserItemDoubleClickedEvent>(
      AssetBrowserItemDoubleClickedEvent{itemPath});
 }

 void ArtifactAssetBrowser::mousePressEvent(QMouseEvent* event)
 {
  impl_->defaultHandleMousePressEvent(event);
 }

 void ArtifactAssetBrowser::focusInEvent(QFocusEvent* event)
 {
  if (auto* input = InputOperator::instance()) {
   input->setActiveContext(QString::fromLatin1(kAssetBrowserContext));
  }
  QWidget::focusInEvent(event);
 }

 void ArtifactAssetBrowser::focusOutEvent(QFocusEvent* event)
 {
  if (auto* input = InputOperator::instance()) {
   if (input->activeContext() == QString::fromLatin1(kAssetBrowserContext)) {
    input->setActiveContext(QStringLiteral("Global"));
   }
  }
  QWidget::focusOutEvent(event);
 }

 void ArtifactAssetBrowser::keyPressEvent(QKeyEvent* event)
 {
  if (!impl_->fileView_ || !impl_->assetModel_) {
   QWidget::keyPressEvent(event);
   return;
  }

  if (auto* input = InputOperator::instance()) {
   input->setActiveContext(QString::fromLatin1(kAssetBrowserContext));
   if (event && input->processKeyPress(this, event->key(), event->modifiers())) {
    event->accept();
    return;
   }
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
     ArtifactProjectService::instance()->importAssetsFromPathsAsync(paths, {});
    }
   }
   event->accept();
   return;
  }

  // Delete — 選択ファイルをプロジェクトから削除 + 物理ファイル削除
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
   if (!(event->modifiers() & Qt::ControlModifier) && !(event->modifiers() & Qt::ShiftModifier)) {
    impl_->deleteSelected();
    event->accept();
    return;
   }
  }

  // F2 — リネーム
  if (event->key() == Qt::Key_F2) {
   impl_->renameSelected();
   event->accept();
   return;
  }

  // Ctrl+Shift+N — 新規フォルダ作成
  if (event->key() == Qt::Key_N && (event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier)) {
   impl_->createNewFolder();
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

bool ArtifactAssetBrowser::eventFilter(QObject* watched, QEvent* event)
{
  if (watched == impl_->searchEdit_ && event && event->type() == QEvent::KeyPress) {
   auto* keyEvent = static_cast<QKeyEvent*>(event);
   if (keyEvent->key() == Qt::Key_Escape && impl_->searchEdit_ && !impl_->searchEdit_->text().isEmpty()) {
    impl_->searchEdit_->clear();
    return true;
    }
  }

  if (impl_ && impl_->fileView_ && watched == impl_->fileView_->viewport() && event) {
    switch (event->type()) {
      case QEvent::Leave:
        impl_->hideHoverPreview();
        break;
      case QEvent::MouseMove: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (!mouseEvent) {
          break;
        }
        const QModelIndex index = impl_->fileView_->indexAt(mouseEvent->pos());
        if (!index.isValid()) {
          impl_->hideHoverPreview();
          break;
        }
        const AssetMenuItem item = impl_->assetModel_->itemAt(index.row());
        const QString filePath = item.path.toQString();
        if (filePath.isEmpty() || impl_->isHoverPreviewPath(filePath)) {
          break;
        }
        impl_->scheduleHoverPreview(filePath, mouseEvent->globalPosition().toPoint());
        break;
      }
      default:
        break;
    }
  }

  return QWidget::eventFilter(watched, event);
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
   selectionChanged(normalizedPaths);
  }
  impl_->refreshLeftHubSummary();
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
     svc->importAssetsFromPathsAsync(filePaths, {});
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
   QString normalizedType = type.trimmed().toLower();
   if (normalizedType != QStringLiteral("all") &&
       normalizedType != QStringLiteral("images") &&
       normalizedType != QStringLiteral("videos") &&
       normalizedType != QStringLiteral("audio") &&
       normalizedType != QStringLiteral("3d")) {
    normalizedType = QStringLiteral("all");
   }
   impl_->currentFileTypeFilter_ = normalizedType;
   ArtifactCore::LayeredConfigStore::instance().setValue(
       QStringLiteral("AssetBrowser/FileTypeFilter"), normalizedType);

   // Update button state
   if (impl_->filterButtonGroup_) {
    if (normalizedType == "all") impl_->filterButtonGroup_->button(0)->setChecked(true);
    else if (normalizedType == "images") impl_->filterButtonGroup_->button(1)->setChecked(true);
    else if (normalizedType == "videos") impl_->filterButtonGroup_->button(2)->setChecked(true);
    else if (normalizedType == "audio") impl_->filterButtonGroup_->button(3)->setChecked(true);
    else if (normalizedType == "3d") impl_->filterButtonGroup_->button(4)->setChecked(true);
   }

   impl_->applyFilters();
  }

  void ArtifactAssetBrowser::setStatusFilter(const QString& status)
  {
   QString normalizedStatus = status.trimmed().toLower();
   if (normalizedStatus != QStringLiteral("all") &&
       normalizedStatus != QStringLiteral("imported") &&
       normalizedStatus != QStringLiteral("favorite") &&
       normalizedStatus != QStringLiteral("missing") &&
       normalizedStatus != QStringLiteral("unused")) {
    normalizedStatus = QStringLiteral("all");
   }
   impl_->currentStatusFilter_ = normalizedStatus;
   ArtifactCore::LayeredConfigStore::instance().setValue(
       QStringLiteral("AssetBrowser/StatusFilter"), normalizedStatus);
   impl_->applyFilters();
   impl_->refreshLeftHubSummary();
  }

 void ArtifactAssetBrowser::navigateToFolder(const QString& folderPath)
  {
  if (folderPath.isEmpty() || !QDir(folderPath).exists()) return;
  impl_->currentDirectoryPath_ = folderPath;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("AssetBrowser/CurrentDirectory"), folderPath);
  if (impl_->directoryModel_) {
   impl_->directoryModel_->addRecentPath(folderPath, QFileInfo(folderPath).fileName());
  }
  impl_->watchCurrentDirectory();
  impl_->clearThumbnailCache();
  if (impl_->breadcrumb_) {
   impl_->breadcrumb_->setPath(folderPath);
  }
  impl_->applyFilters();
  impl_->syncDirectorySelection();
  impl_->refreshLeftHubSummary();
}

 void ArtifactAssetBrowser::updateFileInfo(const QString& filePath)
 {
  if (filePath.isEmpty() || !impl_->fileInfoLabel_) return;

  QFileInfo fileInfo(filePath);

  // Resolve the browser item so sequences share the same aggregated status
  // as the row markers (preview / import / relink all read this one summary).
  AssetMenuItem assetItem;
  const bool isSequenceItem = impl_->findAssetItemByPath(filePath, &assetItem) && assetItem.isSequence;
  const QStringList sequencePaths = isSequenceItem ? assetItem.sequencePaths : QStringList{};

 if (!fileInfo.exists()) {
   QString info = QString("<b>%1</b><br>").arg(fileInfo.fileName());
   info += QStringLiteral("File not found<br>");
   info += impl_->assetStatusInfoHtml(impl_->assetStatusForPaths(filePath, sequencePaths));
   impl_->fileInfoLabel_->setText(info);
   if (impl_->filePreviewLabel_) {
    impl_->filePreviewLabel_->setPixmap(QPixmap());
    impl_->filePreviewLabel_->setText(QStringLiteral("Preview unavailable"));
   }
   return;
  }

  if (impl_->filePreviewLabel_) {
   const QSize previewSize(std::max(160, impl_->filePreviewLabel_->width() - 12), 144);
   const QIcon previewIcon = impl_->getFileIcon(fileInfo.fileName(), filePath);
   const QPixmap previewPixmap = previewIcon.pixmap(previewSize);
   if (!previewPixmap.isNull()) {
    impl_->filePreviewLabel_->setText(QString());
    impl_->filePreviewLabel_->setPixmap(previewPixmap.scaled(
        previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
   } else {
    impl_->filePreviewLabel_->setPixmap(QPixmap());
    impl_->filePreviewLabel_->setText(fileInfo.isDir()
        ? QStringLiteral("Folder")
        : QStringLiteral("Preview unavailable"));
   }
  }

// Build information string
   QString info;
   info += QString("<b>%1</b><br>").arg(fileInfo.fileName());
   const auto assetMeta = ArtifactCore::ArtifactAssetMetaFile::load(filePath);
   if (assetMeta.isValid()) {
    const QStringList tags = assetMeta.tags();
    if (!tags.isEmpty()) {
     info += QStringLiteral("Tags: %1<br>").arg(tags.join(QStringLiteral(", ")));
    }
    const QString proxy = assetMeta.proxyPath(QStringLiteral("2K"));
    if (!proxy.isEmpty()) {
     info += QStringLiteral("Proxy: %1<br>").arg(QFileInfo(proxy).fileName());
    }
    const QVariant vectorKind = assetMeta.customValue(QStringLiteral("vector/sourceKind"));
    if (vectorKind.isValid()) {
     info += QStringLiteral("Vector: %1 (%2)<br>")
         .arg(vectorKind.toInt())
         .arg(assetMeta.customValue(QStringLiteral("vector/editable")).toBool()
                  ? QStringLiteral("editable")
                  : QStringLiteral("preview"));
    }
    const QVariant imageWidth = assetMeta.customValue(QStringLiteral("image/width"));
    const QVariant imageHeight = assetMeta.customValue(QStringLiteral("image/height"));
    if (imageWidth.isValid() && imageHeight.isValid()) {
     info += QStringLiteral("Image: %1 × %2, %3-bit, %4 channels%5<br>")
         .arg(imageWidth.toInt())
         .arg(imageHeight.toInt())
         .arg(assetMeta.customValue(QStringLiteral("image/bitDepth")).toInt())
         .arg(assetMeta.customValue(QStringLiteral("image/channels")).toInt())
         .arg(assetMeta.customValue(QStringLiteral("image/hdr")).toBool()
                  ? QStringLiteral(" (HDR)")
                  : QString());
    }
   }

   // Check if it's a folder
  if (fileInfo.isDir()) {
    info += "Type: Folder<br>";
    info += QString("Entries: %1<br>").arg(QDir(filePath).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).size());
    info += impl_->assetStatusInfoHtml(impl_->assetStatusForPaths(filePath));
    impl_->fileInfoLabel_->setText(info);
    return;
   }

   info += QString("Size: %1 KB<br>").arg(fileInfo.size() / 1024);
   const QString lowerName = fileInfo.fileName().toLower();
   if (lowerName.endsWith(QStringLiteral(".mask.json"))) {
    info += QStringLiteral("Type: Mask Preset<br>");
   } else {
    info += QString("Type: %1<br>").arg(fileInfo.suffix().toUpper());
   }
   info += QString("Modified: %1<br>").arg(fileInfo.lastModified().toString("yyyy-MM-dd hh:mm"));
   info += impl_->assetStatusInfoHtml(impl_->assetStatusForPaths(filePath, sequencePaths));
   info += QString("Source Uses: %1<br>").arg(impl_->sourceUseCountForPath(filePath, sequencePaths));
   info += QString("Thumbnail: %1<br>").arg(impl_->thumbnailDebugStatus(filePath).toHtmlEscaped());

   // A sequence row is represented by its first frame path. Surface the
   // logical sequence metadata here so selection does not look like a single
   // still image in the inspector.
   if (impl_->assetModel_) {
    for (int row = 0; row < impl_->assetModel_->rowCount(); ++row) {
     const AssetMenuItem sequenceItem = impl_->assetModel_->itemAt(row);
     if (!sequenceItem.isSequence ||
         (!sequenceItem.sequencePaths.contains(filePath) &&
          sequenceItem.path.toQString() != filePath)) {
      continue;
     }
     info += QString("Sequence Frames: %1<br>").arg(sequenceItem.sequenceFrameCount);
     info += QString("Frame Start: %1<br>").arg(sequenceItem.sequenceStartFrame);
     info += QString("Padding: %1 digits<br>").arg(sequenceItem.sequencePadding);
     info += QString("Sequence Status: %1<br>")
                 .arg(sequenceItem.type.toQString().toHtmlEscaped());
     if (filePath != sequenceItem.path.toQString()) {
       const QFileInfo selectedFrameInfo(filePath);
      const QRegularExpressionMatch selectedMatch =
          QRegularExpression(QStringLiteral(R"(\d{3,})"))
              .match(selectedFrameInfo.fileName());
      if (selectedMatch.hasMatch()) {
       info += QString("Selected Frame: %1<br>")
                   .arg(selectedMatch.captured(0).toLongLong());
       }
      }
      QImageReader selectedReader(filePath);
      QString frameStatus = selectedReader.canRead()
          ? QStringLiteral("OK")
          : QStringLiteral("Unreadable");
      if (selectedReader.canRead() && !sequenceItem.sequencePaths.isEmpty()) {
       QImageReader representativeReader(sequenceItem.sequencePaths.first());
       const QSize representativeSize = representativeReader.size();
       const QSize selectedSize = selectedReader.size();
       if (representativeSize.isValid() && selectedSize.isValid() &&
           representativeSize != selectedSize) {
        frameStatus = QStringLiteral("Size Mismatch");
       }
      }
      info += QString("Selected Frame Status: %1<br>")
                  .arg(frameStatus.toHtmlEscaped());
     break;
    }
   }

   // Get detailed information based on file type
   const QString fileName = fileInfo.fileName();

   // Image files
   if (lowerName.endsWith(".png") || lowerName.endsWith(".jpg") ||
       lowerName.endsWith(".jpeg") || lowerName.endsWith(".bmp") ||
       lowerName.endsWith(".gif") || lowerName.endsWith(".tga") ||
       lowerName.endsWith(".tiff") || lowerName.endsWith(".webp") ||
       lowerName.endsWith(".hdr") || lowerName.endsWith(".exr") ||
       lowerName.endsWith(".ico") || lowerName.endsWith(".dds") ||
       lowerName.endsWith(".ktx") || lowerName.endsWith(".psd") ||
       lowerName.endsWith(".psb")) {
    QSize imageSize;
    QString imageError;
    const QImage imagePreview = AssetThumbnail::loadImageViaOIIO(filePath, QSize(), &imageError);
    if (!imagePreview.isNull()) {
     imageSize = imagePreview.size();
    }
    if (imageSize.isValid()) {
     info += QString("Resolution: %1 x %2 px<br>").arg(imageSize.width()).arg(imageSize.height());
     const QString suffix = fileInfo.suffix().toUpper();
     if (!suffix.isEmpty()) {
      info += QString("Format: %1<br>").arg(suffix);
     }
    } else if (!imageError.isEmpty()) {
     info += QString("Image decode: %1<br>").arg(imageError);
    }
   }
   // Video files
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
      const double duration = frameCount / fps;
      info += QString("Duration: %1 s<br>").arg(QString::number(duration, 'f', 2));
     }
    }
   }
   // Audio files
   else if (impl_->isAudioFile(fileName)) {
    // Basic audio information
    info += QString("Kind: Audio<br>");
    // Additional audio metadata could be added here if needed
   }
   // Font files
   else if (impl_->isFontFile(fileName)) {
    info += QString("Kind: Font<br>");
    // Font-specific information could be added here if needed
   }
   else if (fileName.toLower().endsWith(QStringLiteral(".mask.json"))) {
    info += QString("Kind: Mask Preset<br>");
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
  QMenu* frequentMenu = contextMenu.addMenu(QStringLiteral("Frequent"));
  QMenu* allMenu = contextMenu.addMenu(QStringLiteral("All"));

  auto addAction = [this](QMenu* menu, const QString& text, auto&& callback) -> QAction* {
    if (!menu) {
      return nullptr;
    }
    QAction* action = menu->addAction(text);
    connect(action, &QAction::triggered, this, std::forward<decltype(callback)>(callback));
    return action;
  };

  // New Folder action (always available)
  addAction(frequentMenu, QStringLiteral("New Folder (Ctrl+Shift+N)"), [this]() {
   if (impl_) {
    impl_->createNewFolder();
   }
  });

  const QStringList selectedAssetPaths = impl_->selectedAssetPaths();
  const QStringList selectedTargets = selectedAssetPaths.isEmpty() ? QStringList{filePath} : selectedAssetPaths;

  // Expand sequences to every frame so the import keeps the whole sequence
  // relationship (the service groups the frames back into one footage item).
  // findAssetItemByPath also resolves clicked frame rows to their parent
  // sequence, matching the frame-row import behavior.
  QStringList importTargets;
  int expandedSequenceFrames = 0;
  int expandedSequenceCount = 0;
  for (const QString& target : selectedTargets) {
   AssetMenuItem targetItem;
   if (impl_->findAssetItemByPath(target, &targetItem) &&
       targetItem.isSequence && !targetItem.sequencePaths.isEmpty()) {
    importTargets.append(targetItem.sequencePaths);
    expandedSequenceFrames += targetItem.sequencePaths.size();
    ++expandedSequenceCount;
   } else {
    importTargets.append(target);
   }
  }
  importTargets.removeDuplicates();

  // Add to Project action (import operation names show the frame count)
  QString addActionLabel;
  if (expandedSequenceCount == 1 && selectedTargets.size() == 1) {
   addActionLabel = QStringLiteral("Add Image Sequence (%1 frames) to Project")
       .arg(expandedSequenceFrames);
  } else if (selectedTargets.size() > 1) {
   addActionLabel = expandedSequenceCount > 0
    ? QStringLiteral("Add %1 Items to Project (%2 sequence frames)")
       .arg(selectedTargets.size()).arg(expandedSequenceFrames)
    : QStringLiteral("Add %1 Items to Project").arg(selectedTargets.size());
  } else {
   addActionLabel = QStringLiteral("Add to Project");
  }
  addAction(frequentMenu, addActionLabel, [this, importTargets, filePath]() {
   if (importTargets.isEmpty() && filePath.isEmpty()) return;
   auto* svc = ArtifactProjectService::instance();
   if (!svc) return;
   const QStringList requested =
       importTargets.isEmpty() ? QStringList{filePath} : importTargets;
   QPointer<ArtifactAssetBrowser> owner(this);
   svc->importAssetsFromPathsAsync(
       requested, [owner, requested, filePath](QStringList imported) {
    if (!owner) return;
    if (imported.isEmpty()) {
     QMessageBox::warning(
         owner, QStringLiteral("Import Failed"),
         QStringLiteral("No requested files could be imported."));
     return;
    }
    if (imported.size() < requested.size()) {
     QMessageBox::warning(
         owner, QStringLiteral("Import Incomplete"),
         QStringLiteral("Imported %1 of %2 requested files.")
             .arg(imported.size())
             .arg(requested.size()));
    }
    if (auto* service = ArtifactProjectService::instance()) {
     if (auto project = service->getCurrentProjectSharedPtr()) {
       QStringList unrecorded;
       for (const QString& importedPath : imported) {
        auto* undo = UndoManager::instance();
        if (undo) {
         if (!undo->push(std::make_unique<AssetRegistrationCommand>(
                 project, importedPath))) {
          project->removeAssetByPath(importedPath);
          unrecorded.append(importedPath);
         }
        } else {
         project->addAssetFromPath(importedPath);
        }
      }
      if (!unrecorded.isEmpty()) {
       QMessageBox::warning(
           owner, QStringLiteral("Import Undo Not Recorded"),
           QStringLiteral("%1 imported asset(s) could not be added to Undo history and were removed from the project.")
               .arg(unrecorded.size()));
      }
     }
    }
    owner->impl_->applyFilters();
    // Keep the info/preview pane in sync with the refreshed row status.
    owner->updateFileInfo(filePath.isEmpty() ? imported.first() : filePath);
   });
  });

  if (!item.isFolder) {
   addAction(frequentMenu, QStringLiteral("Preview in Contents Viewer"), [this, filePath]() {
    if (filePath.isEmpty()) return;
    itemDoubleClicked(filePath);
   });
  }

  if (item.isSequence) {
   const bool expanded = impl_->expandedSequencePaths_.contains(filePath);
   addAction(frequentMenu,
             expanded ? QStringLiteral("Collapse Sequence Frames")
                      : QStringLiteral("Expand Sequence Frames"),
             [this, filePath, expanded]() {
    if (filePath.isEmpty()) return;
    if (expanded) {
     impl_->expandedSequencePaths_.remove(filePath);
    } else {
     impl_->expandedSequencePaths_.insert(filePath);
    }
    impl_->applyFilters();
   });
  }

if (item.isFolder) {
    addAction(frequentMenu, QStringLiteral("Open Folder"), [this, filePath]() {
     if (filePath.isEmpty()) return;
     impl_->currentDirectoryPath_ = filePath;
     impl_->clearThumbnailCache();
     impl_->applyFilters();
     impl_->syncDirectorySelection();
    });
   }

// Relink action for footage items
if (!item.isFolder) {
  addAction(allMenu, QStringLiteral("Relink Selected Footage..."), [this, filePath, item]() {
    if (filePath.isEmpty()) return;
    // Show file dialog to select new file path
    QString newPath = QFileDialog::getOpenFileName(nullptr, "Relink Footage", QDir::homePath(), "All Files (*.*)");
    if (newPath.isEmpty()) return;
    // Relink the footage item by path
    auto* svc = ArtifactProjectService::instance();
    if (!svc) return;
      bool success = svc->relinkFootageByPath(filePath, newPath);
    if (success) {
      auto* undo = UndoManager::instance();
      if (undo && !undo->push(
              std::make_unique<RelinkAssetCommand>(filePath, newPath))) {
        svc->relinkFootageByPath(newPath, filePath);
        QMessageBox::warning(
            this, QStringLiteral("Relink Not Recorded"),
            QStringLiteral("The relink could not be recorded in Undo history and was reverted."));
        return;
      }
      impl_->applyFilters();
      // Keep the info/preview pane in sync with the refreshed row status.
      updateFileInfo(newPath);
    } else {
      QMessageBox::warning(
          this, QStringLiteral("Relink Failed"),
          item.isSequence
              ? QStringLiteral(
                    "The image sequence could not be relinked.\n\n"
                    "Choose a representative frame whose directory contains "
                    "every expected frame with matching numbering and padding.\n\n"
                    "Current status: %1")
                    .arg(item.type.toQString())
              : QStringLiteral("The selected footage could not be relinked."));
    }
  });

  addAction(allMenu, QStringLiteral("Find Relink Candidates..."),
            [this, filePath]() {
    if (filePath.isEmpty()) return;
    auto* svc = ArtifactProjectService::instance();
    if (!svc) return;
    const QString searchRoot = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Search for Relink Candidates"),
        QDir::homePath());
    if (searchRoot.isEmpty()) return;

    const auto candidates =
        svc->findRelinkCandidates(filePath, searchRoot, 32);
    if (candidates.isEmpty()) {
      QMessageBox::information(
          this, QStringLiteral("No Relink Candidates"),
          QStringLiteral("No matching candidates were found below:\n%1")
              .arg(searchRoot));
      return;
    }

    QStringList labels;
    labels.reserve(candidates.size());
    for (const auto& candidate : candidates) {
      QString sequenceSummary;
      if (candidate.sequenceExpectedFrames > 0) {
        sequenceSummary = QStringLiteral("\nSequence: %1/%2 frames found")
                              .arg(candidate.sequenceFoundFrames)
                              .arg(candidate.sequenceExpectedFrames);
      }
      labels.append(QStringLiteral("[%1] %2\n%3%4")
                        .arg(candidate.score)
                        .arg(candidate.path)
                        .arg(candidate.reason)
                        .arg(sequenceSummary));
    }
    bool accepted = false;
    const QString selected = QInputDialog::getItem(
        this, QStringLiteral("Select Relink Candidate"),
        QStringLiteral("Candidate:"), labels, 0, false, &accepted);
    if (!accepted) return;
    const int selectedIndex = labels.indexOf(selected);
    if (selectedIndex < 0 || selectedIndex >= candidates.size()) return;

    const QString newPath = candidates.at(selectedIndex).path;
    if (!svc->relinkFootageByPath(filePath, newPath)) {
      QMessageBox::warning(this, QStringLiteral("Relink Failed"),
                           QStringLiteral("The selected candidate could not be relinked."));
      return;
    }
    auto* undo = UndoManager::instance();
    if (undo && !undo->push(
            std::make_unique<RelinkAssetCommand>(filePath, newPath))) {
      svc->relinkFootageByPath(newPath, filePath);
      QMessageBox::warning(
          this, QStringLiteral("Relink Not Recorded"),
          QStringLiteral("The relink could not be recorded in Undo history and was reverted."));
      return;
    }
    impl_->applyFilters();
    updateFileInfo(newPath);
  });

  if (selectedTargets.size() > 1) {
    addAction(allMenu, QStringLiteral("Find Relink Candidates for Selected..."),
              [this, selectedTargets]() {
      auto* svc = ArtifactProjectService::instance();
      if (!svc) return;
      const QString searchRoot = QFileDialog::getExistingDirectory(
          this, QStringLiteral("Search for Relink Candidates"),
          QDir::homePath());
      if (searchRoot.isEmpty()) return;

      const auto project = svc->getCurrentProjectSharedPtr();
      const QVector<QString> sourceProperties = {
          QStringLiteral("image.sourcePath"),
          QStringLiteral("video.sourcePath"),
          QStringLiteral("audio.sourcePath"),
          QStringLiteral("svg.sourcePath")};
      const auto referenceCount = [&](const QString& oldPath) {
        if (!project) return 0;
        const QString normalizedOldPath = QDir::cleanPath(
            QFileInfo(oldPath).absoluteFilePath());
        int count = 0;
        std::function<void(ProjectItem*)> visit = [&](ProjectItem* item) {
          if (!item) return;
          if (item->type() == eProjectItemType::Composition) {
            const auto* compositionItem =
                static_cast<const CompositionItem*>(item);
            const auto composition = project->findComposition(
                compositionItem->compositionId).ptr.lock();
            if (composition) {
              for (const auto& layer : composition->allLayerRef()) {
                if (!layer) continue;
                const QJsonObject layerJson = layer->toJson();
                for (const QString& property : sourceProperties) {
                  const QString path = layerJson.value(property).toString();
                  if (!path.isEmpty() &&
                      QDir::cleanPath(QFileInfo(path).absoluteFilePath()) ==
                          normalizedOldPath) {
                    ++count;
                    break;
                  }
                }
              }
            }
          }
          for (auto* child : item->children) visit(child);
        };
        for (auto* root : project->projectItems()) visit(root);
        return count;
      };

      QVector<QPair<QString, QString>> changes;
      QSet<QString> processedFootagePaths;
      for (const QString& selectedPath : selectedTargets) {
        auto* footage = svc->findFootageItemByPath(selectedPath);
        const QString oldPath = footage ? footage->filePath : selectedPath;
        const QString normalizedOldPath =
            QDir::cleanPath(QFileInfo(oldPath).absoluteFilePath());
        if (processedFootagePaths.contains(normalizedOldPath)) continue;
        processedFootagePaths.insert(normalizedOldPath);

        const auto candidates = svc->findRelinkCandidates(oldPath, searchRoot, 32);
        if (candidates.isEmpty()) {
          QMessageBox::warning(
              this, QStringLiteral("Batch Relink Cancelled"),
              QStringLiteral("No candidate was found for:\n%1").arg(oldPath));
          return;
        }
        const int references = referenceCount(oldPath);
        QStringList labels;
        for (const auto& candidate : candidates) {
          QString sequenceSummary;
          if (candidate.sequenceExpectedFrames > 0) {
            sequenceSummary = QStringLiteral("\nSequence: %1/%2 frames found")
                                  .arg(candidate.sequenceFoundFrames)
                                  .arg(candidate.sequenceExpectedFrames);
          }
          labels.append(QStringLiteral("[%1] %2\n%3%4\nReferences: %5")
                            .arg(candidate.score)
                            .arg(candidate.path)
                            .arg(candidate.reason)
                            .arg(sequenceSummary)
                            .arg(references));
        }
        bool accepted = false;
        const QString selected = QInputDialog::getItem(
            this, QStringLiteral("Select Relink Candidate"),
            QStringLiteral("Candidate for:\n%1").arg(oldPath), labels, 0,
            false, &accepted);
        if (!accepted) return;
        const int index = labels.indexOf(selected);
        if (index < 0 || index >= candidates.size()) return;
        changes.append(qMakePair(oldPath, candidates.at(index).path));
      }

      QVector<RelinkLayerSourceChange> layerChanges;
      const QVector<QPair<QString, QString>> layerSourceProperties = {
          {QStringLiteral("image.sourcePath"), QStringLiteral("image.sourcePath")},
          {QStringLiteral("video.sourcePath"), QStringLiteral("video.sourcePath")},
          {QStringLiteral("audio.sourcePath"), QStringLiteral("audio.sourcePath")},
          {QStringLiteral("svg.sourcePath"), QStringLiteral("svg.sourcePath")}};
      if (project) {
        std::function<void(ProjectItem*)> visit = [&](ProjectItem* item) {
          if (!item) return;
          if (item->type() == eProjectItemType::Composition) {
            const auto* compositionItem =
                static_cast<const CompositionItem*>(item);
            const auto composition = project->findComposition(
                compositionItem->compositionId).ptr.lock();
            if (composition) {
              for (const auto& layer : composition->allLayerRef()) {
                if (!layer) continue;
                const QJsonObject layerJson = layer->toJson();
                for (const auto& property : layerSourceProperties) {
                  const QString oldPath =
                      layerJson.value(property.first).toString();
                  for (const auto& change : changes) {
                    const QString normalizedLayerPath = QDir::cleanPath(
                        QFileInfo(oldPath).absoluteFilePath());
                    const QString normalizedFootagePath = QDir::cleanPath(
                        QFileInfo(change.first).absoluteFilePath());
                    if (!oldPath.isEmpty() &&
                        normalizedLayerPath == normalizedFootagePath) {
                      layerChanges.append({layer, property.second, oldPath,
                                           change.second});
                      break;
                    }
                  }
                }
              }
            }
          }
          for (auto* child : item->children) visit(child);
        };
        for (auto* root : project->projectItems()) visit(root);
      }

      QVector<QPair<QString, QString>> applied;
      for (const auto& change : changes) {
        if (!svc->relinkFootageByPath(change.first, change.second)) {
          for (auto it = applied.crbegin(); it != applied.crend(); ++it) {
            svc->relinkFootageByPath(it->second, it->first);
          }
          QMessageBox::warning(
              this, QStringLiteral("Batch Relink Failed"),
              QStringLiteral("No changes were kept because relinking failed for:\n%1")
                  .arg(change.first));
          return;
        }
        applied.append(change);
      }
      QVector<RelinkLayerSourceChange> appliedLayers;
      for (const auto& change : layerChanges) {
        auto layer = change.layer.lock();
        if (!layer || !layer->setLayerPropertyValue(change.propertyPath,
                                                     change.newPath)) {
          for (auto it = appliedLayers.crbegin(); it != appliedLayers.crend();
               ++it) {
            if (auto previous = it->layer.lock()) {
              previous->setLayerPropertyValue(it->propertyPath, it->oldPath);
            }
          }
          for (auto it = applied.crbegin(); it != applied.crend(); ++it) {
            svc->relinkFootageByPath(it->second, it->first);
          }
          QMessageBox::warning(
              this, QStringLiteral("Batch Relink Failed"),
              QStringLiteral("A layer source could not be updated; all changes were rolled back."));
          return;
        }
        appliedLayers.append(change);
      }
      const auto rollbackApplied = [&]() {
        for (auto it = layerChanges.crbegin(); it != layerChanges.crend(); ++it) {
          if (auto layer = it->layer.lock()) {
            layer->setLayerPropertyValue(it->propertyPath, it->oldPath);
          }
        }
        for (auto it = applied.crbegin(); it != applied.crend(); ++it) {
          svc->relinkFootageByPath(it->second, it->first);
        }
      };
      auto command = std::make_unique<RelinkAssetBatchCommand>(
          applied, layerChanges);
      auto* undo = UndoManager::instance();
      if (undo && !undo->push(std::move(command))) {
        rollbackApplied();
        QMessageBox::warning(
            this, QStringLiteral("Batch Relink Not Recorded"),
            QStringLiteral("The relink could not be recorded in Undo history and was reverted."));
        return;
      }
      impl_->applyFilters();
    });
  }
}

  // Interpret Footage action for media files
  if (!item.isFolder && (impl_->isVideoFile(filePath) || impl_->isImageFile(filePath))) {
    QAction* interpretAction = contextMenu.addAction("Interpret Footage...");
    connect(interpretAction, &QAction::triggered, this, [this, filePath]() {
      if (filePath.isEmpty()) return;
      auto* svc = ArtifactProjectService::instance();
      if (!svc) return;
      auto project = svc->getCurrentProjectSharedPtr();
      if (!project) return;
      Artifact::FootageItem* footage = nullptr;
      for (auto* root : project->projectItems()) {
        std::function<void(ProjectItem*)> walk = [&](ProjectItem* item) {
          if (!item) return;
          if (item->type() == Artifact::eProjectItemType::Footage) {
            auto* fi = static_cast<Artifact::FootageItem*>(item);
            if (fi->filePath == filePath || fi->sequencePaths.contains(filePath)) {
              footage = fi;
              return;
            }
          }
          for (auto* child : item->children) walk(child);
        };
        walk(root);
        if (footage) break;
      }
      if (!footage) return;
      auto& interpretSvc = FootageInterpretService::instance();
      auto report = interpretSvc.preflightChange(footage, footage->frameRate);
      ArtifactWidgets::InterpretFootageDialog dialog(
          QFileInfo(filePath).fileName(),
          footage->frameRate,
          footage->frameRate,
          report.affectedLayerCount,
          report.affectedKeyframeCount,
          report.hasTimeRemap);
      if (dialog.exec() == QDialog::Accepted) {
        double newFps = dialog.selectedFrameRate();
        int mode = dialog.selectedPreserveMode();
        FrameRatePreserveMode preserveMode;
        switch (mode) {
          case 0: preserveMode = FrameRatePreserveMode::KeepKeyframes; break;
          case 1: preserveMode = FrameRatePreserveMode::KeepTime; break;
          default: preserveMode = FrameRatePreserveMode::ReSample; break;
        }
        QString error;
        interpretSvc.applyFrameRateChange(footage, newFps, preserveMode, &error);
        if (error.isEmpty()) {
          interpretSvc.applyColorInterpretation(
              footage, dialog.selectedInputColorSpace(),
              dialog.selectedInputTransferFunction(), &error);
        }
        if (!error.isEmpty()) {
          QMessageBox::warning(nullptr, "Interpret Footage", error);
        }
        impl_->applyFilters();
      }
    });
  }

  // Open in File Explorer action
  // Open in File Explorer action
  const bool favorite = impl_->isFavoriteAssetPath(filePath);
  addAction(frequentMenu, favorite ? QStringLiteral("Remove from Favorites") : QStringLiteral("Add to Favorites"), [this, filePath]() {
   if (filePath.isEmpty()) return;
   if (!impl_->directoryModel_) return;
   impl_->toggleFavoritePath(filePath);
   impl_->applyFilters();
   impl_->syncDirectorySelection();
  });

  addAction(frequentMenu, QStringLiteral("Find References"), [this, filePath, item]() {
    if (filePath.isEmpty()) return;
    auto *service = ArtifactProjectService::instance();
    const auto project = service ? service->getCurrentProjectSharedPtr()
                                 : ArtifactProjectPtr{};
    QStringList referencePaths;
    if (item.isSequence && !item.sequencePaths.isEmpty()) {
      referencePaths = item.sequencePaths;
    } else {
      referencePaths = {filePath};
    }
    QStringList references;
    if (project) {
      std::function<void(ProjectItem*)> visit = [&](ProjectItem *item) {
        if (!item) return;
        if (item->type() == eProjectItemType::Composition) {
          const auto *compositionItem = static_cast<const CompositionItem *>(item);
          const auto composition = project->findComposition(compositionItem->compositionId).ptr.lock();
          if (composition) {
            for (const auto &layer : composition->allLayerRef()) {
              if (!layer) continue;
              const QByteArray serialized =
                  QJsonDocument(layer->toJson()).toJson(QJsonDocument::Compact);
              const QString serializedLayer = QString::fromUtf8(serialized);
              const bool matchesReference = std::any_of(
                  referencePaths.cbegin(), referencePaths.cend(),
                  [&serializedLayer](const QString& referencePath) {
                    return !referencePath.isEmpty() &&
                           serializedLayer.contains(referencePath, Qt::CaseInsensitive);
                  });
              if (matchesReference) {
                references.push_back(QStringLiteral("Composition %1 / %2 (%3)")
                                         .arg(composition->id().toString(),
                                              layer->layerName(), layer->id().toString()));
              }
            }
          }
        }
        for (auto *child : item->children) visit(child);
      };
      for (auto *root : project->projectItems()) visit(root);
    }
    QMessageBox::information(
        this, QStringLiteral("Find References"),
        references.isEmpty()
            ? QStringLiteral("No references found in the current project.")
            : QStringLiteral("References in the current project:\n\n%1")
                  .arg(references.join(QStringLiteral("\n"))));
  });

  if (filePath.toLower().endsWith(QStringLiteral(".mask.json"))) {
    addAction(frequentMenu, QStringLiteral("Apply Mask Preset to Selected Layer"), [this, filePath]() {
      auto* app = ArtifactApplicationManager::instance();
      auto* selectionManager = app ? app->layerSelectionManager() : nullptr;
      if (!selectionManager) {
        QMessageBox::information(this, QStringLiteral("Mask Preset"),
                                 QStringLiteral("適用先レイヤーが見つかりません。"));
        return;
      }
      auto layer = selectionManager->currentLayer();
      if (!layer) {
        QMessageBox::information(this, QStringLiteral("Mask Preset"),
                                 QStringLiteral("先に適用先レイヤーを選択してください。"));
        return;
      }
      LayerMask mask;
      if (!ArtifactPresetManager::loadMaskPreset(mask, filePath)) {
        QMessageBox::warning(this, QStringLiteral("Mask Preset"),
                             QStringLiteral("マスクプリセットを読み込めませんでした。"));
        return;
      }
      const auto choice = QMessageBox::question(
          this, QStringLiteral("Mask Preset"),
          QStringLiteral("マスクを置換しますか？\n\n"
                         "Yes: 置換\nNo: 追加"),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
      std::vector<LayerMask> before;
      before.reserve(static_cast<std::size_t>(layer->maskCount()));
      for (int index = 0; index < layer->maskCount(); ++index) {
        before.push_back(layer->mask(index));
      }
      std::vector<LayerMask> after;
      if (choice != QMessageBox::Yes) {
        after = before;
      }
      after.push_back(mask);
      auto command = std::make_unique<MaskEditCommand>(
          layer, std::move(before), std::move(after));
      if (auto* undo = UndoManager::instance()) {
        if (!undo->push(std::move(command))) {
          return;
        }
      } else {
        command->redo();
        if (!command->lastOperationSucceeded()) {
          return;
        }
      }
      layer->changed();
    });
  }

  // Open in File Explorer action
   addAction(allMenu, QStringLiteral("Open in File Explorer"), [filePath]() {
    QFileInfo fileInfo(filePath);
    QString folderPath = fileInfo.dir().absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
   });

  // Copy file path action
  addAction(frequentMenu, QStringLiteral("Copy File Path"), [filePath]() {
   QApplication::clipboard()->setText(filePath);
  });

  // Rename action (F2)
  addAction(allMenu, QStringLiteral("Rename (F2)"), [this]() {
   if (impl_) {
    impl_->renameSelected();
   }
  });

  // Delete action (Del)
  const QString deleteLabel = selectedAssetPaths.size() > 1
   ? QStringLiteral("Delete %1 Items (Del)").arg(selectedAssetPaths.size())
   : QStringLiteral("Delete (Del)");
  addAction(allMenu, deleteLabel, [this]() {
   if (impl_) {
    impl_->deleteSelected();
   }
  });

  // Show file properties action
  addAction(frequentMenu, QStringLiteral("Properties"), [filePath]() {
   QFileInfo fileInfo(filePath);
   QString info = QString("Name: %1\nSize: %2 bytes\nType: %3\nPath: %4")
     .arg(fileInfo.fileName())
     .arg(fileInfo.size())
     .arg(fileInfo.suffix())
     .arg(filePath);
   QMessageBox::information(nullptr,
                            QStringLiteral("Asset Properties"),
                            info);
  });

  if (item.isSequence && !item.sequencePaths.isEmpty()) {
    QMenu *framesMenu = frequentMenu->addMenu(
        QStringLiteral("Preview Sequence Frame"));
    constexpr int kFrameMenuSideCount = 50;
    const int totalFrameCount = item.sequencePaths.size();
    const int leadingFrameCount = totalFrameCount <= kFrameMenuSideCount * 2
        ? totalFrameCount
        : kFrameMenuSideCount;
    QSize expectedFrameSize;
    for (const QString& candidatePath : item.sequencePaths) {
      QImageReader candidateReader(candidatePath);
      if (candidateReader.canRead() && candidateReader.size().isValid()) {
        expectedFrameSize = candidateReader.size();
        break;
      }
    }
    const auto addFramePreviewAction = [framesMenu, expectedFrameSize](const QString& framePath) {
      QImageReader reader(framePath);
      const bool readable = reader.canRead();
      QString label = QFileInfo(framePath).fileName();
      if (!readable) {
        label += QStringLiteral(" (Unreadable)");
      } else if (expectedFrameSize.isValid() && reader.size().isValid() &&
                 reader.size() != expectedFrameSize) {
        label += QStringLiteral(" (Size Mismatch)");
      }
      QAction *frameAction = framesMenu->addAction(label);
      frameAction->setEnabled(readable);
      if (readable) {
        frameAction->setData(framePath);
      }
    };
    for (int frameIndex = 0; frameIndex < leadingFrameCount; ++frameIndex) {
      const QString framePath = item.sequencePaths.at(frameIndex);
      addFramePreviewAction(framePath);
    }
    const int trailingStart = std::max(leadingFrameCount,
                                       totalFrameCount - kFrameMenuSideCount);
    if (trailingStart > leadingFrameCount) {
      framesMenu->addSeparator();
        for (int frameIndex = trailingStart; frameIndex < totalFrameCount;
           ++frameIndex) {
        const QString framePath = item.sequencePaths.at(frameIndex);
        addFramePreviewAction(framePath);
      }
    }
    if (trailingStart > leadingFrameCount) {
      QAction *moreAction = framesMenu->addAction(
          QStringLiteral("Middle frames omitted (%1)")
              .arg(trailingStart - leadingFrameCount));
      moreAction->setEnabled(false);
    }
  }

  // Show menu at cursor position
  QAction *chosenAction = contextMenu.exec(accessibilityMenuPosition(
      contextMenu, impl_->fileView_->mapToGlobal(pos)));
  if (chosenAction && chosenAction->data().isValid()) {
    const QString framePath = chosenAction->data().toString();
    if (!framePath.isEmpty()) {
      itemDoubleClicked(framePath);
    }
  }
 }

// ─────────────────────────────────────────────
// Audio waveform thumbnail generation
// ─────────────────────────────────────────────

ArtifactCore::AudioSegment ArtifactAssetBrowser::Impl::loadAudioFile(const QString& audioFilePath)
{
    ArtifactCore::AudioSegment segment;

    // Try to load via SimpleWav
    ArtifactCore::SimpleWav wav;
    if (!wav.loadFromFile(audioFilePath)) {
        qWarning() << "[AssetBrowser] Failed to load audio file:" << audioFilePath;
        return segment;
    }

    // Convert to AudioSegment - OPTIMIZED: only load first N seconds
    const int maxFrames = kMaxThumbnailAudioSeconds * wav.sampleRate();
    const auto fullData = wav.getAudioData();
    const int framesToLoad = static_cast<int>(std::min<qsizetype>(fullData.size(), static_cast<qsizetype>(maxFrames)));

    segment.sampleRate = wav.sampleRate();
    segment.layout = ArtifactCore::AudioChannelLayout::Mono;
    segment.channelData.resize(1);
    segment.channelData[0] = fullData.mid(0, framesToLoad);
    segment.startFrame = 0;

    return segment;
}

QIcon ArtifactAssetBrowser::Impl::generateAudioWaveformThumbnail(const QString& audioFilePath)
{
    // Check if we already have a pending job for this file
    const std::uint64_t currentGeneration = thumbnailGeneration_.load(std::memory_order_relaxed);
    if (auto it = pendingWaveJobs_.find(audioFilePath); it != pendingWaveJobs_.end()) {
        if (it.value().generation == currentGeneration) {
            return defaultAudioIcon_;  // Return placeholder while generating
        }
        auto* staleWatcher = it.value().watcher;
        pendingWaveJobs_.erase(it);
        discardStaleThumbnailWatcher(staleWatcher);
    }

    // Check if this file previously failed
    if (failedWavePaths_.contains(audioFilePath)) {
        return defaultAudioIcon_;
    }

    // Check cache first
    if (hasCurrentThumbnail(audioFilePath)) {
        return thumbnailCache_[audioFilePath];
    }

    // Start async generation
    startAsyncWaveformGeneration(audioFilePath);

    // Return placeholder while generating
    return defaultAudioIcon_;
}

void ArtifactAssetBrowser::Impl::startAsyncWaveformGeneration(const QString& audioFilePath)
{
    if (pendingWaveJobs_.contains(audioFilePath)) {
        const auto existing = pendingWaveJobs_.value(audioFilePath);
        if (existing.generation == thumbnailGeneration_.load(std::memory_order_relaxed)) {
            return;  // Already pending
        }
        const auto staleJob = pendingWaveJobs_.take(audioFilePath);
        discardStaleThumbnailWatcher(staleJob.watcher);
    }

    const quint64 jobGeneration = thumbnailGeneration_.load(std::memory_order_relaxed);
    auto* watcher = new QFutureWatcher<QImage>();

    // Connect finished signal
    QObject::connect(watcher, &QFutureWatcher<QImage>::finished, [this, watcher, audioFilePath, jobGeneration]() {
        const QImage image = watcher->result();
        pendingWaveJobs_.remove(audioFilePath);
        if (jobGeneration != thumbnailGeneration_.load(std::memory_order_relaxed)) {
            watcher->deleteLater();
            return;
        }
        if (!image.isNull()) {
            // QPixmap/QIcon are GUI resources.  Keep their construction on the
            // watcher's thread (the browser/UI thread), never in QtConcurrent.
            const QIcon icon(QPixmap::fromImage(image));
            cacheThumbnail(audioFilePath, icon);
            if (assetModel_ && assetModel_->updateItemIconByPath(audioFilePath, icon)) {
                // model updated via dataChanged
            } else if (fileView_) {
                fileView_->update();
            }

        } else {
            // Mark as failed to avoid retry
            failedWavePaths_.insert(audioFilePath);
        }
        watcher->deleteLater();
    });

    // Capture thumbnail size for the worker
    const QSize thumbSize = thumbnailSize_;

    // Run waveform generation in background thread
    QFuture<QImage> future = QtConcurrent::run([audioFilePath, thumbSize]() -> QImage {
        try {
            // Load audio (in background thread)
            ArtifactCore::SimpleWav wav;
            if (!wav.loadFromFile(audioFilePath)) {
                return QImage();
            }

            // Downsample for thumbnail (max 30 seconds)
            const int maxFrames = 30 * wav.sampleRate();
            const auto fullData = wav.getAudioData();
    const qsizetype framesToLoad = qMin(fullData.size(), static_cast<qsizetype>(maxFrames));

            ArtifactCore::AudioSegment segment;
            segment.sampleRate = wav.sampleRate();
            segment.layout = ArtifactCore::AudioChannelLayout::Mono;
            segment.channelData.resize(1);
            segment.channelData[0] = fullData.mid(0, framesToLoad);
            segment.startFrame = 0;

            if (segment.channelData[0].isEmpty()) {
                return QImage();
            }

            // Generate waveform data
            AudioWaveformGenerator generator;
            const int thumbWidth = thumbSize.width() * 2;
            WaveformData waveData = generator.generate(segment, thumbWidth);

            if (waveData.peaks.isEmpty()) {
                return QImage();
            }

            // QImage is reentrant and can be painted in a worker.  Conversion
            // to GUI-backed QPixmap/QIcon happens in the finished callback.
            QImage image(thumbSize, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);

            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);

            const int w = image.width();
            const int h = image.height();
            const int centerY = h / 2;
            const float padding = 2.0f;

            painter.fillRect(0, 0, w, h, QColor(30, 30, 40, 200));
            painter.setPen(QColor(80, 80, 100));
            painter.drawLine(QLineF(padding, centerY, w - padding, centerY));

            const float maxAmplitude = std::max(std::abs(waveData.minSample), std::abs(waveData.maxSample));
            const float scale = (centerY - padding) / (maxAmplitude > 0.001f ? maxAmplitude : 1.0f);

            if (!waveData.rms.isEmpty()) {
                painter.setPen(QPen(QColor(60, 100, 160, 120), 1.0f));
                for (int i = 0; i < waveData.rms.size() && i < w; ++i) {
                    const float rmsVal = waveData.rms[i] * scale;
                    painter.drawLine(QLineF(i, centerY - rmsVal, i, centerY + rmsVal));
                }
            }

            painter.setPen(QPen(QColor(100, 160, 230, 200), 1.2f));
            for (int i = 0; i < waveData.peaks.size() && i < w; ++i) {
                const float peakVal = waveData.peaks[i] * scale;
                painter.drawLine(QLineF(i, centerY - peakVal, i, centerY + peakVal));
            }

            painter.end();
            return image;
        } catch (...) {
            return QImage();
        }
    });

    watcher->setFuture(future);
    pendingWaveJobs_[audioFilePath] = {audioFilePath, watcher, jobGeneration};
}

// ─────────────────────────────────────────────
// File Operations: Delete / Rename / New Folder / FileSystemWatcher
// ─────────────────────────────────────────────

void ArtifactAssetBrowser::Impl::setupFileSystemWatcher()
{
  if (fsWatcher_) return;
  fsWatcher_ = new QFileSystemWatcher();
  QObject::connect(fsWatcher_, &QFileSystemWatcher::directoryChanged,
                   QCoreApplication::instance(), [this](const QString& path) {
    Q_UNUSED(path);
    thumbnailGeneration_.fetch_add(1, std::memory_order_relaxed);
    if (!watchScheduled_) {
      watchScheduled_ = true;
      QTimer::singleShot(500, [this]() {
        watchScheduled_ = false;
        clearThumbnailCache();
        applyFilters();
      });
    }
  });
  QObject::connect(fsWatcher_, &QFileSystemWatcher::fileChanged,
                   QCoreApplication::instance(), [this](const QString& path) {
    Q_UNUSED(path);
    thumbnailGeneration_.fetch_add(1, std::memory_order_relaxed);
    if (!watchScheduled_) {
      watchScheduled_ = true;
      QTimer::singleShot(500, [this]() {
        watchScheduled_ = false;
        clearThumbnailCache();
        applyFilters();
      });
    }
  });
}

void ArtifactAssetBrowser::Impl::watchCurrentDirectory()
{
  if (!fsWatcher_) return;
  if (!currentDirectoryPath_.isEmpty() && QDir(currentDirectoryPath_).exists()) {
    if (!fsWatcher_->directories().contains(currentDirectoryPath_)) {
      fsWatcher_->addPath(currentDirectoryPath_);
    }
  }
}

void ArtifactAssetBrowser::Impl::handleFileRenamed(const QString& oldPath, const QString& newPath)
{
  Q_UNUSED(oldPath);
  Q_UNUSED(newPath);
  clearThumbnailCache();
  applyFilters();
}

void ArtifactAssetBrowser::Impl::handleFileDeleted(const QString& path)
{
  Q_UNUSED(path);
  clearThumbnailCache();
  applyFilters();
}

void ArtifactAssetBrowser::Impl::createNewFolder()
{
  QString folderName = promptNewFolderName();
  if (folderName.isEmpty()) return;

  QString parentDir = currentDirectoryPath_;
  if (parentDir.isEmpty()) {
    parentDir = ArtifactProjectService::instance()
                    ? ArtifactProjectService::instance()->currentProjectAssetsPath()
                    : QString();
  }
  if (parentDir.isEmpty()) {
    parentDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Assets";
  }
  if (parentDir.isEmpty()) return;

  QString newPath = QDir(parentDir).filePath(folderName);
  if (QDir().mkpath(newPath)) {
    clearThumbnailCache();
    applyFilters();
  }
}

void ArtifactAssetBrowser::Impl::renameSelected()
{
  QStringList paths = selectedAssetPaths();
  if (paths.isEmpty()) return;

  QString oldPath = paths.first();
  QFileInfo fi(oldPath);
  QString newName = promptNewName(fi.fileName());
  if (newName.isEmpty() || newName == fi.fileName()) return;

  QString newPath = fi.absolutePath() + "/" + newName;
  if (!QFileInfo::exists(newPath)) {
    auto *undo = UndoManager::instance();
    auto command = std::make_unique<MoveAssetFileCommand>(oldPath, newPath);
    bool applied = false;
    if (undo) {
      applied = undo->push(std::move(command));
    } else {
      command->redo();
      applied = command->lastOperationSucceeded();
    }
    if (!applied) {
      return;
    }
    if (!QFileInfo::exists(oldPath) && QFileInfo::exists(newPath)) {
      clearThumbnailCache();
      applyFilters();
    }
  }
}

void ArtifactAssetBrowser::Impl::deleteSelected()
{
  QStringList paths = selectedAssetPaths();
  if (paths.isEmpty()) return;

  if (!confirmDelete(paths)) return;

  int deletedCount = 0;
  const auto project = ArtifactProjectService::instance()
                           ? ArtifactProjectService::instance()->getCurrentProjectSharedPtr()
                           : ArtifactProjectPtr{};
  auto deleteBatch = std::make_unique<MacroUndoCommand>(
      QStringLiteral("Delete Assets"));
  QStringList deleteCandidates;
  for (const QString& path : paths) {
    if (!QFileInfo::exists(path)) continue;
    deleteCandidates.push_back(path);
    deleteBatch->addChild(
        std::make_unique<DeleteAssetFileCommand>(project, path));
  }
  if (!deleteCandidates.isEmpty()) {
    auto *undo = UndoManager::instance();
    bool applied = false;
    if (undo) {
      applied = undo->push(std::move(deleteBatch));
    } else {
      deleteBatch->redo();
      applied = deleteBatch->lastOperationSucceeded();
    }
    if (!applied) {
      return;
    }
  }
  for (const QString& path : deleteCandidates) {
    if (!QFileInfo::exists(path)) {
      ++deletedCount;
    }
  }

  if (deletedCount > 0) {
    clearThumbnailCache();
    applyFilters();
  }
}

QString ArtifactAssetBrowser::Impl::promptNewName(const QString& currentName) const
{
  bool ok = false;
  QString newName = QInputDialog::getText(nullptr, "Rename", "New name:",
                                           QLineEdit::Normal, currentName, &ok);
  if (!ok || newName.isEmpty()) return "";
  return newName;
}

QString ArtifactAssetBrowser::Impl::promptNewFolderName() const
{
  bool ok = false;
  QString name = QInputDialog::getText(nullptr, "New Folder", "Folder name:",
                                        QLineEdit::Normal, "New Folder", &ok);
  if (!ok || name.isEmpty()) return "";
  return name;
}

bool ArtifactAssetBrowser::Impl::confirmDelete(const QStringList& paths) const
{
  QString message;
  if (paths.size() == 1) {
    QFileInfo fi(paths.first());
    message = QStringLiteral("Are you sure you want to delete '%1'?<br>This will permanently remove the file from disk.").arg(fi.fileName());
  } else {
    message = QStringLiteral("Are you sure you want to delete %1 items?<br>This will permanently remove the files from disk.").arg(paths.size());
  }

  QMessageBox msgBox;
  msgBox.setWindowTitle("Confirm Delete");
  msgBox.setText(message);
  msgBox.setIcon(QMessageBox::Warning);
  msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
  msgBox.setDefaultButton(QMessageBox::No);
  return msgBox.exec() == QMessageBox::Yes;
}

} // namespace Artifact
