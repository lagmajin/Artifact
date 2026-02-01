module;

export module Artifact.Project.Roles;

import std;

export namespace Artifact {

// Data roles stored in QStandardItem::data at Qt::UserRole + offset.
// Use this enum to avoid magic numbers throughout the codebase.
export enum class ProjectItemDataRole : int {
    ProjectItemType = 1, // Qt::UserRole + 1 => optional type identifier
    CompositionId = 2,   // Qt::UserRole + 2 => composition id string
};

};
