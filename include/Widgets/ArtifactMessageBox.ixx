module;
#include <utility>

#include <QString>
#include <QStringList>
#include <QWidget>
export module Artifact.Widgets.AppDialogs;

export namespace Artifact {

class ArtifactMessageBox {
public:
    static bool confirmDelete(QWidget* parent, const QString& title, const QString& text);
    static bool confirmOverwrite(QWidget* parent, const QString& title, const QString& text);
    static bool confirmAction(QWidget* parent, const QString& title, const QString& text);
};

enum class ArtifactRenameTarget {
    Composition,
    Layer,
    ProjectItem
};

class ArtifactRenameDialog {
public:
    static QString getName(QWidget* parent,
                           ArtifactRenameTarget target,
                           const QString& currentName,
                           const QString& contextText = {},
                           const QString& detailText = {},
                           const QStringList& unavailableNames = {},
                           bool* accepted = nullptr);
};

} // namespace Artifact
