module;
#include <QString>
#include <QList>
#include <QUuid>
#include <QVariant>

export module Artifact.Widgets.ClipBufferModel;

import std;

export namespace Artifact {

struct ArtifactClipBufferItem {
    QUuid id;
    QString layerName;
    qint64 frame = 0;
    QString description;
    QVariant data; // Stores clip context or payload representation
};

class ArtifactClipBufferModel {
public:
    ArtifactClipBufferModel();
    ~ArtifactClipBufferModel();

    QList<ArtifactClipBufferItem> items() const;
    void addClip(const QString &layerName, qint64 frame, const QString &description, const QVariant &data);
    void removeClip(const QUuid &id);
    void clear();

    std::size_t maxItems() const;
    void setMaxItems(std::size_t limit);

private:
    class Impl;
    Impl *impl_;
};

} // namespace Artifact
