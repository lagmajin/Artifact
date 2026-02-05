module;
#include <wobjectdefs.h>
#include <QObject>

export module Artifact.Render.Queue.Service;

import std;

export namespace Artifact
{
 // Forward declarations
 class ArtifactRenderJob;

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
  
  // Job control methods
  void startAllJobs();
  void pauseAllJobs();
  void cancelAllJobs();
  
  // Job query methods
  int jobCount() const;
  // ArtifactRenderJob getJob(int index) const;  // Commented out - ArtifactRenderJob not exported
  int getTotalProgress() const;
  
  // Callback setters
  void setJobAddedCallback(std::function<void(int)> callback);
  void setJobRemovedCallback(std::function<void(int)> callback);
  void setJobUpdatedCallback(std::function<void(int)> callback);
  // void setJobStatusChangedCallback(std::function<void(int, ArtifactRenderJob::Status)> callback);  // Commented out - ArtifactRenderJob not exported
  void setJobProgressChangedCallback(std::function<void(int, int)> callback);
  void setAllJobsCompletedCallback(std::function<void()> callback);
  void setAllJobsRemovedCallback(std::function<void()> callback);

 };



};