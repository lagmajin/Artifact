module;
#include <QColor>
#include <QJsonObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

export module Artifact.Export.Session;

import Artifact.Composition.Abstract;

export namespace Artifact {

struct ArtifactExportLayerSnapshot {
    QString id;
    QString name;
    QString parentId;
    int type = 0;
    bool visible = true;
    bool is3D = false;
    bool requiresPreRender = false;
    QString preRenderReason;
    int blendMode = 0;
    double opacity = 1.0;
    int inPoint = 0;
    int outPoint = 0;
    QJsonObject serialized;
};

struct ArtifactExportAssetSnapshot {
    QString sourcePath;
    QString relativePath;
};

struct ArtifactExportSnapshot {
    QString name;
    QSize size;
    QColor backgroundColor;
    double frameRate = 30.0;
    int inPoint = 0;
    int outPoint = 0;
    QVector<ArtifactExportLayerSnapshot> layers;
    QVector<ArtifactExportAssetSnapshot> assets;
    QStringList warnings;
    bool hasTransformKeyframes = false;
};

class ArtifactExportSession {
public:
    explicit ArtifactExportSession(ArtifactCompositionPtr composition);

    bool build(QString* errorMessage = nullptr);
    bool isBuilt() const noexcept;
    const ArtifactExportSnapshot& snapshot() const noexcept;
    ArtifactCompositionPtr composition() const noexcept;
    QString assetPathFor(const QString& sourcePath) const;
    bool copyAssets(const QString& outputDirectory,
                    QString* errorMessage = nullptr) const;

private:
    ArtifactCompositionPtr composition_;
    ArtifactExportSnapshot snapshot_;
    bool built_ = false;
};

} // namespace Artifact
