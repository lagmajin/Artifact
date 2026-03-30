module;
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <QMatrix4x4>
#include <qtypes.h>
#include <wobjectdefs.h>

export module Artifact.Layer.Abstract;

import std;

import Size;
import Utils.Id;
import Utils.String.Like;
import Utils.String.UniString;
import Layer.Blend;
import Layer.State;
import Animation.Transform2D;
import Animation.Transform3D;
import Artifact.Effect.Abstract;
import Artifact.Mask.LayerMask;
import Frame.Position;
import Audio.Segment;
export import Property.Abstract;
export import Property.Group;

import Artifact.Render.IRenderer;

export namespace Artifact {

using namespace ArtifactCore;

enum class LayerType {
  Unknown = 0,
  None,
  Null,
  Solid,      // 単色レイヤー
  Image,      // 静止画像（PNG/JPGなど）
  Adjustment, // 調整レイヤー
  Text,       // テキストレイヤー
  Shape,      // シェイプレイヤー
  Precomp = 8,    // プリコンポジション
  Audio = 9,      // 音声レイヤー
  Video = 10,     // 映像・音声フッテージ
  Camera = 11,
  Light = 12,
  Group = 13,
  Folder = 14,
  Particle = 15   // パーティクルレイヤー
};

enum class LayerDirtyFlag : uint32_t {
  Clean = 0,
  Transform = 1 << 0,
  Effect = 1 << 1,
  Mask = 1 << 2,
  Source = 1 << 3, // For image/video changes
  All = 0xFFFFFFFF
};

enum class LayerDirtyReason : uint64_t {
  None = 0,
  PropertyChanged = 1ull << 0,
  TransformChanged = 1ull << 1,
  EffectChanged = 1ull << 2,
  SourceChanged = 1ull << 3,
  TimelineChanged = 1ull << 4,
  VisibilityChanged = 1ull << 5,
  PlaybackChanged = 1ull << 6,
  UserEdit = 1ull << 7
};

// LOD (Level of Detail) system
enum class DetailLevel {
  Low,    // 簡略化された描画（ズーム 0-25%）
  Medium, // 標準的な描画（ズーム 25-75%）
  High    // 高詳細な描画（ズーム 75-100%）
};

class ArtifactAbstractLayer;

using ArtifactAbstractLayerPtr = std::shared_ptr<ArtifactAbstractLayer>;
using ArtifactAbstractLayerWeak = std::weak_ptr<ArtifactAbstractLayer>;

class ArtifactAbstractLayer : public QObject {
  W_OBJECT(ArtifactAbstractLayer)
private:
  class Impl;
  Impl *impl_;

protected:
  void setSourceSize(const Size_2D &size);
  std::shared_ptr<ArtifactCore::AbstractProperty>
  persistentLayerProperty(const QString &propertyPath,
                          ArtifactCore::PropertyType type,
                          const QVariant &value,
                          int priority = 0) const;

public:
    ArtifactAbstractLayer();
    virtual ~ArtifactAbstractLayer();

protected:
    void setIs3D(bool value);

  virtual QJsonObject toJson() const;
  static ArtifactAbstractLayerPtr fromJson(const QJsonObject &obj);

  void Show();
  void Hide();
  bool isVisible() const;
  void setVisible(bool visible = true);
  QString layerName() const;
  void setLayerName(const QString &name);
  QString layerNote() const;
  void setLayerNote(const QString& note);

  void setComposition(void *comp);
  void *composition() const;

  virtual void draw(ArtifactIRenderer *renderer) = 0;

  LAYER_BLEND_TYPE layerBlendType() const;
  void setBlendMode(LAYER_BLEND_TYPE type);

  LayerID id() const;

  std::type_index type_index() const;
  void *QueryInterface(const std::type_index &ti);
  virtual UniString className() const;

  /*Transform*/
  Size_2D sourceSize() const;
  Size_2D aabb() const;
  QRectF transformedBoundingBox() const;

  AnimatableTransform2D &transform2D();
  const AnimatableTransform2D &transform2D() const;
  AnimatableTransform3D &transform3D();
  const AnimatableTransform3D &transform3D() const;
  
  QVector3D position3D() const;
  void setPosition3D(const QVector3D &pos);
  QVector3D rotation3D() const;
  void setRotation3D(const QVector3D &rot);

  void setTransform();
  QTransform getGlobalTransform() const;
  QTransform getLocalTransform() const;
  QMatrix4x4 getGlobalTransform4x4() const;
  QMatrix4x4 getLocalTransform4x4() const;
  ArtifactAbstractLayerPtr parentLayer() const;
  bool isTimeRemapEnabled() const;
  void setTimeRemapEnabled(bool);
  void setTimeRemapKey(int64_t compFrame, double sourceFrame);
  /*Transform*/

  /*Timeline*/
  FramePosition inPoint() const;
  virtual void setInPoint(const FramePosition &pos);
  FramePosition outPoint() const;
  virtual void setOutPoint(const FramePosition &pos);
  FramePosition startTime() const;
  void setStartTime(const FramePosition &pos);

  bool isActiveAt(const FramePosition &pos) const;

  void goToStartFrame();
  void goToEndFrame();
  void goToNextFrame();
  void goToPrevFrame();
  virtual void goToFrame(int64_t frameNumber = 0);
  int64_t currentFrame() const;
  void applyPropertiesFromJson(const QJsonObject &obj);
  virtual void fromJsonProperties(const QJsonObject &obj);
  /*Timeline*/

  void setParentById(const LayerID &id);
  LayerID parentLayerId() const;
  void clearParent();
  bool hasParent() const;
  virtual bool isNullLayer() const;
  virtual QRectF localBounds() const;

  virtual bool isAdjustmentLayer() const;
  void setAdjustmentLayer(bool isAdjustment);

  bool is3D() const;

  virtual bool hasAudio() const;
  virtual bool hasVideo() const;

  /**
   * @brief Get audio data for a specific time range.
   * @param outSegment The segment to fill with audio data. It should be pre-allocated or will be resized.
   * @param start Starting frame position in the composition timeline.
   * @param frameCount Number of audio samples to retrieve.
   * @param sampleRate The requested sample rate (e.g. 48000).
   * @return true if any audio was provided, false otherwise.
   */
  virtual bool getAudio(AudioSegment &outSegment, const FramePosition &start,
                       int frameCount, int sampleRate);

  bool isClicked() const;
  bool preciseHit() const;

  // Flags
  bool isGuide() const;
  void setGuide(bool guide);
  bool isSolo() const;
  void setSolo(bool solo);
  bool isLocked() const;
  void setLocked(bool locked);
  bool isShy() const;
  void setShy(bool shy);

  int labelColorIndex() const;
  void setLabelColorIndex(int index);

  // Opacity
  float opacity() const;
  void setOpacity(float value);

  // Dirty Management
  void setDirty(LayerDirtyFlag flag = LayerDirtyFlag::All);
  void clearDirty(LayerDirtyFlag flag = LayerDirtyFlag::All);
  bool isDirty(LayerDirtyFlag flag = LayerDirtyFlag::All) const;
  void addDirtyReason(LayerDirtyReason reason);
  bool hasDirtyReason(LayerDirtyReason reason) const;
  uint64_t dirtyReasonMask() const;
  void clearDirtyReasons();

  /*Thumbnail*/
  QImage getThumbnail(int width = 128, int height = 128) const;
  /*Thumbnail*/

  /*Effects*/
  void addEffect(std::shared_ptr<class ArtifactAbstractEffect> effect);
  void removeEffect(const UniString &effectID);
  void clearEffects();
  std::vector<std::shared_ptr<class ArtifactAbstractEffect>> getEffects() const;
  std::shared_ptr<class ArtifactAbstractEffect>
  getEffect(const UniString &effectID) const;
  int effectCount() const;
  /*Effects*/

  virtual std::vector<ArtifactCore::PropertyGroup>
  getLayerPropertyGroups() const;
  virtual bool setLayerPropertyValue(const QString &propertyPath,
                                     const QVariant &value);
  std::shared_ptr<ArtifactCore::AbstractProperty> getProperty(const QString &name) const;
  /*Generic Properties*/

  /*Masks*/
  void addMask(const LayerMask &mask);
  void removeMask(int index);
  void setMask(int index, const LayerMask &mask);
  LayerMask mask(int index) const;
  int maskCount() const;
  void clearMasks();
  bool hasMasks() const;
  /*Masks*/

  // LOD (Level of Detail) rendering
  virtual void drawLOD(ArtifactIRenderer* renderer, DetailLevel lod);

  void changed() W_SIGNAL(changed);
  void layerNoteChanged(QString note)
    W_SIGNAL(layerNoteChanged, note);

public:
};

inline uint qHash(const Artifact::ArtifactAbstractLayerPtr &key,
                  uint seed = 0) noexcept {
  return static_cast<uint>(
      ::qHash(reinterpret_cast<quintptr>(key.get()), seed));
}

} // namespace Artifact
