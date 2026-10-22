module;
#include <QString>
#include <QList>
#include <QUuid>
#include <QVariant>

module Artifact.Widgets.ClipBufferModel;

namespace Artifact {

class ArtifactClipBufferModel::Impl {
public:
    QList<ArtifactClipBufferItem> items_;
    std::size_t maxLimit_ = 20;
};

ArtifactClipBufferModel::ArtifactClipBufferModel()
    : impl_(new Impl()) {}

ArtifactClipBufferModel::~ArtifactClipBufferModel() {
    delete impl_;
}

QList<ArtifactClipBufferItem> ArtifactClipBufferModel::items() const {
    return impl_->items_;
}

void ArtifactClipBufferModel::addClip(const QString &layerName, qint64 frame,
                                      const QString &description, const QVariant &data) {
    ArtifactClipBufferItem item;
    item.id = QUuid::createUuid();
    item.layerName = layerName;
    item.frame = frame;
    item.description = description;
    item.data = data;

    impl_->items_.prepend(item); // Newest on top
    while (impl_->items_.size() > static_cast<int>(impl_->maxLimit_)) {
        impl_->items_.removeLast();
    }
}

void ArtifactClipBufferModel::removeClip(const QUuid &id) {
    for (auto it = impl_->items_.begin(); it != impl_->items_.end(); ++it) {
        if (it->id == id) {
            impl_->items_.erase(it);
            return;
        }
    }
}

void ArtifactClipBufferModel::clear() {
    impl_->items_.clear();
}

std::size_t ArtifactClipBufferModel::maxItems() const {
    return impl_->maxLimit_;
}

void ArtifactClipBufferModel::setMaxItems(std::size_t limit) {
    impl_->maxLimit_ = limit;
    while (impl_->items_.size() > static_cast<int>(impl_->maxLimit_)) {
        impl_->items_.removeLast();
    }
}

} // namespace Artifact
