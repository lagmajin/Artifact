module;
#include <QString>
#include <QVector>
#include <QColor>
module Artifact.Project.Items;

import std;
import Utils.Id;
import Utils.String.UniString;

namespace Artifact {

FolderItem* FolderItem::addChildFolder(const UniString& name) {
    auto* folder = new FolderItem();
    folder->name = name;
    folder->parent = this;
    this->children.append(folder);
    return folder;
}

} // namespace Artifact
