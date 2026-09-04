module;
#include <QWidget>
#include <wobjectdefs.h>

class QEvent;
class QListWidget;
class QPushButton;

export module Artifact.Widgets.TemplateLibrary;

import Artifact.Template.Document;

export namespace Artifact {

class ArtifactTemplateLibraryWidget final : public QWidget {
    W_OBJECT(ArtifactTemplateLibraryWidget)
public:
    explicit ArtifactTemplateLibraryWidget(QWidget* parent = nullptr);

    void setLibrary(const ArtifactTemplateLibrary& library);
    ArtifactTemplateLibrary library() const;
    void refresh();
    ArtifactTemplateDocument selectedDocument(QString* errorMessage = nullptr) const;
    int applySelectedToCurrentComposition(QString* errorMessage = nullptr) const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ArtifactTemplateLibrary library_;
    QListWidget* list_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
};

} // namespace Artifact
