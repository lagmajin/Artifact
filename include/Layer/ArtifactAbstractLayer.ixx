module ;
#include <QRectF>
#include <wobjectdefs.h>
#include <QHash>
#include <QString>
#include <QObject>
#include <qtypes.h>
#include <QJsonObject>

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

import <cstdint> ;


export namespace Artifact {

 using namespace ArtifactCore;

 enum class LayerType
 {
  Unknown=0,
  None,
  Null,
  Solid,       // 単色レイヤー
  Image,       // 静止画像（PNG/JPGなど）
  Adjustment,  // 調整レイヤー
  Text,        // テキストレイヤー
  Shape,       // シェイプレイヤー
  Precomp,     // プリコンポジション
  Audio,       // 音声レイヤー
  Video,       // 映像レイヤー
  Media,       // 映像・音声メディア
  Camera,
 };

 class ArtifactAbstractLayer;

 using ArtifactAbstractLayerPtr = std::shared_ptr<ArtifactAbstractLayer>;
 using ArtifactAbstractLayerWeak = std::weak_ptr<ArtifactAbstractLayer>;
 
 

 class ArtifactAbstractLayer :public QObject {
  W_OBJECT(ArtifactAbstractLayer)
 private:
  class Impl;
  Impl* impl_;
 protected:
 	void setSourceSize(const Size_2D& size);
 public:
  ArtifactAbstractLayer();
  virtual ~ArtifactAbstractLayer();
  // Serialization (stub implementations returning empty/default values)
  // Return a JSON representation of this layer. Default implementation
  // returns an empty object; override in derived classes to provide data.
  QJsonObject toJson() const;

  // Construct a layer from JSON. Default returns empty shared_ptr.
  static ArtifactAbstractLayerPtr fromJson(const QJsonObject &obj);
  void Show();
  void Hide();
  bool isVisible() const;
  void setVisible(bool visible=true);
  QString layerName() const;
  void setLayerName(const QString& name);

  virtual void draw() = 0;

  LAYER_BLEND_TYPE layerBlendType() const;
  void setBlendMode(LAYER_BLEND_TYPE type);

  LayerID id() const;
 	
  std::type_index type_index() const;
  void* QueryInterface(const std::type_index& ti);
  UniString className() const;

  /*Transform*/
  Size_2D sourceSize() const;
  Size_2D aabb() const;
 QRectF transformedBoundingBox() const;
 	
  AnimatableTransform2D& transform2D();
  const AnimatableTransform2D& transform2D() const;
  AnimatableTransform3D& transform3D();
  const AnimatableTransform3D& transform3D() const;
  void setTransform();
  bool isTimeRemapEnabled() const;
  void setTimeRemapEnabled(bool);
  void setTimeRemapKey(int64_t compFrame, double sourceFrame);
 	/*Transform*/
 	
 	/*Timeline*/
  void goToStartFrame();
  void goToEndFrame();
  void goToNextFrame();
  void goToPrevFrame();
  void goToFrame(int64_t frameNumber = 0);
 	/*Timeline*/
 	
  void setParentById(LayerID& id);
  virtual bool isNullLayer() const;
 	
  virtual bool isAdjustmentLayer() const;
  void setAdjustmentLayer(bool isAdjustment);

  bool is3D() const;
  //Audio

  virtual bool hasAudio() const;
  virtual bool hasVideo() const;

  bool isClicked() const;
  bool preciseHit() const;
 public:


 };

 inline uint qHash(const Artifact::ArtifactAbstractLayerPtr& key, uint seed = 0) noexcept {
  return static_cast<uint>(::qHash(reinterpret_cast<quintptr>(key.get()), seed));
 }



}