module;
#include <wobjectdefs.h>
#include <QObject>

export module Artifact.Render.Queue.Service;

import std;

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
  void addRenderQueue();
  void removeRenderQueue();
  void removeAllRenderQueues();

  void startRenderQueue();
  void startAllRenderQueues();

 };



};