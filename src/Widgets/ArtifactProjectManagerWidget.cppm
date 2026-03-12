module;
#include <QVector>
#include <QWidget>
#include <wobjectimpl.h>
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
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QPixmap>
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
import Artifact.Widgets.LayerPanelWidget;

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
            const QImage thumb = composition->getThumbnail(targetSize.width(), targetSize.height());
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
        setFixedHeight(90);
        auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(15);

        thumbnail = new QLabel();
        thumbnail->setFixedSize(120, 68);
        thumbnail->setAlignment(Qt::AlignCenter);
        thumbnail->setText("PREVIEW");
        thumbnail->setStyleSheet(R"(
            QLabel {
                background-color: #111;
                border: 1px solid #3e3e42;
                border-radius: 3px;
                color: #444;
                font-size: 9px;
                font-weight: bold;
            }
        )");

        auto infoLayout = new QVBoxLayout();
        infoLayout->setSpacing(2);
        infoLayout->setContentsMargins(0, 5, 0, 5);

        titleLabel = new QLabel("Project");
        titleLabel->setStyleSheet("color: #eee; font-weight: bold; font-size: 13px;");

        detailsLabel = new QLabel("Select an item to see details");
        detailsLabel->setStyleSheet("color: #888; font-size: 11px;");

        infoLayout->addWidget(titleLabel);
        infoLayout->addWidget(detailsLabel);
        infoLayout->addStretch();

        layout->addWidget(thumbnail);
        layout->addLayout(infoLayout);
        layout->addStretch();

        setStyleSheet("background-color: #252526; border-bottom: 2px solid #1a1a1a;");
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
  setStyleSheet("background-color: rgba(30, 30, 30, 240); border: 1px solid #444; border-radius: 6px;");

  impl_->layout = new QVBoxLayout(this);
  impl_->layout->setContentsMargins(10, 10, 10, 10);
  impl_->layout->setSpacing(6);

  impl_->thumbnailLabel = new QLabel(this);
  impl_->thumbnailLabel->setFixedSize(200, 112);
  impl_->thumbnailLabel->setScaledContents(true);
  impl_->thumbnailLabel->setStyleSheet("background-color: #000; border-radius: 4px;");
  impl_->layout->addWidget(impl_->thumbnailLabel, 0, Qt::AlignCenter);

  for (int i = 0; i < 3; ++i) {
    QLabel* l = new QLabel(this);
    l->setStyleSheet("color: #ccc; font-size: 11px; font-family: 'Segoe UI';");
    impl_->infoLabels.append(l);
    impl_->layout->addWidget(l);
  }
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

// --- Project View (Tree) ---
class ArtifactProjectView::Impl {
public:
    QTimer* hoverTimer = nullptr;
    QModelIndex hoverIndex;
    HoverThumbnailPopupWidget* hoverPopup = nullptr;
    QPoint hoverStartPos;
    QString lastContextCommandId;
    QString lastContextCommandLabel;
    QString lastNewCommandId;
    QString lastNewCommandLabel;
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

ArtifactProjectView::ArtifactProjectView(QWidget* parent) : QTreeView(parent), impl_(new Impl()) {
    setMouseTracking(true);
    if(viewport()) viewport()->setMouseTracking(true);
    setFrameShape(QFrame::NoFrame);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setRootIsDecorated(true);
    setIndentation(15);
    setAlternatingRowColors(true);
    setAnimated(true);

    header()->setStyleSheet(R"(
        QHeaderView::section {
            background-color: #2D2D30;
            color: #888;
            padding: 4px 10px;
            border: none;
            border-right: 1px solid #1a1a1a;
            font-size: 10px;
            font-weight: bold;
            text-transform: uppercase;
        }
    )");

    setStyleSheet(R"(
        QTreeView {
            background-color: #1E1E1E;
            color: #CCC;
            alternate-background-color: #252526;
            selection-background-color: #094771;
            selection-color: white;
            outline: none;
            border: none;
        }
        QTreeView::item {
            padding: 5px;
            border-bottom: 1px solid #1a1a1a;
        }
        QTreeView::item:hover {
            background-color: #2D2D30;
        }

        /* --- Modern Slim ScrollBar Style (DCC Style) --- */
        QScrollBar:vertical {
            border: none;
            background: #1e1e1e;
            width: 6px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #3e3e42;
            min-height: 20px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: #4a4a4e;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }

        QScrollBar:horizontal {
            border: none;
            background: #1e1e1e;
            height: 6px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal {
            background: #3e3e42;
            min-width: 20px;
            border-radius: 3px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #4a4a4e;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: none;
        }
    )");
}

ArtifactProjectView::~ArtifactProjectView() { delete impl_; }

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
                 ArtifactLayerTimelinePanelWrapper* panel = new ArtifactLayerTimelinePanelWrapper(cid);
                 panel->setAttribute(Qt::WA_DeleteOnClose);
                 panel->show();
             }
        } else if (typeVar.toInt() == static_cast<int>(eProjectItemType::Footage)) {
            QVariant ptrVar = actualIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
            ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
            if (item && item->type() == eProjectItemType::Footage) {
                const QString path = static_cast<FootageItem*>(item)->filePath;
                if (!path.isEmpty()) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
                }
            }
        }
    }
}

void ArtifactProjectView::mouseDoubleClickEvent(QMouseEvent* event) {
    QModelIndex idx = indexAt(event->position().toPoint());
    if (idx.isValid()) handleItemDoubleClicked(idx);
    QTreeView::mouseDoubleClickEvent(event);
}

void ArtifactProjectView::mouseMoveEvent(QMouseEvent* event) {
    const QPoint mousePos = event->position().toPoint();
    QModelIndex idx = indexAt(mousePos);
    if (!impl_->hoverTimer) {
        impl_->hoverTimer = new QTimer(this);
        impl_->hoverTimer->setSingleShot(true);
        connect(impl_->hoverTimer, &QTimer::timeout, this, [this]() {
            const QPoint localPos = viewport()->mapFromGlobal(QCursor::pos());
            if (!viewport()->rect().contains(localPos)) return;

            const QModelIndex currentIndex = indexAt(localPos);
            if (!currentIndex.isValid() || currentIndex != impl_->hoverIndex) return;

            QModelIndex sourceIdx = currentIndex;
            if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(currentIndex.model())) {
                sourceIdx = proxy->mapToSource(currentIndex).siblingAtColumn(0);
            }
            const QVariant typeVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
            if (!typeVar.isValid()) return;
            const auto type = static_cast<eProjectItemType>(typeVar.toInt());
            if (type != eProjectItemType::Footage && type != eProjectItemType::Composition) return;

            if (!impl_->hoverPopup) impl_->hoverPopup = new HoverThumbnailPopupWidget();
            const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
            ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
            const QPixmap preview = projectItemPreviewPixmap(item, QSize(200, 112));
            impl_->hoverPopup->setThumbnail(preview);
            impl_->hoverPopup->setLabels(projectItemMetadataLines(sourceIdx, item));
            const QRect itemRect = visualRect(currentIndex);
            QPoint popupPos = viewport()->mapToGlobal(itemRect.topRight() + QPoint(14, 6));
            impl_->hoverPopup->showAt(popupPos);
        });
    }
    if (event->buttons() != Qt::NoButton) {
        if (impl_->hoverPopup) impl_->hoverPopup->hide();
        impl_->hoverTimer->stop();
        impl_->hoverIndex = QModelIndex();
        QTreeView::mouseMoveEvent(event);
        return;
    }
    if (idx != impl_->hoverIndex) {
        if (impl_->hoverPopup) impl_->hoverPopup->hide();
        impl_->hoverIndex = idx;
        impl_->hoverStartPos = mousePos;
        impl_->hoverTimer->stop();
        if (idx.isValid()) impl_->hoverTimer->start(1100);
    } else if (idx.isValid() && (mousePos - impl_->hoverStartPos).manhattanLength() > 6) {
        if (impl_->hoverPopup) impl_->hoverPopup->hide();
        impl_->hoverTimer->stop();
        impl_->hoverStartPos = mousePos;
        impl_->hoverTimer->start(1100);
    }
    QTreeView::mouseMoveEvent(event);
}

void ArtifactProjectView::leaveEvent(QEvent* event) {
    if (impl_) {
        if (impl_->hoverTimer) impl_->hoverTimer->stop();
        if (impl_->hoverPopup) impl_->hoverPopup->hide();
        impl_->hoverIndex = QModelIndex();
    }
    QTreeView::leaveEvent(event);
}

void ArtifactProjectView::contextMenuEvent(QContextMenuEvent* event) {
    QModelIndex idx = indexAt(event->pos());
    QMenu menu(this);
    auto svc = ArtifactProjectService::instance();
    QHash<QString, std::function<void()>> availableContextCommands;
    QHash<QString, QString> availableContextLabels;
    QHash<QString, std::function<void()>> availableNewCommands;
    QHash<QString, QString> availableNewLabels;

    auto addTrackedAction = [this, &menu, &availableContextCommands, &availableContextLabels](const QString& id, const QString& label, std::function<void()> run) {
        availableContextCommands.insert(id, run);
        availableContextLabels.insert(id, label);
        menu.addAction(label, [this, id, label, run = std::move(run)]() mutable {
            impl_->lastContextCommandId = id;
            impl_->lastContextCommandLabel = label;
            run();
        });
    };

    auto addTrackedNewAction = [this, &availableNewCommands, &availableNewLabels](QMenu* targetMenu, const QString& id, const QString& label, std::function<void()> run) {
        availableNewCommands.insert(id, run);
        availableNewLabels.insert(id, label);
        targetMenu->addAction(label, [this, id, label, run = std::move(run)]() mutable {
            impl_->lastNewCommandId = id;
            impl_->lastNewCommandLabel = label;
            run();
        });
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
        });
        addTrackedAction(QStringLiteral("copy_name"), QStringLiteral("Copy Name"), [sourceIdx]() {
            QApplication::clipboard()->setText(sourceIdx.data(Qt::DisplayRole).toString());
        });
        
        if (type == eProjectItemType::Composition) {
            addTrackedAction(QStringLiteral("set_active_composition"), QStringLiteral("Set as Active Composition"), [sourceIdx]() {
                QVariant idVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
                if (idVar.isValid()) {
                    ArtifactProjectService::instance()->changeCurrentComposition(CompositionID(idVar.toString()));
                }
            });

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
                fpsSpin->setValue(std::max(1.0, composition->frameRate().framerate()));
                fpsLayout->addWidget(new QLabel(QStringLiteral("Frame Rate"), dialog));
                fpsLayout->addWidget(fpsSpin);
                layout->addLayout(fpsLayout);

                const FrameRange currentRange = composition->frameRange().normalized();
                auto* rangeLayout = new QHBoxLayout();
                auto* startSpin = new QSpinBox(dialog);
                startSpin->setRange(-1000000, 1000000);
                startSpin->setValue(static_cast<int>(currentRange.start().value()));
                auto* endSpin = new QSpinBox(dialog);
                endSpin->setRange(-1000000, 1000000);
                endSpin->setValue(static_cast<int>(currentRange.end().value()));
                rangeLayout->addWidget(new QLabel(QStringLiteral("Start"), dialog));
                rangeLayout->addWidget(startSpin);
                rangeLayout->addWidget(new QLabel(QStringLiteral("End"), dialog));
                rangeLayout->addWidget(endSpin);
                layout->addLayout(rangeLayout);

                auto* infoLabel = new QLabel(
                    QStringLiteral("ID: %1").arg(compositionId.toString()), dialog);
                infoLabel->setStyleSheet(QStringLiteral("color: #888;"));
                layout->addWidget(infoLabel);

                auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
                layout->addWidget(buttons);

                QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
                QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, svc, compositionId, composition, nameEdit, widthSpin, heightSpin, fpsSpin, startSpin, endSpin]() {
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
            });
            
            addTrackedAction(QStringLiteral("interpret_footage"), QStringLiteral("Interpret Footage..."), []() {
                // Placeholder for footage settings
            });
        }

        if (type == eProjectItemType::Footage) {
            addTrackedAction(QStringLiteral("reveal_in_explorer"), QStringLiteral("Reveal in Explorer"), [item]() {
                if (item && item->type() == eProjectItemType::Footage) {
                    QString path = static_cast<FootageItem*>(item)->filePath;
                    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
                }
            });
            addTrackedAction(QStringLiteral("copy_file_path"), QStringLiteral("Copy File Path"), [item]() {
                if (item && item->type() == eProjectItemType::Footage) {
                    QApplication::clipboard()->setText(static_cast<FootageItem*>(item)->filePath);
                }
            });
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
            });
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
        });

        QMenu* moveToFolderMenu = menu.addMenu(QStringLiteral("Move to Folder"));
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
            emptyAction->setEnabled(false);
        }

        addTrackedAction(QStringLiteral("delete"), QStringLiteral("Delete"), [this, item, svc]() {
            if (!svc || !item) {
                return;
            }
            const QString message = svc->projectItemRemovalConfirmationMessage(item);
            const auto answer = QMessageBox::question(
                this,
                QStringLiteral("項目削除"),
                message,
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return;
            }
            if (!svc->removeProjectItem(item)) {
                QMessageBox::warning(this, QStringLiteral("削除失敗"),
                    QStringLiteral("項目の削除に失敗しました。"));
            }
        });

        menu.addSeparator();
        addTrackedAction(QStringLiteral("expand_children"), QStringLiteral("Expand Children"), [this, idx]() {
            setExpanded(idx, true);
        });
        addTrackedAction(QStringLiteral("collapse_children"), QStringLiteral("Collapse Children"), [this, idx]() {
            setExpanded(idx, false);
        });
        
        menu.addSeparator();
    }

    // "New" menu group
    auto newMenu = menu.addMenu("New");
    addTrackedNewAction(newMenu, QStringLiteral("new_composition"), QStringLiteral("Composition..."), [svc]() {
        if (svc) {
            svc->createComposition(UniString("New Comp"));
        }
    });
    addTrackedNewAction(newMenu, QStringLiteral("new_solid"), QStringLiteral("Solid..."), []() {
        // Placeholder for solid creation dialog
    });
    addTrackedNewAction(newMenu, QStringLiteral("new_folder"), QStringLiteral("Folder"), [svc]() {
        if (svc && svc->getCurrentProjectSharedPtr()) {
            svc->getCurrentProjectSharedPtr()->createFolder("New Folder");
        }
    });

    menu.addSeparator();
    addTrackedAction(QStringLiteral("expand_all"), QStringLiteral("Expand All"), [this]() { expandAll(); });
    addTrackedAction(QStringLiteral("collapse_all"), QStringLiteral("Collapse All"), [this]() { collapseAll(); });
    addTrackedAction(QStringLiteral("refresh_view"), QStringLiteral("Refresh View"), [this]() { viewport()->update(); });
    addTrackedAction(QStringLiteral("show_dependency_graph"), QStringLiteral("Show Dependency Graph..."), [this, svc]() {
        Impl::showDependencyGraphDialog(this, svc);
    });
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
    });
    menu.addSeparator();
    addTrackedAction(QStringLiteral("import_file"), QStringLiteral("Import File..."), [this, svc]() {
        Q_UNUSED(svc);
        QStringList paths = QFileDialog::getOpenFileNames(this, "Import Files", "", "All Files (*.*)");
        for (const auto& p : paths) {
             impl_->handleFileDrop(p);
        }
    });

    if (!impl_->lastContextCommandId.isEmpty() && availableContextCommands.contains(impl_->lastContextCommandId)) {
        QAction* firstAction = menu.actions().isEmpty() ? nullptr : menu.actions().first();
        QAction* separator = firstAction ? menu.insertSeparator(firstAction) : menu.addSeparator();
        const QString repeatLabel = QStringLiteral("Repeat Last Command: %1").arg(
            availableContextLabels.value(impl_->lastContextCommandId, impl_->lastContextCommandLabel));
        QAction* repeatAction = new QAction(repeatLabel, &menu);
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

    menu.setStyleSheet(R"(
        QMenu { background-color: #2D2D30; color: #CCC; border: 1px solid #1a1a1a; padding: 4px; }
        QMenu::item { padding: 4px 20px 4px 20px; border-radius: 2px; }
        QMenu::item:selected { background-color: #094771; color: white; }
        QMenu::separator { height: 1px; background: #3e3e42; margin: 4px 10px; }
    )");

    menu.exec(event->globalPos());
}

void ArtifactProjectView::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
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
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else event->ignore();
}

void ArtifactProjectView::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else event->ignore();
}

void ArtifactProjectView::mousePressEvent(QMouseEvent* event) {
    if (impl_) {
        if (impl_->hoverTimer) impl_->hoverTimer->stop();
        if (impl_->hoverPopup) impl_->hoverPopup->hide();
    }
    if (event->button() == Qt::LeftButton) {
         QModelIndex idx = indexAt(event->position().toPoint());
         if (idx.isValid()) {
             itemSelected(idx);

             QModelIndex sourceIdx = idx;
             if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(idx.model())) {
                 sourceIdx = proxy->mapToSource(idx).siblingAtColumn(0);
             }
             QVariant typeVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
             if (typeVar.isValid() && typeVar.toInt() == static_cast<int>(eProjectItemType::Composition)) {
                 QVariant idVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
                 if (idVar.isValid()) {
                     CompositionID cid(idVar.toString());
                     ArtifactProjectService::instance()->changeCurrentComposition(cid);
                 }
             }
         }
    }
    QTreeView::mousePressEvent(event);
}

void ArtifactProjectView::mouseReleaseEvent(QMouseEvent* event) {
    if (impl_ && impl_->hoverTimer) impl_->hoverTimer->stop();
    QTreeView::mouseReleaseEvent(event);
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
    bool thumbnailEnabled = true;
    QSet<QString> unusedAssetPaths_;
    struct ProxyJob {
        QString inputPath;
        QString outputPath;
    };
    std::deque<ProxyJob> proxyJobs_;
    QTimer* proxyQueueTimer_ = nullptr;
    QMetaObject::Connection currentRowChangedConnection_;

    QModelIndex currentSourceIndexFromSelection() const {
        if (!projectView_ || !projectView_->selectionModel()) {
            return {};
        }
        const auto selectedRows = projectView_->selectionModel()->selectedRows(0);
        if (selectedRows.isEmpty()) {
            return {};
        }
        QModelIndex current = selectedRows.first();
        if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(current.model())) {
            current = proxy->mapToSource(current).siblingAtColumn(0);
        }
        return current;
    }

    ProjectItem* currentSelectedItem() const {
        const QModelIndex sourceIdx = currentSourceIndexFromSelection();
        const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        return ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
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
        projectView_->expand(proxyIndex.parent());
        projectView_->selectionModel()->setCurrentIndex(
            proxyIndex,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Current);
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
            projectView_->header()->setSortIndicatorShown(false);
            projectView_->header()->setSectionsClickable(false);
            projectView_->header()->setStretchLastSection(true);
            projectView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
            for (int col = 1; col < projectView_->model()->columnCount(); ++col) {
                projectView_->header()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
            }
            projectView_->expandToDepth(1);
            if (projectView_->selectionModel()) {
                QObject::disconnect(currentRowChangedConnection_);
                currentRowChangedConnection_ =
                    QObject::connect(projectView_->selectionModel(), &QItemSelectionModel::currentRowChanged, projectView_,
                    [this](const QModelIndex& current, const QModelIndex&) {
                        if (!current.isValid() || !proxyModel_ || !infoPanel_) {
                            return;
                        }
                        infoPanel_->updateInfo(proxyModel_->mapToSource(current));
                    });
            }
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
    }
};

ArtifactProjectManagerWidget::ArtifactProjectManagerWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    impl_->infoPanel_ = new ProjectInfoPanel(this);
    mainLayout->addWidget(impl_->infoPanel_);

    impl_->projectNameLabel = new QLabel("PROJECT");
    impl_->projectNameLabel->setStyleSheet("color: #888; font-size: 10px; padding: 5px 10px; font-weight: bold;");
    mainLayout->addWidget(impl_->projectNameLabel);

    impl_->searchBar = new QLineEdit();
    impl_->searchBar->setPlaceholderText("Search (type:footage tag:png regex:shot_.* unused:true)...");
    impl_->searchBar->setStyleSheet(R"(
        QLineEdit {
            background-color: #1E1E1E;
            color: #AAA;
            border: 1px solid #333;
            border-radius: 4px;
            padding: 5px 10px;
            margin: 0 10px 8px 10px;
            font-size: 11px;
        }
        QLineEdit:focus { border: 1px solid #007ACC; color: white; }
    )");
    impl_->searchBar->setClearButtonEnabled(true);
    mainLayout->addWidget(impl_->searchBar);

    auto* filterBar = new QHBoxLayout();
    filterBar->setContentsMargins(10, 0, 10, 6);
    filterBar->setSpacing(8);
    impl_->typeFilterBox = new QComboBox(this);
    impl_->typeFilterBox->addItems(QStringList() << "All" << "Composition" << "Footage" << "Folder" << "Solid");
    impl_->unusedOnlyCheck = new QCheckBox("Unused only", this);
    impl_->proxyQueueProgress = new QProgressBar(this);
    impl_->proxyQueueProgress->setVisible(false);
    impl_->proxyQueueProgress->setTextVisible(true);
    impl_->proxyQueueProgress->setFormat("Proxy queue %v/%m");
    filterBar->addWidget(impl_->typeFilterBox);
    filterBar->addWidget(impl_->unusedOnlyCheck);
    filterBar->addStretch();
    filterBar->addWidget(impl_->proxyQueueProgress, 1);
    mainLayout->addLayout(filterBar);

    impl_->projectView_ = new ArtifactProjectView();
    mainLayout->addWidget(impl_->projectView_);

    impl_->toolBox = new ArtifactProjectManagerToolBox();
    mainLayout->addWidget(impl_->toolBox);

    connect(impl_->searchBar, &QLineEdit::textChanged, [this](const QString& t) { impl_->handleSearch(t); });
    connect(impl_->typeFilterBox, &QComboBox::currentTextChanged, [this](const QString&) {
        impl_->handleSearch(impl_->searchBar ? impl_->searchBar->text() : QString());
    });
    connect(impl_->unusedOnlyCheck, &QCheckBox::toggled, [this](bool) {
        impl_->handleSearch(impl_->searchBar ? impl_->searchBar->text() : QString());
    });
    connect(impl_->projectView_, &ArtifactProjectView::itemSelected, [this](const QModelIndex& idx) {
        if (impl_->proxyModel_) impl_->infoPanel_->updateInfo(impl_->proxyModel_->mapToSource(idx));
    });
    connect(impl_->toolBox, &ArtifactProjectManagerToolBox::newCompositionRequested, [this]() {
         if (auto* svc = ArtifactProjectService::instance()) {
             svc->createComposition(UniString("Composition"));
         }
    });
    connect(impl_->toolBox, &ArtifactProjectManagerToolBox::newFolderRequested, [this]() {
         auto* svc = ArtifactProjectService::instance();
         if (!svc) return;
         auto project = svc->getCurrentProjectSharedPtr();
         if (project) project->createFolder("New Folder");
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
         if (!impl_->projectView_ || !impl_->projectView_->selectionModel()) return;
         auto selection = impl_->projectView_->selectionModel()->selectedIndexes();
         if (!selection.isEmpty()) {
             QModelIndex idx = selection.first();
             QModelIndex sourceIdx = idx;
             if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(idx.model())) {
                 sourceIdx = proxy->mapToSource(idx).siblingAtColumn(0);
             }
             QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
             ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
             
             auto* svc = ArtifactProjectService::instance();
             if (!svc) return;
             auto project = svc->getCurrentProjectSharedPtr();
             if (project && item) project->removeItem(item);
         }
    });

    auto svc = ArtifactProjectService::instance();
    connect(svc, &ArtifactProjectService::projectChanged, this, [this]() { updateRequested(); });
    connect(svc, &ArtifactProjectService::projectCreated, this, [this]() { updateRequested(); });

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

void ArtifactProjectManagerWidget::updateRequested() {
    impl_->update();
    setEnabled(true);
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
QSize ArtifactProjectManagerWidget::sizeHint() const { return QSize(300, 600); }

// --- ToolBox Implementation ---
ArtifactProjectManagerToolBox::ArtifactProjectManagerToolBox(QWidget* parent) : QWidget(parent) {
    setFixedHeight(32);
    setStyleSheet("background-color: #2D2D30; border-top: 1px solid #1a1a1a;");
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(10);

    auto createBtn = [](const QString& label, const QString& tip) {
        auto b = new QPushButton(label);
        b->setFixedSize(24, 24);
        b->setToolTip(tip);
        b->setStyleSheet(R"(
            QPushButton { background: transparent; border: none; font-size: 11px; font-weight: bold; color: #aaa; border-radius: 3px; }
            QPushButton:hover { background: #444; color: white; }
        )");
        return b;
    };

    auto btnNew = createBtn("N", "New Composition");
    auto btnFolder = createBtn("F", "New Folder");
    auto btnProxy = createBtn("P", "Generate Proxies");
    auto btnDel = createBtn("D", "Delete");

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
void ArtifactProjectManagerToolBox::resizeEvent(QResizeEvent*) {}

}
