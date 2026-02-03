module;
#include <wobjectdefs.h>
#include <QObject>
export module Artifact.Service.ActiveContext;


export namespace Artifact
{
 class ArtifactActiveContextService :public QObject
 {
  W_OBJECT(ArtifactActiveContextService)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactActiveContextService(QObject* parent = nullptr);
  ~ArtifactActiveContextService();
  void setHandler(QObject* obj);
 public/**/:
  void sendPlayToActiveContext(); W_SLOT(sendPlayToActiveContext)
 	void sendPauseToActiveContext(); W_SLOT(sendPauseToActiveContext)
 	void sendStopToActiveContext(); W_SLOT(sendStopToActiveContext)
 	void sendNextFrameToActiveContext(); W_SLOT(sendNextFrameToActiveContext)
 	void sendPreviousFrameToActiveContext(); W_SLOT(sendPreviousFrameToActiveContext)
 	void sendGoToStartToActiveContext(); W_SLOT(sendGoToStartToActiveContext)
 	void sendGoToEndToActiveContext(); W_SLOT(sendGoToEndToActiveContext)
 	void sendSeekToFrameToActiveContext(int frame); W_SLOT(sendSeekToFrameToActiveContext)
 	
 };


};