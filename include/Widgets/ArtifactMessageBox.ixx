module;
#include <QString>

export module Artifact.Widgets.MessageBox;

class QWidget;

export namespace Artifact {

class ArtifactMessageBox {
public:
    static bool confirmDelete(QWidget* parent, const QString& title, const QString& text);
    static bool confirmOverwrite(QWidget* parent, const QString& title, const QString& text);
    static bool confirmAction(QWidget* parent, const QString& title, const QString& text);
};

} // namespace Artifact
