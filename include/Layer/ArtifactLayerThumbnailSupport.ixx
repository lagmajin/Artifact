module;
#include <QImage>
#include <QSize>
#include <QString>

export module Artifact.Layer.ThumbnailSupport;

export namespace Artifact {

QImage buildLayerThumbnail(const QSize &targetSize, const QString &layerName,
                           int sourceWidth, int sourceHeight);

} // namespace Artifact
