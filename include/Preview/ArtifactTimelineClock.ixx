module;
#include <wobjectimpl.h>
#include <QObject>
export module Artifact.Preview.Controller;

export namespace Artifact
{
 class ArtifactTimelineClock:public QObject
 {
 	W_OBJECT(ArtifactTimelineClock)
 private:
  class Impl;
  Impl* impl_;
 public:
 	ArtifactTimelineClock(QObject*parent=nullptr);
    ~ArtifactTimelineClock();
 public/*signals*/:
 	
 public /*slots*/:
  void tick(); W_SLOT(tick);
 };


};