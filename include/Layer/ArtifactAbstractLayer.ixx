module;

#include <wobjectdefs.h>
#include<QString>
#include <QObject>



export module Artifact.Layers.Abstract;


import Utils.String.Like;

export namespace Artifact {

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
  //Q_OBJECT
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactAbstractLayer();
  virtual ~ArtifactAbstractLayer();
  void Show();
  void Hide();
  bool isVisible() const;
  virtual void draw() = 0;

	
 };

 class Artifact2DLayer:public ArtifactAbstractLayer
 {
	 
 };




}