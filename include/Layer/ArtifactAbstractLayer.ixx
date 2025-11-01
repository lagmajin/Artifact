module;

#include <wobjectdefs.h>
#include<QString>
#include <QObject>



export module Artifact.Layers.Abstract;

import std;

import Utils.Id;
import Utils.String.Like;
import Layer.Blend;


export namespace Artifact {

 using namespace ArtifactCore;

 enum class LayerType
 {
  None,
  Solid,       // 単色レイヤー
  Image,       // 静止画像（PNG/JPGなど）
  Adjustment,  // 調整レイヤー
  Text,        // テキストレイヤー
  Shape,       // シェイプレイヤー
  Precomp,     // プリコンポジション
  Audio,       // 音声のみレイヤー
  Video
 };



 class ArtifactAbstractLayer :public QObject {
  W_OBJECT(ArtifactAbstractLayer)
 private:
  class Impl;
  Impl* impl_
 protected:
 	
 public:
  ArtifactAbstractLayer();
  virtual ~ArtifactAbstractLayer();
  void Show();
  void Hide();
  bool isVisible() const;
  virtual void draw() = 0;

  LAYER_BLEND_TYPE layerBlendType() const;
  void setBlendMode(LAYER_BLEND_TYPE type);

  Id layerId() const;
 	
  std::type_index type_index() const;
 };

 class Artifact2DLayer :public ArtifactAbstractLayer
 {

 };

 using ArtifactAbstractLayerPtr = std::shared_ptr<ArtifactAbstractLayer>;
 using ArtifactAbstractLayerWeak = std::weak_ptr<ArtifactAbstractLayer>;


}