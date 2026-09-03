module;

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QVariant>

#include <functional>

module Artifact.Widgets.ProjectFilterProxyModel;

import Artifact.Project;
import Artifact.Project.Items;
import Artifact.Project.Roles;
import Artifact.Layer.Search.Query;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Composition;
import Artifact.Layer.Video;

namespace Artifact {
namespace {

QString renderInputRoleLabel(const ProjectRenderInputRole role)
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

int projectItemUsageCount(ProjectItem* item)
{
    if (!item) {
        return 0;
    }

    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return 0;
    }
    auto project = service->getCurrentProjectSharedPtr();
    if (!project) {
        return 0;
    }

    const auto normalizePath = [](const QString& path) {
        return QDir::cleanPath(path.trimmed());
    };
    const auto matchesFootagePath = [&](const FootageItem* footage,
                                        const QString& candidatePath) {
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
            auto* compositionItem = static_cast<CompositionItem*>(current);
            const auto found = project->findComposition(compositionItem->compositionId);
            auto composition = found.ptr.lock();
            if (composition) {
                for (const auto& layer : composition->allLayerRef()) {
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
                        if (footageLayer && matchesFootagePath(
                                static_cast<FootageItem*>(item), footageLayer->sourceFile())) {
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

}

ProjectFilterProxyModel::ProjectFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void ProjectFilterProxyModel::setSearchQuery(const ArtifactLayerSearchQuery& query)
{
    query_ = query;
    invalidateFilter();
}

void ProjectFilterProxyModel::setUnusedAssetPaths(const QSet<QString>& unusedPaths)
{
    unusedAssetPaths_ = unusedPaths;
    invalidateFilter();
}

const QSet<QString>& ProjectFilterProxyModel::unusedAssetPaths() const
{
    return unusedAssetPaths_;
}

QVariant ProjectFilterProxyModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    const QModelIndex sourceIndex = mapToSource(index);
    const QVariant ptrVar = sourceModel()->data(
        sourceIndex.siblingAtColumn(0),
        Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
    auto* item = ptrVar.isValid()
        ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>())
        : nullptr;
    if (item && item->type() == eProjectItemType::Footage) {
        const auto* footage = static_cast<const FootageItem*>(item);
        const QString path = QDir::cleanPath(footage->filePath);
        const bool renderInput = footage->assetUsage == ProjectAssetUsage::RenderInput;
        const bool unused = !renderInput && unusedAssetPaths_.contains(path);
        bool missing = !QFileInfo(path).exists();
        if (footage->isSequence) {
            for (const QString& sequencePath : footage->sequencePaths) {
                if (!QFileInfo(sequencePath).exists()) {
                    missing = true;
                    break;
                }
            }
        }
        if (role == Qt::DisplayRole && index.column() == 0 &&
            (unused || missing || renderInput)) {
            QString text = QSortFilterProxyModel::data(index, role).toString();
            if (missing && !text.startsWith(QStringLiteral("[Missing] "))) {
                text = QStringLiteral("[Missing] %1").arg(text);
            }
            if (unused && !text.contains(QStringLiteral("[Unused] "))) {
                text = QStringLiteral("[Unused] %1").arg(text);
            }
            if (renderInput && !text.contains(QStringLiteral("[Input: "))) {
                text = QStringLiteral("[Input: %1] %2")
                    .arg(renderInputRoleLabel(footage->renderInputRole), text);
            }
            return text;
        }
        if (role == Qt::ForegroundRole) {
            if (missing) return QColor(220, 105, 105);
            if (unused) return QColor(150, 150, 60);
            if (renderInput) return QColor(79, 196, 214);
        }
    }
    return QSortFilterProxyModel::data(index, role);
}

void ProjectFilterProxyModel::setAdvancedFilter(const QString& expression,
                                                const QString& typeFilter,
                                                const bool unusedOnly)
{
    rawExpression_ = expression.trimmed();
    typeFilter_ = typeFilter.trimmed().toLower();
    unusedOnly_ = unusedOnly;
    parseExpression();
    invalidateFilter();
}

int ProjectFilterProxyModel::visibleRowCount() const
{
    return countAcceptedRows(QModelIndex());
}

bool ProjectFilterProxyModel::filterAcceptsRow(const int sourceRow,
                                               const QModelIndex& sourceParent) const
{
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
    ProjectItem* item = ptrVar.isValid()
        ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>())
        : nullptr;

    if (matchesAdvanced(rowIdx0, itemType, item) && matchesLegacyQuery(sourceRow, sourceParent)) {
        return true;
    }
    const int childCount = sourceModel() ? sourceModel()->rowCount(rowIdx0) : 0;
    for (int row = 0; row < childCount; ++row) {
        if (filterAcceptsRow(row, rowIdx0)) {
            return true;
        }
    }
    return false;
}

int ProjectFilterProxyModel::countAcceptedRows(const QModelIndex& sourceParent) const
{
    if (!sourceModel()) {
        return 0;
    }
    int count = 0;
    const int rowCount = sourceModel()->rowCount(sourceParent);
    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex rowIdx0 = sourceModel()->index(row, 0, sourceParent);
        if (!rowIdx0.isValid()) {
            continue;
        }
        const QVariant typeVar = sourceModel()->data(
            rowIdx0, Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
        const eProjectItemType itemType = typeVar.isValid()
            ? static_cast<eProjectItemType>(typeVar.toInt())
            : eProjectItemType::Unknown;
        const QVariant ptrVar = sourceModel()->data(
            rowIdx0, Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        ProjectItem* item = ptrVar.isValid()
            ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>())
            : nullptr;
        if (matchesAdvanced(rowIdx0, itemType, item) && matchesLegacyQuery(row, sourceParent)) {
            ++count;
        }
        count += countAcceptedRows(rowIdx0);
    }
    return count;
}

bool ProjectFilterProxyModel::matchesLegacyQuery(const int sourceRow,
                                                 const QModelIndex& sourceParent) const
{
    if (query_.isSearchTextEmpty()) {
        return true;
    }
    const int columns = sourceModel() ? sourceModel()->columnCount(sourceParent) : 0;
    for (int column = 0; column < columns; ++column) {
        const QModelIndex index = sourceModel()->index(sourceRow, column, sourceParent);
        const QString text = sourceModel()->data(index, Qt::DisplayRole).toString();
        if (query_.matches(text.toUtf8().constData(), LayerSearchType::Any, true, false, false)) {
            return true;
        }
    }
    return false;
}

void ProjectFilterProxyModel::parseExpression()
{
    plainTerms_.clear();
    tagTerms_.clear();
    regexPattern_.clear();
    regexEnabled_ = false;
    missingOnly_ = false;
    usedOnly_ = false;
    bool expressionTypeSeen = false;

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
            const QString value = token.mid(5).trimmed().toLower();
            if (!value.isEmpty()) {
                if (!expressionTypeSeen) {
                    typeFilter_ = value;
                    expressionTypeSeen = true;
                } else if (!typeFilter_.contains(value, Qt::CaseInsensitive)) {
                    typeFilter_ += QStringLiteral(",") + value;
                }
            }
            continue;
        }
        if (token.compare("unused:true", Qt::CaseInsensitive) == 0 ||
            token.compare("is:unused", Qt::CaseInsensitive) == 0 ||
            token.compare("used:false", Qt::CaseInsensitive) == 0) {
            unusedOnly_ = true;
            continue;
        }
        if (token.compare("used:true", Qt::CaseInsensitive) == 0 ||
            token.compare("is:used", Qt::CaseInsensitive) == 0) {
            usedOnly_ = true;
            continue;
        }
        if (token.compare("missing:true", Qt::CaseInsensitive) == 0 ||
            token.compare("is:missing", Qt::CaseInsensitive) == 0) {
            missingOnly_ = true;
            continue;
        }
        plainTerms_.append(token);
    }
}

bool ProjectFilterProxyModel::typeMatches(const eProjectItemType itemType,
                                          const ProjectItem* item) const
{
    if (typeFilter_.isEmpty() || typeFilter_ == "all") {
        return true;
    }
    const QStringList requestedTypes = typeFilter_.split(
        QRegularExpression(QStringLiteral("[,|+]+")), Qt::SkipEmptyParts);
    bool recognizedType = false;
    for (const QString& requestedType : requestedTypes) {
        const QString normalizedType = requestedType.trimmed().toLower();
        if (normalizedType == QStringLiteral("all")) return true;
        if (normalizedType == QStringLiteral("composition")) {
            recognizedType = true;
            if (itemType == eProjectItemType::Composition) return true;
        } else if (normalizedType == QStringLiteral("footage")) {
            recognizedType = true;
            if (itemType == eProjectItemType::Footage) return true;
        } else if (normalizedType == QStringLiteral("folder")) {
            recognizedType = true;
            if (itemType == eProjectItemType::Folder) return true;
        } else if (normalizedType == QStringLiteral("solid")) {
            recognizedType = true;
            if (itemType == eProjectItemType::Solid) return true;
        } else if (normalizedType == QStringLiteral("input source") ||
                   normalizedType == QStringLiteral("input") ||
                   normalizedType == QStringLiteral("renderinput")) {
            recognizedType = true;
            if (itemType == eProjectItemType::Footage && item &&
                static_cast<const FootageItem*>(item)->assetUsage == ProjectAssetUsage::RenderInput) {
                return true;
            }
        }
    }
    return !recognizedType;
}

bool ProjectFilterProxyModel::matchesAdvanced(const QModelIndex& index,
                                              const eProjectItemType itemType,
                                              ProjectItem* item) const
{
    if (!typeMatches(itemType, item)) return false;

    const bool classifiedRenderInput = item && itemType == eProjectItemType::Footage &&
        static_cast<const FootageItem*>(item)->assetUsage == ProjectAssetUsage::RenderInput;
    if (usedOnly_ && (!item || (!classifiedRenderInput && projectItemUsageCount(item) <= 0))) {
        return false;
    }

    const QString name = sourceModel()->data(index, Qt::DisplayRole).toString();
    QString searchBlob = name;
    if (item) {
        searchBlob += QStringLiteral(" ") + item->tags.join(QStringLiteral(" "));
    }
    if (item && itemType == eProjectItemType::Footage) {
        const auto* footage = static_cast<const FootageItem*>(item);
        const QString path = footage->filePath;
        const QString normalizedPath = QDir::cleanPath(path);
        searchBlob += QStringLiteral(" ") + path;
        if (classifiedRenderInput) {
            searchBlob += QStringLiteral(" input source render input ") +
                renderInputRoleLabel(footage->renderInputRole);
        }
        if (unusedOnly_ && classifiedRenderInput) return false;
        if (unusedOnly_ && !unusedAssetPaths_.contains(normalizedPath) &&
            projectItemUsageCount(item) > 0) return false;
        if (missingOnly_) {
            bool missing = !QFileInfo(path).exists();
            if (footage->isSequence) {
                for (const QString& sequencePath : footage->sequencePaths) {
                    if (!QFileInfo(sequencePath).exists()) {
                        missing = true;
                        break;
                    }
                }
            }
            if (!missing) return false;
        }
    } else if (unusedOnly_ || missingOnly_) {
        if (unusedOnly_ && projectItemUsageCount(item) > 0) return false;
        if (missingOnly_) return false;
    }

    for (const QString& term : plainTerms_) {
        if (!searchBlob.contains(term, Qt::CaseInsensitive)) return false;
    }
    for (const QString& tag : tagTerms_) {
        if (!searchBlob.contains(tag, Qt::CaseInsensitive)) return false;
    }
    if (regexEnabled_) {
        const QRegularExpression regex(regexPattern_, QRegularExpression::CaseInsensitiveOption);
        if (!regex.isValid() || !regex.match(searchBlob).hasMatch()) return false;
    }
    return true;
}

}
