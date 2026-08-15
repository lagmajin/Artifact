module;
#include <QJsonArray>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <wobjectdefs.h>

export module Artifact.Widgets.TemplateParameters;

import Artifact.Template.Document;

export namespace Artifact {

class ArtifactTemplateParametersWidget final : public QWidget {
    W_OBJECT(ArtifactTemplateParametersWidget)
public:
    explicit ArtifactTemplateParametersWidget(QWidget* parent = nullptr);

    void setParameters(const QJsonArray& parameters);
    QJsonArray parameters() const;
    void setDocument(const ArtifactTemplateDocument& document);
    ArtifactTemplateDocument document() const;

private:
    QTreeWidget* tree_ = nullptr;
    ArtifactTemplateDocument document_;
};

} // namespace Artifact
