module;

#include <wobjectdefs.h>
#include<QString>
#include <QObject>



export module Artifact.Layers.Abstract;



export namespace Artifact {


 class ArtifactAbstractLayer:public QObject {
  //Q_OBJECT
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactAbstractLayer();
  virtual ~ArtifactAbstractLayer();
  void Show();
  void Hide();
  virtual void draw() = 0;
 };




}