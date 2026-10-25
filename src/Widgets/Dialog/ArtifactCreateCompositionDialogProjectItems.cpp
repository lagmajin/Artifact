module;

#include <QElapsedTimer>
#include <QSet>
#include <QString>
#include <QDebug>

module Dialog.Composition;

import Artifact.Project.Items;
import Artifact.Service.Project;
import Utils.String.UniString;

namespace Artifact {
namespace {

void collectCompositionNames(ProjectItem* item, QSet<QString>& names)
{
    if (!item) {
        return;
    }
    if (item->type() == eProjectItemType::Composition) {
        const QString name = item->name.toQString().trimmed();
        if (!name.isEmpty()) {
            names.insert(name);
        }
    }
    for (auto* child : item->children) {
        collectCompositionNames(child, names);
    }
}

} // namespace

QSet<QString> occupiedCompositionNames()
{
    QElapsedTimer timer;
    timer.start();
    QSet<QString> names;
    if (auto* service = ArtifactProjectService::instance()) {
        for (auto* root : service->projectItems()) {
            collectCompositionNames(root, names);
        }
    }
    qInfo() << "[CreateCompositionDialog][Names] occupiedCompositionNames ms="
            << timer.elapsed() << "count=" << names.size();
    return names;
}

} // namespace Artifact
