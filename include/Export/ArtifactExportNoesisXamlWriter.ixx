module;
#include <QString>

export module Artifact.Export.NoesisXamlWriter;

import Artifact.Composition.Abstract;

export namespace Artifact {

struct ArtifactNoesisXamlExportOptions {
    double preRenderScale = 1.0;
};

class ArtifactExportNoesisXamlWriter {
public:
    static bool write(const ArtifactCompositionPtr& composition,
                      const QString& outputDirectory,
                      const ArtifactNoesisXamlExportOptions& options = {},
                      QString* errorMessage = nullptr);
    static bool write(const ArtifactCompositionPtr& composition,
                      const QString& outputDirectory,
                      QString* errorMessage);
};

} // namespace Artifact
