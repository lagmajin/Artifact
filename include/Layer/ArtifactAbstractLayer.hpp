module;

#include <wobjectdefs.h>
#include <QtCore/QObject>

export module ArtifactAbstractLayer;



export namespace Artifact {


 class ArtifactAbstractLayerPrivate;

 class ArtifactAbstractLayer:public QObject {
  //Q_OBJECT
 private:
  ArtifactAbstractLayerPrivate* pLayer_;
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