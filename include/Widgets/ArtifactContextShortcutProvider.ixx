module;
#include <QString>
#include <QList>
#include <QKeySequence>

export module Artifact.Widgets.ContextShortcutProvider;

import Artifact.Workspace.Modes;

export namespace Artifact {

struct ArtifactShortcutHelpEntry {
    QString category;
    QString actionName;
    QKeySequence shortcut;
    QString description;
};

class ArtifactContextShortcutProvider {
public:
    static QList<ArtifactShortcutHelpEntry> getShortcutsForMode(WorkspaceMode mode);
};

} // namespace Artifact
