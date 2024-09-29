#pragma once


#include <QtCore/QObject>


namespace Artifact {


 class ArtifactAbstractLayerPrivate;

 class ArtifactAbstractLayer:public QObject {
  Q_OBJECT
 private:
  ArtifactAbstractLayerPrivate* pLayer_;
 public:
  ArtifactAbstractLayer();
  virtual ~ArtifactAbstractLayer();
  void setRotation();
  void setScale();
 signals:
  void layerUpdated();
  
 public slots:
  void Show();
  void Hide();
 };

}