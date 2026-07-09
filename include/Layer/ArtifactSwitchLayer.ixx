module;
#include <utility>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <wobjectdefs.h>
#include <memory>
#include <vector>

export module Artifact.Layer.Switch;

import Artifact.Layers.Abstract._2D;
import Audio.LipSyncTrack;
import FloatRGBA;
import Frame.Position;

export namespace Artifact {

/// スイッチレイヤー: 子レイヤーを切り替えて表示する
/// After Effects の「Switch Layer」/ Moho の「Switch Layer」相当
class ArtifactSwitchLayer : public ArtifactAbstract2DLayer {
    W_OBJECT(ArtifactSwitchLayer)
private:
    class Impl;
    Impl* impl_;
public:
    ArtifactSwitchLayer();
    ~ArtifactSwitchLayer();
    ArtifactSwitchLayer(const ArtifactSwitchLayer&) = delete;
    ArtifactSwitchLayer& operator=(const ArtifactSwitchLayer&) = delete;

    void setComposition(void* comp) override;
    void draw(ArtifactIRenderer* renderer) override;
    bool isSwitchLayer() const { return true; }
    QRectF localBounds() const override;

    // 子レイヤー管理
    int addChildLayer(const ArtifactAbstractLayerPtr& layer);
    bool removeChildLayer(int index);
    ArtifactAbstractLayerPtr childLayerAt(int index) const;
    int childCount() const { return childrenCount(); }
    std::vector<ArtifactAbstractLayerPtr> allChildren() const;

    // アクティブ選択
    int activeIndex() const;
    void setActiveIndex(int index);
    ArtifactAbstractLayerPtr activeChild() const;

    // 切り替えタイミング
    bool syncToTimeline() const;
    void setSyncToTimeline(bool sync);
    int timelineFrameForIndex(int index) const;
    void setFrameForIndex(int index, int frame);
    void setTimelineFrames(const std::vector<int>& frames);
    std::vector<int> timelineFrames() const;
    void applyLipSyncTrack(const ArtifactCore::LipSyncTrack& track);

    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool hasChildren() const { return childrenCount() > 0; }
    int childrenCount() const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};

} // namespace Artifact

W_REGISTER_ARGTYPE(Artifact::ArtifactSwitchLayer)
