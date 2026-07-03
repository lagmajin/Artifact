module;
#include <wobjectimpl.h>
#include <algorithm>
#include <utility>
#include <QJsonArray>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QRectF>

module Artifact.Layer.Switch;

import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Widgets.CompositionRenderOverlay;
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
    auto active = activeChild();
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
    return impl_->children_[impl_->activeIndex_];
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
    return obj;
}

void ArtifactSwitchLayer::fromJson(const QJsonObject& obj) {
    impl_->activeIndex_ = obj.value("activeIndex").toInt(0);
    impl_->syncToTimeline_ = obj.value("syncToTimeline").toBool(false);
}

} // namespace Artifact

W_OBJECT_IMPL(ArtifactSwitchLayer)
