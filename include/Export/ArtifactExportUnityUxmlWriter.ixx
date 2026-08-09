module;
#include <QString>

export module Artifact.Export.UnityUxmlWriter;

import Artifact.Composition.Abstract;

export namespace Artifact {

struct ArtifactUnityUxmlExportOptions {
    double preRenderScale = 1.0;
};

class ArtifactExportUnityUxmlWriter {
public:
    static bool write(const ArtifactCompositionPtr& composition,
                      const QString& outputDirectory,
                      const ArtifactUnityUxmlExportOptions& options = {},
                      QString* errorMessage = nullptr);
    static bool write(const ArtifactCompositionPtr& composition,
                      const QString& outputDirectory,
                      QString* errorMessage);
};

} // namespace Artifact
