module;
#include <QString>

export module Artifact.Export.LottieWriter;

import Artifact.Composition.Abstract;

export namespace Artifact {

struct ArtifactLottieExportOptions {
    bool prettyPrint = true;
    bool embedImages = true;
    bool compressKeyframes = true;
    double preRenderScale = 1.0;
};

class ArtifactExportLottieWriter {
public:
    static bool write(const ArtifactCompositionPtr& composition,
                      const QString& outputPath,
                      const ArtifactLottieExportOptions& options = {},
                      QString* errorMessage = nullptr);
};

} // namespace Artifact
