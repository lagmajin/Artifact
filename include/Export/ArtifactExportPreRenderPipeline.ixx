module;
#include <QSize>
#include <QString>
#include <QVector>

export module Artifact.Export.PreRenderPipeline;

import Artifact.Layer.Abstract;

export namespace Artifact {

struct ArtifactExportPreRenderOptions {
    QSize resolution;
    int frame = 0;
    double resolutionScale = 1.0;
};

struct ArtifactExportPreRenderSequenceOptions {
    QSize resolution;
    int startFrame = 0;
    int endFrame = 0;
    int frameStep = 1;
    double resolutionScale = 1.0;
};

class ArtifactExportPreRenderPipeline {
public:
    static bool renderLayer(ArtifactAbstractLayer* layer,
                            const QString& outputPath,
                            const ArtifactExportPreRenderOptions& options = {},
                            QString* errorMessage = nullptr);
    static bool renderLayerSequence(
        ArtifactAbstractLayer* layer,
        const QString& outputDirectory,
        const QString& fileStem,
        const ArtifactExportPreRenderSequenceOptions& options,
        QVector<QString>* outputPaths,
        QString* errorMessage = nullptr);
};

} // namespace Artifact
