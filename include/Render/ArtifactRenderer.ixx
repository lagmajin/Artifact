module;
#include <wobjectdefs.h>
#include <QObject>
export module Render;


//#include <QtCore/QObject>




export namespace Artifact {

 class ArtifactRenderer :public QObject{
// Q_OBJECT
  W_OBJECT(ArtifactRenderer)
 private:

 public:

 public slots:
 signals:
  void renderingStarted();
  void renderingFinished();
};







}