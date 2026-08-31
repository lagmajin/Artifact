module;

#include <QJsonObject>
#include <QString>
#include <QVector>

export module Artifact.Undo.ProjectItemSupport;

import Artifact.Project.Items;

export namespace Artifact {

ProjectItem* findProjectItemInTreeForUndo(const QVector<ProjectItem*>& items,
                                          const QString& itemId);
QJsonObject projectItemSnapshotForUndo(const ProjectItem* item);

}
