module;
#include <QString>

export module Artifact.Export.GamefaceWriter;

import Artifact.Composition.Abstract;

export namespace Artifact {

struct ArtifactGamefaceExportOptions {
    double preRenderScale = 1.0;
};

class ArtifactExportGamefaceWriter {
public:
    static bool write(const ArtifactCompositionPtr& composition,
                      const QString& outputDirectory,
                      const ArtifactGamefaceExportOptions& options = {},
                      QString* errorMessage = nullptr);
    static bool write(const ArtifactCompositionPtr& composition,
                      const QString& outputDirectory,
                      QString* errorMessage);
};

} // namespace Artifact
