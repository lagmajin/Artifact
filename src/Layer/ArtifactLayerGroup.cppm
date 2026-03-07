module;

#include <QString>
#include <QVector>
#include <QColor>
#include <QDebug>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Layer.Group;




import Utils.Id;

namespace Artifact
{

// ==================== ArtifactLayerGroup::Impl ====================

class ArtifactLayerGroup::Impl
{
public:
    LayerID id_ = 0;
    QString name_ = "New Group";
    LayerID parentGroupId_ = 0;  // 0 = no parent (root level)

    QVector<LayerID> childLayers_;

    bool isExpanded_ = true;
    bool isMuted_ = false;
    float groupOpacity_ = 1.0f;
    LAYER_BLEND_TYPE groupBlendMode_ = LAYER_BLEND_TYPE::Normal;
    QColor groupColor_ = Qt::gray;
    bool isLocked_ = false;
};

// ==================== ArtifactLayerGroup ====================

ArtifactLayerGroup::ArtifactLayerGroup()
    : impl_(new Impl())
{
    impl_->id_ = generateId();
}

ArtifactLayerGroup::ArtifactLayerGroup(const QString& name)
    : impl_(new Impl())
{
    impl_->id_ = generateId();
    impl_->name_ = name;
}

ArtifactLayerGroup::~ArtifactLayerGroup()
{
    delete impl_;
}

ArtifactLayerGroup::LayerID ArtifactLayerGroup::id() const
{
    return impl_->id_;
}

void ArtifactLayerGroup::setId(LayerID id)
{
    impl_->id_ = id;
}

QString ArtifactLayerGroup::name() const
{
    return impl_->name_;
}

void ArtifactLayerGroup::setName(const QString& name)
{
    impl_->name_ = name;
}

ArtifactLayerGroup::LayerID ArtifactLayerGroup::parentGroupId() const
{
    return impl_->parentGroupId_;
}

void ArtifactLayerGroup::setParentGroupId(LayerID parentId)
{
    impl_->parentGroupId_ = parentId;
}

void ArtifactLayerGroup::addLayer(LayerID layerId)
{
    if (!impl_->childLayers_.contains(layerId)) {
        impl_->childLayers_.append(layerId);
    }
}

void ArtifactLayerGroup::removeLayer(LayerID layerId)
{
    impl_->childLayers_.removeAll(layerId);
}

QVector<ArtifactLayerGroup::LayerID> ArtifactLayerGroup::childLayers() const
{
    return impl_->childLayers_;
}

bool ArtifactLayerGroup::containsLayer(LayerID layerId) const
{
    return impl_->childLayers_.contains(layerId);
}

int ArtifactLayerGroup::layerCount() const
{
    return impl_->childLayers_.size();
}

bool ArtifactLayerGroup::isExpanded() const
{
    return impl_->isExpanded_;
}

void ArtifactLayerGroup::setExpanded(bool expanded)
{
    impl_->isExpanded_ = expanded;
}

bool ArtifactLayerGroup::isMuted() const
{
    return impl_->isMuted_;
}

void ArtifactLayerGroup::setMuted(bool muted)
{
    impl_->isMuted_ = muted;
}

float ArtifactLayerGroup::groupOpacity() const
{
    return impl_->groupOpacity_;
}

void ArtifactLayerGroup::setGroupOpacity(float opacity)
{
    impl_->groupOpacity_ = std::max(0.0f, std::min(1.0f, opacity));
}

LAYER_BLEND_TYPE ArtifactLayerGroup::groupBlendMode() const
{
    return impl_->groupBlendMode_;
}

void ArtifactLayerGroup::setGroupBlendMode(LAYER_BLEND_TYPE mode)
{
    impl_->groupBlendMode_ = mode;
}

QColor ArtifactLayerGroup::groupColor() const
{
    return impl_->groupColor_;
}

void ArtifactLayerGroup::setGroupColor(const QColor& color)
{
    impl_->groupColor_ = color;
}

bool ArtifactLayerGroup::isLocked() const
{
    return impl_->isLocked_;
}

void ArtifactLayerGroup::setLocked(bool locked)
{
    impl_->isLocked_ = locked;
}

void ArtifactLayerGroup::moveLayerUp(LayerID layerId)
{
    int idx = impl_->childLayers_.indexOf(layerId);
    if (idx > 0) {
        impl_->childLayers_.removeAt(idx);
        impl_->childLayers_.insert(idx - 1, layerId);
    }
}

void ArtifactLayerGroup::moveLayerDown(LayerID layerId)
{
    int idx = impl_->childLayers_.indexOf(layerId);
    if (idx >= 0 && idx < impl_->childLayers_.size() - 1) {
        impl_->childLayers_.removeAt(idx);
        impl_->childLayers_.insert(idx + 1, layerId);
    }
}

void ArtifactLayerGroup::moveLayerToIndex(LayerID layerId, int index)
{
    int idx = impl_->childLayers_.indexOf(layerId);
    if (idx >= 0) {
        impl_->childLayers_.removeAt(idx);
        index = std::max(0, std::min(index, impl_->childLayers_.size()));
        impl_->childLayers_.insert(index, layerId);
    }
}

QVector<ArtifactLayerGroup::LayerID> ArtifactLayerGroup::flatten() const
{
    // グループを展開して子レイヤーのリストを返す
    return impl_->childLayers_;
}

// ==================== ArtifactLayerGroupCollection::Impl ====================

class ArtifactLayerGroupCollection::Impl
{
public:
    QHash<LayerID, std::unique_ptr<ArtifactLayerGroup>> groups_;
    QHash<LayerID, LayerID> layerToGroupMap_;  // layerId -> groupId

    LayerID nextGroupId_ = 1;
    const LayerID rootGroupId_ = 0;  // 特別なルートグループID
};

ArtifactLayerGroupCollection::ArtifactLayerGroupCollection()
    : impl_(new Impl())
{
}

ArtifactLayerGroupCollection::~ArtifactLayerGroupCollection()
{
    delete impl_;
}

ArtifactLayerGroup* ArtifactLayerGroupCollection::createGroup(const QString& name)
{
    LayerID newId = impl_->nextGroupId_++;
    auto group = std::make_unique<ArtifactLayerGroup>(name);
    group->setId(newId);
    ArtifactLayerGroup* ptr = group.get();
    impl_->groups_[newId] = std::move(group);
    return ptr;
}

void ArtifactLayerGroupCollection::deleteGroup(LayerID groupId)
{
    if (groupId == impl_->rootGroupId_) {
        qWarning() << "Cannot delete root group";
        return;
    }

    // 子レイヤーと子グループを親に移動
    auto* group = getGroup(groupId);
    if (group) {
        LayerID parentId = group->parentGroupId();
        for (LayerID layerId : group->childLayers()) {
            setLayerGroup(layerId, parentId);
        }

        // 子グループも削除（再帰的）
        for (auto& [id, g] : impl_->groups_) {
            if (g->parentGroupId() == groupId) {
                deleteGroup(id);
            }
        }
    }

    impl_->groups_.remove(groupId);
}

ArtifactLayerGroup* ArtifactLayerGroupCollection::getGroup(LayerID groupId)
{
    if (groupId == impl_->rootGroupId_) {
        // ルートグループのロジックが必要な場合はここで処理
        return nullptr;
    }
    auto it = impl_->groups_.find(groupId);
    return (it != impl_->groups_.end()) ? it->get() : nullptr;
}

const ArtifactLayerGroup* ArtifactLayerGroupCollection::getGroup(LayerID groupId) const
{
    if (groupId == impl_->rootGroupId_) {
        return nullptr;
    }
    auto it = impl_->groups_.find(groupId);
    return (it != impl_->groups_.end()) ? it->get() : nullptr;
}

QVector<ArtifactLayerGroup*> ArtifactLayerGroupCollection::allGroups()
{
    QVector<ArtifactLayerGroup*> result;
    for (auto& [id, group] : impl_->groups_) {
        result.append(group.get());
    }
    return result;
}

QVector<const ArtifactLayerGroup*> ArtifactLayerGroupCollection::allGroups() const
{
    QVector<const ArtifactLayerGroup*> result;
    for (const auto& [id, group] : impl_->groups_) {
        result.append(group.get());
    }
    return result;
}

ArtifactLayerGroup::LayerID ArtifactLayerGroupCollection::rootGroupId() const
{
    return impl_->rootGroupId_;
}

QVector<ArtifactLayerGroup*> ArtifactLayerGroupCollection::getRootGroups()
{
    QVector<ArtifactLayerGroup*> result;
    for (auto& [id, group] : impl_->groups_) {
        if (group->parentGroupId() == impl_->rootGroupId_) {
            result.append(group.get());
        }
    }
    return result;
}

QVector<ArtifactLayerGroup*> ArtifactLayerGroupCollection::getChildGroups(LayerID parentId)
{
    QVector<ArtifactLayerGroup*> result;
    for (auto& [id, group] : impl_->groups_) {
        if (group->parentGroupId() == parentId) {
            result.append(group.get());
        }
    }
    return result;
}

ArtifactLayerGroup::LayerID ArtifactLayerGroupCollection::getLayerGroup(LayerID layerId) const
{
    auto it = impl_->layerToGroupMap_.find(layerId);
    return (it != impl_->layerToGroupMap_.end()) ? it.value() : impl_->rootGroupId_;
}

void ArtifactLayerGroupCollection::setLayerGroup(LayerID layerId, LayerID groupId)
{
    // 既存の所属を解除
    LayerID currentGroup = getLayerGroup(layerId);
    if (currentGroup != impl_->rootGroupId_) {
        if (auto* group = getGroup(currentGroup)) {
            group->removeLayer(layerId);
        }
    }

    // 新しい所属を設定
    impl_->layerToGroupMap_[layerId] = groupId;
    if (groupId != impl_->rootGroupId_) {
        if (auto* group = getGroup(groupId)) {
            group->addLayer(layerId);
        }
    }
}

int ArtifactLayerGroupCollection::groupCount() const
{
    return static_cast<int>(impl_->groups_.size());
}

bool ArtifactLayerGroupCollection::isEmpty() const
{
    return impl_->groups_.empty();
}

void ArtifactLayerGroupCollection::clear()
{
    impl_->groups_.clear();
    impl_->layerToGroupMap_.clear();
}

void ArtifactLayerGroupCollection::moveGroup(LayerID groupId, LayerID newParentId, int index)
{
    if (auto* group = getGroup(groupId)) {
        group->setParentGroupId(newParentId);
    }
}

} // namespace Artifact
