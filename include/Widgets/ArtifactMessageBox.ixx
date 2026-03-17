module;
#include <QString>
#include <QWidget>

export module Artifact.Widgets.AppDialogs;

export namespace Artifact {

class ArtifactMessageBox {
public:
    static bool confirmDelete(QWidget* parent, const QString& title, const QString& text);
    static bool confirmOverwrite(QWidget* parent, const QString& title, const QString& text);
    static bool confirmAction(QWidget* parent, const QString& title, const QString& text);
};

} // namespace Artifact
