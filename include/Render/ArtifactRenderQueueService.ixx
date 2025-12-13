module;
#include <wobjectdefs.h>
#include <QObject>

export module Artifact.Render.Queue.Service;

export namespace Artifact
{
 class ArtifactRenderQueueService:public QObject
 {
 	W_OBJECT(ArtifactRenderQueueService)
 private:
   class Impl;
   Impl* impl_;
 public:
  explicit ArtifactRenderQueueService(QObject*parent=nullptr);
  ~ArtifactRenderQueueService();
 };



};