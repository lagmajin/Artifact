#pragma once


#include <QtCore/QObject>


namespace Artifact {


 class ArtifactAbstractLayerPrivate;

 class ArtifactAbstractLayer:public QObject {
 private:
  ArtifactAbstractLayerPrivate* pLayer_;
 public:
  ArtifactAbstractLayer();
  virtual ~ArtifactAbstractLayer();
 signals:

 public slots:
  void Show();
  void Hide();
 };

}