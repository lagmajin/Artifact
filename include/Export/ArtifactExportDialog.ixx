module;
#include <QString>
#include <QStringList>
#include <QWidget>

export module Artifact.Export.Dialog;

import Artifact.Composition.Abstract;

export namespace Artifact {

class ArtifactExportDialog {
public:
    static bool run(QWidget* parent,
                    const ArtifactCompositionPtr& composition,
                    QString* errorMessage = nullptr);
};

} // namespace Artifact
