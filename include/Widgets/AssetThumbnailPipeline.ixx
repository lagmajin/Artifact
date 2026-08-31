module;

#include <QFileInfo>
#include <QImage>
#include <QIcon>
#include <QSize>
#include <QString>

export module Artifact.Widgets.AssetThumbnailPipeline;

export namespace Artifact::AssetThumbnail {

QIcon loadFromDisk(const QFileInfo& fileInfo);
void saveToDisk(const QFileInfo& fileInfo, const QImage& image);

QImage loadImageViaWIC(const QString& filePath,
                       const QSize& targetSize = QSize(),
                       QString* errorOut = nullptr);
QImage loadImageViaWindowsShell(const QString& filePath,
                                const QSize& targetSize = QSize(),
                                QString* errorOut = nullptr);
QImage loadImageViaOIIO(const QString& filePath,
                        const QSize& targetSize = QSize(),
                        QString* errorOut = nullptr);

}
