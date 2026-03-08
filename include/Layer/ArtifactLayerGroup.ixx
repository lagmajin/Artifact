module;

#include <QString>
#include <QVector>
#include <QColor>

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
export module Artifact.Layer.Group;




import Artifact.Layer.Abstract;
import Layer.Blend;
import Utils.Id;

export namespace Artifact
{
    using namespace ArtifactCore;
// レイヤーグループ（Photoshopのフォルダ一样的機能）
class ArtifactLayerGroup
{
public:
    using LayerID = ArtifactCore::LayerID;

private:
    class Impl;
    Impl* impl_;

public:
    ArtifactLayerGroup();
    explicit ArtifactLayerGroup(const QString& name);
    ~ArtifactLayerGroup();

    // グループID
    LayerID id() const;
    void setId(LayerID id);

    // グループ名
    QString name() const;
    void setName(const QString& name);

    // 親グループ
    LayerID parentGroupId() const;
    void setParentGroupId(LayerID parentId);

    // 子レイヤー管理
    void addLayer(LayerID layerId);
    void removeLayer(LayerID layerId);
    QVector<LayerID> childLayers() const;
    bool containsLayer(LayerID layerId) const;
    int layerCount() const;

    // グループ状態
    bool isExpanded() const;
    void setExpanded(bool expanded);

    bool isMuted() const;
    void setMuted(bool muted);

    // グループ全体の不透明度
    float groupOpacity() const;
    void setGroupOpacity(float opacity);

    // グループ全体のブレンドモード
    LAYER_BLEND_TYPE groupBlendMode() const;
    void setGroupBlendMode(LAYER_BLEND_TYPE mode);

    // カラーコーディング（レイヤーパネルでの表示色）
    QColor groupColor() const;
    void setGroupColor(const QColor& color);

    // ロック状態
    bool isLocked() const;
    void setLocked(bool locked);

    // ソート
    void moveLayerUp(LayerID layerId);
    void moveLayerDown(LayerID layerId);
    void moveLayerToIndex(LayerID layerId, int index);

    // フラット化（グループを展開して子レイヤーを返す）
    QVector<LayerID> flatten() const;
};

// レイヤーグループコレクション（複数グループ管理）
class ArtifactLayerGroupCollection
{
private:
    class Impl;
    Impl* impl_;

public:
    using LayerID = ArtifactLayerGroup::LayerID;
    ArtifactLayerGroupCollection();
    ~ArtifactLayerGroupCollection();

    // グループ作成/削除
    ArtifactLayerGroup* createGroup(const QString& name);
    void deleteGroup(LayerID groupId);

    // グループ取得
    ArtifactLayerGroup* getGroup(LayerID groupId);
    const ArtifactLayerGroup* getGroup(LayerID groupId) const;
    QVector<ArtifactLayerGroup*> allGroups();
    QVector<const ArtifactLayerGroup*> allGroups() const;

    // ルートグループ（どのグループにも属さないレイヤー用）
    LayerID rootGroupId() const;

    // グループ階層取得
    QVector<ArtifactLayerGroup*> getRootGroups();
    QVector<ArtifactLayerGroup*> getChildGroups(LayerID parentId);

    // レイヤー所属グループ
    LayerID getLayerGroup(LayerID layerId) const;
    void setLayerGroup(LayerID layerId, LayerID groupId);

    // グループ数
    int groupCount() const;
    bool isEmpty() const;
    void clear();

    // グループ順序変更
    void moveGroup(LayerID groupId, LayerID newParentId, int index);
};

} // namespace Artifact
