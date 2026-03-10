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
import Utils.Id;





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
  static ArtifactRenderQueueService* instance();
  void addRenderQueue();
  void addRenderQueueForComposition(const ArtifactCore::CompositionID& compositionId, const QString& compositionName);
  void removeRenderQueue();
  void removeRenderQueueAt(int index);
  void duplicateRenderQueueAt(int index);
  void moveRenderQueue(int fromIndex, int toIndex);
  void removeAllRenderQueues();
  void removeRenderQueuesForComposition(const ArtifactCore::CompositionID& compositionId);
  bool hasRenderQueueForComposition(const ArtifactCore::CompositionID& compositionId) const;
  int renderQueueCountForComposition(const ArtifactCore::CompositionID& compositionId) const;
  QString jobCompositionNameAt(int index) const;
  QString jobStatusAt(int index) const;
  int jobProgressAt(int index) const;
  QString jobOutputPathAt(int index) const;
  void setJobOutputPathAt(int index, const QString& outputPath);
  bool jobFrameRangeAt(int index, int* startFrame, int* endFrame) const;
  void setJobFrameRangeAt(int index, int startFrame, int endFrame);
  bool jobOutputSettingsAt(
    int index,
    QString* outputFormat,
    QString* codec,
    int* width,
    int* height,
    double* fps,
    int* bitrateKbps) const;
  void setJobOutputSettingsAt(
    int index,
    const QString& outputFormat,
    const QString& codec,
    int width,
    int height,
    double fps,
    int bitrateKbps);
  QString jobErrorMessageAt(int index) const;
  bool jobOverlayTransformAt(int index, float* offsetX, float* offsetY, float* scale, float* rotationDeg) const;
  void setJobOverlayTransform(int index, float offsetX, float offsetY, float scale, float rotationDeg);
  void resetJobForRerun(int index);
  int resetCompletedAndFailedJobsForRerun();

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
  void setJobStatusChangedCallback(std::function<void(int, int)> callback);
  void setJobProgressChangedCallback(std::function<void(int, int)> callback);
  void setAllJobsCompletedCallback(std::function<void()> callback);
  void setAllJobsRemovedCallback(std::function<void()> callback);

 };



};
