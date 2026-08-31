module;

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>

module Artifact.Undo.ProjectItemSupport;

import Artifact.Project.Items;
import Utils.String.UniString;

namespace Artifact {

ProjectItem* findProjectItemInTreeForUndo(const QVector<ProjectItem*>& items,
                                          const QString& itemId)
{
    if (itemId.trimmed().isEmpty()) {
        return nullptr;
    }
    for (auto* item : items) {
        if (!item) {
            continue;
        }
        if (item->id.toString() == itemId) {
            return item;
        }
        if (auto* nested = findProjectItemInTreeForUndo(item->children, itemId)) {
            return nested;
        }
    }
    return nullptr;
}

QJsonObject projectItemSnapshotForUndo(const ProjectItem* item)
{
    QJsonObject object;
    if (!item) {
        return object;
    }
    object.insert(QStringLiteral("id"), item->id.toString());
    object.insert(QStringLiteral("name"), item->name.toQString());
    object.insert(QStringLiteral("tags"), QJsonArray::fromStringList(item->tags));
    switch (item->type()) {
    case eProjectItemType::Folder: {
        object.insert(QStringLiteral("type"), QStringLiteral("folder"));
        QJsonArray children;
        for (const auto* child : item->children) {
            children.append(projectItemSnapshotForUndo(child));
        }
        object.insert(QStringLiteral("children"), children);
        break;
    }
    case eProjectItemType::Footage: {
        const auto* footage = static_cast<const FootageItem*>(item);
        object.insert(QStringLiteral("type"), QStringLiteral("footage"));
        object.insert(QStringLiteral("filePath"), footage->filePath);
        object.insert(QStringLiteral("isSequence"), footage->isSequence);
        object.insert(QStringLiteral("subimageIndex"), footage->subimageIndex);
        object.insert(QStringLiteral("frameRate"), footage->frameRate);
        object.insert(QStringLiteral("inputColorSpace"), footage->inputColorSpace);
        object.insert(QStringLiteral("inputTransferFunction"),
                      footage->inputTransferFunction);
        QJsonArray sequencePaths;
        for (const auto& path : footage->sequencePaths) {
            sequencePaths.append(path);
        }
        object.insert(QStringLiteral("sequencePaths"), sequencePaths);
        if (footage->assetUsage == ProjectAssetUsage::RenderInput) {
            object.insert(QStringLiteral("assetUsage"), QStringLiteral("renderInput"));
            QString role = QStringLiteral("generic");
            switch (footage->renderInputRole) {
            case ProjectRenderInputRole::AlphaMatte: role = QStringLiteral("alphaMatte"); break;
            case ProjectRenderInputRole::LumaMatte: role = QStringLiteral("lumaMatte"); break;
            case ProjectRenderInputRole::DisplacementMap: role = QStringLiteral("displacementMap"); break;
            case ProjectRenderInputRole::DepthMap: role = QStringLiteral("depthMap"); break;
            case ProjectRenderInputRole::NormalMap: role = QStringLiteral("normalMap"); break;
            case ProjectRenderInputRole::Texture: role = QStringLiteral("texture"); break;
            default: break;
            }
            object.insert(QStringLiteral("renderInputRole"), role);
        }
        break;
    }
    case eProjectItemType::Solid: {
        const auto* solid = static_cast<const SolidItem*>(item);
        object.insert(QStringLiteral("type"), QStringLiteral("solid"));
        object.insert(QStringLiteral("color"), solid->color.name(QColor::HexArgb));
        break;
    }
    default:
        break;
    }
    return object;
}

}
