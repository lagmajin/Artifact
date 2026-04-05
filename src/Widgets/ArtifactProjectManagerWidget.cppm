module;
#include <QVector>
#include <QWidget>
#include <wobjectimpl.h>
#include <Widgets/Dialog/ArtifactDialogButtons.hpp>
#include <QBoxLayout>
#include <QLabel>
#include <QClipboard>
#include <QEvent>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDrag>
#include <QDropEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QVariant>
#include <QStandardItem>
#include <QModelIndex>
#include <QItemSelection>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QPixmap>
#include <QIcon>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QStringList>
#include <QTimer>
#include <QInputDialog>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QHeaderView>
#include <QPushButton>
#include <QFileDialog>
#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QShortcut>
#include <QRegularExpression>
#include <QDirIterator>
#include <QMessageBox>
#include <QHash>
#include <QFileInfo>
#include <QComboBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QStandardPaths>
#include <QSet>
#include <QDialog>
#include <QTreeWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QStyle>
#include <QPainter>
#include <QPalette>

#include <QScrollBar>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
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
module Artifact.Widgets.ProjectManagerWidget;

import std;
import Artifact.Widgets.SoftwareRenderInspectors;
import Widgets.Utils.CSS;


import Utils.String.UniString;
import Utils.Id;
import Artifact.Project.Manager;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Project.Model;
import Artifact.Project.Items;
import Artifact.Project.Roles;
import Artifact.Project.Cleanup;
import Artifact.Composition.Abstract;
import Artifact.Layer.Search.Query;
import Artifact.Event.Types;
import Event.Bus;
import Artifact.Layer.InitParams;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Widgets.CreatePlaneLayerDialog;
import Artifact.Widgets.AppDialogs;
import Dialog.Composition;
import Utils.Path;

namespace Artifact {

 using namespace ArtifactCore;

namespace {

bool isImportableAssetFile(const QString& path)
{
    const QString lower = path.toLower();
    return lower.endsWith(".png") || lower.endsWith(".jpg") ||
        lower.endsWith(".jpeg") || lower.endsWith(".bmp") ||
        lower.endsWith(".gif") || lower.endsWith(".tga") ||
        lower.endsWith(".tiff") || lower.endsWith(".exr") ||
        lower.endsWith(".mp4") || lower.endsWith(".mov") ||
        lower.endsWith(".avi") || lower.endsWith(".mkv") ||
        lower.endsWith(".webm") || lower.endsWith(".flv") ||
        lower.endsWith(".mp3") || lower.endsWith(".wav") ||
        lower.endsWith(".ogg") || lower.endsWith(".flac") ||
        lower.endsWith(".aac") || lower.endsWith(".m4a") ||
        lower.endsWith(".ttf") || lower.endsWith(".otf") ||
        lower.endsWith(".ttc") || lower.endsWith(".woff") ||
        lower.endsWith(".woff2");
}

void collectImportablePaths(const QString& localPath, QStringList& outPaths)
{
    if (localPath.isEmpty()) {
        return;
    }
    const QFileInfo info(localPath);
    if (!info.exists()) {
        return;
    }
    if (info.isDir()) {
        QDirIterator it(localPath, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString candidate = it.next();
            if (isImportableAssetFile(candidate)) {
                outPaths.append(candidate);
            }
        }
        return;
    }
    if (isImportableAssetFile(localPath)) {
        outPaths.append(localPath);
    }
}

void collectFolders(ProjectItem* item, QVector<FolderItem*>& out)
{
    if (!item) {
        return;
    }
    if (item->type() == eProjectItemType::Folder) {
        out.append(static_cast<FolderItem*>(item));
    }
    for (auto* child : item->children) {
        collectFolders(child, out);
    }
}

bool isDescendantOf(const ProjectItem* node, const ProjectItem* potentialAncestor)
{
    for (const ProjectItem* p = node; p; p = p->parent) {
        if (p == potentialAncestor) {
            return true;
        }
    }
    return false;
}

QString folderDisplayPath(const FolderItem* folder)
{
    if (!folder) {
        return QStringLiteral("(Folder)");
    }
    QStringList names;
    const ProjectItem* cur = folder;
    while (cur) {
        const QString n = cur->name.toQString().trimmed();
        names.prepend(n.isEmpty() ? QStringLiteral("(Unnamed)") : n);
        cur = cur->parent;
    }
    return names.join(QStringLiteral(" / "));
}

QString projectItemTypeLabel(const eProjectItemType type)
{
    switch (type) {
    case eProjectItemType::Composition:
        return QStringLiteral("Composition");
    case eProjectItemType::Footage:
        return QStringLiteral("Footage");
    case eProjectItemType::Folder:
        return QStringLiteral("Folder");
    case eProjectItemType::Solid:
        return QStringLiteral("Solid");
    default:
        return QStringLiteral("Item");
    }
}

QPixmap projectItemPreviewPixmap(ProjectItem* item, const QSize& targetSize)
{
    if (!item) {
        return {};
    }

    if (item->type() == eProjectItemType::Footage) {
        const QString path = static_cast<FootageItem*>(item)->filePath;
        const QFileInfo info(path);
        if (!info.exists()) {
            return {};
        }
        QPixmap pix(path);
        if (pix.isNull()) {
            return {};
        }
        return pix.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    if (item->type() == eProjectItemType::Composition) {
        auto* svc = ArtifactProjectService::instance();
        if (!svc) {
            return {};
        }
        const auto found = svc->findComposition(static_cast<CompositionItem*>(item)->compositionId);
        if (!found.success) {
            return {};
        }
        if (auto composition = found.ptr.lock()) {
           const QImage thumb = generateCompositionThumbnail(composition, targetSize);
            if (!thumb.isNull()) {
                return QPixmap::fromImage(thumb);
            }
        }
    }

    return {};
}

QStringList projectItemMetadataLines(const QModelIndex& sourceIndex, ProjectItem* item)
{
    const QModelIndex source0 = sourceIndex.siblingAtColumn(0);
    const QString title = source0.data(Qt::DisplayRole).toString();
    const QString size = sourceIndex.siblingAtColumn(1).data(Qt::DisplayRole).toString().trimmed();
    const QString duration = sourceIndex.siblingAtColumn(2).data(Qt::DisplayRole).toString().trimmed();
    const QString fps = sourceIndex.siblingAtColumn(3).data(Qt::DisplayRole).toString().trimmed();
    const QString updated = sourceIndex.siblingAtColumn(4).data(Qt::DisplayRole).toString().trimmed();

    QStringList lines;
    lines << title;

    if (!item) {
        lines << QStringLiteral("Unknown item") << QStringLiteral("No metadata");
        return lines;
    }

    if (item->type() == eProjectItemType::Footage) {
        const QString path = static_cast<FootageItem*>(item)->filePath;
        const QFileInfo info(path);
        const QString state = info.exists() ? QStringLiteral("Available") : QStringLiteral("Missing");
        lines << QStringLiteral("%1 • %2").arg(projectItemTypeLabel(item->type()), state);
        if (!size.isEmpty() || !duration.isEmpty() || !fps.isEmpty()) {
            lines << QStringLiteral("%1  %2  %3").arg(size, duration, fps).trimmed();
        } else {
            lines << info.fileName();
        }
        return lines;
    }

    if (item->type() == eProjectItemType::Composition) {
        QString secondLine = projectItemTypeLabel(item->type());
        if (!size.isEmpty()) {
            secondLine += QStringLiteral(" • %1").arg(size);
        }
        lines << secondLine;
        lines << QStringLiteral("%1  %2  Updated %3").arg(duration, fps, updated).trimmed();
        return lines;
    }

    lines << projectItemTypeLabel(item->type());
    if (!updated.isEmpty() && updated != QStringLiteral("-")) {
        lines << QStringLiteral("Updated %1").arg(updated);
    } else {
        lines << QStringLiteral("No preview available");
    }
    return lines;
}

QIcon loadSvgAsQIcon(const QString& path, int size = 16)
{
    if (path.isEmpty()) return QIcon();
    if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
        QSvgRenderer renderer(path);
        if (renderer.isValid()) {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            renderer.render(&painter);
            painter.end();
            if (!pixmap.isNull()) return QIcon(pixmap);
        }
        return QIcon();
    }
    return QIcon(path);
}

QIcon loadProjectViewIcon(const QString& resourceRelativePath, const QString& fallbackFileName = {})
{
    QIcon icon = loadSvgAsQIcon(ArtifactCore::resolveIconResourcePath(resourceRelativePath));
    if (!icon.isNull() && !icon.pixmap(16, 16).isNull()) {
        return icon;
    }
    if (!fallbackFileName.isEmpty()) {
        icon = loadSvgAsQIcon(ArtifactCore::resolveIconPath(fallbackFileName));
        if (!icon.isNull() && !icon.pixmap(16, 16).isNull()) {
            return icon;
        }
    }
    if (auto* appStyle = QApplication::style()) {
        return appStyle->standardIcon(QStyle::SP_FileIcon);
    }
    return icon;
}

constexpr int kHeaderResizeHitRadius = 7;
constexpr int kHeaderGripWidth = 9;
constexpr int kHeaderGripHeight = 11;

struct HeaderResizeHit {
    int column = -1;
    int boundaryX = 0;
};

HeaderResizeHit headerResizeHit(const QVector<int>& columnWidths, const QPoint& mousePos, const int headerHeight)
{
    if (mousePos.y() < 0 || mousePos.y() >= headerHeight) {
        return {};
    }

    int x = 0;
    for (int i = 0; i < columnWidths.size(); ++i) {
        const int width = columnWidths[i];
        const int left = x;
        const int right = x + width;
        if (std::abs(mousePos.x() - right) <= kHeaderResizeHitRadius) {
            return {i, right};
        }
        if (i > 0 && std::abs(mousePos.x() - left) <= kHeaderResizeHitRadius) {
            return {i - 1, left};
        }
        x += width;
    }
    return {};
}

} // namespace

 W_OBJECT_IMPL(ArtifactProjectManagerWidget)
 W_OBJECT_IMPL(ArtifactProjectView)
 W_OBJECT_IMPL(ArtifactProjectManagerToolBox)

// --- Preview/Info Panel at top ---
class ProjectInfoPanel : public QWidget {
public:
    QLabel* thumbnail;
    QLabel* titleLabel;
    QLabel* detailsLabel;
    QHash<QString, QPixmap> previewCache;

    ProjectInfoPanel(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("projectInfoPanel"));
        setAutoFillBackground(true);
        setFixedHeight(90);
        const QColor background = QColor(ArtifactCore::currentDCCTheme().backgroundColor);
        const QColor surface = QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor);
        const QColor text = QColor(ArtifactCore::currentDCCTheme().textColor);
        const QColor muted = text.darker(130);
        const QColor border = QColor(ArtifactCore::currentDCCTheme().borderColor);
        QPalette widgetPalette = palette();
        widgetPalette.setColor(QPalette::Window, background);
        widgetPalette.setColor(QPalette::WindowText, text);
        setPalette(widgetPalette);
        auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(15);

        thumbnail = new QLabel();
        thumbnail->setFixedSize(120, 68);
        thumbnail->setAlignment(Qt::AlignCenter);
        thumbnail->setText("PREVIEW");
        thumbnail->setAutoFillBackground(true);
        {
            QPalette pal = thumbnail->palette();
            pal.setColor(QPalette::Window, surface);
            pal.setColor(QPalette::WindowText, muted);
            pal.setColor(QPalette::Base, surface);
            pal.setColor(QPalette::Mid, border);
            thumbnail->setPalette(pal);
        }

        auto infoLayout = new QVBoxLayout();
        infoLayout->setSpacing(2);
        infoLayout->setContentsMargins(0, 5, 0, 5);

        titleLabel = new QLabel("Project");
        {
            QFont f = titleLabel->font();
            f.setBold(true);
            f.setPointSize(13);
            titleLabel->setFont(f);
            QPalette pal = titleLabel->palette();
            pal.setColor(QPalette::WindowText, text);
            titleLabel->setPalette(pal);
        }

        detailsLabel = new QLabel("Select an item to see details");
        {
            QPalette pal = detailsLabel->palette();
            pal.setColor(QPalette::WindowText, muted);
            detailsLabel->setPalette(pal);
        }

        infoLayout->addWidget(titleLabel);
        infoLayout->addWidget(detailsLabel);
        infoLayout->addStretch();

        layout->addWidget(thumbnail);
        layout->addLayout(infoLayout);
        layout->addStretch();
    }

    void updateInfo(const QModelIndex& index) {
        if (!index.isValid()) {
            titleLabel->setText("Project");
            detailsLabel->setText("No selection");
            thumbnail->setText("PREVIEW");
            thumbnail->setPixmap(QPixmap());
            return;
        }
        const QModelIndex source0 = index.siblingAtColumn(0);
        QString name = source0.data(Qt::DisplayRole).toString();
        titleLabel->setText(name);

        // Lazy preview generation: only decode imagery when the selected row needs it.
        thumbnail->setText("PREVIEW");
        thumbnail->setPixmap(QPixmap());
        QVariant ptrVar = source0.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
        const QStringList metadata = projectItemMetadataLines(index, item);
        detailsLabel->setText(metadata.mid(1).join(QStringLiteral("\n")));

        if (!item) {
            return;
        }

        const QString cacheKey = item->type() == eProjectItemType::Composition
            ? QStringLiteral("comp:%1").arg(static_cast<CompositionItem*>(item)->compositionId.toString())
            : (item->type() == eProjectItemType::Footage
                ? QStringLiteral("footage:%1").arg(static_cast<FootageItem*>(item)->filePath)
                : QStringLiteral("%1:%2").arg(static_cast<int>(item->type())).arg(name));

        auto cacheIt = previewCache.constFind(cacheKey);
        if (cacheIt != previewCache.constEnd()) {
            thumbnail->setPixmap(*cacheIt);
            thumbnail->setText(QString());
            return;
        }

        const QPixmap pix = projectItemPreviewPixmap(item, thumbnail->size());
        if (!pix.isNull()) {
            previewCache.insert(cacheKey, pix);
            thumbnail->setPixmap(pix);
            thumbnail->setText(QString());
            return;
        }

        if (item->type() == eProjectItemType::Footage &&
            !QFileInfo(static_cast<FootageItem*>(item)->filePath).exists()) {
            thumbnail->setText("MISSING");
            return;
        }

        thumbnail->setText(projectItemTypeLabel(item->type()).toUpper());
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);
        painter.fillRect(event->rect(), QColor(ArtifactCore::currentDCCTheme().backgroundColor));
        QWidget::paintEvent(event);
    }
};

// --- Hover Popup ---
class HoverThumbnailPopupWidget::Impl {
 public:
  Impl() : thumbnailLabel(nullptr) {}
  QLabel* thumbnailLabel;
  QVector<QLabel*> infoLabels;
  QVBoxLayout* layout;
};

HoverThumbnailPopupWidget::HoverThumbnailPopupWidget(QWidget* parent) : QWidget(parent), impl_(new Impl()) {
  setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  setAutoFillBackground(false);

  impl_->layout = new QVBoxLayout(this);
  impl_->layout->setContentsMargins(10, 10, 10, 10);
  impl_->layout->setSpacing(6);

  impl_->thumbnailLabel = new QLabel(this);
  impl_->thumbnailLabel->setFixedSize(200, 112);
  impl_->thumbnailLabel->setScaledContents(true);
  {
    QPalette pal = impl_->thumbnailLabel->palette();
    pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
    impl_->thumbnailLabel->setAutoFillBackground(true);
    impl_->thumbnailLabel->setPalette(pal);
  }
  impl_->layout->addWidget(impl_->thumbnailLabel, 0, Qt::AlignCenter);

  for (int i = 0; i < 3; ++i) {
    QLabel* l = new QLabel(this);
    {
      QPalette pal = l->palette();
      pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(115));
      l->setPalette(pal);
    }
    impl_->infoLabels.append(l);
    impl_->layout->addWidget(l);
  }
}

void HoverThumbnailPopupWidget::paintEvent(QPaintEvent* event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(QColor(30, 30, 30, 240));
  painter.setPen(QPen(QColor(ArtifactCore::currentDCCTheme().borderColor)));
  painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);
  QWidget::paintEvent(event);
}

HoverThumbnailPopupWidget::~HoverThumbnailPopupWidget() { delete impl_; }
void HoverThumbnailPopupWidget::setThumbnail(const QPixmap& px) { if(impl_->thumbnailLabel) impl_->thumbnailLabel->setPixmap(px); }
void HoverThumbnailPopupWidget::setLabels(const QStringList& ls) {
  for (int i = 0; i < impl_->infoLabels.size(); ++i) {
    impl_->infoLabels[i]->setText(i < ls.size() ? ls[i] : QString());
  }
}
void HoverThumbnailPopupWidget::setLabel(int idx, const QString& t) { if(idx>=0 && idx<impl_->infoLabels.size()) impl_->infoLabels[idx]->setText(t); }
void HoverThumbnailPopupWidget::showAt(const QPoint& p) {
  QPoint pos = p;
  const QSize popupSize = sizeHint().expandedTo(QSize(240, 140));
  if (QScreen* screen = QGuiApplication::screenAt(p)) {
    const QRect avail = screen->availableGeometry();
    if (pos.x() + popupSize.width() > avail.right() - 8) {
      pos.setX(avail.right() - popupSize.width() - 8);
    }
    if (pos.y() + popupSize.height() > avail.bottom() - 8) {
      pos.setY(avail.bottom() - popupSize.height() - 8);
    }
    pos.setX(std::max(avail.left() + 8, pos.x()));
    pos.setY(std::max(avail.top() + 8, pos.y()));
  }
  move(pos);
  show();
  raise();
  QTimer::singleShot(4500, this, &QWidget::hide);
}

class ProjectFilterProxyModel : public QSortFilterProxyModel {
public:
    explicit ProjectFilterProxyModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent) {}

    void setSearchQuery(const ArtifactLayerSearchQuery& query) {
        query_ = query;
        invalidateFilter();
    }

    void setUnusedAssetPaths(const QSet<QString>& unusedPaths) {
        unusedAssetPaths_ = unusedPaths;
        invalidateFilter();
    }

    void setAdvancedFilter(const QString& expression, const QString& typeFilter, const bool unusedOnly) {
        rawExpression_ = expression.trimmed();
        typeFilter_ = typeFilter.trimmed().toLower();
        unusedOnly_ = unusedOnly;
        parseExpression();
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        const QModelIndex rowIdx0 = sourceModel()->index(sourceRow, 0, sourceParent);
        if (!rowIdx0.isValid()) {
            return false;
        }

        const QVariant typeVar = sourceModel()->data(
            rowIdx0, Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
        const eProjectItemType itemType = typeVar.isValid()
            ? static_cast<eProjectItemType>(typeVar.toInt())
            : eProjectItemType::Unknown;

        const QVariant ptrVar = sourceModel()->data(
            rowIdx0, Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;

        const bool rowMatch = matchesAdvanced(rowIdx0, itemType, item) && matchesLegacyQuery(sourceRow, sourceParent);
        if (rowMatch) {
            return true;
        }

        // Keep parent folders visible if any child matches.
        const int childCount = sourceModel() ? sourceModel()->rowCount(rowIdx0) : 0;
        for (int r = 0; r < childCount; ++r) {
            if (filterAcceptsRow(r, rowIdx0)) {
                return true;
            }
        }
        return false;
    }

private:
    bool matchesLegacyQuery(const int sourceRow, const QModelIndex& sourceParent) const {
        if (query_.isSearchTextEmpty()) {
            return true;
        }

        const int cols = sourceModel() ? sourceModel()->columnCount(sourceParent) : 0;
        for (int c = 0; c < cols; ++c) {
            const QModelIndex idx = sourceModel()->index(sourceRow, c, sourceParent);
            const QString text = sourceModel()->data(idx, Qt::DisplayRole).toString();
            if (query_.matches(text, LayerSearchType::Any, true, false, false)) {
                return true;
            }
        }
        return false;
    }

    void parseExpression() {
        plainTerms_.clear();
        tagTerms_.clear();
        regexPattern_.clear();
        regexEnabled_ = false;

        const QStringList tokens = rawExpression_.split(' ', Qt::SkipEmptyParts);
        for (const QString& token : tokens) {
            if (token.startsWith("tag:", Qt::CaseInsensitive)) {
                const QString value = token.mid(4).trimmed();
                if (!value.isEmpty()) tagTerms_.append(value);
                continue;
            }
            if (token.startsWith("regex:", Qt::CaseInsensitive)) {
                regexPattern_ = token.mid(6).trimmed();
                regexEnabled_ = !regexPattern_.isEmpty();
                continue;
            }
            if (token.startsWith("type:", Qt::CaseInsensitive)) {
                const QString v = token.mid(5).trimmed().toLower();
                if (!v.isEmpty()) typeFilter_ = v;
                continue;
            }
            if (token.compare("unused:true", Qt::CaseInsensitive) == 0 ||
                token.compare("is:unused", Qt::CaseInsensitive) == 0) {
                unusedOnly_ = true;
                continue;
            }
            plainTerms_.append(token);
        }
    }

    bool typeMatches(const eProjectItemType itemType) const {
        if (typeFilter_.isEmpty() || typeFilter_ == "all") {
            return true;
        }
        if (typeFilter_ == "composition") return itemType == eProjectItemType::Composition;
        if (typeFilter_ == "footage") return itemType == eProjectItemType::Footage;
        if (typeFilter_ == "folder") return itemType == eProjectItemType::Folder;
        if (typeFilter_ == "solid") return itemType == eProjectItemType::Solid;
        return true;
    }

    bool matchesAdvanced(const QModelIndex& idx0, const eProjectItemType itemType, ProjectItem* item) const {
        if (!typeMatches(itemType)) return false;

        const QString name = sourceModel()->data(idx0, Qt::DisplayRole).toString();
        QString searchBlob = name;
        if (item && itemType == eProjectItemType::Footage) {
            const QString path = static_cast<FootageItem*>(item)->filePath;
            searchBlob += QStringLiteral(" ") + path;
            if (unusedOnly_ && !unusedAssetPaths_.contains(path)) {
                return false;
            }
        } else if (unusedOnly_) {
            return false;
        }

        for (const QString& term : plainTerms_) {
            if (!searchBlob.contains(term, Qt::CaseInsensitive)) {
                return false;
            }
        }

        for (const QString& tag : tagTerms_) {
            if (!searchBlob.contains(tag, Qt::CaseInsensitive)) {
                return false;
            }
        }

        if (regexEnabled_) {
            const QRegularExpression rx(regexPattern_, QRegularExpression::CaseInsensitiveOption);
            if (!rx.isValid()) return false;
            if (!rx.match(searchBlob).hasMatch()) return false;
        }

        return true;
    }

private:
    ArtifactLayerSearchQuery query_;
    QSet<QString> unusedAssetPaths_;
    QString rawExpression_;
    QString typeFilter_;
    bool unusedOnly_ = false;
    bool regexEnabled_ = false;
    QString regexPattern_;
    QStringList plainTerms_;
    QStringList tagTerms_;
};

bool renameProjectItem(ProjectItem* item, const QString& newName) {
    auto* svc = ArtifactProjectService::instance();
    if (!svc || !item) {
        return false;
    }
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    if (item->type() == eProjectItemType::Composition) {
        auto* compItem = static_cast<CompositionItem*>(item);
        return svc->renameComposition(compItem->compositionId, UniString::fromQString(trimmed));
    }
    auto shared = svc->getCurrentProjectSharedPtr();
    if (!shared) {
        return false;
    }
    item->name = UniString::fromQString(trimmed);
    shared->projectChanged();
    return true;
}

void scheduleProjectViewRefresh(ArtifactProjectView* view)
{
    if (!view) {
        return;
    }
    static constexpr auto kRefreshQueuedProperty = "artifactProjectViewRefreshQueued";
    if (view->property(kRefreshQueuedProperty).toBool()) {
        return;
    }
    view->setProperty(kRefreshQueuedProperty, true);
    QTimer::singleShot(0, view, [view]() {
        view->setProperty(kRefreshQueuedProperty, false);
        view->refreshVisibleContent();
    });
}

// --- Project View (Tree) ---
class ArtifactProjectView::Impl {
public:
    struct VisibleRow {
        QModelIndex index0;
        int depth = 0;
    };

    struct Colors {
        static inline const QColor Background = QColor(0x28, 0x28, 0x28);
        static inline const QColor HeaderBackground = QColor(0x25, 0x25, 0x26);
        static inline const QColor HeaderText = QColor(0x8D, 0x99, 0xA6);
        static inline const QColor HeaderSeparator = QColor(0x3E, 0x3E, 0x42);
        static inline const QColor HeaderHover = QColor(0x2D, 0x2D, 0x30);
        static inline const QColor RowHover = QColor(0x2A, 0x2A, 0x2C);
        static inline const QColor RowSelected = QColor(0x09, 0x47, 0x71);
        static inline const QColor RowSelectedText = QColor(0xF5, 0xF7, 0xFA);
        static inline const QColor RowText = QColor(0xCC, 0xCC, 0xCC);
        static inline const QColor RowBorder = QColor(0x28, 0x28, 0x28);
        static inline const QColor BranchNormal = QColor(0x8D, 0x99, 0xA6);
        static inline const QColor BranchHover = QColor(0xCC, 0xCC, 0xCC);
    };

    QAbstractItemModel* model = nullptr;
    QItemSelectionModel* selectionModel = nullptr;
    QTimer* hoverTimer = nullptr;
    QModelIndex hoverIndex;
    HoverThumbnailPopupWidget* hoverPopup = nullptr;
    QPoint hoverStartPos;
    QPoint dragStartPos;
    QString lastContextCommandId;
    QString lastContextCommandLabel;
    QString lastNewCommandId;
    QString lastNewCommandLabel;
    QVector<int> columnWidths = {260, 120, 120, 100, 140, 180};
    QVector<VisibleRow> visibleRows;
    QSet<QString> expandedKeys;
    QVector<QMetaObject::Connection> modelConnections;
    bool sortingEnabled = false;
    int sortColumn = 0;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    int resizingColumn = -1;
    int resizeStartX = 0;
    int hoverHeaderColumn = -1;
    QModelIndex hoverBranchIndex;
    QLineEdit* nameEditor = nullptr;
    QModelIndex editingIndex;
    int headerHeight = 24;
    int rowHeight = 28;
    int indentWidth = 16;

    QString keyForIndex(QModelIndex index) const {
        index = index.siblingAtColumn(0);
        if (!index.isValid()) {
            return QString();
        }
        const QVariant compId = index.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
        if (compId.isValid() && !compId.toString().isEmpty()) {
            return QStringLiteral("comp:%1").arg(compId.toString());
        }
        const QVariant assetId = index.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::AssetId));
        if (assetId.isValid() && !assetId.toString().isEmpty()) {
            return QStringLiteral("asset:%1").arg(assetId.toString());
        }
        const QVariant ptrVar = index.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        if (ptrVar.isValid()) {
            return QStringLiteral("ptr:%1").arg(ptrVar.value<quintptr>());
        }
        QStringList path;
        while (index.isValid()) {
            path.prepend(index.data(Qt::DisplayRole).toString());
            index = index.parent();
        }
        return path.join(QStringLiteral("/"));
    }

    bool isExpanded(const QModelIndex& index) const {
        return expandedKeys.contains(keyForIndex(index));
    }

    void setExpandedState(const QModelIndex& index, const bool expanded) {
        const QString key = keyForIndex(index);
        if (key.isEmpty()) {
            return;
        }
        if (expanded) {
            expandedKeys.insert(key);
        } else {
            expandedKeys.remove(key);
        }
    }

    bool hasChildren(const QModelIndex& index) const {
        return model && model->rowCount(index.siblingAtColumn(0)) > 0;
    }

    int totalColumnWidth() const {
        int total = 0;
        for (const int width : columnWidths) {
            total += width;
        }
        return total;
    }

    void rebuildVisibleRows() {
        visibleRows.clear();
        if (!model) {
            return;
        }
        std::function<void(const QModelIndex&, int)> appendRows = [&](const QModelIndex& parent, const int depth) {
            const int childCount = model->rowCount(parent);
            for (int row = 0; row < childCount; ++row) {
                const QModelIndex index0 = model->index(row, 0, parent);
                if (!index0.isValid()) {
                    continue;
                }
                visibleRows.push_back({index0, depth});
                if (hasChildren(index0) && isExpanded(index0)) {
                    appendRows(index0, depth + 1);
                }
            }
        };
        appendRows({}, 0);
    }

    int rowForIndex(const QModelIndex& index) const {
        const QModelIndex index0 = index.siblingAtColumn(0);
        for (int i = 0; i < visibleRows.size(); ++i) {
            if (visibleRows[i].index0 == index0) {
                return i;
            }
        }
        return -1;
    }

    FolderItem* currentFolderTarget(const ArtifactProjectView* view) const {
        if (!view || !view->selectionModel()) {
            return nullptr;
        }
        const auto rows = view->selectionModel()->selectedRows(0);
        if (rows.isEmpty()) {
            return nullptr;
        }
        QModelIndex sourceIdx = rows.first();
        if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(sourceIdx.model())) {
            sourceIdx = proxy->mapToSource(sourceIdx).siblingAtColumn(0);
        }
        const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
        if (!item) {
            return nullptr;
        }
        if (item->type() == eProjectItemType::Folder) {
            return static_cast<FolderItem*>(item);
        }
        if (item->parent && item->parent->type() == eProjectItemType::Folder) {
            return static_cast<FolderItem*>(item->parent);
        }
        return nullptr;
    }

    void createFolderAtSelection(ArtifactProjectView* view) const {
        auto* svc = ArtifactProjectService::instance();
        if (!svc) {
            return;
        }
        auto project = svc->getCurrentProjectSharedPtr();
        if (!project) {
            return;
        }

        bool ok;
        QString name = QInputDialog::getText(view, QStringLiteral("New Folder"),
            QStringLiteral("Folder Name:"), QLineEdit::Normal, QStringLiteral("New Folder"), &ok);
        
        if (!ok || name.trimmed().isEmpty()) {
            return;
        }

        project->createFolder(UniString::fromQString(name), currentFolderTarget(view));
        // createFolder notifies projectChanged() internally
    }

    void handleFileDrop(const QString& str) {
        auto* svc = ArtifactProjectService::instance();
        if (!svc) return;
        QStringList importTargets;
        collectImportablePaths(str, importTargets);
        importTargets.removeDuplicates();
        if (importTargets.isEmpty()) return;
        svc->importAssetsFromPaths(importTargets);
    }

    static void collectFootage(ProjectItem* item, QVector<FootageItem*>& out) {
        if (!item) return;
        if (item->type() == eProjectItemType::Footage) {
            out.append(static_cast<FootageItem*>(item));
        }
        for (auto* child : item->children) {
            collectFootage(child, out);
        }
    }

    static QString findByFileName(const QString& rootDir, const QString& fileName) {
        if (rootDir.isEmpty() || fileName.isEmpty()) return QString();
        QDirIterator it(rootDir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString candidate = it.next();
            if (QFileInfo(candidate).fileName().compare(fileName, Qt::CaseInsensitive) == 0) {
                return candidate;
            }
        }
        return QString();
    }

    static int relinkMissingFootage(const QString& rootDir, const QVector<FootageItem*>& targets) {
        int relinked = 0;
        for (auto* footage : targets) {
            if (!footage) continue;
            const QFileInfo currentInfo(footage->filePath);
            if (currentInfo.exists()) continue;
            const QString replacement = findByFileName(rootDir, currentInfo.fileName());
            if (!replacement.isEmpty()) {
                footage->filePath = QFileInfo(replacement).absoluteFilePath();
                ++relinked;
            }
        }
        return relinked;
    }

    static void showDependencyGraphDialog(QWidget* parent, ArtifactProjectService* svc) {
        if (!svc) return;
        auto project = svc->getCurrentProjectSharedPtr();
        if (!project) return;

        auto* dialog = new QDialog(parent);
        dialog->setWindowTitle("Composition Dependency Graph");
        dialog->resize(720, 520);
        auto* layout = new QVBoxLayout(dialog);
        auto* tree = new QTreeWidget(dialog);
        tree->setColumnCount(3);
        tree->setHeaderLabels(QStringList() << "Node" << "Depends On" << "Type");
        layout->addWidget(tree);

        QHash<QString, QString> footageByPath;
        const auto roots = project->projectItems();
        std::function<void(ProjectItem*)> gather = [&](ProjectItem* item) {
            if (!item) return;
            if (item->type() == eProjectItemType::Footage) {
                auto* f = static_cast<FootageItem*>(item);
                footageByPath.insert(f->filePath, f->name.toQString());
            }
            for (auto* c : item->children) gather(c);
        };
        for (auto* r : roots) gather(r);

        std::function<void(ProjectItem*)> visit = [&](ProjectItem* item) {
            if (!item) return;
            if (item->type() == eProjectItemType::Composition) {
                auto* compItem = static_cast<CompositionItem*>(item);
                auto rootNode = new QTreeWidgetItem(tree);
                rootNode->setText(0, compItem->name.toQString());
                rootNode->setText(1, "-");
                rootNode->setText(2, "Composition");

                auto findRes = project->findComposition(compItem->compositionId);
                if (findRes.success) {
                    if (auto comp = findRes.ptr.lock()) {
                        for (const auto& layer : comp->allLayer()) {
                            if (!layer) continue;
                            auto* layerNode = new QTreeWidgetItem(rootNode);
                            layerNode->setText(0, layer->layerName());
                            layerNode->setText(1, "-");
                            layerNode->setText(2, "Layer");

                            const auto groups = layer->getLayerPropertyGroups();
                            for (const auto& g : groups) {
                                for (const auto& p : g.allProperties()) {
                                    if (!p) continue;
                                    const QString propName = p->getName().toLower();
                                    if (!propName.contains("path") &&
                                        !propName.contains("source") &&
                                        !propName.contains("composition")) {
                                        continue;
                                    }
                                    const QString value = p->getValue().toString().trimmed();
                                    if (value.isEmpty()) continue;
                                    auto* depNode = new QTreeWidgetItem(layerNode);
                                    depNode->setText(0, p->getName());
                                    depNode->setText(1, value);
                                    if (footageByPath.contains(value)) {
                                        depNode->setText(2, "Footage");
                                    } else if (value.contains('-') || value.contains('{')) {
                                        depNode->setText(2, "Composition?");
                                    } else {
                                        depNode->setText(2, "PropertyRef");
                                    }
                                }
                            }
                        }
                    }
                }
            }
            for (auto* c : item->children) visit(c);
        };
        for (auto* r : roots) visit(r);

        tree->expandToDepth(1);
        dialog->exec();
        dialog->deleteLater();
    }
};

ArtifactProjectView::ArtifactProjectView(QWidget* parent) : QWidget(parent), impl_(new Impl()) {
    setMouseTracking(true);
    setAcceptDrops(true);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);
    impl_->hoverTimer = new QTimer(this);
    impl_->hoverTimer->setSingleShot(true);
    
    verticalScrollBar_ = new QScrollBar(Qt::Vertical, this);
    scrollY_ = 0;

    connect(verticalScrollBar_, &QScrollBar::valueChanged, this, [this](int value) {
        scrollY_ = value;
        update();
    });
}

ArtifactProjectView::~ArtifactProjectView() { delete impl_; }

void ArtifactProjectView::setModel(QAbstractItemModel* model)
{
    if (!impl_) {
        return;
    }
    if (impl_->model == model) {
        refreshVisibleContent();
        return;
    }

    for (const auto& connection : impl_->modelConnections) {
        QObject::disconnect(connection);
    }
    impl_->modelConnections.clear();

    if (impl_->selectionModel) {
        delete impl_->selectionModel;
        impl_->selectionModel = nullptr;
    }

    impl_->model = model;

    if (impl_->model) {
        impl_->selectionModel = new QItemSelectionModel(impl_->model, this);
        QObject::connect(impl_->selectionModel, &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex&, const QModelIndex&) {
                update();
            });
        QObject::connect(impl_->selectionModel, &QItemSelectionModel::selectionChanged, this,
            [this](const QItemSelection&, const QItemSelection&) {
                update();
                if (impl_->selectionModel) {
                    itemSelected(impl_->selectionModel->currentIndex());
                }
            });
        impl_->modelConnections.push_back(QObject::connect(impl_->model, &QAbstractItemModel::modelReset, this,
            [this]() { refreshVisibleContent(); }));
        impl_->modelConnections.push_back(QObject::connect(impl_->model,
            qOverload<const QList<QPersistentModelIndex>&, QAbstractItemModel::LayoutChangeHint>(&QAbstractItemModel::layoutChanged),
            this,
            [this](const QList<QPersistentModelIndex>&, QAbstractItemModel::LayoutChangeHint) { refreshVisibleContent(); }));
        impl_->modelConnections.push_back(QObject::connect(impl_->model, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex&, int, int) { refreshVisibleContent(); }));
        impl_->modelConnections.push_back(QObject::connect(impl_->model, &QAbstractItemModel::rowsRemoved, this,
            [this](const QModelIndex&, int, int) { refreshVisibleContent(); }));
        impl_->modelConnections.push_back(QObject::connect(impl_->model, &QAbstractItemModel::rowsMoved, this,
            [this](const QModelIndex&, int, int, const QModelIndex&, int) { refreshVisibleContent(); }));
        impl_->modelConnections.push_back(QObject::connect(impl_->model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex&, const QModelIndex&, const QList<int>&) {
                update();
            }));
    }

    refreshVisibleContent();
}

QAbstractItemModel* ArtifactProjectView::model() const
{
    return impl_ ? impl_->model : nullptr;
}

QItemSelectionModel* ArtifactProjectView::selectionModel() const
{
    return impl_ ? impl_->selectionModel : nullptr;
}

QModelIndex ArtifactProjectView::currentIndex() const
{
    if (!selectionModel()) {
        return {};
    }
    return selectionModel()->currentIndex();
}

void ArtifactProjectView::setCurrentIndex(const QModelIndex& index)
{
    if (!selectionModel()) {
        return;
    }
    selectionModel()->setCurrentIndex(
        index,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Current);
    ensureIndexVisible(index);
    refreshVisibleContent();
}

void ArtifactProjectView::setSortingEnabled(const bool enabled)
{
    if (!impl_) {
        return;
    }
    impl_->sortingEnabled = enabled;
}

void ArtifactProjectView::sortByColumn(const int column, const Qt::SortOrder order)
{
    if (impl_ && impl_->sortingEnabled && impl_->model) {
        impl_->model->sort(column, order);
        refreshVisibleContent();
    }
}

void ArtifactProjectView::setColumnWidth(const int column, const int width)
{
    if (!impl_ || column < 0) {
        return;
    }
    if (column >= impl_->columnWidths.size()) {
        impl_->columnWidths.resize(column + 1);
    }
    impl_->columnWidths[column] = std::max(40, width);
    refreshVisibleContent();
}

void ArtifactProjectView::expand(const QModelIndex& index)
{
    setExpanded(index, true);
}

void ArtifactProjectView::collapse(const QModelIndex& index)
{
    setExpanded(index, false);
}

void ArtifactProjectView::setExpanded(const QModelIndex& index, const bool expanded)
{
    if (!impl_) {
        return;
    }
    const QModelIndex index0 = index.siblingAtColumn(0);
    if (!index0.isValid() || !impl_->hasChildren(index0)) {
        return;
    }
    impl_->setExpandedState(index0, expanded);
    refreshVisibleContent();
}

void ArtifactProjectView::expandAll()
{
    if (!impl_ || !impl_->model) {
        return;
    }
    std::function<void(const QModelIndex&)> expandRecursive = [&](const QModelIndex& parent) {
        const int childCount = impl_->model->rowCount(parent);
        for (int row = 0; row < childCount; ++row) {
            const QModelIndex child = impl_->model->index(row, 0, parent);
            if (!child.isValid()) {
                continue;
            }
            if (impl_->hasChildren(child)) {
                impl_->setExpandedState(child, true);
                expandRecursive(child);
            }
        }
    };
    expandRecursive({});
    refreshVisibleContent();
}

void ArtifactProjectView::collapseAll()
{
    if (!impl_) {
        return;
    }
    impl_->expandedKeys.clear();
    refreshVisibleContent();
}

void ArtifactProjectView::expandToDepth(const int depth)
{
    if (!impl_ || !impl_->model || depth < 0) {
        return;
    }
    std::function<void(const QModelIndex&, int)> expandRecursive = [&](const QModelIndex& parent, const int currentDepth) {
        const int childCount = impl_->model->rowCount(parent);
        for (int row = 0; row < childCount; ++row) {
            const QModelIndex child = impl_->model->index(row, 0, parent);
            if (!child.isValid()) {
                continue;
            }
            if (impl_->hasChildren(child) && currentDepth < depth) {
                impl_->setExpandedState(child, true);
                expandRecursive(child, currentDepth + 1);
            }
        }
    };
    expandRecursive({}, 0);
    refreshVisibleContent();
}

QModelIndex ArtifactProjectView::indexAt(const QPoint& pos) const
{
    if (!impl_) {
        return {};
    }
    const int y = pos.y() + scrollY_;
    if (y < impl_->headerHeight) {
        return {};
    }
    const int row = (y - impl_->headerHeight) / impl_->rowHeight;
    if (row < 0 || row >= impl_->visibleRows.size()) {
        return {};
    }
    return impl_->visibleRows[row].index0;
}

QRect ArtifactProjectView::visualRect(const QModelIndex& index) const
{
    if (!impl_) {
        return {};
    }
    const int row = impl_->rowForIndex(index);
    if (row < 0) {
        return {};
    }
    const int y = impl_->headerHeight + row * impl_->rowHeight - scrollY_;
    const int x = 0;
    return QRect(x, y, std::max(width(), impl_->totalColumnWidth()), impl_->rowHeight);
}

void ArtifactProjectView::ensureIndexVisible(const QModelIndex& index)
{
    if (!impl_) {
        return;
    }
    const int row = impl_->rowForIndex(index);
    if (row < 0) {
        return;
    }

    const int rowTop = impl_->headerHeight + row * impl_->rowHeight;
    const int rowBottom = rowTop + impl_->rowHeight;
    const int viewTop = scrollY_ + impl_->headerHeight;
    const int viewBottom = scrollY_ + height();

    if (rowTop < viewTop) {
        verticalScrollBar_->setValue(std::max(0, rowTop - impl_->headerHeight));
    } else if (rowBottom > viewBottom) {
        verticalScrollBar_->setValue(std::max(0, rowBottom - height()));
    }
}

void ArtifactProjectView::paintEvent(QPaintEvent* event)
{
    if (!impl_) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Sync background fill (Modo style)
    painter.fillRect(rect(), QColor(40, 40, 40));

    const int contentWidth = std::max(width(), impl_->totalColumnWidth());

    // Draw Rows
    painter.save();
    // We want the rows to be clipped to the area below the header.
    painter.setClipRect(0, impl_->headerHeight, width(), height() - impl_->headerHeight);
    painter.translate(0, -scrollY_);

    const int firstRow = std::max(0, (scrollY_ - impl_->headerHeight) / impl_->rowHeight);
    const int visibleRowCount = height() / impl_->rowHeight + 3;
    const int totalVisibleRows = static_cast<int>(impl_->visibleRows.size());
    const int lastRow = std::min(totalVisibleRows, firstRow + visibleRowCount);

    for (int row = firstRow; row < lastRow; ++row) {
        const auto& visibleRow = impl_->visibleRows[row];
        const QModelIndex index0 = visibleRow.index0;
        const QRect rowRect(
            0,
            impl_->headerHeight + row * impl_->rowHeight,
            contentWidth,
            impl_->rowHeight);

        const bool selected = selectionModel() && selectionModel()->isRowSelected(index0.row(), index0.parent());
        const bool hovered = impl_->hoverIndex.isValid() && impl_->hoverIndex == index0;

        const QColor rowFill = selected ? Impl::Colors::RowSelected : (hovered ? Impl::Colors::RowHover : Impl::Colors::Background);
        painter.fillRect(rowRect, rowFill);

        painter.setPen(Impl::Colors::RowBorder);
        painter.drawLine(rowRect.bottomLeft(), rowRect.bottomRight());

        int cellX = 0;
        const int configuredColumnCount = static_cast<int>(impl_->columnWidths.size());
        const int modelColumnCount = impl_->model ? impl_->model->columnCount({}) : 0;
        const int columnCount = std::max(configuredColumnCount, modelColumnCount);

        for (int column = 0; column < columnCount; ++column) {
            const int width = column < impl_->columnWidths.size() ? impl_->columnWidths[column] : 120;
            const QRect cellRect(cellX, rowRect.top(), width, rowRect.height());

            const QModelIndex cellIndex = index0.siblingAtColumn(column);
            painter.setPen(selected ? Impl::Colors::RowSelectedText : Impl::Colors::RowText);

            if (column == 0) {
                if (impl_->editingIndex.isValid() && impl_->editingIndex == index0 && impl_->nameEditor) {
                    const int indent = visibleRow.depth * impl_->indentWidth;
                    int textLeft = cellRect.left() + 8 + indent + (impl_->hasChildren(index0) ? 18 : 0);
                    const QVariant iconVar = cellIndex.data(Qt::DecorationRole);
                    if (iconVar.canConvert<QIcon>()) { textLeft += 22; }
                    const QRect editorRect(textLeft, cellRect.top() + 2, std::max(50, cellRect.right() - textLeft - 8), cellRect.height() - 4);
                    // Name editor needs to be moved accounting for scrollY_
                    const QRect screenEditorRect = editorRect.translated(0, -scrollY_);
                    if (impl_->nameEditor->geometry() != screenEditorRect) impl_->nameEditor->setGeometry(screenEditorRect);
                    if (!impl_->nameEditor->isVisible()) { impl_->nameEditor->show(); impl_->nameEditor->setFocus(); }
                } else {
                    const int indent = visibleRow.depth * impl_->indentWidth;
                    const QRect contentRect = cellRect.adjusted(8 + indent, 0, -8, 0);
                    if (impl_->hasChildren(index0)) {
                        const QRect branchRect(contentRect.left(), contentRect.center().y() - 6, 12, 12);
                        const bool branchHovered = (impl_->hoverBranchIndex == index0);
                        QPainterPath branchPath;
                        if (impl_->isExpanded(index0)) {
                            branchPath.moveTo(branchRect.left() + 2, branchRect.top() + 4);
                            branchPath.lineTo(branchRect.right() - 2, branchRect.top() + 4);
                            branchPath.lineTo(branchRect.center().x(), branchRect.bottom() - 2);
                        } else {
                            branchPath.moveTo(branchRect.left() + 4, branchRect.top() + 2);
                            branchPath.lineTo(branchRect.left() + 4, branchRect.bottom() - 2);
                            branchPath.lineTo(branchRect.right() - 2, branchRect.center().y());
                        }
                        painter.fillPath(branchPath, (selected || branchHovered) ? Impl::Colors::RowSelectedText : Impl::Colors::BranchNormal);
                    }
                    int textLeft = contentRect.left() + (impl_->hasChildren(index0) ? 18 : 0);
                    const QVariant iconVar = cellIndex.data(Qt::DecorationRole);
                    if (iconVar.canConvert<QIcon>()) {
                        const QIcon icon = qvariant_cast<QIcon>(iconVar);
                        const QRect iconRect(textLeft, rowRect.top() + (rowRect.height() - 16) / 2, 16, 16);
                        icon.paint(&painter, iconRect);
                        textLeft += 22;
                    }
                    painter.drawText(QRect(textLeft, rowRect.top(), std::max(0, cellRect.right() - textLeft - 8), rowRect.height()),
                        Qt::AlignVCenter | Qt::AlignLeft,
                        painter.fontMetrics().elidedText(cellIndex.data(Qt::DisplayRole).toString(), Qt::ElideRight, std::max(0, cellRect.width() - (textLeft - cellRect.left()) - 12)));
                }
            } else {
                const QString text = cellIndex.data(Qt::DisplayRole).toString();
                Qt::Alignment alignment = Qt::AlignVCenter | Qt::AlignLeft;
                if (column == 1 || column == 2 || column == 3) alignment = Qt::AlignVCenter | Qt::AlignRight;
                painter.drawText(cellRect.adjusted(8, 0, -8, 0), alignment, painter.fontMetrics().elidedText(text, Qt::ElideRight, cellRect.width() - 16));
            }
            painter.setPen(Impl::Colors::RowBorder);
            painter.drawLine(cellRect.topRight(), cellRect.bottomRight());
            cellX += width;
        }
    }
    painter.restore();

    // Draw Header
    painter.fillRect(QRect(0, 0, width(), impl_->headerHeight), Impl::Colors::HeaderBackground);

    int headerX = 0;
    const int configuredColumnCount = static_cast<int>(impl_->columnWidths.size());
    const int modelColumnCount = impl_->model ? impl_->model->columnCount({}) : 0;
    const int columnCount = std::max(configuredColumnCount, modelColumnCount);

    for (int column = 0; column < columnCount; ++column) {
        const int width = column < impl_->columnWidths.size() ? impl_->columnWidths[column] : 120;
        const QRect headerRect(headerX, 0, width, impl_->headerHeight);

        if (headerRect.right() >= 0 && headerRect.left() <= this->width()) {
            if (impl_->hoverHeaderColumn == column && impl_->resizingColumn == -1) {
                painter.fillRect(headerRect.adjusted(0, 0, -1, -1), Impl::Colors::HeaderHover);
            }

            painter.setPen(Impl::Colors::HeaderText);
            const QString label = impl_->model ? impl_->model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString() : QString();
            painter.drawText(headerRect.adjusted(10, 0, -20, 0), Qt::AlignVCenter | Qt::AlignLeft, label);

            if (impl_->sortingEnabled && impl_->sortColumn == column) {
                const int arrowSize = 8;
                const int arrowX = headerRect.right() - 15;
                const int arrowY = headerRect.center().y();
                QPainterPath arrowPath;
                if (impl_->sortOrder == Qt::AscendingOrder) {
                    arrowPath.moveTo(arrowX - arrowSize/2, arrowY + arrowSize/4);
                    arrowPath.lineTo(arrowX + arrowSize/2, arrowY + arrowSize/4);
                    arrowPath.lineTo(arrowX, arrowY - arrowSize/4);
                } else {
                    arrowPath.moveTo(arrowX - arrowSize/2, arrowY - arrowSize/4);
                    arrowPath.lineTo(arrowX + arrowSize/2, arrowY - arrowSize/4);
                    arrowPath.lineTo(arrowX, arrowY + arrowSize/4);
                }
                painter.fillPath(arrowPath, Impl::Colors::HeaderText);
            }

            painter.setPen(Impl::Colors::HeaderSeparator);
            painter.drawLine(headerRect.topRight() + QPoint(0, 4), headerRect.bottomRight() - QPoint(0, 4));

            if (column + 1 < columnCount) {
                const bool isResizeHot = (impl_->hoverHeaderColumn == column || impl_->resizingColumn == column);
                const QColor gripColor = isResizeHot
                    ? QColor(214, 222, 231, 210)
                    : QColor(143, 153, 166, 150);
                painter.setPen(QPen(gripColor, 1.4, Qt::SolidLine, Qt::RoundCap));
                const int gripX = headerRect.right() - (kHeaderGripWidth / 2);
                const int centerY = headerRect.center().y();
                const int startY = centerY - (kHeaderGripHeight / 2);
                for (int i = 0; i < 3; ++i) {
                    const int y = startY + i * 3;
                    painter.drawLine(QPointF(gripX - 1.5, y), QPointF(gripX + 1.5, y));
                }
            }
        }
        headerX += width;
    }
    painter.setPen(Impl::Colors::HeaderSeparator);
    painter.drawLine(0, impl_->headerHeight - 1, width(), impl_->headerHeight - 1);
}

void ArtifactProjectView::handleItemDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    QModelIndex actualIdx = index;
    if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(index.model())) {
        actualIdx = proxy->mapToSource(index);
    }

    QVariant typeVar = actualIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
    if (typeVar.isValid()) {
        if (typeVar.toInt() == static_cast<int>(eProjectItemType::Composition)) {
             QVariant idVar = actualIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
             if (idVar.isValid()) {
                 CompositionID cid(idVar.toString());
                 ArtifactProjectService::instance()->changeCurrentComposition(cid);
             }
        } else if (typeVar.toInt() == static_cast<int>(eProjectItemType::Folder)) {
            if (impl_->hasChildren(actualIdx)) {
                setExpanded(actualIdx, !impl_->isExpanded(actualIdx));
            }
        } else if (typeVar.toInt() == static_cast<int>(eProjectItemType::Footage)) {
            QVariant ptrVar = actualIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
            ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
            if (item && item->type() == eProjectItemType::Footage) {
                Q_UNUSED(item);
            }
        }
    }
}

void ArtifactProjectView::mouseDoubleClickEvent(QMouseEvent* event) {
    const QPoint mousePos = event->position().toPoint();
    if (mousePos.y() < impl_->headerHeight) {
        const HeaderResizeHit resizeHit = headerResizeHit(impl_->columnWidths, mousePos, impl_->headerHeight);
        if (resizeHit.column >= 0 && std::abs(mousePos.x() - resizeHit.boundaryX) <= kHeaderResizeHitRadius) {
            const QFontMetrics fm(font());
            int widest = fm.horizontalAdvance(impl_->model
                ? impl_->model->headerData(resizeHit.column, Qt::Horizontal, Qt::DisplayRole).toString()
                : QString());
            for (const auto& row : impl_->visibleRows) {
                if (!row.index0.isValid()) {
                    continue;
                }
                const QModelIndex cellIndex = row.index0.siblingAtColumn(resizeHit.column);
                if (!cellIndex.isValid()) {
                    continue;
                }
                int width = fm.horizontalAdvance(cellIndex.data(Qt::DisplayRole).toString());
                if (resizeHit.column == 0) {
                    width += 36;
                } else {
                    width += 20;
                }
                widest = std::max(widest, width);
            }
            setColumnWidth(resizeHit.column, std::clamp(widest + 24, 56, 420));
            refreshVisibleContent();
            event->accept();
            return;
        }
    }

    const QModelIndex idx = indexAt(mousePos);
    if (idx.isValid()) {
        itemDoubleClicked(idx);
        handleItemDoubleClicked(idx);
        event->accept();
        return;
    }
    event->ignore();
}

void ArtifactProjectView::editIndex(const QModelIndex& index) {
    if (!index.isValid() || !impl_) return;
    impl_->editingIndex = index.siblingAtColumn(0);
    
    if (!impl_->nameEditor) {
        QLineEdit* editor = new QLineEdit(this);
        impl_->nameEditor = editor;
        {
            QPalette pal = impl_->nameEditor->palette();
            pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
            pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
            pal.setColor(QPalette::Highlight, QColor(ArtifactCore::currentDCCTheme().accentColor));
            impl_->nameEditor->setPalette(pal);
        }
        connect(impl_->nameEditor, &QLineEdit::editingFinished, this, [this]() {
            if (!impl_ || !impl_->editingIndex.isValid() || !impl_->nameEditor) return;
            const QString newName = impl_->nameEditor->text().trimmed();
            if (!newName.isEmpty()) {
                QModelIndex sourceIdx = impl_->editingIndex;
                if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(sourceIdx.model())) {
                    sourceIdx = proxy->mapToSource(sourceIdx);
                }
                QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
                ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
                if (item) {
                    renameProjectItem(item, newName);
                }
            }
            impl_->editingIndex = QModelIndex();
            impl_->nameEditor->hide();
            this->update();
        });
    }
    
    impl_->nameEditor->setText(index.data(Qt::DisplayRole).toString());
    impl_->nameEditor->selectAll();
    this->update();
}

void ArtifactProjectView::mouseMoveEvent(QMouseEvent* event) {
    if (!impl_) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPoint mousePos = event->position().toPoint();

    if (impl_->resizingColumn != -1) {
        const int deltaX = mousePos.x() - impl_->resizeStartX;
        impl_->columnWidths[impl_->resizingColumn] = std::max(40, impl_->columnWidths[impl_->resizingColumn] + deltaX);
        impl_->resizeStartX = mousePos.x();
        refreshVisibleContent();
        event->accept();
        return;
    }

    if (mousePos.y() < impl_->headerHeight) {
        int x = 0;
        int hoveredCol = -1;
        for (int i = 0; i < impl_->columnWidths.size(); ++i) {
            const int width = impl_->columnWidths[i];
            if (mousePos.x() >= x && mousePos.x() < x + width) {
                hoveredCol = i;
                break;
            }
            x += width;
        }
        const HeaderResizeHit resizeHit = headerResizeHit(impl_->columnWidths, mousePos, impl_->headerHeight);
        setCursor(resizeHit.column >= 0 ? Qt::SplitHCursor : Qt::ArrowCursor);
        if (hoveredCol != impl_->hoverHeaderColumn) { impl_->hoverHeaderColumn = hoveredCol; update(); }
    } else {
        setCursor(Qt::ArrowCursor);
        if (impl_->hoverHeaderColumn != -1) { impl_->hoverHeaderColumn = -1; update(); }
    }

    const QModelIndex idx = indexAt(mousePos);
    QModelIndex branchIdx;
    if (idx.isValid() && mousePos.y() >= impl_->headerHeight) {
        const QRect rowRect = visualRect(idx);
        const int row = impl_->rowForIndex(idx);
        const int depth = (row >= 0 && row < impl_->visibleRows.size()) ? impl_->visibleRows[row].depth : 0;
        const QRect branchRect(8 + depth * impl_->indentWidth, rowRect.top(), 20, rowRect.height());
        if (impl_->hasChildren(idx) && branchRect.contains(mousePos)) branchIdx = idx;
    }
    if (branchIdx != impl_->hoverBranchIndex) { impl_->hoverBranchIndex = branchIdx; update(); }

    if (!impl_->hoverTimer) {
        impl_->hoverTimer = new QTimer(this);
        impl_->hoverTimer->setSingleShot(true);
        connect(impl_->hoverTimer, &QTimer::timeout, this, [this]() {
            const QPoint localPos = mapFromGlobal(QCursor::pos());
            if (!rect().contains(localPos)) return;
            const QModelIndex currentIndex = indexAt(localPos);
            if (!currentIndex.isValid() || currentIndex != impl_->hoverIndex) return;
            QModelIndex sourceIdx = currentIndex;
            if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(currentIndex.model())) sourceIdx = proxy->mapToSource(currentIndex).siblingAtColumn(0);
            const QVariant typeVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
            if (!typeVar.isValid()) return;
            const auto type = static_cast<eProjectItemType>(typeVar.toInt());
            if (type != eProjectItemType::Footage && type != eProjectItemType::Composition) return;
            if (!impl_->hoverPopup) impl_->hoverPopup = new HoverThumbnailPopupWidget();
            const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
            ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
            impl_->hoverPopup->setThumbnail(projectItemPreviewPixmap(item, QSize(200, 112)));
            impl_->hoverPopup->setLabels(projectItemMetadataLines(sourceIdx, item));
            impl_->hoverPopup->showAt(mapToGlobal(visualRect(currentIndex).topRight() + QPoint(14, 6)));
        });
    }

    if (event->buttons() != Qt::NoButton && impl_->resizingColumn == -1) {
        if (impl_->hoverPopup) impl_->hoverPopup->hide();
        impl_->hoverTimer->stop();
        impl_->hoverIndex = QModelIndex();
        if ((mousePos - impl_->dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
            const QModelIndex dragIdx = indexAt(impl_->dragStartPos);
            if (dragIdx.isValid() && selectionModel()) {
                auto* mime = new QMimeData();
                QList<QUrl> urls;
                QStringList filePaths;
                const auto selectedRows = selectionModel()->selectedRows(0);
                for (const QModelIndex& proxyIdx : selectedRows) {
                    QModelIndex sourceIdx = proxyIdx;
                    if (auto* proxy = qobject_cast<const QSortFilterProxyModel*>(proxyIdx.model()))
                        sourceIdx = proxy->mapToSource(proxyIdx).siblingAtColumn(0);
                    const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
                    ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
                    if (item && item->type() == eProjectItemType::Footage) {
                        const QString path = static_cast<FootageItem*>(item)->filePath;
                        if (!path.isEmpty()) {
                            urls.append(QUrl::fromLocalFile(path));
                            filePaths.append(path);
                        }
                    }
                }
                if (!urls.isEmpty()) {
                    mime->setUrls(urls);
                    mime->setText(filePaths.join(QStringLiteral("\n")));
                } else {
                    delete mime;
                    mime = impl_->model->mimeData(selectedRows);
                }
                auto* drag = new QDrag(this);
                drag->setMimeData(mime);
                drag->exec(Qt::CopyAction | Qt::MoveAction);
            }
        }
        event->accept();
        return;
    }

    if (idx != impl_->hoverIndex) {
        if (impl_->hoverPopup) impl_->hoverPopup->hide();
        impl_->hoverIndex = idx;
        impl_->hoverStartPos = mousePos;
        impl_->hoverTimer->stop();
        if (idx.isValid()) impl_->hoverTimer->start(1100);
        update();
    } else if (idx.isValid() && (mousePos - impl_->hoverStartPos).manhattanLength() > 6) {
        if (impl_->hoverPopup) impl_->hoverPopup->hide();
        impl_->hoverTimer->stop();
        impl_->hoverStartPos = mousePos;
        impl_->hoverTimer->start(1100);
    }
    event->accept();
}

void ArtifactProjectView::wheelEvent(QWheelEvent* event)
{
    if (verticalScrollBar_) {
        verticalScrollBar_->setValue(verticalScrollBar_->value() - event->angleDelta().y());
    }
    event->accept();
}

void ArtifactProjectView::leaveEvent(QEvent* event) {
    if (!impl_) {
        QWidget::leaveEvent(event);
        return;
    }
    if (impl_->hoverTimer) {
        impl_->hoverTimer->stop();
    }
    if (impl_->hoverPopup) {
        impl_->hoverPopup->hide();
    }
    impl_->hoverIndex = QModelIndex();
    impl_->hoverHeaderColumn = -1;
    impl_->hoverBranchIndex = QModelIndex();
    unsetCursor();
    update();
    QWidget::leaveEvent(event);
}

void ArtifactProjectView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (verticalScrollBar_) {
        verticalScrollBar_->setGeometry(width() - 8, 0, 8, height());
    }
    updateScrollRange();
    this->repaint();
}

void ArtifactProjectView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    scheduleProjectViewRefresh(this);
}

bool ArtifactProjectView::event(QEvent* event)
{
    const bool handled = QWidget::event(event);
    if (event && (event->type() == QEvent::WindowActivate ||
                  event->type() == QEvent::ActivationChange ||
                  event->type() == QEvent::PolishRequest)) {
        scheduleProjectViewRefresh(this);
    }
    return handled;
}

void ArtifactProjectView::updateScrollRange()
{
    if (!impl_ || !verticalScrollBar_) return;
    const int contentHeight = impl_->headerHeight + static_cast<int>(impl_->visibleRows.size()) * impl_->rowHeight;
    verticalScrollBar_->setPageStep(height());
    verticalScrollBar_->setRange(0, std::max(0, contentHeight - height()));
    verticalScrollBar_->setVisible(contentHeight > height());
}

void ArtifactProjectView::refreshVisibleContent()
{
    if (!impl_) {
        return;
    }
    impl_->rebuildVisibleRows();
    updateScrollRange();
    update();
}

void ArtifactProjectView::contextMenuEvent(QContextMenuEvent* event) {
    QModelIndex idx = indexAt(event->pos());
    QMenu menu(this);
    auto svc = ArtifactProjectService::instance();
    QHash<QString, std::function<void()>> availableContextCommands;
    QHash<QString, QString> availableContextLabels;
    QHash<QString, std::function<void()>> availableNewCommands;
    QHash<QString, QString> availableNewLabels;

    auto addTrackedAction = [this, &menu, &availableContextCommands, &availableContextLabels](const QString& id, const QString& label, std::function<void()> run, const QIcon& icon = QIcon()) {
        availableContextCommands.insert(id, run);
        availableContextLabels.insert(id, label);
        QAction* action = menu.addAction(label, [this, id, label, run = std::move(run)]() mutable {
            impl_->lastContextCommandId = id;
            impl_->lastContextCommandLabel = label;
            run();
        });
        if (!icon.isNull()) {
            action->setIcon(icon);
        }
    };

    auto addTrackedNewAction = [this, &availableNewCommands, &availableNewLabels](QMenu* targetMenu, const QString& id, const QString& label, std::function<void()> run, const QIcon& icon = QIcon()) {
        availableNewCommands.insert(id, run);
        availableNewLabels.insert(id, label);
        QAction* action = targetMenu->addAction(label, [this, id, label, run = std::move(run)]() mutable {
            impl_->lastNewCommandId = id;
            impl_->lastNewCommandLabel = label;
            run();
        });
        if (!icon.isNull()) {
            action->setIcon(icon);
        }
    };

    if (idx.isValid()) {
        QModelIndex sourceIdx = idx;
        if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(idx.model())) {
            sourceIdx = proxy->mapToSource(idx).siblingAtColumn(0);
        }

        QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
        
        QVariant typeVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
        eProjectItemType type = typeVar.isValid() ? static_cast<eProjectItemType>(typeVar.toInt()) : eProjectItemType::Footage;

        addTrackedAction(QStringLiteral("open"), QStringLiteral("Open"), [this, idx]() {
            handleItemDoubleClicked(idx);
        }, loadProjectViewIcon(QStringLiteral("MaterialVS/blue/file_open.svg")));
        addTrackedAction(QStringLiteral("copy_name"), QStringLiteral("Copy Name"), [sourceIdx]() {
            QApplication::clipboard()->setText(sourceIdx.data(Qt::DisplayRole).toString());
        }, loadProjectViewIcon(QStringLiteral("MaterialVS/neutral/content_copy.svg")));
        
        if (type == eProjectItemType::Composition) {
            addTrackedAction(QStringLiteral("set_active_composition"), QStringLiteral("Set as Active Composition"), [sourceIdx]() {
                QVariant idVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
                if (idVar.isValid()) {
                    ArtifactProjectService::instance()->changeCurrentComposition(CompositionID(idVar.toString()));
                }
            }, loadProjectViewIcon(QStringLiteral("MaterialVS/blue/movie_creation.svg")));

            addTrackedAction(QStringLiteral("composition_settings"), QStringLiteral("Composition Settings..."), [this, sourceIdx]() {
                auto* svc = ArtifactProjectService::instance();
                if (!svc) {
                    return;
                }

                const QVariant idVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
                if (!idVar.isValid()) {
                    return;
                }

                const CompositionID compositionId(idVar.toString());
                const auto found = svc->findComposition(compositionId);
                auto composition = found.ptr.lock();
                if (!found.success || !composition) {
                    QMessageBox::warning(this, QStringLiteral("Composition Settings"),
                        QStringLiteral("Could not load the selected composition."));
                    return;
                }

                auto* dialog = new QDialog(this);
                dialog->setAttribute(Qt::WA_DeleteOnClose);
                dialog->setWindowTitle(QStringLiteral("Composition Settings"));
                dialog->setModal(true);
                dialog->resize(360, 260);

                auto* layout = new QVBoxLayout(dialog);
                auto* nameLabel = new QLabel(QStringLiteral("Name"), dialog);
                auto* nameEdit = new QLineEdit(composition->settings().compositionName().toQString(), dialog);
                layout->addWidget(nameLabel);
                layout->addWidget(nameEdit);

                auto* sizeLayout = new QHBoxLayout();
                auto* widthSpin = new QSpinBox(dialog);
                widthSpin->setRange(1, 32768);
                widthSpin->setValue(std::max(1, composition->settings().compositionSize().width()));
                auto* heightSpin = new QSpinBox(dialog);
                heightSpin->setRange(1, 32768);
                heightSpin->setValue(std::max(1, composition->settings().compositionSize().height()));
                sizeLayout->addWidget(new QLabel(QStringLiteral("Width"), dialog));
                sizeLayout->addWidget(widthSpin);
                sizeLayout->addWidget(new QLabel(QStringLiteral("Height"), dialog));
                sizeLayout->addWidget(heightSpin);
                layout->addLayout(sizeLayout);

                auto* fpsLayout = new QHBoxLayout();
                auto* fpsSpin = new QDoubleSpinBox(dialog);
                fpsSpin->setRange(1.0, 240.0);
                fpsSpin->setDecimals(3);
                fpsSpin->setSingleStep(0.5);
                fpsSpin->setValue(std::max(1.0, static_cast<double>(composition->frameRate().framerate())));
                fpsLayout->addWidget(new QLabel(QStringLiteral("Frame Rate"), dialog));
                fpsLayout->addWidget(fpsSpin);
                layout->addLayout(fpsLayout);

                const FrameRange currentRange = composition->frameRange().normalized();
                auto* rangeLayout = new QHBoxLayout();
                auto* startSpin = new QSpinBox(dialog);
                startSpin->setRange(-1000000, 1000000);
                startSpin->setValue(static_cast<int>(currentRange.start()));
                auto* endSpin = new QSpinBox(dialog);
                endSpin->setRange(-1000000, 1000000);
                endSpin->setValue(static_cast<int>(currentRange.end()));
                rangeLayout->addWidget(new QLabel(QStringLiteral("Start"), dialog));
                rangeLayout->addWidget(startSpin);
                rangeLayout->addWidget(new QLabel(QStringLiteral("End"), dialog));
                rangeLayout->addWidget(endSpin);
                layout->addLayout(rangeLayout);

                auto* infoLabel = new QLabel(
                    QStringLiteral("ID: %1").arg(compositionId.toString()), dialog);
                {
                    QPalette pal = infoLabel->palette();
                    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(135));
                    infoLabel->setPalette(pal);
                }
                layout->addWidget(infoLabel);

                const DialogButtonRow buttons = createWindowsDialogButtonRow(dialog);
                layout->addWidget(buttons.widget);

                QObject::connect(buttons.cancelButton, &QPushButton::clicked, dialog, &QDialog::reject);
                QObject::connect(buttons.okButton, &QPushButton::clicked, dialog, [this, dialog, svc, compositionId, composition, nameEdit, widthSpin, heightSpin, fpsSpin, startSpin, endSpin]() {
                    const QString trimmedName = nameEdit->text().trimmed();
                    if (trimmedName.isEmpty()) {
                        QMessageBox::warning(dialog, QStringLiteral("Composition Settings"),
                            QStringLiteral("Name must not be empty."));
                        return;
                    }

                    const int startFrame = startSpin->value();
                    const int endFrame = endSpin->value();
                    if (startFrame > endFrame) {
                        QMessageBox::warning(dialog, QStringLiteral("Composition Settings"),
                            QStringLiteral("Start frame must be less than or equal to end frame."));
                        return;
                    }

                    composition->setCompositionName(UniString::fromQString(trimmedName));
                    composition->setCompositionSize(QSize(widthSpin->value(), heightSpin->value()));
                    composition->setFrameRate(FrameRate(static_cast<float>(fpsSpin->value())));
                    composition->setFrameRange(FrameRange(FramePosition(startFrame), FramePosition(endFrame)));

                    if (!svc->renameComposition(compositionId, UniString::fromQString(trimmedName))) {
                        QMessageBox::warning(dialog, QStringLiteral("Composition Settings"),
                            QStringLiteral("Failed to update composition name."));
                        return;
                    }

                    if (auto project = svc->getCurrentProjectSharedPtr()) {
                        project->projectChanged();
                    }
                    if (auto current = svc->currentComposition().lock()) {
                        if (current->id() == compositionId) {
                            if (auto* playback = ArtifactPlaybackService::instance()) {
                                playback->setFrameRange(composition->frameRange());
                                playback->setFrameRate(composition->frameRate());
                            }
                        }
                    }

                    dialog->accept();
                });

                dialog->exec();
            }, loadProjectViewIcon(QStringLiteral("MaterialVS/neutral/settings.svg")));
            
            addTrackedAction(QStringLiteral("interpret_footage"), QStringLiteral("Interpret Footage..."), []() {
                // Placeholder for footage settings
            }, loadProjectViewIcon(QStringLiteral("MaterialVS/purple/info.svg")));
        }

        if (type == eProjectItemType::Footage) {
            addTrackedAction(QStringLiteral("reveal_in_explorer"), QStringLiteral("Reveal in Explorer (R)"), [item]() {
                if (item && item->type() == eProjectItemType::Footage) {
                    QString path = static_cast<FootageItem*>(item)->filePath;
                    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
                }
            }, loadProjectViewIcon(QStringLiteral("MaterialVS/blue/folder.svg")));
            addTrackedAction(QStringLiteral("copy_file_path"), QStringLiteral("Copy File Path"), [item]() {
                if (item && item->type() == eProjectItemType::Footage) {
                    QApplication::clipboard()->setText(static_cast<FootageItem*>(item)->filePath);
                }
            }, loadProjectViewIcon(QStringLiteral("MaterialVS/neutral/content_copy.svg")));
            addTrackedAction(QStringLiteral("relink_selected_footage"), QStringLiteral("Relink Selected Footage..."), [this, item, svc]() {
                if (!svc || !item || item->type() != eProjectItemType::Footage) return;
                const QString root = QFileDialog::getExistingDirectory(this, "Relink Selected Footage - Search Root");
                if (root.isEmpty()) return;
                QVector<FootageItem*> targets;
                targets.append(static_cast<FootageItem*>(item));
                const int relinked = Impl::relinkMissingFootage(root, targets);
                if (relinked > 0) {
                    svc->projectChanged();
                }
                QMessageBox::information(this, "Relink Result",
                                         QString("Relinked %1 file(s).").arg(relinked));
            }, loadProjectViewIcon(QStringLiteral("MaterialVS/yellow/link.svg")));
        }

        menu.addSeparator();
        addTrackedAction(QStringLiteral("rename"), QStringLiteral("Rename"), [this, idx]() {
            bool ok;
            QString name = QInputDialog::getText(this, "Rename", "New Name:", QLineEdit::Normal, idx.data().toString(), &ok);
            if(ok && !name.isEmpty()) {
                 QModelIndex sourceIdx = idx;
                 if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(idx.model())) {
                     sourceIdx = proxy->mapToSource(idx).siblingAtColumn(0);
                 }
                 QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
                 ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
                 if (!renameProjectItem(item, name)) {
                     QMessageBox::warning(this, QStringLiteral("Rename Failed"),
                        QStringLiteral("Could not rename the selected project item."));
                }
            }
        }, loadProjectViewIcon(QStringLiteral("MaterialVS/blue/edit.svg")));

        QMenu* moveToFolderMenu = menu.addMenu(QStringLiteral("Move to Folder"));
        moveToFolderMenu->setIcon(loadProjectViewIcon(QStringLiteral("MaterialVS/yellow/folder.svg")));
        bool hasMoveTarget = false;
        if (svc && item) {
            if (auto project = svc->getCurrentProjectSharedPtr()) {
                QVector<FolderItem*> folders;
                const auto roots = project->projectItems();
                for (auto* root : roots) {
                    collectFolders(root, folders);
                }
                for (auto* folder : folders) {
                    if (!folder) {
                        continue;
                    }
                    QAction* moveAction = moveToFolderMenu->addAction(folderDisplayPath(folder));
                    moveAction->setIcon(loadProjectViewIcon(QStringLiteral("MaterialVS/yellow/folder.svg")));
                    const bool canMove = (folder != item) && !isDescendantOf(folder, item);
                    moveAction->setEnabled(canMove);
                    if (!canMove) {
                        continue;
                    }
                    hasMoveTarget = true;
                    QObject::connect(moveAction, &QAction::triggered, this, [this, svc, item, folder]() {
                        if (!svc || !item || !folder) {
                            return;
                        }
                        if (!svc->moveProjectItem(item, folder)) {
                            QMessageBox::warning(this, QStringLiteral("Move Failed"),
                                QStringLiteral("Could not move the selected item to the target folder."));
                        }
                    });
                }
            }
        }
        if (!hasMoveTarget) {
            QAction* emptyAction = moveToFolderMenu->addAction(QStringLiteral("(No valid target folder)"));
            emptyAction->setIcon(loadProjectViewIcon(QStringLiteral("MaterialVS/neutral/help.svg")));
            emptyAction->setEnabled(false);
        }

        addTrackedAction(QStringLiteral("delete"), QStringLiteral("Delete"), [this, item, svc]() {
            if (!svc || !item) {
                return;
            }
            const QString message = svc->projectItemRemovalConfirmationMessage(item);
            if (!ArtifactMessageBox::confirmDelete(this, QStringLiteral("項目削除"), message)) {
                return;
            }
            if (!svc->removeProjectItem(item)) {
                QMessageBox::warning(this, QStringLiteral("削除失敗"),
                    QStringLiteral("項目の削除に失敗しました。"));
            }
        }, loadProjectViewIcon(QStringLiteral("MaterialVS/red/delete.svg")));

        menu.addSeparator();
        addTrackedAction(QStringLiteral("expand_children"), QStringLiteral("Expand Children"), [this, idx]() {
            setExpanded(idx, true);
        }, loadProjectViewIcon(QStringLiteral("MaterialVS/blue/visibility.svg")));
        addTrackedAction(QStringLiteral("collapse_children"), QStringLiteral("Collapse Children"), [this, idx]() {
            setExpanded(idx, false);
        }, loadProjectViewIcon(QStringLiteral("MaterialVS/orange/visibility_off.svg")));
        
        menu.addSeparator();
    }

    // "New" menu group
    auto newMenu = menu.addMenu("New");
    newMenu->setIcon(loadProjectViewIcon(QStringLiteral("MaterialVS/green/check_circle.svg")));
    addTrackedNewAction(newMenu, QStringLiteral("new_composition"), QStringLiteral("Composition..."), [this]() {
         auto dialog = new CreateCompositionDialog(this);
         if (dialog->exec()) {
             const ArtifactCompositionInitParams params = dialog->acceptedInitParams();
             QTimer::singleShot(0, this, [params]() {
                 if (auto* svc = ArtifactProjectService::instance()) {
                     svc->createComposition(params);
                 }
             });
         }
         dialog->deleteLater();
    }, loadProjectViewIcon(QStringLiteral("MaterialVS/blue/movie_creation.svg")));
    addTrackedNewAction(newMenu, QStringLiteral("new_solid"), QStringLiteral("Solid..."), [this, svc]() {
        if (!svc) return;
        if (!svc->currentComposition().lock()) {
            // Try to create a comp first if none exists
            if (svc->hasProject()) {
                svc->createComposition(UniString(QStringLiteral("Composition")));
            }
        }
        if (!svc->currentComposition().lock()) {
            QMessageBox::warning(this, "Layer", "コンポジションが選択されていません。");
            return;
        }
        CreateSolidLayerSettingDialog dialog(this);
        QObject::connect(&dialog, &CreateSolidLayerSettingDialog::submit, this, [svc](const ArtifactSolidLayerInitParams& params) {
            if (svc) {
                svc->addLayerToCurrentComposition(params);
            }
        });
        dialog.setModal(true);
        dialog.exec();
    }, loadProjectViewIcon(QStringLiteral("MaterialVS/purple/format_shapes.svg")));
    addTrackedNewAction(newMenu, QStringLiteral("new_folder"), QStringLiteral("Folder"), [this]() {
        impl_->createFolderAtSelection(this);
    }, loadProjectViewIcon(QStringLiteral("MaterialVS/yellow/folder.svg")));

    menu.addSeparator();
    addTrackedAction(QStringLiteral("expand_all"), QStringLiteral("Expand All"), [this]() { expandAll(); },
                     loadProjectViewIcon(QStringLiteral("MaterialVS/blue/visibility.svg")));
    addTrackedAction(QStringLiteral("collapse_all"), QStringLiteral("Collapse All"), [this]() { collapseAll(); },
                     loadProjectViewIcon(QStringLiteral("MaterialVS/orange/visibility_off.svg")));
    addTrackedAction(QStringLiteral("refresh_view"), QStringLiteral("Refresh View"), [this]() { update(); },
                     loadProjectViewIcon(QStringLiteral("MaterialVS/blue/replay.svg")));
    addTrackedAction(QStringLiteral("show_dependency_graph"), QStringLiteral("Show Dependency Graph..."), [this, svc]() {
        Impl::showDependencyGraphDialog(this, svc);
    }, loadProjectViewIcon(QStringLiteral("MaterialVS/yellow/link.svg")));
    menu.addSeparator();
    addTrackedAction(QStringLiteral("relink_missing_footage"), QStringLiteral("Relink Missing Footage..."), [this, svc]() {
        if (!svc) return;
        const QString root = QFileDialog::getExistingDirectory(this, "Relink Missing Footage - Search Root");
        if (root.isEmpty()) return;

        QVector<FootageItem*> targets;
        const auto roots = svc->projectItems();
        for (auto* r : roots) {
            Impl::collectFootage(r, targets);
        }

        const int relinked = Impl::relinkMissingFootage(root, targets);
        if (relinked > 0) {
            svc->projectChanged();
        }
        QMessageBox::information(this, "Relink Result",
                                 QString("Relinked %1 missing footage item(s).").arg(relinked));
    }, loadProjectViewIcon(QStringLiteral("MaterialVS/yellow/link.svg")));
    menu.addSeparator();
    addTrackedAction(QStringLiteral("import_file"), QStringLiteral("Import File..."), [this, svc]() {
        Q_UNUSED(svc);
        QStringList paths = QFileDialog::getOpenFileNames(this, "Import Files", "", "All Files (*.*)");
        for (const auto& p : paths) {
             impl_->handleFileDrop(p);
        }
    }, loadProjectViewIcon(QStringLiteral("MaterialVS/green/file_open.svg")));

    if (!impl_->lastContextCommandId.isEmpty() && availableContextCommands.contains(impl_->lastContextCommandId)) {
        QAction* firstAction = menu.actions().isEmpty() ? nullptr : menu.actions().first();
        QAction* separator = firstAction ? menu.insertSeparator(firstAction) : menu.addSeparator();
        const QString repeatLabel = QStringLiteral("Repeat Last Command: %1").arg(
            availableContextLabels.value(impl_->lastContextCommandId, impl_->lastContextCommandLabel));
        QAction* repeatAction = new QAction(repeatLabel, &menu);
        repeatAction->setIcon(loadProjectViewIcon(QStringLiteral("MaterialVS/blue/replay.svg")));
        const QString commandId = impl_->lastContextCommandId;
        const QString commandLabel = availableContextLabels.value(commandId, impl_->lastContextCommandLabel);
        QObject::connect(repeatAction, &QAction::triggered, &menu, [this, commandId, commandLabel, run = availableContextCommands.value(commandId)]() mutable {
            impl_->lastContextCommandId = commandId;
            impl_->lastContextCommandLabel = commandLabel;
            run();
        });
        if (separator) {
            menu.insertAction(separator, repeatAction);
        } else {
            menu.addAction(repeatAction);
        }
    }

    if (!impl_->lastNewCommandId.isEmpty() && availableNewCommands.contains(impl_->lastNewCommandId)) {
        QAction* firstNewAction = newMenu->actions().isEmpty() ? nullptr : newMenu->actions().first();
        QAction* separator = firstNewAction ? newMenu->insertSeparator(firstNewAction) : newMenu->addSeparator();
        const QString repeatLabel = QStringLiteral("Repeat Last New Command: %1").arg(
            availableNewLabels.value(impl_->lastNewCommandId, impl_->lastNewCommandLabel));
        QAction* repeatAction = new QAction(repeatLabel, newMenu);
        repeatAction->setIcon(loadProjectViewIcon(QStringLiteral("MaterialVS/green/replay.svg")));
        const QString commandId = impl_->lastNewCommandId;
        const QString commandLabel = availableNewLabels.value(commandId, impl_->lastNewCommandLabel);
        QObject::connect(repeatAction, &QAction::triggered, newMenu, [this, commandId, commandLabel, run = availableNewCommands.value(commandId)]() mutable {
            impl_->lastNewCommandId = commandId;
            impl_->lastNewCommandLabel = commandLabel;
            run();
        });
        if (separator) {
            newMenu->insertAction(separator, repeatAction);
        } else {
            newMenu->addAction(repeatAction);
        }
    }

    menu.exec(event->globalPos());
}

void ArtifactProjectView::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    auto mapToSourceColumn0 = [](const QModelIndex& idx) -> QModelIndex {
        if (!idx.isValid()) {
            return {};
        }
        if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(idx.model())) {
            return proxy->mapToSource(idx).siblingAtColumn(0);
        }
        return idx.siblingAtColumn(0);
    };
    auto itemFromIndex = [&](const QModelIndex& idx) -> ProjectItem* {
        const QModelIndex src = mapToSourceColumn0(idx);
        const QVariant ptrVar = src.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        return ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
    };

    // Internal DnD move: reparent the currently selected item to a folder.
    const bool isInternalDnD = (event->source() == this)
        && mimeData->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist"));
    if (isInternalDnD) {
        auto* svc = ArtifactProjectService::instance();
        if (!svc) {
            event->ignore();
            return;
        }

        QModelIndexList selectedRows;
        if (selectionModel()) {
            selectedRows = selectionModel()->selectedRows(0);
        }
        if (selectedRows.isEmpty()) {
            QModelIndex curr = currentIndex();
            if (curr.isValid()) {
                selectedRows.append(curr);
            }
        }

        if (selectedRows.isEmpty()) {
            event->ignore();
            return;
        }

        const QModelIndex targetIndex = indexAt(event->position().toPoint());
        ProjectItem* targetItem = itemFromIndex(targetIndex);
        ProjectItem* targetFolder = nullptr;

        if (!targetItem) {
            if (auto project = svc->getCurrentProjectSharedPtr()) {
                const auto roots = project->projectItems();
                for (auto* root : roots) {
                    if (root && root->type() == eProjectItemType::Folder) {
                        targetFolder = root;
                        break;
                    }
                }
            }
        } else if (targetItem->type() == eProjectItemType::Folder) {
            targetFolder = targetItem;
        } else {
            targetFolder = targetItem->parent;
        }

        if (!targetFolder) {
            event->ignore();
            return;
        }

        bool movedAny = false;
        for (const QModelIndex& idx : selectedRows) {
            ProjectItem* item = itemFromIndex(idx);
            if (!item || item == targetFolder || isDescendantOf(targetFolder, item)) {
                continue;
            }
            if (svc->moveProjectItem(item, targetFolder)) {
                movedAny = true;
            }
        }

        if (!movedAny) {
            event->ignore();
            return;
        }

        event->setDropAction(Qt::MoveAction);
        event->accept();
        return;
    }

    if (!mimeData->hasUrls()) {
        event->ignore();
        return;
    }

    QStringList importTargets;
    for (const QUrl& url : mimeData->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        collectImportablePaths(url.toLocalFile(), importTargets);
    }
    importTargets.removeDuplicates();
    if (importTargets.isEmpty()) {
        event->ignore();
        return;
    }

    if (auto* svc = ArtifactProjectService::instance()) {
        svc->importAssetsFromPaths(importTargets);
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void ArtifactProjectView::dragEnterEvent(QDragEnterEvent* event) {
    const bool isInternalDnD = (event->source() == this)
        && event->mimeData()->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist"));
    if (isInternalDnD) {
        event->acceptProposedAction();
        return;
    }
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else event->ignore();
}

void ArtifactProjectView::dragMoveEvent(QDragMoveEvent* event) {
    const bool isInternalDnD = (event->source() == this)
        && event->mimeData()->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist"));
    if (isInternalDnD) {
        event->acceptProposedAction();
        return;
    }
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else event->ignore();
}

void ArtifactProjectView::mousePressEvent(QMouseEvent* event) {
    if (!impl_) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (impl_->hoverTimer) {
        impl_->hoverTimer->stop();
    }
    if (impl_->hoverPopup) impl_->hoverPopup->hide();
    if (impl_->nameEditor && impl_->nameEditor->isVisible()) { impl_->nameEditor->hide(); impl_->editingIndex = QModelIndex(); update(); }
    const QPoint mousePos = event->position().toPoint();
    if (mousePos.y() < impl_->headerHeight) {
        const HeaderResizeHit resizeHit = headerResizeHit(impl_->columnWidths, mousePos, impl_->headerHeight);
        if (resizeHit.column >= 0) {
            impl_->resizingColumn = resizeHit.column;
            impl_->resizeStartX = resizeHit.boundaryX;
            setCursor(Qt::SplitHCursor);
            event->accept();
            return;
        }

        int x = 0;
        for (int i = 0; i < impl_->columnWidths.size(); ++i) {
            const int width = impl_->columnWidths[i];
            const QRect hr(x, 0, width, impl_->headerHeight);
            if (hr.contains(mousePos)) {
                if (impl_->sortingEnabled) { impl_->sortOrder = (impl_->sortColumn == i && impl_->sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder; impl_->sortColumn = i; sortByColumn(i, impl_->sortOrder); }
                return;
            }
            x += width;
        }
    }
    if (event->button() == Qt::LeftButton) {
        impl_->dragStartPos = mousePos;
        const QModelIndex idx = indexAt(mousePos);
        if (idx.isValid()) {
            const QRect rowRect = visualRect(idx);
            const int row = impl_->rowForIndex(idx);
            const int depth = (row >= 0 && row < impl_->visibleRows.size()) ? impl_->visibleRows[row].depth : 0;
            const QRect branchRect(8 + depth * impl_->indentWidth, rowRect.top(), 20, rowRect.height());
            if (impl_->hasChildren(idx) && branchRect.contains(mousePos)) { setExpanded(idx, !impl_->isExpanded(idx)); return; }
            if (selectionModel()) {
                if (event->modifiers() & Qt::ControlModifier) selectionModel()->select(idx, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
                else if (event->modifiers() & Qt::ShiftModifier) {
                    QModelIndex curr = selectionModel()->currentIndex();
                    int s = impl_->rowForIndex(curr), e = impl_->rowForIndex(idx);
                    if (s != -1 && e != -1) { QItemSelection sel; for (int i = std::min(s, e); i <= std::max(s, e); ++i) sel.select(impl_->visibleRows[i].index0, impl_->visibleRows[i].index0); selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows); }
                } else selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
            return;
        }
        if (selectionModel()) selectionModel()->clearSelection();
        update();
    }
    QWidget::mousePressEvent(event);
}

void ArtifactProjectView::mouseReleaseEvent(QMouseEvent* event) {
    if (impl_->resizingColumn != -1) { impl_->resizingColumn = -1; unsetCursor(); return; }
    QWidget::mouseReleaseEvent(event);
}

void ArtifactProjectView::keyPressEvent(QKeyEvent* event)
{
    if (!impl_ || impl_->visibleRows.isEmpty()) { QWidget::keyPressEvent(event); return; }
    if (event->key() == Qt::Key_F2) { if (currentIndex().isValid()) editIndex(currentIndex()); return; }
    
    // R キーで選択フッテージをエクスプローラーで表示
    if (event->key() == Qt::Key_R) {
        QModelIndex idx = currentIndex();
        if (idx.isValid()) {
            QModelIndex sourceIdx = idx;
            if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(idx.model())) {
                sourceIdx = proxy->mapToSource(idx).siblingAtColumn(0);
            }
            QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
            ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
            if (item && item->type() == eProjectItemType::Footage) {
                QString path = static_cast<FootageItem*>(item)->filePath;
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
                event->accept();
                return;
            }
        }
    }
    
    QModelIndex target = currentIndex();
    int currRow = impl_->rowForIndex(target);
    if (currRow < 0) { currRow = 0; target = impl_->visibleRows.front().index0; }
    const int vRows = (height() - impl_->headerHeight) / impl_->rowHeight;
    const int lastRow = static_cast<int>(impl_->visibleRows.size()) - 1;
    switch (event->key()) {
    case Qt::Key_Up: target = impl_->visibleRows[std::max(0, currRow - 1)].index0; break;
    case Qt::Key_Down: target = impl_->visibleRows[std::min(currRow + 1, lastRow)].index0; break;
    case Qt::Key_PageUp: target = impl_->visibleRows[std::max(0, currRow - vRows)].index0; break;
    case Qt::Key_PageDown: target = impl_->visibleRows[std::min(currRow + vRows, lastRow)].index0; break;
    case Qt::Key_Left: if (target.isValid() && impl_->hasChildren(target) && impl_->isExpanded(target)) collapse(target); else if (target.parent().isValid()) target = target.parent().siblingAtColumn(0); return;
    case Qt::Key_Right: if (target.isValid() && impl_->hasChildren(target) && !impl_->isExpanded(target)) expand(target); else if (target.isValid() && impl_->hasChildren(target)) target = impl_->model->index(0, 0, target); return;
    case Qt::Key_Return: case Qt::Key_Enter: handleItemDoubleClicked(target); return;
    default: QWidget::keyPressEvent(event); return;
    }
    if (target.isValid()) { setCurrentIndex(target); itemSelected(target); }
}

QSize ArtifactProjectView::sizeHint() const { return QSize(400, 400); }

// --- Main Widget Implementation ---
class ArtifactProjectManagerWidget::Impl {
public:
    ArtifactProjectView* projectView_ = nullptr;
    ArtifactProjectModel* projectModel_ = nullptr;
    ProjectFilterProxyModel* proxyModel_ = nullptr;
    ProjectInfoPanel* infoPanel_ = nullptr;
    QLineEdit* searchBar = nullptr;
    QComboBox* typeFilterBox = nullptr;
    QCheckBox* unusedOnlyCheck = nullptr;
    QProgressBar* proxyQueueProgress = nullptr;
    ArtifactProjectManagerToolBox* toolBox = nullptr;
    QLabel* projectNameLabel = nullptr;
    QLabel* selectionSummaryLabel = nullptr;
    QLabel* selectionDetailLabel = nullptr;
    QPushButton* openSelectionButton = nullptr;
    QPushButton* revealSelectionButton = nullptr;
    QPushButton* renameSelectionButton = nullptr;
    QPushButton* deleteSelectionButton = nullptr;
    QPushButton* relinkSelectionButton = nullptr;
    QPushButton* copyPathButton = nullptr;
    bool thumbnailEnabled = true;
    QSet<QString> unusedAssetPaths_;
    struct ProxyJob {
        QString inputPath;
        QString outputPath;
    };
    std::deque<ProxyJob> proxyJobs_;
    QTimer* proxyQueueTimer_ = nullptr;
    QMetaObject::Connection currentRowChangedConnection_;
    bool headerLayoutInitialized_ = false;
    ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
    std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
    QTimer* thumbnailUpdateDebounce_ = nullptr;

    QModelIndex currentSelectionIndex0() const {
        if (!projectView_ || !projectView_->selectionModel()) {
            return {};
        }
        const auto rows = projectView_->selectionModel()->selectedRows(0);
        if (rows.isEmpty()) {
            return {};
        }
        QModelIndex index = rows.first();
        if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(index.model())) {
            index = proxy->mapToSource(index).siblingAtColumn(0);
        }
        return index.siblingAtColumn(0);
    }

    static int relinkMissingFootage(const QString& rootDir, const QVector<FootageItem*>& targets) {
        auto findByFileName = [](const QString& searchRoot, const QString& fileName) -> QString {
            if (searchRoot.isEmpty() || fileName.isEmpty()) {
                return QString();
            }
            QDirIterator it(searchRoot, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString candidate = it.next();
                if (QFileInfo(candidate).fileName().compare(fileName, Qt::CaseInsensitive) == 0) {
                    return candidate;
                }
            }
            return QString();
        };

        int relinked = 0;
        for (auto* footage : targets) {
            if (!footage) {
                continue;
            }
            const QFileInfo currentInfo(footage->filePath);
            if (currentInfo.exists()) {
                continue;
            }
            const QString replacement = findByFileName(rootDir, currentInfo.fileName());
            if (!replacement.isEmpty()) {
                footage->filePath = QFileInfo(replacement).absoluteFilePath();
                ++relinked;
            }
        }
        return relinked;
    }

    QModelIndex currentSourceIndexFromSelection() const {
        return currentSelectionIndex0();
    }

    ProjectItem* currentSelectedItem() const {
        const QModelIndex sourceIdx = currentSourceIndexFromSelection();
        const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        return ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
    }

    static QString normalizeFilePathForProjectSelection(const QString& path) {
        if (path.isEmpty()) {
            return {};
        }
        const QFileInfo info(path);
        const QString canonical = info.canonicalFilePath();
        if (!canonical.isEmpty()) {
            return QDir::cleanPath(canonical);
        }
        return QDir::cleanPath(info.absoluteFilePath());
    }

    bool selectItemsByFilePaths(const QStringList& filePaths) {
        if (!projectView_ || !proxyModel_ || !projectModel_ || !projectView_->selectionModel() || filePaths.isEmpty()) {
            return false;
        }

        QSet<QString> targetPaths;
        for (const QString& rawPath : filePaths) {
            const QString path = normalizeFilePathForProjectSelection(rawPath.trimmed());
            if (path.isEmpty()) {
                continue;
            }
            if (QFileInfo(path).isDir()) {
                continue;
            }
            targetPaths.insert(path);
        }
        if (targetPaths.isEmpty()) {
            return false;
        }

        QList<QModelIndex> sourceMatches;
        std::function<void(const QModelIndex&)> visit = [&](const QModelIndex& parent) {
            const int rowCount = projectModel_->rowCount(parent);
            for (int row = 0; row < rowCount; ++row) {
                const QModelIndex idx = projectModel_->index(row, 0, parent);
                if (!idx.isValid()) {
                    continue;
                }
                const QVariant ptrVar = idx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
                auto* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
                if (item && item->type() == eProjectItemType::Footage) {
                    const auto* footage = static_cast<FootageItem*>(item);
                    const QString itemPath = normalizeFilePathForProjectSelection(footage->filePath);
                    if (!itemPath.isEmpty() && targetPaths.contains(itemPath)) {
                        sourceMatches.push_back(idx);
                    }
                }
                visit(idx);
            }
        };
        visit({});

        if (sourceMatches.isEmpty()) {
            return false;
        }

        QItemSelection selection;
        QModelIndex firstProxyIndex;
        QModelIndex firstSourceIndex;
        for (const QModelIndex& sourceIdx : sourceMatches) {
            const QModelIndex proxyIdx = proxyModel_->mapFromSource(sourceIdx).siblingAtColumn(0);
            if (!proxyIdx.isValid()) {
                continue;
            }
            selection.select(proxyIdx, proxyIdx);
            if (!firstProxyIndex.isValid()) {
                firstProxyIndex = proxyIdx;
                firstSourceIndex = sourceIdx;
            }
            for (QModelIndex parent = proxyIdx.parent(); parent.isValid(); parent = parent.parent()) {
                projectView_->expand(parent);
            }
        }

        if (!firstProxyIndex.isValid()) {
            return false;
        }

        auto* sel = projectView_->selectionModel();
        QSignalBlocker blocker(sel);
        sel->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        sel->setCurrentIndex(firstProxyIndex, QItemSelectionModel::Current | QItemSelectionModel::Rows);
        projectView_->ensureIndexVisible(firstProxyIndex);
        projectView_->refreshVisibleContent();
        if (infoPanel_) {
            infoPanel_->updateInfo(firstSourceIndex);
        }
        refreshSelectionChrome();
        return true;
    }

    FolderItem* currentFolderTarget() const {
        ProjectItem* item = currentSelectedItem();
        if (!item) {
            return nullptr;
        }
        if (item->type() == eProjectItemType::Folder) {
            return static_cast<FolderItem*>(item);
        }
        if (item->parent && item->parent->type() == eProjectItemType::Folder) {
            return static_cast<FolderItem*>(item->parent);
        }
        return nullptr;
    }

    void createFolderAtSelection() const {
        auto* svc = ArtifactProjectService::instance();
        if (!svc) {
            return;
        }
        auto project = svc->getCurrentProjectSharedPtr();
        if (!project) {
            return;
        }

        bool ok;
        QString name = QInputDialog::getText(projectView_, QStringLiteral("New Folder"),
            QStringLiteral("Folder Name:"), QLineEdit::Normal, QStringLiteral("New Folder"), &ok);
        
        if (!ok || name.trimmed().isEmpty()) {
            return;
        }

        project->createFolder(UniString::fromQString(name), currentFolderTarget());
        // createFolder notifies projectChanged() internally
    }

    bool renameSelectedItem(QWidget* parent) {
        ProjectItem* item = currentSelectedItem();
        if (!item) {
            return false;
        }
        bool ok = false;
        const QString currentName = item->name.toQString();
        const QString newName = QInputDialog::getText(parent, "Rename", "New Name:", QLineEdit::Normal, currentName, &ok);
        if (!ok || newName.trimmed().isEmpty()) {
            return false;
        }
        return renameProjectItem(item, newName);
    }

    bool deleteSelectedItem(QWidget* parent) {
        ProjectItem* item = currentSelectedItem();
        auto* svc = ArtifactProjectService::instance();
        if (!item || !svc) {
            return false;
        }
        const QString message = svc->projectItemRemovalConfirmationMessage(item);
        const auto answer = QMessageBox::question(
            parent,
            QStringLiteral("項目削除"),
            message,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return false;
        }
        return svc->removeProjectItem(item);
    }

    QString selectedItemPath() const {
        ProjectItem* item = currentSelectedItem();
        if (!item) {
            return QString();
        }
        if (item->type() == eProjectItemType::Footage) {
            return static_cast<FootageItem*>(item)->filePath;
        }
        if (item->type() == eProjectItemType::Folder) {
            return folderDisplayPath(static_cast<FolderItem*>(item));
        }
        if (item->type() == eProjectItemType::Composition) {
            return static_cast<CompositionItem*>(item)->compositionId.toString();
        }
        return item->name.toQString();
    }

    QStringList selectedItemIds() const {
        QStringList ids;
        if (!projectView_ || !projectView_->selectionModel()) {
            return ids;
        }
        const auto rows = projectView_->selectionModel()->selectedRows(0);
        ids.reserve(rows.size());
        for (const auto& row : rows) {
            QModelIndex sourceIdx = row;
            if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(sourceIdx.model())) {
                sourceIdx = proxy->mapToSource(sourceIdx).siblingAtColumn(0);
            }
            const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
            auto* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
            if (item) {
                ids.push_back(item->id.toString());
            }
        }
        return ids;
    }

    SelectionChangedEvent makeSelectionChangedEvent() const {
        SelectionChangedEvent event;
        event.selectedItemIds = selectedItemIds();
        event.currentItemId = currentSelectedItem() ? currentSelectedItem()->id.toString() : QString();
        event.selectedCount = event.selectedItemIds.size();
        return event;
    }

    QString selectionSummaryText() const {
        const int selectedCount = projectView_ && projectView_->selectionModel()
            ? projectView_->selectionModel()->selectedRows(0).size()
            : 0;
        const QString searchText = searchBar ? searchBar->text().trimmed() : QString();
        const QString typeText = typeFilterBox ? typeFilterBox->currentText() : QStringLiteral("All");
        const QString unusedText = unusedOnlyCheck && unusedOnlyCheck->isChecked()
            ? QStringLiteral("Unused only")
            : QStringLiteral("All items");
        return QStringLiteral("Selected: %1 | Filter: %2 | Type: %3 | %4")
            .arg(selectedCount)
            .arg(searchText.isEmpty() ? QStringLiteral("-") : searchText)
            .arg(typeText)
            .arg(unusedText);
    }

    void refreshSelectionChrome() {
        if (selectionSummaryLabel) {
            selectionSummaryLabel->setText(selectionSummaryText());
        }
        ProjectItem* item = currentSelectedItem();
        const bool hasItem = item != nullptr;
        const bool isFootage = item && item->type() == eProjectItemType::Footage;
        const bool isFolder = item && item->type() == eProjectItemType::Folder;
        const bool isComposition = item && item->type() == eProjectItemType::Composition;
        const QString pathText = selectedItemPath();
        const QString statusText = !item ? QStringLiteral("No selection")
            : isFootage ? (QFileInfo(static_cast<FootageItem*>(item)->filePath).exists() ? QStringLiteral("Available") : QStringLiteral("Missing"))
            : isFolder ? QStringLiteral("Folder")
            : isComposition ? QStringLiteral("Composition")
            : QStringLiteral("Item");
        if (selectionDetailLabel) {
            if (!hasItem) {
                selectionDetailLabel->setText(QStringLiteral("Use the search bar or click an item to inspect it."));
            } else {
                selectionDetailLabel->setText(QStringLiteral("%1 | %2").arg(statusText, pathText.isEmpty() ? QStringLiteral("-") : pathText));
            }
        }
        if (openSelectionButton) openSelectionButton->setEnabled(hasItem);
        if (revealSelectionButton) revealSelectionButton->setEnabled(isFootage);
        if (renameSelectionButton) renameSelectionButton->setEnabled(hasItem);
        if (deleteSelectionButton) deleteSelectionButton->setEnabled(hasItem);
        if (copyPathButton) copyPathButton->setEnabled(isFootage);
        if (relinkSelectionButton) {
            relinkSelectionButton->setEnabled(isFootage && !QFileInfo(static_cast<FootageItem*>(item)->filePath).exists());
        }
    }

    void openSelectedItem(QWidget* parent) {
        Q_UNUSED(parent);
        const QModelIndex idx = currentSelectionIndex0();
        if (!idx.isValid()) {
            return;
        }
        if (projectView_) {
            ProjectItem* item = currentSelectedItem();
            if (item && item->type() == eProjectItemType::Folder) {
                projectView_->handleItemDoubleClicked(idx);
            } else {
                projectView_->itemDoubleClicked(idx);
                projectView_->handleItemDoubleClicked(idx);
            }
        }
    }

    void revealSelectedItem(QWidget* parent) {
        Q_UNUSED(parent);
        ProjectItem* item = currentSelectedItem();
        if (!item) {
            return;
        }
        if (item->type() == eProjectItemType::Footage) {
            const QString path = static_cast<FootageItem*>(item)->filePath;
            if (!path.isEmpty()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
            }
        }
    }

    void copySelectedPathToClipboard() {
        const QString path = selectedItemPath();
        if (!path.isEmpty()) {
            QApplication::clipboard()->setText(path);
        }
    }

    void relinkSelectedItem(QWidget* parent) {
        auto* svc = ArtifactProjectService::instance();
        ProjectItem* item = currentSelectedItem();
        if (!svc || !item || item->type() != eProjectItemType::Footage) {
            return;
        }
        const QString root = QFileDialog::getExistingDirectory(parent, QStringLiteral("Relink Selected Footage - Search Root"));
        if (root.isEmpty()) {
            return;
        }
        QVector<FootageItem*> targets;
        targets.append(static_cast<FootageItem*>(item));
        const int relinked = Impl::relinkMissingFootage(root, targets);
        if (relinked > 0) {
            svc->projectChanged();
        }
        QMessageBox::information(parent, QStringLiteral("Relink Result"),
                                 QStringLiteral("Relinked %1 file(s).").arg(relinked));
    }

    void syncSelectionToCurrentComposition() {
        if (!projectView_ || !proxyModel_) {
            return;
        }
        auto* svc = ArtifactProjectService::instance();
        if (!svc) {
            return;
        }
        auto currentComp = svc->currentComposition().lock();
        if (!currentComp) {
            return;
        }

        std::function<QModelIndex(const QModelIndex&)> findCompositionIndex = [&](const QModelIndex& parent) -> QModelIndex {
            if (!projectModel_) {
                return {};
            }
            const int rowCount = projectModel_->rowCount(parent);
            for (int row = 0; row < rowCount; ++row) {
                const QModelIndex idx = projectModel_->index(row, 0, parent);
                if (!idx.isValid()) {
                    continue;
                }
                const QVariant typeVar = idx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
                if (typeVar.isValid() && typeVar.toInt() == static_cast<int>(eProjectItemType::Composition)) {
                    const QVariant idVar = idx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
                    if (idVar.isValid() && idVar.toString() == currentComp->id().toString()) {
                        return idx;
                    }
                }
                if (const QModelIndex child = findCompositionIndex(idx); child.isValid()) {
                    return child;
                }
            }
            return {};
        };

        const QModelIndex sourceIndex = findCompositionIndex({});
        if (!sourceIndex.isValid()) {
            return;
        }
        const QModelIndex proxyIndex = proxyModel_->mapFromSource(sourceIndex);
        if (!proxyIndex.isValid()) {
            return;
        }
        for (QModelIndex parent = proxyIndex.parent(); parent.isValid(); parent = parent.parent()) {
            projectView_->expand(parent);
        }
        projectView_->selectionModel()->setCurrentIndex(
            proxyIndex,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Current);
        projectView_->ensureIndexVisible(proxyIndex);
        if (infoPanel_) {
            infoPanel_->updateInfo(sourceIndex);
        }
    }

    static void collectFootageRecursive(ProjectItem* item, QVector<FootageItem*>& out) {
        if (!item) return;
        if (item->type() == eProjectItemType::Footage) {
            out.append(static_cast<FootageItem*>(item));
        }
        for (auto* child : item->children) {
            collectFootageRecursive(child, out);
        }
    }

    void queueProxyGeneration(const QVector<FootageItem*>& footageItems) {
        const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir proxyDir(appDataRoot);
        proxyDir.mkpath(QStringLiteral("ProxyCache"));
        const QString proxyRoot = proxyDir.filePath(QStringLiteral("ProxyCache"));

        for (auto* f : footageItems) {
            if (!f || f->filePath.isEmpty()) continue;
            const QFileInfo src(f->filePath);
            if (!src.exists()) continue;
            const QString out = QDir(proxyRoot).filePath(src.completeBaseName() + QStringLiteral(".proxy.jpg"));
            proxyJobs_.push_back({src.absoluteFilePath(), out});
        }
        if (!proxyQueueTimer_) {
            proxyQueueTimer_ = new QTimer();
            proxyQueueTimer_->setInterval(5);
            QObject::connect(proxyQueueTimer_, &QTimer::timeout, [this]() {
                processNextProxyJob();
            });
        }
        if (!proxyJobs_.empty()) {
            if (proxyQueueProgress) {
                proxyQueueProgress->setMaximum(static_cast<int>(proxyJobs_.size()));
                proxyQueueProgress->setValue(0);
                proxyQueueProgress->setVisible(true);
            }
            proxyQueueTimer_->start();
        }
    }

    void processNextProxyJob() {
        if (proxyJobs_.empty()) {
            if (proxyQueueTimer_) proxyQueueTimer_->stop();
            if (proxyQueueProgress) proxyQueueProgress->setVisible(false);
            return;
        }

        const ProxyJob job = proxyJobs_.front();
        proxyJobs_.pop_front();
        QImage img(job.inputPath);
        if (!img.isNull()) {
            const QImage scaled = img.scaled(640, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            scaled.save(job.outputPath, "JPG", 80);
        }

        if (proxyQueueProgress) {
            const int done = proxyQueueProgress->maximum() - static_cast<int>(proxyJobs_.size());
            proxyQueueProgress->setValue(done);
        }
    }

    void refreshUnusedAssetCache() {
        unusedAssetPaths_.clear();
        auto shared = ArtifactProjectService::instance()->getCurrentProjectSharedPtr();
        if (!shared) return;
        const QStringList list = ArtifactProjectCleanupTool::findUnusedAssetPaths(shared.get());
        for (const QString& s : list) {
            unusedAssetPaths_.insert(s);
        }
        if (proxyModel_) {
            proxyModel_->setUnusedAssetPaths(unusedAssetPaths_);
        }
    }

    void update() {
        auto shared = ArtifactProjectService::instance()->getCurrentProjectSharedPtr();
        if (!projectModel_) projectModel_ = new ArtifactProjectModel();
        projectModel_->setProject(shared);

        if (!proxyModel_) {
            proxyModel_ = new ProjectFilterProxyModel();
            proxyModel_->setSourceModel(projectModel_);
            proxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
            proxyModel_->setRecursiveFilteringEnabled(true);
            proxyModel_->setSortCaseSensitivity(Qt::CaseInsensitive);
        }
        if (projectView_) {
            projectView_->setModel(proxyModel_);
            projectView_->setSortingEnabled(true);
            projectView_->sortByColumn(0, Qt::AscendingOrder);
            if (!headerLayoutInitialized_) {
                projectView_->setColumnWidth(0, 260);
                projectView_->setColumnWidth(1, 120);
                projectView_->setColumnWidth(2, 120);
                projectView_->setColumnWidth(3, 100);
                projectView_->setColumnWidth(4, 140);
                projectView_->setColumnWidth(5, 180);
                headerLayoutInitialized_ = true;
            }
            projectView_->expandToDepth(1);
            if (projectView_->selectionModel()) {
                QObject::disconnect(currentRowChangedConnection_);
                currentRowChangedConnection_ =
                    QObject::connect(projectView_->selectionModel(), &QItemSelectionModel::currentRowChanged, projectView_,
                    [this](const QModelIndex& current, const QModelIndex&) {
                        if (!current.isValid() || !proxyModel_ || !infoPanel_) {
                            refreshSelectionChrome();
                            return;
                        }
                        infoPanel_->updateInfo(proxyModel_->mapToSource(current));
                        refreshSelectionChrome();
                    });
            }
            refreshSelectionChrome();
        }
        refreshUnusedAssetCache();
        if (proxyModel_) {
            proxyModel_->setAdvancedFilter(
                searchBar ? searchBar->text() : QString(),
                typeFilterBox ? typeFilterBox->currentText() : QString(),
                unusedOnlyCheck ? unusedOnlyCheck->isChecked() : false);
        }
        syncSelectionToCurrentComposition();
    }

    void handleSearch(const QString& text) {
        if (!proxyModel_) return;
        const QString trimmed = text.trimmed();
        ArtifactLayerSearchQuery query;
        query.setSearchText(trimmed);
        proxyModel_->setSearchQuery(query);
        proxyModel_->setAdvancedFilter(
            trimmed,
            typeFilterBox ? typeFilterBox->currentText() : QString(),
            unusedOnlyCheck ? unusedOnlyCheck->isChecked() : false);
        if (!trimmed.isEmpty() && projectView_) projectView_->expandAll();
        refreshSelectionChrome();
    }
};

ArtifactProjectManagerWidget::ArtifactProjectManagerWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    setObjectName(QStringLiteral("artifactProjectManagerWidget"));
    setAutoFillBackground(true);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* chromePanel = new QWidget(this);
    chromePanel->setObjectName(QStringLiteral("projectManagerChrome"));
    chromePanel->setAutoFillBackground(true);
    auto* chromeLayout = new QVBoxLayout(chromePanel);
    chromeLayout->setContentsMargins(0, 0, 0, 0);
    chromeLayout->setSpacing(0);

    impl_->infoPanel_ = new ProjectInfoPanel(chromePanel);
    chromeLayout->addWidget(impl_->infoPanel_);

    impl_->projectNameLabel = new QLabel("PROJECT");
    impl_->projectNameLabel->setObjectName(QStringLiteral("projectManagerSectionLabel"));
    chromeLayout->addWidget(impl_->projectNameLabel);

    auto* selectionChrome = new QWidget(chromePanel);
    auto* selectionChromeLayout = new QVBoxLayout(selectionChrome);
    selectionChromeLayout->setContentsMargins(10, 0, 10, 8);
    selectionChromeLayout->setSpacing(4);
    impl_->selectionSummaryLabel = new QLabel(QStringLiteral("Selected: 0 | Filter: - | Type: All | All items"), selectionChrome);
    impl_->selectionSummaryLabel->setWordWrap(true);
    {
        QFont f = impl_->selectionSummaryLabel->font();
        f.setPointSize(10);
        impl_->selectionSummaryLabel->setFont(f);
        QPalette pal = impl_->selectionSummaryLabel->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(135));
        impl_->selectionSummaryLabel->setPalette(pal);
    }
    selectionChromeLayout->addWidget(impl_->selectionSummaryLabel);
    impl_->selectionDetailLabel = new QLabel(QStringLiteral("Use the search bar or click an item to inspect it."), selectionChrome);
    impl_->selectionDetailLabel->setWordWrap(true);
    {
        QFont f = impl_->selectionDetailLabel->font();
        f.setPointSize(11);
        impl_->selectionDetailLabel->setFont(f);
        QPalette pal = impl_->selectionDetailLabel->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(120));
        impl_->selectionDetailLabel->setPalette(pal);
    }
    selectionChromeLayout->addWidget(impl_->selectionDetailLabel);

    auto* selectionButtons = new QHBoxLayout();
    selectionButtons->setSpacing(6);
    impl_->openSelectionButton = new QPushButton(QStringLiteral("Open"), selectionChrome);
    impl_->revealSelectionButton = new QPushButton(QStringLiteral("Reveal"), selectionChrome);
    impl_->renameSelectionButton = new QPushButton(QStringLiteral("Rename"), selectionChrome);
    impl_->deleteSelectionButton = new QPushButton(QStringLiteral("Delete"), selectionChrome);
    impl_->relinkSelectionButton = new QPushButton(QStringLiteral("Relink"), selectionChrome);
    impl_->copyPathButton = new QPushButton(QStringLiteral("Copy Path"), selectionChrome);
    selectionButtons->addWidget(impl_->openSelectionButton);
    selectionButtons->addWidget(impl_->revealSelectionButton);
    selectionButtons->addWidget(impl_->renameSelectionButton);
    selectionButtons->addWidget(impl_->deleteSelectionButton);
    selectionButtons->addWidget(impl_->relinkSelectionButton);
    selectionButtons->addWidget(impl_->copyPathButton);
    selectionButtons->addStretch();
    selectionChromeLayout->addLayout(selectionButtons);
    chromeLayout->addWidget(selectionChrome);

    impl_->searchBar = new QLineEdit(chromePanel);
    impl_->searchBar->setPlaceholderText("Search (type:footage tag:png regex:shot_.* unused:true)...");
    impl_->searchBar->setClearButtonEnabled(true);
    {
        QFont f = impl_->searchBar->font();
        f.setPointSize(11);
        impl_->searchBar->setFont(f);
        QPalette pal = impl_->searchBar->palette();
        pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
        pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
        pal.setColor(QPalette::PlaceholderText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(145));
        pal.setColor(QPalette::Highlight, QColor(ArtifactCore::currentDCCTheme().accentColor));
        impl_->searchBar->setPalette(pal);
    }
    chromeLayout->addWidget(impl_->searchBar);

    auto* filterBarHost = new QWidget(chromePanel);
    filterBarHost->setObjectName(QStringLiteral("projectManagerFilterBar"));
    filterBarHost->setAutoFillBackground(true);
    auto* filterBar = new QHBoxLayout(filterBarHost);
    filterBar->setContentsMargins(10, 0, 10, 6);
    filterBar->setSpacing(8);
    impl_->typeFilterBox = new QComboBox(filterBarHost);
    impl_->typeFilterBox->addItems(QStringList() << "All" << "Composition" << "Footage" << "Folder" << "Solid");
    impl_->unusedOnlyCheck = new QCheckBox("Unused only", filterBarHost);
    impl_->proxyQueueProgress = new QProgressBar(filterBarHost);
    impl_->proxyQueueProgress->setVisible(false);
    impl_->proxyQueueProgress->setTextVisible(true);
    impl_->proxyQueueProgress->setFormat("Proxy queue %v/%m");
    filterBar->addWidget(impl_->typeFilterBox);
    filterBar->addWidget(impl_->unusedOnlyCheck);
    filterBar->addStretch();
    filterBar->addWidget(impl_->proxyQueueProgress, 1);
    chromeLayout->addWidget(filterBarHost);
    mainLayout->addWidget(chromePanel);

    impl_->projectView_ = new ArtifactProjectView(this);
    impl_->projectView_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(impl_->projectView_);

    impl_->toolBox = new ArtifactProjectManagerToolBox(this);
    mainLayout->addWidget(impl_->toolBox);

    connect(impl_->searchBar, &QLineEdit::textChanged, [this](const QString& t) { impl_->handleSearch(t); });
    connect(impl_->typeFilterBox, &QComboBox::currentTextChanged, [this](const QString&) {
        impl_->handleSearch(impl_->searchBar ? impl_->searchBar->text() : QString());
    });
    connect(impl_->unusedOnlyCheck, &QCheckBox::toggled, [this](bool) {
        impl_->handleSearch(impl_->searchBar ? impl_->searchBar->text() : QString());
    });
    connect(impl_->openSelectionButton, &QPushButton::clicked, this, [this]() {
        impl_->openSelectedItem(this);
    });
    connect(impl_->revealSelectionButton, &QPushButton::clicked, this, [this]() {
        impl_->revealSelectedItem(this);
    });
    connect(impl_->renameSelectionButton, &QPushButton::clicked, this, [this]() {
        if (!impl_->renameSelectedItem(this)) {
            QMessageBox::information(this, QStringLiteral("Rename"), QStringLiteral("Select an item to rename."));
        }
    });
    connect(impl_->deleteSelectionButton, &QPushButton::clicked, this, [this]() {
        if (!impl_->deleteSelectedItem(this)) {
            QMessageBox::information(this, QStringLiteral("Delete"), QStringLiteral("Select an item to delete."));
        }
    });
    connect(impl_->relinkSelectionButton, &QPushButton::clicked, this, [this]() {
        impl_->relinkSelectedItem(this);
    });
    connect(impl_->copyPathButton, &QPushButton::clicked, this, [this]() {
        impl_->copySelectedPathToClipboard();
    });
    connect(impl_->projectView_, &ArtifactProjectView::itemSelected, [this](const QModelIndex& idx) {
        if (!impl_) {
            return;
        }
        if (impl_->proxyModel_ && impl_->infoPanel_) {
            impl_->infoPanel_->updateInfo(impl_->proxyModel_->mapToSource(idx));
        }
        impl_->eventBus_.post<SelectionChangedEvent>(impl_->makeSelectionChangedEvent());
        (void)impl_->eventBus_.drain();
    });
    connect(impl_->projectView_, &ArtifactProjectView::itemDoubleClicked, [this](const QModelIndex& idx) {
        itemDoubleClicked(idx);
    });
    connect(impl_->toolBox, &ArtifactProjectManagerToolBox::newCompositionRequested, [this]() {
         auto dialog = new CreateCompositionDialog(this);
         if (dialog->exec()) {
             const ArtifactCompositionInitParams params = dialog->acceptedInitParams();
             QTimer::singleShot(0, this, [params]() {
                 if (auto* svc = ArtifactProjectService::instance()) {
                     svc->createComposition(params);
                 }
             });
         }
         dialog->deleteLater();
    });
    connect(impl_->toolBox, &ArtifactProjectManagerToolBox::newFolderRequested, [this]() {
         impl_->createFolderAtSelection();
    });
    connect(impl_->toolBox, &ArtifactProjectManagerToolBox::generateProxyRequested, [this]() {
         QVector<FootageItem*> footage;
         auto* svc = ArtifactProjectService::instance();
         if (!svc) return;
         auto project = svc->getCurrentProjectSharedPtr();
         if (project) {
             const auto roots = project->projectItems();
             for (auto* root : roots) {
                 Impl::collectFootageRecursive(root, footage);
             }
         }
         impl_->queueProxyGeneration(footage);
    });
    connect(impl_->toolBox, &ArtifactProjectManagerToolBox::deleteRequested, [this]() {
         if (!impl_->deleteSelectedItem(this)) {
             QMessageBox::information(this, QStringLiteral("Delete"), QStringLiteral("Select an item to delete."));
         }
    });

    auto svc = ArtifactProjectService::instance();
    connect(svc, &ArtifactProjectService::projectChanged, this, [this, svc]() {
        if (!impl_) {
            return;
        }
        const QString projectName = svc && svc->getCurrentProjectSharedPtr()
            ? svc->projectName().toQString()
            : QString();
        impl_->eventBus_.post<ProjectChangedEvent>(ProjectChangedEvent{QString(), projectName});
        (void)impl_->eventBus_.drain();
    });
    connect(svc, &ArtifactProjectService::projectCreated, this, [this, svc]() {
        if (!impl_) {
            return;
        }
        const QString projectName = svc && svc->getCurrentProjectSharedPtr()
            ? svc->projectName().toQString()
            : QString();
        impl_->eventBus_.post<ProjectChangedEvent>(ProjectChangedEvent{QString(), projectName});
        (void)impl_->eventBus_.drain();
    });
    connect(svc, &ArtifactProjectService::compositionCreated, this, [this](const CompositionID& id) {
        impl_->eventBus_.post<CompositionCreatedEvent>(CompositionCreatedEvent{
            id.toString(), QString()
        });
        (void)impl_->eventBus_.drain();
    });
    connect(svc, &ArtifactProjectService::layerCreated, this, [this](const CompositionID& cid, const LayerID& lid) {
        impl_->eventBus_.post<LayerChangedEvent>(LayerChangedEvent{
            cid.toString(),
            lid.toString(),
            LayerChangedEvent::ChangeType::Created
        });
        (void)impl_->eventBus_.drain();
    });
    connect(svc, &ArtifactProjectService::layerRemoved, this, [this](const CompositionID& cid, const LayerID& lid) {
        impl_->eventBus_.post<LayerChangedEvent>(LayerChangedEvent{
            cid.toString(),
            lid.toString(),
            LayerChangedEvent::ChangeType::Removed
        });
        (void)impl_->eventBus_.drain();
    });
    connect(svc, &ArtifactProjectService::currentCompositionChanged, this, [this](const CompositionID& cid) {
        if (!impl_) {
            return;
        }
        impl_->eventBus_.post<CurrentCompositionChangedEvent>(CurrentCompositionChangedEvent{
            cid.toString()
        });
        (void)impl_->eventBus_.drain();
    });

    // EventBus subscriptions
    impl_->thumbnailUpdateDebounce_ = new QTimer(this);
    impl_->thumbnailUpdateDebounce_->setSingleShot(true);
    impl_->thumbnailUpdateDebounce_->setInterval(300);
    connect(impl_->thumbnailUpdateDebounce_, &QTimer::timeout, this, [this]() {
        updateRequested();
    });

    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<CompositionCreatedEvent>([this](const CompositionCreatedEvent&) {
        impl_->thumbnailUpdateDebounce_->start();
    }));
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<LayerChangedEvent>([this](const LayerChangedEvent&) {
        impl_->thumbnailUpdateDebounce_->start();
    }));
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<ProjectChangedEvent>([this](const ProjectChangedEvent&) {
        updateRequested();
    }));
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>([this](const CurrentCompositionChangedEvent&) {
        if (impl_) {
            impl_->syncSelectionToCurrentComposition();
        }
    }));
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<SelectionChangedEvent>([this](const SelectionChangedEvent&) {
        if (impl_) {
            impl_->refreshSelectionChrome();
        }
    }));

    impl_->refreshSelectionChrome();

    auto* focusSearchShortcut = new QShortcut(QKeySequence::Find, this);
    connect(focusSearchShortcut, &QShortcut::activated, this, [this]() {
        if (impl_->searchBar) {
            impl_->searchBar->setFocus();
            impl_->searchBar->selectAll();
        }
    });

    auto* clearSearchShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), impl_->searchBar);
    connect(clearSearchShortcut, &QShortcut::activated, this, [this]() {
        if (impl_->searchBar && !impl_->searchBar->text().isEmpty()) {
            impl_->searchBar->clear();
        }
    });

    auto* renameShortcut = new QShortcut(QKeySequence(Qt::Key_F2), this);
    connect(renameShortcut, &QShortcut::activated, this, [this]() {
        if (!impl_->renameSelectedItem(this)) {
            return;
        }
    });

    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(deleteShortcut, &QShortcut::activated, this, [this]() {
        if (!impl_->deleteSelectedItem(this)) {
            return;
        }
    });

    impl_->update();

    connect(ArtifactProjectService::instance(), &ArtifactProjectService::currentCompositionChanged, this, [this](const CompositionID&) {
        impl_->syncSelectionToCurrentComposition();
    });
}

ArtifactProjectManagerWidget::~ArtifactProjectManagerWidget() { delete impl_; }

ArtifactProjectView* ArtifactProjectManagerWidget::projectView() const
{
    return impl_ ? impl_->projectView_ : nullptr;
}

bool ArtifactProjectManagerWidget::selectItemsByFilePaths(const QStringList& filePaths)
{
    return impl_ ? impl_->selectItemsByFilePaths(filePaths) : false;
}

void ArtifactProjectManagerWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), QColor(0x20, 0x25, 0x2C));
    QWidget::paintEvent(event);
}

void ArtifactProjectManagerWidget::updateRequested() {
    impl_->update();
    if (impl_) {
        impl_->refreshSelectionChrome();
    }
    setEnabled(true);
}

void ArtifactProjectManagerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (impl_ && impl_->projectView_) {
        scheduleProjectViewRefresh(impl_->projectView_);
        update();
    }
}

void ArtifactProjectManagerWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!impl_) {
        return;
    }
    impl_->update();
    if (impl_->projectView_) {
        scheduleProjectViewRefresh(impl_->projectView_);
    }
}

bool ArtifactProjectManagerWidget::event(QEvent* event)
{
    if (impl_) {
        (void)impl_->eventBus_.drain();
    }
    const bool handled = QWidget::event(event);
    if (event && (event->type() == QEvent::WindowActivate ||
                  event->type() == QEvent::ActivationChange ||
                  event->type() == QEvent::PolishRequest)) {
        if (!impl_) {
            return handled;
        }
        if (impl_->projectView_ && isVisible()) {
            scheduleProjectViewRefresh(impl_->projectView_);
        }
        update();
    }
    return handled;
}

void ArtifactProjectManagerWidget::triggerUpdate() { impl_->update(); }
void ArtifactProjectManagerWidget::setFilter() {
    if (!impl_ || !impl_->searchBar) return;
    impl_->searchBar->setFocus();
    impl_->searchBar->selectAll();
}
void ArtifactProjectManagerWidget::setThumbnailEnabled(bool b) {
    if (!impl_ || !impl_->infoPanel_) return;
    impl_->thumbnailEnabled = b;
    impl_->infoPanel_->thumbnail->setVisible(b);
}
void ArtifactProjectManagerWidget::dropEvent(QDropEvent* event) { /* Handled by child view but kept for API */ }
void ArtifactProjectManagerWidget::dragEnterEvent(QDragEnterEvent* event) { /* Handled by child view but kept for API */ }
void ArtifactProjectManagerWidget::contextMenuEvent(QContextMenuEvent* event) { /* Handled by child view but kept for API */ }
QSize ArtifactProjectManagerWidget::sizeHint() const { return QSize(250, 600); }

// --- ToolBox Implementation ---
ArtifactProjectManagerToolBox::ArtifactProjectManagerToolBox(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("projectManagerToolBox"));
    setAutoFillBackground(true);
    setFixedHeight(32);
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(10);

    auto createBtn = [](const QString& tip, const QString& iconPath, QStyle::StandardPixmap fallbackIcon, const QString& fallbackText) {
        auto b = new QPushButton();
        b->setFixedSize(24, 24);
        b->setToolTip(tip);
        QIcon icon = loadProjectViewIcon(iconPath);
        if ((icon.isNull() || icon.pixmap(16, 16).isNull()) && QApplication::style()) {
            icon = QApplication::style()->standardIcon(fallbackIcon);
        }
        if (!icon.isNull() && !icon.pixmap(16, 16).isNull()) {
            b->setIcon(icon);
        } else {
            b->setText(fallbackText);
        }
        b->setIconSize(QSize(16, 16));
        b->setFlat(true);
        {
            QPalette pal = b->palette();
            pal.setColor(QPalette::ButtonText, QColor(ArtifactCore::currentDCCTheme().textColor));
            b->setPalette(pal);
        }
        return b;
    };

    auto btnNew = createBtn("New Composition", QStringLiteral("MaterialVS/blue/movie_creation.svg"), QStyle::SP_FileDialogNewFolder, QStringLiteral("N"));
    auto btnFolder = createBtn("New Folder", QStringLiteral("MaterialVS/yellow/folder.svg"), QStyle::SP_DirIcon, QStringLiteral("F"));
    auto btnProxy = createBtn("Generate Proxies", QStringLiteral("MaterialVS/green/replay.svg"), QStyle::SP_BrowserReload, QStringLiteral("P"));
    auto btnDel = createBtn("Delete", QStringLiteral("MaterialVS/red/delete.svg"), QStyle::SP_TrashIcon, QStringLiteral("D"));

    layout->addWidget(btnNew);
    layout->addWidget(btnFolder);
    layout->addWidget(btnProxy);
    layout->addStretch();
    layout->addWidget(btnDel);

    connect(btnNew, &QPushButton::clicked, this, &ArtifactProjectManagerToolBox::newCompositionRequested);
    connect(btnFolder, &QPushButton::clicked, this, &ArtifactProjectManagerToolBox::newFolderRequested);
    connect(btnProxy, &QPushButton::clicked, this, &ArtifactProjectManagerToolBox::generateProxyRequested);
    connect(btnDel, &QPushButton::clicked, this, &ArtifactProjectManagerToolBox::deleteRequested);
}

ArtifactProjectManagerToolBox::~ArtifactProjectManagerToolBox() {}
void ArtifactProjectManagerToolBox::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
    QWidget::paintEvent(event);
}
void ArtifactProjectManagerToolBox::resizeEvent(QResizeEvent*) {}

}
