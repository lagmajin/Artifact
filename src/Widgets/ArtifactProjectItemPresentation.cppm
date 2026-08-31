module;

#include <QBoxLayout>
#include <QByteArray>
#include <QColor>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QModelIndex>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QRectF>
#include <QSize>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

export module Artifact.Widgets.ProjectItemPresentation;

import Widgets.Utils.CSS;
import Artifact.Widgets.SoftwareRenderInspectors;
import Artifact.Widgets.ProjectResponsiveLayout;
import Artifact.Project.Items;
import Artifact.Project.Roles;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Video;
import Artifact.Layer.Composition;
import Asset.Manager;
import Artifact.Event.Types;
import Event.Bus;

export namespace Artifact {

namespace detail {

enum class AssetKind { Image, Video, Audio, Font, Other };

AssetKind assetKindFromPath(const QString& path) {
    const QString lower = path.toLower();
    if (lower.endsWith(".png") || lower.endsWith(".jpg") ||
        lower.endsWith(".jpeg") || lower.endsWith(".bmp") ||
        lower.endsWith(".gif") || lower.endsWith(".tga") ||
        lower.endsWith(".tiff") || lower.endsWith(".exr")) {
        return AssetKind::Image;
    }
    if (lower.endsWith(".mp4") || lower.endsWith(".mov") ||
        lower.endsWith(".avi") || lower.endsWith(".mkv") ||
        lower.endsWith(".webm") || lower.endsWith(".flv")) {
        return AssetKind::Video;
    }
    if (lower.endsWith(".mp3") || lower.endsWith(".wav") ||
        lower.endsWith(".ogg") || lower.endsWith(".flac") ||
        lower.endsWith(".aac") || lower.endsWith(".m4a")) {
        return AssetKind::Audio;
    }
    if (lower.endsWith(".ttf") || lower.endsWith(".otf") ||
        lower.endsWith(".ttc") || lower.endsWith(".woff") ||
        lower.endsWith(".woff2")) {
        return AssetKind::Font;
    }
    return AssetKind::Other;
}

bool isImportableAssetFile(const QString& path) {
    return assetKindFromPath(path) != AssetKind::Other;
}

QString projectItemFootageKindLabel(const QString& path) {
    switch (assetKindFromPath(path)) {
    case AssetKind::Image:
        return QStringLiteral("Image");
    case AssetKind::Video:
        return QStringLiteral("Video");
    case AssetKind::Audio:
        return QStringLiteral("Audio");
    case AssetKind::Font:
        return QStringLiteral("Font");
    default:
        return QStringLiteral("Footage");
    }
}

int projectItemUsageCount(ProjectItem* item)
{
    if (!item) {
        return 0;
    }

    auto* svc = ArtifactProjectService::instance();
    if (!svc) {
        return 0;
    }

    auto project = svc->getCurrentProjectSharedPtr();
    if (!project) {
        return 0;
    }

    const auto normalizePath = [](const QString& path) {
        return QDir::cleanPath(path.trimmed());
    };

    const auto matchesFootagePath = [&](const FootageItem* footage, const QString& candidatePath) {
        if (!footage) {
            return false;
        }
        const QString normalizedCandidate = normalizePath(candidatePath);
        if (normalizedCandidate.isEmpty()) {
            return false;
        }
        if (normalizedCandidate == normalizePath(footage->filePath)) {
            return true;
        }
        for (const QString& sequencePath : footage->sequencePaths) {
            if (normalizedCandidate == normalizePath(sequencePath)) {
                return true;
            }
        }
        return false;
    };

    int usageCount = 0;
    std::function<void(ProjectItem*)> walkItems;
    walkItems = [&](ProjectItem* current) {
        if (!current) {
            return;
        }
        if (current->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(current);
            const auto found = project->findComposition(compItem->compositionId);
            auto comp = found.ptr.lock();
            if (comp) {
                for (const auto& layer : comp->allLayerRef()) {
                    if (!layer) {
                        continue;
                    }
                    if (item->type() == eProjectItemType::Composition) {
                        auto* compositionLayer = dynamic_cast<ArtifactCompositionLayer*>(layer.get());
                        if (compositionLayer && compositionLayer->sourceCompositionId() ==
                                                    static_cast<CompositionItem*>(item)->compositionId) {
                            ++usageCount;
                        }
                    } else if (item->type() == eProjectItemType::Footage) {
                        auto* footageLayer = dynamic_cast<ArtifactVideoLayer*>(layer.get());
                        if (footageLayer && matchesFootagePath(static_cast<FootageItem*>(item),
                                                               footageLayer->sourceFile())) {
                            ++usageCount;
                        }
                    }
                }
            }
        }
        for (auto* child : current->children) {
            walkItems(child);
        }
    };

    for (auto* root : project->projectItems()) {
        walkItems(root);
    }

    return usageCount;
}

int projectItemSourceUseCount(ProjectItem* item)
{
    if (!item || item->type() != eProjectItemType::Footage) {
        return 0;
    }

    const auto* footage = static_cast<const FootageItem*>(item);
    const QString path = footage->filePath.trimmed();
    if (path.isEmpty()) {
        return 0;
    }

    auto& assetManager = ArtifactCore::AssetManager::instance();
    QUuid sourceId = assetManager.sourceId(path);
    if (sourceId.isNull()) {
        const QString absolutePath = QFileInfo(path).absoluteFilePath();
        if (absolutePath != path) {
            sourceId = assetManager.sourceId(absolutePath);
        }
    }
    return sourceId.isNull() ? 0 : assetManager.useCount(sourceId);

QString projectRenderInputRoleLabel(ProjectRenderInputRole role);

QString projectItemTileBadgeText(ProjectItem* item)
{
    if (!item) {
        return QStringLiteral("Item");
    }

    switch (item->type()) {
    case eProjectItemType::Composition:
        return QStringLiteral("Composition");
    case eProjectItemType::Folder:
        return QStringLiteral("Folder");
    case eProjectItemType::Solid:
        return QStringLiteral("Solid");
    case eProjectItemType::Footage: {
        const auto* footage = static_cast<FootageItem*>(item);
        if (footage && footage->assetUsage == ProjectAssetUsage::RenderInput) {
            return projectRenderInputRoleLabel(footage->renderInputRole);
        }
        const QFileInfo info(footage ? footage->filePath : QString());
        bool missing = !info.exists();
        if (footage && footage->isSequence) {
            for (const QString& sequencePath : footage->sequencePaths) {
                if (!QFileInfo(sequencePath).exists()) {
                    missing = true;
                    break;
                }
            }
        }
        if (missing) {
            return QStringLiteral("Missing");
        }
        return projectItemFootageKindLabel(info.filePath());
    }
    default:
        return QStringLiteral("Item");
    }
}

QString projectItemTypeLabel(eProjectItemType type)
{
    switch (type) {
    case eProjectItemType::Folder:
        return QStringLiteral("Folder");
    case eProjectItemType::Composition:
        return QStringLiteral("Composition");
    case eProjectItemType::Footage:
        return QStringLiteral("Footage");
    case eProjectItemType::Solid:
        return QStringLiteral("Solid");
    default:
        return QStringLiteral("Item");
    }
}

QString projectRenderInputRoleLabel(const ProjectRenderInputRole role)
{
    switch (role) {
    case ProjectRenderInputRole::AlphaMatte: return QStringLiteral("Alpha Matte");
    case ProjectRenderInputRole::LumaMatte: return QStringLiteral("Luma Matte");
    case ProjectRenderInputRole::DisplacementMap: return QStringLiteral("Displacement");
    case ProjectRenderInputRole::DepthMap: return QStringLiteral("Depth");
    case ProjectRenderInputRole::NormalMap: return QStringLiteral("Normal");
    case ProjectRenderInputRole::Texture: return QStringLiteral("Texture");
    default: return QStringLiteral("Render Input");
    }
}

QString projectRenderInputRoleKey(const ProjectRenderInputRole role)
{
    switch (role) {
    case ProjectRenderInputRole::AlphaMatte: return QStringLiteral("alphaMatte");
    case ProjectRenderInputRole::LumaMatte: return QStringLiteral("lumaMatte");
    case ProjectRenderInputRole::DisplacementMap: return QStringLiteral("displacementMap");
    case ProjectRenderInputRole::DepthMap: return QStringLiteral("depthMap");
    case ProjectRenderInputRole::NormalMap: return QStringLiteral("normalMap");
    case ProjectRenderInputRole::Texture: return QStringLiteral("texture");
    default: return QStringLiteral("generic");
    }
}

bool isDescendantOf(const ProjectItem* item, const ProjectItem* ancestor)
{
    if (!item || !ancestor) {
        return false;
    }
    for (const ProjectItem* current = item->parent; current; current = current->parent) {
        if (current == ancestor) {
            return true;
        }
    }
    return false;
}

QString folderDisplayPath(FolderItem* folder)
{
    if (!folder) {
        return {};
    }
    QStringList parts;
    for (ProjectItem* current = folder; current; current = current->parent) {
        if (!current->name.toQString().isEmpty()) {
            parts.prepend(current->name.toQString());
        }
    }
    return parts.join(QStringLiteral("/"));
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

void collectImportablePaths(const QString& input, QStringList& out)
{
    auto addPath = [&out](const QString& path) {
        const QFileInfo info(path.trimmed());
        if (!info.exists()) {
            return;
        }
        if (info.isDir()) {
            QDirIterator it(info.absoluteFilePath(), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                out.append(it.next());
            }
            return;
        }
        if (isImportableAssetFile(info.absoluteFilePath())) {
            out.append(info.absoluteFilePath());
        }
    };

    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (trimmed.contains('\n') || trimmed.contains('\r') || trimmed.contains(';')) {
        const QStringList parts = trimmed.split(QRegularExpression(QStringLiteral("[\\r\\n;]+")),
                                                Qt::SkipEmptyParts);
        for (const QString& part : parts) {
            addPath(part);
        }
        return;
    }
    addPath(trimmed);
}

struct ProxyMeta {
    ProxyQuality quality = ProxyQuality::Half;
    bool enabled = true;
    QDateTime sourceLastModified;
    QString qualityLabel;
};

QHash<QString, ProxyMeta>& proxyMetadata() {
    static QHash<QString, ProxyMeta> meta;
    return meta;
}

QString proxyFilePathForFootage(const QString& sourceFilePath)
{
    const QFileInfo src(sourceFilePath);
    if (src.filePath().isEmpty() || src.completeBaseName().isEmpty()) {
        return {};
    }
    const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataRoot.isEmpty()) {
        return {};
    }
    QDir proxyDir(appDataRoot);
    const QString proxyRoot = proxyDir.filePath(QStringLiteral("ProxyCache"));
    const QByteArray digest = QCryptographicHash::hash(src.absoluteFilePath().toUtf8(), QCryptographicHash::Sha1).toHex();
    const QString proxyName = QStringLiteral("%1_%2.proxy.jpg")
                                  .arg(src.completeBaseName(),
                                       QString::fromLatin1(digest.left(10)));
    return QDir(proxyRoot).filePath(proxyName);
}

QString projectItemStatusChipText(ProjectItem* item)
{
    if (!item) {
        return QStringLiteral("Unknown");
    }

    switch (item->type()) {
    case eProjectItemType::Footage: {
        const auto* footage = static_cast<FootageItem*>(item);
        const QFileInfo info(footage ? footage->filePath : QString());
        if (!info.exists()) {
            return QStringLiteral("Missing");
        }
        const QString proxyPath = proxyFilePathForFootage(footage->filePath);
        if (!proxyPath.isEmpty() && QFileInfo(proxyPath).exists()) {
            const auto it = proxyMetadata().constFind(footage->filePath);
            if (it != proxyMetadata().constEnd() && it->sourceLastModified.isValid()) {
                const QFileInfo src(footage->filePath);
                if (src.exists() && src.lastModified() > it->sourceLastModified) {
                    return QStringLiteral("Proxy Stale");
                }
            }
            return QStringLiteral("Proxy Ready");
        }
        return QStringLiteral("Ready");
    }
    case eProjectItemType::Composition:
        return QStringLiteral("Live");
    case eProjectItemType::Folder:
        return QStringLiteral("Container");
    case eProjectItemType::Solid:
        return QStringLiteral("Static");
    default:
        return QStringLiteral("Item");
    }
}

QColor projectItemStatusChipColor(const QString& statusText)
{
    if (statusText.contains(QStringLiteral("Missing"), Qt::CaseInsensitive)) {
        return QColor(215, 84, 84);
    }
    if (statusText.contains(QStringLiteral("Stale"), Qt::CaseInsensitive)) {
        return QColor(218, 166, 72);
    }
    if (statusText.contains(QStringLiteral("Proxy Ready"), Qt::CaseInsensitive)) {
        return QColor(94, 178, 126);
    }
    if (statusText.contains(QStringLiteral("Ready"), Qt::CaseInsensitive) ||
        statusText.contains(QStringLiteral("Live"), Qt::CaseInsensitive)) {
        return QColor(88, 148, 205);
    }
    if (statusText.contains(QStringLiteral("Container"), Qt::CaseInsensitive)) {
        return QColor(188, 151, 73);
    }
    return QColor(150, 160, 174);
}

void syncProxyPathToProject(const QString& sourceFilePath, const QString& proxyPath,
                            bool enabled, bool globalEnabled)
{
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }
    auto project = service->getCurrentProjectSharedPtr();
    if (!project) {
        return;
    }
    const QString targetPath = QFileInfo(sourceFilePath).absoluteFilePath();
    if (targetPath.isEmpty() || proxyPath.isEmpty()) {
        return;
    }
    const bool shouldUseProxy = globalEnabled && enabled;
    const auto roots = project->projectItems();
    std::function<void(ProjectItem*)> visit = [&](ProjectItem* item) {
        if (!item) {
            return;
        }
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto found = service->findComposition(compItem->compositionId);
            auto comp = found.ptr.lock();
            if (!found.success || !comp) {
                return;
            }
            const auto layers = comp->allLayer();
            for (const auto& layer : layers) {
                auto videoLayer = layer ? ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(layer) : nullptr;
                if (!videoLayer) {
                    continue;
                }
                const QString layerSourcePath = videoLayer->sourcePath().trimmed();
                if (layerSourcePath.isEmpty() || QFileInfo(layerSourcePath).absoluteFilePath() != targetPath) {
                    continue;
                }
                if (shouldUseProxy) {
                    if (videoLayer->proxyPath() == proxyPath) continue;
                    videoLayer->setProxyPath(proxyPath);
                } else {
                    videoLayer->clearProxy();
                }
                videoLayer->changed();
                ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                    LayerChangedEvent{comp->id().toString(), videoLayer->id().toString(),
                                      LayerChangedEvent::ChangeType::Modified});
            }
        }
        for (auto* child : item->children) {
            visit(child);
        }
    };
    for (auto* root : roots) {
        visit(root);
    }
}

QPixmap projectItemPreviewPixmap(ProjectItem* item, const QSize& targetSize)
{
    if (!item) {
        return {};
    }

    const auto isImageFile = [](const QString& lowerPath) {
        return lowerPath.endsWith(".png") || lowerPath.endsWith(".jpg") ||
               lowerPath.endsWith(".jpeg") || lowerPath.endsWith(".bmp") ||
               lowerPath.endsWith(".gif") || lowerPath.endsWith(".tga") ||
               lowerPath.endsWith(".tiff") || lowerPath.endsWith(".webp") ||
               lowerPath.endsWith(".hdr") || lowerPath.endsWith(".exr") ||
               lowerPath.endsWith(".ico") || lowerPath.endsWith(".dds") ||
               lowerPath.endsWith(".ktx") || lowerPath.endsWith(".psd") ||
               lowerPath.endsWith(".psb");
    };
    const auto isVideoFile = [](const QString& lowerPath) {
        return lowerPath.endsWith(".mp4") || lowerPath.endsWith(".mov") ||
               lowerPath.endsWith(".avi") || lowerPath.endsWith(".mkv") ||
               lowerPath.endsWith(".webm");
    };

    if (item->type() == eProjectItemType::Footage) {
        const QString path = static_cast<FootageItem*>(item)->filePath;
        const QFileInfo info(path);
        if (!info.exists()) {
            return {};
        }

        QString lowerPath = path.toLower();
        if (isImageFile(lowerPath)) {
            QPixmap pix(path);
            if (pix.isNull()) {
                return {};
            }
            return pix.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        if (isVideoFile(lowerPath)) {
            QPixmap pix(targetSize);
            pix.fill(Qt::transparent);
            QPainter painter(&pix);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(QColor(90, 90, 90), 1.0));
            painter.setBrush(QColor(66, 148, 98));
            painter.drawRoundedRect(QRectF(1.0, 1.0, targetSize.width() - 2.0, targetSize.height() - 2.0), 3.0, 3.0);
            painter.setPen(Qt::white);
            painter.drawText(pix.rect(), Qt::AlignCenter, QStringLiteral("V"));
            return pix;
        }

        return {};
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
    if (!item) {
        return {};
    }
    Q_UNUSED(sourceIndex);
    QStringList lines;
    const auto isImageFile = [](const QString& lowerPath) {
        return lowerPath.endsWith(".png") || lowerPath.endsWith(".jpg") ||
               lowerPath.endsWith(".jpeg") || lowerPath.endsWith(".bmp") ||
               lowerPath.endsWith(".gif") || lowerPath.endsWith(".tga") ||
               lowerPath.endsWith(".tiff") || lowerPath.endsWith(".webp") ||
               lowerPath.endsWith(".hdr") || lowerPath.endsWith(".exr") ||
               lowerPath.endsWith(".ico") || lowerPath.endsWith(".dds") ||
               lowerPath.endsWith(".ktx") || lowerPath.endsWith(".psd") ||
               lowerPath.endsWith(".psb");
    };
    const auto isVideoFile = [](const QString& lowerPath) {
        return lowerPath.endsWith(".mp4") || lowerPath.endsWith(".mov") ||
               lowerPath.endsWith(".avi") || lowerPath.endsWith(".mkv") ||
               lowerPath.endsWith(".webm");
    };
    const auto isAudioFile = [](const QString& lowerPath) {
        return lowerPath.endsWith(".wav") || lowerPath.endsWith(".mp3") ||
               lowerPath.endsWith(".flac") || lowerPath.endsWith(".ogg") ||
               lowerPath.endsWith(".m4a") || lowerPath.endsWith(".aac");
    };
    const auto isFontFile = [](const QString& lowerPath) {
        return lowerPath.endsWith(".ttf") || lowerPath.endsWith(".otf") ||
               lowerPath.endsWith(".ttc") || lowerPath.endsWith(".woff") ||
               lowerPath.endsWith(".woff2");
    };

    // Composition metadata
    if (item->type() == eProjectItemType::Composition) {
        auto* composition = static_cast<CompositionItem*>(item);
        const QString itemName = composition->name.toQString().trimmed();
        lines << QStringLiteral("Type: Composition");
        lines << QStringLiteral("Name: %1").arg(itemName.isEmpty()
                                                    ? QStringLiteral("Composition")
                                                    : itemName);
        if (auto* svc = ArtifactProjectService::instance()) {
            const auto found = svc->findComposition(composition->compositionId);
            if (auto comp = found.ptr.lock()) {
                const QSize compSize = comp->settings().compositionSize();
                const auto frameRange = comp->frameRange().normalized();
                const auto workAreaRange = comp->workAreaRange().normalized();
                const ResponsiveLayoutSet responsiveLayout = comp->responsiveLayout();
                lines << QStringLiteral("Status: Ready");
                lines << QStringLiteral("Resolution: %1 x %2")
                             .arg(compSize.width())
                             .arg(compSize.height());
                lines << QStringLiteral("Layout: %1 • %2 variants")
                             .arg(responsiveLayoutActiveSummary(responsiveLayout))
                             .arg(responsiveLayout.variants.size());
                lines << QStringLiteral("Timing: %1 fps • %2 frames")
                             .arg(QString::number(comp->frameRate().framerate(), 'f', 2))
                             .arg(frameRange.duration());
                lines << QStringLiteral("Work Area: %1 frames • %2 layers")
                             .arg(workAreaRange.duration())
                            .arg(comp->allLayer().size());
                const QColor bgColor = QColor::fromRgbF(
                    comp->backgroundColor().r(),
                    comp->backgroundColor().g(),
                    comp->backgroundColor().b(),
                    comp->backgroundColor().a());
                lines << QStringLiteral("Background: %1")
                             .arg(bgColor.name(QColor::HexArgb).toUpper());
                lines << QStringLiteral("Composition ID: %1")
                             .arg(composition->compositionId.toString());
                lines << QStringLiteral("Used In: %1")
                             .arg(projectItemUsageCount(item));
            } else {
                lines << QStringLiteral("Status: Composition data unavailable");
            }
        }
    }

    // Footage metadata
    if (item->type() == eProjectItemType::Footage) {
        auto* footage = static_cast<FootageItem*>(item);
        const QString path = footage->filePath;
        QFileInfo info(path);
        const bool exists = info.exists();
        lines << QStringLiteral("Type: %1").arg(projectItemFootageKindLabel(path));
        if (footage->assetUsage == ProjectAssetUsage::RenderInput) {
            lines << QStringLiteral("Usage: Input Source • %1")
                         .arg(projectRenderInputRoleLabel(footage->renderInputRole));
        } else {
            lines << QStringLiteral("Usage: Production Source");
        }
        if (exists) {
            lines << QStringLiteral("Status: Ready");
            lines << QStringLiteral("File Size: %1 KB").arg(info.size() / 1024);
            lines << QStringLiteral("Modified: %1").arg(info.lastModified().toString("yyyy-MM-dd hh:mm"));
        } else {
            lines << QStringLiteral("Status: Missing");
        }
        lines << QStringLiteral("Used In: %1 layers • Source Uses: %2")
                     .arg(projectItemUsageCount(item))
                     .arg(projectItemSourceUseCount(item));

        QString lowerPath = path.toLower();
        if (exists) {
            if (isImageFile(lowerPath)) {
                QImageReader imageReader(path);
                const QSize imageSize = imageReader.size();
                if (imageSize.isValid()) {
                    lines << QStringLiteral("Resolution: %1 x %2").arg(imageSize.width()).arg(imageSize.height());
                    const QByteArray format = imageReader.format();
                    if (!format.isEmpty()) {
                        lines << QStringLiteral("Format: %1").arg(QString::fromLatin1(format).toUpper());
                    }
                }
            } else if (isVideoFile(lowerPath)) {
                lines << QStringLiteral("Kind: Video");
                lines << QStringLiteral("Preview: Generated thumbnail");
            } else if (isAudioFile(lowerPath)) {
                lines << QStringLiteral("Kind: Audio");
            } else if (isFontFile(lowerPath)) {
                lines << QStringLiteral("Kind: Font");
            }
        }
    }

    // Folder metadata
    if (item->type() == eProjectItemType::Folder) {
        int childCount = 0;
        if (!item->children.isEmpty()) {
            childCount = item->children.size();
        }
        lines << QStringLiteral("Type: Folder");
        lines << QStringLiteral("Contents: %1 items").arg(childCount);
    }

    // Solid metadata
    if (item->type() == eProjectItemType::Solid) {
        const auto* solid = static_cast<const SolidItem*>(item);
        if (solid) {
            lines << QStringLiteral("Type: Solid");
            lines << QStringLiteral("Color: %1")
                          .arg(solid->color.name(QColor::HexArgb).toUpper());
        }
    }

    if (item->type() == eProjectItemType::Composition) {
        auto* composition = static_cast<CompositionItem*>(item);
        if (composition) {
            if (auto* svc = ArtifactProjectService::instance()) {
                const auto found = svc->findComposition(composition->compositionId);
                if (auto comp = found.ptr.lock()) {
                    const QSize compSize = comp->settings().compositionSize();
                    const auto frameRange = comp->frameRange().normalized();
                    lines << QStringLiteral("Size: %1x%2")
                                  .arg(compSize.width())
                                  .arg(compSize.height());
                    lines << QStringLiteral("Timing: %1 fps • %2 frames")
                                  .arg(QString::number(comp->frameRate().framerate(), 'f', 2))
                                  .arg(frameRange.duration());
                }
            }
        }
    }

    return lines;
}

} // namespace detail

using namespace detail;

class ProjectInfoPanel : public QWidget {
public:
    QLabel* thumbnail;
    QLabel* titleLabel;
    QLabel* detailsLabel;
    QHash<QString, QPixmap> previewCache;

    ProjectInfoPanel(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("projectInfoPanel"));
        setAutoFillBackground(true);
        setFixedHeight(96);
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
        layout->setContentsMargins(8, 6, 10, 6);
        layout->setSpacing(10);

        thumbnail = new QLabel();
        thumbnail->setFixedSize(150, 84);
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
        infoLayout->setSpacing(1);
        infoLayout->setContentsMargins(0, 2, 0, 2);

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

        detailsLabel = new QLabel("Select an item to inspect details");
        detailsLabel->setWordWrap(false);
        detailsLabel->setMinimumHeight(52);
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
            detailsLabel->setText("Open a project or search to inspect details");
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

} // namespace Artifact
