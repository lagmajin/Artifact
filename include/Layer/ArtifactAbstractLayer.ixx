module;

#include <wobjectdefs.h>
#include <QtCore/QObject>

export module ArtifactAbstractLayer;



export namespace Artifact {


 class ArtifactAbstractLayer:public QObject {
  //Q_OBJECT
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactAbstractLayer();
  virtual ~ArtifactAbstractLayer();
  //void setRotation();
  //void setScale();
 //signals:
  //void layerUpdated();
  
 //public slots:
  void Show();
  void Hide();
 };

}