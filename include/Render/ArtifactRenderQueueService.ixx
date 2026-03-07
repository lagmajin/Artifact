module;
#include <wobjectdefs.h>
#include <QObject>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Render.Queue.Service;





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