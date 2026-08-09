module;
#include <QString>

export module Artifact.Export.RmlUiWriter;

import Artifact.Composition.Abstract;

export namespace Artifact {

struct ArtifactRmlUiExportOptions {
    double preRenderScale = 1.0;
};

class ArtifactExportRmlUiWriter {
public:
    static bool write(const ArtifactCompositionPtr& composition,
                      const QString& outputDirectory,
                      const ArtifactRmlUiExportOptions& options = {},
                      QString* errorMessage = nullptr);
    static bool write(const ArtifactCompositionPtr& composition,
                      const QString& outputDirectory,
                      QString* errorMessage);
};

} // namespace Artifact
