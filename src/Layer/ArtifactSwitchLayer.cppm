module;
#include <wobjectimpl.h>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <limits>
#include <QJsonArray>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QRectF>

module Artifact.Layer.Switch;

import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Widgets.CompositionRenderOverlay;
import Audio.LipSyncTrack;
import FloatRGBA;

namespace Artifact {

class ArtifactSwitchLayer::Impl {
public:
    std::vector<ArtifactAbstractLayerPtr> children_;
    int activeIndex_ = 0;
    bool syncToTimeline_ = false;
    // timelineFrame[i] = フレーム番号。空なら index 順
    std::vector<int> timelineFrames_;
    ArtifactAbstractComposition* composition_ = nullptr;
};

ArtifactSwitchLayer::ArtifactSwitchLayer()
    : impl_(new Impl())
{
    setLayerName(QStringLiteral("Switch Layer"));
}
ArtifactSwitchLayer::~ArtifactSwitchLayer() { delete impl_; }

void ArtifactSwitchLayer::setComposition(void* comp) {
    ArtifactAbstractLayer::setComposition(comp);
    impl_->composition_ = static_cast<ArtifactAbstractComposition*>(comp);
    for (auto& child : impl_->children_) {
        if (child) child->setComposition(comp);
    }
}

QRectF ArtifactSwitchLayer::localBounds() const {
    auto active = activeChild();
    return active ? active->localBounds() : QRectF(0, 0, 100, 100);
}

void ArtifactSwitchLayer::draw(ArtifactIRenderer* renderer) {
    const int index = [this]() {
        if (!impl_ || impl_->children_.empty()) {
            return 0;
        }
        if (!impl_->syncToTimeline_ || !impl_->composition_) {
            return std::clamp(impl_->activeIndex_, 0,
                              std::max(0, static_cast<int>(impl_->children_.size()) - 1));
        }
        const int64_t currentFrame = impl_->composition_->framePosition().framePosition();
        int resolvedIndex = -1;
        int64_t bestFrame = std::numeric_limits<int64_t>::min();
        for (int i = 0; i < static_cast<int>(impl_->children_.size()); ++i) {
            const int frame = (i < static_cast<int>(impl_->timelineFrames_.size()))
                                  ? impl_->timelineFrames_[i]
                                  : i;
            if (frame <= currentFrame && static_cast<int64_t>(frame) >= bestFrame) {
                bestFrame = frame;
                resolvedIndex = i;
            }
        }
        if (resolvedIndex >= 0) {
            return resolvedIndex;
        }
        return std::clamp(impl_->activeIndex_, 0,
                          std::max(0, static_cast<int>(impl_->children_.size()) - 1));
    }();
    auto active = (index >= 0 && index < static_cast<int>(impl_->children_.size()))
                      ? impl_->children_[index]
                      : ArtifactAbstractLayerPtr{};
    if (active) active->draw(renderer);
}

int ArtifactSwitchLayer::addChildLayer(const ArtifactAbstractLayerPtr& layer) {
    if (!layer) return -1;
    layer->setComposition(impl_->composition_);
    impl_->children_.push_back(layer);
    if (impl_->timelineFrames_.size() < impl_->children_.size()) {
        impl_->timelineFrames_.push_back(static_cast<int>(impl_->children_.size()) - 1);
    }
    return static_cast<int>(impl_->children_.size()) - 1;
}

bool ArtifactSwitchLayer::removeChildLayer(int index) {
    if (index < 0 || index >= static_cast<int>(impl_->children_.size())) return false;
    impl_->children_.erase(impl_->children_.begin() + index);
    if (index < static_cast<int>(impl_->timelineFrames_.size()))
        impl_->timelineFrames_.erase(impl_->timelineFrames_.begin() + index);
    if (impl_->activeIndex_ >= static_cast<int>(impl_->children_.size()))
        impl_->activeIndex_ = std::max(0, static_cast<int>(impl_->children_.size()) - 1);
    return true;
}

ArtifactAbstractLayerPtr ArtifactSwitchLayer::childLayerAt(int index) const {
    if (index < 0 || index >= static_cast<int>(impl_->children_.size())) return nullptr;
    return impl_->children_[index];
}

std::vector<ArtifactAbstractLayerPtr> ArtifactSwitchLayer::allChildren() const {
    return impl_->children_;
}

int ArtifactSwitchLayer::activeIndex() const { return impl_->activeIndex_; }

void ArtifactSwitchLayer::setActiveIndex(int index) {
    impl_->activeIndex_ = std::clamp(index, 0, std::max(0, static_cast<int>(impl_->children_.size()) - 1));
}

ArtifactAbstractLayerPtr ArtifactSwitchLayer::activeChild() const {
    if (impl_->children_.empty()) return nullptr;
    const int index = [this]() {
        if (!impl_ || impl_->children_.empty()) {
            return 0;
        }
        if (!impl_->syncToTimeline_ || !impl_->composition_) {
            return std::clamp(impl_->activeIndex_, 0,
                              std::max(0, static_cast<int>(impl_->children_.size()) - 1));
        }
        const int64_t currentFrame = impl_->composition_->framePosition().framePosition();
        int resolvedIndex = -1;
        int64_t bestFrame = std::numeric_limits<int64_t>::min();
        for (int i = 0; i < static_cast<int>(impl_->children_.size()); ++i) {
            const int frame = (i < static_cast<int>(impl_->timelineFrames_.size()))
                                  ? impl_->timelineFrames_[i]
                                  : i;
            if (frame <= currentFrame && static_cast<int64_t>(frame) >= bestFrame) {
                bestFrame = frame;
                resolvedIndex = i;
            }
        }
        if (resolvedIndex >= 0) {
            return resolvedIndex;
        }
        return std::clamp(impl_->activeIndex_, 0,
                          std::max(0, static_cast<int>(impl_->children_.size()) - 1));
    }();
    if (index < 0 || index >= static_cast<int>(impl_->children_.size())) {
        return nullptr;
    }
    return impl_->children_[index];
}

bool ArtifactSwitchLayer::syncToTimeline() const { return impl_->syncToTimeline_; }
void ArtifactSwitchLayer::setSyncToTimeline(bool sync) { impl_->syncToTimeline_ = sync; }

int ArtifactSwitchLayer::timelineFrameForIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(impl_->timelineFrames_.size()))
        return index;
    return impl_->timelineFrames_[index];
}

void ArtifactSwitchLayer::setFrameForIndex(int index, int frame) {
    if (index < 0) return;
    if (index >= static_cast<int>(impl_->timelineFrames_.size()))
        impl_->timelineFrames_.resize(index + 1, index);
    impl_->timelineFrames_[index] = frame;
}

void ArtifactSwitchLayer::setTimelineFrames(const std::vector<int>& frames) {
    impl_->timelineFrames_ = frames;
    if (impl_->timelineFrames_.size() < impl_->children_.size()) {
        const size_t start = impl_->timelineFrames_.size();
        impl_->timelineFrames_.resize(impl_->children_.size());
        for (size_t i = start; i < impl_->timelineFrames_.size(); ++i) {
            impl_->timelineFrames_[i] = static_cast<int>(i);
        }
    }
}

std::vector<int> ArtifactSwitchLayer::timelineFrames() const {
    return impl_->timelineFrames_;
}

void ArtifactSwitchLayer::applyLipSyncTrack(const ArtifactCore::LipSyncTrack& track) {
    const auto& events = track.events();
    if (events.empty()) {
        return;
    }

    setSyncToTimeline(true);

    std::vector<int> frames;
    frames.resize(static_cast<size_t>(childrenCount()), 0);
    std::vector<bool> seen(static_cast<size_t>(childrenCount()), false);

    for (const auto& event : events) {
        const int shapeIndex = event.mouthShapeIndex();
        if (shapeIndex < 0 || shapeIndex >= childrenCount()) {
            continue;
        }
        const size_t index = static_cast<size_t>(shapeIndex);
        if (!seen[index] || event.frame < frames[index]) {
            frames[index] = static_cast<int>(event.frame);
            seen[index] = true;
        }
    }

    for (int i = 0; i < childrenCount(); ++i) {
        const size_t index = static_cast<size_t>(i);
        if (!seen[index]) {
            frames[index] = i;
        }
    }

    setTimelineFrames(frames);
    setActiveIndex(std::clamp(activeIndex(), 0, std::max(0, childrenCount() - 1)));
}

int ArtifactSwitchLayer::childrenCount() const {
    return static_cast<int>(impl_->children_.size());
}

std::vector<ArtifactCore::PropertyGroup> ArtifactSwitchLayer::getLayerPropertyGroups() const {
    auto groups = ArtifactAbstract2DLayer::getLayerPropertyGroups();
    ArtifactCore::PropertyGroup switchGrp(QStringLiteral("Switch Layer"));
    switchGrp.addProperty(persistentLayerProperty(
        QStringLiteral("switch.activeIndex"),
        ArtifactCore::PropertyType::Integer, activeIndex(), -100));
    switchGrp.addProperty(persistentLayerProperty(
        QStringLiteral("switch.count"),
        ArtifactCore::PropertyType::Integer, childrenCount(), -99));
    switchGrp.addProperty(persistentLayerProperty(
        QStringLiteral("switch.syncToTimeline"),
        ArtifactCore::PropertyType::Boolean, syncToTimeline(), -98));
    groups.push_back(switchGrp);
    return groups;
}

QJsonObject ArtifactSwitchLayer::toJson() const {
    QJsonObject obj;
    QJsonArray children;
    for (size_t i = 0; i < impl_->children_.size(); ++i) {
        QJsonObject childObj;
        if (impl_->children_[i]) {
            childObj["id"] = impl_->children_[i]->id().toString();
            childObj["layerName"] = impl_->children_[i]->layerName();
        }
        childObj["index"] = static_cast<int>(i);
        children.append(childObj);
    }
    obj["children"] = children;
    obj["activeIndex"] = impl_->activeIndex_;
    obj["syncToTimeline"] = impl_->syncToTimeline_;
    QJsonArray timelineFrames;
    for (int frame : impl_->timelineFrames_) {
        timelineFrames.append(frame);
    }
    obj["timelineFrames"] = timelineFrames;
    return obj;
}

void ArtifactSwitchLayer::fromJson(const QJsonObject& obj) {
    impl_->activeIndex_ = obj.value("activeIndex").toInt(0);
    impl_->syncToTimeline_ = obj.value("syncToTimeline").toBool(false);
    impl_->timelineFrames_.clear();
    const QJsonArray frames = obj.value("timelineFrames").toArray();
    impl_->timelineFrames_.reserve(frames.size());
    for (const auto& val : frames) {
        impl_->timelineFrames_.push_back(val.toInt());
    }
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactSwitchLayer)
