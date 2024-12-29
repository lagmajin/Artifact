#pragma once

#include <QtCore/QObject>




namespace Artifact {

 class ArtifactRenderer :public QObject{
 Q_OBJECT
 private:

 public:

 public slots:
 signals:
  void renderingStarted();
  void renderingFinished();
};







}